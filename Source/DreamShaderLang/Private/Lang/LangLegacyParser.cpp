// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The legacy (1.x) front end, top level: the module loop, `import`, the block words with their
// `(Name = ..., Root = ...)` headers, `Function` / `GraphFunction`, `Namespace` and `VirtualFunction`.
//
// The legacy front end is not a second parser. It is this same FLangParser with the Frontend member set to
// Legacy (or, inside a `.dsh`, with a legacy scope open around one declaration), reading the shared token
// stream and producing the ordinary 2.0 tree (Plan/m4m5/research-legacy.md section 3):
//
//   Shader(Name = "Dir/M_X", Root = "R")  ->  #pragma material(...)          (Settings)
//                                            uniform ... / static const ...  (Properties)
//                                            /// @name Dir/M_X  /// @root R
//                                            export void M_X(inout material Base) { Outputs head; Graph; Outputs tail }
//                                            #pragma layout(...)             (Layout)
//   ShaderFunction / ShaderLayer / ...    ->  /// @name ... export <Result type or void> Leaf(inputs..., out outputs...)
//   Function / GraphFunction             ->  /// @custom [selfcontained] with an opaque, 1.x-normalised body
//   Namespace(Name = "N") { Function F }  ->  the function N_F
//   VirtualFunction(Name = "F")           ->  /// @asset <reference> extern ... F(...);
//
// Every declaration it makes is marked FDecl::bLegacy, and everything 2.0 cannot say is recorded in
// FLegacyMigrationInfo (LangLegacy.h), the one channel of 1.x facts to migrate, Adopt and the
// VirtualFunction sync (batch-2 contract, agreement A11).

#include "LangParserInternal.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangLegacy.h"
#include "Lang/LangLexer.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/Char.h"
#include "Misc/CString.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.LegacyParser"

namespace UE::DreamShader::Lang::Private
{
	namespace LegacyParser
	{
		/**
		 * 1.x let a Graph declare a name its Outputs section had already declared (`Outputs = { vec3 Color; ... }` and then
		 * `Graph = { vec3 Color = ...; }`): there was one variable table, and the Outputs bindings read it at the end. In
		 * 2.0 both declarations land in one block, where the second is a redeclaration. So a top-level Graph declaration
		 * of an output's name becomes what it meant: an assignment to that output when it has an initializer the author
		 * wrote, nothing at all when it has none. Declarators that name no output stay declarations, in source order.
		 */
		static void FoldLegacyOutputRedeclarations(TArray<FStmtPtr>& GraphStatements, const TArray<FString>& OutputNames, FLegacyMigrationInfo* LegacyInfo)
		{
			if (OutputNames.Num() == 0)
			{
				return;
			}

			const auto NamesOutput = [&OutputNames](const FString& Name)
			{
				return OutputNames.ContainsByPredicate([&Name](const FString& Output) { return Output.Equals(Name, ESearchCase::CaseSensitive); });
			};

			for (int32 StatementIndex = 0; StatementIndex < GraphStatements.Num(); ++StatementIndex)
			{
				FVarDeclStmt* Declaration = GraphStatements[StatementIndex].IsValid() ? GraphStatements[StatementIndex]->As<FVarDeclStmt>() : nullptr;
				if (!Declaration || !Declaration->Declarators.ContainsByPredicate([&NamesOutput](const FDeclarator& Declarator) { return NamesOutput(Declarator.Name); }))
				{
					continue;
				}

				// One statement per declarator, in the order they were written: a later initializer may read an earlier name.
				TArray<FStmtPtr> Replacements;
				for (FDeclarator& Declarator : Declaration->Declarators)
				{
					const bool bSynthesized = LegacyInfo && LegacyInfo->SynthesizedInitializers.ContainsByPredicate(
						[Declaration, &Declarator](const FLegacySynthesizedInitializer& Record)
						{
							return Record.Declaration == Declaration && Record.Name.Equals(Declarator.Name, ESearchCase::CaseSensitive);
						});

					if (!NamesOutput(Declarator.Name))
					{
						TUniquePtr<FVarDeclStmt> Kept = MakeUnique<FVarDeclStmt>();
						Kept->Storage = Declaration->Storage;
						Kept->Type = Declaration->Type;
						Kept->Span = Declarator.Span;
						if (bSynthesized)
						{
							// The record follows the declaration it describes.
							for (FLegacySynthesizedInitializer& Record : LegacyInfo->SynthesizedInitializers)
							{
								if (Record.Declaration == Declaration && Record.Name.Equals(Declarator.Name, ESearchCase::CaseSensitive))
								{
									Record.Declaration = Kept.Get();
								}
							}
						}
						Kept->Declarators.Add(MoveTemp(Declarator));
						Replacements.Add(MoveTemp(Kept));
						continue;
					}

					if (Declarator.Initializer.IsValid() && !bSynthesized)
					{
						TUniquePtr<FAssignExpr> Assign = MakeUnique<FAssignExpr>();
						Assign->Op = EAssignOp::Assign;
						Assign->Target = LegacyAst::MakeIdentifier(Declarator.Name, Declarator.NameSpan);
						Assign->Value = MoveTemp(Declarator.Initializer);
						Assign->Span = Declarator.Span;

						TUniquePtr<FExprStmt> Statement = MakeUnique<FExprStmt>();
						Statement->Expression = MoveTemp(Assign);
						Statement->Span = Declarator.Span;
						Replacements.Add(MoveTemp(Statement));
					}
				}

				if (LegacyInfo)
				{
					// What is left pointing at the statement about to go describes a declarator that went with it.
					LegacyInfo->SynthesizedInitializers.RemoveAll([Declaration](const FLegacySynthesizedInitializer& Record) { return Record.Declaration == Declaration; });
				}

				GraphStatements.RemoveAt(StatementIndex);
				for (int32 Offset = 0; Offset < Replacements.Num(); ++Offset)
				{
					GraphStatements.Insert(MoveTemp(Replacements[Offset]), StatementIndex + Offset);
				}
				StatementIndex += Replacements.Num() - 1;
			}
		}

		/** The extension a message names for this file, `dsm` for a `.dsm`. */
		static FString LegacyFileExtension(const ELangFileKind Kind)
		{
			switch (Kind)
			{
			case ELangFileKind::Dss: return TEXT("dss");
			case ELangFileKind::Dsh: return TEXT("dsh");
			case ELangFileKind::Dsm: return TEXT("dsm");
			case ELangFileKind::Dsf: return TEXT("dsf");
			case ELangFileKind::Dsi: return TEXT("dsi");
			case ELangFileKind::Unknown: return TEXT("dsm");
			}
			return TEXT("dsm");
		}

		/** A value in an attribute list or a section entry: tokens up to `,` / `)` (or `;`) at depth zero. */
		static void SkipLegacyTopLevelValue(FLangParser& Parser, const bool bStopAtSemicolon, int32& OutFirst, int32& OutEnd)
		{
			OutFirst = Parser.GetTokenIndex();
			int32 Depth = 0;
			while (!Parser.AtEnd())
			{
				const ELangTokenKind Kind = Parser.Current().Kind;
				if (Depth == 0
					&& (Kind == ELangTokenKind::Comma || Kind == ELangTokenKind::RightParen || Kind == ELangTokenKind::RightBrace
						|| (bStopAtSemicolon && Kind == ELangTokenKind::Semicolon)))
				{
					break;
				}
				if (Kind == ELangTokenKind::LeftParen || Kind == ELangTokenKind::LeftBracket || Kind == ELangTokenKind::LeftBrace)
				{
					++Depth;
				}
				else if (Kind == ELangTokenKind::RightParen || Kind == ELangTokenKind::RightBracket || Kind == ELangTokenKind::RightBrace)
				{
					Depth = FMath::Max(0, Depth - 1);
				}
				Parser.Advance();
			}
			OutEnd = Parser.GetTokenIndex();
		}

		static FLangSpan SpanOfLegacyTopLevelTokens(FLangParser& Parser, const int32 First, const int32 End)
		{
			const int32 Saved = Parser.GetTokenIndex();
			Parser.SetTokenIndex(First);
			FLangSpan Span = Parser.Current().Span;
			if (End - 1 > First)
			{
				Parser.SetTokenIndex(End - 1);
				Span = FLangSpan::Join(Span, Parser.Current().Span);
			}
			Parser.SetTokenIndex(Saved);
			return Span;
		}

		static const FPragmaArgument* FindLegacyAttribute(const TArray<FPragmaArgument>& Attributes, const TCHAR* Key)
		{
			// 1.x read attributes from a TMap<FString>: the key matched whatever its case.
			for (const FPragmaArgument& Attribute : Attributes)
			{
				if (Attribute.Key.Equals(Key, ESearchCase::IgnoreCase))
				{
					return &Attribute;
				}
			}
			return nullptr;
		}

		/** At a `{`: skips through its `}`; at a `(`, through its `)`. Nothing otherwise. */
		static void SkipLegacyBalanced(FLangParser& Parser)
		{
			const ELangTokenKind Open = Parser.Current().Kind;
			const ELangTokenKind Close = Open == ELangTokenKind::LeftBrace ? ELangTokenKind::RightBrace
				: Open == ELangTokenKind::LeftParen ? ELangTokenKind::RightParen
				: ELangTokenKind::EndOfFile;
			if (Close == ELangTokenKind::EndOfFile)
			{
				return;
			}
			int32 Depth = 0;
			while (!Parser.AtEnd())
			{
				const ELangTokenKind Kind = Parser.Advance().Kind;
				if (Kind == Open)
				{
					++Depth;
				}
				else if (Kind == Close && --Depth <= 0)
				{
					return;
				}
			}
		}

		/** `Word (attributes) { ... } [;]`: what is left of a block the parser gave up on. */
		static void SkipLegacyBlockRemainder(FLangParser& Parser)
		{
			if (Parser.Check(ELangTokenKind::LeftParen))
			{
				SkipLegacyBalanced(Parser);
			}
			if (Parser.Check(ELangTokenKind::LeftBrace))
			{
				SkipLegacyBalanced(Parser);
			}
			Parser.Match(ELangTokenKind::Semicolon);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Words
	// ---------------------------------------------------------------------------------------------

	bool FLangParser::IsLegacyAssetBlockWord(const FString& Text)
	{
		// Exact case, as 1.x's TryConsumeKeyword matched them.
		return Text.Equals(TEXT("Shader"), ESearchCase::CaseSensitive)
			|| Text.Equals(TEXT("ShaderFunction"), ESearchCase::CaseSensitive)
			|| Text.Equals(TEXT("ShaderLayer"), ESearchCase::CaseSensitive)
			|| Text.Equals(TEXT("ShaderLayerBlend"), ESearchCase::CaseSensitive)
			|| Text.Equals(TEXT("MaterialLayer"), ESearchCase::CaseSensitive)
			|| Text.Equals(TEXT("MaterialLayerBlend"), ESearchCase::CaseSensitive);
	}

	bool FLangParser::IsLegacyTopLevelWord(const FString& Text)
	{
		return Text.Equals(TEXT("Function"), ESearchCase::CaseSensitive)
			|| Text.Equals(TEXT("GraphFunction"), ESearchCase::CaseSensitive)
			|| Text.Equals(TEXT("Namespace"), ESearchCase::CaseSensitive)
			|| Text.Equals(TEXT("VirtualFunction"), ESearchCase::CaseSensitive)
			|| IsLegacyAssetBlockWord(Text);
	}

	// ---------------------------------------------------------------------------------------------
	// Module loop and recovery
	// ---------------------------------------------------------------------------------------------

	TUniquePtr<FModule> FLangParser::ParseLegacyModule(FLegacyMigrationInfo* Info)
	{
		SetLegacyInfo(Info);

		TUniquePtr<FModule> Module = MakeUnique<FModule>();
		Module->FilePath = Source.GetPath();
		Module->FileKind = FileKind;

		while (!AtEnd())
		{
			const int32 StartIndex = GetTokenIndex();

			TArray<FDeclPtr> Declarations;
			if (!ParseLegacyTopLevel(Declarations))
			{
				SkipToLegacyTopLevelBoundary();
			}
			for (FDeclPtr& Declaration : Declarations)
			{
				if (Declaration.IsValid())
				{
					Module->Declarations.Add(MoveTemp(Declaration));
				}
			}

			if (GetTokenIndex() == StartIndex)
			{
				Advance();
			}
		}

		// 1.x refused a file with no block at all (DSH2010). A header may hold nothing but imports.
		if (!bLegacySawConstruct && FileKind != ELangFileKind::Dsh && !Diagnostics.HasErrors())
		{
			Diagnostics.Error(
				TEXT("DSH2254"),
				Source.MakeSpan(0, 0),
				LOCTEXT("NoLegacyBlock", "Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace or VirtualFunction block in this 1.x file, found none."));
		}

		return Module;
	}

	void FLangParser::SkipToLegacyTopLevelBoundary()
	{
		// Stops BEFORE a top-level word even when it is the first token: a construct that failed after
		// consuming its whole block leaves the cursor exactly there, and the module loop guarantees progress.
		int32 Depth = 0;
		while (!AtEnd())
		{
			const FLangToken& Token = Current();
			if (Depth == 0 && Token.bAtLineStart
				&& ((Token.Kind == ELangTokenKind::Identifier && IsLegacyTopLevelWord(Token.Text)) || Token.IsKeyword(ELangKeyword::Import)))
			{
				return;
			}

			if (Token.Kind == ELangTokenKind::LeftBrace)
			{
				++Depth;
			}
			else if (Token.Kind == ELangTokenKind::RightBrace)
			{
				Advance();
				if (--Depth <= 0)
				{
					return;
				}
				continue;
			}
			Advance();
		}
	}

	bool FLangParser::ParseLegacyTopLevel(TArray<FDeclPtr>& OutDecls)
	{
		// A `///` line in a 1.x file is a comment (the legacy lexer does not emit doc comments; inside a
		// `.dsh` the 2.0 module loop has taken the block above the declaration already).
		while (Check(ELangTokenKind::DocComment))
		{
			RecordSkippedDocComment(Advance());
		}
		if (AtEnd() || Match(ELangTokenKind::Semicolon))
		{
			return true;
		}

		const FLangToken& Token = Current();

		const bool bImportKeyword = Token.IsKeyword(ELangKeyword::Import);
		const bool bImportWord = Token.Kind == ELangTokenKind::Identifier && Token.Text.Equals(TEXT("import"), ESearchCase::IgnoreCase);
		if (bImportKeyword || bImportWord)
		{
			const int32 StartIndex = GetTokenIndex();
			if (bImportWord)
			{
				Diagnostics.Warning(
					TEXT("DSH2253"),
					Token.Span,
					FText::Format(LOCTEXT("ImportCase", "'{0}' is read as 'import'; write it in lower case."), FText::FromString(Token.Text)));
			}
			Advance();

			if (!Check(ELangTokenKind::StringLiteral))
			{
				return Diagnostics.Error(
					TEXT("DSH2252"),
					Current().Span,
					FText::Format(LOCTEXT("ImportPath", "Expected a double-quoted path after 'import', found {0}."), DescribeToken(Current())));
			}
			const FLangToken& PathToken = Advance();
			Match(ELangTokenKind::Semicolon);

			const FString Path = PathToken.Text.TrimStartAndEnd();
			const FString Extension = FPaths::GetExtension(Path);
			if (Extension.Equals(TEXT("dsf"), ESearchCase::IgnoreCase) || Extension.Equals(TEXT("dsm"), ESearchCase::IgnoreCase))
			{
				return Diagnostics.Error(
					TEXT("DSH2252"),
					PathToken.Span,
					FText::Format(LOCTEXT("ImportNotHeader", "Expected an import of a '.dsh' header, found '{0}'; a material or function file is compiled on its own, not included."), FText::FromString(Path)));
			}

			const int32 Colon = Path.Find(TEXT(":"));
			if (Colon > 0)
			{
				const FString Qualifier = Path.Left(Colon);
				const bool bRootQualifier = Qualifier.Equals(TEXT("Project"), ESearchCase::IgnoreCase)
					|| Qualifier.StartsWith(TEXT("Plugin."), ESearchCase::IgnoreCase)
					|| Qualifier.StartsWith(TEXT("Plugins."), ESearchCase::IgnoreCase)
					|| Qualifier.StartsWith(TEXT("Plugin/"), ESearchCase::IgnoreCase)
					|| Qualifier.StartsWith(TEXT("Plugins/"), ESearchCase::IgnoreCase);
				if (bRootQualifier)
				{
					return Diagnostics.Error(
						TEXT("DSH2252"),
						PathToken.Span,
						FText::Format(LOCTEXT("ImportQualified", "Expected an import path inside this file's own source root, found the root-qualified '{0}', which the 2.0 include resolver does not read."), FText::FromString(Path)));
				}
			}

			TUniquePtr<FIncludeDecl> Include = MakeUnique<FIncludeDecl>();
			Include->bLegacy = true;
			Include->Path = PathToken.Text;
			Include->PathSpan = PathToken.Span;
			Include->bImportSpelling = true;
			Include->Span = SpanFrom(StartIndex);
			OutDecls.Add(MoveTemp(Include));
			return true;
		}

		if (Token.Kind == ELangTokenKind::Identifier)
		{
			if (IsLegacyAssetBlockWord(Token.Text))
			{
				return ParseLegacyBlock(OutDecls);
			}
			if (Token.Text.Equals(TEXT("Function"), ESearchCase::CaseSensitive) || Token.Text.Equals(TEXT("GraphFunction"), ESearchCase::CaseSensitive))
			{
				FDeclPtr Function = ParseLegacyFunction(FString());
				if (!Function.IsValid())
				{
					return false;
				}
				OutDecls.Add(MoveTemp(Function));
				return true;
			}
			if (Token.Text.Equals(TEXT("Namespace"), ESearchCase::CaseSensitive))
			{
				return ParseLegacyNamespace(OutDecls);
			}
			if (Token.Text.Equals(TEXT("VirtualFunction"), ESearchCase::CaseSensitive))
			{
				FDeclPtr Function = ParseLegacyVirtualFunction();
				if (!Function.IsValid())
				{
					return false;
				}
				OutDecls.Add(MoveTemp(Function));
				return true;
			}
		}

		// What is left is either 2.0 syntax, which a 1.x file does not take, or nothing either language knows.
		const bool bLooksLike20 = Token.Kind == ELangTokenKind::Directive
			|| Token.IsKeyword(ELangKeyword::Uniform)
			|| Token.IsKeyword(ELangKeyword::Static)
			|| Token.IsKeyword(ELangKeyword::Const)
			|| Token.IsKeyword(ELangKeyword::Extern)
			|| Token.IsKeyword(ELangKeyword::Export)
			|| Token.IsKeyword(ELangKeyword::Struct)
			|| LooksLikeDeclarationStart();
		if (bLooksLike20)
		{
			return Diagnostics.Error(
				TEXT("DSH2248"),
				Token.Span,
				FText::Format(
					LOCTEXT("TwoPointZeroInLegacyFile", "Expected a 1.x block in a '.{0}' file, found {1}, which is 2.0 syntax; 2.0 declarations belong in a .dss file or a .dsh header."),
					FText::FromString(LegacyParser::LegacyFileExtension(FileKind)),
					DescribeToken(Token)));
		}

		return Diagnostics.Error(
			TEXT("DSH2240"),
			Token.Span,
			FText::Format(
				LOCTEXT("UnknownTopLevel", "Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace, VirtualFunction or import, found {0}."),
				DescribeToken(Token)));
	}

	bool FLangParser::ParseLegacyDeclarationInHeader(FDocBlock&& Doc, TArray<FDeclPtr>& OutDecls)
	{
		const FLangToken& Token = Current();
		if (IsLegacyAssetBlockWord(Token.Text))
		{
			// 1.x decided this by a substring scan of the whole file (DSH8135); here it is one declaration.
			Diagnostics.Error(
				TEXT("DSH2249"),
				Token.Span,
				FText::Format(
					LOCTEXT("AssetBlockInHeader", "Expected only Function, GraphFunction, Namespace and VirtualFunction blocks in a '.dsh' header, found the asset block '{0}', which belongs in a .dsm or .dsf file."),
					FText::FromString(Token.Text)));
			Advance();
			LegacyParser::SkipLegacyBlockRemainder(*this);
			return false;
		}

		LegacyPendingDoc = MoveTemp(Doc);
		++LegacyScopeDepth;
		const bool bOk = ParseLegacyTopLevel(OutDecls);
		--LegacyScopeDepth;
		LegacyPendingDoc = FDocBlock();

		for (FDeclPtr& Declaration : OutDecls)
		{
			if (Declaration.IsValid())
			{
				Declaration->bLegacy = true;
			}
		}
		return bOk;
	}

	// ---------------------------------------------------------------------------------------------
	// `(Key = Value, ...)`
	// ---------------------------------------------------------------------------------------------

	bool FLangParser::ParseLegacyAttributes(TArray<FPragmaArgument>& OutAttributes)
	{
		if (!Check(ELangTokenKind::LeftParen))
		{
			return Diagnostics.Error(
				TEXT("DSH2241"),
				Current().Span,
				FText::Format(
					LOCTEXT("AttributesMissing", "Expected '(' with the block's attributes, such as '(Name = \"M_Example\")', found {0}."),
					DescribeToken(Current())));
		}
		Advance(); // (

		if (Match(ELangTokenKind::RightParen))
		{
			return true;
		}

		for (;;)
		{
			const int32 StartIndex = GetTokenIndex();
			if (!Check(ELangTokenKind::Identifier))
			{
				return Diagnostics.Error(
					TEXT("DSH2243"),
					Current().Span,
					FText::Format(LOCTEXT("AttributeName", "Expected an attribute name such as 'Name', found {0}."), DescribeToken(Current())));
			}
			const FString Key = Advance().Text;

			if (!Match(ELangTokenKind::Assign))
			{
				return Diagnostics.Error(
					TEXT("DSH2243"),
					Current().Span,
					FText::Format(LOCTEXT("AttributeEquals", "Expected '=' after the attribute '{0}', found {1}."), FText::FromString(Key), DescribeToken(Current())));
			}

			int32 First = 0;
			int32 End = 0;
			LegacyParser::SkipLegacyTopLevelValue(*this, false, First, End);
			if (End == First)
			{
				return Diagnostics.Error(
					TEXT("DSH2243"),
					Current().Span,
					FText::Format(LOCTEXT("AttributeValue", "Expected a value after '{0} =', found {1}."), FText::FromString(Key), DescribeToken(Current())));
			}

			FPragmaArgument Attribute;
			Attribute.Key = Key;
			const FLangSpan ValueSpan = LegacyParser::SpanOfLegacyTopLevelTokens(*this, First, End);
			const int32 Saved = GetTokenIndex();
			SetTokenIndex(First);
			if (End - First == 1 && Check(ELangTokenKind::StringLiteral))
			{
				Attribute.Value = Current().Text;
				Attribute.bQuoted = true;
			}
			else
			{
				Attribute.Value = Slice(ValueSpan).TrimStartAndEnd();
				Attribute.bQuoted = false;
			}
			SetTokenIndex(Saved);
			Attribute.Span = SpanFrom(StartIndex);

			for (int32 ItemIndex = 0; ItemIndex < OutAttributes.Num(); ++ItemIndex)
			{
				if (OutAttributes[ItemIndex].Key.Equals(Key, ESearchCase::IgnoreCase))
				{
					Diagnostics.Warning(
						TEXT("DSH2244"),
						Attribute.Span,
						FText::Format(LOCTEXT("AttributeRepeated", "The attribute '{0}' is written twice; the later value wins, as it did in 1.x."), FText::FromString(Key)));
					OutAttributes.RemoveAt(ItemIndex);
					break;
				}
			}
			OutAttributes.Add(MoveTemp(Attribute));

			if (Match(ELangTokenKind::Comma))
			{
				continue;
			}
			if (Match(ELangTokenKind::RightParen))
			{
				return true;
			}
			return Diagnostics.Error(
				TEXT("DSH2243"),
				Current().Span,
				FText::Format(LOCTEXT("AttributeSeparator", "Expected ',' or ')' in the attribute list, found {0}."), DescribeToken(Current())));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Blocks: Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend
	// ---------------------------------------------------------------------------------------------

	namespace LegacyParser
	{
		/** The asset leaf of a 1.x `Name=` ("Dir/M_X" -> "M_X") as an identifier, unique among this file's legacy names. */
		static FString MakeLegacyLeafIdentifier(const FString& Name, TArray<FString>& UsedNames)
		{
			FString Leaf = Name.TrimStartAndEnd();
			int32 Slash = INDEX_NONE;
			if (Leaf.FindLastChar(TEXT('/'), Slash))
			{
				Leaf.RightChopInline(Slash + 1);
			}
			int32 Dot = INDEX_NONE;
			if (Leaf.FindChar(TEXT('.'), Dot))
			{
				Leaf.LeftInline(Dot);
			}

			const FString Identifier = LegacyAst::SanitizeIdentifier(Leaf);
			FString Candidate = Identifier;
			for (int32 Suffix = 2;; ++Suffix)
			{
				bool bUsed = false;
				for (const FString& Used : UsedNames)
				{
					if (Used.Equals(Candidate, ESearchCase::CaseSensitive))
					{
						bUsed = true;
						break;
					}
				}
				if (!bUsed)
				{
					break;
				}
				Candidate = FString::Printf(TEXT("%s_%d"), *Identifier, Suffix);
			}
			UsedNames.Add(Candidate);
			return Candidate;
		}

		/** A `///` directive the legacy front end wrote from 1.x state, recorded for migrate. */
		static void AddLegacyBlockDirective(FLegacyMigrationInfo* Info, FDecl& Decl, const FString& Key, const FString& Value)
		{
			FDocDirective Directive;
			Directive.Key = Key;
			Directive.Value = Value;
			Decl.Doc.Directives.Add(MoveTemp(Directive));

			if (Info)
			{
				FLegacySynthesizedDirective& Record = Info->SynthesizedDirectives.AddDefaulted_GetRef();
				Record.Decl = &Decl;
				Record.Key = Key;
				Record.Value = Value;
			}
		}
	}

	bool FLangParser::ParseLegacyBlock(TArray<FDeclPtr>& OutDecls)
	{
		const int32 HeaderStart = GetTokenIndex();
		const FLangToken& WordToken = Advance();
		const FString BlockWord = WordToken.Text;

		if (FileKind == ELangFileKind::Dsh)
		{
			Diagnostics.Error(
				TEXT("DSH2249"),
				WordToken.Span,
				FText::Format(
					LOCTEXT("AssetBlockInLegacyHeader", "Expected only Function, GraphFunction, Namespace and VirtualFunction blocks in a '.dsh' header, found the asset block '{0}', which belongs in a .dsm or .dsf file."),
					FText::FromString(BlockWord)));
			LegacyParser::SkipLegacyBlockRemainder(*this);
			return true;
		}

		const bool bShader = BlockWord.Equals(TEXT("Shader"), ESearchCase::CaseSensitive);
		const bool bLayerBlend = BlockWord.Equals(TEXT("ShaderLayerBlend"), ESearchCase::CaseSensitive)
			|| BlockWord.Equals(TEXT("MaterialLayerBlend"), ESearchCase::CaseSensitive);
		const bool bLayer = !bLayerBlend
			&& (BlockWord.Equals(TEXT("ShaderLayer"), ESearchCase::CaseSensitive) || BlockWord.Equals(TEXT("MaterialLayer"), ESearchCase::CaseSensitive));

		if (BlockWord.StartsWith(TEXT("MaterialLayer"), ESearchCase::CaseSensitive))
		{
			Diagnostics.Warning(
				TEXT("DSH2251"),
				WordToken.Span,
				FText::Format(
					LOCTEXT("OldLayerWord", "'{0}' is the old spelling of '{1}'; it still reads the same."),
					FText::FromString(BlockWord),
					FText::FromString(bLayerBlend ? FString(TEXT("ShaderLayerBlend")) : FString(TEXT("ShaderLayer")))));
		}
		if (bShader && bLegacySawShaderBlock)
		{
			Diagnostics.Error(
				TEXT("DSH2250"),
				WordToken.Span,
				LOCTEXT("SecondShader", "Expected one Shader block in a file, found a second one."));
		}

		TArray<FPragmaArgument> Attributes;
		if (!ParseLegacyAttributes(Attributes))
		{
			return false;
		}
		const FLangSpan HeaderSpan = SpanFrom(HeaderStart);

		const FPragmaArgument* NameAttribute = LegacyParser::FindLegacyAttribute(Attributes, TEXT("Name"));
		if (!NameAttribute || NameAttribute->Value.TrimStartAndEnd().IsEmpty())
		{
			Diagnostics.Error(
				TEXT("DSH2242"),
				HeaderSpan,
				FText::Format(LOCTEXT("BlockWithoutName", "Expected a 'Name = \"...\"' attribute on '{0}', found none."), FText::FromString(BlockWord)));
			LegacyParser::SkipLegacyBlockRemainder(*this);
			return true;
		}
		const FString BlockName = NameAttribute->Value.TrimStartAndEnd();
		const FLangSpan NameValueSpan = NameAttribute->Span;
		const FPragmaArgument* RootAttribute = LegacyParser::FindLegacyAttribute(Attributes, TEXT("Root"));

		if (!Check(ELangTokenKind::LeftBrace))
		{
			return Diagnostics.Error(
				TEXT("DSH2257"),
				Current().Span,
				FText::Format(LOCTEXT("BlockOpen", "Expected '`{' to open the '{0}' block, found {1}."), FText::FromString(BlockWord), DescribeToken(Current())));
		}
		Advance(); // {

		bLegacySawConstruct = true;
		if (bShader)
		{
			bLegacySawShaderBlock = true;
		}

		FLegacyBlockContext Block;
		Block.BlockWord = BlockWord;
		Block.Name = BlockName;
		Block.Root = RootAttribute ? RootAttribute->Value.TrimStartAndEnd() : FString();
		Block.bHasRoot = RootAttribute != nullptr;
		Block.bMaterial = bShader;
		Block.Info = LegacyInfo;

		const int32 FirstParameterDeclaration = LegacyInfo ? LegacyInfo->ParameterDeclarations.Num() : 0;
		FLegacyBlockContext* const OuterBlock = LegacyBlock;
		LegacyBlock = &Block;

		struct FPendingSection
		{
			FString Name;
			FLangSpan HeaderSpan;
			FLangSpan BodySpan;
			const FNode* FirstNode = nullptr;
			bool bOwnedByDeclaration = false;
			bool bSettings = false;
			bool bGraph = false;
		};
		TArray<FPendingSection> Sections;

		TArray<FDeclPtr> PropertyDecls;
		TArray<FPragmaArgument> Settings;
		bool bHasSettings = false;
		FLangSpan SettingsSpan;
		TArray<FStmtPtr> OutputsHead;
		TArray<FStmtPtr> OutputsTail;
		bool bHasOutputs = false;
		TArray<FParam> Inputs;
		TArray<FParam> Outputs;
		TArray<FString> InputNames;
		TArray<FString> OutputNames;
		TArray<TPair<FString, FString>> ParamDocs;
		int32 GraphOpenIndex = INDEX_NONE;
		TArray<FDeclPtr> LayoutDecls;
		bool bHasLayout = false;

		while (!Check(ELangTokenKind::RightBrace))
		{
			if (AtEnd())
			{
				LegacyBlock = OuterBlock;
				return FailAtEnd(FText::Format(LOCTEXT("WhileBlock", "the '{0}' block"), FText::FromString(BlockName)));
			}
			if (Match(ELangTokenKind::Semicolon))
			{
				continue;
			}
			if (Check(ELangTokenKind::DocComment))
			{
				RecordSkippedDocComment(Advance());
				continue;
			}

			const int32 SectionStart = GetTokenIndex();
			if (!Check(ELangTokenKind::Identifier))
			{
				Diagnostics.Error(
					TEXT("DSH2257"),
					Current().Span,
					FText::Format(LOCTEXT("SectionName", "Expected a section name such as 'Properties' or 'Graph' in '{0}', found {1}."), FText::FromString(BlockWord), DescribeToken(Current())));
				while (!AtEnd() && !Check(ELangTokenKind::LeftBrace) && !Check(ELangTokenKind::RightBrace))
				{
					Advance();
				}
				LegacyParser::SkipLegacyBalanced(*this);
				continue;
			}

			const FLangToken& SectionToken = Advance();
			const FString SectionName = SectionToken.Text;
			Match(ELangTokenKind::Assign);
			if (!Check(ELangTokenKind::LeftBrace))
			{
				Diagnostics.Error(
					TEXT("DSH2257"),
					Current().Span,
					FText::Format(LOCTEXT("SectionOpen", "Expected '`{' after the section name '{0}', found {1}."), FText::FromString(SectionName), DescribeToken(Current())));
				while (!AtEnd() && !Check(ELangTokenKind::LeftBrace) && !Check(ELangTokenKind::RightBrace))
				{
					Advance();
				}
				LegacyParser::SkipLegacyBalanced(*this);
				continue;
			}

			FPendingSection Pending;
			Pending.Name = SectionName;
			Pending.HeaderSpan = SpanFrom(SectionStart);
			const FLangSpan BodyOpenSpan = Current().Span;

			auto IsSection = [&SectionName](const TCHAR* Candidate) -> bool
			{
				// 1.x compared section names ignoring case.
				return SectionName.Equals(Candidate, ESearchCase::IgnoreCase);
			};

			if (IsSection(TEXT("Properties")))
			{
				const int32 Before = PropertyDecls.Num();
				ParseLegacyProperties(Block, PropertyDecls);
				Pending.FirstNode = PropertyDecls.Num() > Before ? PropertyDecls[Before].Get() : nullptr;
			}
			else if (IsSection(TEXT("Settings")))
			{
				// Repeated Settings sections merge key by key, the later value winning (1.x kept one map).
				ParseLegacySettings(Block, Settings);
				const FLangSpan ThisSpan = FLangSpan::Join(Pending.HeaderSpan, Previous().Span);
				SettingsSpan = bHasSettings ? FLangSpan::Join(SettingsSpan, ThisSpan) : ThisSpan;
				bHasSettings = true;
				Pending.bSettings = true;
			}
			else if (bShader && IsSection(TEXT("Outputs")))
			{
				const int32 HeadBefore = OutputsHead.Num();
				const int32 TailBefore = OutputsTail.Num();
				ParseLegacyOutputs(Block, OutputsHead, OutputsTail);
				bHasOutputs = true;
				Pending.FirstNode = OutputsHead.Num() > HeadBefore
					? static_cast<const FNode*>(OutputsHead[HeadBefore].Get())
					: (OutputsTail.Num() > TailBefore ? static_cast<const FNode*>(OutputsTail[TailBefore].Get()) : nullptr);
			}
			else if (!bShader && (IsSection(TEXT("Outputs")) || IsSection(TEXT("Results"))))
			{
				ParseLegacyParamsWithDocs(Block, EParamDirection::Out, Outputs, OutputNames, ParamDocs);
				bHasOutputs = true;
				Pending.bOwnedByDeclaration = true;
			}
			else if (!bShader && IsSection(TEXT("Inputs")))
			{
				ParseLegacyParamsWithDocs(Block, EParamDirection::In, Inputs, InputNames, ParamDocs);
				Pending.bOwnedByDeclaration = true;
			}
			else if (IsSection(TEXT("Graph")))
			{
				if (GraphOpenIndex != INDEX_NONE)
				{
					Diagnostics.Warning(
						TEXT("DSH2258"),
						Pending.HeaderSpan,
						FText::Format(LOCTEXT("GraphRepeated", "The section '{0}' is written twice; the later one wins, as it did in 1.x."), FText::FromString(SectionName)));
				}
				// Parsed after the last section, so a property declared below the Graph still expands in it.
				GraphOpenIndex = GetTokenIndex();
				LegacyParser::SkipLegacyBalanced(*this);
				Pending.bGraph = true;
			}
			else if (IsSection(TEXT("Layout")))
			{
				if (bHasLayout)
				{
					Diagnostics.Warning(
						TEXT("DSH2258"),
						Pending.HeaderSpan,
						FText::Format(LOCTEXT("LayoutRepeated", "The section '{0}' is written twice; the later one wins, as it did in 1.x."), FText::FromString(SectionName)));
					LayoutDecls.Reset();
				}
				bHasLayout = true;
				ParseLegacyLayout(LayoutDecls);
				Pending.FirstNode = LayoutDecls.Num() > 0 ? LayoutDecls[0].Get() : nullptr;
			}
			else if (IsSection(TEXT("Code")))
			{
				Diagnostics.Error(
					TEXT("DSH2246"),
					SectionToken.Span,
					FText::Format(LOCTEXT("CodeSection", "Expected 'Graph' as the body section of '{0}', found 'Code', which 1.x accepted only inside a Function."), FText::FromString(BlockWord)));
				LegacyParser::SkipLegacyBalanced(*this);
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH2245"),
					SectionToken.Span,
					FText::Format(
						bShader
							? LOCTEXT("UnknownShaderSection", "Expected a Shader section (Properties, Settings, Outputs, Graph or Layout), found '{0}'.")
							: LOCTEXT("UnknownFunctionSection", "Expected a function section (Properties, Inputs, Outputs, Settings, Graph or Layout), found '{0}'."),
						FText::FromString(SectionName)));
				LegacyParser::SkipLegacyBalanced(*this);
			}

			Pending.BodySpan = FLangSpan::Join(BodyOpenSpan, Previous().Span);
			Sections.Add(MoveTemp(Pending));
			Match(ELangTokenKind::Semicolon);
		}
		Advance(); // }
		const int32 AfterBlock = GetTokenIndex();

		TUniquePtr<FBlockStmt> Graph;
		if (GraphOpenIndex != INDEX_NONE)
		{
			SetTokenIndex(GraphOpenIndex);
			Graph = ParseLegacyGraphBody(Block);
			SetTokenIndex(AfterBlock);
		}
		LegacyBlock = OuterBlock;

		const FNode* GraphFirstNode = (Graph.IsValid() && Graph->Statements.Num() > 0) ? Graph->Statements[0].Get() : nullptr;

		FLegacyBlock BlockRecord;
		BlockRecord.BlockWord = BlockWord;
		BlockRecord.Name = BlockName;
		BlockRecord.Root = Block.Root;
		BlockRecord.bHasRoot = Block.bHasRoot;
		BlockRecord.HeaderSpan = HeaderSpan;

		FDecl* MainDecl = nullptr;
		const FNode* SettingsNode = nullptr;

		if (bShader)
		{
			if (GraphOpenIndex == INDEX_NONE && OutputsHead.Num() == 0)
			{
				Diagnostics.Error(
					TEXT("DSH2255"),
					HeaderSpan,
					FText::Format(LOCTEXT("ShaderWithoutGraph", "Expected a Graph section in the Shader '{0}', found none."), FText::FromString(BlockName)));
			}
			if (!bHasOutputs)
			{
				Diagnostics.Warning(
					TEXT("DSH2256"),
					HeaderSpan,
					FText::Format(LOCTEXT("ShaderWithoutOutputs", "The Shader '{0}' has no Outputs section, so nothing its Graph computes reaches the material."), FText::FromString(BlockName)));
			}

			// Settings -> `#pragma material(...)`, keys and values as written.
			if (bHasSettings)
			{
				TUniquePtr<FPragmaDecl> MaterialPragma = MakeUnique<FPragmaDecl>();
				MaterialPragma->bLegacy = true;
				MaterialPragma->PragmaKind = EPragmaKind::Material;
				MaterialPragma->Name = TEXT("material");
				MaterialPragma->Span = SettingsSpan;
				for (const FPragmaArgument& Setting : Settings)
				{
					if (Setting.Key.Equals(TEXT("Backend"), ESearchCase::IgnoreCase))
					{
						BlockRecord.bHasBackend = true;
						BlockRecord.BackendRaw = Setting.Value;
					}
				}
				MaterialPragma->Arguments = MoveTemp(Settings);
				SettingsNode = MaterialPragma.Get();
				OutDecls.Add(MoveTemp(MaterialPragma));
			}

			for (FDeclPtr& Property : PropertyDecls)
			{
				OutDecls.Add(MoveTemp(Property));
			}

			// `export void <Leaf>(inout material Base) { Outputs declarations; Graph; Outputs bindings }`
			TUniquePtr<FFunctionDecl> Entry = MakeUnique<FFunctionDecl>();
			Entry->bLegacy = true;
			Entry->Linkage = EFunctionLinkage::Export;
			ClassifyTypeName(TEXT("void"), Entry->ReturnType);
			Entry->ReturnType.Span = HeaderSpan;
			Entry->Name = LegacyParser::MakeLegacyLeafIdentifier(BlockName, LegacyUsedNames);
			Entry->NameSpan = NameValueSpan;
			Entry->Span = HeaderSpan;

			FParam BaseParam;
			BaseParam.Direction = EParamDirection::InOut;
			ClassifyTypeName(TEXT("material"), BaseParam.Type);
			BaseParam.Type.Span = HeaderSpan;
			BaseParam.Name = TEXT("Base");
			BaseParam.NameSpan = HeaderSpan;
			BaseParam.Span = HeaderSpan;
			Entry->Params.Add(MoveTemp(BaseParam));

			Entry->Body = MakeUnique<FBlockStmt>();
			Entry->Body->Span = Graph.IsValid() ? Graph->Span : FLangSpan();
			for (FStmtPtr& Statement : OutputsHead)
			{
				if (const FVarDeclStmt* Declaration = Statement.IsValid() ? Statement->As<FVarDeclStmt>() : nullptr)
				{
					for (const FDeclarator& Declarator : Declaration->Declarators)
					{
						BlockRecord.OutputNames.Add(Declarator.Name);
					}
				}
				Entry->Body->Statements.Add(MoveTemp(Statement));
			}
			if (Graph.IsValid())
			{
				LegacyParser::FoldLegacyOutputRedeclarations(Graph->Statements, BlockRecord.OutputNames, LegacyInfo);
				for (FStmtPtr& Statement : Graph->Statements)
				{
					Entry->Body->Statements.Add(MoveTemp(Statement));
				}
			}
			for (FStmtPtr& Statement : OutputsTail)
			{
				Entry->Body->Statements.Add(MoveTemp(Statement));
			}

			LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Entry, TEXT("name"), BlockName);
			if (Block.bHasRoot)
			{
				LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Entry, TEXT("root"), Block.Root);
			}

			MainDecl = Entry.Get();
			OutDecls.Add(MoveTemp(Entry));
		}
		else
		{
			TUniquePtr<FFunctionDecl> Function = MakeUnique<FFunctionDecl>();
			Function->bLegacy = true;
			Function->Linkage = EFunctionLinkage::Export;
			ClassifyTypeName(TEXT("void"), Function->ReturnType);
			Function->ReturnType.Span = HeaderSpan;
			Function->Name = LegacyParser::MakeLegacyLeafIdentifier(BlockName, LegacyUsedNames);
			Function->NameSpan = NameValueSpan;
			Function->Span = HeaderSpan;
			MainDecl = Function.Get();
			SettingsNode = MainDecl;

			// Function-block Settings: 1.x read four keys and ignored the rest.
			bool bExposeToLibrary = false;
			FString LibraryCategories;
			FString Description;
			bool bHasDescription = false;
			for (const FPragmaArgument& Setting : Settings)
			{
				const FString Key = Setting.Key.ToLower();
				if (Key == TEXT("description"))
				{
					Description = Setting.Value;
					bHasDescription = true;
				}
				else if (Key == TEXT("exposetolibrary"))
				{
					if (Setting.Value.Equals(TEXT("true"), ESearchCase::IgnoreCase))
					{
						bExposeToLibrary = true;
					}
					else if (!Setting.Value.Equals(TEXT("false"), ESearchCase::IgnoreCase))
					{
						Diagnostics.Warning(
							TEXT("DSH3265"),
							Setting.Span,
							FText::Format(LOCTEXT("ExposeNotBool", "Expected 'true' or 'false' for 'ExposeToLibrary', found '{0}'; 1.x ignored the setting and so does this front end."), FText::FromString(Setting.Value)));
					}
				}
				else if (Key == TEXT("librarycategories"))
				{
					LibraryCategories = Setting.Value;
				}
				else if (Key == TEXT("userexposedcaption"))
				{
					BlockRecord.bHasUserExposedCaption = true;
					BlockRecord.UserExposedCaption = Setting.Value;
					Diagnostics.Warning(
						TEXT("DSH3264"),
						Setting.Span,
						LOCTEXT("UserExposedCaption", "'UserExposedCaption' has no 2.0 spelling and is not applied; its value is kept for migration."));
				}
				else
				{
					if (Key == TEXT("backend"))
					{
						BlockRecord.bHasBackend = true;
						BlockRecord.BackendRaw = Setting.Value;
					}
					Diagnostics.Warning(
						TEXT("DSH3263"),
						Setting.Span,
						FText::Format(LOCTEXT("FunctionSettingIgnored", "'{0}' is not a setting of '{1}'; 1.x ignored it and so does this front end."), FText::FromString(Setting.Key), FText::FromString(BlockWord)));
				}
			}

			// Outputs: the first one (1.x order) named `Result` is the return value; the rest are `out` parameters
			// after the inputs (batch-2 contract 2.4). A layer's material output is its `inout material`.
			int32 ReturnOutput = INDEX_NONE;
			int32 MaterialOutput = INDEX_NONE;
			if (bLayer || bLayerBlend)
			{
				for (int32 ItemIndex = 0; ItemIndex < Outputs.Num(); ++ItemIndex)
				{
					if (Outputs[ItemIndex].Type.IsMaterial())
					{
						MaterialOutput = ItemIndex;
						break;
					}
				}
				if (MaterialOutput == INDEX_NONE)
				{
					Diagnostics.Error(
						TEXT("DSH3278"),
						HeaderSpan,
						FText::Format(LOCTEXT("LayerWithoutMaterial", "Expected a MaterialAttributes output on '{0}', found none."), FText::FromString(BlockName)));
				}
			}
			else if (OutputNames.Num() > 0 && OutputNames[0].Equals(TEXT("Result"), ESearchCase::CaseSensitive))
			{
				for (int32 ItemIndex = 0; ItemIndex < Outputs.Num(); ++ItemIndex)
				{
					if (Outputs[ItemIndex].Name.Equals(TEXT("Result"), ESearchCase::CaseSensitive))
					{
						ReturnOutput = ItemIndex;
						break;
					}
				}
			}

			// The layer's material input, when 1.x gave it a name of its own; see below the body.
			TOptional<FParam> RenamedLayerInput;
			// `opt` on it: the `inout` parameter is that input, and says so in its place.
			bool bLayerInputOptional = false;
			for (FParam& Input : Inputs)
			{
				if (bLayer && Input.Type.IsMaterial())
				{
					bLayerInputOptional = bLayerInputOptional || Input.bOptional;
					// A layer has one material in and the same one out; 2.0 spells both as the `inout` parameter.
					if (MaterialOutput != INDEX_NONE && !Input.Name.Equals(Outputs[MaterialOutput].Name, ESearchCase::CaseSensitive))
					{
						Diagnostics.Warning(
							TEXT("DSH3277"),
							Input.Span,
							FText::Format(
								LOCTEXT("LayerInputRenamed", "The layer input '{0}' becomes the 'inout material' parameter named after the output '{1}', so the input pin changes its name."),
								FText::FromString(Input.Name),
								FText::FromString(Outputs[MaterialOutput].Name)));
						// 1.x kept its values by name ignoring case, so a name that differs in case only was the output's.
						if (!RenamedLayerInput.IsSet() && !Input.Name.Equals(Outputs[MaterialOutput].Name, ESearchCase::IgnoreCase))
						{
							RenamedLayerInput = MoveTemp(Input);
						}
					}
					continue;
				}
				Function->Params.Add(MoveTemp(Input));
			}

			for (int32 ItemIndex = 0; ItemIndex < Outputs.Num(); ++ItemIndex)
			{
				if (ItemIndex == ReturnOutput)
				{
					Function->ReturnType = Outputs[ItemIndex].Type;
					continue;
				}
				if (ItemIndex == MaterialOutput)
				{
					continue;
				}
				Function->Params.Add(MoveTemp(Outputs[ItemIndex]));
			}
			if (MaterialOutput != INDEX_NONE)
			{
				FParam MaterialParam = MoveTemp(Outputs[MaterialOutput]);
				MaterialParam.Direction = EParamDirection::InOut;
				MaterialParam.bOptional = bLayer && bLayerInputOptional;
				Function->Params.Add(MoveTemp(MaterialParam));
			}

			Function->Body = Graph.IsValid() ? MoveTemp(Graph) : MakeUnique<FBlockStmt>();
			LegacyParser::FoldLegacyOutputRedeclarations(Function->Body->Statements, OutputNames, LegacyInfo);

			if (RenamedLayerInput.IsSet() && MaterialOutput != INDEX_NONE && Function->Params.Num() > 0)
			{
				// 1.x kept the two apart: the input under its own name, and the output starting as the empty
				// MakeMaterialAttributes it seeded every material output with that no input shares the name of. The
				// `inout` parameter is both, so the body opens by taking the input out of it under its 1.x name and
				// emptying it: `material Base = Attrs; Attrs = <empty>;`.
				const FParam& LayerInput = RenamedLayerInput.GetValue();
				const FParam& LayerResult = Function->Params.Last();

				TUniquePtr<FVarDeclStmt> InputLocal = MakeUnique<FVarDeclStmt>();
				InputLocal->Type = LayerInput.Type;
				InputLocal->Span = LayerInput.Span;

				FDeclarator InputDeclarator;
				InputDeclarator.Name = LayerInput.Name;
				InputDeclarator.NameSpan = LayerInput.NameSpan;
				InputDeclarator.Span = LayerInput.Span;
				InputDeclarator.Initializer = LegacyAst::MakeIdentifier(LayerResult.Name, LayerInput.Span);
				InputLocal->Declarators.Add(MoveTemp(InputDeclarator));

				// Both lines sit where the input was declared: they are one thought, and the blank lines the source has
				// between its Inputs and its Outputs are not between them.
				TUniquePtr<FAssignExpr> Empty = MakeUnique<FAssignExpr>();
				Empty->Op = EAssignOp::Assign;
				Empty->Target = LegacyAst::MakeIdentifier(LayerResult.Name, LayerInput.Span);
				Empty->Value = MakeLegacyZeroInitializer(LayerResult.Type, LayerInput.Span);
				Empty->Span = LayerInput.Span;

				TUniquePtr<FExprStmt> EmptyStatement = MakeUnique<FExprStmt>();
				EmptyStatement->Expression = MoveTemp(Empty);
				EmptyStatement->Span = LayerInput.Span;

				Function->Body->Statements.Insert(MoveTemp(EmptyStatement), 0);
				Function->Body->Statements.Insert(MoveTemp(InputLocal), 0);
			}

			if (ReturnOutput != INDEX_NONE)
			{
				// 1.x had no `return`: its Graph assigns the output named `Result`. 2.0 spells that output as a local the
				// body returns, zero-initialised the way 1.x initialised every declared output.
				const FParam& ResultOutput = Outputs[ReturnOutput];

				TUniquePtr<FVarDeclStmt> ResultLocal = MakeUnique<FVarDeclStmt>();
				ResultLocal->Type = ResultOutput.Type;
				ResultLocal->Span = ResultOutput.Span;

				FDeclarator Declarator;
				Declarator.Name = ResultOutput.Name;
				Declarator.NameSpan = ResultOutput.NameSpan;
				Declarator.Span = ResultOutput.Span;
				Declarator.Initializer = MakeLegacyZeroInitializer(ResultLocal->Type, ResultOutput.Span);
				if (Declarator.Initializer.IsValid() && LegacyInfo)
				{
					FLegacySynthesizedInitializer& Record = LegacyInfo->SynthesizedInitializers.AddDefaulted_GetRef();
					Record.Declaration = ResultLocal.Get();
					Record.Name = Declarator.Name;
					Record.Span = Declarator.Span;
				}
				ResultLocal->Declarators.Add(MoveTemp(Declarator));
				Function->Body->Statements.Insert(MoveTemp(ResultLocal), 0);

				TUniquePtr<FReturnStmt> Return = MakeUnique<FReturnStmt>();
				Return->Span = ResultOutput.Span;
				Return->Value = LegacyAst::MakeIdentifier(ResultOutput.Name, ResultOutput.Span);
				Function->Body->Statements.Add(MoveTemp(Return));
			}

			LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("name"), BlockName);
			if (Block.bHasRoot)
			{
				LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("root"), Block.Root);
			}
			if (bLayer)
			{
				LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("layer"), FString());
			}
			if (bLayerBlend)
			{
				LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("layerblend"), FString());
			}
			if (bHasDescription)
			{
				LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("desc"), Description);
			}
			if (bExposeToLibrary && !LibraryCategories.IsEmpty())
			{
				LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("library"), LibraryCategories);
			}
			for (const FString& StaticInput : LegacyStaticBoolInputs)
			{
				LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("static"), StaticInput);
			}
			LegacyStaticBoolInputs.Reset();
			for (const TPair<FString, FString>& Doc : ParamDocs)
			{
				if (!(ReturnOutput != INDEX_NONE && Doc.Key.Equals(TEXT("Result"), ESearchCase::CaseSensitive)))
				{
					LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("param"), Doc.Key + TEXT(" ") + Doc.Value);
				}
			}

			BlockRecord.InputNames = InputNames;
			BlockRecord.OutputNames = OutputNames;

			for (FDeclPtr& Property : PropertyDecls)
			{
				OutDecls.Add(MoveTemp(Property));
			}
			OutDecls.Add(MoveTemp(Function));
		}

		for (FDeclPtr& Layout : LayoutDecls)
		{
			OutDecls.Add(MoveTemp(Layout));
		}

		if (LegacyInfo)
		{
			for (const FPendingSection& Pending : Sections)
			{
				FLegacySection& Section = LegacyInfo->Sections.AddDefaulted_GetRef();
				Section.Block = MainDecl;
				Section.Name = Pending.Name;
				Section.HeaderSpan = Pending.HeaderSpan;
				Section.BodySpan = Pending.BodySpan;
				Section.FirstNode = Pending.bSettings ? SettingsNode
					: Pending.bGraph ? (GraphFirstNode ? GraphFirstNode : MainDecl)
					: Pending.bOwnedByDeclaration ? MainDecl
					: Pending.FirstNode;
			}

			for (int32 ItemIndex = FirstParameterDeclaration; ItemIndex < LegacyInfo->ParameterDeclarations.Num(); ++ItemIndex)
			{
				LegacyInfo->ParameterDeclarations[ItemIndex].Block = MainDecl;
			}

			BlockRecord.Decl = MainDecl;
			LegacyInfo->Blocks.Add(MoveTemp(BlockRecord));
		}

		// The block is consumed whatever was wrong inside it; returning true keeps the module loop from
		// skipping the construct that follows.
		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// Function / GraphFunction
	// ---------------------------------------------------------------------------------------------

	namespace LegacyParser
	{
		struct FLegacyBodyRename
		{
			/** Into the body as written (between the braces). */
			int32 Offset = 0;
			int32 Length = 0;
			FString From;
			FString To;
		};

		static bool IsLegacyBodyIdentifierStart(const TCHAR Character)
		{
			return FChar::IsAlpha(Character) || Character == TEXT('_');
		}

		static bool IsLegacyBodyIdentifierPart(const TCHAR Character)
		{
			return FChar::IsAlnum(Character) || Character == TEXT('_');
		}

		/**
		 * 1.x NormalizeShaderLanguageText: whole identifiers matched ignoring case against the GLSL aliases,
		 * and `A::B` (no spaces) flattened with SanitizeIdentifier; `//` and block comments and "strings" are
		 * left alone. No line is added or removed, so a shader error still maps to its source line.
		 */
		static FString NormalizeLegacyFunctionBody(const FString& Body, TArray<FLegacyBodyRename>& OutRenames)
		{
			static const TCHAR* const Aliases[][2] =
			{
				{ TEXT("vec2"), TEXT("float2") }, { TEXT("vec3"), TEXT("float3") }, { TEXT("vec4"), TEXT("float4") },
				{ TEXT("ivec2"), TEXT("int2") }, { TEXT("ivec3"), TEXT("int3") }, { TEXT("ivec4"), TEXT("int4") },
				{ TEXT("uvec2"), TEXT("uint2") }, { TEXT("uvec3"), TEXT("uint3") }, { TEXT("uvec4"), TEXT("uint4") },
				{ TEXT("bvec2"), TEXT("bool2") }, { TEXT("bvec3"), TEXT("bool3") }, { TEXT("bvec4"), TEXT("bool4") },
				{ TEXT("mat2"), TEXT("float2x2") }, { TEXT("mat3"), TEXT("float3x3") }, { TEXT("mat4"), TEXT("float4x4") },
				{ TEXT("mix"), TEXT("lerp") }, { TEXT("fract"), TEXT("frac") }, { TEXT("mod"), TEXT("fmod") },
			};

			const int32 Length = Body.Len();
			FString Out;
			Out.Reserve(Length + 16);

			bool bInString = false;
			bool bInLineComment = false;
			bool bInBlockComment = false;
			int32 Index = 0;
			while (Index < Length)
			{
				const TCHAR Character = Body[Index];
				const TCHAR Next = Index + 1 < Length ? Body[Index + 1] : TEXT('\0');

				if (bInLineComment)
				{
					Out.AppendChar(Character);
					bInLineComment = Character != TEXT('\n');
					++Index;
					continue;
				}
				if (bInBlockComment)
				{
					Out.AppendChar(Character);
					if (Character == TEXT('*') && Next == TEXT('/'))
					{
						Out.AppendChar(Next);
						bInBlockComment = false;
						Index += 2;
					}
					else
					{
						++Index;
					}
					continue;
				}
				if (bInString)
				{
					Out.AppendChar(Character);
					if (Character == TEXT('\\') && Index + 1 < Length)
					{
						Out.AppendChar(Next);
						Index += 2;
						continue;
					}
					bInString = Character != TEXT('"');
					++Index;
					continue;
				}
				if (Character == TEXT('"'))
				{
					bInString = true;
					Out.AppendChar(Character);
					++Index;
					continue;
				}
				if (Character == TEXT('/') && Next == TEXT('/'))
				{
					bInLineComment = true;
					Out.AppendChar(Character);
					Out.AppendChar(Next);
					Index += 2;
					continue;
				}
				if (Character == TEXT('/') && Next == TEXT('*'))
				{
					bInBlockComment = true;
					Out.AppendChar(Character);
					Out.AppendChar(Next);
					Index += 2;
					continue;
				}

				if (!IsLegacyBodyIdentifierStart(Character))
				{
					Out.AppendChar(Character);
					++Index;
					continue;
				}

				const int32 Start = Index;
				++Index;
				while (Index < Length && IsLegacyBodyIdentifierPart(Body[Index]))
				{
					++Index;
				}
				const FString Identifier = Body.Mid(Start, Index - Start);

				FString Qualified = Identifier;
				int32 QualifiedEnd = Index;
				bool bQualified = false;
				while (QualifiedEnd + 2 < Length
					&& Body[QualifiedEnd] == TEXT(':')
					&& Body[QualifiedEnd + 1] == TEXT(':')
					&& IsLegacyBodyIdentifierStart(Body[QualifiedEnd + 2]))
				{
					int32 NextEnd = QualifiedEnd + 3;
					while (NextEnd < Length && IsLegacyBodyIdentifierPart(Body[NextEnd]))
					{
						++NextEnd;
					}
					Qualified += TEXT("::") + Body.Mid(QualifiedEnd + 2, NextEnd - (QualifiedEnd + 2));
					QualifiedEnd = NextEnd;
					bQualified = true;
				}

				if (bQualified)
				{
					FLegacyBodyRename& Rename = OutRenames.AddDefaulted_GetRef();
					Rename.Offset = Start;
					Rename.Length = QualifiedEnd - Start;
					Rename.From = Qualified;
					Rename.To = LegacyAst::SanitizeIdentifier(Qualified);
					Out += Rename.To;
					Index = QualifiedEnd;
					continue;
				}

				const FString Lower = Identifier.ToLower();
				const TCHAR* Replacement = nullptr;
				for (const auto& Alias : Aliases)
				{
					if (Lower.Equals(Alias[0], ESearchCase::CaseSensitive))
					{
						Replacement = Alias[1];
						break;
					}
				}
				if (Replacement)
				{
					FLegacyBodyRename& Rename = OutRenames.AddDefaulted_GetRef();
					Rename.Offset = Start;
					Rename.Length = Index - Start;
					Rename.From = Identifier;
					Rename.To = Replacement;
					Out += Replacement;
				}
				else
				{
					Out += Identifier;
				}
			}
			return Out;
		}

		/** Skips `//`, block comments, "strings" and 'characters' from Index; true when it moved. */
		static bool SkipLegacyBodyTrivia(const FString& Body, int32& Index)
		{
			const int32 Length = Body.Len();
			const TCHAR Character = Body[Index];
			const TCHAR Next = Index + 1 < Length ? Body[Index + 1] : TEXT('\0');
			if (Character == TEXT('/') && Next == TEXT('/'))
			{
				while (Index < Length && Body[Index] != TEXT('\n'))
				{
					++Index;
				}
				return true;
			}
			if (Character == TEXT('/') && Next == TEXT('*'))
			{
				const int32 Close = Body.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index + 2);
				Index = Close == INDEX_NONE ? Length : Close + 2;
				return true;
			}
			if (Character == TEXT('"') || Character == TEXT('\''))
			{
				++Index;
				while (Index < Length && Body[Index] != Character && Body[Index] != TEXT('\n'))
				{
					Index += Body[Index] == TEXT('\\') ? 2 : 1;
				}
				Index = FMath::Min(Index + 1, Length);
				return true;
			}
			return false;
		}

		/** A depth-zero `return;` (1.x DSH3012 when the function has a return type); INDEX_NONE when there is none. */
		static int32 FindLegacyBareReturn(const FString& Body)
		{
			const int32 Length = Body.Len();
			int32 Depth = 0;
			int32 Index = 0;
			while (Index < Length)
			{
				if (SkipLegacyBodyTrivia(Body, Index))
				{
					continue;
				}
				const TCHAR Character = Body[Index];
				if (Character == TEXT('{'))
				{
					++Depth;
				}
				else if (Character == TEXT('}'))
				{
					Depth = FMath::Max(0, Depth - 1);
				}
				else if (Depth == 0 && Character == TEXT('r') && Body.Mid(Index, 6).Equals(TEXT("return"), ESearchCase::CaseSensitive)
					&& (Index == 0 || !IsLegacyBodyIdentifierPart(Body[Index - 1]))
					&& (Index + 6 >= Length || !IsLegacyBodyIdentifierPart(Body[Index + 6])))
				{
					int32 Probe = Index + 6;
					while (Probe < Length && FChar::IsWhitespace(Body[Probe]))
					{
						++Probe;
					}
					if (Probe < Length && Body[Probe] == TEXT(';'))
					{
						return Index;
					}
				}
				++Index;
			}
			return INDEX_NONE;
		}

		struct FLegacyHoistSite
		{
			int32 Offset = 0;
			int32 Length = 0;
		};

		/**
		 * The `UE.Name(...)` calls of an opaque body, as 1.x found them in a GraphFunction: `UE.` after an identifier
		 * boundary (ignoring case for a 1.x body, exact for a `/// @custom` one), then an identifier, optional
		 * whitespace and a `(` with a matching `)`; comments, strings and character literals skipped. OutSubstrate
		 * receives `Substrate.Name(` sites, which are never lifted. False at an unterminated call (OutUnterminated is
		 * its offset).
		 */
		static bool ScanLegacyHoistSites(const FString& Body, const bool bIgnoreCase, TArray<FLegacyHoistSite>& OutSites, TArray<int32>& OutSubstrate, int32& OutUnterminated)
		{
			const int32 Length = Body.Len();
			int32 Index = 0;
			while (Index < Length)
			{
				if (SkipLegacyBodyTrivia(Body, Index))
				{
					continue;
				}

				const bool bBoundary = Index == 0 || !IsLegacyBodyIdentifierPart(Body[Index - 1]);
				const bool bUE = bBoundary && Index + 3 < Length
					&& (Body[Index] == TEXT('U') || (bIgnoreCase && Body[Index] == TEXT('u')))
					&& (Body[Index + 1] == TEXT('E') || (bIgnoreCase && Body[Index + 1] == TEXT('e')))
					&& Body[Index + 2] == TEXT('.');
				const bool bSubstrate = bBoundary && Body.Mid(Index, 10).Equals(TEXT("Substrate."), ESearchCase::CaseSensitive);
				const int32 NameStart = bUE ? Index + 3 : (bSubstrate ? Index + 10 : INDEX_NONE);

				if (NameStart != INDEX_NONE && NameStart < Length && IsLegacyBodyIdentifierStart(Body[NameStart]))
				{
					int32 Cursor = NameStart + 1;
					while (Cursor < Length && IsLegacyBodyIdentifierPart(Body[Cursor]))
					{
						++Cursor;
					}
					while (Cursor < Length && FChar::IsWhitespace(Body[Cursor]))
					{
						++Cursor;
					}
					if (Cursor < Length && Body[Cursor] == TEXT('('))
					{
						if (bSubstrate)
						{
							OutSubstrate.Add(Index);
							Index = Cursor;
							continue;
						}

						int32 Depth = 0;
						int32 Close = INDEX_NONE;
						int32 Scan = Cursor;
						while (Scan < Length)
						{
							if (Body[Scan] == TEXT('"'))
							{
								SkipLegacyBodyTrivia(Body, Scan);
								continue;
							}
							if (Body[Scan] == TEXT('('))
							{
								++Depth;
							}
							else if (Body[Scan] == TEXT(')') && --Depth == 0)
							{
								Close = Scan;
								break;
							}
							++Scan;
						}
						if (Close == INDEX_NONE)
						{
							OutUnterminated = Index;
							return false;
						}

						// A `/// @custom` body selects an output the way the language does -- `UE.X(...).Name`,
						// `UE.X(...)[k]` -- and the selection is part of the lifted call: the input carries that output. 1.x
						// said it inside the parentheses (`Output = "Min"`). A member followed by `(` is HLSL's own.
						int32 SiteEnd = Close + 1;
						while (!bIgnoreCase)
						{
							int32 Probe = SiteEnd;
							while (Probe < Length && (Body[Probe] == TEXT(' ') || Body[Probe] == TEXT('\t')))
							{
								++Probe;
							}
							if (Probe + 1 < Length && Body[Probe] == TEXT('.') && IsLegacyBodyIdentifierStart(Body[Probe + 1]))
							{
								int32 NameEnd = Probe + 2;
								while (NameEnd < Length && IsLegacyBodyIdentifierPart(Body[NameEnd]))
								{
									++NameEnd;
								}
								int32 After = NameEnd;
								while (After < Length && FChar::IsWhitespace(Body[After]))
								{
									++After;
								}
								if (After < Length && Body[After] == TEXT('('))
								{
									break;
								}
								SiteEnd = NameEnd;
								continue;
							}
							if (Probe < Length && Body[Probe] == TEXT('['))
							{
								int32 Digits = Probe + 1;
								while (Digits < Length && (Body[Digits] == TEXT(' ') || Body[Digits] == TEXT('\t')))
								{
									++Digits;
								}
								const int32 FirstDigit = Digits;
								while (Digits < Length && FChar::IsDigit(Body[Digits]))
								{
									++Digits;
								}
								int32 CloseBracket = Digits;
								while (CloseBracket < Length && (Body[CloseBracket] == TEXT(' ') || Body[CloseBracket] == TEXT('\t')))
								{
									++CloseBracket;
								}
								if (Digits > FirstDigit && CloseBracket < Length && Body[CloseBracket] == TEXT(']'))
								{
									SiteEnd = CloseBracket + 1;
									continue;
								}
							}
							break;
						}

						FLegacyHoistSite& Site = OutSites.AddDefaulted_GetRef();
						Site.Offset = Index;
						Site.Length = SiteEnd - Index;
						Index = SiteEnd;
						continue;
					}
				}
				++Index;
			}
			return true;
		}

		/** The Custom input a hoisted call becomes: `_ds_<Sanitized function>_UE<k>`, then `_1`, `_2`... against the `in` parameters and earlier hoists, ignoring case. */
		static FString MakeLegacyHoistInputName(const FString& QualifiedFunctionName, const int32 Ordinal, const TArray<FString>& Taken)
		{
			const FString Base = LegacyAst::SanitizeIdentifier(FString::Printf(TEXT("__ds_%s_UE%d"), *QualifiedFunctionName, Ordinal));
			FString Candidate = Base;
			for (int32 Suffix = 1;; ++Suffix)
			{
				bool bTaken = false;
				for (const FString& Name : Taken)
				{
					if (Name.Equals(Candidate, ESearchCase::IgnoreCase))
					{
						bTaken = true;
						break;
					}
				}
				if (!bTaken)
				{
					return Candidate;
				}
				Candidate = FString::Printf(TEXT("%s_%d"), *Base, Suffix);
			}
		}

		static void AddLegacyNamespaceFlatten(FLegacyMigrationInfo* Info, const FString& From, const FString& To)
		{
			if (!Info)
			{
				return;
			}
			for (const TPair<FString, FString>& Entry : Info->NamespaceFlatten)
			{
				if (Entry.Key.Equals(From, ESearchCase::CaseSensitive))
				{
					return;
				}
			}
			Info->NamespaceFlatten.Emplace(From, To);
		}

		static bool IsLegacyIdentifierName(const FString& Text)
		{
			if (Text.IsEmpty() || !IsLegacyBodyIdentifierStart(Text[0]))
			{
				return false;
			}
			for (const TCHAR Character : Text)
			{
				if (!IsLegacyBodyIdentifierPart(Character))
				{
					return false;
				}
			}
			return true;
		}
	}

	FDeclPtr FLangParser::ParseLegacyFunction(const FString& NamespaceName)
	{
		const int32 StartIndex = GetTokenIndex();
		const FLangToken& WordToken = Advance();
		const FString Word = WordToken.Text;
		const bool bGraph = Word.Equals(TEXT("GraphFunction"), ESearchCase::CaseSensitive);

		TUniquePtr<FFunctionDecl> Function = MakeUnique<FFunctionDecl>();
		Function->bLegacy = true;
		Function->Linkage = EFunctionLinkage::Internal;
		Function->Doc = MoveTemp(LegacyPendingDoc);
		LegacyPendingDoc = FDocBlock();

		struct FLegacyRenameScope
		{
			const FDecl*& Slot;
			const FDecl* Saved;
			~FLegacyRenameScope() { Slot = Saved; }
		};
		FLegacyRenameScope RenameScope{ LegacyRenameDecl, LegacyRenameDecl };
		LegacyRenameDecl = Function.Get();

		// -- `SelfContained` / `Inline`
		bool bSelfContained = false;
		if (Check(ELangTokenKind::Identifier)
			&& (Current().Text.Equals(TEXT("SelfContained"), ESearchCase::IgnoreCase) || Current().Text.Equals(TEXT("Inline"), ESearchCase::IgnoreCase)))
		{
			const FLangToken& Modifier = Current();
			if (bGraph)
			{
				if (Peek(1).Kind == ELangTokenKind::Identifier)
				{
					// 1.x skipped the modifier check for GraphFunction and read the word as a return type.
					Diagnostics.Error(
						TEXT("DSH6307"),
						Modifier.Span,
						FText::Format(LOCTEXT("GraphModifier", "Expected a return type or a name after 'GraphFunction', found the modifier '{0}', which only a Function takes."), FText::FromString(Modifier.Text)));
					Advance();
				}
			}
			else if (Peek(1).Kind == ELangTokenKind::LeftParen)
			{
				Diagnostics.Error(
					TEXT("DSH6300"),
					Peek(1).Span,
					FText::Format(LOCTEXT("NameAfterModifier", "Expected a function name after '{0}', found '('."), FText::FromString(Modifier.Text)));
				return nullptr;
			}
			else
			{
				bSelfContained = true;
				if (Modifier.Text.Equals(TEXT("Inline"), ESearchCase::IgnoreCase))
				{
					Diagnostics.Warning(
						TEXT("DSH6306"),
						Modifier.Span,
						LOCTEXT("InlineModifier", "'Inline' is the old spelling of 'SelfContained'; the function becomes '@custom selfcontained'."));
				}
				Advance();
			}
		}

		// -- `[ReturnType] Name`
		if (!Check(ELangTokenKind::Identifier))
		{
			Diagnostics.Error(
				TEXT("DSH6300"),
				Current().Span,
				FText::Format(LOCTEXT("FunctionName", "Expected a function name after '{0}', found {1}."), FText::FromString(Word), DescribeToken(Current())));
			return nullptr;
		}
		const FLangToken& FirstWord = Advance();
		bool bHasReturnType = false;
		FString Name;
		FLangSpan NameSpan;
		if (Check(ELangTokenKind::LeftParen))
		{
			Name = FirstWord.Text;
			NameSpan = FirstWord.Span;
			ClassifyTypeName(TEXT("void"), Function->ReturnType);
			Function->ReturnType.Span = FirstWord.Span;
		}
		else if (Check(ELangTokenKind::Identifier))
		{
			bHasReturnType = true;
			ClassifyLegacyTypeName(FirstWord.Text, Function->ReturnType);
			Function->ReturnType.Span = FirstWord.Span;
			RecordLegacyRename(FLegacyRename::EKind::TypeSpelling, FirstWord.Text, Function->ReturnType.Name, FirstWord.Span);
			const FLangToken& NameToken = Advance();
			Name = NameToken.Text;
			NameSpan = NameToken.Span;
		}
		else
		{
			Diagnostics.Error(
				TEXT("DSH6300"),
				Current().Span,
				FText::Format(LOCTEXT("FunctionNameOrParen", "Expected '(' or a function name after '{0}', found {1}."), FText::FromString(FirstWord.Text), DescribeToken(Current())));
			return nullptr;
		}

		if (!Check(ELangTokenKind::LeftParen))
		{
			Diagnostics.Error(
				TEXT("DSH6319"),
				Current().Span,
				FText::Format(LOCTEXT("FunctionParamsOpen", "Expected '(' to open the parameter list of '{0}', found {1}."), FText::FromString(Name), DescribeToken(Current())));
			return nullptr;
		}

		const FString Qualified = NamespaceName.IsEmpty() ? Name : NamespaceName + TEXT("::") + Name;
		Function->Name = NamespaceName.IsEmpty() ? Name : LegacyAst::SanitizeIdentifier(Qualified);
		Function->NameSpan = NameSpan;
		if (!NamespaceName.IsEmpty())
		{
			Function->LegacyQualifiedName = Qualified;
			LegacyParser::AddLegacyNamespaceFlatten(LegacyInfo, Qualified, Function->Name);
			RecordLegacyRename(FLegacyRename::EKind::NamespaceQualifier, Qualified, Function->Name, NameSpan);
		}

		// -- `( [in|out] Type Name, ... )`: 1.x split each parameter on whitespace into two or three words
		Advance(); // (
		int32 OutCount = 0;
		bool bSignatureOk = true;
		TArray<FString> InputNames;
		TArray<FString> OutputNames;
		while (!Check(ELangTokenKind::RightParen))
		{
			if (AtEnd())
			{
				FailAtEnd(FText::Format(LOCTEXT("WhileParams", "the parameter list of '{0}'"), FText::FromString(Name)));
				return nullptr;
			}
			if (Match(ELangTokenKind::Comma))
			{
				continue;
			}

			const int32 ParamStart = GetTokenIndex();
			TArray<const FLangToken*> Words;
			bool bWordsOk = true;
			while (!AtEnd() && !Check(ELangTokenKind::Comma) && !Check(ELangTokenKind::RightParen))
			{
				const FLangToken& Part = Advance();
				bWordsOk &= Part.Kind == ELangTokenKind::Identifier || Part.Kind == ELangTokenKind::Keyword;
				Words.Add(&Part);
			}
			const FLangSpan ParamSpan = SpanFrom(ParamStart);

			if (!bWordsOk || Words.Num() < 2 || Words.Num() > 3)
			{
				Diagnostics.Error(
					TEXT("DSH6301"),
					ParamSpan,
					FText::Format(LOCTEXT("ParamWords", "Expected '[in|out] Type Name' in the parameter list of '{0}', found '{1}'."), FText::FromString(Name), FText::FromString(Slice(ParamSpan))));
				bSignatureOk = false;
				continue;
			}

			const FString Qualifier = Words.Num() == 3 ? Words[0]->Text.ToLower() : FString(TEXT("in"));
			const FLangToken& TypeToken = *Words[Words.Num() - 2];
			const FLangToken& NameToken = *Words.Last();
			if (Qualifier != TEXT("in") && Qualifier != TEXT("out"))
			{
				Diagnostics.Error(
					TEXT("DSH6302"),
					Words[0]->Span,
					FText::Format(
						LOCTEXT("ParamQualifier", "Expected 'in' or 'out' before the parameter '{0}' of '{1}', found '{2}'; a 1.x function has no 'inout'."),
						FText::FromString(NameToken.Text),
						FText::FromString(Name),
						FText::FromString(Words[0]->Text)));
				bSignatureOk = false;
				continue;
			}
			if (NameToken.Text.Equals(TEXT("__return"), ESearchCase::IgnoreCase))
			{
				Diagnostics.Error(
					TEXT("DSH6303"),
					NameToken.Span,
					FText::Format(LOCTEXT("ReturnParamName", "Expected a parameter name other than '__return', which 1.x reserved, in '{0}'."), FText::FromString(Name)));
				bSignatureOk = false;
				continue;
			}

			FParam Param;
			Param.Direction = Qualifier == TEXT("out") ? EParamDirection::Out : EParamDirection::In;
			ClassifyLegacyTypeName(TypeToken.Text, Param.Type);
			Param.Type.Span = TypeToken.Span;
			RecordLegacyRename(FLegacyRename::EKind::TypeSpelling, TypeToken.Text, Param.Type.Name, TypeToken.Span);
			Param.Name = NameToken.Text;
			Param.NameSpan = NameToken.Span;
			Param.Span = ParamSpan;
			if (Param.Direction == EParamDirection::Out)
			{
				++OutCount;
				OutputNames.Add(Param.Name);
			}
			else
			{
				InputNames.Add(Param.Name);
			}
			Function->Params.Add(MoveTemp(Param));
		}
		Advance(); // )
		const FLangSpan HeaderSpan = SpanFrom(StartIndex);

		if (bSignatureOk && bHasReturnType && OutCount > 0)
		{
			Diagnostics.Error(
				TEXT("DSH6304"),
				HeaderSpan,
				FText::Format(LOCTEXT("ReturnAndOut", "Expected either a return type or 'out' parameters on '{0}', found both."), FText::FromString(Name)));
		}
		if (bSignatureOk && !bHasReturnType && OutCount == 0)
		{
			Diagnostics.Error(
				TEXT("DSH6305"),
				HeaderSpan,
				FText::Format(LOCTEXT("NoResult", "Expected '{0}' to return a value or to have at least one 'out' parameter, found neither."), FText::FromString(Name)));
		}

		// -- the body: HLSL, captured verbatim and normalised the 1.x way
		if (!Check(ELangTokenKind::LeftBrace))
		{
			Diagnostics.Error(
				TEXT("DSH6319"),
				Current().Span,
				FText::Format(LOCTEXT("FunctionBodyOpen", "Expected '`{' to open the body of '{0}', found {1}."), FText::FromString(Name), DescribeToken(Current())));
			return nullptr;
		}

		FString Raw;
		FLangSpan BodySpan;
		if (!CaptureRawBody(Raw, BodySpan))
		{
			return nullptr;
		}
		const int32 BodyContentOffset = BodySpan.Offset + 1;

		TArray<LegacyParser::FLegacyBodyRename> Renames;
		const FString Normalized = LegacyParser::NormalizeLegacyFunctionBody(Raw, Renames);
		for (const LegacyParser::FLegacyBodyRename& Rename : Renames)
		{
			const bool bQualifier = Rename.From.Contains(TEXT("::"));
			if (bQualifier)
			{
				LegacyParser::AddLegacyNamespaceFlatten(LegacyInfo, Rename.From, Rename.To);
			}
			RecordLegacyRename(
				bQualifier ? FLegacyRename::EKind::NamespaceQualifier : FLegacyRename::EKind::GlslAlias,
				Rename.From,
				Rename.To,
				Source.MakeSpan(BodyContentOffset + Rename.Offset, Rename.Length));
		}

		if (bHasReturnType)
		{
			const int32 BareReturn = LegacyParser::FindLegacyBareReturn(Raw);
			if (BareReturn != INDEX_NONE)
			{
				Diagnostics.Error(
					TEXT("DSH6308"),
					Source.MakeSpan(BodyContentOffset + BareReturn, 6),
					FText::Format(
						LOCTEXT("BareReturn", "Expected a value after 'return' in '{0}', which returns '{1}', found a bare 'return;'."),
						FText::FromString(Name),
						FText::FromString(Function->ReturnType.Name)));
			}
		}

		Function->bOpaqueBody = true;
		Function->RawBody = Normalized;
		Function->BodySpan = BodySpan;
		Function->Span = SpanFrom(StartIndex);
		LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("custom"), bSelfContained ? FString(TEXT("selfcontained")) : FString());

		// -- GraphFunction: every `UE.*` call becomes a Custom input, its text kept in RawBody at a recorded offset
		if (bGraph)
		{
			LiftCallsOutOfOpaqueBody(*Function, Qualified, BodyContentOffset, InputNames);
		}

		bLegacySawConstruct = true;
		LegacyUsedNames.Add(Function->Name);

		if (LegacyInfo)
		{
			FLegacyBlock& Record = LegacyInfo->Blocks.AddDefaulted_GetRef();
			Record.BlockWord = Word;
			Record.Decl = Function.Get();
			Record.Name = Qualified;
			Record.HeaderSpan = HeaderSpan;
			Record.InputNames = InputNames;
			Record.OutputNames = bHasReturnType ? TArray<FString>{ TEXT("__return") } : OutputNames;
		}

		return Function;
	}

	// ---------------------------------------------------------------------------------------------
	// Calls lifted out of an opaque body (rule L8; plan section 11 #19 for a `/// @custom` function)
	// ---------------------------------------------------------------------------------------------

	void FLangParser::LiftCallsOutOfOpaqueBody(FFunctionDecl& Function, const FString& QualifiedName, const int32 BodyContentOffset, const TArray<FString>& InputNames)
	{
		// 1.x matched `UE.` ignoring case; a 2.0 body spells the namespace the way the language does.
		const bool bLegacy = IsLegacyMode();
		const FString& Body = Function.RawBody;

		TArray<LegacyParser::FLegacyHoistSite> Sites;
		TArray<int32> SubstrateSites;
		int32 Unterminated = INDEX_NONE;
		if (!LegacyParser::ScanLegacyHoistSites(Body, bLegacy, Sites, SubstrateSites, Unterminated))
		{
			Diagnostics.Error(
				TEXT("DSH6314"),
				Source.MakeSpan(BodyContentOffset + Unterminated, 3),
				FText::Format(LOCTEXT("HoistUnterminated", "Expected a ')' to close the 'UE.' call in the body of '{0}', found the end of the body."), FText::FromString(QualifiedName)));
		}
		for (const int32 SubstrateSite : SubstrateSites)
		{
			Diagnostics.Warning(
				TEXT("DSH6316"),
				Source.MakeSpan(BodyContentOffset + SubstrateSite, 10),
				FText::Format(LOCTEXT("SubstrateNotHoisted", "A 'Substrate.' call in the body of '{0}' is not lifted into a node, because no custom node input carries a Substrate value; it reaches the shader compiler as text."), FText::FromString(QualifiedName)));
		}
		if (Sites.Num() == 0)
		{
			return;
		}

		// A text whose offsets and lines are the file's up to the body, then the body as RawBody holds it (normalised,
		// for a 1.x function): the lifted calls parse with real line numbers (columns shift only past a rename on the
		// same line).
		const FString& Original = Source.GetText();
		FString Synthetic;
		Synthetic.Reserve(BodyContentOffset + Body.Len());
		for (int32 ItemIndex = 0; ItemIndex < BodyContentOffset && ItemIndex < Original.Len(); ++ItemIndex)
		{
			const TCHAR Character = Original[ItemIndex];
			Synthetic.AppendChar(Character == TEXT('\n') || Character == TEXT('\r') ? Character : TEXT(' '));
		}
		Synthetic += Body;
		const FLangSourceText SyntheticSource(Source.GetPath(), MoveTemp(Synthetic));

		FLangLexOptions LexOptions;
		LexOptions.bEmitDocComments = false;
		LexOptions.bEmitDirectives = false;
		LexOptions.bEmitComments = false;
		TArray<FLangToken> BodyTokens;
		FLangDiagnosticSink LexScratch(Source.GetPath());
		LexDreamShaderLang(SyntheticSource, LexOptions, BodyTokens, LexScratch);

		TArray<FString> TakenNames = InputNames;
		for (int32 Ordinal = 0; Ordinal < Sites.Num(); ++Ordinal)
		{
			const LegacyParser::FLegacyHoistSite& Site = Sites[Ordinal];
			const int32 SiteStart = BodyContentOffset + Site.Offset;
			const int32 SiteEnd = SiteStart + Site.Length;

			TArray<FLangToken> SiteTokens;
			for (const FLangToken& Token : BodyTokens)
			{
				if (Token.Kind != ELangTokenKind::EndOfFile && Token.Span.Offset >= SiteStart && Token.Span.End() <= SiteEnd)
				{
					SiteTokens.Add(Token);
				}
			}
			FLangToken EndToken;
			EndToken.Kind = ELangTokenKind::EndOfFile;
			EndToken.Span = SyntheticSource.MakeSpan(SiteEnd, 0);
			SiteTokens.Add(MoveTemp(EndToken));

			FLangParser Sub(SyntheticSource, MoveTemp(SiteTokens), bLegacy ? ELangFrontend::Legacy : Frontend, FileKind, Diagnostics);
			FExprPtr Call;
			if (bLegacy)
			{
				Sub.LegacyInfo = LegacyInfo;
				Sub.LegacyRenameDecl = &Function;
				Sub.LegacySelectionGroupKeys = MoveTemp(LegacySelectionGroupKeys);
				Call = Sub.ParseStandaloneExpression();
				LegacySelectionGroupKeys = MoveTemp(Sub.LegacySelectionGroupKeys);
				LegacySynthesizedIndexExprs.Append(Sub.LegacySynthesizedIndexExprs);
				if (Call.IsValid())
				{
					ValidateLegacyValue(*Call);
				}
			}
			else
			{
				Call = Sub.ParseStandaloneExpression();
			}

			FHoistedCall& Hoisted = Function.HoistedCalls.AddDefaulted_GetRef();
			Hoisted.InputName = LegacyParser::MakeLegacyHoistInputName(QualifiedName, Ordinal, TakenNames);
			TakenNames.Add(Hoisted.InputName);
			Hoisted.Call = MoveTemp(Call);
			Hoisted.RawBodyOffset = Site.Offset;
			Hoisted.RawBodyLength = Site.Length;
			Hoisted.Span = SyntheticSource.MakeSpan(SiteStart, Site.Length);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Namespace
	// ---------------------------------------------------------------------------------------------

	bool FLangParser::ParseLegacyNamespace(TArray<FDeclPtr>& OutDecls)
	{
		Advance(); // Namespace
		const FDocBlock NamespaceDoc = MoveTemp(LegacyPendingDoc);
		LegacyPendingDoc = FDocBlock();
		RecordSkippedDocBlock(NamespaceDoc);

		TArray<FPragmaArgument> Attributes;
		if (!ParseLegacyAttributes(Attributes))
		{
			return false;
		}

		const FPragmaArgument* NameAttribute = LegacyParser::FindLegacyAttribute(Attributes, TEXT("Name"));
		const FString NamespaceName = NameAttribute ? NameAttribute->Value.TrimStartAndEnd() : FString();
		if (NamespaceName.IsEmpty() || !LegacyParser::IsLegacyIdentifierName(NamespaceName))
		{
			Diagnostics.Error(
				TEXT("DSH6309"),
				NameAttribute ? NameAttribute->Span : Previous().Span,
				NamespaceName.IsEmpty()
					? LOCTEXT("NamespaceNoName", "Expected a 'Name = \"...\"' attribute with a name on 'Namespace', found none.")
					: FText::Format(LOCTEXT("NamespaceNotIdentifier", "Expected the namespace name to be an identifier, found '{0}'."), FText::FromString(NamespaceName)));
			LegacyParser::SkipLegacyBlockRemainder(*this);
			return true;
		}

		if (!Check(ELangTokenKind::LeftBrace))
		{
			return Diagnostics.Error(
				TEXT("DSH2257"),
				Current().Span,
				FText::Format(LOCTEXT("NamespaceOpen", "Expected '`{' to open the namespace '{0}', found {1}."), FText::FromString(NamespaceName), DescribeToken(Current())));
		}
		Advance(); // {
		bLegacySawConstruct = true;

		while (!Check(ELangTokenKind::RightBrace))
		{
			if (AtEnd())
			{
				return FailAtEnd(FText::Format(LOCTEXT("WhileNamespace", "the namespace '{0}'"), FText::FromString(NamespaceName)));
			}
			if (Check(ELangTokenKind::DocComment))
			{
				RecordSkippedDocComment(Advance());
				continue;
			}
			if (Match(ELangTokenKind::Semicolon))
			{
				continue;
			}

			const bool bFunctionWord = Check(ELangTokenKind::Identifier)
				&& (Current().Text.Equals(TEXT("Function"), ESearchCase::CaseSensitive) || Current().Text.Equals(TEXT("GraphFunction"), ESearchCase::CaseSensitive));
			if (bFunctionWord)
			{
				FDeclPtr Member = ParseLegacyFunction(NamespaceName);
				if (Member.IsValid())
				{
					OutDecls.Add(MoveTemp(Member));
					continue;
				}
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH6310"),
					Current().Span,
					FText::Format(LOCTEXT("NamespaceMember", "Expected only Function and GraphFunction blocks inside the namespace '{0}', found {1}."), FText::FromString(NamespaceName), DescribeToken(Current())));
			}

			// Skip to the next member word or the namespace's closing brace.
			int32 Depth = 0;
			bool bMoved = false;
			while (!AtEnd())
			{
				const FLangToken& Token = Current();
				if (Depth == 0 && bMoved && Token.Kind == ELangTokenKind::Identifier
					&& (Token.Text.Equals(TEXT("Function"), ESearchCase::CaseSensitive) || Token.Text.Equals(TEXT("GraphFunction"), ESearchCase::CaseSensitive)))
				{
					break;
				}
				if (Depth == 0 && Token.Kind == ELangTokenKind::RightBrace)
				{
					break;
				}
				if (Token.Kind == ELangTokenKind::LeftBrace)
				{
					++Depth;
				}
				else if (Token.Kind == ELangTokenKind::RightBrace)
				{
					--Depth;
				}
				Advance();
				bMoved = true;
			}
		}
		Advance(); // }
		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// VirtualFunction
	// ---------------------------------------------------------------------------------------------

	FDeclPtr FLangParser::ParseLegacyVirtualFunction()
	{
		const int32 StartIndex = GetTokenIndex();
		Advance(); // VirtualFunction

		TUniquePtr<FFunctionDecl> Function = MakeUnique<FFunctionDecl>();
		Function->bLegacy = true;
		Function->Linkage = EFunctionLinkage::Extern;
		Function->Doc = MoveTemp(LegacyPendingDoc);
		LegacyPendingDoc = FDocBlock();
		ClassifyTypeName(TEXT("void"), Function->ReturnType);

		TArray<FPragmaArgument> Attributes;
		if (!ParseLegacyAttributes(Attributes))
		{
			return nullptr;
		}
		const FLangSpan HeaderSpan = SpanFrom(StartIndex);
		Function->ReturnType.Span = HeaderSpan;

		const FPragmaArgument* NameAttribute = LegacyParser::FindLegacyAttribute(Attributes, TEXT("Name"));
		const FString Name = NameAttribute ? NameAttribute->Value.TrimStartAndEnd() : FString();
		if (Name.IsEmpty() || !LegacyParser::IsLegacyIdentifierName(Name))
		{
			Diagnostics.Error(
				TEXT("DSH6311"),
				NameAttribute ? NameAttribute->Span : HeaderSpan,
				Name.IsEmpty()
					? LOCTEXT("VirtualNoName", "Expected a 'Name = \"...\"' attribute with a name on 'VirtualFunction', found none.")
					: FText::Format(LOCTEXT("VirtualNotIdentifier", "Expected the VirtualFunction name to be an identifier, found '{0}'."), FText::FromString(Name)));
			LegacyParser::SkipLegacyBlockRemainder(*this);
			return nullptr;
		}

		if (!Check(ELangTokenKind::LeftBrace))
		{
			Diagnostics.Error(
				TEXT("DSH2257"),
				Current().Span,
				FText::Format(LOCTEXT("VirtualOpen", "Expected '`{' to open the VirtualFunction '{0}', found {1}."), FText::FromString(Name), DescribeToken(Current())));
			return nullptr;
		}
		Advance(); // {
		bLegacySawConstruct = true;

		FLegacyBlockContext Block;
		Block.BlockWord = TEXT("VirtualFunction");
		Block.Name = Name;
		Block.Info = LegacyInfo;

		TArray<FParam> Inputs;
		TArray<FParam> Outputs;
		TArray<FString> InputNames;
		TArray<FString> OutputNames;
		TArray<TPair<FString, FString>> Docs;
		TArray<FPragmaArgument> Options;

		while (!Check(ELangTokenKind::RightBrace))
		{
			if (AtEnd())
			{
				FailAtEnd(FText::Format(LOCTEXT("WhileVirtual", "the VirtualFunction '{0}'"), FText::FromString(Name)));
				return nullptr;
			}
			if (Match(ELangTokenKind::Semicolon))
			{
				continue;
			}
			if (Check(ELangTokenKind::DocComment))
			{
				RecordSkippedDocComment(Advance());
				continue;
			}
			if (!Check(ELangTokenKind::Identifier))
			{
				Diagnostics.Error(
					TEXT("DSH2257"),
					Current().Span,
					FText::Format(LOCTEXT("VirtualSectionName", "Expected a section name such as 'Inputs' in the VirtualFunction '{0}', found {1}."), FText::FromString(Name), DescribeToken(Current())));
				while (!AtEnd() && !Check(ELangTokenKind::LeftBrace) && !Check(ELangTokenKind::RightBrace))
				{
					Advance();
				}
				LegacyParser::SkipLegacyBalanced(*this);
				continue;
			}

			const FLangToken& SectionToken = Advance();
			const FString SectionName = SectionToken.Text;
			Match(ELangTokenKind::Assign);
			if (!Check(ELangTokenKind::LeftBrace))
			{
				Diagnostics.Error(
					TEXT("DSH2257"),
					Current().Span,
					FText::Format(LOCTEXT("VirtualSectionOpen", "Expected '`{' after the section name '{0}', found {1}."), FText::FromString(SectionName), DescribeToken(Current())));
				while (!AtEnd() && !Check(ELangTokenKind::LeftBrace) && !Check(ELangTokenKind::RightBrace))
				{
					Advance();
				}
				LegacyParser::SkipLegacyBalanced(*this);
				continue;
			}

			if (SectionName.Equals(TEXT("Inputs"), ESearchCase::IgnoreCase) || SectionName.Equals(TEXT("Properties"), ESearchCase::IgnoreCase))
			{
				ParseLegacyParamsWithDocs(Block, EParamDirection::In, Inputs, InputNames, Docs);
			}
			else if (SectionName.Equals(TEXT("Outputs"), ESearchCase::IgnoreCase) || SectionName.Equals(TEXT("Results"), ESearchCase::IgnoreCase))
			{
				ParseLegacyParamsWithDocs(Block, EParamDirection::Out, Outputs, OutputNames, Docs);
			}
			else if (SectionName.Equals(TEXT("Options"), ESearchCase::IgnoreCase) || SectionName.Equals(TEXT("Settings"), ESearchCase::IgnoreCase))
			{
				ParseLegacySettings(Block, Options);
			}
			else if (SectionName.Equals(TEXT("Graph"), ESearchCase::IgnoreCase) || SectionName.Equals(TEXT("Code"), ESearchCase::IgnoreCase))
			{
				Diagnostics.Error(
					TEXT("DSH2247"),
					SectionToken.Span,
					FText::Format(LOCTEXT("VirtualBody", "Expected no body in the VirtualFunction '{0}', which declares an existing asset, found the section '{1}'."), FText::FromString(Name), FText::FromString(SectionName)));
				LegacyParser::SkipLegacyBalanced(*this);
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH2245"),
					SectionToken.Span,
					FText::Format(LOCTEXT("VirtualUnknownSection", "Expected a VirtualFunction section (Inputs, Outputs or Options), found '{0}'."), FText::FromString(SectionName)));
				LegacyParser::SkipLegacyBalanced(*this);
			}
			Match(ELangTokenKind::Semicolon);
		}
		Advance(); // }
		Function->Span = SpanFrom(StartIndex);

		// Asset= on the header, else Options { Asset = ...; } (1.x read both).
		FString AssetText;
		FLangSpan AssetSpan = HeaderSpan;
		if (const FPragmaArgument* AssetAttribute = LegacyParser::FindLegacyAttribute(Attributes, TEXT("Asset")))
		{
			AssetText = AssetAttribute->Value.TrimStartAndEnd();
			AssetSpan = AssetAttribute->Span;
		}
		FString Description;
		bool bHasDescription = false;
		for (const FPragmaArgument& Option : Options)
		{
			if (AssetText.IsEmpty() && Option.Key.Equals(TEXT("Asset"), ESearchCase::IgnoreCase))
			{
				AssetText = Option.Value.TrimStartAndEnd();
				AssetSpan = Option.Span;
			}
			else if (Option.Key.Equals(TEXT("Description"), ESearchCase::IgnoreCase))
			{
				Description = Option.Value;
				bHasDescription = true;
			}
		}

		if (AssetText.IsEmpty())
		{
			Diagnostics.Error(
				TEXT("DSH6312"),
				HeaderSpan,
				FText::Format(LOCTEXT("VirtualNoAsset", "Expected an 'Asset = Path(...)' option on the VirtualFunction '{0}', found none."), FText::FromString(Name)));
		}
		if (Outputs.Num() == 0)
		{
			Diagnostics.Error(
				TEXT("DSH6313"),
				HeaderSpan,
				FText::Format(LOCTEXT("VirtualNoOutput", "Expected at least one output on the VirtualFunction '{0}', found none."), FText::FromString(Name)));
		}

		Function->Name = Name;
		Function->NameSpan = NameAttribute->Span;

		int32 ReturnOutput = INDEX_NONE;
		if (OutputNames.Num() > 0 && OutputNames[0].Equals(TEXT("Result"), ESearchCase::CaseSensitive))
		{
			for (int32 ItemIndex = 0; ItemIndex < Outputs.Num(); ++ItemIndex)
			{
				if (Outputs[ItemIndex].Name.Equals(TEXT("Result"), ESearchCase::CaseSensitive))
				{
					ReturnOutput = ItemIndex;
					break;
				}
			}
		}
		for (FParam& Input : Inputs)
		{
			Function->Params.Add(MoveTemp(Input));
		}
		for (int32 ItemIndex = 0; ItemIndex < Outputs.Num(); ++ItemIndex)
		{
			if (ItemIndex == ReturnOutput)
			{
				Function->ReturnType = Outputs[ItemIndex].Type;
				continue;
			}
			Function->Params.Add(MoveTemp(Outputs[ItemIndex]));
		}

		if (!AssetText.IsEmpty())
		{
			// Unresolved: `Path(Plugins.MoonToon, "...")` needs plugin mounts, which the emitter has (research-legacy.md 3.8).
			LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("asset"), AssetText);
			if (LegacyInfo)
			{
				FLegacyAssetReference& Reference = LegacyInfo->AssetReferences.AddDefaulted_GetRef();
				Reference.Use = FLegacyAssetReference::EUse::VirtualFunctionAsset;
				Reference.Text = AssetText;
				Reference.Span = AssetSpan;
				Reference.Node = Function.Get();
			}
		}
		if (bHasDescription)
		{
			LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("desc"), Description);
		}
		for (const FString& StaticInput : LegacyStaticBoolInputs)
		{
			LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("static"), StaticInput);
		}
		LegacyStaticBoolInputs.Reset();
		for (const TPair<FString, FString>& Doc : Docs)
		{
			if (!(ReturnOutput != INDEX_NONE && Doc.Key.Equals(TEXT("Result"), ESearchCase::CaseSensitive)))
			{
				LegacyParser::AddLegacyBlockDirective(LegacyInfo, *Function, TEXT("param"), Doc.Key + TEXT(" ") + Doc.Value);
			}
		}

		LegacyUsedNames.Add(Function->Name);
		if (LegacyInfo)
		{
			FLegacyBlock& Record = LegacyInfo->Blocks.AddDefaulted_GetRef();
			Record.BlockWord = TEXT("VirtualFunction");
			Record.Decl = Function.Get();
			Record.Name = Name;
			Record.HeaderSpan = HeaderSpan;
			Record.InputNames = InputNames;
			Record.OutputNames = OutputNames;
		}

		return Function;
	}
}

#undef LOCTEXT_NAMESPACE
