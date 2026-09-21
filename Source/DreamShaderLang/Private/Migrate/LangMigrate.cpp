// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See Migrate/LangMigrate.h.
//
// One walk over every function body, children before parents, with the bound module answering what each node
// resolved to. A rename is applied where it is found. A call that has to become a statement of its own is moved
// out of the expression it sat in, which is replaced by the variable the statement fills; the statement is queued in
// front of the statement being walked, in the block being walked, and put in once that block is done.
//
// Children first matters twice: a selection inside another call's arguments is already a variable by the time the
// outer call is looked at, so two outer calls that were equal in 1.x are still equal as text -- and that text is the
// key one hoisted statement is shared under, because 1.x made one node for equal calls too
// (FLegacyOutputSelection::Group). A variable the text reads being written in between ends the sharing, and so does
// leaving the block the statement was put in, or entering a loop.

#include "Migrate/LangMigrate.h"

#include "Decompile/IRToAstInternal.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "Lang/LangLexer.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangToken.h"

#define LOCTEXT_NAMESPACE "DreamShaderLangMigrate"

namespace UE::DreamShader::Lang
{
	namespace LegacyMigrate
	{
		/** `SAMPLERTYPE_Normal`, `Sampler Type::normal`: what 1.x compared enumerators by (NormaliseLegacyEnumKey). */
		static FString NormaliseEnumKey(const FString& Text)
		{
			FString Key;
			Key.Reserve(Text.Len());
			for (const TCHAR Char : Text)
			{
				if (Char == TCHAR(' ') || Char == TCHAR('\t') || Char == TCHAR('_') || Char == TCHAR('-')
					|| Char == TCHAR(':') || Char == TCHAR('.') || Char == TCHAR('/'))
				{
					continue;
				}
				Key.AppendChar(FChar::ToLower(Char));
			}
			return Key;
		}

		/** An identifier, a dotted chain or a string literal as the word it spells. */
		static bool FlattenWord(const FExpr& Expr, FString& Out)
		{
			if (const FIdentifierExpr* Identifier = Expr.As<FIdentifierExpr>())
			{
				Out = Identifier->Name;
				return true;
			}
			if (const FLiteralExpr* Literal = Expr.As<FLiteralExpr>())
			{
				if (Literal->LiteralKind == ELiteralKind::String)
				{
					Out = Literal->Text;
					return true;
				}
				return false;
			}
			if (const FMemberExpr* Member = Expr.As<FMemberExpr>())
			{
				FString Prefix;
				if (Member->Object && FlattenWord(*Member->Object, Prefix))
				{
					Out = Prefix + TEXT(".") + Member->Member;
					return true;
				}
			}
			return false;
		}

		/** The catalog's spelling of a 1.x enumerator, when exactly one matches the way 1.x matched them (rule L12). */
		static bool TryCanonicaliseEnumerator(const FString& Spelling, const IR::FCatalogProperty& Property, FString& OutEnumerator)
		{
			TArray<FString> Keys;
			FString Rest = Spelling.TrimStartAndEnd();
			Keys.Add(NormaliseEnumKey(Rest));
			const int32 Scope = Rest.Find(TEXT("::"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (Scope != INDEX_NONE)
			{
				Rest = Rest.Mid(Scope + 2);
				Keys.Add(NormaliseEnumKey(Rest));
			}
			const int32 Prefix = Rest.Find(TEXT("_"), ESearchCase::CaseSensitive);
			if (Prefix != INDEX_NONE)
			{
				Keys.Add(NormaliseEnumKey(Rest.Mid(Prefix + 1)));
			}

			int32 Found = INDEX_NONE;
			for (int32 Index = 0; Index < Property.EnumValues.Num(); ++Index)
			{
				const FString Key = NormaliseEnumKey(Property.EnumValues[Index]);
				if (Key.IsEmpty() || !Keys.Contains(Key))
				{
					continue;
				}
				if (Found != INDEX_NONE)
				{
					return false;
				}
				Found = Index;
			}
			if (Found == INDEX_NONE)
			{
				return false;
			}
			OutEnumerator = Property.EnumValues[Found];
			return true;
		}

		/** `Project:Shared/X.dsh`, `Plugin.MoonToon:X.dsh`: a root named in front of the path, which `#include` has no way to say. */
		static bool IsRootQualifiedImport(const FString& Path)
		{
			int32 Colon = INDEX_NONE;
			int32 Slash = INDEX_NONE;
			if (!Path.FindChar(TCHAR(':'), Colon))
			{
				return false;
			}
			// `C:/...` is a drive, not a root.
			return Colon > 1 && (!Path.FindChar(TCHAR('/'), Slash) || Colon < Slash);
		}

		/** The variable an lvalue writes: `v` of `v.x`, `v[0]`, `(v)`. */
		static const FIdentifierExpr* FindRootIdentifier(const FExpr& Expr)
		{
			const FExpr* Current = &Expr;
			while (Current)
			{
				if (const FIdentifierExpr* Identifier = Current->As<FIdentifierExpr>())
				{
					return Identifier;
				}
				if (const FMemberExpr* Member = Current->As<FMemberExpr>())
				{
					Current = Member->Object.Get();
				}
				else if (const FIndexExpr* Index = Current->As<FIndexExpr>())
				{
					Current = Index->Object.Get();
				}
				else if (const FParenExpr* Paren = Current->As<FParenExpr>())
				{
					Current = Paren->Inner.Get();
				}
				else
				{
					return nullptr;
				}
			}
			return nullptr;
		}

		/** Every identifier an expression mentions, callees included: more than it reads, which only ends a sharing early. */
		static void CollectIdentifiers(const FExpr& Expr, TSet<FString>& OutNames)
		{
			switch (Expr.Kind)
			{
			case ENodeKind::IdentifierExpr:
				OutNames.Add(static_cast<const FIdentifierExpr&>(Expr).Name);
				break;
			case ENodeKind::MemberExpr:
				if (static_cast<const FMemberExpr&>(Expr).Object) { CollectIdentifiers(*static_cast<const FMemberExpr&>(Expr).Object, OutNames); }
				break;
			case ENodeKind::IndexExpr:
				if (static_cast<const FIndexExpr&>(Expr).Object) { CollectIdentifiers(*static_cast<const FIndexExpr&>(Expr).Object, OutNames); }
				if (static_cast<const FIndexExpr&>(Expr).Index) { CollectIdentifiers(*static_cast<const FIndexExpr&>(Expr).Index, OutNames); }
				break;
			case ENodeKind::CallExpr:
			{
				const FCallExpr& Call = static_cast<const FCallExpr&>(Expr);
				if (Call.Callee) { CollectIdentifiers(*Call.Callee, OutNames); }
				for (const FArgument& Argument : Call.Arguments)
				{
					if (Argument.Value) { CollectIdentifiers(*Argument.Value, OutNames); }
				}
				break;
			}
			case ENodeKind::UnaryExpr:
				if (static_cast<const FUnaryExpr&>(Expr).Operand) { CollectIdentifiers(*static_cast<const FUnaryExpr&>(Expr).Operand, OutNames); }
				break;
			case ENodeKind::BinaryExpr:
				if (static_cast<const FBinaryExpr&>(Expr).Left) { CollectIdentifiers(*static_cast<const FBinaryExpr&>(Expr).Left, OutNames); }
				if (static_cast<const FBinaryExpr&>(Expr).Right) { CollectIdentifiers(*static_cast<const FBinaryExpr&>(Expr).Right, OutNames); }
				break;
			case ENodeKind::AssignExpr:
				if (static_cast<const FAssignExpr&>(Expr).Target) { CollectIdentifiers(*static_cast<const FAssignExpr&>(Expr).Target, OutNames); }
				if (static_cast<const FAssignExpr&>(Expr).Value) { CollectIdentifiers(*static_cast<const FAssignExpr&>(Expr).Value, OutNames); }
				break;
			case ENodeKind::ConditionalExpr:
				if (static_cast<const FConditionalExpr&>(Expr).Condition) { CollectIdentifiers(*static_cast<const FConditionalExpr&>(Expr).Condition, OutNames); }
				if (static_cast<const FConditionalExpr&>(Expr).TrueValue) { CollectIdentifiers(*static_cast<const FConditionalExpr&>(Expr).TrueValue, OutNames); }
				if (static_cast<const FConditionalExpr&>(Expr).FalseValue) { CollectIdentifiers(*static_cast<const FConditionalExpr&>(Expr).FalseValue, OutNames); }
				break;
			case ENodeKind::CastExpr:
				if (static_cast<const FCastExpr&>(Expr).Operand) { CollectIdentifiers(*static_cast<const FCastExpr&>(Expr).Operand, OutNames); }
				break;
			case ENodeKind::ParenExpr:
				if (static_cast<const FParenExpr&>(Expr).Inner) { CollectIdentifiers(*static_cast<const FParenExpr&>(Expr).Inner, OutNames); }
				break;
			case ENodeKind::InitializerListExpr:
				for (const FExprPtr& Element : static_cast<const FInitializerListExpr&>(Expr).Elements)
				{
					if (Element) { CollectIdentifiers(*Element, OutNames); }
				}
				break;
			default:
				break;
			}
		}

		class FMigrator
		{
		public:
			FMigrator(FModule& InModule, const FLegacyMigrationInfo& InLegacy, const FBoundModule& InBound, const FLangMigrateOptions& InOptions, FLangDiagnosticSink& InDiagnostics)
				: Module(InModule)
				, Legacy(InLegacy)
				, Bound(InBound)
				, Options(InOptions)
				, Diagnostics(InDiagnostics)
			{
			}

			bool Run();

		private:
			struct FInsertion
			{
				int32 BeforeIndex = 0;
				FStmtPtr Statement;
			};

			/** One statement list being walked: a function body, or a block inside one. */
			struct FFrame
			{
				TArray<FStmtPtr>* Statements = nullptr;
				int32 Index = 0;
				TArray<FInsertion> Insertions;
			};

			/** One call turned into a statement: what each of its 1.x outputs is read from, by ordinal. */
			struct FHoistedStatement
			{
				TArray<FExprPtr> Outputs;
				TSet<FString> Reads;
				/** Frames.Num() where it was put: gone when that block is left. */
				int32 Depth = 0;
			};

			void MigrateDeclaration(FDecl& Decl);
			/** The comment above a 1.x block goes above the first declaration the block became. */
			void MoveBlockComments();
			void MigrateDocBlock(FFunctionDecl& Function);
			void MigrateParams(FFunctionDecl& Function);
			void MigrateBody(FFunctionDecl& Function);
			/** A directive value of several lines (a 1.x `Description = "a\r\nb"`): the description as free text, anything else on one line. */
			void MigrateDocLineBreaks(FDecl& Decl);
			/** The calls lifted out of a verbatim body (rule L8), written back into it in the spelling a `.dss` reads. */
			void MigrateLiftedCalls(FFunctionDecl& Function);
			/** The indentation a `Namespace` block left on every line of a verbatim body, taken off. */
			void DedentOpaqueBody(FFunctionDecl& Function);

			void WalkStatements(TArray<FStmtPtr>& Statements);
			void BeginTopStatement(const FStmt& Statement);
			void EndTopStatement(FStmtPtr& Statement, int32 FirstInsertion);
			void ApplyInsertions(FFrame& Frame);
			void VisitStmt(FStmt& Stmt);
			void VisitLoopBody(FStmt& Body);
			/** Walks the expression in Slot, then names the first output of a node 1.x read by default (rule L3c). */
			void VisitExprSlot(FExprPtr& Slot, bool bIsStatementExpression);
			void VisitExprSlotInner(FExprPtr& Slot, bool bIsStatementExpression);
			void VisitCallArguments(FCallExpr& Call);

			void RenameIdentifier(FIdentifierExpr& Identifier, const FBoundExpr& Binding);
			void MigrateReflectedCall(FCallExpr& Call, const FBoundExpr& Binding);
			void MigrateCoreOpCall(FCallExpr& Call, const FBoundExpr& Binding);
			/** The callee and the argument names as declared (rule L19), and every `out` argument noted as a write. */
			void CanonicaliseUserCall(FCallExpr& Call, const FBoundExpr& Binding, const FBoundFunction& Callee);
			void MigrateValueCall(FExprPtr& Slot, FCallExpr& Call, const FBoundExpr& Binding, const FBoundFunction& Callee);
			void MigrateStatementCall(FExprStmt& Statement, FCallExpr& Call, const FBoundExpr& Binding);
			/** Slot holds the call or a selector over it; afterwards it reads the variable that output landed in. */
			void HoistCall(FExprPtr& Slot, FExprPtr& CallSlot, const FBoundExpr& CallBinding, int32 Ordinal, const FLangSpan& Span);
			/**
			 * Variables for the `out` parameters a call leaves out, passed by name. OutParamOutputs gets, per parameter, what
			 * that output is read from afterwards: the new variable, or a copy of the argument the call already passed.
			 */
			void PassAbsentOuts(FCallExpr& Call, const FBoundExpr& Binding, const FBoundFunction& Callee, TArray<FExprPtr>& OutParamOutputs);

			void NoteWrite(const FExpr& Target);
			FString ClaimLocalName(const FString& Wanted);
			void QueueStatement(FStmtPtr Statement);
			static FTypeRef MakeExactTypeRef(const IR::FIRType& Type);

			FModule& Module;
			const FLegacyMigrationInfo& Legacy;
			const FBoundModule& Bound;
			const FLangMigrateOptions& Options;
			FLangDiagnosticSink& Diagnostics;

			// ----- the body being walked
			const FBoundFunction* BoundFunction = nullptr;
			TArray<FFrame> Frames;
			/** Lower case: what is compared ignoring case further down must not differ by case here. */
			TSet<FString> NamesInUse;
			TMap<FString, FHoistedStatement> HoistedCalls;
			/** Rule L5 locals nobody declared, not yet reached. */
			TArray<const FBoundLocal*> PendingImplicit;
			/** The ones first mentioned in the top-level statement being walked, and still without a declaration. */
			TArray<const FBoundLocal*> ImplicitHere;
			TSet<FString> DeclaredImplicit;
			/** Set by the statement being walked when it wants to be another statement altogether. */
			FStmtPtr Replacement;
			/** Calls that hold a `default` placeholder whose value is gone already (rule L25); swept when the body is done. */
			TArray<FCallExpr*> DefaultedCalls;
			bool bFailed = false;
		};

		FTypeRef FMigrator::MakeExactTypeRef(const IR::FIRType& Type)
		{
			// An `out` argument has to be of exactly the parameter's type, `int` and `half` included, so the binder's own
			// spelling of the type and not the graph's.
			const FString Spelled = Type.ToString();
			return DecompileAst::MakeTypeRef(DecompileAst::IsIdentifierText(Spelled) ? Spelled : FString(TEXT("float")));
		}

		FString FMigrator::ClaimLocalName(const FString& Wanted)
		{
			const FString Base = DecompileAst::MakeSourceIdentifier(Wanted, TEXT("Value"));
			FString Candidate = Base;
			for (int32 Suffix = 2; NamesInUse.Contains(Candidate.ToLower()); ++Suffix)
			{
				Candidate = FString::Printf(TEXT("%s_%d"), *Base, Suffix);
			}
			NamesInUse.Add(Candidate.ToLower());
			return Candidate;
		}

		void FMigrator::QueueStatement(FStmtPtr Statement)
		{
			if (Frames.Num() == 0 || !Statement)
			{
				return;
			}
			FInsertion Insertion;
			Insertion.BeforeIndex = Frames.Last().Index;
			Insertion.Statement = MoveTemp(Statement);
			Frames.Last().Insertions.Add(MoveTemp(Insertion));
		}

		void FMigrator::NoteWrite(const FExpr& Target)
		{
			const FIdentifierExpr* Written = FindRootIdentifier(Target);
			if (!Written)
			{
				return;
			}
			for (TMap<FString, FHoistedStatement>::TIterator It = HoistedCalls.CreateIterator(); It; ++It)
			{
				if (It->Value.Reads.Contains(Written->Name))
				{
					It.RemoveCurrent();
				}
			}
		}

		// ------------------------------------------------------------------------------ declarations

		void FMigrator::MigrateDocBlock(FFunctionDecl& Function)
		{
			const FLangMigrateProductName* Answer = Options.ProductNames.FindByPredicate([&Function](const FLangMigrateProductName& Candidate)
			{
				return Candidate.Decl == &Function;
			});

			const bool bHasRoot = Function.Doc.Has(Directive::Root);
			if (!Answer)
			{
				if (bHasRoot)
				{
					Diagnostics.Warning(TEXT("DSH9094"), Function.NameSpan, FText::Format(
						LOCTEXT("RootKept", "'{0}' keeps its '/// @root', the 1.x spelling of where its asset goes, because the place the new file would put it could not be worked out; check the asset path the first build reports."),
						FText::FromString(Function.Name)));
				}
				return;
			}

			// A block rebuilt canonically: free text, then one directive per line. The parse order indexes the very array
			// that is being filtered.
			TArray<FDocDirective> Kept;
			for (FDocDirective& Existing : Function.Doc.Directives)
			{
				if (!Existing.Key.Equals(Directive::Root, ESearchCase::CaseSensitive) && !Existing.Key.Equals(Directive::Name, ESearchCase::CaseSensitive))
				{
					Kept.Add(MoveTemp(Existing));
				}
			}
			Function.Doc.Directives = MoveTemp(Kept);
			Function.Doc.Order.Reset();
			if (!Answer->Name.IsEmpty())
			{
				FDocDirective Name;
				Name.Key = Directive::Name;
				Name.Value = Answer->Name;
				Function.Doc.Directives.Insert(MoveTemp(Name), 0);
			}
		}

		void FMigrator::MigrateParams(FFunctionDecl& Function)
		{
			for (FParam& Param : Function.Params)
			{
				// 1.x `opt` without a value: optional, previewing what the engine previews. 2.0 says optional with a default,
				// so that value is spelled: zero, and the alpha of one a four-wide input has in the engine.
				if (!Param.bOptional || Param.Default || Param.Direction != EParamDirection::In)
				{
					continue;
				}
				const int32 Width = Param.Type.IsNumeric() && !Param.Type.IsMatrix() ? Param.Type.ComponentCount() : 0;
				if (Width >= 1 && Width <= 4)
				{
					const double EnginePreview[4] = { 0.0, 0.0, 0.0, 1.0 };
					Param.Default = DecompileAst::MakeVectorLiteralExpr(EnginePreview, Width, /* bBool */ false);
				}
				else
				{
					Diagnostics.Warning(TEXT("DSH9094"), Param.NameSpan, FText::Format(
						LOCTEXT("OptionalWithoutDefault", "'{0}' of '{1}' was an optional input, and a {2} has no default the language can write; it is a required input in the migrated file."),
						FText::FromString(Param.Name),
						FText::FromString(Function.Name),
						FText::FromString(Param.Type.Name)));
				}
				Param.bOptional = false;
			}
		}

		void FMigrator::MigrateDeclaration(FDecl& Decl)
		{
			Decl.bLegacy = false;

			if (FIncludeDecl* Include = Decl.As<FIncludeDecl>())
			{
				if (IsRootQualifiedImport(Include->Path))
				{
					bFailed = true;
					Diagnostics.Error(TEXT("DSH9091"), Include->PathSpan, FText::Format(
						LOCTEXT("RootQualifiedImport", "'{0}' names a source root in front of the path, which '#include' cannot; write the path from the root's own folder (a leading '/') or relative to this file, then migrate again."),
						FText::FromString(Include->Path)));
				}
				Include->bImportSpelling = false;
				return;
			}

			if (FPragmaDecl* Pragma = Decl.As<FPragmaDecl>())
			{
				if (Pragma->PragmaKind != EPragmaKind::Material)
				{
					return;
				}
				for (FPragmaArgument& Argument : Pragma->Arguments)
				{
					if (!Argument.Key.Equals(TEXT("Backend"), ESearchCase::IgnoreCase))
					{
						continue;
					}
					// 1.x read an empty Backend as Graph, and Instance is the old name of ThinCustom; 2.0 takes neither.
					const FString Value = Argument.Value.TrimStartAndEnd();
					if (Value.IsEmpty())
					{
						Argument.Value = TEXT("Graph");
						Argument.bQuoted = false;
					}
					else if (Value.Equals(TEXT("Instance"), ESearchCase::IgnoreCase))
					{
						Argument.Value = TEXT("ThinCustom");
						Argument.bQuoted = false;
					}
				}
				return;
			}

			MigrateDocLineBreaks(Decl);

			if (FFunctionDecl* Function = Decl.As<FFunctionDecl>())
			{
				// Rule L23: a block whose asset shares its name with a function this file can call was bound under a name of
				// its own, and that is the name a `.dss` has to say.
				if (const FBoundFunction* Declared = Bound.Functions.FindByPredicate([Function](const FBoundFunction& Candidate) { return Candidate.Decl == Function; }))
				{
					if (!Declared->Name.IsEmpty() && !Declared->Name.Equals(Function->Name, ESearchCase::CaseSensitive))
					{
						Function->Name = Declared->Name;
					}
				}
				MigrateDocBlock(*Function);
				MigrateParams(*Function);
				if (Function->Body)
				{
					MigrateBody(*Function);
				}
				else if (Function->bOpaqueBody)
				{
					MigrateLiftedCalls(*Function);
					DedentOpaqueBody(*Function);
				}

				// A function of a `Namespace` block named its Custom node `N::F`, and its flattened name says `N_F`.
				if (!Function->LegacyQualifiedName.IsEmpty() && !Function->Doc.Has(Directive::Name))
				{
					FDocDirective Name;
					Name.Key = Directive::Name;
					Name.Value = Function->LegacyQualifiedName;
					Function->Doc.Directives.Add(MoveTemp(Name));
					Function->Doc.Order.Reset();
					Function->LegacyQualifiedName.Reset();
				}
			}
		}

		void FMigrator::MigrateDocLineBreaks(FDecl& Decl)
		{
			// `Description = "first\r\nsecond"`: 1.x read the escapes, and the directive the block became holds the
			// line breaks themselves -- which, written out, would end the `///` comment after its first line. A
			// description is the doc block's free text, line for line, so that is what it becomes. Anything else with a
			// line break in it (a pin's `@param`, a description with an `@` in it, which free text cannot hold) has no
			// such form: it is written on one line, and said.
			FDocBlock& Doc = Decl.Doc;
			for (int32 Index = 0; Index < Doc.Directives.Num(); ++Index)
			{
				FDocDirective& Entry = Doc.Directives[Index];
				if (!Entry.Value.Contains(TEXT("\n")) && !Entry.Value.Contains(TEXT("\r")))
				{
					continue;
				}
				FString Text = Entry.Value.Replace(TEXT("\r\n"), TEXT("\n")).Replace(TEXT("\r"), TEXT("\n")).TrimStartAndEnd();

				if (Entry.Key.Equals(Directive::Desc, ESearchCase::CaseSensitive) && !Text.Contains(TEXT("@")) && Doc.FreeText.Num() == 0)
				{
					TArray<FString> Lines;
					Text.ParseIntoArray(Lines, TEXT("\n"), /* bCullEmpty */ false);
					for (const FString& Line : Lines)
					{
						Doc.FreeText.Add(Line.TrimEnd());
					}
					Doc.Directives.RemoveAt(Index--);
					Doc.Order.Reset();
					continue;
				}

				Text.ReplaceInline(TEXT("\n"), TEXT(" "));
				Entry.Value = Text;
				Diagnostics.Warning(TEXT("DSH9094"), Decl.Span, FText::Format(
					LOCTEXT("DocLineBreaksLost", "'@{0}' held a text of several lines, which one '///' line cannot; it is written on one line, its line breaks as blanks."),
					FText::FromString(Entry.Key)));
			}
		}

		void FMigrator::MigrateLiftedCalls(FFunctionDecl& Function)
		{
			// Rule L8: the `UE.` calls of a GraphFunction stay in its body, and a `/// @custom` function
			// has them lifted the same way. What 1.x let such a call say -- `UE.Expression(Class = ...)`, a class or an
			// argument under another name, an output chosen inside the parentheses -- the legacy front end rewrote on the
			// call it parsed, and the rules of a graph body apply to it as to any call: it is printed back into the body
			// where its 1.x text stood.
			if (Function.HoistedCalls.Num() == 0)
			{
				return;
			}
			BoundFunction = Bound.Functions.FindByPredicate([&Function](const FBoundFunction& Candidate)
			{
				return Candidate.Decl == &Function;
			});
			if (!BoundFunction)
			{
				return;
			}

			Frames.Reset();
			NamesInUse.Reset();
			HoistedCalls.Reset();
			PendingImplicit.Reset();
			ImplicitHere.Reset();
			DeclaredImplicit.Reset();
			Replacement.Reset();
			DefaultedCalls.Reset();

			// Last first: what stands in front of a replaced text keeps its offset.
			for (int32 Index = Function.HoistedCalls.Num() - 1; Index >= 0; --Index)
			{
				FHoistedCall& Hoisted = Function.HoistedCalls[Index];
				if (!Hoisted.Call
					|| Hoisted.RawBodyOffset < 0
					|| Hoisted.RawBodyLength <= 0
					|| Hoisted.RawBodyOffset + Hoisted.RawBodyLength > Function.RawBody.Len())
				{
					continue;
				}
				VisitExprSlot(Hoisted.Call, /* bIsStatementExpression */ false);
				if (!Hoisted.Call)
				{
					continue;
				}
				const FString Text = PrintDreamShaderLangExpr(*Hoisted.Call);
				const int32 Delta = Text.Len() - Hoisted.RawBodyLength;
				Function.RawBody = Function.RawBody.Left(Hoisted.RawBodyOffset) + Text + Function.RawBody.Mid(Hoisted.RawBodyOffset + Hoisted.RawBodyLength);
				Hoisted.RawBodyLength = Text.Len();
				for (int32 Later = Index + 1; Later < Function.HoistedCalls.Num(); ++Later)
				{
					Function.HoistedCalls[Later].RawBodyOffset += Delta;
				}
			}

			for (FCallExpr* Call : DefaultedCalls)
			{
				Call->Arguments.RemoveAll([](const FArgument& Argument) { return !Argument.Value.IsValid(); });
			}
			DefaultedCalls.Reset();
			BoundFunction = nullptr;
		}

		void FMigrator::DedentOpaqueBody(FFunctionDecl& Function)
		{
			// A function that stood inside `Namespace(...) { }` carries that block's indentation on every line of its
			// verbatim body, and the declaration it becomes stands at the margin. What follows the body's last line break
			// is the indentation of the function's own `}`: taken off every line, when every line has it and the body is
			// indented deeper than that.
			const FString& Body = Function.RawBody;
			int32 LastBreak = INDEX_NONE;
			if (!Body.FindLastChar(TEXT('\n'), LastBreak))
			{
				return;
			}
			const FString Indent = Body.Mid(LastBreak + 1);
			if (Indent.IsEmpty())
			{
				return;
			}
			for (const TCHAR Character : Indent)
			{
				if (Character != TEXT(' ') && Character != TEXT('\t'))
				{
					return;
				}
			}

			bool bDeeper = false;
			for (int32 LineStart = Body.Find(TEXT("\n"), ESearchCase::CaseSensitive) + 1; LineStart > 0 && LineStart <= LastBreak;)
			{
				int32 LineEnd = Body.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, LineStart);
				LineEnd = LineEnd == INDEX_NONE ? Body.Len() : LineEnd;
				const FString Line = Body.Mid(LineStart, LineEnd - LineStart);
				if (!Line.TrimStartAndEnd().IsEmpty())
				{
					if (!Line.StartsWith(Indent, ESearchCase::CaseSensitive))
					{
						return;
					}
					bDeeper |= Line.Len() > Indent.Len() && (Line[Indent.Len()] == TEXT(' ') || Line[Indent.Len()] == TEXT('\t'));
				}
				LineStart = LineEnd + 1;
			}
			if (!bDeeper)
			{
				return;
			}

			FString Dedented;
			Dedented.Reserve(Body.Len());
			TArray<int32> RemovedAt;
			bool bAtLineStart = false;
			for (int32 Index = 0; Index < Body.Len();)
			{
				if (bAtLineStart && Body.Mid(Index, Indent.Len()).Equals(Indent, ESearchCase::CaseSensitive))
				{
					RemovedAt.Add(Index);
					Index += Indent.Len();
				}
				bAtLineStart = false;
				while (Index < Body.Len())
				{
					const TCHAR Character = Body[Index++];
					Dedented.AppendChar(Character);
					if (Character == TEXT('\n'))
					{
						bAtLineStart = true;
						break;
					}
				}
			}

			// The lifted calls say where in the body their text stands.
			for (FHoistedCall& Hoisted : Function.HoistedCalls)
			{
				int32 Before = 0;
				int32 Inside = 0;
				for (const int32 Offset : RemovedAt)
				{
					if (Offset < Hoisted.RawBodyOffset)
					{
						++Before;
					}
					else if (Offset < Hoisted.RawBodyOffset + Hoisted.RawBodyLength)
					{
						++Inside;
					}
				}
				Hoisted.RawBodyOffset -= Before * Indent.Len();
				Hoisted.RawBodyLength -= Inside * Indent.Len();
			}
			Function.RawBody = MoveTemp(Dedented);
		}

		// -------------------------------------------------------------------------------------- bodies

		void FMigrator::MigrateBody(FFunctionDecl& Function)
		{
			BoundFunction = Bound.Functions.FindByPredicate([&Function](const FBoundFunction& Candidate)
			{
				return Candidate.Decl == &Function;
			});
			if (!BoundFunction)
			{
				return;
			}

			Frames.Reset();
			NamesInUse.Reset();
			HoistedCalls.Reset();
			PendingImplicit.Reset();
			ImplicitHere.Reset();
			DeclaredImplicit.Reset();
			Replacement.Reset();

			for (const FBoundParam& Param : BoundFunction->Params)
			{
				NamesInUse.Add(Param.Name.ToLower());
			}
			for (const FBoundLocal& Local : BoundFunction->Locals)
			{
				NamesInUse.Add(Local.Name.ToLower());
			}
			for (const FBoundGlobal& Global : Bound.Globals)
			{
				NamesInUse.Add(Global.Name.ToLower());
			}
			for (const FBoundFunction& Other : Bound.Functions)
			{
				NamesInUse.Add(Other.Name.ToLower());
			}

			// Rule L5: a variable that receives an output without having been declared was declared by the call. It has no
			// declarator, and its span is the identifier at that first call.
			for (const FBoundLocal& Local : BoundFunction->Locals)
			{
				if (Local.Decl == nullptr)
				{
					PendingImplicit.Add(&Local);
				}
			}

			DefaultedCalls.Reset();
			WalkStatements(Function.Body->Statements);

			// Rule L25: the `default` placeholders, now that nothing addresses an argument by its index any more.
			for (FCallExpr* Call : DefaultedCalls)
			{
				Call->Arguments.RemoveAll([](const FArgument& Argument) { return !Argument.Value.IsValid(); });
			}
			DefaultedCalls.Reset();

			// A span no statement covers: declared up front, which is always early enough.
			if (PendingImplicit.Num() > 0 && Function.Body->Statements.Num() > 0)
			{
				TArray<FStmtPtr> Declarations;
				for (const FBoundLocal* Local : PendingImplicit)
				{
					bool bAlreadyDeclared = false;
					DeclaredImplicit.Add(Local->Name, &bAlreadyDeclared);
					if (!bAlreadyDeclared)
					{
						Declarations.Add(DecompileAst::MakeVarDeclStmt(MakeExactTypeRef(Local->Type), Local->Name, FExprPtr()));
					}
				}
				Function.Body->Statements.Insert(MoveTemp(Declarations), 0);
			}

			BoundFunction = nullptr;
		}

		void FMigrator::WalkStatements(TArray<FStmtPtr>& Statements)
		{
			Frames.Emplace();
			const int32 Depth = Frames.Num();
			Frames[Depth - 1].Statements = &Statements;

			for (int32 Index = 0; Index < Statements.Num(); ++Index)
			{
				if (!Statements[Index])
				{
					continue;
				}
				// By index every time: a block inside this statement pushes a frame, and the array may move.
				Frames[Depth - 1].Index = Index;

				if (Depth == 1)
				{
					const int32 FirstInsertion = Frames[0].Insertions.Num();
					BeginTopStatement(*Statements[Index]);
					VisitStmt(*Statements[Index]);
					EndTopStatement(Statements[Index], FirstInsertion);
				}
				else
				{
					VisitStmt(*Statements[Index]);
				}
			}

			ApplyInsertions(Frames[Depth - 1]);
			Frames.Pop();

			if (Depth > 1)
			{
				// What was hoisted inside this block is out of scope outside it.
				for (TMap<FString, FHoistedStatement>::TIterator It = HoistedCalls.CreateIterator(); It; ++It)
				{
					if (It->Value.Depth >= Depth)
					{
						It.RemoveCurrent();
					}
				}
			}
		}

		void FMigrator::BeginTopStatement(const FStmt& Statement)
		{
			ImplicitHere.Reset();
			Replacement.Reset();
			for (int32 LocalIndex = 0; LocalIndex < PendingImplicit.Num();)
			{
				const FBoundLocal& Local = *PendingImplicit[LocalIndex];
				if (Local.Span.Length <= 0 || Local.Span.Offset < Statement.Span.Offset || Local.Span.Offset >= Statement.Span.End())
				{
					++LocalIndex;
					continue;
				}
				// Two sibling blocks may each have declared the name; one declaration in front of the first covers both.
				bool bAlreadyDeclared = false;
				DeclaredImplicit.Add(Local.Name, &bAlreadyDeclared);
				if (!bAlreadyDeclared)
				{
					ImplicitHere.Add(&Local);
				}
				PendingImplicit.RemoveAt(LocalIndex);
			}
		}

		void FMigrator::EndTopStatement(FStmtPtr& Statement, const int32 FirstInsertion)
		{
			// Rule L26: `VAcc = 0.0;` declared VAcc, and a `.dss` says so on the same line: `float VAcc = 0.0;`.
			if (!Replacement && Statement.IsValid())
			{
				FExprStmt* Line = Statement->As<FExprStmt>();
				FAssignExpr* Assign = (Line && Line->Expression) ? Line->Expression->As<FAssignExpr>() : nullptr;
				const FIdentifierExpr* Target = (Assign && Assign->Op == EAssignOp::Assign && Assign->Target) ? Assign->Target->As<FIdentifierExpr>() : nullptr;
				const int32 Declared = Target == nullptr ? INDEX_NONE : ImplicitHere.IndexOfByPredicate([Target](const FBoundLocal* Local)
				{
					return Local->Name.Equals(Target->Name, ESearchCase::CaseSensitive);
				});
				if (Declared != INDEX_NONE && Assign->Value)
				{
					Replacement = DecompileAst::MakeVarDeclStmt(MakeExactTypeRef(ImplicitHere[Declared]->Type), Target->Name, MoveTemp(Assign->Value));
					ImplicitHere.RemoveAt(Declared);
				}
			}

			// In front of everything this statement queued: a hoisted call may already pass the variable.
			int32 InsertAt = FirstInsertion;
			for (const FBoundLocal* Local : ImplicitHere)
			{
				FInsertion Insertion;
				Insertion.BeforeIndex = Frames[0].Index;
				Insertion.Statement = DecompileAst::MakeVarDeclStmt(MakeExactTypeRef(Local->Type), Local->Name, FExprPtr());
				Frames[0].Insertions.Insert(MoveTemp(Insertion), InsertAt++);
			}
			ImplicitHere.Reset();

			if (Replacement)
			{
				// The comments and the blank line above it belong to the line, whatever statement the line is now.
				Replacement->Span = Statement->Span;
				FLangTrivia Moved;
				if (Module.Trivia.RemoveAndCopyValue(Statement.Get(), Moved))
				{
					Module.Trivia.Add(Replacement.Get(), MoveTemp(Moved));
				}
				Statement = MoveTemp(Replacement);
			}
		}

		void FMigrator::ApplyInsertions(FFrame& Frame)
		{
			if (Frame.Insertions.Num() == 0 || !Frame.Statements)
			{
				return;
			}

			TArray<FStmtPtr>& Statements = *Frame.Statements;
			TArray<FStmtPtr> Rebuilt;
			Rebuilt.Reserve(Statements.Num() + Frame.Insertions.Num());
			for (int32 Index = 0; Index < Statements.Num(); ++Index)
			{
				const FNode* FirstInserted = nullptr;
				for (FInsertion& Insertion : Frame.Insertions)
				{
					if (Insertion.BeforeIndex == Index && Insertion.Statement)
					{
						FirstInserted = FirstInserted ? FirstInserted : Insertion.Statement.Get();
						Rebuilt.Add(MoveTemp(Insertion.Statement));
					}
				}

				// The paragraph break the statement had goes above what now leads the paragraph. With comments of its own the
				// statement keeps a break too: they were written about it, not about what was put in front.
				if (FirstInserted && Statements[Index])
				{
					int32 BlankLines = 0;
					if (FLangTrivia* Original = Module.Trivia.Find(Statements[Index].Get()))
					{
						BlankLines = Original->BlankLinesBefore;
						if (Original->Leading.Num() == 0)
						{
							Original->BlankLinesBefore = 0;
						}
					}
					if (BlankLines > 0)
					{
						FLangTrivia Leading;
						Leading.BlankLinesBefore = BlankLines;
						Module.Trivia.Add(FirstInserted, MoveTemp(Leading));
					}
				}
				Rebuilt.Add(MoveTemp(Statements[Index]));
			}
			Statements = MoveTemp(Rebuilt);
			Frame.Insertions.Reset();
		}

		void FMigrator::VisitLoopBody(FStmt& Body)
		{
			// A value hoisted before the loop would be the first iteration's in every one after it.
			HoistedCalls.Reset();
			VisitStmt(Body);
			HoistedCalls.Reset();
		}

		void FMigrator::VisitStmt(FStmt& Stmt)
		{
			switch (Stmt.Kind)
			{
			case ENodeKind::VarDeclStmt:
			{
				FVarDeclStmt& Declaration = static_cast<FVarDeclStmt&>(Stmt);
				for (FDeclarator& Declarator : Declaration.Declarators)
				{
					if (!Declarator.Initializer)
					{
						continue;
					}
					// `float Cam = UE.Expression(Class = "CameraPositionWS", OutputType = "float")`: 1.x typed the variable by
					// that word and wired the whole node. A `.dss` types the call by its class, so the variable is declared
					// what the node makes -- the same wire -- and where that width does not fit later, 2.0 says so there.
					const FBoundExpr* Initializer = Bound.Find(*Declarator.Initializer);
					if (Initializer && Initializer->bLegacyDeclaredType && Declaration.Declarators.Num() == 1
						&& Initializer->LegacyCatalogType.IsNumeric() && Initializer->LegacyCatalogType.Cols == 1
						&& Initializer->LegacyCatalogType.Rows > Initializer->Type.Rows)
					{
						Declaration.Type = MakeExactTypeRef(Initializer->LegacyCatalogType);
					}
					VisitExprSlot(Declarator.Initializer, /* bIsStatementExpression */ false);
				}
				break;
			}
			case ENodeKind::ExprStmt:
			{
				FExprStmt& Statement = static_cast<FExprStmt&>(Stmt);
				if (!Statement.Expression)
				{
					break;
				}
				// The binding first: the key is the node the binder saw, and the walk may put another in its place.
				const FBoundExpr* Binding = Bound.Find(*Statement.Expression);
				VisitExprSlot(Statement.Expression, /* bIsStatementExpression */ true);
				FCallExpr* Call = Statement.Expression ? Statement.Expression->As<FCallExpr>() : nullptr;
				if (Call && Binding && Binding->Kind == EBoundExprKind::FunctionCall)
				{
					MigrateStatementCall(Statement, *Call, *Binding);
				}
				break;
			}
			case ENodeKind::BlockStmt:
				WalkStatements(static_cast<FBlockStmt&>(Stmt).Statements);
				break;
			case ENodeKind::IfStmt:
			{
				FIfStmt& If = static_cast<FIfStmt&>(Stmt);
				if (If.Condition) { VisitExprSlot(If.Condition, false); }
				if (If.Then) { VisitStmt(*If.Then); }
				if (If.Else) { VisitStmt(*If.Else); }
				break;
			}
			case ENodeKind::ForStmt:
			{
				FForStmt& For = static_cast<FForStmt&>(Stmt);
				if (For.Init) { VisitStmt(*For.Init); }
				HoistedCalls.Reset();
				if (For.Condition) { VisitExprSlot(For.Condition, false); }
				if (For.Step) { VisitExprSlot(For.Step, false); }
				if (For.Body) { VisitLoopBody(*For.Body); }
				break;
			}
			case ENodeKind::WhileStmt:
			{
				FWhileStmt& While = static_cast<FWhileStmt&>(Stmt);
				HoistedCalls.Reset();
				if (While.Condition) { VisitExprSlot(While.Condition, false); }
				if (While.Body) { VisitLoopBody(*While.Body); }
				break;
			}
			case ENodeKind::DoWhileStmt:
			{
				FDoWhileStmt& DoWhile = static_cast<FDoWhileStmt&>(Stmt);
				if (DoWhile.Body) { VisitLoopBody(*DoWhile.Body); }
				if (DoWhile.Condition) { VisitExprSlot(DoWhile.Condition, false); }
				HoistedCalls.Reset();
				break;
			}
			case ENodeKind::ReturnStmt:
			{
				FReturnStmt& Return = static_cast<FReturnStmt&>(Stmt);
				if (Return.Value) { VisitExprSlot(Return.Value, false); }
				break;
			}
			default:
				break;
			}
		}

		void FMigrator::VisitCallArguments(FCallExpr& Call)
		{
			for (FArgument& Argument : Call.Arguments)
			{
				if (Argument.Value)
				{
					VisitExprSlot(Argument.Value, false);
				}
			}
		}

		/** `rgb`, as 1.x spelled the leading components it took. */
		static FString LeadingSwizzle(const int32 Width)
		{
			return FString(TEXT("rgba")).Left(FMath::Clamp(Width, 1, 4));
		}

		void FMigrator::VisitExprSlot(FExprPtr& Slot, const bool bIsStatementExpression)
		{
			if (!Slot)
			{
				return;
			}

			// Read before the walk: the key is the node the binder saw, and the walk may put another in its place.
			const FBoundExpr* Binding = Bound.Find(*Slot);
			const bool bNameFirstOutput = Binding && Binding->bLegacyDefaultOutput && Binding->Type.IsNode();
			const int32 CatalogIndex = bNameFirstOutput ? Binding->Type.CatalogIndex : INDEX_NONE;
			// Set with Truncate, and with Identity at a node's pin, where 1.x connected the wider value as it was.
			const int32 TruncateWidth = Binding ? Binding->LegacyTruncateWidth : 0;

			VisitExprSlotInner(Slot, bIsStatementExpression);

			// Rule L3c: `float2 vp = UE.ScreenPosition();` read the node's first output, and a `.dss` says which one that
			// is: `UE.ScreenPosition().ViewportUV`, or `[0]` for an output whose name is not a word.
			if (bNameFirstOutput && Slot && Bound.Catalog && Bound.Catalog->Expressions.IsValidIndex(CatalogIndex)
				&& Bound.Catalog->Expressions[CatalogIndex].Outputs.Num() > 0)
			{
				const FString& OutputName = Bound.Catalog->Expressions[CatalogIndex].Outputs[0].Name;
				const FLangSpan Span = Slot->Span;
				Slot = (DecompileAst::IsIdentifierText(OutputName) && !DecompileAst::IsReservedIdentifier(OutputName))
					? DecompileAst::MakeMemberExpr(MoveTemp(Slot), OutputName)
					: DecompileAst::MakeIndexExpr(MoveTemp(Slot), 0);
				Slot->Span = Span;
			}

			// Rule L22: a value wider than the place it goes was cut down to it, and a `.dss` writes that: `Colour.rgb`.
			if (TruncateWidth >= 1 && TruncateWidth <= 3 && Slot)
			{
				const FLangSpan Span = Slot->Span;
				Slot = DecompileAst::MakeMemberExpr(MoveTemp(Slot), LeadingSwizzle(TruncateWidth));
				Slot->Span = Span;
			}
		}

		void FMigrator::VisitExprSlotInner(FExprPtr& Slot, const bool bIsStatementExpression)
		{
			FExpr& Expr = *Slot;
			// Looked up before anything below moves or renames: the key is the node the binder saw.
			const FBoundExpr* Binding = Bound.Find(Expr);

			switch (Expr.Kind)
			{
			case ENodeKind::IdentifierExpr:
				if (Binding)
				{
					RenameIdentifier(static_cast<FIdentifierExpr&>(Expr), *Binding);
				}
				return;

			case ENodeKind::MemberExpr:
			case ENodeKind::IndexExpr:
			{
				FExprPtr& ObjectSlot = Expr.Kind == ENodeKind::MemberExpr ? static_cast<FMemberExpr&>(Expr).Object : static_cast<FIndexExpr&>(Expr).Object;
				if (Binding && Binding->Kind == EBoundExprKind::FunctionCallOutput && ObjectSlot && ObjectSlot->Is<FCallExpr>())
				{
					// Rule L3b. The call's arguments are walked and the call is not: it is not a value call that left its
					// outputs out, it is this selection.
					const FBoundExpr* CallBinding = Bound.Find(*ObjectSlot);
					VisitCallArguments(static_cast<FCallExpr&>(*ObjectSlot));
					if (CallBinding && CallBinding->Kind == EBoundExprKind::FunctionCall)
					{
						const FLangSpan Span = Expr.Span;
						HoistCall(Slot, ObjectSlot, *CallBinding, Binding->FieldIndex, Span);
					}
					return;
				}

				if (ObjectSlot)
				{
					VisitExprSlot(ObjectSlot, false);
				}
				if (Expr.Kind == ENodeKind::IndexExpr && static_cast<FIndexExpr&>(Expr).Index)
				{
					VisitExprSlot(static_cast<FIndexExpr&>(Expr).Index, false);
				}

				// `UE.SceneTexture(...)[0]`: 1.x selected an output by its number (`OutputIndex = 0`, and the front end's own
				// shorthands), and a `.dss` reads better with the output's name where it has one that is a word.
				if (Expr.Kind == ENodeKind::IndexExpr && Binding && Binding->Kind == EBoundExprKind::NodeOutput
					&& Bound.Catalog && Bound.Catalog->Expressions.IsValidIndex(Binding->Index)
					&& Bound.Catalog->Expressions[Binding->Index].Outputs.IsValidIndex(Binding->FieldIndex))
				{
					const FString& OutputName = Bound.Catalog->Expressions[Binding->Index].Outputs[Binding->FieldIndex].Name;
					if (DecompileAst::IsIdentifierText(OutputName) && !DecompileAst::IsReservedIdentifier(OutputName)
						&& Bound.Catalog->Expressions[Binding->Index].FindOutput(OutputName) == Binding->FieldIndex)
					{
						const FLangSpan Span = Expr.Span;
						FExprPtr Node = MoveTemp(static_cast<FIndexExpr&>(Expr).Object);
						Slot = DecompileAst::MakeMemberExpr(MoveTemp(Node), OutputName);
						Slot->Span = Span;
						// Expr is gone with the slot's old node.
						return;
					}
				}

				// `Output = "Modifier Payload"`: an output selected by a name a `.dss` cannot spell as a member is selected
				// by its number.
				if (Expr.Kind == ENodeKind::MemberExpr && Binding && Binding->Kind == EBoundExprKind::NodeOutput
					&& !DecompileAst::IsIdentifierText(static_cast<FMemberExpr&>(Expr).Member))
				{
					const FLangSpan Span = Expr.Span;
					const int32 OutputIndex = Binding->FieldIndex;
					FExprPtr Node = MoveTemp(static_cast<FMemberExpr&>(Expr).Object);
					Slot = DecompileAst::MakeIndexExpr(MoveTemp(Node), OutputIndex);
					Slot->Span = Span;
					// Expr is gone with the slot's old node.
					return;
				}

				// `Base.basecolor`: the catalog's spelling.
				if (Expr.Kind == ENodeKind::MemberExpr && Binding && Binding->Kind == EBoundExprKind::MaterialField
					&& Bound.Catalog && Bound.Catalog->MaterialAttributes.IsValidIndex(Binding->FieldIndex))
				{
					FMemberExpr& Member = static_cast<FMemberExpr&>(Expr);
					const IR::FCatalogMaterialAttribute& Attribute = Bound.Catalog->MaterialAttributes[Binding->FieldIndex];
					const bool bSpelledExactly = Member.Member.Equals(Attribute.Name, ESearchCase::CaseSensitive)
						|| Attribute.Aliases.ContainsByPredicate([&Member](const FString& Alias) { return Alias.Equals(Member.Member, ESearchCase::CaseSensitive); });
					if (!bSpelledExactly)
					{
						Member.Member = Attribute.Name;
					}
				}
				// `UE.VertexColor().rgb` for an output declared `RGB`: the catalog's spelling again (Index is the class,
				// FieldIndex the output).
				else if (Expr.Kind == ENodeKind::MemberExpr && Binding && Binding->Kind == EBoundExprKind::NodeOutput
					&& Bound.Catalog && Bound.Catalog->Expressions.IsValidIndex(Binding->Index)
					&& Bound.Catalog->Expressions[Binding->Index].Outputs.IsValidIndex(Binding->FieldIndex))
				{
					FMemberExpr& Member = static_cast<FMemberExpr&>(Expr);
					const IR::FCatalogPin& Output = Bound.Catalog->Expressions[Binding->Index].Outputs[Binding->FieldIndex];
					const bool bSpelledExactly = Member.Member.Equals(Output.Name, ESearchCase::CaseSensitive)
						|| Output.Aliases.ContainsByPredicate([&Member](const FString& Alias) { return Alias.Equals(Member.Member, ESearchCase::CaseSensitive); });
					if (!bSpelledExactly)
					{
						// Only the case changes: whichever of the name and its aliases was meant keeps its own letters.
						if (Member.Member.Equals(Output.Name, ESearchCase::IgnoreCase))
						{
							Member.Member = Output.Name;
						}
						else if (const FString* Alias = Output.Aliases.FindByPredicate([&Member](const FString& Candidate) { return Candidate.Equals(Member.Member, ESearchCase::IgnoreCase); }))
						{
							Member.Member = *Alias;
						}
					}
				}
				return;
			}

			case ENodeKind::CallExpr:
			{
				FCallExpr& Call = static_cast<FCallExpr&>(Expr);
				VisitCallArguments(Call);
				if (!Binding)
				{
					return;
				}
				if (Binding->Kind == EBoundExprKind::CoreOp)
				{
					MigrateCoreOpCall(Call, *Binding);
				}
				else if (Binding->Kind == EBoundExprKind::ReflectedCall)
				{
					MigrateReflectedCall(Call, *Binding);
				}
				else if (Binding->Kind == EBoundExprKind::FunctionCall && Bound.Functions.IsValidIndex(Binding->Index))
				{
					if (bIsStatementExpression)
					{
						// MigrateStatementCall, once the statement has its expression back.
						return;
					}
					MigrateValueCall(Slot, Call, *Binding, Bound.Functions[Binding->Index]);
				}
				return;
			}

			case ENodeKind::UnaryExpr:
			{
				FUnaryExpr& Unary = static_cast<FUnaryExpr&>(Expr);
				if (Unary.Operand)
				{
					VisitExprSlot(Unary.Operand, false);
					const bool bWrites = Unary.Op == EUnaryOp::PreIncrement || Unary.Op == EUnaryOp::PreDecrement
						|| Unary.Op == EUnaryOp::PostIncrement || Unary.Op == EUnaryOp::PostDecrement;
					if (bWrites && Unary.Operand)
					{
						NoteWrite(*Unary.Operand);
					}
				}
				return;
			}
			case ENodeKind::BinaryExpr:
				if (static_cast<FBinaryExpr&>(Expr).Left) { VisitExprSlot(static_cast<FBinaryExpr&>(Expr).Left, false); }
				if (static_cast<FBinaryExpr&>(Expr).Right) { VisitExprSlot(static_cast<FBinaryExpr&>(Expr).Right, false); }
				return;
			case ENodeKind::AssignExpr:
			{
				FAssignExpr& Assign = static_cast<FAssignExpr&>(Expr);
				if (Assign.Value) { VisitExprSlot(Assign.Value, false); }
				if (Assign.Target)
				{
					VisitExprSlot(Assign.Target, false);
					if (Assign.Target)
					{
						NoteWrite(*Assign.Target);
					}
				}
				return;
			}
			case ENodeKind::ConditionalExpr:
				if (static_cast<FConditionalExpr&>(Expr).Condition) { VisitExprSlot(static_cast<FConditionalExpr&>(Expr).Condition, false); }
				if (static_cast<FConditionalExpr&>(Expr).TrueValue) { VisitExprSlot(static_cast<FConditionalExpr&>(Expr).TrueValue, false); }
				if (static_cast<FConditionalExpr&>(Expr).FalseValue) { VisitExprSlot(static_cast<FConditionalExpr&>(Expr).FalseValue, false); }
				return;
			case ENodeKind::CastExpr:
				if (static_cast<FCastExpr&>(Expr).Operand) { VisitExprSlot(static_cast<FCastExpr&>(Expr).Operand, false); }
				return;
			case ENodeKind::ParenExpr:
				if (static_cast<FParenExpr&>(Expr).Inner) { VisitExprSlot(static_cast<FParenExpr&>(Expr).Inner, false); }
				return;
			case ENodeKind::InitializerListExpr:
				for (FExprPtr& Element : static_cast<FInitializerListExpr&>(Expr).Elements)
				{
					if (Element) { VisitExprSlot(Element, false); }
				}
				return;
			default:
				return;
			}
		}

		void FMigrator::RenameIdentifier(FIdentifierExpr& Identifier, const FBoundExpr& Binding)
		{
			// Rule L19: 1.x read names ignoring case, and the binder took the one declaration that matched.
			const FString* Declared = nullptr;
			if (Binding.Kind == EBoundExprKind::Local && BoundFunction && BoundFunction->Locals.IsValidIndex(Binding.LocalSlot))
			{
				Declared = &BoundFunction->Locals[Binding.LocalSlot].Name;
			}
			else if (Binding.Kind == EBoundExprKind::Param && BoundFunction && BoundFunction->Params.IsValidIndex(Binding.Index))
			{
				Declared = &BoundFunction->Params[Binding.Index].Name;
			}
			else if (Binding.Kind == EBoundExprKind::Global && Bound.Globals.IsValidIndex(Binding.Index))
			{
				Declared = &Bound.Globals[Binding.Index].Name;
			}
			if (Declared && !Declared->IsEmpty() && !Identifier.Name.Equals(*Declared, ESearchCase::CaseSensitive))
			{
				Identifier.Name = *Declared;
			}
		}

		void FMigrator::MigrateCoreOpCall(FCallExpr& Call, const FBoundExpr& Binding)
		{
			// Rules L2 and L19: `mix`, `Lerp`. Whatever the call was written as, the op has one HLSL name.
			FIdentifierExpr* Callee = Call.Callee ? Call.Callee->As<FIdentifierExpr>() : nullptr;
			const TCHAR* HlslName = IR::GetCoreOpInfo(Binding.CoreOp).HlslName;
			if (Callee && HlslName && !Callee->Name.Equals(HlslName, ESearchCase::CaseSensitive))
			{
				Callee->Name = HlslName;
			}
		}

		void FMigrator::MigrateReflectedCall(FCallExpr& Call, const FBoundExpr& Binding)
		{
			if (!Bound.Catalog || !Bound.Catalog->Expressions.IsValidIndex(Binding.Index))
			{
				return;
			}
			const IR::FCatalogExpression& Class = Bound.Catalog->Expressions[Binding.Index];
			const auto IsSpelling = [](const FString& Written, const FString& Name, const TArray<FString>& Aliases)
			{
				return Written.Equals(Name, ESearchCase::CaseSensitive)
					|| Aliases.ContainsByPredicate([&Written](const FString& Alias) { return Alias.Equals(Written, ESearchCase::CaseSensitive); });
			};
			/** The `Class = "..."` argument to drop once the call is written `UE.<Class>(...)`; INDEX_NONE keeps it. */
			int32 NamedClassArgument = INDEX_NONE;

			// `UE.texcoord(...)`: the catalog's spelling. And `UE.Expression(Class = "SceneTexture", ...)`, which is how
			// 1.x named a class it had no word for and how the legacy front end spells its own expansions (it has no
			// catalog to ask): when the language has a name for the class, the call is written with it. The generic
			// form stays for a class only `Class = "..."` reaches.
			if (FMemberExpr* Callee = Call.Callee ? Call.Callee->As<FMemberExpr>() : nullptr)
			{
				const bool bGeneric = Callee->Member.Equals(TEXT("Expression"), ESearchCase::IgnoreCase);
				if (!bGeneric && !IsSpelling(Callee->Member, Class.ShortName, Class.Aliases))
				{
					Callee->Member = Class.ShortName;
				}
				else if (bGeneric
					&& DecompileAst::IsIdentifierText(Class.ShortName) && !DecompileAst::IsReservedIdentifier(Class.ShortName)
					&& !Class.ShortName.Equals(TEXT("Expression"), ESearchCase::IgnoreCase)
					&& Bound.Catalog->FindExpression(Class.Namespace, Class.ShortName) == Binding.Index)
				{
					const FIdentifierExpr* Root = Callee->Object ? Callee->Object->As<FIdentifierExpr>() : nullptr;
					const int32 ClassArgument = Call.Arguments.IndexOfByPredicate([](const FArgument& Argument)
					{
						return Argument.Name.Equals(TEXT("Class"), ESearchCase::CaseSensitive);
					});
					if (Root && Root->Name.Equals(Class.Namespace, ESearchCase::CaseSensitive) && ClassArgument != INDEX_NONE)
					{
						Callee->Member = Class.ShortName;
						NamedClassArgument = ClassArgument;
					}
				}
			}

			for (const FBoundArgument& Matched : Binding.Args)
			{
				if (!Call.Arguments.IsValidIndex(Matched.ArgumentIndex) || Matched.TargetIndex == INDEX_NONE)
				{
					continue;
				}
				FArgument& Argument = Call.Arguments[Matched.ArgumentIndex];

				if (Matched.bIsProperty && Class.Properties.IsValidIndex(Matched.TargetIndex))
				{
					const IR::FCatalogProperty& Property = Class.Properties[Matched.TargetIndex];
					if (!Argument.Name.IsEmpty() && !IsSpelling(Argument.Name, Property.Name, Property.Aliases))
					{
						Argument.Name = Property.Name;
					}

					// Rule L12: the enumerator the way the catalog lists it.
					FString Written;
					if (Property.Type == IR::ECatalogValueType::Enum && Property.EnumValues.Num() > 0 && Argument.Value && FlattenWord(*Argument.Value, Written))
					{
						const bool bListed = Property.EnumValues.ContainsByPredicate([&Written](const FString& Value) { return Value.Equals(Written, ESearchCase::CaseSensitive); });
						FString Canonical;
						if (!bListed && TryCanonicaliseEnumerator(Written, Property, Canonical))
						{
							Argument.Value = (DecompileAst::IsIdentifierText(Canonical) && !DecompileAst::IsReservedIdentifier(Canonical))
								? DecompileAst::MakeIdentifierExpr(Canonical)
								: DecompileAst::MakeStringExpr(Canonical);
						}
					}
				}
				else if (!Matched.bIsProperty && Class.Inputs.IsValidIndex(Matched.TargetIndex))
				{
					const IR::FCatalogPin& Pin = Class.Inputs[Matched.TargetIndex];
					if (!Argument.Name.IsEmpty() && Argument.PinIndex == INDEX_NONE && !IsSpelling(Argument.Name, Pin.Name, Pin.Aliases))
					{
						Argument.Name = Pin.Name;
					}
				}
			}

			// Rule L21: a `DefaultValue` the class has no property for, which the binder left unbound (DSH5288) the way
			// 1.x ignored it; and the `Class` argument of a call that is now written by the class's name. Last, and from
			// the back: Binding.Args addresses the arguments by index.
			for (int32 Index = Call.Arguments.Num() - 1; Index >= 0; --Index)
			{
				const bool bBound = Binding.Args.ContainsByPredicate([Index](const FBoundArgument& Matched) { return Matched.ArgumentIndex == Index; });
				if (Index == NamedClassArgument
					|| (!bBound && Call.Arguments[Index].Name.Equals(TEXT("DefaultValue"), ESearchCase::IgnoreCase)))
				{
					Call.Arguments.RemoveAt(Index);
				}
			}
		}

		void FMigrator::CanonicaliseUserCall(FCallExpr& Call, const FBoundExpr& Binding, const FBoundFunction& Callee)
		{
			if (FIdentifierExpr* CalleeName = Call.Callee ? Call.Callee->As<FIdentifierExpr>() : nullptr)
			{
				if (!CalleeName->Name.Equals(Callee.Name, ESearchCase::CaseSensitive))
				{
					CalleeName->Name = Callee.Name;
				}
			}

			for (const FBoundArgument& Matched : Binding.Args)
			{
				if (!Call.Arguments.IsValidIndex(Matched.ArgumentIndex))
				{
					continue;
				}
				FArgument& Argument = Call.Arguments[Matched.ArgumentIndex];
				if (!Callee.Params.IsValidIndex(Matched.TargetIndex))
				{
					// The return value's receiver (rule L5): written like an `out`.
					if (Matched.TargetIndex == Callee.Params.Num() && Argument.Value)
					{
						NoteWrite(*Argument.Value);
					}
					continue;
				}

				const FBoundParam& Param = Callee.Params[Matched.TargetIndex];
				if (!Argument.Name.IsEmpty() && !Argument.Name.Equals(Param.Name, ESearchCase::CaseSensitive))
				{
					Argument.Name = Param.Name;
				}
				if (Param.Direction != EParamDirection::In && Argument.Value)
				{
					NoteWrite(*Argument.Value);
				}
			}

			// Rule L25: `F(a, default, c)` left an input to its default, and 2.0 leaves an argument out by not writing it.
			// The placeholder loses its value here and goes once the whole body is done (DefaultedCalls) -- until then the
			// binder's argument indices still address this list -- and what follows it is passed by name.
			bool bAfterPlaceholder = false;
			for (int32 Index = 0; Index < Call.Arguments.Num(); ++Index)
			{
				FArgument& Argument = Call.Arguments[Index];
				const FBoundArgument* Matched = Binding.Args.FindByPredicate([Index](const FBoundArgument& Candidate) { return Candidate.ArgumentIndex == Index; });
				const FIdentifierExpr* Word = Argument.Value ? Argument.Value->As<FIdentifierExpr>() : nullptr;
				if (!Matched && Word && Word->Name.Equals(TEXT("default"), ESearchCase::CaseSensitive) && !Bound.Find(*Word))
				{
					Argument.Value.Reset();
					DefaultedCalls.AddUnique(&Call);
					bAfterPlaceholder = true;
					continue;
				}
				if (bAfterPlaceholder && Argument.Name.IsEmpty() && Matched && Callee.Params.IsValidIndex(Matched->TargetIndex))
				{
					Argument.Name = Callee.Params[Matched->TargetIndex].Name;
				}
			}
		}

		void FMigrator::PassAbsentOuts(FCallExpr& Call, const FBoundExpr& Binding, const FBoundFunction& Callee, TArray<FExprPtr>& OutParamOutputs)
		{
			OutParamOutputs.Reset();
			OutParamOutputs.SetNum(Callee.Params.Num());
			for (int32 ParamIndex = 0; ParamIndex < Callee.Params.Num(); ++ParamIndex)
			{
				const FBoundParam& Param = Callee.Params[ParamIndex];
				if (Param.Direction == EParamDirection::In)
				{
					continue;
				}

				const FBoundArgument* Passed = Binding.Args.FindByPredicate([ParamIndex](const FBoundArgument& Argument) { return Argument.TargetIndex == ParamIndex; });
				if (Passed && Call.Arguments.IsValidIndex(Passed->ArgumentIndex) && Call.Arguments[Passed->ArgumentIndex].Value)
				{
					// Already a variable of the caller's: that is where the output is.
					OutParamOutputs[ParamIndex] = DecompileAst::CloneExpr(*Call.Arguments[Passed->ArgumentIndex].Value);
					continue;
				}
				if (Param.Direction == EParamDirection::InOut)
				{
					// Not reachable: the binder lets an `out` be absent and nothing else (DSH4217).
					continue;
				}

				// The declared type, from the declaration: an `out` argument has to match it exactly.
				const FTypeRef Type = (Callee.Decl && Callee.Decl->Params.IsValidIndex(ParamIndex))
					? Callee.Decl->Params[ParamIndex].Type
					: MakeExactTypeRef(Param.Type);
				const FString Local = ClaimLocalName(Callee.Name + TEXT("_") + Param.Name);
				QueueStatement(DecompileAst::MakeVarDeclStmt(Type, Local, FExprPtr()));
				OutParamOutputs[ParamIndex] = DecompileAst::MakeIdentifierExpr(Local);
				// By name: optional inputs in front of it may have been left out too, and a name does not care.
				DecompileAst::AddNamedArgument(Call, Param.Name, DecompileAst::MakeIdentifierExpr(Local));
			}
		}

		void FMigrator::MigrateValueCall(FExprPtr& Slot, FCallExpr& Call, const FBoundExpr& Binding, const FBoundFunction& Callee)
		{
			// A 1.x value call passes inputs only and IS the function's first output (rule L3b): the return value, or for
			// a function that returns nothing its first `out`.
			if (Callee.ReturnType.IsVoid())
			{
				const FLangSpan Span = Call.Span;
				HoistCall(Slot, Slot, Binding, 0, Span);
				return;
			}

			CanonicaliseUserCall(Call, Binding, Callee);

			bool bLeavesOutputsOut = false;
			for (int32 ParamIndex = 0; ParamIndex < Callee.Params.Num(); ++ParamIndex)
			{
				if (Callee.Params[ParamIndex].Direction != EParamDirection::Out)
				{
					continue;
				}
				const bool bPassed = Binding.Args.ContainsByPredicate([ParamIndex](const FBoundArgument& Argument) { return Argument.TargetIndex == ParamIndex; });
				bLeavesOutputsOut = bLeavesOutputsOut || !bPassed;
			}
			if (bLeavesOutputsOut)
			{
				// 2.0 wants every `out` passed: to variables nobody reads.
				TArray<FExprPtr> Unread;
				PassAbsentOuts(Call, Binding, Callee, Unread);
			}
		}

		void FMigrator::HoistCall(FExprPtr& Slot, FExprPtr& CallSlot, const FBoundExpr& CallBinding, const int32 Ordinal, const FLangSpan& Span)
		{
			if (!Bound.Functions.IsValidIndex(CallBinding.Index) || !CallSlot || !CallSlot->Is<FCallExpr>())
			{
				return;
			}
			const FBoundFunction& Callee = Bound.Functions[CallBinding.Index];
			FCallExpr& Call = static_cast<FCallExpr&>(*CallSlot);

			// The writes of this call's own `out` arguments end older sharings, not the one about to be made.
			CanonicaliseUserCall(Call, CallBinding, Callee);

			// Equal calls were one node in 1.x, and they are one statement here. The text is taken before the outputs are
			// passed, so a second occurrence -- which has none yet -- finds the first.
			const FString Key = PrintDreamShaderLangExpr(Call);
			FHoistedStatement* Existing = HoistedCalls.Find(Key);
			if (Existing)
			{
				// This occurrence is not kept: what stands in Slot is replaced below, and the call goes with it.
				DefaultedCalls.Remove(&Call);
			}
			if (!Existing)
			{
				FHoistedStatement Hoisted;
				Hoisted.Depth = Frames.Num();
				CollectIdentifiers(Call, Hoisted.Reads);

				TArray<FExprPtr> ParamOutputs;
				PassAbsentOuts(Call, CallBinding, Callee, ParamOutputs);

				// 1.x output order: the return value when there is one, then the `out` parameters as declared.
				FString ReturnLocal;
				if (!Callee.ReturnType.IsVoid())
				{
					ReturnLocal = ClaimLocalName(Callee.Name + TEXT("_Result"));
					Hoisted.Outputs.Add(DecompileAst::MakeIdentifierExpr(ReturnLocal));
				}
				for (int32 ParamIndex = 0; ParamIndex < Callee.Params.Num(); ++ParamIndex)
				{
					if (Callee.Params[ParamIndex].Direction != EParamDirection::In)
					{
						Hoisted.Outputs.Add(MoveTemp(ParamOutputs[ParamIndex]));
					}
				}

				if (ReturnLocal.IsEmpty())
				{
					QueueStatement(DecompileAst::MakeExprStmt(MoveTemp(CallSlot)));
				}
				else
				{
					const FTypeRef ReturnType = Callee.Decl ? Callee.Decl->ReturnType : MakeExactTypeRef(Callee.ReturnType);
					QueueStatement(DecompileAst::MakeVarDeclStmt(ReturnType, ReturnLocal, MoveTemp(CallSlot)));
				}
				Existing = &HoistedCalls.Add(Key, MoveTemp(Hoisted));
			}

			if (!Existing->Outputs.IsValidIndex(Ordinal) || !Existing->Outputs[Ordinal])
			{
				Diagnostics.Warning(TEXT("DSH9094"), Span, FText::Format(
					LOCTEXT("SelectedOutputMissing", "This reads output {0} of '{1}', which has no variable to be read from; the expression is replaced by '0.0'."),
					FText::AsNumber(Ordinal),
					FText::FromString(Callee.Name)));
				Slot = DecompileAst::MakeNumberExpr(0.0);
				return;
			}
			// Slot may be CallSlot itself; either way what stood here is gone now.
			Slot = DecompileAst::CloneExpr(*Existing->Outputs[Ordinal]);
		}

		void FMigrator::MigrateStatementCall(FExprStmt& Statement, FCallExpr& Call, const FBoundExpr& Binding)
		{
			if (!Bound.Functions.IsValidIndex(Binding.Index))
			{
				return;
			}
			const FBoundFunction& Callee = Bound.Functions[Binding.Index];
			CanonicaliseUserCall(Call, Binding, Callee);

			TArray<int32> ParamOfArgument;
			ParamOfArgument.Init(INDEX_NONE, Call.Arguments.Num());
			int32 ReceiverArgument = INDEX_NONE;
			for (const FBoundArgument& Matched : Binding.Args)
			{
				if (!Call.Arguments.IsValidIndex(Matched.ArgumentIndex))
				{
					continue;
				}
				if (Matched.TargetIndex == Callee.Params.Num())
				{
					ReceiverArgument = Matched.ArgumentIndex;
				}
				else
				{
					ParamOfArgument[Matched.ArgumentIndex] = Matched.TargetIndex;
				}
			}

			// Rule L5: a 1.x statement call passes its inputs in input order and its receivers last. 2.0 passes arguments
			// in parameter order. Where the two part ways -- an `out` declared between inputs, optional inputs left out --
			// the argument and everything after it is named, which finds the parameter whatever the order.
			bool bNaming = false;
			int32 Position = 0;
			for (int32 ArgumentIndex = 0; ArgumentIndex < Call.Arguments.Num(); ++ArgumentIndex)
			{
				if (ArgumentIndex == ReceiverArgument)
				{
					continue;
				}
				FArgument& Argument = Call.Arguments[ArgumentIndex];
				const int32 ParamIndex = ParamOfArgument[ArgumentIndex];
				if (!Argument.Name.IsEmpty())
				{
					bNaming = true;
				}
				else if (Callee.Params.IsValidIndex(ParamIndex) && (bNaming || ParamIndex != Position))
				{
					bNaming = true;
					Argument.Name = Callee.Params[ParamIndex].Name;
				}
				++Position;
			}

			if (!Call.Arguments.IsValidIndex(ReceiverArgument) || !Call.Arguments[ReceiverArgument].Value)
			{
				return;
			}

			// `F(a, b, R, O);` is `R = F(a, b, O);` -- and `T R = F(a, b, O);` when this call is what declared R.
			FExprPtr Receiver = MoveTemp(Call.Arguments[ReceiverArgument].Value);
			Call.Arguments.RemoveAt(ReceiverArgument);

			const FIdentifierExpr* ReceiverName = Receiver->As<FIdentifierExpr>();
			const bool bIsTopStatement = Frames.Num() == 1 && Frames[0].Statements
				&& Frames[0].Statements->IsValidIndex(Frames[0].Index)
				&& (*Frames[0].Statements)[Frames[0].Index].Get() == &Statement;
			if (ReceiverName && bIsTopStatement)
			{
				const int32 Implicit = ImplicitHere.IndexOfByPredicate([ReceiverName](const FBoundLocal* Local)
				{
					return Local->Name.Equals(ReceiverName->Name, ESearchCase::CaseSensitive);
				});
				if (Implicit != INDEX_NONE)
				{
					const FTypeRef ReturnType = Callee.Decl ? Callee.Decl->ReturnType : MakeExactTypeRef(Callee.ReturnType);
					Replacement = DecompileAst::MakeVarDeclStmt(ReturnType, ReceiverName->Name, MoveTemp(Statement.Expression));
					ImplicitHere.RemoveAt(Implicit);
					return;
				}
			}

			Statement.Expression = DecompileAst::MakeAssignExpr(MoveTemp(Receiver), MoveTemp(Statement.Expression));
		}

		void FMigrator::MoveBlockComments()
		{
			// `// what this is` above `Shader(...)` hangs on the function the block became, because that is the node that
			// starts where the block's header did. The block's Settings and Properties are declarations of their own and
			// are written in front of the function, which would leave the comment between them and it: it goes to the first
			// of them, where a reader meets the block, and the function stands apart from the run above it.
			for (const FLegacyBlock& Block : Legacy.Blocks)
			{
				const int32 MainIndex = Module.Declarations.IndexOfByPredicate([&Block](const FDeclPtr& Decl) { return Decl.Get() == Block.Decl; });
				if (Block.Decl == nullptr || MainIndex == INDEX_NONE)
				{
					continue;
				}

				const auto IsOfBlock = [this, &Block](const FDecl& Decl)
				{
					return Legacy.Sections.ContainsByPredicate([&Block, &Decl](const FLegacySection& Section)
					{
						return Section.Block == Block.Decl
							&& Decl.Span.Offset >= Section.BodySpan.Offset
							&& Decl.Span.Offset < Section.BodySpan.End();
					});
				};

				int32 FirstIndex = MainIndex;
				while (FirstIndex > 0 && Module.Declarations[FirstIndex - 1].IsValid() && IsOfBlock(*Module.Declarations[FirstIndex - 1]))
				{
					--FirstIndex;
				}
				if (FirstIndex == MainIndex)
				{
					continue;
				}

				TArray<FLangComment> Comments;
				int32 BlankLinesBefore = 0;
				if (FLangTrivia* MainTrivia = Module.Trivia.Find(Block.Decl))
				{
					Comments = MoveTemp(MainTrivia->Leading);
					MainTrivia->Leading.Reset();
					BlankLinesBefore = MainTrivia->BlankLinesBefore;
				}
				// The map may move what it holds when it grows: every entry is asked for again.
				Module.Trivia.FindOrAdd(Block.Decl).BlankLinesBefore = 1;

				FLangTrivia& FirstTrivia = Module.Trivia.FindOrAdd(Module.Declarations[FirstIndex].Get());
				if (Comments.Num() > 0)
				{
					Comments.Append(MoveTemp(FirstTrivia.Leading));
					FirstTrivia.Leading = MoveTemp(Comments);
				}
				FirstTrivia.BlankLinesBefore = FMath::Max(FirstTrivia.BlankLinesBefore, BlankLinesBefore);
			}
		}

		bool FMigrator::Run()
		{
			for (FDeclPtr& Decl : Module.Declarations)
			{
				if (Decl)
				{
					MigrateDeclaration(*Decl);
				}
			}
			MoveBlockComments();
			return !bFailed;
		}
	}

	bool MigrateDreamShaderLegacyModule(
		FModule& Module,
		const FLegacyMigrationInfo& Legacy,
		const FBoundModule& Bound,
		const FLangMigrateOptions& Options,
		FLangDiagnosticSink& Diagnostics)
	{
		// What the rewrite needs is what the binder resolved; of the front end's own record it reads where a block's
		// sections sat, and the rest is the host's to report from.
		LegacyMigrate::FMigrator Migrator(Module, Legacy, Bound, Options, Diagnostics);
		const bool bMigrated = Migrator.Run();
		// The file is 2.0 from here on: `.dss`, whatever it was.
		Module.FileKind = ELangFileKind::Dss;
		return bMigrated;
	}

	void CollectDreamShaderComments(const FLangSourceText& Source, const bool bIncludeDocComments, TArray<FString>& OutComments)
	{
		OutComments.Reset();

		// `///` always as its own kind: with the kind off the lexer hands those lines over as ordinary comments, and then
		// there is no telling them apart.
		FLangLexOptions LexOptions;
		LexOptions.bEmitComments = true;
		LexOptions.bEmitDocComments = true;
		TArray<FLangToken> Tokens;
		FLangDiagnosticSink Unread;
		LexDreamShaderLang(Source, LexOptions, Tokens, Unread);

		for (const FLangToken& Token : Tokens)
		{
			const bool bIsDoc = Token.Kind == ELangTokenKind::DocComment;
			if (Token.Kind != ELangTokenKind::Comment && !(bIsDoc && bIncludeDocComments))
			{
				continue;
			}

			// What is compared is what was said, not how it was fenced: `// x`, `/// x` and `/* x */` all say "x".
			FString Text = Token.Text.TrimStartAndEnd();
			while (Text.StartsWith(TEXT("/")) || Text.StartsWith(TEXT("*")))
			{
				Text.RightChopInline(1);
			}
			while (Text.EndsWith(TEXT("/")) || Text.EndsWith(TEXT("*")))
			{
				Text.LeftChopInline(1);
			}
			Text.TrimStartAndEndInline();

			// Nor how it was spaced: the printer joins the pieces of a `///` line with its own gap and indents the lines
			// of a block comment its own way, and neither loses a word. Every run of white space is one space.
			FString Collapsed;
			Collapsed.Reserve(Text.Len());
			bool bInGap = false;
			for (const TCHAR Character : Text)
			{
				if (FChar::IsWhitespace(Character))
				{
					bInGap = true;
					continue;
				}
				if (bInGap && !Collapsed.IsEmpty())
				{
					Collapsed.AppendChar(TEXT(' '));
				}
				bInGap = false;
				Collapsed.AppendChar(Character);
			}
			if (!Collapsed.IsEmpty())
			{
				OutComments.Add(MoveTemp(Collapsed));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
