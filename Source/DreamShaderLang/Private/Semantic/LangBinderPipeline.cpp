// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Pipeline mode: the binder for a `.dsp`.
//
// A `.dsp` is a Custom Pass pipeline -- `#pragma pipeline(...)`, `uniform` parameters an activation may override,
// `static const` values, `buffer` declarations and `pass` blocks -- and it becomes one UDreamPassPipeline. Nothing in it
// lowers to a node. This pass reads the declarations into the payload the PassPipeline product carries
// (IR::FIRPassPipeline): every spelling canonical, every default applied -- a pass's injection point from the pragma, a
// buffer's resolution from its first writer, a compute pass's thread group from the `.usf`'s [numthreads] -- and every
// `param` folded to a parameter times a multiplier plus an offset, a constant, or the pipeline's weight.
//
// Then the rules V1-V13 of DreamShader_Plan/05 s7: names, keys and values (V1); which kind of pass may run at which
// injection point (V2, the matrix of 03 s2); where the built-in textures exist (V3, 02 s5.3); reads before writes in
// frame order (V4); a pass reading what it writes (V5); what a fullscreen material, a mesh material and a `.usf` must be
// (V6-V8, from FBindOptions::PipelineReferences, which the host resolved before binding); resolutions (V9); one
// tonemapper replacement (V10); `param` values (V11); exported buffers (V12); buffers nobody writes or reads (V13).
// Without references the engine facts are taken as written and their checks are skipped, said once (DSH7360), the way a
// `.dsi` without its parent's schema is (DSH7263).
//
// Uniform and constant initializers go through the ordinary expression binder (BindGlobals), and so does every numeric
// key value, so `Scale = HalfRes * 0.5` folds a `static const` the way an initializer does. Names, key words and filters
// are read off the tree: `Injection = PostProcess.AfterDOF` names a point, it is not a member access.
//
// The canonical spelling tables are in Lang/LangPipelineInternal.h; the two the runtime keeps as well (injection points,
// buffer formats) are handed out here (GetDreamShaderPassInjectionNames / GetDreamShaderPassFormatNames).
//
// Diagnostics owned by this file: DSH3311-DSH3317 (what a `.dsp` holds, and `#pragma pipeline` or a pipeline declaration
// outside one), DSH4400-DSH4408, DSH4410-DSH4415 (names and references), DSH7300-DSH7322, DSH7325-DSH7347,
// DSH7350-DSH7357, DSH7359, DSH7360 (the rules V1-V13) and DSH7361, DSH7362, DSH7364-DSH7370 (inline HLSL). DSH4409 is
// not used: a pass's `.usf` may live anywhere (the engine compiles a snapshot of it under /DreamPassUser), so "not under a
// mapped directory" is no error.
//
// A fullscreen pass with `Shader =` always runs in the pixel slot (FDreamPassPS), whatever its output count, and a
// compute pass in the compute slot: both have the slot limits (8 inputs, 4 outputs, 16 parameter vectors). Only a
// fullscreen pass with `Material =` has the post-process material's 5 input slots (DreamShader_Plan/09, deviation table).
//
// A pass's HLSL may be in the `.dsp` itself (DreamShader_Plan/10): its own `hlsl { }` block, holding whole functions
// (Block; the entry is `Main` unless `Entry` names another) or the statements of the entry (Body; the compiler writes the
// function around them); or `Entry = X;` alone, X a function of the file's one `hlsl { }` block (Shared). The blocks are
// read with the character scanner of Lang/LangHlslText.h -- which form a block is, its functions, their `[numthreads]` --
// and what would break a slot the compiler builds from them is refused here: the shared code of the file's block naming
// what a pass's slot #defines (it is compiled into every inline pass's slot), an entry called from anywhere (it is in its
// own passes' slots only), a binding taking a name of the body form. The rest of the text is the shader compiler's, at the
// pre-check, which reports into the `.dsp`.

#include "LangBinderInternal.h"

#include "Lang/LangPipelineInternal.h"
#include "Lang/LangPipelineSource.h"
#include "Lang/LangHlslText.h"

#include "IR/IR.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/CString.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "DreamShader.Binder.Pipeline"

namespace UE::DreamShader::Lang
{
	// ---------------------------------------------------------------------------------------------
	// LangBound.h and LangPipelineSource.h: the members that need a translation unit
	// ---------------------------------------------------------------------------------------------

	int32 FBoundPipeline::FindBuffer(const FString& Name) const
	{
		return Payload.Buffers.IndexOfByPredicate([&Name](const IR::FIRPassBuffer& Buffer) { return Buffer.Name.Equals(Name, ESearchCase::CaseSensitive); });
	}

	int32 FBoundPipeline::FindPass(const FString& Name) const
	{
		return Payload.Passes.IndexOfByPredicate([&Name](const IR::FIRPass& Pass) { return Pass.Name.Equals(Name, ESearchCase::CaseSensitive); });
	}

	int32 FBoundPipeline::FindParameter(const FString& Name) const
	{
		return Payload.Parameters.IndexOfByPredicate([&Name](const IR::FIRPassParameter& Parameter) { return Parameter.Name.Equals(Name, ESearchCase::CaseSensitive); });
	}

	// The `.dsp` spellings, in the order of the runtime's enums. DreamShaderPass keeps the same two tables
	// (Private/DreamPassTypes.cpp); DreamShader.Pass.Logic.InjectionNames fails when they drift apart.
	TConstArrayView<const TCHAR*> GetDreamShaderPassInjectionNames()
	{
		return MakeArrayView(Private::PipelineVocabulary::InjectionNames);
	}

	TConstArrayView<const TCHAR*> GetDreamShaderPassFormatNames()
	{
		return MakeArrayView(Private::PipelineVocabulary::FormatNames);
	}
}

namespace UE::DreamShader::Lang::Private
{
	// A named namespace, not an anonymous one: the module builds as one unity blob.
	namespace LangBinderPipelinePrivate
	{
		using namespace PipelineVocabulary;

		// ----------------------------------------------------------------------------- tree reading

		static const FExpr* PipelineUnparen(const FExpr* Expr)
		{
			while (Expr)
			{
				const FParenExpr* Paren = Expr->As<FParenExpr>();
				if (!Paren)
				{
					break;
				}
				Expr = Paren->Inner.Get();
			}
			return Expr;
		}

		/** `Name` or `A.B.C` written with identifiers only: the dotted text; false for anything else. */
		static bool FlattenPipelineName(const FExpr* Expr, FString& OutName)
		{
			Expr = PipelineUnparen(Expr);
			if (!Expr)
			{
				return false;
			}
			if (const FIdentifierExpr* Identifier = Expr->As<FIdentifierExpr>())
			{
				OutName = Identifier->Name;
				return true;
			}
			if (const FMemberExpr* Member = Expr->As<FMemberExpr>())
			{
				FString Object;
				if (!FlattenPipelineName(Member->Object.Get(), Object))
				{
					return false;
				}
				OutName = Object + TEXT(".") + Member->Member;
				return true;
			}
			return false;
		}

		/** One bare word: an identifier, through parentheses. */
		static bool ReadPipelineWord(const FExpr* Expr, FString& OutWord)
		{
			Expr = PipelineUnparen(Expr);
			const FIdentifierExpr* Identifier = Expr ? Expr->As<FIdentifierExpr>() : nullptr;
			if (!Identifier)
			{
				return false;
			}
			OutWord = Identifier->Name;
			return true;
		}

		static bool ReadPipelineString(const FExpr* Expr, FString& OutText)
		{
			Expr = PipelineUnparen(Expr);
			const FLiteralExpr* Literal = Expr ? Expr->As<FLiteralExpr>() : nullptr;
			if (!Literal || Literal->LiteralKind != ELiteralKind::String)
			{
				return false;
			}
			OutText = Literal->Text;
			return true;
		}

		/** `Name(arguments)` with an identifier callee: the name and the call. */
		static const FCallExpr* ReadPipelineCall(const FExpr* Expr, FString& OutCallee)
		{
			Expr = PipelineUnparen(Expr);
			const FCallExpr* Call = Expr ? Expr->As<FCallExpr>() : nullptr;
			const FIdentifierExpr* Callee = (Call && Call->Callee) ? Call->Callee->As<FIdentifierExpr>() : nullptr;
			if (!Callee)
			{
				return nullptr;
			}
			OutCallee = Callee->Name;
			return Call;
		}

		/** `a | b | c` over leaves an identifier (or, with bAllowStrings, a string): the leaves in order. */
		static bool CollectBarWords(const FExpr* Expr, const bool bAllowStrings, TArray<FString>& OutWords, TArray<FLangSpan>& OutSpans)
		{
			Expr = PipelineUnparen(Expr);
			if (!Expr)
			{
				return false;
			}
			if (const FBinaryExpr* Binary = Expr->As<FBinaryExpr>())
			{
				return Binary->Op == EBinaryOp::BitwiseOr
					&& CollectBarWords(Binary->Left.Get(), bAllowStrings, OutWords, OutSpans)
					&& CollectBarWords(Binary->Right.Get(), bAllowStrings, OutWords, OutSpans);
			}
			FString Word;
			if (ReadPipelineWord(Expr, Word) || (bAllowStrings && ReadPipelineString(Expr, Word)))
			{
				OutWords.Add(Word);
				OutSpans.Add(Expr->Span);
				return true;
			}
			return false;
		}

		/** `Game | Editor` as the pragma keeps it: words split at the bars, blanks trimmed. */
		static void SplitPragmaBarList(const FString& Value, TArray<FString>& OutWords)
		{
			TArray<FString> Parts;
			Value.ParseIntoArray(Parts, TEXT("|"), /* bCullEmpty */ false);
			for (const FString& Part : Parts)
			{
				OutWords.Add(Part.TrimStartAndEnd());
			}
		}

		/** The ConstantType spelling of a folded value: int, bool, float, float2..float4. */
		static FString PipelineConstantTypeSpelling(const IR::FIRType& Type)
		{
			if (Type.Rows <= 1 && Type.IsIntegral())
			{
				return TEXT("int");
			}
			if (Type.Rows <= 1 && Type.IsBool())
			{
				return TEXT("bool");
			}
			const int32 Width = FMath::Clamp(Type.Rows, 1, 4);
			return Width == 1 ? FString(TEXT("float")) : FString::Printf(TEXT("float%d"), Width);
		}

		/** What a pipeline `uniform` may be: float..float4, int, bool, Texture2D. */
		static bool IsPipelineParameterType(const IR::FIRType& Type)
		{
			if (Type.IsTexture())
			{
				return Type.Texture == ETextureKind::Texture2D;
			}
			if (Type.Cols != 1)
			{
				return false;
			}
			switch (Type.Kind)
			{
			case IR::EIRTypeKind::Float: return Type.Rows >= 1 && Type.Rows <= 4;
			case IR::EIRTypeKind::Int:
			case IR::EIRTypeKind::Bool:  return Type.Rows == 1;
			default:                     return false;
			}
		}

		/** What a pipeline `static const` may be: a number or a bool, one to four components. */
		static bool IsPipelineConstantType(const IR::FIRType& Type)
		{
			return Type.IsNumeric() && Type.Cols == 1 && Type.Rows >= 1 && Type.Rows <= 4;
		}

		static bool IsEnabledLiteral(const FExpr* Expr, bool& bOutValue)
		{
			Expr = PipelineUnparen(Expr);
			const FLiteralExpr* Literal = Expr ? Expr->As<FLiteralExpr>() : nullptr;
			if (!Literal || Literal->LiteralKind != ELiteralKind::Bool)
			{
				return false;
			}
			bOutValue = Literal->bBool;
			return true;
		}

		/** `PostProcessInputsUsed`: how many of the post-process input slots the material's own SceneTexture nodes take. */
		static int32 CountUsedInputSlots(const uint32 Mask)
		{
			int32 Count = 0;
			for (int32 Bit = 0; Bit < MaterialInputSlots; ++Bit)
			{
				Count += (Mask & (1u << Bit)) != 0 ? 1 : 0;
			}
			return Count;
		}

		// ------------------------------------------------------------------------------ inline HLSL text

		/** Offset into the text of an `hlsl` block as a span of the `.dsp`: the text starts right after the block's `{`. */
		static FLangSpan SpanInHlslBlock(const FLangSpan& BodySpan, const FString& RawBody, const int32 Offset, const int32 Length)
		{
			FLangSpan Span;
			Span.Offset = BodySpan.Offset + 1 + Offset;
			Span.Length = FMath::Max(Length, 1);
			Span.Line = BodySpan.Line;
			Span.Column = BodySpan.Column + 1;
			const int32 Last = FMath::Clamp(Offset, 0, RawBody.Len());
			for (int32 Index = 0; Index < Last; ++Index)
			{
				if (RawBody[Index] == TCHAR('\n'))
				{
					++Span.Line;
					Span.Column = 1;
				}
				else
				{
					++Span.Column;
				}
			}
			return Span;
		}

		/** A 1-based line of an `hlsl` block's text as a span of the `.dsp`: its first character that is not blank, to its end. */
		static FLangSpan SpanOfHlslBlockLine(const FLangSpan& BodySpan, const FString& RawBody, const int32 Line)
		{
			int32 Start = 0;
			int32 Current = 1;
			while (Current < Line && Start < RawBody.Len())
			{
				if (RawBody[Start++] == TCHAR('\n'))
				{
					++Current;
				}
			}
			while (Start < RawBody.Len() && (RawBody[Start] == TCHAR(' ') || RawBody[Start] == TCHAR('\t')))
			{
				++Start;
			}
			int32 End = Start;
			while (End < RawBody.Len() && RawBody[End] != TCHAR('\n') && RawBody[End] != TCHAR('\r'))
			{
				++End;
			}
			return SpanInHlslBlock(BodySpan, RawBody, Start, End - Start);
		}

		/**
		 * One name the slot's registry #defines for a pass (DreamShaderCompiler, Pass/DreamShaderPassSlotRegistry.cpp,
		 * BuildSlotSection): a read as itself, <Name>Size and <Name>UVRect, a compute write as itself and <Name>Size, a pixel
		 * write as <Name>Size, a param as itself, and the Entry, renamed to the slot's entry point.
		 */
		struct FSlotDefinedName
		{
			FString Name;
			/** What the pass wrote that defines it: `read Source`, `param Radius`, empty for the Entry. */
			FString Binding;
		};

		static void CollectSlotDefinedNames(const IR::FIRPass& Pass, TArray<FSlotDefinedName>& OutNames)
		{
			const bool bCompute = Pass.Kind.Equals(PassKinds[Kind::Compute], ESearchCase::CaseSensitive);
			for (const IR::FIRPassBinding& Binding : Pass.Reads)
			{
				const FString Written = FString::Printf(TEXT("read %s"), *Binding.Slot);
				OutNames.Add({ Binding.Slot, Written });
				OutNames.Add({ Binding.Slot + TEXT("Size"), Written });
				OutNames.Add({ Binding.Slot + TEXT("UVRect"), Written });
			}
			for (const IR::FIRPassBinding& Binding : Pass.Writes)
			{
				const FString Written = FString::Printf(TEXT("write %s"), *Binding.Slot);
				if (bCompute)
				{
					OutNames.Add({ Binding.Slot, Written });
				}
				OutNames.Add({ Binding.Slot + TEXT("Size"), Written });
			}
			for (const IR::FIRPassParam& Param : Pass.Params)
			{
				OutNames.Add({ Param.Target, FString::Printf(TEXT("param %s"), *Param.Target) });
			}
			if (!Pass.Entry.IsEmpty())
			{
				OutNames.Add({ Pass.Entry, FString() });
			}
		}

		static bool ContainsExactly(const TArray<FString>& Names, const FString& Name)
		{
			return Names.ContainsByPredicate([&Name](const FString& Candidate) { return Candidate.Equals(Name, ESearchCase::CaseSensitive); });
		}

		/** Where a value of one pass is: its source, scaled and offset (or the plain constant). */
		struct FPipelineParamForm
		{
			bool bConstant = false;
			double Value[4] = { 0.0, 0.0, 0.0, 0.0 };
			int32 Components = 1;
			FString ConstantType;
			/** Parameter or Weight, for a non-constant form. */
			FString SourceKind;
			FString Parameter;
			/** The parameter's type spelling (FIRPassParameter::Type); `float` for the weight. */
			FString ParameterType;
			double Multiplier = 1.0;
			double Offset = 0.0;
		};

		/** The keys one pass wrote, with the statement that wrote each: spans for the rules that run after the block. */
		struct FPipelinePassKeys
		{
			TArray<TPair<FString, const FPassStmt*>> Written;
			/** The pass's `hlsl { }` block; null without one. */
			const FPassStmt* Hlsl = nullptr;

			const FPassStmt* Find(const TCHAR* Key) const
			{
				for (const TPair<FString, const FPassStmt*>& Entry : Written)
				{
					if (Entry.Key.Equals(Key, ESearchCase::CaseSensitive))
					{
						return Entry.Value;
					}
				}
				return nullptr;
			}

			FLangSpan SpanOf(const TCHAR* Key, const FLangSpan& Fallback) const
			{
				const FPassStmt* Statement = Find(Key);
				return Statement ? Statement->Span : Fallback;
			}
		};

		/** A texture's size, as far as the pipeline can tell it: what V7 and V9 compare. */
		struct FPipelineSizeClass
		{
			FString Resolution;
			double Scale = 1.0;
			int32 Width = 0;
			int32 Height = 0;

			bool operator==(const FPipelineSizeClass& Other) const
			{
				if (!Resolution.Equals(Other.Resolution, ESearchCase::CaseSensitive))
				{
					return false;
				}
				if (Resolution.Equals(ResolutionFixed, ESearchCase::CaseSensitive))
				{
					return Width == Other.Width && Height == Other.Height;
				}
				return SameFloat(Scale, Other.Scale);
			}

			/**
			 * Whether a texture of this size is never smaller than one of Other's. Only sizes of one kind compare before the
			 * frame -- two fixed sizes, two scales of one resolution; Render against Output, or a fixed size against one that
			 * follows the view, depends on the view and does not count.
			 */
			bool Covers(const FPipelineSizeClass& Other) const
			{
				if (!Resolution.Equals(Other.Resolution, ESearchCase::CaseSensitive))
				{
					return false;
				}
				if (Resolution.Equals(ResolutionFixed, ESearchCase::CaseSensitive))
				{
					return Width >= Other.Width && Height >= Other.Height;
				}
				return SameFloat(Scale, Other.Scale) || Scale > Other.Scale;
			}

			FString Describe() const
			{
				if (Resolution.Equals(ResolutionFixed, ESearchCase::CaseSensitive))
				{
					return FString::Printf(TEXT("%d x %d"), Width, Height);
				}
				return SameFloat(Scale, 1.0) ? Resolution : FString::Printf(TEXT("%s x %s"), *Resolution, *FString::SanitizeFloat(Scale));
			}
		};

		enum class EPipelineValueSite : uint8
		{
			Pragma,
			Buffer,
			Pass,
		};

		/**
		 * The binder of one `.dsp`. A class of its own rather than more members of FLangBinder: it reads the binder's
		 * public state and calls its expression binding, and keeps the pipeline's bookkeeping to itself.
		 */
		class FPipelineBinder
		{
		public:
			explicit FPipelineBinder(FLangBinder& InBinder)
				: Binder(InBinder)
				, Bound(InBinder.GetBound())
				, Diagnostics(InBinder.GetDiagnostics())
				, Root(InBinder.GetRootModule())
				, File(InBinder.GetRootModule().FilePath)
				, Pipeline(InBinder.GetBound().Pipeline)
				, Payload(InBinder.GetBound().Pipeline.Payload)
				, References(InBinder.GetOptions().PipelineReferences)
			{
			}

			void Run();

		private:
			// -- declarations
			void DeclarePragma(const FPragmaDecl& Pragma);
			void DeclareVariable(const FVariableDecl& Decl);
			void ReportForeignDeclaration(const FDecl& Decl);
			/**
			 * DSH3317 for every `///` directive outside Allowed. After BindDirectives (a uniform, a constant), only for the
			 * ones it takes in silence there and a pipeline has no use for, and for keys the language does not define.
			 */
			void ReportDirectivesWithoutEffect(const FDocBlock& Doc, TConstArrayView<const TCHAR*> Allowed, const FText& Where, bool bAfterBindDirectives);
			FString DescriptionOf(const FDocBlock& Doc, const FText& Where);

			// -- the payload
			void BuildParameters();
			void BindPragma();
			void BindBuffers();
			void BindBuffer(const FBufferDecl& Decl);
			void BindPasses();
			void BindPass(const FPassDecl& Decl, int32 KindIndex);
			void BindPassSetting(IR::FIRPass& Pass, int32 KindIndex, const FPassStmt& Statement, FPipelinePassKeys& Keys);
			void BindPassBinding(IR::FIRPass& Pass, const FPassStmt& Statement);
			void BindPassParam(IR::FIRPass& Pass, const FPassStmt& Statement);
			bool BuildFilter(const FExpr* Expr, TArray<IR::FIRPassFilterClause>& OutClauses);
			bool BuildFilterTerm(const FExpr* Expr, IR::FIRPassFilterTerm& OutTerm);
			void FinishPass(IR::FIRPass& Pass, int32 KindIndex, const FPassDecl& Decl, const FPipelinePassKeys& Keys);
			void CheckPassAgainstReferences(IR::FIRPass& Pass, int32 KindIndex, const FPassDecl& Decl, const FPipelinePassKeys& Keys);
			void ApplyBufferDefaults();

			// -- inline HLSL (DreamShader_Plan/10)
			void DeclareHlslBlock(const FHlslBlockDecl& Decl);
			void BindPassHlsl(const IR::FIRPass& Pass, const FPassStmt& Statement, FPipelinePassKeys& Keys);
			/** A fullscreen pass without a material, or a compute pass: where its code is (FIRPass::HlslSource), and that it is there. */
			void ResolveHlslSource(IR::FIRPass& Pass, int32 KindIndex, const FPipelinePassKeys& Keys, const FLangSpan& NameSpan);
			/** A Block or Shared pass's Entry in the scan of its block: defined there, and of the kind and group size the pass runs. */
			void CheckInlineEntry(IR::FIRPass& Pass, int32 KindIndex, const FHlslTextScan& Scan, bool bShared, bool bEntryWritten, const FPipelinePassKeys& Keys, const FLangSpan& NameSpan);
			/** After every pass: the shared code of the file's block against every inline pass's slot, and the calls of entries. */
			void CheckSharedHlsl();

			// -- across passes
			void CheckFrame();
			void CheckPassAtInjection(const IR::FIRPass& Pass, int32 PassIndex, int32 KindIndex, int32 InjectionIndex);
			void CheckBuiltinBinding(const IR::FIRPass& Pass, const IR::FIRPassBinding& Binding, bool bWrite, int32 InjectionIndex);
			void CheckBufferLifetimes();
			void BuildProduct();

			// -- values
			enum class EFold : uint8
			{
				Value,
				NotConstant,
				Reported,
			};
			EFold FoldConstant(const FExpr& Expr, double OutValue[4], int32& OutComponents, IR::FIRType& OutType);
			void ReportBadValue(EPipelineValueSite Site, const FLangSpan& Span, const FText& Message);
			bool ReadNumber(const FExpr* Expr, EPipelineValueSite Site, const FString& Key, bool bIntegral, double& OutValue);
			bool ReadBool(const FExpr* Expr, EPipelineValueSite Site, const FString& Key, bool& bOutValue);
			bool ReadVector(const FExpr* Expr, EPipelineValueSite Site, const FString& Key, int32 MinComponents, int32 MaxComponents, bool bIntegral, double OutValue[4], int32& OutComponents);
			/** A colour-like value: a number broadcast to four channels, or a vector padded with zeros. */
			bool ReadColor(const FExpr* Expr, EPipelineValueSite Site, const FString& Key, double OutValue[4]);
			bool ReadKeyWord(const FExpr* Expr, EPipelineValueSite Site, const FString& Key, TConstArrayView<const TCHAR*> Allowed, FString& OutWord);
			bool ReadInjectionName(const FExpr* Expr, const FLangSpan& Span, FString& OutName);
			bool ResolveInjectionText(const FString& Text, const FLangSpan& Span, FString& OutName);
			bool ResolveEnabled(const FString& Name, const FLangSpan& Span, FString& OutParameter);
			bool FoldParam(const FExpr* Expr, FPipelineParamForm& OutForm);
			bool MentionsRuntimeValue(const FExpr* Expr) const;
			bool CheckParamNames(const FExpr* Expr, bool bCallee);

			// -- lookups
			bool IsKnownBuffer(const FString& Name) const;
			const IR::FIRPassBuffer* FindPayloadBuffer(const FString& Name) const;
			int32 FindParameterGlobal(const FString& Name) const;
			FPipelineSizeClass SizeClassOf(const FString& Buffer, int32 InjectionIndex) const;
			void ReportUnknownBuffer(const FString& Name, const FLangSpan& Span);

			FLangBinder& Binder;
			FBoundModule& Bound;
			FLangDiagnosticSink& Diagnostics;
			const FModule& Root;
			const FString File;
			FBoundPipeline& Pipeline;
			IR::FIRPassPipeline& Payload;
			const FPipelineReferences* References;

			TArray<const FBufferDecl*> BufferDecls;
			TArray<const FPassDecl*> PassDecls;
			/** Parallel to Payload.Passes. */
			TArray<FPipelinePassKeys> PassKeys;
			TArray<int32> PassKindIndices;

			/** The file's `hlsl { }` block, and the scan of its text; null and empty without one. */
			const FHlslBlockDecl* SharedHlslDecl = nullptr;
			FHlslTextScan SharedScan;
		};

		// ------------------------------------------------------------------------------------- run

		void FPipelineBinder::Run()
		{
			Pipeline.bIsPipeline = true;
			Pipeline.References = References;
			Binder.CurrentFile = File;

			for (const FDeclPtr& DeclPtr : Root.Declarations)
			{
				const FDecl* Decl = DeclPtr.Get();
				if (!Decl)
				{
					continue;
				}

				// Keyed like the declare pass keys every file-scope declaration; a `.dsp` has no regions.
				Bound.StatementRegions.Add(Decl, INDEX_NONE);

				switch (Decl->Kind)
				{
				case ENodeKind::PragmaDecl:
					DeclarePragma(*static_cast<const FPragmaDecl*>(Decl));
					break;
				case ENodeKind::VariableDecl:
					DeclareVariable(*static_cast<const FVariableDecl*>(Decl));
					break;
				case ENodeKind::BufferDecl:
					BufferDecls.Add(static_cast<const FBufferDecl*>(Decl));
					break;
				case ENodeKind::PassDecl:
					PassDecls.Add(static_cast<const FPassDecl*>(Decl));
					break;
				case ENodeKind::HlslBlockDecl:
					DeclareHlslBlock(*static_cast<const FHlslBlockDecl*>(Decl));
					break;
				case ENodeKind::FunctionDecl:
				case ENodeKind::StructDecl:
				case ENodeKind::IncludeDecl:
					ReportForeignDeclaration(*Decl);
					break;
				default:
					break;
				}
			}

			// Every initializer bound and folded, in declaration order, so a constant may read the one above it;
			// DSH7210 for a `static const` whose initializer is not a compile-time value.
			Binder.BindGlobals();
			Binder.CurrentFile = File;

			BuildParameters();
			// After the parameters: `Enabled = Name` names one.
			BindPragma();
			BindBuffers();
			BindPasses();
			// Every pass bound: the file block's shared code is compiled into the slot of each one whose HLSL is inline.
			CheckSharedHlsl();
			ApplyBufferDefaults();
			CheckFrame();
			CheckBufferLifetimes();

			if (!References)
			{
				Diagnostics.Info(
					TEXT("DSH7360"),
					File,
					Pipeline.Pragma ? Pipeline.Pragma->Span : FLangSpan(),
					LOCTEXT("PipelineNoReferences", "The materials, shader files and pass layers this pipeline names are not available here, so they were taken as written and the checks that need them were skipped; compile the pipeline in the editor to have them checked."));
			}
			else if (!References->bCustomPassAvailable)
			{
				Diagnostics.Info(
					TEXT("DSH7360"),
					File,
					Pipeline.Pragma ? Pipeline.Pragma->Span : FLangSpan(),
					LOCTEXT("PipelineNoCustomPassFacts", "This engine has no Custom Pass runtime (it needs Unreal Engine 5.8 or later), so what the passes' materials offer them -- UserSceneTexture inputs, UE.DreamPassOutput pins, usage flags, the pre-exposure and translator settings -- was not read and not checked; the pipeline is not built on this engine."));
			}

			BuildProduct();
		}

		// ------------------------------------------------------------------------------ declarations

		void FPipelineBinder::DeclarePragma(const FPragmaDecl& Pragma)
		{
			switch (Pragma.PragmaKind)
			{
			case EPragmaKind::Pipeline:
				if (Pipeline.Pragma)
				{
					Diagnostics.Error(
						TEXT("DSH3313"),
						File,
						Pragma.Span,
						FText::Format(
							LOCTEXT("PipelineSecondPragma", "'#pragma pipeline' is written a second time, and one '.dsp' is one pipeline; the line {0} already configures it."),
							FText::AsNumber(Pipeline.Pragma->Span.Line)));
					return;
				}
				Pipeline.Pragma = &Pragma;
				return;

			case EPragmaKind::Material:
				Diagnostics.Error(
					TEXT("DSH3314"),
					File,
					Pragma.Span,
					LOCTEXT("PipelineHoldsMaterialPragma", "'#pragma material' configures a material, and a '.dsp' is configured by '#pragma pipeline(...)'; the material a pass draws with is a '.dss' of its own."));
				return;

			case EPragmaKind::Instance:
				Binder.ReportInstancePragmaOutsideDsi(Pragma);
				return;

			case EPragmaKind::Layout:
			case EPragmaKind::Region:
			case EPragmaKind::EndRegion:
				Diagnostics.Warning(
					TEXT("DSH3315"),
					File,
					Pragma.Span,
					FText::Format(
						LOCTEXT("PipelineIgnoresGraphPragma", "'#pragma {0}' boxes or places graph nodes, and a '.dsp' has no graph; the line was ignored."),
						FText::FromString(Pragma.Name)));
				return;

			case EPragmaKind::Unknown:
			default:
				return;
			}
		}

		void FPipelineBinder::ReportForeignDeclaration(const FDecl& Decl)
		{
			if (const FFunctionDecl* Function = Decl.As<FFunctionDecl>())
			{
				Diagnostics.Error(
					TEXT("DSH3314"),
					File,
					Function->NameSpan,
					FText::Format(
						LOCTEXT("PipelineHoldsFunction", "A '.dsp' holds '#pragma pipeline', 'uniform', 'static const', 'buffer' and 'pass' declarations and an 'hlsl' block, and '{0}' is a function outside it; an HLSL function goes in an 'hlsl { }' block, the file's or a pass's, and a DreamShaderLang one in a '.dss' or a '.dsh'."),
						FText::FromString(Function->Name)));
			}
			else if (const FStructDecl* Struct = Decl.As<FStructDecl>())
			{
				Diagnostics.Error(
					TEXT("DSH3314"),
					File,
					Struct->NameSpan,
					FText::Format(
						LOCTEXT("PipelineHoldsStruct", "A '.dsp' holds '#pragma pipeline', 'uniform', 'static const', 'buffer' and 'pass' declarations and an 'hlsl' block, and 'struct {0}' declares a type outside it; an HLSL struct goes in an 'hlsl { }' block, and a DreamShaderLang one in a '.dsh'."),
						FText::FromString(Struct->Name)));
			}
			else if (const FIncludeDecl* Include = Decl.As<FIncludeDecl>())
			{
				Diagnostics.Error(
					TEXT("DSH3314"),
					File,
					Include->PathSpan,
					FText::Format(
						LOCTEXT("PipelineHoldsInclude", "A '.dsp' includes no DreamShaderLang, and has no code that could use '{0}'; remove the include. A shader file the HLSL of a pass needs is included inside its 'hlsl { }' block."),
						FText::FromString(Include->Path)));
			}
		}

		void FPipelineBinder::ReportDirectivesWithoutEffect(const FDocBlock& Doc, TConstArrayView<const TCHAR*> Allowed, const FText& Where, const bool bAfterBindDirectives)
		{
			// Every key the language defines; after BindDirectives the ones it judges for the target are its business.
			static const TCHAR* const LanguageKeys[] =
			{
				Directive::Group, Directive::Desc, Directive::Slider, Directive::Sort, Directive::Name, Directive::Sampler,
				Directive::Default, Directive::Static, Directive::Library, Directive::Param, Directive::Asset, Directive::Custom,
				Directive::Layer, Directive::LayerBlend, Directive::Page, Directive::Pin, Directive::Root,
			};
			// What BindDirectives takes in silence on a variable and a pipeline has no use for.
			static const TCHAR* const SilentOnVariables[] = { Directive::Name, Directive::Sampler, Directive::Static, Directive::Page };

			for (const FDocDirective& Entry : Doc.Directives)
			{
				if (HasName(Allowed, Entry.Key))
				{
					continue;
				}
				if (bAfterBindDirectives && HasName(LanguageKeys, Entry.Key) && !HasName(SilentOnVariables, Entry.Key))
				{
					continue;
				}
				Diagnostics.Warning(
					TEXT("DSH3317"),
					File,
					Entry.Span,
					FText::Format(
						LOCTEXT("PipelineDirectiveNoEffect", "'@{0}' has no effect on {1} in a '.dsp'; remove it."),
						FText::FromString(Entry.Key),
						Where));
			}
		}

		FString FPipelineBinder::DescriptionOf(const FDocBlock& Doc, const FText& Where)
		{
			static const TCHAR* const Allowed[] = { Directive::Desc };
			ReportDirectivesWithoutEffect(Doc, MakeArrayView(Allowed), Where, /* bAfterBindDirectives */ false);
			if (const FDocDirective* Desc = Doc.Find(Directive::Desc))
			{
				const FString Value = Desc->Value.TrimStartAndEnd();
				if (!Value.IsEmpty())
				{
					return Value;
				}
			}
			return FString::Join(Doc.FreeText, TEXT("\n")).TrimStartAndEnd();
		}

		void FPipelineBinder::DeclareVariable(const FVariableDecl& Decl)
		{
			const FDeclarator& Declarator = Decl.Declarator;
			const bool bUniform = Decl.Storage == EStorageClass::Uniform;
			const bool bConstant = Decl.Storage == EStorageClass::StaticConst || Decl.Storage == EStorageClass::Const;
			if (!bUniform && !bConstant)
			{
				Diagnostics.Error(
					TEXT("DSH3316"),
					File,
					Declarator.NameSpan,
					FText::Format(
						LOCTEXT("PipelineVariableStorage", "'{0}' is a file-scope variable of a '.dsp', which is a 'uniform' (a parameter an activation may override) or a 'static const' (a compile-time value)."),
						FText::FromString(Declarator.Name)));
				return;
			}

			if (Declarator.Name.Equals(WeightParameterName, ESearchCase::CaseSensitive))
			{
				Diagnostics.Error(
					TEXT("DSH4414"),
					File,
					Declarator.NameSpan,
					LOCTEXT("PipelineWeightDeclared", "'DreamPassWeight' is the pipeline's weight in a view, which every pass can read as 'param P = DreamPassWeight'; it cannot be declared."));
				return;
			}

			if (!Binder.CheckNameAvailable(Declarator.Name, Declarator.NameSpan, File))
			{
				return;
			}
			// A `uniform` becomes a parameter of the asset, named by an FName, which compares ignoring case
			// (UDreamPassPipeline::FindParameter, Validate): two that differ in case only would be one parameter there. A
			// `static const` is folded away and never reaches the asset, so only two uniforms can clash this way.
			if (bUniform)
			{
				const FBoundGlobal* SameName = Bound.Globals.FindByPredicate([&Declarator](const FBoundGlobal& Other)
				{
					return Other.bIsParameter && Other.Name.Equals(Declarator.Name, ESearchCase::IgnoreCase);
				});
				if (SameName)
				{
					Diagnostics.Error(
						TEXT("DSH4210"),
						File,
						Declarator.NameSpan,
						FText::Format(
							LOCTEXT("ParameterNameCaseClash", "The parameter '{0}' differs from '{1}', declared on line {2}, in case only; the pipeline asset keeps parameter names as Unreal names, which compare ignoring case, so the two would be one parameter there. Rename one."),
							FText::FromString(Declarator.Name),
							FText::FromString(SameName->Name),
							FText::AsNumber(SameName->Decl ? SameName->Decl->Span.Line : 0)));
					return;
				}
			}

			FBoundGlobal Global;
			Global.Name = Declarator.Name;
			Global.Decl = &Decl;
			Global.Storage = Decl.Storage;
			Global.File = File;
			Global.bIsParameter = bUniform;
			Global.bIsConstant = bConstant;
			if (!Binder.ResolveTypeRef(Decl.Type, Global.Type))
			{
				Global.Type = IR::FIRType::Error();
			}

			// The directives a pipeline parameter carries (05 s2): the rest are a `.dss` parameter's, and say nothing here.
			Global.Directives = Binder.BindDirectives(Decl.Doc, bUniform ? EDirectiveTarget::Uniform : EDirectiveTarget::Constant, &Decl.Type);
			static const TCHAR* const Allowed[] = { Directive::Group, Directive::Desc, Directive::Slider, Directive::Sort, Directive::Default };
			ReportDirectivesWithoutEffect(
				Decl.Doc,
				MakeArrayView(Allowed),
				bUniform ? LOCTEXT("WherePipelineParameter", "a pipeline parameter") : LOCTEXT("WherePipelineConstant", "a constant"),
				/* bAfterBindDirectives */ true);

			Binder.ResolveArrayCount(Declarator.ArrayDimensions, Declarator.Initializer.Get(), Declarator.Span, Global.ArrayCount);
			if (Global.ArrayCount > 0)
			{
				Diagnostics.Error(
					TEXT("DSH7320"),
					File,
					Declarator.Span,
					FText::Format(
						LOCTEXT("PipelineArray", "'{0}' is an array, and a pipeline has no array parameters or constants; declare one per element."),
						FText::FromString(Declarator.Name)));
			}
			else if (!Global.Type.IsError())
			{
				if (bUniform && !IsPipelineParameterType(Global.Type))
				{
					Diagnostics.Error(
						TEXT("DSH7320"),
						File,
						Decl.Type.Span,
						FText::Format(
							LOCTEXT("PipelineParameterType", "'{0}' is declared '{1}', and a pipeline parameter is a float, float2, float3, float4, int, bool or Texture2D."),
							FText::FromString(Declarator.Name),
							FText::FromString(Decl.Type.Name)));
				}
				else if (bConstant && !IsPipelineConstantType(Global.Type))
				{
					Diagnostics.Error(
						TEXT("DSH7320"),
						File,
						Decl.Type.Span,
						FText::Format(
							LOCTEXT("PipelineConstantType", "'{0}' is declared '{1}', and a pipeline constant is a number or a bool of one to four components."),
							FText::FromString(Declarator.Name),
							FText::FromString(Decl.Type.Name)));
				}
			}

			if (bUniform && Global.Type.IsTexture() && Declarator.Initializer)
			{
				Diagnostics.Error(
					TEXT("DSH7320"),
					File,
					Declarator.Initializer->Span,
					FText::Format(
						LOCTEXT("PipelineTextureInitializer", "'{0}' is a texture parameter, which takes an asset rather than a value; write '/// @default /Game/...' above it instead of an initializer."),
						FText::FromString(Declarator.Name)));
			}
			if (bConstant && !Declarator.Initializer)
			{
				Diagnostics.Error(
					TEXT("DSH7320"),
					File,
					Declarator.NameSpan,
					FText::Format(
						LOCTEXT("PipelineConstantNoInitializer", "'{0}' is a compile-time constant and must be initialised where it is declared."),
						FText::FromString(Declarator.Name)));
			}

			Bound.Globals.Add(MoveTemp(Global));
			Binder.GlobalArrayValues.AddDefaulted();
		}

		// -------------------------------------------------------------------------------- parameters

		void FPipelineBinder::BuildParameters()
		{
			for (int32 GlobalIndex = 0; GlobalIndex < Bound.Globals.Num(); ++GlobalIndex)
			{
				const FBoundGlobal& Global = Bound.Globals[GlobalIndex];
				if (!Global.bIsParameter || !Global.Decl || Global.ArrayCount > 0 || !IsPipelineParameterType(Global.Type))
				{
					continue;
				}

				IR::FIRPassParameter Parameter;
				Parameter.Name = Global.Name;
				Parameter.Type = Global.Type.ToString();
				Parameter.Group = Global.Directives.Group;
				Parameter.Description = Global.Directives.Desc;
				Parameter.bHasSlider = Global.Directives.bHasSlider;
				Parameter.SliderMin = Global.Directives.bHasSlider ? Global.Directives.SliderMin : 0.0;
				Parameter.SliderMax = Global.Directives.bHasSlider ? Global.Directives.SliderMax : 1.0;
				Parameter.SortPriority = Global.Directives.bHasSort ? Global.Directives.Sort : 0;
				Parameter.Source.File = File;
				Parameter.Source.Span = Global.Decl->Span;

				if (Global.Type.IsTexture())
				{
					// `/// @default None`, or no `@default`: no texture.
					const FString Asset = Global.Directives.DefaultAsset.TrimStartAndEnd();
					Parameter.Default = IR::FIRPropertyValue::MakeObject(Asset.Equals(TEXT("None"), ESearchCase::IgnoreCase) ? FString() : Asset);
				}
				else
				{
					double Value[4] = { 0.0, 0.0, 0.0, 0.0 };
					if (const FExpr* Initializer = Global.Decl->Declarator.Initializer.Get())
					{
						int32 Components = 1;
						if (!Binder.TypeOf(*Initializer).IsError() && !Binder.GetConstant(*Initializer, Value, Components))
						{
							Diagnostics.Error(
								TEXT("DSH7320"),
								File,
								Initializer->Span,
								FText::Format(
									LOCTEXT("PipelineDefaultNotConstant", "The default of '{0}' is written into the pipeline asset, so it has to be a value the compiler can fold: a literal, a 'static const', or arithmetic over those."),
									FText::FromString(Global.Name)));
						}
					}

					if (Global.Type.Kind == IR::EIRTypeKind::Int)
					{
						Parameter.Default = IR::FIRPropertyValue::MakeInt(static_cast<int64>(FMath::RoundToDouble(Value[0])));
					}
					else if (Global.Type.Kind == IR::EIRTypeKind::Bool)
					{
						Parameter.Default = IR::FIRPropertyValue::MakeBool(Value[0] != 0.0);
					}
					else
					{
						Parameter.Default = IR::FIRPropertyValue::MakeFloat4(Value, FMath::Clamp(Global.Type.Rows, 1, 4));
					}
				}

				Payload.Parameters.Add(MoveTemp(Parameter));
				Pipeline.ParameterGlobals.Add(GlobalIndex);
			}
		}

		int32 FPipelineBinder::FindParameterGlobal(const FString& Name) const
		{
			const int32 GlobalIndex = Bound.FindGlobal(Name);
			return (GlobalIndex != INDEX_NONE && Bound.Globals[GlobalIndex].bIsParameter) ? GlobalIndex : INDEX_NONE;
		}

		bool FPipelineBinder::ResolveEnabled(const FString& Name, const FLangSpan& Span, FString& OutParameter)
		{
			OutParameter.Reset();
			if (Name.Equals(TEXT("true"), ESearchCase::CaseSensitive))
			{
				return true;
			}

			bool bFolded = false;
			bool bValue = true;
			if (Name.Equals(TEXT("false"), ESearchCase::CaseSensitive))
			{
				bFolded = true;
				bValue = false;
			}
			else
			{
				const int32 GlobalIndex = Bound.FindGlobal(Name);
				if (GlobalIndex == INDEX_NONE)
				{
					TArray<FString> Candidates;
					for (const FBoundGlobal& Global : Bound.Globals)
					{
						Candidates.Add(Global.Name);
					}
					const FString Suggestion = FLangBinder::SuggestCaseInsensitive(Name, Candidates);
					Diagnostics.Error(
						TEXT("DSH4404"),
						File,
						Span,
						Suggestion.IsEmpty()
							? FText::Format(LOCTEXT("EnabledUnknown", "'{0}' is not a 'uniform' or 'static const' of this pipeline; 'Enabled' takes a 'uniform bool' or 'true'."), FText::FromString(Name))
							: FText::Format(LOCTEXT("EnabledUnknownDidYouMean", "'{0}' is not a 'uniform' or 'static const' of this pipeline; did you mean '{1}'? Names are case-sensitive."), FText::FromString(Name), FText::FromString(Suggestion)));
					return false;
				}

				const FBoundGlobal& Global = Bound.Globals[GlobalIndex];
				const bool bBool = Global.Type.IsBool() && Global.Type.Rows == 1 && Global.Type.Cols == 1;
				if (!bBool)
				{
					Diagnostics.Error(
						TEXT("DSH4405"),
						File,
						Span,
						FText::Format(
							LOCTEXT("EnabledNotBool", "'{0}' is a '{1}', and 'Enabled' takes a 'uniform bool' or 'true'."),
							FText::FromString(Name),
							FText::FromString(Global.Type.ToString())));
					return false;
				}
				if (Global.bIsParameter)
				{
					OutParameter = Name;
					return true;
				}

				// A `static const bool`: what it folds to.
				const FExpr* Initializer = Global.Decl ? Global.Decl->Declarator.Initializer.Get() : nullptr;
				double Value[4] = { 1.0, 0.0, 0.0, 0.0 };
				int32 Components = 1;
				if (Initializer && Binder.GetConstant(*Initializer, Value, Components))
				{
					bFolded = true;
					bValue = Value[0] != 0.0;
				}
			}

			if (bFolded && !bValue)
			{
				Diagnostics.Error(
					TEXT("DSH7322"),
					File,
					Span,
					LOCTEXT("EnabledFalse", "'Enabled' is false, so this would never run, and a pipeline asset has no switch that is always off; drive it with a 'uniform bool', or comment the declaration out."));
				return false;
			}
			return true;
		}

		// ------------------------------------------------------------------------------------ pragma

		void FPipelineBinder::BindPragma()
		{
			if (!Pipeline.Pragma)
			{
				return;
			}
			const FPragmaDecl& Pragma = *Pipeline.Pragma;

			// The `///` block above the pragma documents the file; no directive means anything there.
			ReportDirectivesWithoutEffect(Pragma.Doc, TConstArrayView<const TCHAR*>(), LOCTEXT("WherePipelinePragma", "'#pragma pipeline'"), /* bAfterBindDirectives */ false);

			TArray<const FPragmaArgument*> Seen;
			for (const FPragmaArgument& Argument : Pragma.Arguments)
			{
				if (Argument.Key.IsEmpty())
				{
					// The parser takes keys only for this pragma; a positional argument never gets here.
					continue;
				}

				const FPragmaArgument* const* First = Seen.FindByPredicate([&Argument](const FPragmaArgument* Earlier)
				{
					return Earlier->Key.Equals(Argument.Key, ESearchCase::CaseSensitive);
				});
				if (First)
				{
					Diagnostics.Error(
						TEXT("DSH7301"),
						File,
						Argument.Span,
						FText::Format(
							LOCTEXT("PipelinePragmaDuplicateKey", "'{0}' is set twice by '#pragma pipeline'; it was already set on line {1}."),
							FText::FromString(Argument.Key),
							FText::AsNumber((*First)->Span.Line)));
					continue;
				}
				Seen.Add(&Argument);

				const int32 KeyIndex = FindName(PragmaKeys, Argument.Key);
				if (KeyIndex == INDEX_NONE)
				{
					const FString Suggestion = FindNameIgnoringCase(PragmaKeys, Argument.Key);
					Diagnostics.Error(
						TEXT("DSH7300"),
						File,
						Argument.Span,
						Suggestion.IsEmpty()
							? FText::Format(LOCTEXT("PipelinePragmaUnknownKey", "'{0}' is not a key of '#pragma pipeline'; the keys are {1}."), FText::FromString(Argument.Key), FText::FromString(JoinNames(PragmaKeys)))
							: FText::Format(LOCTEXT("PipelinePragmaUnknownKeyDidYouMean", "'{0}' is not a key of '#pragma pipeline'; did you mean '{1}'? Keys are case-sensitive."), FText::FromString(Argument.Key), FText::FromString(Suggestion)));
					continue;
				}

				const FString Value = Argument.Value.TrimStartAndEnd();
				const FString Key = PragmaKeys[KeyIndex];

				if (Key.Equals(TEXT("Order"), ESearchCase::CaseSensitive))
				{
					FString Digits = Value;
					if (Digits.StartsWith(TEXT("-")) || Digits.StartsWith(TEXT("+")))
					{
						Digits.RightChopInline(1);
					}
					bool bInteger = !Argument.bQuoted && !Digits.IsEmpty() && Digits.Len() <= 9;
					for (const TCHAR Char : Digits)
					{
						bInteger &= Char >= TEXT('0') && Char <= TEXT('9');
					}
					if (!bInteger)
					{
						ReportBadValue(EPipelineValueSite::Pragma, Argument.Span, FText::Format(
							LOCTEXT("PipelineOrderNotInteger", "'Order' takes a whole number, the order among pipelines at one injection point (smaller first), and '{0}' is not one."),
							FText::FromString(Value)));
						continue;
					}
					Payload.Order = FCString::Atoi(*Value);
				}
				else if (Key.Equals(TEXT("Injection"), ESearchCase::CaseSensitive))
				{
					FString Name;
					if (ResolveInjectionText(Value, Argument.Span, Name))
					{
						Payload.DefaultInjection = Name;
					}
				}
				else if (Key.Equals(TEXT("Views"), ESearchCase::CaseSensitive) || Key.Equals(TEXT("Requires"), ESearchCase::CaseSensitive))
				{
					const bool bViews = Key.Equals(TEXT("Views"), ESearchCase::CaseSensitive);
					const TConstArrayView<const TCHAR*> Names = bViews ? MakeArrayView(ViewNames) : MakeArrayView(RequirementNames);

					// `Views = ""` is the one way to write no view at all: the parser reads an unquoted value only as a word. Said
					// before the split, which is no help here -- on 5.8 an empty string splits into one empty word (ParseTokens),
					// which would read as an unknown view.
					if (bViews && Value.IsEmpty())
					{
						ReportBadValue(EPipelineValueSite::Pragma, Argument.Span, LOCTEXT("PipelineNoViews", "'Views' names no view, so the pipeline would run nowhere."));
						continue;
					}
					// Any other quoted value is a string, not names, and is refused as a quoted 'Order' or 'Enabled' is: dropping it
					// would leave the pipeline on its default views or requirements in silence.
					if (Argument.bQuoted)
					{
						ReportBadValue(EPipelineValueSite::Pragma, Argument.Span, FText::Format(
							LOCTEXT("PipelineFlagsQuoted", "'{0}' takes names written without quotes and joined by '|', such as '{0} = {1}', and \"{2}\" is a quoted string."),
							FText::FromString(Key),
							FText::FromString(bViews ? TEXT("Game | SceneCapture") : TEXT("PostProcess | CustomStencil")),
							FText::FromString(Value)));
						continue;
					}

					TArray<FString> Words;
					SplitPragmaBarList(Value, Words);

					TArray<bool> Set;
					Set.Init(false, Names.Num());
					bool bAllKnown = true;
					for (const FString& Word : Words)
					{
						const int32 Index = FindName(Names, Word);
						if (Index == INDEX_NONE)
						{
							bAllKnown = false;
							const FString Suggestion = FindNameIgnoringCase(Names, Word);
							ReportBadValue(EPipelineValueSite::Pragma, Argument.Span, Suggestion.IsEmpty()
								? FText::Format(LOCTEXT("PipelineFlagUnknown", "'{0}' is not one of the '{1}' a pipeline knows: {2}."), FText::FromString(Word), FText::FromString(Key), FText::FromString(JoinNames(Names)))
								: FText::Format(LOCTEXT("PipelineFlagUnknownDidYouMean", "'{0}' is not one of the '{1}' a pipeline knows; did you mean '{2}'?"), FText::FromString(Word), FText::FromString(Key), FText::FromString(Suggestion)));
							continue;
						}
						Set[Index] = true;
					}
					if (!bAllKnown)
					{
						continue;
					}

					TArray<FString> Canonical;
					for (int32 Index = 0; Index < Names.Num(); ++Index)
					{
						if (Set[Index])
						{
							Canonical.Add(Names[Index]);
						}
					}
					if (bViews)
					{
						// Never empty here: an unquoted value is at least one word, and every word was a known one.
						// Game | Editor is the default, and the payload says so by saying nothing.
						const bool bDefault = Canonical.Num() == static_cast<int32>(UE_ARRAY_COUNT(DefaultViews))
							&& Canonical[0].Equals(DefaultViews[0], ESearchCase::CaseSensitive)
							&& Canonical[1].Equals(DefaultViews[1], ESearchCase::CaseSensitive);
						Payload.Views = bDefault ? TArray<FString>() : Canonical;
					}
					else
					{
						Payload.Requires = Canonical;
					}
				}
				else if (Key.Equals(TEXT("Enabled"), ESearchCase::CaseSensitive))
				{
					if (Argument.bQuoted || !IsPipelineIdentifierText(Value))
					{
						ReportBadValue(EPipelineValueSite::Pragma, Argument.Span, FText::Format(
							LOCTEXT("PipelineEnabledNotName", "'Enabled' takes the name of a 'uniform bool' or 'true', and '{0}' is neither."),
							FText::FromString(Value)));
						continue;
					}
					FString Parameter;
					if (ResolveEnabled(Value, Argument.Span, Parameter))
					{
						Payload.EnabledParameter = Parameter;
					}
				}
			}
		}

		bool FPipelineBinder::ResolveInjectionText(const FString& Text, const FLangSpan& Span, FString& OutName)
		{
			const int32 Index = FindInjection(Text);
			if (Index != INDEX_NONE)
			{
				OutName = InjectionNames[Index];
				return true;
			}

			FString Suggestion = FindNameIgnoringCase(InjectionNames, Text);
			if (Suggestion.IsEmpty() && !Text.Contains(TEXT(".")))
			{
				// `AfterDOF` for `PostProcess.AfterDOF`: the subscriptions of the post-process chain carry its prefix.
				Suggestion = FindNameIgnoringCase(InjectionNames, FString(TEXT("PostProcess.")) + Text);
				if (Suggestion.IsEmpty() && HasName(InjectionNames, FString(TEXT("PostProcess.")) + Text))
				{
					Suggestion = FString(TEXT("PostProcess.")) + Text;
				}
			}
			Diagnostics.Error(
				TEXT("DSH7303"),
				File,
				Span,
				Suggestion.IsEmpty()
					? FText::Format(LOCTEXT("UnknownInjection", "'{0}' is not an injection point; the points are {1}."), FText::FromString(Text), FText::FromString(JoinNames(InjectionNames)))
					: FText::Format(LOCTEXT("UnknownInjectionDidYouMean", "'{0}' is not an injection point; did you mean '{1}'?"), FText::FromString(Text), FText::FromString(Suggestion)));
			return false;
		}

		bool FPipelineBinder::ReadInjectionName(const FExpr* Expr, const FLangSpan& Span, FString& OutName)
		{
			FString Text;
			if (!FlattenPipelineName(Expr, Text))
			{
				ReportBadValue(EPipelineValueSite::Pass, Span, LOCTEXT("InjectionNotName", "'Injection' takes the name of an injection point, written without quotes: 'Injection = PostProcess.AfterDOF;'."));
				return false;
			}
			return ResolveInjectionText(Text, Expr ? Expr->Span : Span, OutName);
		}

		// ------------------------------------------------------------------------------------ values

		FPipelineBinder::EFold FPipelineBinder::FoldConstant(const FExpr& Expr, double OutValue[4], int32& OutComponents, IR::FIRType& OutType)
		{
			Binder.BindExpr(Expr);
			OutType = Binder.TypeOf(Expr);
			if (OutType.IsError())
			{
				return EFold::Reported;
			}
			if (OutType.GraphComponentCount() == 0 || !Binder.GetConstant(Expr, OutValue, OutComponents))
			{
				return EFold::NotConstant;
			}
			return EFold::Value;
		}

		void FPipelineBinder::ReportBadValue(const EPipelineValueSite Site, const FLangSpan& Span, const FText& Message)
		{
			// One raise per site, each with its code spelled out: the generator finds codes by the literal.
			switch (Site)
			{
			case EPipelineValueSite::Pragma:
				Diagnostics.Error(TEXT("DSH7302"), File, Span, Message);
				break;
			case EPipelineValueSite::Buffer:
				Diagnostics.Error(TEXT("DSH7308"), File, Span, Message);
				break;
			case EPipelineValueSite::Pass:
				Diagnostics.Error(TEXT("DSH7313"), File, Span, Message);
				break;
			}
		}

		bool FPipelineBinder::ReadNumber(const FExpr* Expr, const EPipelineValueSite Site, const FString& Key, const bool bIntegral, double& OutValue)
		{
			if (!Expr)
			{
				return false;
			}
			double Value[4] = { 0.0, 0.0, 0.0, 0.0 };
			int32 Components = 1;
			IR::FIRType Type;
			const EFold Fold = FoldConstant(*Expr, Value, Components, Type);
			if (Fold == EFold::Reported)
			{
				return false;
			}
			if (Fold == EFold::NotConstant || Type.Rows != 1 || Type.IsBool() || (bIntegral && !Type.IsIntegral()))
			{
				ReportBadValue(Site, Expr->Span, FText::Format(
					bIntegral
						? LOCTEXT("ValueNotInteger", "'{0}' takes a whole number the compiler can fold, and this is a {1}.")
						: LOCTEXT("ValueNotNumber", "'{0}' takes a number the compiler can fold, and this is a {1}."),
					FText::FromString(Key),
					Fold == EFold::NotConstant ? LOCTEXT("RuntimeValue", "value known only at run time") : FText::FromString(Type.ToString())));
				return false;
			}
			OutValue = Value[0];
			return true;
		}

		bool FPipelineBinder::ReadBool(const FExpr* Expr, const EPipelineValueSite Site, const FString& Key, bool& bOutValue)
		{
			if (!Expr)
			{
				return false;
			}
			double Value[4] = { 0.0, 0.0, 0.0, 0.0 };
			int32 Components = 1;
			IR::FIRType Type;
			const EFold Fold = FoldConstant(*Expr, Value, Components, Type);
			if (Fold == EFold::Reported)
			{
				return false;
			}
			if (Fold == EFold::NotConstant || !Type.IsBool() || Type.Rows != 1)
			{
				ReportBadValue(Site, Expr->Span, FText::Format(
					LOCTEXT("ValueNotBool", "'{0}' takes 'true' or 'false'."),
					FText::FromString(Key)));
				return false;
			}
			bOutValue = Value[0] != 0.0;
			return true;
		}

		bool FPipelineBinder::ReadVector(
			const FExpr* Expr, const EPipelineValueSite Site, const FString& Key, const int32 MinComponents, const int32 MaxComponents,
			const bool bIntegral, double OutValue[4], int32& OutComponents)
		{
			if (!Expr)
			{
				return false;
			}
			IR::FIRType Type;
			const EFold Fold = FoldConstant(*Expr, OutValue, OutComponents, Type);
			if (Fold == EFold::Reported)
			{
				return false;
			}
			const int32 Width = Type.GraphComponentCount();
			const bool bIntegralValues = !bIntegral
				|| (FMath::Frac(OutValue[0]) == 0.0 && FMath::Frac(OutValue[1]) == 0.0 && FMath::Frac(OutValue[2]) == 0.0 && FMath::Frac(OutValue[3]) == 0.0);
			if (Fold == EFold::NotConstant || Type.IsBool() || Width < MinComponents || Width > MaxComponents || !bIntegralValues)
			{
				ReportBadValue(Site, Expr->Span, FText::Format(
					LOCTEXT("ValueNotVector", "'{0}' takes {1} the compiler can fold, and this is a {2}."),
					FText::FromString(Key),
					MinComponents == MaxComponents
						? FText::Format(LOCTEXT("VectorOfN", "{0} {1}"), FText::AsNumber(MinComponents), bIntegral ? LOCTEXT("WholeNumbers", "whole numbers") : LOCTEXT("Numbers", "numbers"))
						: FText::Format(LOCTEXT("VectorOfRange", "{0} to {1} {2}"), FText::AsNumber(MinComponents), FText::AsNumber(MaxComponents), bIntegral ? LOCTEXT("WholeNumbers2", "whole numbers") : LOCTEXT("Numbers2", "numbers")),
					Fold == EFold::NotConstant ? LOCTEXT("RuntimeValue2", "value known only at run time") : FText::FromString(Type.ToString())));
				return false;
			}
			OutComponents = Width;
			return true;
		}

		bool FPipelineBinder::ReadColor(const FExpr* Expr, const EPipelineValueSite Site, const FString& Key, double OutValue[4])
		{
			double Value[4] = { 0.0, 0.0, 0.0, 0.0 };
			int32 Components = 1;
			if (!ReadVector(Expr, Site, Key, 1, 4, /* bIntegral */ false, Value, Components))
			{
				return false;
			}
			// HLSL's rule: a scalar stands for all four channels; a narrower vector leaves the rest at zero.
			for (int32 Channel = 0; Channel < 4; ++Channel)
			{
				OutValue[Channel] = Components == 1 ? Value[0] : (Channel < Components ? Value[Channel] : 0.0);
			}
			return true;
		}

		bool FPipelineBinder::ReadKeyWord(const FExpr* Expr, const EPipelineValueSite Site, const FString& Key, TConstArrayView<const TCHAR*> Allowed, FString& OutWord)
		{
			FString Word;
			const FLangSpan Span = Expr ? Expr->Span : FLangSpan();
			if (!ReadPipelineWord(Expr, Word))
			{
				ReportBadValue(Site, Span, FText::Format(
					LOCTEXT("ValueNotWord", "'{0}' takes one of {1}, written without quotes."),
					FText::FromString(Key),
					FText::FromString(JoinNames(Allowed))));
				return false;
			}
			const int32 Index = FindName(Allowed, Word);
			if (Index == INDEX_NONE)
			{
				const FString Suggestion = FindNameIgnoringCase(Allowed, Word);
				ReportBadValue(Site, Span, Suggestion.IsEmpty()
					? FText::Format(LOCTEXT("ValueUnknownWord", "'{0}' is not a value of '{1}'; it takes {2}."), FText::FromString(Word), FText::FromString(Key), FText::FromString(JoinNames(Allowed)))
					: FText::Format(LOCTEXT("ValueUnknownWordDidYouMean", "'{0}' is not a value of '{1}'; did you mean '{2}'?"), FText::FromString(Word), FText::FromString(Key), FText::FromString(Suggestion)));
				return false;
			}
			OutWord = Allowed[Index];
			return true;
		}

		// ----------------------------------------------------------------------------------- buffers

		bool FPipelineBinder::IsKnownBuffer(const FString& Name) const
		{
			return IsBuiltinBuffer(Name) || FindPayloadBuffer(Name) != nullptr;
		}

		const IR::FIRPassBuffer* FPipelineBinder::FindPayloadBuffer(const FString& Name) const
		{
			return Payload.Buffers.FindByPredicate([&Name](const IR::FIRPassBuffer& Buffer) { return Buffer.Name.Equals(Name, ESearchCase::CaseSensitive); });
		}

		void FPipelineBinder::ReportUnknownBuffer(const FString& Name, const FLangSpan& Span)
		{
			TArray<FString> Candidates;
			for (const IR::FIRPassBuffer& Buffer : Payload.Buffers)
			{
				Candidates.Add(Buffer.Name);
			}
			for (const TCHAR* Builtin : BuiltinBufferNames)
			{
				Candidates.Add(Builtin);
			}
			const FString Suggestion = FLangBinder::SuggestCaseInsensitive(Name, Candidates);
			Diagnostics.Error(
				TEXT("DSH4403"),
				File,
				Span,
				Suggestion.IsEmpty()
					? FText::Format(LOCTEXT("UnknownBuffer", "'{0}' is neither a buffer of this pipeline nor a built-in texture; declare it with 'buffer {0} : <Format>;'."), FText::FromString(Name))
					: FText::Format(LOCTEXT("UnknownBufferDidYouMean", "'{0}' is neither a buffer of this pipeline nor a built-in texture; did you mean '{1}'? Names are case-sensitive."), FText::FromString(Name), FText::FromString(Suggestion)));
		}

		void FPipelineBinder::BindBuffers()
		{
			for (const FBufferDecl* Decl : BufferDecls)
			{
				BindBuffer(*Decl);
			}
		}

		void FPipelineBinder::BindBuffer(const FBufferDecl& Decl)
		{
			if (IsBuiltinBuffer(Decl.Name))
			{
				Diagnostics.Error(
					TEXT("DSH4402"),
					File,
					Decl.NameSpan,
					FText::Format(
						LOCTEXT("BufferBuiltinName", "'{0}' is a built-in texture a pass binds without declaring it, and cannot be declared as a buffer."),
						FText::FromString(Decl.Name)));
				return;
			}
			if (const int32 Existing = Pipeline.FindBuffer(Decl.Name); Existing != INDEX_NONE)
			{
				Diagnostics.Error(
					TEXT("DSH4400"),
					File,
					Decl.NameSpan,
					FText::Format(
						LOCTEXT("BufferTwice", "Buffer '{0}' is declared twice; the declaration on line {1} already makes it."),
						FText::FromString(Decl.Name),
						FText::AsNumber(Payload.Buffers[Existing].Source.Span.Line)));
				return;
			}
			// The asset keeps buffer names as FNames, which compare ignoring case (UDreamPassPipeline::FindBuffer, Validate): two
			// names that differ in case only would be one buffer there, and the runtime would refuse the pipeline's passes.
			if (const IR::FIRPassBuffer* SameName = Payload.Buffers.FindByPredicate([&Decl](const IR::FIRPassBuffer& Buffer) { return Buffer.Name.Equals(Decl.Name, ESearchCase::IgnoreCase); }))
			{
				Diagnostics.Error(
					TEXT("DSH4400"),
					File,
					Decl.NameSpan,
					FText::Format(
						LOCTEXT("BufferNameCaseClash", "Buffer '{0}' differs from '{1}', declared on line {2}, in case only; the pipeline asset keeps buffer names as Unreal names, which compare ignoring case, so the two would be one buffer there. Rename one."),
						FText::FromString(Decl.Name),
						FText::FromString(SameName->Name),
						FText::AsNumber(SameName->Source.Span.Line)));
				return;
			}
			if (Bound.FindGlobal(Decl.Name) != INDEX_NONE || Decl.Name.Equals(WeightParameterName, ESearchCase::CaseSensitive))
			{
				Diagnostics.Error(
					TEXT("DSH4400"),
					File,
					Decl.NameSpan,
					FText::Format(
						LOCTEXT("BufferNameTaken", "'{0}' already names a parameter or a constant of this pipeline; a buffer shares one namespace with them."),
						FText::FromString(Decl.Name)));
				return;
			}

			IR::FIRPassBuffer Buffer;
			Buffer.Name = Decl.Name;
			Buffer.Source.File = File;
			Buffer.Source.Span = Decl.Span;
			Buffer.Description = DescriptionOf(Decl.Doc, LOCTEXT("WhereBuffer", "a buffer"));

			const int32 FormatIndex = FindName(FormatNames, Decl.Format);
			if (FormatIndex == INDEX_NONE)
			{
				const FString Suggestion = FindNameIgnoringCase(FormatNames, Decl.Format);
				Diagnostics.Error(
					TEXT("DSH7305"),
					File,
					Decl.FormatSpan,
					Suggestion.IsEmpty()
						? FText::Format(LOCTEXT("UnknownFormat", "'{0}' is not a buffer format; the formats are {1}."), FText::FromString(Decl.Format), FText::FromString(JoinNames(FormatNames)))
						: FText::Format(LOCTEXT("UnknownFormatDidYouMean", "'{0}' is not a buffer format; did you mean '{1}'? Formats are case-sensitive."), FText::FromString(Decl.Format), FText::FromString(Suggestion)));
				Buffer.Format = Decl.Format;
			}
			else
			{
				Buffer.Format = FormatNames[FormatIndex];
				// Nothing can use an integer buffer yet: an HLSL slot declares its inputs Texture2D<float4> and its compute
				// outputs RWTexture2D<float4>, which an integer view does not match, and materials, mesh passes, exports
				// and the visualizer read and write floats. A float format holds an id exactly up to 2^24.
				if (IsIntegerFormat(Buffer.Format))
				{
					Diagnostics.Error(
						TEXT("DSH7321"),
						File,
						Decl.FormatSpan,
						FText::Format(
							LOCTEXT("IntegerFormatUnsupported", "'{0}' is an integer format, which no pass can read or write yet: HLSL passes see float4 textures, and materials and mesh passes write floats. Use 'R32F' or 'RG32F' (an id is exact up to 16777216)."),
							FText::FromString(Buffer.Format)));
				}
			}

			const FPipelineKeyValue* ScaleKey = nullptr;
			const FPipelineKeyValue* SizeKey = nullptr;
			const FPipelineKeyValue* ResolutionKey = nullptr;
			TArray<const FPipelineKeyValue*> Seen;
			for (const FPipelineKeyValue& Argument : Decl.Arguments)
			{
				const FPipelineKeyValue* const* First = Seen.FindByPredicate([&Argument](const FPipelineKeyValue* Earlier)
				{
					return Earlier->Key.Equals(Argument.Key, ESearchCase::CaseSensitive);
				});
				if (First)
				{
					Diagnostics.Error(
						TEXT("DSH7307"),
						File,
						Argument.Span,
						FText::Format(
							LOCTEXT("BufferKeyTwice", "'{0}' is set twice for buffer '{1}'; it was already set on line {2}."),
							FText::FromString(Argument.Key),
							FText::FromString(Decl.Name),
							FText::AsNumber((*First)->Span.Line)));
					continue;
				}
				Seen.Add(&Argument);

				const int32 KeyIndex = FindName(BufferKeys, Argument.Key);
				if (KeyIndex == INDEX_NONE)
				{
					const FString Suggestion = FindNameIgnoringCase(BufferKeys, Argument.Key);
					Diagnostics.Error(
						TEXT("DSH7306"),
						File,
						Argument.KeySpan,
						Suggestion.IsEmpty()
							? FText::Format(LOCTEXT("UnknownBufferKey", "'{0}' is not a key of a buffer; the keys are {1}."), FText::FromString(Argument.Key), FText::FromString(JoinNames(BufferKeys)))
							: FText::Format(LOCTEXT("UnknownBufferKeyDidYouMean", "'{0}' is not a key of a buffer; did you mean '{1}'? Keys are case-sensitive."), FText::FromString(Argument.Key), FText::FromString(Suggestion)));
					continue;
				}

				const FString Key = BufferKeys[KeyIndex];
				const FExpr* Value = Argument.Value.Get();

				if (Key.Equals(TEXT("Scale"), ESearchCase::CaseSensitive))
				{
					double Scale = 1.0;
					if (ReadNumber(Value, EPipelineValueSite::Buffer, Key, /* bIntegral */ false, Scale))
					{
						if (Scale < 0.0625 || Scale > 4.0)
						{
							ReportBadValue(EPipelineValueSite::Buffer, Argument.Span, FText::Format(
								LOCTEXT("ScaleRange", "'Scale' of buffer '{0}' is {1}, and a scale is between 0.0625 and 4."),
								FText::FromString(Decl.Name),
								FText::FromString(FString::SanitizeFloat(Scale))));
						}
						else
						{
							Buffer.Scale = Scale;
						}
					}
					ScaleKey = &Argument;
				}
				else if (Key.Equals(TEXT("Size"), ESearchCase::CaseSensitive))
				{
					double Size[4] = { 0.0, 0.0, 0.0, 0.0 };
					int32 Components = 2;
					if (ReadVector(Value, EPipelineValueSite::Buffer, Key, 2, 2, /* bIntegral */ true, Size, Components))
					{
						if (Size[0] < 1.0 || Size[1] < 1.0 || Size[0] > 16384.0 || Size[1] > 16384.0)
						{
							ReportBadValue(EPipelineValueSite::Buffer, Argument.Span, FText::Format(
								LOCTEXT("SizeRange", "'Size' of buffer '{0}' is {1} x {2}, and each side is between 1 and 16384."),
								FText::FromString(Decl.Name),
								FText::AsNumber(static_cast<int64>(Size[0])),
								FText::AsNumber(static_cast<int64>(Size[1]))));
						}
						else
						{
							Buffer.Resolution = ResolutionFixed;
							Buffer.bResolutionWritten = true;
							Buffer.FixedWidth = static_cast<int32>(Size[0]);
							Buffer.FixedHeight = static_cast<int32>(Size[1]);
						}
					}
					SizeKey = &Argument;
				}
				else if (Key.Equals(TEXT("Resolution"), ESearchCase::CaseSensitive))
				{
					FString Word;
					if (ReadPipelineWord(Value, Word) && Word.Equals(ResolutionFixed, ESearchCase::CaseSensitive))
					{
						ReportBadValue(EPipelineValueSite::Buffer, Argument.Span, LOCTEXT("ResolutionFixed", "A fixed size is written 'Size = int2(width, height)', not 'Resolution = Fixed'."));
					}
					else if (ReadKeyWord(Value, EPipelineValueSite::Buffer, Key, MakeArrayView(WrittenResolutionNames), Word))
					{
						Buffer.Resolution = Word;
						Buffer.bResolutionWritten = true;
					}
					ResolutionKey = &Argument;
				}
				else if (Key.Equals(TEXT("Clear"), ESearchCase::CaseSensitive))
				{
					FString Word;
					if (ReadPipelineWord(Value, Word) && Word.Equals(TEXT("None"), ESearchCase::CaseSensitive))
					{
						Buffer.bClear = false;
					}
					else
					{
						double Color[4] = { 0.0, 0.0, 0.0, 0.0 };
						if (ReadColor(Value, EPipelineValueSite::Buffer, Key, Color))
						{
							Buffer.bClear = true;
							for (int32 Channel = 0; Channel < 4; ++Channel)
							{
								Buffer.ClearValue[Channel] = Color[Channel];
							}
						}
					}
				}
				else if (Key.Equals(TEXT("Mips"), ESearchCase::CaseSensitive))
				{
					double Mips = 1.0;
					if (ReadNumber(Value, EPipelineValueSite::Buffer, Key, /* bIntegral */ true, Mips))
					{
						if (Mips != 1.0)
						{
							ReportBadValue(EPipelineValueSite::Buffer, Argument.Span, FText::Format(
								LOCTEXT("MipsUnsupported", "'Mips' of buffer '{0}' is {1}; only 1 is supported because passes cannot initialize or update a mip chain."),
								FText::FromString(Decl.Name),
								FText::AsNumber(static_cast<int64>(Mips))));
						}
						else
						{
							Buffer.Mips = static_cast<int32>(Mips);
						}
					}
				}
				else if (Key.Equals(TEXT("History"), ESearchCase::CaseSensitive))
				{
					ReadBool(Value, EPipelineValueSite::Buffer, Key, Buffer.bHistory);
				}
				else if (Key.Equals(TEXT("Export"), ESearchCase::CaseSensitive))
				{
					ReadBool(Value, EPipelineValueSite::Buffer, Key, Buffer.bExport);
				}
			}

			if (SizeKey && (ScaleKey || ResolutionKey))
			{
				const FPipelineKeyValue* Other = ScaleKey ? ScaleKey : ResolutionKey;
				Diagnostics.Error(
					TEXT("DSH7309"),
					File,
					Other->Span,
					FText::Format(
						LOCTEXT("BufferSizeAndScale", "'Size' gives buffer '{0}' a fixed size, so '{1}' has nothing to say; remove one of them."),
						FText::FromString(Decl.Name),
						FText::FromString(Other->Key)));
			}

			if (Buffer.bExport && (IsDepthFormat(Buffer.Format) || IsIntegerFormat(Buffer.Format)))
			{
				Diagnostics.Error(
					TEXT("DSH7355"),
					File,
					Decl.FormatSpan,
					FText::Format(
						LOCTEXT("ExportFormat", "Buffer '{0}' is exported, and an exported buffer becomes a render target asset that materials sample, which a '{1}' buffer cannot be; export a float format."),
						FText::FromString(Decl.Name),
						FText::FromString(Buffer.Format)));
			}

			Payload.Buffers.Add(MoveTemp(Buffer));
			Pipeline.BufferDecls.Add(&Decl);
		}

		void FPipelineBinder::ApplyBufferDefaults()
		{
			for (IR::FIRPassBuffer& Buffer : Payload.Buffers)
			{
				if (!Buffer.bResolutionWritten)
				{
					Buffer.Resolution = InferBufferResolution(Payload, Buffer.Name);
				}
			}
		}

		// ------------------------------------------------------------------------------------ passes

		void FPipelineBinder::BindPasses()
		{
			for (const FPassDecl* Decl : PassDecls)
			{
				const int32 Existing = Pipeline.FindPass(Decl->Name);
				if (Existing != INDEX_NONE)
				{
					Diagnostics.Error(
						TEXT("DSH4401"),
						File,
						Decl->NameSpan,
						FText::Format(
							LOCTEXT("PassTwice", "A pass named '{0}' is already declared on line {1}; RDG events and stats are named after passes, so each name is used once."),
							FText::FromString(Decl->Name),
							FText::AsNumber(Payload.Passes[Existing].Source.Span.Line)));
					continue;
				}
				// As for buffers: the asset keeps pass names as FNames (UDreamPassPipeline::FindPassIndex, Validate).
				if (const IR::FIRPass* SameName = Payload.Passes.FindByPredicate([Decl](const IR::FIRPass& Pass) { return Pass.Name.Equals(Decl->Name, ESearchCase::IgnoreCase); }))
				{
					Diagnostics.Error(
						TEXT("DSH4401"),
						File,
						Decl->NameSpan,
						FText::Format(
							LOCTEXT("PassNameCaseClash", "Pass '{0}' differs from '{1}', declared on line {2}, in case only; the pipeline asset keeps pass names as Unreal names, which compare ignoring case, so the two would be one pass there. Rename one."),
							FText::FromString(Decl->Name),
							FText::FromString(SameName->Name),
							FText::AsNumber(SameName->Source.Span.Line)));
					continue;
				}

				const int32 KindIndex = FindName(PassKinds, Decl->PassKind);
				if (KindIndex == INDEX_NONE)
				{
					const FString Suggestion = FindNameIgnoringCase(PassKinds, Decl->PassKind);
					Diagnostics.Error(
						TEXT("DSH7304"),
						File,
						Decl->PassKindSpan,
						Suggestion.IsEmpty()
							? FText::Format(LOCTEXT("UnknownPassKind", "'{0}' is not a pass kind; a pass is {1}."), FText::FromString(Decl->PassKind), FText::FromString(JoinNames(PassKinds)))
							: FText::Format(LOCTEXT("UnknownPassKindDidYouMean", "'{0}' is not a pass kind; did you mean '{1}'? Kinds are written in lower case."), FText::FromString(Decl->PassKind), FText::FromString(Suggestion)));
				}

				BindPass(*Decl, KindIndex);
			}
		}

		void FPipelineBinder::BindPass(const FPassDecl& Decl, const int32 KindIndex)
		{
			IR::FIRPass Pass;
			Pass.Name = Decl.Name;
			Pass.Kind = KindIndex != INDEX_NONE ? FString(PassKinds[KindIndex]) : Decl.PassKind;
			Pass.Source.File = File;
			Pass.Source.Span = Decl.Span;
			Pass.Description = DescriptionOf(Decl.Doc, LOCTEXT("WherePass", "a pass"));

			FPipelinePassKeys Keys;
			for (const TUniquePtr<FPassStmt>& Statement : Decl.Statements)
			{
				if (!Statement)
				{
					continue;
				}
				switch (Statement->StmtKind)
				{
				case EPassStmtKind::Setting:
					BindPassSetting(Pass, KindIndex, *Statement, Keys);
					break;
				case EPassStmtKind::Read:
				case EPassStmtKind::Write:
					BindPassBinding(Pass, *Statement);
					break;
				case EPassStmtKind::Param:
					BindPassParam(Pass, *Statement);
					break;
				case EPassStmtKind::Hlsl:
					BindPassHlsl(Pass, *Statement, Keys);
					break;
				}
			}

			FinishPass(Pass, KindIndex, Decl, Keys);
			if (References && KindIndex != INDEX_NONE)
			{
				CheckPassAgainstReferences(Pass, KindIndex, Decl, Keys);
			}

			Payload.Passes.Add(MoveTemp(Pass));
			Pipeline.PassDecls.Add(&Decl);
			PassKeys.Add(MoveTemp(Keys));
			PassKindIndices.Add(KindIndex);
		}

		void FPipelineBinder::BindPassSetting(IR::FIRPass& Pass, const int32 KindIndex, const FPassStmt& Statement, FPipelinePassKeys& Keys)
		{
			const FString& Key = Statement.Name;
			const FExpr* Value = Statement.Value.Get();

			if (const FPassStmt* First = Keys.Find(*Key))
			{
				Diagnostics.Error(
					TEXT("DSH7312"),
					File,
					Statement.Span,
					FText::Format(
						LOCTEXT("PassKeyTwice", "'{0}' is set twice in pass '{1}'; it was already set on line {2}."),
						FText::FromString(Key),
						FText::FromString(Pass.Name),
						FText::AsNumber(First->Span.Line)));
				return;
			}

			const bool bCommon = HasName(CommonPassKeys, Key);
			const bool bOwnKind = KindIndex != INDEX_NONE && HasName(KeysOfKind(KindIndex), Key);
			if (!bCommon && !bOwnKind)
			{
				if (KindIndex == INDEX_NONE)
				{
					// The kind is unknown and said so; what its keys are cannot be told.
					return;
				}
				int32 OtherKind = INDEX_NONE;
				for (int32 Candidate = 0; Candidate < static_cast<int32>(UE_ARRAY_COUNT(PassKinds)) && OtherKind == INDEX_NONE; ++Candidate)
				{
					if (Candidate != KindIndex && HasName(KeysOfKind(Candidate), Key))
					{
						OtherKind = Candidate;
					}
				}
				if (OtherKind != INDEX_NONE)
				{
					Diagnostics.Error(
						TEXT("DSH7311"),
						File,
						Statement.NameSpan,
						FText::Format(
							LOCTEXT("PassKeyOfOtherKind", "'{0}' is a key of {1} passes, and '{2}' is a {3} pass."),
							FText::FromString(Key),
							FText::FromString(PassKinds[OtherKind]),
							FText::FromString(Pass.Name),
							FText::FromString(PassKinds[KindIndex])));
					return;
				}

				TArray<const TCHAR*> Own;
				for (const TCHAR* CommonKey : CommonPassKeys)
				{
					Own.Add(CommonKey);
				}
				for (const TCHAR* OwnKey : KeysOfKind(KindIndex))
				{
					Own.Add(OwnKey);
				}
				const FString Suggestion = FindNameIgnoringCase(Own, Key);
				Diagnostics.Error(
					TEXT("DSH7310"),
					File,
					Statement.NameSpan,
					Suggestion.IsEmpty()
						? FText::Format(LOCTEXT("UnknownPassKey", "'{0}' is not a key of a {1} pass; its keys are {2}."), FText::FromString(Key), FText::FromString(PassKinds[KindIndex]), FText::FromString(JoinNames(Own)))
						: FText::Format(LOCTEXT("UnknownPassKeyDidYouMean", "'{0}' is not a key of a {1} pass; did you mean '{2}'? Keys are case-sensitive."), FText::FromString(Key), FText::FromString(PassKinds[KindIndex]), FText::FromString(Suggestion)));
				return;
			}

			Keys.Written.Emplace(Key, &Statement);

			if (Key.Equals(TEXT("Injection"), ESearchCase::CaseSensitive))
			{
				FString Name;
				if (ReadInjectionName(Value, Statement.Span, Name))
				{
					Pass.Injection = Name;
					Pass.bInjectionWritten = true;
				}
			}
			else if (Key.Equals(TEXT("Enabled"), ESearchCase::CaseSensitive))
			{
				bool bLiteral = true;
				FString Word;
				if (IsEnabledLiteral(Value, bLiteral))
				{
					ResolveEnabled(bLiteral ? TEXT("true") : TEXT("false"), Value->Span, Pass.EnabledParameter);
				}
				else if (ReadPipelineWord(Value, Word))
				{
					ResolveEnabled(Word, Value->Span, Pass.EnabledParameter);
				}
				else
				{
					ReportBadValue(EPipelineValueSite::Pass, Statement.Span, LOCTEXT("PassEnabledNotName", "'Enabled' takes the name of a 'uniform bool', or 'true'."));
				}
			}
			else if (Key.Equals(TEXT("Material"), ESearchCase::CaseSensitive) || Key.Equals(TEXT("Shader"), ESearchCase::CaseSensitive))
			{
				const bool bMaterial = Key.Equals(TEXT("Material"), ESearchCase::CaseSensitive);
				FString Text;
				if (!ReadPipelineString(Value, Text) || Text.TrimStartAndEnd().IsEmpty())
				{
					ReportBadValue(EPipelineValueSite::Pass, Statement.Span, bMaterial
						? LOCTEXT("MaterialNotString", "'Material' takes the material as a quoted name or object path: 'Material = \"PP_Composite\";'.")
						: LOCTEXT("ShaderNotString", "'Shader' takes the shader file as a quoted path: 'Shader = \"Passes/Blur.usf\";'."));
					return;
				}
				Text = Text.TrimStartAndEnd();
				if (bMaterial)
				{
					Pass.MaterialReference = Text;
				}
				else
				{
					// A leading `/` is a virtual path; anything else is relative to the folder of this `.dsp` and is the host's to
					// resolve (FPipelineShaderInfo). The file may be anywhere: the engine compiles a snapshot of it.
					Pass.ShaderReference = Text;
					if (Text.StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
					{
						Pass.ShaderVirtualPath = Text;
					}
					const FString Extension = FPaths::GetExtension(Text);
					if (!Extension.Equals(TEXT("usf"), ESearchCase::IgnoreCase) && !Extension.Equals(TEXT("ush"), ESearchCase::IgnoreCase))
					{
						Diagnostics.Error(
							TEXT("DSH4407"),
							File,
							Value->Span,
							FText::Format(
								LOCTEXT("ShaderExtension", "'{0}' is not a '.usf' or '.ush' file; the engine compiles shader files of those two kinds only."),
								FText::FromString(Text)));
					}
				}
			}
			else if (Key.Equals(TEXT("Entry"), ESearchCase::CaseSensitive))
			{
				FString Entry;
				if (!(ReadPipelineWord(Value, Entry) || ReadPipelineString(Value, Entry)) || !IsPipelineIdentifierText(Entry))
				{
					ReportBadValue(EPipelineValueSite::Pass, Statement.Span, LOCTEXT("EntryNotName", "'Entry' takes the name of the function in the shader file: 'Entry = BlurCS;'."));
					return;
				}
				Pass.Entry = Entry;
			}
			else if (Key.Equals(TEXT("Threads"), ESearchCase::CaseSensitive))
			{
				double Threads[4] = { 1.0, 1.0, 1.0, 0.0 };
				int32 Components = 3;
				if (!ReadVector(Value, EPipelineValueSite::Pass, Key, 2, 3, /* bIntegral */ true, Threads, Components))
				{
					return;
				}
				if (Components == 2)
				{
					Threads[2] = 1.0;
				}
				const double Total = Threads[0] * Threads[1] * Threads[2];
				if (Threads[0] < 1.0 || Threads[1] < 1.0 || Threads[2] < 1.0 || Threads[2] > 64.0 || Total > 1024.0)
				{
					ReportBadValue(EPipelineValueSite::Pass, Statement.Span, LOCTEXT("ThreadsRange", "'Threads' is a thread group of at least 1 in each direction, at most 64 in z and at most 1024 threads in all."));
					return;
				}
				Pass.ThreadsX = static_cast<int32>(Threads[0]);
				Pass.ThreadsY = static_cast<int32>(Threads[1]);
				Pass.ThreadsZ = static_cast<int32>(Threads[2]);
				Pass.bThreadsWritten = true;
			}
			else if (Key.Equals(TEXT("Dispatch"), ESearchCase::CaseSensitive))
			{
				const FExpr* Inner = PipelineUnparen(Value);
				FString Buffer;
				if (ReadPipelineWord(Inner, Buffer))
				{
					Pass.DispatchMode = TEXT("Buffer");
					Pass.DispatchBuffer = Buffer;
					Pass.DispatchScale = 1.0;
				}
				else if (const FBinaryExpr* Binary = Inner ? Inner->As<FBinaryExpr>() : nullptr;
					Binary && (Binary->Op == EBinaryOp::Divide || Binary->Op == EBinaryOp::Multiply) && ReadPipelineWord(Binary->Left.Get(), Buffer))
				{
					double Factor = 1.0;
					if (!ReadNumber(Binary->Right.Get(), EPipelineValueSite::Pass, Key, /* bIntegral */ false, Factor))
					{
						return;
					}
					if (Factor <= 0.0)
					{
						ReportBadValue(EPipelineValueSite::Pass, Statement.Span, LOCTEXT("DispatchFactor", "'Dispatch = Buffer / n' and 'Buffer * n' take a positive n."));
						return;
					}
					Pass.DispatchMode = TEXT("Buffer");
					Pass.DispatchBuffer = Buffer;
					Pass.DispatchScale = Binary->Op == EBinaryOp::Divide ? 1.0 / Factor : Factor;
				}
				else
				{
					double Size[4] = { 1.0, 1.0, 1.0, 0.0 };
					int32 Components = 3;
					if (!ReadVector(Value, EPipelineValueSite::Pass, Key, 2, 3, /* bIntegral */ true, Size, Components))
					{
						return;
					}
					if (Components == 2)
					{
						Size[2] = 1.0;
					}
					if (Size[0] < 1.0 || Size[1] < 1.0 || Size[2] < 1.0)
					{
						ReportBadValue(EPipelineValueSite::Pass, Statement.Span, LOCTEXT("DispatchSizeRange", "A fixed 'Dispatch' is at least one thread in each direction."));
						return;
					}
					Pass.DispatchMode = TEXT("Fixed");
					Pass.DispatchX = static_cast<int32>(Size[0]);
					Pass.DispatchY = static_cast<int32>(Size[1]);
					Pass.DispatchZ = static_cast<int32>(Size[2]);
				}
				if (Pass.DispatchMode.Equals(TEXT("Buffer"), ESearchCase::CaseSensitive) && !IsKnownBuffer(Pass.DispatchBuffer))
				{
					ReportUnknownBuffer(Pass.DispatchBuffer, Value->Span);
				}
			}
			else if (Key.Equals(TEXT("Filter"), ESearchCase::CaseSensitive))
			{
				TArray<IR::FIRPassFilterClause> Clauses;
				if (BuildFilter(Value, Clauses))
				{
					Pass.Filter = MoveTemp(Clauses);
				}
			}
			else if (Key.Equals(TEXT("Mode"), ESearchCase::CaseSensitive))
			{
				ReadKeyWord(Value, EPipelineValueSite::Pass, Key, MakeArrayView(MeshModes), Pass.MeshMode);
			}
			else if (Key.Equals(TEXT("Depth"), ESearchCase::CaseSensitive))
			{
				FString Callee;
				if (const FCallExpr* Call = ReadPipelineCall(Value, Callee); Call && Callee.Equals(TEXT("Own"), ESearchCase::CaseSensitive))
				{
					FString Buffer;
					if (Call->Arguments.Num() != 1 || !Call->Arguments[0].Name.IsEmpty() || !ReadPipelineWord(Call->Arguments[0].Value.Get(), Buffer))
					{
						ReportBadValue(EPipelineValueSite::Pass, Statement.Span, LOCTEXT("DepthOwnShape", "'Depth = Own(Buffer)' takes one Depth32 buffer of this pipeline."));
						return;
					}
					Pass.Depth = TEXT("Own");
					Pass.DepthBuffer = Buffer;
					const IR::FIRPassBuffer* Declared = FindPayloadBuffer(Buffer);
					if (!Declared)
					{
						if (IsBuiltinBuffer(Buffer))
						{
							Diagnostics.Error(
								TEXT("DSH7330"),
								File,
								Call->Arguments[0].Value->Span,
								FText::Format(
									LOCTEXT("DepthOwnBuiltin", "'Depth = Own({0})' tests against a depth of this pipeline's own, and '{0}' is a built-in texture; declare 'buffer MyDepth : Depth32;', or write 'Depth = TestScene'."),
									FText::FromString(Buffer)));
						}
						else
						{
							ReportUnknownBuffer(Buffer, Call->Arguments[0].Value->Span);
						}
					}
					else if (!IsDepthFormat(Declared->Format))
					{
						Diagnostics.Error(
							TEXT("DSH7330"),
							File,
							Call->Arguments[0].Value->Span,
							FText::Format(
								LOCTEXT("DepthOwnNotDepth", "'Depth = Own({0})' needs a Depth32 buffer, and '{0}' is {1}."),
								FText::FromString(Buffer),
								FText::FromString(Declared->Format)));
					}
				}
				else
				{
					static const TCHAR* const Words[] = { TEXT("TestScene"), TEXT("None") };
					FString Word;
					if (ReadKeyWord(Value, EPipelineValueSite::Pass, Key, MakeArrayView(Words), Word))
					{
						Pass.Depth = Word;
						Pass.DepthBuffer.Reset();
					}
				}
			}
			else if (Key.Equals(TEXT("Cull"), ESearchCase::CaseSensitive))
			{
				ReadKeyWord(Value, EPipelineValueSite::Pass, Key, MakeArrayView(CullModes), Pass.Cull);
			}
			else if (Key.Equals(TEXT("Blend"), ESearchCase::CaseSensitive))
			{
				ReadKeyWord(Value, EPipelineValueSite::Pass, Key, MakeArrayView(BlendModes), Pass.Blend);
			}
			else if (Key.Equals(TEXT("Usage"), ESearchCase::CaseSensitive))
			{
				TArray<FString> Words;
				TArray<FLangSpan> Spans;
				if (!CollectBarWords(Value, /* bAllowStrings */ false, Words, Spans))
				{
					ReportBadValue(EPipelineValueSite::Pass, Statement.Span, FText::Format(
						LOCTEXT("UsageShape", "'Usage' takes vertex factory kinds joined by '|': {0}."),
						FText::FromString(JoinNames(UsageNames))));
					return;
				}
				const int32 UsageCount = static_cast<int32>(UE_ARRAY_COUNT(UsageNames));
				TArray<bool> Set;
				Set.Init(false, UsageCount);
				bool bAllKnown = true;
				for (int32 Index = 0; Index < Words.Num(); ++Index)
				{
					const int32 Flag = FindName(UsageNames, Words[Index]);
					if (Flag == INDEX_NONE)
					{
						bAllKnown = false;
						FString Suggestion = FindNameIgnoringCase(UsageNames, Words[Index]);
						if (Suggestion.IsEmpty() && Words[Index].Equals(TEXT("Splines"), ESearchCase::IgnoreCase))
						{
							Suggestion = TEXT("SplineMesh");
						}
						ReportBadValue(EPipelineValueSite::Pass, Spans[Index], Suggestion.IsEmpty()
							? FText::Format(LOCTEXT("UsageUnknown", "'{0}' is not a mesh usage; the usages are {1}."), FText::FromString(Words[Index]), FText::FromString(JoinNames(UsageNames)))
							: FText::Format(LOCTEXT("UsageUnknownDidYouMean", "'{0}' is not a mesh usage; did you mean '{1}'?"), FText::FromString(Words[Index]), FText::FromString(Suggestion)));
						continue;
					}
					Set[Flag] = true;
				}
				if (!bAllKnown)
				{
					return;
				}
				TArray<FString> Canonical;
				for (int32 Flag = 0; Flag < UsageCount; ++Flag)
				{
					if (Set[Flag])
					{
						Canonical.Add(UsageNames[Flag]);
					}
				}
				// The default set is said by saying nothing (FIRPass::Usage).
				bool bDefault = Canonical.Num() == static_cast<int32>(UE_ARRAY_COUNT(DefaultUsage));
				for (int32 Index = 0; bDefault && Index < Canonical.Num(); ++Index)
				{
					bDefault = Canonical[Index].Equals(DefaultUsage[Index], ESearchCase::CaseSensitive);
				}
				Pass.Usage = bDefault ? TArray<FString>() : Canonical;
			}
			else if (Key.Equals(TEXT("Nanite"), ESearchCase::CaseSensitive))
			{
				FString Callee;
				if (const FCallExpr* Call = ReadPipelineCall(Value, Callee); Call && Callee.Equals(TEXT("AssignStencil"), ESearchCase::CaseSensitive))
				{
					double Bits = 255.0;
					if (Call->Arguments.Num() != 1 || !Call->Arguments[0].Name.IsEmpty())
					{
						ReportBadValue(EPipelineValueSite::Pass, Statement.Span, LOCTEXT("AssignStencilShape", "'Nanite = AssignStencil(n)' takes one stencil value, 1 to 255."));
						return;
					}
					if (!ReadNumber(Call->Arguments[0].Value.Get(), EPipelineValueSite::Pass, TEXT("AssignStencil"), /* bIntegral */ true, Bits))
					{
						return;
					}
					if (Bits < 1.0 || Bits > 255.0)
					{
						ReportBadValue(EPipelineValueSite::Pass, Statement.Span, LOCTEXT("AssignStencilRange", "'AssignStencil' takes a stencil value from 1 to 255."));
						return;
					}
					Pass.Nanite = TEXT("AssignStencil");
					Pass.AssignedStencilValue = static_cast<int32>(Bits);
				}
				else
				{
					static const TCHAR* const Words[] = { TEXT("Skip"), TEXT("StencilMask") };
					ReadKeyWord(Value, EPipelineValueSite::Pass, Key, MakeArrayView(Words), Pass.Nanite);
				}
			}
			else if (Key.Equals(TEXT("NaniteValue"), ESearchCase::CaseSensitive))
			{
				ReadColor(Value, EPipelineValueSite::Pass, Key, Pass.NaniteValue);
			}
			else if (Key.Equals(TEXT("Value"), ESearchCase::CaseSensitive))
			{
				ReadColor(Value, EPipelineValueSite::Pass, Key, Pass.ClearValue);
			}
		}

		// ------------------------------------------------------------------------------------ filter

		bool FPipelineBinder::BuildFilter(const FExpr* Expr, TArray<IR::FIRPassFilterClause>& OutClauses)
		{
			const FExpr* Inner = PipelineUnparen(Expr);
			if (!Inner)
			{
				return false;
			}

			if (const FBinaryExpr* Binary = Inner->As<FBinaryExpr>())
			{
				if (Binary->Op == EBinaryOp::BitwiseOr)
				{
					// A union: either side's clauses.
					TArray<IR::FIRPassFilterClause> Left;
					TArray<IR::FIRPassFilterClause> Right;
					if (!BuildFilter(Binary->Left.Get(), Left) || !BuildFilter(Binary->Right.Get(), Right))
					{
						return false;
					}
					OutClauses = MoveTemp(Left);
					OutClauses.Append(MoveTemp(Right));
					return true;
				}
				if (Binary->Op == EBinaryOp::BitwiseAnd)
				{
					// An intersection: every clause of one side with every clause of the other (`&` binds tighter than `|`,
					// so `A & B | C` arrived here already as `(A & B) | C`).
					TArray<IR::FIRPassFilterClause> Left;
					TArray<IR::FIRPassFilterClause> Right;
					if (!BuildFilter(Binary->Left.Get(), Left) || !BuildFilter(Binary->Right.Get(), Right))
					{
						return false;
					}
					OutClauses.Reset();
					for (const IR::FIRPassFilterClause& A : Left)
					{
						for (const IR::FIRPassFilterClause& B : Right)
						{
							IR::FIRPassFilterClause& Clause = OutClauses.AddDefaulted_GetRef();
							Clause.AllOf = A.AllOf;
							Clause.AllOf.Append(B.AllOf);
						}
					}
					return true;
				}
				Diagnostics.Error(
					TEXT("DSH7314"),
					File,
					Inner->Span,
					LOCTEXT("FilterOperator", "A filter joins its terms with '|' (either) and '&' (both), and nothing else."));
				return false;
			}

			IR::FIRPassFilterTerm Term;
			if (!BuildFilterTerm(Inner, Term))
			{
				return false;
			}
			OutClauses.Reset();
			OutClauses.AddDefaulted_GetRef().AllOf.Add(MoveTemp(Term));
			return true;
		}

		bool FPipelineBinder::BuildFilterTerm(const FExpr* Expr, IR::FIRPassFilterTerm& OutTerm)
		{
			FString Callee;
			const FCallExpr* Call = ReadPipelineCall(Expr, Callee);
			if (!Call)
			{
				Diagnostics.Error(
					TEXT("DSH7314"),
					File,
					Expr->Span,
					LOCTEXT("FilterTermShape", "A filter term is 'Stencil(value)', 'Stencil(value, mask)', 'Layer(Name | ...)' or 'List(Name)'."));
				return false;
			}
			for (const FArgument& Argument : Call->Arguments)
			{
				if (!Argument.Name.IsEmpty() || Argument.PinIndex != INDEX_NONE)
				{
					Diagnostics.Error(
						TEXT("DSH7314"),
						File,
						Argument.Span,
						FText::Format(
							LOCTEXT("FilterNamedArgument", "'{0}(...)' takes its arguments by position."),
							FText::FromString(Callee)));
					return false;
				}
			}

			if (Callee.Equals(FilterStencil, ESearchCase::CaseSensitive))
			{
				if (Call->Arguments.Num() < 1 || Call->Arguments.Num() > 2)
				{
					Diagnostics.Error(
						TEXT("DSH7314"),
						File,
						Call->Span,
						LOCTEXT("StencilArity", "'Stencil' takes the stencil value and, optionally, the mask it is compared under: 'Stencil(1)', 'Stencil(4, 0x0F)'."));
					return false;
				}
				double Value = 1.0;
				double Mask = 255.0;
				if (!ReadNumber(Call->Arguments[0].Value.Get(), EPipelineValueSite::Pass, TEXT("Stencil"), /* bIntegral */ true, Value)
					|| (Call->Arguments.Num() == 2 && !ReadNumber(Call->Arguments[1].Value.Get(), EPipelineValueSite::Pass, TEXT("Stencil"), /* bIntegral */ true, Mask)))
				{
					return false;
				}
				if (Value < 0.0 || Value > 255.0 || Mask < 0.0 || Mask > 255.0)
				{
					Diagnostics.Error(
						TEXT("DSH7314"),
						File,
						Call->Span,
						LOCTEXT("StencilRange", "A stencil value and its mask are 0 to 255."));
					return false;
				}
				OutTerm.Kind = FilterStencil;
				OutTerm.StencilValue = static_cast<int32>(Value);
				OutTerm.StencilMask = static_cast<int32>(Mask);
				return true;
			}

			if (Callee.Equals(FilterLayer, ESearchCase::CaseSensitive) || Callee.Equals(FilterList, ESearchCase::CaseSensitive))
			{
				const bool bLayer = Callee.Equals(FilterLayer, ESearchCase::CaseSensitive);
				TArray<FString> Names;
				TArray<FLangSpan> Spans;
				const bool bShape = Call->Arguments.Num() == 1
					&& CollectBarWords(Call->Arguments[0].Value.Get(), /* bAllowStrings */ true, Names, Spans)
					&& (bLayer || Names.Num() == 1);
				bool bEmpty = false;
				for (const FString& Name : Names)
				{
					bEmpty |= Name.TrimStartAndEnd().IsEmpty();
				}
				if (!bShape || bEmpty)
				{
					Diagnostics.Error(
						TEXT("DSH7314"),
						File,
						Call->Span,
						bLayer
							? LOCTEXT("LayerShape", "'Layer' takes pass layer names joined by '|': 'Layer(Highlight)', 'Layer(Enemies | Allies)'.")
							: LOCTEXT("ListShape", "'List' takes one list name: 'List(Enemies)'."));
					return false;
				}
				if (bLayer)
				{
					OutTerm.Kind = FilterLayer;
					for (const FString& Name : Names)
					{
						OutTerm.Layers.AddUnique(Name.TrimStartAndEnd());
					}
				}
				else
				{
					OutTerm.Kind = FilterList;
					OutTerm.List = Names[0].TrimStartAndEnd();
				}
				return true;
			}

			Diagnostics.Error(
				TEXT("DSH7314"),
				File,
				Call->Callee->Span,
				FText::Format(
					LOCTEXT("FilterUnknownTerm", "'{0}' is not a filter term; a mesh pass selects by 'Stencil(...)', 'Layer(...)' and 'List(...)'."),
					FText::FromString(Callee)));
			return false;
		}

		// ---------------------------------------------------------------------------------- bindings

		void FPipelineBinder::BindPassBinding(IR::FIRPass& Pass, const FPassStmt& Statement)
		{
			const bool bWrite = Statement.StmtKind == EPassStmtKind::Write;

			IR::FIRPassBinding Binding;
			Binding.Slot = Statement.Name.IsEmpty() ? Statement.Buffer : Statement.Name;
			Binding.Buffer = Statement.Buffer;
			Binding.bPrevious = Statement.bPrevious;
			Binding.Source.File = File;
			Binding.Source.Span = Statement.Span;

			const IR::FIRPassBuffer* Declared = FindPayloadBuffer(Statement.Buffer);
			if (!Declared && !IsBuiltinBuffer(Statement.Buffer))
			{
				ReportUnknownBuffer(Statement.Buffer, Statement.BufferSpan);
			}
			else if (Statement.bPrevious)
			{
				if (bWrite)
				{
					Diagnostics.Error(
						TEXT("DSH7318"),
						File,
						Statement.PreviousSpan,
						FText::Format(
							LOCTEXT("WritePrevious", "'{0}.Previous' is last frame's contents, which nothing writes any more; write '{0}'."),
							FText::FromString(Statement.Buffer)));
				}
				else if (!Declared)
				{
					Diagnostics.Error(
						TEXT("DSH4415"),
						File,
						Statement.PreviousSpan,
						FText::Format(
							LOCTEXT("BuiltinPrevious", "'{0}.Previous': a built-in texture keeps no history."),
							FText::FromString(Statement.Buffer)));
				}
				else if (!Declared->bHistory)
				{
					Diagnostics.Error(
						TEXT("DSH4415"),
						File,
						Statement.PreviousSpan,
						FText::Format(
							LOCTEXT("PreviousWithoutHistory", "'{0}.Previous' reads last frame's '{0}', and '{0}' keeps none; declare it with 'History = true'."),
							FText::FromString(Statement.Buffer)));
				}
			}

			// A Depth32 buffer is a mesh pass's own depth, and the one other pass that may touch it is a `clear`, whose Value is
			// then the depth (DreamShaderPass, Render/DreamPassUtilityPasses.cpp, ExecuteClearPass). Nothing samples or draws
			// into it as a colour.
			const bool bClearWrite = bWrite && Pass.Kind.Equals(PassKinds[Kind::Clear], ESearchCase::CaseSensitive);
			if (Declared && IsDepthFormat(Declared->Format) && !bClearWrite)
			{
				Diagnostics.Error(
					TEXT("DSH7330"),
					File,
					Statement.BufferSpan,
					FText::Format(
						LOCTEXT("DepthBoundMeshOrClear", "'{0}' is a Depth32 buffer, which a mesh pass tests against as 'Depth = Own({0})' and a clear pass resets ('write {0};', with 'Value' the depth); a {2} pass cannot '{1}' it."),
						FText::FromString(Statement.Buffer),
						FText::FromString(bWrite ? TEXT("write") : TEXT("read")),
						FText::FromString(Pass.Kind)));
			}

			(bWrite ? Pass.Writes : Pass.Reads).Add(MoveTemp(Binding));
		}

		bool FPipelineBinder::MentionsRuntimeValue(const FExpr* Expr) const
		{
			if (!Expr)
			{
				return false;
			}
			switch (Expr->Kind)
			{
			case ENodeKind::IdentifierExpr:
			{
				const FString& Name = static_cast<const FIdentifierExpr*>(Expr)->Name;
				return Name.Equals(WeightParameterName, ESearchCase::CaseSensitive) || FindParameterGlobal(Name) != INDEX_NONE;
			}
			case ENodeKind::ParenExpr:
				return MentionsRuntimeValue(static_cast<const FParenExpr*>(Expr)->Inner.Get());
			case ENodeKind::UnaryExpr:
				return MentionsRuntimeValue(static_cast<const FUnaryExpr*>(Expr)->Operand.Get());
			case ENodeKind::BinaryExpr:
			{
				const FBinaryExpr* Binary = static_cast<const FBinaryExpr*>(Expr);
				return MentionsRuntimeValue(Binary->Left.Get()) || MentionsRuntimeValue(Binary->Right.Get());
			}
			case ENodeKind::CallExpr:
			{
				for (const FArgument& Argument : static_cast<const FCallExpr*>(Expr)->Arguments)
				{
					if (MentionsRuntimeValue(Argument.Value.Get()))
					{
						return true;
					}
				}
				return false;
			}
			case ENodeKind::ConditionalExpr:
			{
				const FConditionalExpr* Conditional = static_cast<const FConditionalExpr*>(Expr);
				return MentionsRuntimeValue(Conditional->Condition.Get()) || MentionsRuntimeValue(Conditional->TrueValue.Get()) || MentionsRuntimeValue(Conditional->FalseValue.Get());
			}
			case ENodeKind::CastExpr:
				return MentionsRuntimeValue(static_cast<const FCastExpr*>(Expr)->Operand.Get());
			case ENodeKind::MemberExpr:
				return MentionsRuntimeValue(static_cast<const FMemberExpr*>(Expr)->Object.Get());
			case ENodeKind::IndexExpr:
				return MentionsRuntimeValue(static_cast<const FIndexExpr*>(Expr)->Object.Get());
			default:
				return false;
			}
		}

		bool FPipelineBinder::CheckParamNames(const FExpr* Expr, const bool bCallee)
		{
			if (!Expr)
			{
				return true;
			}
			switch (Expr->Kind)
			{
			case ENodeKind::IdentifierExpr:
			{
				const FIdentifierExpr* Identifier = static_cast<const FIdentifierExpr*>(Expr);
				if (bCallee || Identifier->Name.Equals(WeightParameterName, ESearchCase::CaseSensitive) || Bound.FindGlobal(Identifier->Name) != INDEX_NONE)
				{
					return true;
				}
				TArray<FString> Candidates;
				for (const FBoundGlobal& Global : Bound.Globals)
				{
					Candidates.Add(Global.Name);
				}
				Candidates.Add(WeightParameterName);
				const FString Suggestion = FLangBinder::SuggestCaseInsensitive(Identifier->Name, Candidates);
				Diagnostics.Error(
					TEXT("DSH4404"),
					File,
					Identifier->Span,
					Suggestion.IsEmpty()
						? FText::Format(LOCTEXT("ParamUnknownName", "'{0}' is not a 'uniform' or 'static const' of this pipeline, nor 'DreamPassWeight'."), FText::FromString(Identifier->Name))
						: FText::Format(LOCTEXT("ParamUnknownNameDidYouMean", "'{0}' is not a 'uniform' or 'static const' of this pipeline; did you mean '{1}'? Names are case-sensitive."), FText::FromString(Identifier->Name), FText::FromString(Suggestion)));
				return false;
			}
			case ENodeKind::ParenExpr:
				return CheckParamNames(static_cast<const FParenExpr*>(Expr)->Inner.Get(), false);
			case ENodeKind::UnaryExpr:
				return CheckParamNames(static_cast<const FUnaryExpr*>(Expr)->Operand.Get(), false);
			case ENodeKind::BinaryExpr:
			{
				const FBinaryExpr* Binary = static_cast<const FBinaryExpr*>(Expr);
				const bool bLeft = CheckParamNames(Binary->Left.Get(), false);
				const bool bRight = CheckParamNames(Binary->Right.Get(), false);
				return bLeft && bRight;
			}
			case ENodeKind::CallExpr:
			{
				const FCallExpr* Call = static_cast<const FCallExpr*>(Expr);
				bool bOk = CheckParamNames(Call->Callee.Get(), true);
				for (const FArgument& Argument : Call->Arguments)
				{
					bOk &= CheckParamNames(Argument.Value.Get(), false);
				}
				return bOk;
			}
			case ENodeKind::ConditionalExpr:
			{
				const FConditionalExpr* Conditional = static_cast<const FConditionalExpr*>(Expr);
				const bool bCondition = CheckParamNames(Conditional->Condition.Get(), false);
				const bool bTrue = CheckParamNames(Conditional->TrueValue.Get(), false);
				const bool bFalse = CheckParamNames(Conditional->FalseValue.Get(), false);
				return bCondition && bTrue && bFalse;
			}
			case ENodeKind::CastExpr:
				return CheckParamNames(static_cast<const FCastExpr*>(Expr)->Operand.Get(), false);
			case ENodeKind::MemberExpr:
				return CheckParamNames(static_cast<const FMemberExpr*>(Expr)->Object.Get(), false);
			case ENodeKind::IndexExpr:
			{
				const FIndexExpr* Index = static_cast<const FIndexExpr*>(Expr);
				const bool bObject = CheckParamNames(Index->Object.Get(), false);
				const bool bIndex = CheckParamNames(Index->Index.Get(), false);
				return bObject && bIndex;
			}
			default:
				return true;
			}
		}

		bool FPipelineBinder::FoldParam(const FExpr* Expr, FPipelineParamForm& OutForm)
		{
			Expr = PipelineUnparen(Expr);
			if (!Expr)
			{
				return false;
			}

			// No parameter and no weight in it: a compile-time value, folded the way an initializer is.
			if (!MentionsRuntimeValue(Expr))
			{
				double Value[4] = { 0.0, 0.0, 0.0, 0.0 };
				int32 Components = 1;
				IR::FIRType Type;
				const EFold Fold = FoldConstant(*Expr, Value, Components, Type);
				if (Fold == EFold::Reported)
				{
					return false;
				}
				if (Fold == EFold::NotConstant)
				{
					Diagnostics.Error(
						TEXT("DSH7352"),
						File,
						Expr->Span,
						LOCTEXT("ParamNotFoldable", "A 'param' is a parameter of the pipeline (times a number, plus a number), 'DreamPassWeight' (the same), or a value the compiler can fold, and this is none of them."));
					return false;
				}
				OutForm = FPipelineParamForm();
				OutForm.bConstant = true;
				OutForm.Components = FMath::Clamp(Type.GraphComponentCount(), 1, 4);
				for (int32 Index = 0; Index < 4; ++Index)
				{
					OutForm.Value[Index] = Value[Index];
				}
				OutForm.ConstantType = PipelineConstantTypeSpelling(Type);
				return true;
			}

			// A run-time value, which has to keep the one shape the asset can hold: source * Multiplier + Offset.
			if (const FIdentifierExpr* Identifier = Expr->As<FIdentifierExpr>())
			{
				OutForm = FPipelineParamForm();
				if (Identifier->Name.Equals(WeightParameterName, ESearchCase::CaseSensitive))
				{
					OutForm.SourceKind = TEXT("Weight");
					OutForm.ParameterType = TEXT("float");
					return true;
				}
				const int32 GlobalIndex = FindParameterGlobal(Identifier->Name);
				const FBoundGlobal& Global = Bound.Globals[GlobalIndex];
				if (!IsPipelineParameterType(Global.Type))
				{
					// Said where it was declared (DSH7320).
					return false;
				}
				OutForm.SourceKind = TEXT("Parameter");
				OutForm.Parameter = Identifier->Name;
				OutForm.ParameterType = Global.Type.ToString();
				return true;
			}

			const auto RefuseShape = [this, Expr]()
			{
				Diagnostics.Error(
					TEXT("DSH7352"),
					File,
					Expr->Span,
					LOCTEXT("ParamNotAffine", "A 'param' over a parameter or 'DreamPassWeight' keeps the shape 'Source * a + b', with a and b numbers the compiler can fold; this expression has another shape. Compute it in the shader or the material instead."));
				return false;
			};

			const auto ScalarOf = [this](const FExpr* Operand, double& OutScalar) -> int32
			{
				// 1: a scalar constant, 0: not constant (or not one number), -1: already reported.
				if (MentionsRuntimeValue(Operand))
				{
					return 0;
				}
				double Value[4] = { 0.0, 0.0, 0.0, 0.0 };
				int32 Components = 1;
				IR::FIRType Type;
				const EFold Fold = FoldConstant(*Operand, Value, Components, Type);
				if (Fold == EFold::Reported)
				{
					return -1;
				}
				if (Fold == EFold::NotConstant || Type.GraphComponentCount() != 1)
				{
					return 0;
				}
				OutScalar = Value[0];
				return 1;
			};

			if (const FUnaryExpr* Unary = Expr->As<FUnaryExpr>())
			{
				if (Unary->Op != EUnaryOp::Negate && Unary->Op != EUnaryOp::Plus)
				{
					return RefuseShape();
				}
				if (!FoldParam(Unary->Operand.Get(), OutForm))
				{
					return false;
				}
				if (Unary->Op == EUnaryOp::Negate)
				{
					OutForm.Multiplier = -OutForm.Multiplier;
					OutForm.Offset = -OutForm.Offset;
				}
				return true;
			}

			if (const FBinaryExpr* Binary = Expr->As<FBinaryExpr>())
			{
				const bool bLeftRuntime = MentionsRuntimeValue(Binary->Left.Get());
				const bool bRightRuntime = MentionsRuntimeValue(Binary->Right.Get());
				if (bLeftRuntime && bRightRuntime)
				{
					return RefuseShape();
				}
				const FExpr* Runtime = bLeftRuntime ? Binary->Left.Get() : Binary->Right.Get();
				const FExpr* Constant = bLeftRuntime ? Binary->Right.Get() : Binary->Left.Get();

				double Scalar = 0.0;
				const int32 ScalarState = ScalarOf(Constant, Scalar);
				if (ScalarState < 0)
				{
					return false;
				}
				if (ScalarState == 0)
				{
					Diagnostics.Error(
						TEXT("DSH7353"),
						File,
						Constant->Span,
						LOCTEXT("ParamOperandNotScalar", "What scales or offsets a parameter in a 'param' is one number the compiler can fold; a vector, or a value known only at run time, has no place in the asset's 'Source * a + b'."));
					return false;
				}
				if (!FoldParam(Runtime, OutForm))
				{
					return false;
				}

				switch (Binary->Op)
				{
				case EBinaryOp::Add:
					OutForm.Offset += Scalar;
					return true;
				case EBinaryOp::Subtract:
					if (bLeftRuntime)
					{
						OutForm.Offset -= Scalar;
					}
					else
					{
						// c - (s * a + b) = s * (-a) + (c - b)
						OutForm.Multiplier = -OutForm.Multiplier;
						OutForm.Offset = Scalar - OutForm.Offset;
					}
					return true;
				case EBinaryOp::Multiply:
					OutForm.Multiplier *= Scalar;
					OutForm.Offset *= Scalar;
					return true;
				case EBinaryOp::Divide:
					if (!bLeftRuntime || Scalar == 0.0)
					{
						return RefuseShape();
					}
					OutForm.Multiplier /= Scalar;
					OutForm.Offset /= Scalar;
					return true;
				default:
					return RefuseShape();
				}
			}

			return RefuseShape();
		}

		void FPipelineBinder::BindPassParam(IR::FIRPass& Pass, const FPassStmt& Statement)
		{
			if (!CheckParamNames(Statement.Value.Get(), false))
			{
				return;
			}

			FPipelineParamForm Form;
			if (!FoldParam(Statement.Value.Get(), Form))
			{
				return;
			}

			IR::FIRPassParam Param;
			Param.Target = Statement.Name;
			Param.Source.File = File;
			Param.Source.Span = Statement.Span;

			if (Form.bConstant)
			{
				Param.SourceKind = TEXT("Constant");
				Param.ConstantType = Form.ConstantType;
				if (Form.ConstantType.Equals(TEXT("int"), ESearchCase::CaseSensitive))
				{
					Param.Constant = IR::FIRPropertyValue::MakeInt(static_cast<int64>(FMath::RoundToDouble(Form.Value[0])));
				}
				else if (Form.ConstantType.Equals(TEXT("bool"), ESearchCase::CaseSensitive))
				{
					Param.Constant = IR::FIRPropertyValue::MakeBool(Form.Value[0] != 0.0);
				}
				else
				{
					Param.Constant = IR::FIRPropertyValue::MakeFloat4(Form.Value, Form.Components);
				}
			}
			else
			{
				const bool bScaled = !SameFloat(Form.Multiplier, 1.0) || !SameFloat(Form.Offset, 0.0);
				const bool bNumber = Form.ParameterType.StartsWith(TEXT("float"), ESearchCase::CaseSensitive)
					|| Form.ParameterType.Equals(TEXT("int"), ESearchCase::CaseSensitive);
				if (bScaled && !bNumber)
				{
					Diagnostics.Error(
						TEXT("DSH7353"),
						File,
						Statement.Value->Span,
						FText::Format(
							LOCTEXT("ParamScaledNotNumber", "'{0}' is a {1}, which is passed on as it is; only a number can be scaled or offset."),
							FText::FromString(Form.Parameter),
							FText::FromString(Form.ParameterType)));
					return;
				}
				Param.SourceKind = Form.SourceKind;
				Param.Parameter = Form.Parameter;
				Param.Multiplier = Form.Multiplier;
				Param.Offset = Form.Offset;
			}

			Pass.Params.Add(MoveTemp(Param));
		}

		// ------------------------------------------------------------------------------- one pass done

		void FPipelineBinder::FinishPass(IR::FIRPass& Pass, const int32 KindIndex, const FPassDecl& Decl, const FPipelinePassKeys& Keys)
		{
			if (!Pass.bInjectionWritten)
			{
				Pass.Injection = Payload.DefaultInjection;
			}
			if (KindIndex == INDEX_NONE)
			{
				return;
			}

			const FLangSpan NameSpan = Decl.NameSpan;
			const auto FindWriteStatement = [&Decl](const int32 WriteIndex) -> const FPassStmt*
			{
				int32 Seen = 0;
				for (const TUniquePtr<FPassStmt>& Statement : Decl.Statements)
				{
					if (Statement && Statement->StmtKind == EPassStmtKind::Write && Seen++ == WriteIndex)
					{
						return Statement.Get();
					}
				}
				return nullptr;
			};

			// An `hlsl` block is the code of a pass that runs HLSL: compute, or fullscreen without a material (said below).
			if (Keys.Hlsl && KindIndex != Kind::Fullscreen && KindIndex != Kind::Compute)
			{
				Diagnostics.Error(
					TEXT("DSH7370"),
					File,
					Keys.Hlsl->NameSpan,
					FText::Format(
						LOCTEXT("HlslBlockInKind", "A {0} pass runs no HLSL of its own, so pass '{1}' cannot hold an 'hlsl' block."),
						FText::FromString(PassKinds[KindIndex]),
						FText::FromString(Pass.Name)));
			}

			switch (KindIndex)
			{
			case Kind::Fullscreen:
			{
				const bool bMaterial = !Pass.MaterialReference.IsEmpty();
				const bool bShader = !Pass.ShaderReference.IsEmpty();
				if (bMaterial && bShader)
				{
					Diagnostics.Error(
						TEXT("DSH7316"),
						File,
						Keys.SpanOf(TEXT("Shader"), NameSpan),
						FText::Format(
							LOCTEXT("FullscreenMaterialAndShader", "Fullscreen pass '{0}' draws a 'Material' or runs a 'Shader', and it names both."),
							FText::FromString(Pass.Name)));
				}
				else if (bMaterial && Keys.Hlsl)
				{
					Diagnostics.Error(
						TEXT("DSH7370"),
						File,
						Keys.Hlsl->NameSpan,
						FText::Format(
							LOCTEXT("HlslBlockWithMaterial", "Fullscreen pass '{0}' draws a material, and an 'hlsl' block is the code of a pass that runs HLSL of its own; remove the block, or 'Material' to run it."),
							FText::FromString(Pass.Name)));
				}
				if (!bMaterial)
				{
					// The pass runs HLSL: where its code is, and that it is somewhere (DSH7315 when it is nowhere).
					ResolveHlslSource(Pass, KindIndex, Keys, NameSpan);
				}
				if (bMaterial && !Pass.Entry.IsEmpty())
				{
					Diagnostics.Error(
						TEXT("DSH7316"),
						File,
						Keys.SpanOf(TEXT("Entry"), NameSpan),
						FText::Format(
							LOCTEXT("EntryWithMaterial", "'Entry' names a function of a shader file, and fullscreen pass '{0}' draws a material."),
							FText::FromString(Pass.Name)));
				}

				if (bMaterial)
				{
					if (Pass.Writes.Num() != 1)
					{
						Diagnostics.Error(
							TEXT("DSH7317"),
							File,
							NameSpan,
							FText::Format(
								LOCTEXT("FullscreenMaterialOneWrite", "A fullscreen material pass writes exactly one buffer, as 'write Buffer;', and '{0}' writes {1}."),
								FText::FromString(Pass.Name),
								FText::AsNumber(Pass.Writes.Num())));
					}
					else if (const FPassStmt* Write = FindWriteStatement(0); Write && !Write->Name.IsEmpty())
					{
						Diagnostics.Error(
							TEXT("DSH7317"),
							File,
							Write->NameSpan,
							FText::Format(
								LOCTEXT("FullscreenMaterialWriteSlot", "A material has one output and no name for it; write 'write {0};'."),
								FText::FromString(Write->Buffer)));
					}
					if (Pass.Reads.Num() > MaterialInputSlots)
					{
						Diagnostics.Error(
							TEXT("DSH7336"),
							File,
							Pass.Reads[MaterialInputSlots].Source.Span,
							FText::Format(
								LOCTEXT("FullscreenMaterialTooManyReads", "Fullscreen material pass '{0}' reads {1} buffers, and a post-process material has {2} input slots, shared with its own SceneTexture nodes; split the pass in two, or write it with 'Shader =' (a '.usf' pass reads up to {3})."),
								FText::FromString(Pass.Name),
								FText::AsNumber(Pass.Reads.Num()),
								FText::AsNumber(MaterialInputSlots),
								FText::AsNumber(MaxSlotInputs)));
					}
				}
				else if (IsHlslPass(Pass))
				{
					if (Pass.Writes.Num() == 0)
					{
						Diagnostics.Error(
							TEXT("DSH7317"),
							File,
							NameSpan,
							FText::Format(
								LOCTEXT("FullscreenShaderNoWrite", "Fullscreen pass '{0}' writes nothing; a pass draws into at least one buffer, 'write Result = Buffer;'."),
								FText::FromString(Pass.Name)));
					}
					// A `.usf` always runs in the pixel slot, whatever its output count (no material is ever generated around it).
					if (Pass.Writes.Num() > MaxSlotOutputs || Pass.Reads.Num() > MaxSlotInputs)
					{
						Diagnostics.Error(
							TEXT("DSH7346"),
							File,
							NameSpan,
							FText::Format(
								LOCTEXT("FullscreenShaderTooMany", "Fullscreen pass '{0}' reads {1} and writes {2} buffers, and the pixel slot a '.usf' pass runs in has {3} inputs and {4} outputs."),
								FText::FromString(Pass.Name),
								FText::AsNumber(Pass.Reads.Num()),
								FText::AsNumber(Pass.Writes.Num()),
								FText::AsNumber(MaxSlotInputs),
								FText::AsNumber(MaxSlotOutputs)));
					}
				}
				break;
			}

			case Kind::Compute:
			{
				ResolveHlslSource(Pass, KindIndex, Keys, NameSpan);
				if (Pass.Writes.Num() == 0)
				{
					Diagnostics.Error(
						TEXT("DSH7317"),
						File,
						NameSpan,
						FText::Format(
							LOCTEXT("ComputeNoWrite", "Compute pass '{0}' writes nothing; a compute pass writes at least one buffer, 'write Result = Buffer;'."),
							FText::FromString(Pass.Name)));
				}
				if (Pass.Writes.Num() > MaxSlotOutputs || Pass.Reads.Num() > MaxSlotInputs)
				{
					Diagnostics.Error(
						TEXT("DSH7346"),
						File,
						NameSpan,
						FText::Format(
							LOCTEXT("ComputeTooMany", "Compute pass '{0}' reads {1} and writes {2} buffers, and a compute slot has {3} inputs and {4} outputs."),
							FText::FromString(Pass.Name),
							FText::AsNumber(Pass.Reads.Num()),
							FText::AsNumber(Pass.Writes.Num()),
							FText::AsNumber(MaxSlotInputs),
							FText::AsNumber(MaxSlotOutputs)));
				}
				if (!Keys.Find(TEXT("Dispatch")))
				{
					// One thread per texel of the first write.
					Pass.DispatchMode = TEXT("Buffer");
					Pass.DispatchBuffer = DefaultDispatchBuffer(Pass);
					Pass.DispatchScale = 1.0;
				}
				break;
			}

			case Kind::Mesh:
			{
				if (Pass.Filter.Num() == 0 && !Keys.Find(TEXT("Filter")))
				{
					Diagnostics.Error(
						TEXT("DSH7315"),
						File,
						NameSpan,
						FText::Format(
							LOCTEXT("MeshNoFilter", "Mesh pass '{0}' needs 'Filter = ...' to say which primitives it draws: 'Stencil(1)', 'Layer(Name)', 'List(Name)'."),
							FText::FromString(Pass.Name)));
				}
				if (Pass.MeshMode.IsEmpty())
				{
					Pass.MeshMode = DefaultMeshMode(Pass);
				}
				if (!Pass.MeshMode.Equals(TEXT("Own"), ESearchCase::CaseSensitive) && Pass.MaterialReference.IsEmpty())
				{
					Diagnostics.Error(
						TEXT("DSH7315"),
						File,
						Keys.SpanOf(TEXT("Mode"), NameSpan),
						FText::Format(
							LOCTEXT("MeshModeNeedsMaterial", "'Mode = {0}' draws with the pass's own material, and mesh pass '{1}' names none; add 'Material = \"...\"', or write 'Mode = Own'."),
							FText::FromString(Pass.MeshMode),
							FText::FromString(Pass.Name)));
				}
				if (Pass.MeshMode.Equals(TEXT("Own"), ESearchCase::CaseSensitive) && !Pass.MaterialReference.IsEmpty())
				{
					Diagnostics.Warning(
						TEXT("DSH7359"),
						File,
						Keys.SpanOf(TEXT("Material"), NameSpan),
						FText::Format(
							LOCTEXT("MeshOwnIgnoresMaterial", "'Mode = Own' draws every primitive with its own material, so the 'Material' of mesh pass '{0}' is never used."),
							FText::FromString(Pass.Name)));
				}
				if (Pass.Nanite.IsEmpty() || !Keys.Find(TEXT("Nanite")))
				{
					Pass.Nanite = DefaultNanitePolicy(Pass);
				}
				else if (Pass.Nanite.Equals(TEXT("StencilMask"), ESearchCase::CaseSensitive) && !FilterUsesStencil(Pass))
				{
					ReportBadValue(EPipelineValueSite::Pass, Keys.SpanOf(TEXT("Nanite"), NameSpan), FText::Format(
						LOCTEXT("StencilMaskWithoutStencil", "'Nanite = StencilMask' writes where CustomStencil matches the filter's 'Stencil(...)', and the filter of '{0}' has none; use 'AssignStencil(n)' for layers and lists."),
						FText::FromString(Pass.Name)));
				}

				if (Pass.Reads.Num() > 0)
				{
					Diagnostics.Error(
						TEXT("DSH7317"),
						File,
						Pass.Reads[0].Source.Span,
						FText::Format(
							LOCTEXT("MeshReads", "Mesh pass '{0}' cannot read a buffer: a mesh pass binds no input, and its material samples what it needs itself."),
							FText::FromString(Pass.Name)));
				}
				if (Pass.Writes.Num() == 0 || Pass.Writes.Num() > MaxMeshOutputs)
				{
					Diagnostics.Error(
						TEXT("DSH7317"),
						File,
						NameSpan,
						FText::Format(
							LOCTEXT("MeshWrites", "Mesh pass '{0}' writes {1} buffers, and a mesh pass writes one to four, 'write Output0 = Buffer;' for each output of UE.DreamPassOutput it fills."),
							FText::FromString(Pass.Name),
							FText::AsNumber(Pass.Writes.Num())));
				}
				for (int32 Index = 0; Index < Pass.Writes.Num(); ++Index)
				{
					const FPassStmt* Write = FindWriteStatement(Index);
					const FString& Slot = Pass.Writes[Index].Slot;
					const bool bNamed = Write && !Write->Name.IsEmpty();
					bool bOutputSlot = false;
					for (int32 Output = 0; Output < MaxMeshOutputs; ++Output)
					{
						bOutputSlot |= Slot.Equals(FString::Printf(TEXT("Output%d"), Output), ESearchCase::CaseSensitive);
					}
					if (!bNamed || !bOutputSlot)
					{
						Diagnostics.Error(
							TEXT("DSH7317"),
							File,
							Pass.Writes[Index].Source.Span,
							FText::Format(
								LOCTEXT("MeshWriteSlot", "A mesh pass names the output of UE.DreamPassOutput each write takes: 'write Output0 = {0};' (Output0 to Output3)."),
								FText::FromString(Pass.Writes[Index].Buffer)));
					}
					// The runtime skips such a pass every frame (Render/DreamPassMesh.cpp, ParseWrites).
					const IR::FIRPassBuffer* Declared = FindPayloadBuffer(Pass.Writes[Index].Buffer);
					if (Declared && IsIntegerFormat(Declared->Format))
					{
						Diagnostics.Error(
							TEXT("DSH7317"),
							File,
							Pass.Writes[Index].Source.Span,
							FText::Format(
								LOCTEXT("MeshWriteInteger", "Mesh pass '{0}' writes '{1}', a {2} buffer, and a mesh pass writes the float4 outputs of UE.DreamPassOutput, which an integer target would take as raw bits; write a float buffer."),
								FText::FromString(Pass.Name),
								FText::FromString(Declared->Name),
								FText::FromString(Declared->Format)));
					}
				}
				break;
			}

			case Kind::Clear:
			case Kind::Copy:
			{
				const bool bClear = KindIndex == Kind::Clear;
				const int32 WantedReads = bClear ? 0 : 1;
				if (Pass.Reads.Num() != WantedReads || Pass.Writes.Num() != 1)
				{
					Diagnostics.Error(
						TEXT("DSH7317"),
						File,
						NameSpan,
						bClear
							? FText::Format(LOCTEXT("ClearShape", "Clear pass '{0}' writes one buffer and reads none: 'write Buffer;' and 'Value = ...;'."), FText::FromString(Pass.Name))
							: FText::Format(LOCTEXT("CopyShape", "Copy pass '{0}' reads one buffer and writes one: 'read Source;' and 'write Target;'."), FText::FromString(Pass.Name)));
				}
				if (Pass.Params.Num() > 0)
				{
					Diagnostics.Warning(
						TEXT("DSH7359"),
						File,
						Pass.Params[0].Source.Span,
						FText::Format(
							LOCTEXT("UtilityParams", "A {0} pass has no shader or material to give a 'param' to."),
							FText::FromString(PassKinds[KindIndex])));
				}
				break;
			}

			default:
				break;
			}

			if (KindIndex == Kind::Mesh && EffectiveMeshMode(Pass).Equals(TEXT("Own"), ESearchCase::CaseSensitive) && Pass.Params.Num() > 0)
			{
				Diagnostics.Warning(
					TEXT("DSH7359"),
					File,
					Pass.Params[0].Source.Span,
					FText::Format(
						LOCTEXT("MeshOwnParams", "Mesh pass '{0}' draws every primitive with its own material, so its 'param' lines reach no material."),
						FText::FromString(Pass.Name)));
			}

			// The names a pass binds, unique per kind of binding -- and, in a `.usf`, across all three, where every one
			// of them becomes a `#define` of the slot's registry.
			const bool bHlsl = IsHlslPass(Pass);
			TArray<FString> Names;
			// A `.usf` pass's names become #defines, which HLSL tells apart by case; a material pass's are matched to the
			// material's UserSceneTexture inputs and parameters as Unreal names, which ignore it -- `read Mask` and
			// `read mask` would bind one input twice there.
			const auto CheckUnique = [this, &Names, &Pass, bHlsl](const FString& Name, const FLangSpan& Span)
			{
				const FString* Earlier = Names.FindByPredicate([&Name, bHlsl](const FString& Candidate)
				{
					return Candidate.Equals(Name, bHlsl ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase);
				});
				if (Earlier)
				{
					Diagnostics.Error(
						TEXT("DSH7319"),
						File,
						Span,
						Earlier->Equals(Name, ESearchCase::CaseSensitive)
							? FText::Format(
								LOCTEXT("SlotTwice", "'{0}' is bound twice in pass '{1}'; inside a pass every input, output and parameter has a name of its own."),
								FText::FromString(Name),
								FText::FromString(Pass.Name))
							: FText::Format(
								LOCTEXT("SlotTwiceCase", "'{0}' and '{1}' differ in case only, and pass '{2}' matches them to its material's inputs and parameters as Unreal names, which ignore case: give them names of their own."),
								FText::FromString(*Earlier),
								FText::FromString(Name),
								FText::FromString(Pass.Name)));
					return;
				}
				Names.Add(Name);
			};
			for (const IR::FIRPassBinding& Binding : Pass.Reads)
			{
				CheckUnique(Binding.Slot, Binding.Source.Span);
			}
			if (!bHlsl)
			{
				Names.Reset();
			}
			const bool bMaterialWrite = KindIndex == Kind::Fullscreen && !Pass.MaterialReference.IsEmpty();
			for (const IR::FIRPassBinding& Binding : Pass.Writes)
			{
				if (!bMaterialWrite)
				{
					CheckUnique(Binding.Slot, Binding.Source.Span);
				}
			}
			if (!bHlsl)
			{
				Names.Reset();
			}
			for (const IR::FIRPassParam& Param : Pass.Params)
			{
				CheckUnique(Param.Target, Param.Source.Span);
			}

			// In a `.usf` pass the slot's registry #defines more than the bound names (DreamShaderCompiler,
			// Pass/DreamShaderPassSlotRegistry.cpp, BuildSlotSection): a read also as <Name>Size and <Name>UVRect, a write as
			// <Name>Size, the Entry as the slot's fixed entry point, and `View` at BeginView; DP_ names are the slot's own
			// parameters. A name that meets one of those fails the slot when the pipeline compiles, so it is refused here too,
			// where a check without a compile sees it. Two bound names that meet were said above.
			if (bHlsl)
			{
				struct FSlotMacro
				{
					FString Name;
					FLangSpan Span;
					/** Written by the pass, as opposed to derived from a written one by the registry. */
					bool bBound = true;
				};
				TArray<FSlotMacro> Macros;
				for (const IR::FIRPassBinding& Binding : Pass.Reads)
				{
					Macros.Add({ Binding.Slot, Binding.Source.Span, true });
					Macros.Add({ Binding.Slot + TEXT("Size"), Binding.Source.Span, false });
					Macros.Add({ Binding.Slot + TEXT("UVRect"), Binding.Source.Span, false });
				}
				for (const IR::FIRPassBinding& Binding : Pass.Writes)
				{
					// A pixel pass writes SV_Target<j>: only the output's size has a name of the slot's.
					if (KindIndex == Kind::Compute)
					{
						Macros.Add({ Binding.Slot, Binding.Source.Span, true });
					}
					Macros.Add({ Binding.Slot + TEXT("Size"), Binding.Source.Span, false });
				}
				for (const IR::FIRPassParam& Param : Pass.Params)
				{
					Macros.Add({ Param.Target, Param.Source.Span, true });
				}

				// Every binding once, by the name it writes; the names derived from a DP_ name start with DP_ as well.
				const auto CheckReserved = [this, &Pass](const FString& Name, const FLangSpan& Span)
				{
					if (Name.StartsWith(TEXT("DP_"), ESearchCase::CaseSensitive))
					{
						Diagnostics.Error(
							TEXT("DSH7319"),
							File,
							Span,
							FText::Format(
								LOCTEXT("SlotNameReserved", "'{0}' is a name of pass '{1}' in its HLSL slot, and names starting with DP_ are the slot's own parameters (DreamPass.ush); rename the binding."),
								FText::FromString(Name),
								FText::FromString(Pass.Name)));
					}
				};
				for (const IR::FIRPassBinding& Binding : Pass.Reads)
				{
					CheckReserved(Binding.Slot, Binding.Source.Span);
				}
				for (const IR::FIRPassBinding& Binding : Pass.Writes)
				{
					CheckReserved(Binding.Slot, Binding.Source.Span);
				}
				for (const IR::FIRPassParam& Param : Pass.Params)
				{
					CheckReserved(Param.Target, Param.Source.Span);
				}

				// The body form's own names (DreamShader_Plan/10 3.2), the parameters and locals of the function the compiler
				// writes around the block: a binding of one of them would be a #define over it.
				if (Pass.HlslSource.Equals(IR::PassHlslSource::Body, ESearchCase::CaseSensitive))
				{
					const TConstArrayView<const TCHAR*> BodyNames = GetDreamPassBodyFormNames(KindIndex == Kind::Compute);
					const FString NameList = JoinNames(BodyNames);
					const auto CheckBodyName = [this, &Pass, BodyNames, &NameList](const FString& Name, const FLangSpan& Span)
					{
						if (HasName(BodyNames, Name))
						{
							Diagnostics.Error(
								TEXT("DSH7367"),
								File,
								Span,
								FText::Format(
									LOCTEXT("BodyFormName", "'{0}' is a name of pass '{1}', and its 'hlsl' block holds the statements of a function the compiler writes, which names its own values {2}; rename the binding."),
									FText::FromString(Name),
									FText::FromString(Pass.Name),
									FText::FromString(NameList)));
						}
					};
					for (const IR::FIRPassBinding& Binding : Pass.Reads)
					{
						CheckBodyName(Binding.Slot, Binding.Source.Span);
					}
					for (const IR::FIRPassBinding& Binding : Pass.Writes)
					{
						CheckBodyName(Binding.Slot, Binding.Source.Span);
					}
					for (const IR::FIRPassParam& Param : Pass.Params)
					{
						CheckBodyName(Param.Target, Param.Source.Span);
					}
				}

				const FString MainEntry = KindIndex == Kind::Compute ? FString(TEXT("DreamPassMainCS")) : FString(TEXT("DreamPassMainPS"));
				const bool bAtBeginView = InjectionIndexOf(Pass) == Injection::BeginView;
				for (int32 Index = 0; Index < Macros.Num(); ++Index)
				{
					const FSlotMacro& Macro = Macros[Index];
					if (Macro.Name.StartsWith(TEXT("DP_"), ESearchCase::CaseSensitive))
					{
						// Said by CheckReserved, once per binding.
						continue;
					}
					FText Clash;
					if (Macro.Name.Equals(MainEntry, ESearchCase::CaseSensitive)
						|| (!Pass.Entry.IsEmpty() && Macro.Name.Equals(Pass.Entry, ESearchCase::CaseSensitive)))
					{
						Clash = LOCTEXT("SlotNameEntry", "'{0}' is a name of pass '{1}' in its HLSL slot, and so is its entry point: the slot's registry renames the Entry to the slot's own entry function with a #define of that name; rename the binding.");
					}
					else if (bAtBeginView && Macro.Name.Equals(TEXT("View"), ESearchCase::CaseSensitive))
					{
						Clash = LOCTEXT("SlotNameView", "'{0}' is a name of pass '{1}' in its HLSL slot, and at BeginView the slot's registry defines 'View' itself, so that a use of the view uniform buffer, which does not exist yet there, fails to compile; rename the binding.");
					}
					else
					{
						for (int32 Earlier = 0; Earlier < Index && Clash.IsEmpty(); ++Earlier)
						{
							// One side derived: two bound names were said above, and two derived ones meet only when their bound names do.
							if (Macros[Earlier].bBound != Macro.bBound && Macros[Earlier].Name.Equals(Macro.Name, ESearchCase::CaseSensitive))
							{
								Clash = LOCTEXT("SlotNameDerived", "'{0}' is a name of pass '{1}' in its HLSL slot twice: the slot's registry names a read's size and UV rect <Name>Size and <Name>UVRect, and a write's size <Name>Size, beside the names the pass binds; rename one of them.");
							}
						}
					}
					if (!Clash.IsEmpty())
					{
						Diagnostics.Error(
							TEXT("DSH7319"),
							File,
							Macro.Span,
							FText::Format(Clash, FText::FromString(Macro.Name), FText::FromString(Pass.Name)));
					}
				}
			}

			// V8: what fits the slot's parameter block (DP_Params, 16 float4), by the runtime's own packing rule.
			if (IsSlotPass(Pass) && Pass.Params.Num() > 0)
			{
				TArray<int32> Widths;
				for (const IR::FIRPassParam& Param : Pass.Params)
				{
					FString Type = TEXT("float");
					if (Param.SourceKind.Equals(TEXT("Constant"), ESearchCase::CaseSensitive))
					{
						Type = Param.ConstantType;
					}
					else if (Param.SourceKind.Equals(TEXT("Parameter"), ESearchCase::CaseSensitive))
					{
						const int32 ParameterIndex = Pipeline.FindParameter(Param.Parameter);
						Type = ParameterIndex != INDEX_NONE ? Payload.Parameters[ParameterIndex].Type : FString(TEXT("float"));
					}
					const int32 Width = SlotWidthOfType(Type);
					if (Width == 0)
					{
						// `read` binds a pipeline buffer or a built-in texture, never a parameter: a texture parameter reaches a
						// material only, through its texture parameter (Render/DreamPassSnapshot.cpp, ApplyMaterialParameter).
						Diagnostics.Error(
							TEXT("DSH7347"),
							File,
							Param.Source.Span,
							FText::Format(
								LOCTEXT("SlotTextureParamMaterial", "'{0}' is a {1}, and the parameter block of a shader slot holds numbers only, so no texture parameter reaches a '.usf' pass. A material takes one through 'param': draw this pass with a material ('Material = ...'), or let a fullscreen material pass that takes the texture by 'param' write it into a buffer this pass reads."),
								FText::FromString(Param.Target),
								FText::FromString(Type)));
						Widths.Reset();
						break;
					}
					Widths.Add(Width);
				}
				if (Widths.Num() == Pass.Params.Num() && !FitSlotParameters(Widths))
				{
					Diagnostics.Error(
						TEXT("DSH7347"),
						File,
						Pass.Params.Last().Source.Span,
						FText::Format(
							LOCTEXT("SlotParamsTooMany", "The parameters of pass '{0}' do not fit the {1} float4 of a shader slot, packed in order (a float4 takes a vector of its own, a float3 three components, a float2 a half, a scalar one); pass fewer, or pack them yourself."),
							FText::FromString(Pass.Name),
							FText::AsNumber(MaxSlotParamVectors)));
				}
			}
		}

		void FPipelineBinder::CheckPassAgainstReferences(IR::FIRPass& Pass, const int32 KindIndex, const FPassDecl& Decl, const FPipelinePassKeys& Keys)
		{
			const FLangSpan NameSpan = Decl.NameSpan;
			const int32 InjectionIndex = InjectionIndexOf(Pass);
			// Below UE 5.8 the host reads a material's domain and blendable location only: its UserSceneTexture inputs, its
			// UE.DreamPassOutput pins, its usage flags and its pre-exposure and translator settings are facts of the Custom Pass
			// runtime it does not have (FPipelineReferences::bCustomPassAvailable). Those checks are skipped there rather than
			// failed on facts that were never read; the emitter refuses the pipeline anyway.
			const bool bPassFacts = References->bCustomPassAvailable;

			// Layers are the project's (D-7): the first MaxPassLayers names of its table, the only ones that have a bit -- the
			// host hands over no others (FPipelineReferences::LayerNames).
			for (const IR::FIRPassFilterClause& Clause : Pass.Filter)
			{
				for (const IR::FIRPassFilterTerm& Term : Clause.AllOf)
				{
					for (const FString& Layer : Term.Layers)
					{
						if (References->LayerNames.ContainsByPredicate([&Layer](const FString& Name) { return Name.Equals(Layer, ESearchCase::CaseSensitive); }))
						{
							continue;
						}
						const FString Suggestion = FLangBinder::SuggestCaseInsensitive(Layer, References->LayerNames);
						Diagnostics.Error(
							TEXT("DSH4412"),
							File,
							Keys.SpanOf(TEXT("Filter"), NameSpan),
							Suggestion.IsEmpty()
								? FText::Format(LOCTEXT("UnknownLayerFirstNames", "'{0}' is not a pass layer of this project; the layers are the first {1} names of Project Settings > DreamPlugin > DreamShader Custom Pass > Layer Names."), FText::FromString(Layer), FText::AsNumber(MaxPassLayers))
								: FText::Format(LOCTEXT("UnknownLayerDidYouMean", "'{0}' is not a pass layer of this project; did you mean '{1}'?"), FText::FromString(Layer), FText::FromString(Suggestion)));
					}
				}
			}

			// The material of a fullscreen or a mesh pass.
			const bool bFullscreenMaterial = KindIndex == Kind::Fullscreen && !Pass.MaterialReference.IsEmpty();
			const bool bMeshMaterial = KindIndex == Kind::Mesh && !Pass.MaterialReference.IsEmpty();
			if (bFullscreenMaterial || bMeshMaterial)
			{
				const FLangSpan MaterialSpan = Keys.SpanOf(TEXT("Material"), NameSpan);
				const FPipelineMaterialInfo* Info = References->FindMaterial(Pass.MaterialReference);
				if (!Info || !Info->bFound)
				{
					Diagnostics.Error(
						TEXT("DSH4406"),
						File,
						MaterialSpan,
						FText::Format(
							LOCTEXT("MaterialNotFound", "No material '{0}' was found: a bare name is the material a '.dss' under the same source root builds, an object path any material or material instance."),
							FText::FromString(Pass.MaterialReference)));
				}
				else
				{
					Pass.MaterialObjectPath = Info->ObjectPath;

					if (bFullscreenMaterial)
					{
						// V6.
						if (!Info->Domain.IsEmpty() && !Info->Domain.Equals(TEXT("PostProcess"), ESearchCase::CaseSensitive))
						{
							Diagnostics.Error(
								TEXT("DSH7337"),
								File,
								MaterialSpan,
								FText::Format(
									LOCTEXT("FullscreenDomain", "'{0}' is a {1} material, and a fullscreen pass draws a Post Process one: '#pragma material(Domain = PostProcess)'."),
									FText::FromString(Pass.MaterialReference),
									FText::FromString(Info->Domain)));
						}

						if (bPassFacts)
						{
							for (const IR::FIRPassBinding& Read : Pass.Reads)
							{
								if (!Info->UserSceneTextureInputs.ContainsByPredicate([&Read](const FString& Name) { return Name.Equals(Read.Slot, ESearchCase::CaseSensitive); }))
								{
									Diagnostics.Error(
										TEXT("DSH7334"),
										File,
										Read.Source.Span,
										FText::Format(
											LOCTEXT("ReadNotAnInput", "'{0}' reads no UserSceneTexture of '{1}': its inputs are {2}."),
											FText::FromString(Read.Slot),
											FText::FromString(Pass.MaterialReference),
											FText::FromString(Info->UserSceneTextureInputs.Num() > 0 ? FString::Join(Info->UserSceneTextureInputs, TEXT(", ")) : FString(TEXT("none")))));
								}
							}
							for (const FString& Input : Info->UserSceneTextureInputs)
							{
								if (!Pass.Reads.ContainsByPredicate([&Input](const IR::FIRPassBinding& Read) { return Read.Slot.Equals(Input, ESearchCase::CaseSensitive); }))
								{
									Diagnostics.Warning(
										TEXT("DSH7335"),
										File,
										NameSpan,
										FText::Format(
											LOCTEXT("InputUnbound", "'{0}' reads the UserSceneTexture '{1}', and pass '{2}' binds nothing to it, so it samples black; add 'read {1} = <Buffer>;'."),
											FText::FromString(Pass.MaterialReference),
											FText::FromString(Input),
											FText::FromString(Pass.Name)));
								}
							}
						}

						// The engine gives every UserSceneTexture input of the material a slot, bound or not
						// (PostProcessMaterial.cpp, the user scene texture slots), so the material's inputs are what must fit.
						const int32 FreeSlots = MaterialInputSlots - CountUsedInputSlots(Info->PostProcessInputsUsed);
						const int32 InputsNeeded = Info->UserSceneTextureInputs.Num();
						if (bPassFacts && InputsNeeded > FreeSlots)
						{
							Diagnostics.Error(
								TEXT("DSH7336"),
								File,
								Pass.Reads.Num() > 0 ? Pass.Reads.Last().Source.Span : MaterialSpan,
								FText::Format(
									LOCTEXT("FullscreenMaterialNoFreeSlots", "'{0}' takes {1} of the {2} post-process input slots with its SceneTexture nodes, which leaves {4} for its UserSceneTexture inputs, and it has {5}; read fewer buffers in it, or write pass '{3}' as a '.usf' pass."),
									FText::FromString(Pass.MaterialReference),
									FText::AsNumber(MaterialInputSlots - FreeSlots),
									FText::AsNumber(MaterialInputSlots),
									FText::FromString(Pass.Name),
									FText::AsNumber(FMath::Max(FreeSlots, 0)),
									FText::AsNumber(InputsNeeded)));
						}

						const bool bWritesData = Pass.Writes.ContainsByPredicate([](const IR::FIRPassBinding& Write) { return !Write.Buffer.Equals(SceneColorName, ESearchCase::CaseSensitive); });
						if (bPassFacts && bWritesData && !Info->bDisablePreExposureScale)
						{
							Diagnostics.Error(
								TEXT("DSH7338"),
								File,
								MaterialSpan,
								FText::Format(
									LOCTEXT("PreExposure", "Pass '{0}' writes a data buffer, and '{1}' scales what it reads and writes by the exposure; give it '#pragma material(bDisablePreExposureScale = true)'."),
									FText::FromString(Pass.Name),
									FText::FromString(Pass.MaterialReference)));
						}

						if (!Info->BlendableLocation.IsEmpty() && InjectionIndex != INDEX_NONE)
						{
							const bool bMaterialAfter = Info->BlendableLocation.Equals(TEXT("SceneColorAfterTonemapping"), ESearchCase::CaseSensitive)
								|| Info->BlendableLocation.Equals(TEXT("ReplacingTonemapper"), ESearchCase::CaseSensitive);
							if (bMaterialAfter != IsAfterTonemapInjection(InjectionIndex))
							{
								Diagnostics.Warning(
									TEXT("DSH7339"),
									File,
									MaterialSpan,
									FText::Format(
										LOCTEXT("BlendableLocation", "'{0}' is compiled for BlendableLocation {1}, and pass '{2}' runs it at {3}, {4} tonemapping, so its colours are in another space than it expects; use {5}."),
										FText::FromString(Pass.MaterialReference),
										FText::FromString(Info->BlendableLocation),
										FText::FromString(Pass.Name),
										FText::FromString(Pass.Injection),
										IsAfterTonemapInjection(InjectionIndex) ? LOCTEXT("AfterTonemap", "after") : LOCTEXT("BeforeTonemap", "before"),
										IsAfterTonemapInjection(InjectionIndex)
											? LOCTEXT("UseAfterTonemapping", "'BlendableLocation = SceneColorAfterTonemapping'")
											: LOCTEXT("UseBeforeTonemapping", "'BlendableLocation = SceneColorAfterDOF' or 'SceneColorBeforeDOF'")));
							}
						}
					}
					else
					{
						// V7.
						if (!Info->Domain.IsEmpty() && !Info->Domain.Equals(TEXT("Surface"), ESearchCase::CaseSensitive))
						{
							Diagnostics.Error(
								TEXT("DSH7344"),
								File,
								MaterialSpan,
								FText::Format(
									LOCTEXT("MeshDomain", "'{0}' is a {1} material, and a mesh pass draws primitives with a Surface one."),
									FText::FromString(Pass.MaterialReference),
									FText::FromString(Info->Domain)));
						}
						// The pass node's pins, the usage flags and the translator setting below are read on 5.8 only (bPassFacts).
						if (bPassFacts && !Info->bHasPassOutput)
						{
							Diagnostics.Error(
								TEXT("DSH7340"),
								File,
								MaterialSpan,
								FText::Format(
									LOCTEXT("MeshNoPassOutput", "'{0}' has no UE.DreamPassOutput in its graph, so a mesh pass has nothing to write; add 'UE.DreamPassOutput(Output0 = ...);' to the material."),
									FText::FromString(Pass.MaterialReference)));
						}
						else if (bPassFacts)
						{
							for (const IR::FIRPassBinding& Write : Pass.Writes)
							{
								for (int32 Output = 0; Output < MaxMeshOutputs; ++Output)
								{
									if (Write.Slot.Equals(FString::Printf(TEXT("Output%d"), Output), ESearchCase::CaseSensitive)
										&& (Info->PassOutputsConnected & (1u << Output)) == 0)
									{
										Diagnostics.Error(
											TEXT("DSH7341"),
											File,
											Write.Source.Span,
											FText::Format(
												LOCTEXT("MeshOutputNotConnected", "'{0}' leaves Output{1} of its UE.DreamPassOutput unconnected, so '{2}' would receive nothing."),
												FText::FromString(Pass.MaterialReference),
												FText::AsNumber(Output),
												FText::FromString(Write.Buffer)));
									}
								}
							}
						}
						for (const FString& Usage : EffectiveUsage(Pass))
						{
							if (bPassFacts && !Info->Usages.ContainsByPredicate([&Usage](const FString& Name) { return Name.Equals(Usage, ESearchCase::CaseSensitive); }))
							{
								// UMaterial's flag for a usage: bUsedWith<Usage>, but for the spline meshes' plural.
								const FString Flag = Usage.Equals(TEXT("SplineMesh"), ESearchCase::CaseSensitive)
									? FString(TEXT("bUsedWithSplineMeshes"))
									: FString(TEXT("bUsedWith")) + Usage;
								Diagnostics.Error(
									TEXT("DSH7342"),
									File,
									Keys.SpanOf(TEXT("Usage"), MaterialSpan),
									FText::Format(
										LOCTEXT("MeshUsage", "Mesh pass '{0}' draws {1} primitives, and '{2}' is not compiled for them; give the material its usage flag ('#pragma material({3} = true)' in its '.dss'), or leave {1} out of 'Usage'."),
										FText::FromString(Pass.Name),
										FText::FromString(Usage),
										FText::FromString(Pass.MaterialReference),
										FText::FromString(Flag)));
							}
						}
						if (bPassFacts && Info->bUsesNewTranslator)
						{
							Diagnostics.Error(
								TEXT("DSH7345"),
								File,
								MaterialSpan,
								FText::Format(
									LOCTEXT("MeshNewTranslator", "'{0}' asks for the new material translator, which UE.DreamPassOutput does not support; remove 'bEnableNewHLSLGenerator' from it."),
									FText::FromString(Pass.MaterialReference)));
						}
					}
				}
			}

			// The `.usf` of a fullscreen or compute pass (V8).
			if (!Pass.ShaderReference.IsEmpty() && (KindIndex == Kind::Fullscreen || KindIndex == Kind::Compute))
			{
				const FLangSpan ShaderSpan = Keys.SpanOf(TEXT("Shader"), NameSpan);
				const FPipelineShaderInfo* Info = References->FindShader(Pass.ShaderReference);
				if (!Info || !Info->bExists)
				{
					Diagnostics.Error(
						TEXT("DSH4408"),
						File,
						ShaderSpan,
						FText::Format(
							LOCTEXT("ShaderNotFound", "The shader file '{0}' does not exist; a path starting with '/' is a virtual shader path, and any other is read from the folder of this '.dsp'."),
							FText::FromString(Pass.ShaderReference)));
				}
				else
				{
					// The file may live anywhere: the engine compiles a snapshot of it under the mapped /DreamPassUser directory,
					// never the original, so where it is needs no check. A virtual path is carried when the host gave one.
					Pass.ShaderFilePath = Info->FilePath;
					if (!Info->VirtualPath.IsEmpty())
					{
						Pass.ShaderVirtualPath = Info->VirtualPath;
					}

					if (!Pass.Entry.IsEmpty())
					{
						// HLSL names are case-sensitive: the entry map is keyed so (FPipelineComputeEntryMap), and the plain list of
						// the other functions is searched by hand, an FString element comparing ignoring case.
						const FIntVector* Threads = Info->ComputeEntries.Find(Pass.Entry);
						const bool bFunction = Info->Functions.ContainsByPredicate([&Pass](const FString& Function)
						{
							return Function.Equals(Pass.Entry, ESearchCase::CaseSensitive);
						});
						if (KindIndex == Kind::Compute)
						{
							if (Threads)
							{
								if (!Pass.bThreadsWritten)
								{
									Pass.ThreadsX = Threads->X;
									Pass.ThreadsY = Threads->Y;
									Pass.ThreadsZ = Threads->Z;
								}
								else if (Pass.ThreadsX != Threads->X || Pass.ThreadsY != Threads->Y || Pass.ThreadsZ != Threads->Z)
								{
									// The dispatch is sized from Threads, the groups from [numthreads]: two answers mean a wrong dispatch.
									Diagnostics.Error(
										TEXT("DSH4413"),
										File,
										Keys.SpanOf(TEXT("Threads"), ShaderSpan),
										FText::Format(
											LOCTEXT("ThreadsDisagree", "'Threads = uint3({0}, {1}, {2})' disagrees with '[numthreads({3}, {4}, {5})]' of '{6}', and the dispatch would be sized for groups the shader does not have; leave 'Threads' out, or write the same numbers."),
											FText::AsNumber(Pass.ThreadsX), FText::AsNumber(Pass.ThreadsY), FText::AsNumber(Pass.ThreadsZ),
											FText::AsNumber(Threads->X), FText::AsNumber(Threads->Y), FText::AsNumber(Threads->Z),
											FText::FromString(Pass.Entry)));
								}
							}
							else if (bFunction)
							{
								if (!Pass.bThreadsWritten)
								{
									Diagnostics.Error(
										TEXT("DSH4411"),
										File,
										Keys.SpanOf(TEXT("Entry"), ShaderSpan),
										FText::Format(
											LOCTEXT("EntryNoNumThreads", "'{0}' in '{1}' has no '[numthreads(x, y, z)]' the compiler can read; write 'Threads = uint3(x, y, z)' in the pass."),
											FText::FromString(Pass.Entry),
											FText::FromString(Pass.ShaderReference)));
								}
							}
							else if (Info->bEntryScanComplete)
							{
								Diagnostics.Error(
									TEXT("DSH4410"),
									File,
									Keys.SpanOf(TEXT("Entry"), ShaderSpan),
									FText::Format(
										LOCTEXT("EntryNotFound", "'{0}' does not define a function '{1}'."),
										FText::FromString(Pass.ShaderReference),
										FText::FromString(Pass.Entry)));
							}
							else if (!Pass.bThreadsWritten)
							{
								// Not in what the host could read, and the file includes what it could not (bEntryScanComplete): the
								// entry may well be there, so it is not called missing -- but its group size is unknown, and the
								// dispatch is counted in groups of it.
								Diagnostics.Error(
									TEXT("DSH4411"),
									File,
									Keys.SpanOf(TEXT("Entry"), ShaderSpan),
									FText::Format(
										LOCTEXT("EntryUnreadNumThreads", "'{0}' is not in what the compiler could read of '{1}', which includes a file it cannot follow, so its '[numthreads(x, y, z)]' is unknown; write 'Threads = uint3(x, y, z)' in the pass."),
										FText::FromString(Pass.Entry),
										FText::FromString(Pass.ShaderReference)));
							}
						}
						else if (Threads)
						{
							// A fullscreen pass runs its Entry as the pixel shader of its slot (FDreamPassPS, renamed DreamPassMainPS by
							// the registry): a function with `[numthreads]` in front of it is a compute shader's entry.
							Diagnostics.Error(
								TEXT("DSH4410"),
								File,
								Keys.SpanOf(TEXT("Entry"), ShaderSpan),
								FText::Format(
									LOCTEXT("EntryIsComputeShader", "'{1}' in '{0}' is a compute shader entry ('[numthreads]' is in front of it), and fullscreen pass '{2}' runs its Entry as a pixel shader; name the pixel shader function, or make the pass 'compute'."),
									FText::FromString(Pass.ShaderReference),
									FText::FromString(Pass.Entry),
									FText::FromString(Pass.Name)));
						}
						else if (!bFunction && Info->bEntryScanComplete)
						{
							Diagnostics.Error(
								TEXT("DSH4410"),
								File,
								Keys.SpanOf(TEXT("Entry"), ShaderSpan),
								FText::Format(
									LOCTEXT("EntryNotFound2", "'{0}' does not define a function '{1}'."),
									FText::FromString(Pass.ShaderReference),
									FText::FromString(Pass.Entry)));
						}
					}
				}
			}
		}

		// ------------------------------------------------------------------------------ inline HLSL

		void FPipelineBinder::DeclareHlslBlock(const FHlslBlockDecl& Decl)
		{
			if (SharedHlslDecl)
			{
				Diagnostics.Error(
					TEXT("DSH7362"),
					File,
					Decl.KeywordSpan,
					FText::Format(
						LOCTEXT("SecondFileHlslBlock", "This file already has an 'hlsl' block, on line {0}, and a '.dsp' has one: write every shared function and entry in it."),
						FText::AsNumber(SharedHlslDecl->KeywordSpan.Line)));
				return;
			}
			// A `///` block above it documents nothing; its words are the author's, its directives have no effect.
			ReportDirectivesWithoutEffect(Decl.Doc, {}, LOCTEXT("WhereHlslBlock", "the file's 'hlsl' block"), /* bAfterBindDirectives */ false);

			SharedHlslDecl = &Decl;
			SharedScan = ScanHlslText(Decl.RawBody);
			Payload.bHasSharedHlsl = true;
			Payload.SharedHlsl = Decl.RawBody;
			Payload.SharedHlslLine = Decl.BodySpan.Line;
		}

		void FPipelineBinder::BindPassHlsl(const IR::FIRPass& Pass, const FPassStmt& Statement, FPipelinePassKeys& Keys)
		{
			if (Keys.Hlsl)
			{
				Diagnostics.Error(
					TEXT("DSH7361"),
					File,
					Statement.NameSpan,
					FText::Format(
						LOCTEXT("SecondPassHlslBlock", "Pass '{0}' holds a second 'hlsl' block, and the code of a pass is one block; the one on line {1} is it."),
						FText::FromString(Pass.Name),
						FText::AsNumber(Keys.Hlsl->NameSpan.Line)));
				return;
			}
			Keys.Hlsl = &Statement;
		}

		void FPipelineBinder::ResolveHlslSource(IR::FIRPass& Pass, const int32 KindIndex, const FPipelinePassKeys& Keys, const FLangSpan& NameSpan)
		{
			const bool bCompute = KindIndex == Kind::Compute;
			const FPassStmt* Block = Keys.Hlsl;
			const FPassStmt* EntryKey = Keys.Find(TEXT("Entry"));

			// ---- a shader file
			if (!Pass.ShaderReference.IsEmpty())
			{
				Pass.HlslSource = IR::PassHlslSource::File;
				if (Block)
				{
					Diagnostics.Error(
						TEXT("DSH7364"),
						File,
						Block->NameSpan,
						FText::Format(
							LOCTEXT("ShaderAndHlslBlock", "Pass '{0}' runs the shader file '{1}' and holds an 'hlsl' block as well, and the code of a pass is in one place: remove 'Shader' to run the block, or the block to run the file."),
							FText::FromString(Pass.Name),
							FText::FromString(Pass.ShaderReference)));
				}
				if (Pass.Entry.IsEmpty())
				{
					Diagnostics.Error(
						TEXT("DSH7315"),
						File,
						Keys.SpanOf(TEXT("Shader"), NameSpan),
						bCompute
							? FText::Format(
								LOCTEXT("ComputeNoEntry", "Compute pass '{0}' needs 'Entry = <function>' naming its '[numthreads]' function."),
								FText::FromString(Pass.Name))
							: FText::Format(
								LOCTEXT("ShaderNeedsEntry", "Pass '{0}' runs a shader file, and needs 'Entry = <function>' naming the function in it."),
								FText::FromString(Pass.Name)));
				}
				return;
			}

			// ---- its own block
			if (Block)
			{
				Pass.InlineHlsl = Block->RawBody;
				Pass.InlineHlslLine = Block->BodySpan.Line;
				const FHlslTextScan Scan = ScanHlslText(Block->RawBody);
				if (!Scan.bHasDeclarations)
				{
					// The statements of the entry: the compiler writes the function, whose name the slot's own entry point is.
					Pass.HlslSource = IR::PassHlslSource::Body;
					if (EntryKey)
					{
						Diagnostics.Error(
							TEXT("DSH7365"),
							File,
							EntryKey->Span,
							FText::Format(
								LOCTEXT("EntryWithBodyForm", "The 'hlsl' block of pass '{0}' holds the statements of its entry, whose function the compiler writes, so 'Entry' has no function to name; remove it, or write whole functions in the block: '{1}'."),
								FText::FromString(Pass.Name),
								FText::FromString(bCompute
									? TEXT("[numthreads(8, 8, 1)] void Main(uint3 Id : SV_DispatchThreadID) { ... }")
									: TEXT("void Main(float4 SvPosition : SV_POSITION, out float4 Out : SV_Target0) { ... }"))));
						Pass.Entry.Reset();
					}
					for (const int32 IncludeLine : Scan.IncludeLines)
					{
						Diagnostics.Error(
							TEXT("DSH7366"),
							File,
							SpanOfHlslBlockLine(Block->BodySpan, Block->RawBody, IncludeLine),
							FText::Format(
								LOCTEXT("IncludeInBodyForm", "'#include' cannot stand among the statements of a function, and the 'hlsl' block of pass '{0}' holds the statements of its entry; include the file in the file's 'hlsl' block, or write whole functions in the pass's block."),
								FText::FromString(Pass.Name)));
					}
					return;
				}

				Pass.HlslSource = IR::PassHlslSource::Block;
				const bool bEntryWritten = !Pass.Entry.IsEmpty();
				if (!bEntryWritten)
				{
					Pass.Entry = TEXT("Main");
				}
				CheckInlineEntry(Pass, KindIndex, Scan, /* bShared */ false, bEntryWritten, Keys, NameSpan);
				return;
			}

			// ---- an entry of the file's block
			if (!Pass.Entry.IsEmpty())
			{
				Pass.HlslSource = IR::PassHlslSource::Shared;
				if (!SharedHlslDecl)
				{
					Diagnostics.Error(
						TEXT("DSH4410"),
						File,
						Keys.SpanOf(TEXT("Entry"), NameSpan),
						FText::Format(
							LOCTEXT("SharedEntryWithoutBlock", "Pass '{0}' has no 'Shader' and no 'hlsl' block of its own, so 'Entry = {1}' names a function of the file's 'hlsl' block, and this '.dsp' has none; write the function in an 'hlsl { }' block at file scope, write the pass's code in an 'hlsl' block of its own, or give it 'Shader = \"<file>.usf\"'."),
							FText::FromString(Pass.Name),
							FText::FromString(Pass.Entry)));
					return;
				}
				CheckInlineEntry(Pass, KindIndex, SharedScan, /* bShared */ true, /* bEntryWritten */ true, Keys, NameSpan);
				return;
			}

			// ---- nowhere
			Diagnostics.Error(
				TEXT("DSH7315"),
				File,
				NameSpan,
				bCompute
					? FText::Format(
						LOCTEXT("ComputeNoShader", "Compute pass '{0}' needs code to run: 'Shader = \"<file>.usf\"' with 'Entry = <function>', an 'hlsl { }' block of its own, or 'Entry' naming a function of the file's 'hlsl { }' block."),
						FText::FromString(Pass.Name))
					: FText::Format(
						LOCTEXT("FullscreenNothing", "Fullscreen pass '{0}' needs code to run: 'Material = \"...\"' (a Post Process material), 'Shader = \"<file>.usf\"' with 'Entry', an 'hlsl { }' block of its own, or 'Entry' naming a function of the file's 'hlsl { }' block."),
						FText::FromString(Pass.Name)));
		}

		void FPipelineBinder::CheckInlineEntry(IR::FIRPass& Pass, const int32 KindIndex, const FHlslTextScan& Scan, const bool bShared, const bool bEntryWritten, const FPipelinePassKeys& Keys, const FLangSpan& NameSpan)
		{
			const FLangSpan EntrySpan = bEntryWritten ? Keys.SpanOf(TEXT("Entry"), NameSpan) : (Keys.Hlsl ? Keys.Hlsl->NameSpan : NameSpan);
			const FHlslTopLevelFunction* Function = Scan.FindFunction(Pass.Entry);
			if (!Function)
			{
				Diagnostics.Error(
					TEXT("DSH4410"),
					File,
					EntrySpan,
					bShared
						? FText::Format(
							LOCTEXT("SharedEntryNotFound", "The file's 'hlsl' block does not define a function '{1}', which pass '{0}' names as its 'Entry'."),
							FText::FromString(Pass.Name),
							FText::FromString(Pass.Entry))
						: bEntryWritten
							? FText::Format(
								LOCTEXT("BlockEntryNotFound", "The 'hlsl' block of pass '{0}' does not define a function '{1}', which 'Entry' names; the entry is a function of the block itself."),
								FText::FromString(Pass.Name),
								FText::FromString(Pass.Entry))
							: FText::Format(
								LOCTEXT("BlockMainNotFound", "The 'hlsl' block of pass '{0}' holds whole functions, and none is 'Main', the entry of a block when the pass writes no 'Entry'; name the entry function 'Main', or write 'Entry = <function>;'."),
								FText::FromString(Pass.Name)));
				return;
			}

			if (KindIndex == Kind::Compute)
			{
				if (Function->bComputeEntry)
				{
					if (!Pass.bThreadsWritten)
					{
						Pass.ThreadsX = Function->GroupSize.X;
						Pass.ThreadsY = Function->GroupSize.Y;
						Pass.ThreadsZ = Function->GroupSize.Z;
					}
					else if (Pass.ThreadsX != Function->GroupSize.X || Pass.ThreadsY != Function->GroupSize.Y || Pass.ThreadsZ != Function->GroupSize.Z)
					{
						// The dispatch is sized from Threads, the groups from [numthreads]: two answers mean a wrong dispatch.
						Diagnostics.Error(
							TEXT("DSH4413"),
							File,
							Keys.SpanOf(TEXT("Threads"), EntrySpan),
							FText::Format(
								LOCTEXT("InlineThreadsDisagree", "'Threads = uint3({0}, {1}, {2})' disagrees with the '[numthreads({3}, {4}, {5})]' in front of '{6}' in the '.dsp', and the dispatch would be sized for groups the shader does not have; leave 'Threads' out, or write the same numbers."),
								FText::AsNumber(Pass.ThreadsX), FText::AsNumber(Pass.ThreadsY), FText::AsNumber(Pass.ThreadsZ),
								FText::AsNumber(Function->GroupSize.X), FText::AsNumber(Function->GroupSize.Y), FText::AsNumber(Function->GroupSize.Z),
								FText::FromString(Pass.Entry)));
					}
				}
				else if (!Pass.bThreadsWritten)
				{
					Diagnostics.Error(
						TEXT("DSH4411"),
						File,
						EntrySpan,
						FText::Format(
							LOCTEXT("InlineEntryNoNumThreads", "'{0}' has no '[numthreads(x, y, z)]' in front of it that the compiler can read -- three whole numbers; write them so, or write 'Threads = uint3(x, y, z)' in pass '{1}'."),
							FText::FromString(Pass.Entry),
							FText::FromString(Pass.Name)));
				}
			}
			else if (Function->bComputeEntry)
			{
				// A fullscreen pass runs its entry as the pixel shader of its slot (FDreamPassPS).
				Diagnostics.Error(
					TEXT("DSH4410"),
					File,
					EntrySpan,
					FText::Format(
						LOCTEXT("InlineEntryIsComputeShader", "'{0}' is a compute shader entry ('[numthreads]' is in front of it), and fullscreen pass '{1}' runs its entry as a pixel shader; name the pixel shader function, or make the pass 'compute'."),
						FText::FromString(Pass.Entry),
						FText::FromString(Pass.Name)));
			}
		}

		void FPipelineBinder::CheckSharedHlsl()
		{
			// The entries of the file's block: what a Shared pass names. Exact spellings, as HLSL tells names apart.
			TArray<FString> SharedEntries;
			TArray<const IR::FIRPass*> SharedEntryPasses;
			for (const IR::FIRPass& Pass : Payload.Passes)
			{
				if (Pass.HlslSource.Equals(IR::PassHlslSource::Shared, ESearchCase::CaseSensitive) && !Pass.Entry.IsEmpty() && !ContainsExactly(SharedEntries, Pass.Entry))
				{
					SharedEntries.Add(Pass.Entry);
					SharedEntryPasses.Add(&Pass);
				}
			}
			const auto FirstPassNaming = [&SharedEntries, &SharedEntryPasses](const FString& Entry) -> const IR::FIRPass*
			{
				for (int32 Index = 0; Index < SharedEntries.Num(); ++Index)
				{
					if (SharedEntries[Index].Equals(Entry, ESearchCase::CaseSensitive))
					{
						return SharedEntryPasses[Index];
					}
				}
				return nullptr;
			};
			const auto ReportEntryCall = [this, &FirstPassNaming](const FHlslIdentifier& Identifier, const FLangSpan& Span)
			{
				const IR::FIRPass* Owner = FirstPassNaming(Identifier.Name);
				Diagnostics.Error(
					TEXT("DSH7369"),
					File,
					Span,
					FText::Format(
						LOCTEXT("SharedEntryCalled", "'{0}' is the entry of pass '{1}' in the file's 'hlsl' block, and is called here; an entry is compiled only into the slots of the passes that name it, so nothing else can call it. Move what it shares into a function of its own in the block, and call that."),
						FText::FromString(Identifier.Name),
						FText::FromString(Owner ? Owner->Name : FString())));
			};

			// ---- the passes' own blocks: none may call an entry of the file's block.
			if (SharedEntries.Num() > 0)
			{
				for (int32 PassIndex = 0; PassIndex < Payload.Passes.Num() && PassIndex < PassKeys.Num(); ++PassIndex)
				{
					const FPassStmt* Block = PassKeys[PassIndex].Hlsl;
					if (!Block || !IR::PassHlslSource::IsInline(Payload.Passes[PassIndex].HlslSource))
					{
						continue;
					}
					TArray<FHlslIdentifier> Identifiers;
					FindHlslIdentifiers(Block->RawBody, Identifiers);
					for (const FHlslIdentifier& Identifier : Identifiers)
					{
						if (Identifier.bFollowedByParenthesis && ContainsExactly(SharedEntries, Identifier.Name))
						{
							ReportEntryCall(Identifier, SpanInHlslBlock(Block->BodySpan, Block->RawBody, Identifier.Offset, Identifier.Name.Len()));
						}
					}
				}
			}

			if (!SharedHlslDecl)
			{
				return;
			}
			const FString& Text = SharedHlslDecl->RawBody;

			// What the slot of each inline pass #defines: the shared code is compiled into all of them, after the definitions.
			struct FDefinedBy
			{
				FSlotDefinedName Name;
				const IR::FIRPass* Pass = nullptr;
			};
			TArray<FDefinedBy> Defined;
			for (const IR::FIRPass& Pass : Payload.Passes)
			{
				if (!IR::PassHlslSource::IsInline(Pass.HlslSource))
				{
					continue;
				}
				TArray<FSlotDefinedName> Names;
				CollectSlotDefinedNames(Pass, Names);
				for (FSlotDefinedName& Name : Names)
				{
					Defined.Add({ MoveTemp(Name), &Pass });
				}
			}

			TArray<FHlslIdentifier> Identifiers;
			FindHlslIdentifiers(Text, Identifiers);
			TArray<FString> Reported;
			for (const FHlslIdentifier& Identifier : Identifiers)
			{
				// The entry function the identifier is in, if it is in one.
				const FHlslTopLevelFunction* InEntry = nullptr;
				for (const FHlslTopLevelFunction& Function : SharedScan.Functions)
				{
					if (Identifier.Offset >= Function.DeclarationStart && Identifier.Offset < Function.End && ContainsExactly(SharedEntries, Function.Name))
					{
						InEntry = &Function;
						break;
					}
				}
				const FLangSpan Span = SpanInHlslBlock(SharedHlslDecl->BodySpan, Text, Identifier.Offset, Identifier.Name.Len());

				if (ContainsExactly(SharedEntries, Identifier.Name) && Identifier.bFollowedByParenthesis)
				{
					// An entry's own name where it is defined; anything else followed by `(` is a call.
					if (!(InEntry && InEntry->Name.Equals(Identifier.Name, ESearchCase::CaseSensitive)))
					{
						ReportEntryCall(Identifier, Span);
					}
					continue;
				}
				if (InEntry)
				{
					// An entry is compiled into its own passes' slots, where their names are what it is meant to use.
					continue;
				}

				const FDefinedBy* Clash = Defined.FindByPredicate([&Identifier](const FDefinedBy& Candidate)
				{
					return Candidate.Name.Name.Equals(Identifier.Name, ESearchCase::CaseSensitive);
				});
				if (!Clash || ContainsExactly(Reported, Identifier.Name))
				{
					continue;
				}
				Reported.Add(Identifier.Name);
				Diagnostics.Error(
					TEXT("DSH7368"),
					File,
					Span,
					Clash->Name.Binding.IsEmpty()
						? FText::Format(
							LOCTEXT("SharedCodeNamesEntry", "'{0}' is in the shared code of the file's 'hlsl' block, and is the entry of pass '{1}' as well, which the pass's slot renames to the slot's entry point with a #define; the shared code is compiled into that slot, after it. Rename it in the block."),
							FText::FromString(Identifier.Name),
							FText::FromString(Clash->Pass->Name))
						: FText::Format(
							LOCTEXT("SharedCodeNamesBinding", "'{0}' is in the shared code of the file's 'hlsl' block, and pass '{1}' defines it in its HLSL slot ('{2}'); the shared code is compiled into the slot of every pass whose HLSL is in the '.dsp', after those #defines. Rename it in the block: only an entry, compiled into the slots of the passes that name it, uses a pass's names."),
							FText::FromString(Identifier.Name),
							FText::FromString(Clash->Pass->Name),
							FText::FromString(Clash->Name.Binding)));
			}
		}

		// ------------------------------------------------------------------------------ across passes

		FPipelineSizeClass FPipelineBinder::SizeClassOf(const FString& Buffer, const int32 InjectionIndex) const
		{
			FPipelineSizeClass Size;
			if (const IR::FIRPassBuffer* Declared = FindPayloadBuffer(Buffer))
			{
				Size.Resolution = Declared->Resolution;
				Size.Scale = Declared->Scale;
				Size.Width = Declared->FixedWidth;
				Size.Height = Declared->FixedHeight;
				return Size;
			}
			// Scene colour follows the point it is read at; every other built-in texture is the renderer's, at render size.
			Size.Resolution = (Buffer.Equals(SceneColorName, ESearchCase::CaseSensitive) && InjectionIndex != INDEX_NONE && IsOutputResolutionInjection(InjectionIndex))
				? ResolutionOutput
				: ResolutionRender;
			return Size;
		}

		void FPipelineBinder::CheckBuiltinBinding(const IR::FIRPass& Pass, const IR::FIRPassBinding& Binding, const bool bWrite, const int32 InjectionIndex)
		{
			const FString& Name = Binding.Buffer;
			if (!IsBuiltinBuffer(Name))
			{
				return;
			}

			// The runtime resolves what follows (DreamShaderPass, Render/DreamPassBuffers.cpp, ResolveRead / ResolveWrite);
			// a binding it cannot resolve skips the pass every frame, so whatever it refuses is refused here.
			if (Name.Equals(TEXT("CustomStencil"), ESearchCase::CaseSensitive))
			{
				// An integer view of the custom depth texture, not a texture of its own: the runtime binds it nowhere.
				Diagnostics.Error(
					TEXT("DSH7327"),
					File,
					Binding.Source.Span,
					FText::Format(
						LOCTEXT("CustomStencilNotBindable", "'CustomStencil' is the stencil half of the custom depth texture, which no pass can bind as a texture of its own, and pass '{0}' {1} it; read it through the scene textures instead: a SceneTexture node in the material, CalcSceneCustomStencil in a '.usf'."),
						FText::FromString(Pass.Name),
						bWrite ? LOCTEXT("CustomStencilWrites", "writes") : LOCTEXT("CustomStencilReads", "reads")));
				return;
			}

			bool bExists = true;
			bool bWritable = false;
			const TCHAR* Since = TEXT("");
			if (Name.Equals(SceneColorName, ESearchCase::CaseSensitive))
			{
				bExists = InjectionIndex >= Injection::AfterBasePass;
				// At PostProcess.TranslucencyAfterDOF the chain is the translucency, and the scene colour beside it is read-only.
				bWritable = bExists && InjectionIndex != Injection::PostProcessTranslucencyAfterDOF;
				Since = InjectionNames[Injection::AfterBasePass];
			}
			else if (Name.Equals(SceneDepthName, ESearchCase::CaseSensitive))
			{
				bExists = InjectionIndex >= Injection::BeforeBasePass;
				Since = InjectionNames[Injection::BeforeBasePass];
			}
			else if (Name.Equals(TEXT("CustomDepth"), ESearchCase::CaseSensitive))
			{
				bExists = InjectionIndex >= Injection::AfterBasePass;
				Since = InjectionNames[Injection::AfterOpaque];
				if (InjectionIndex == Injection::AfterBasePass && !bWrite)
				{
					Diagnostics.Info(
						TEXT("DSH7329"),
						File,
						Binding.Source.Span,
						FText::Format(
							LOCTEXT("CustomDepthAfterBasePass", "'{0}' exists at AfterBasePass only when r.CustomDepth.Order draws custom depth before the base pass; where it does not, pass '{1}' reads a cleared placeholder. From AfterOpaque on it always exists."),
							FText::FromString(Name),
							FText::FromString(Pass.Name)));
				}
			}
			else if (Name.Equals(TEXT("Translucency"), ESearchCase::CaseSensitive))
			{
				// Every subscription of the post-process chain carries it as an input (the engine's GetPostProcessMaterialInputs);
				// only at PostProcess.TranslucencyAfterDOF is it the chain itself, and so writable.
				bExists = IsPostProcessInjection(InjectionIndex);
				bWritable = InjectionIndex == Injection::PostProcessTranslucencyAfterDOF;
				Since = InjectionNames[Injection::PostProcessBeforeDOF];
			}
			else
			{
				// GBufferA..F, Velocity. The GBuffer is written in place at AfterBasePass; the velocity is not written at all.
				bExists = InjectionIndex >= Injection::AfterBasePass;
				bWritable = InjectionIndex == Injection::AfterBasePass && !Name.Equals(TEXT("Velocity"), ESearchCase::CaseSensitive);
				Since = InjectionNames[Injection::AfterBasePass];
			}

			if (!bExists)
			{
				Diagnostics.Error(
					TEXT("DSH7327"),
					File,
					Binding.Source.Span,
					FText::Format(
						Name.Equals(TEXT("Translucency"), ESearchCase::CaseSensitive)
							? LOCTEXT("TranslucencyOnlyThere", "'Translucency' exists on the post-process chain only (PostProcess.*), and pass '{1}' runs at {2}.")
							: LOCTEXT("BuiltinNotYet", "'{0}' does not exist yet at {2}, where pass '{1}' runs; it exists from {3} on."),
						FText::FromString(Name),
						FText::FromString(Pass.Name),
						FText::FromString(Pass.Injection),
						FText::FromString(Since)));
				return;
			}
			if (bWrite && !bWritable)
			{
				FText Reason;
				if (Name.Equals(SceneColorName, ESearchCase::CaseSensitive))
				{
					Reason = LOCTEXT("BuiltinNotWritableSceneColorTranslucency", "'SceneColor' is read-only at PostProcess.TranslucencyAfterDOF, where pass '{1}' runs: the post-process chain carries the translucency there, so write 'Translucency', or write the scene colour at another point.");
				}
				else if (Name.Equals(TEXT("Translucency"), ESearchCase::CaseSensitive))
				{
					Reason = LOCTEXT("BuiltinNotWritableTranslucency", "'Translucency' is written at PostProcess.TranslucencyAfterDOF only, and pass '{1}' runs at {2}; elsewhere on the chain it is read-only.");
				}
				else
				{
					Reason = LOCTEXT("BuiltinNotWritable", "'{0}' is the renderer's and pass '{1}' cannot write it at {2}; write a buffer of the pipeline instead.");
				}
				Diagnostics.Error(
					TEXT("DSH7328"),
					File,
					Binding.Source.Span,
					FText::Format(
						Reason,
						FText::FromString(Name),
						FText::FromString(Pass.Name),
						FText::FromString(Pass.Injection)));
			}
			if (Name.Equals(SceneColorName, ESearchCase::CaseSensitive) && InjectionIndex == Injection::AfterBasePass)
			{
				Diagnostics.Info(
					TEXT("DSH7326"),
					File,
					Binding.Source.Span,
					FText::Format(
						LOCTEXT("SceneColorAfterBasePass", "At AfterBasePass scene colour holds the emissive light only; nothing is lit yet, so pass '{0}' {1} that."),
						FText::FromString(Pass.Name),
						bWrite ? LOCTEXT("WritesInto", "writes into") : LOCTEXT("Reads", "reads")));
			}
		}

		void FPipelineBinder::CheckPassAtInjection(const IR::FIRPass& Pass, const int32 PassIndex, const int32 KindIndex, const int32 InjectionIndex)
		{
			const FPipelinePassKeys& Keys = PassKeys[PassIndex];
			const FLangSpan Span = Keys.SpanOf(TEXT("Injection"), Pipeline.PassDecls[PassIndex]->NameSpan);
			const bool bFullscreenMaterial = KindIndex == Kind::Fullscreen && !Pass.MaterialReference.IsEmpty();
			const bool bFullscreenShader = KindIndex == Kind::Fullscreen && !bFullscreenMaterial;
			// Either kind of HLSL pass runs in a slot, and every slot is handed the same View, SceneTextures and DP_Time
			// wherever it runs (DreamShaderPass, Render/DreamPassGlobalShaders.cpp, FillSlotParameters).
			const bool bSlotPass = bFullscreenShader || KindIndex == Kind::Compute;
			const bool bTestScene = Pass.Depth.Equals(TEXT("TestScene"), ESearchCase::CaseSensitive);

			// V2: the matrix of DreamShader_Plan/03 s2.
			if (bFullscreenMaterial && InjectionIndex <= Injection::BeforeBasePass)
			{
				Diagnostics.Error(
					TEXT("DSH7325"),
					File,
					Span,
					FText::Format(
						LOCTEXT("FullscreenMaterialTooEarly", "A fullscreen material pass needs the scene textures, which do not exist yet at {0}; run '{1}' at AfterBasePass or later."),
						FText::FromString(Pass.Injection),
						FText::FromString(Pass.Name)));
			}
			else if (KindIndex == Kind::Mesh && (InjectionIndex == Injection::BeginView || InjectionIndex == Injection::EndOfView))
			{
				Diagnostics.Error(
					TEXT("DSH7325"),
					File,
					Span,
					FText::Format(
						InjectionIndex == Injection::BeginView
							? LOCTEXT("MeshAtBeginView", "A mesh pass draws primitives against a view that has no depth yet at BeginView; run '{0}' at BeforeBasePass or later.")
							: LOCTEXT("MeshAtEndOfView", "At EndOfView the view is at output resolution and has no depth to draw primitives against; run mesh pass '{0}' earlier."),
						FText::FromString(Pass.Name)));
			}
			else if (bFullscreenMaterial && (InjectionIndex == Injection::AfterBasePass || InjectionIndex == Injection::AfterOpaque))
			{
				Diagnostics.Info(
					TEXT("DSH7326"),
					File,
					Span,
					FText::Format(
						LOCTEXT("FullscreenMaterialUnverified", "A fullscreen material pass at {0} has not been verified on this engine yet; BeforePostProcess is the point it is known to work at."),
						FText::FromString(Pass.Injection)));
			}
			else if (bSlotPass && InjectionIndex == Injection::BeginView)
			{
				// The view uniform buffer is created after BeginView, so the slot's registry defines `View` as a name that does
				// not exist for a pass there (Pass/DreamShaderPassSlotRegistry.cpp, BuildSlotSection): a use is a compile error,
				// not a placeholder. DP_Time stands in for the timing values (Shaders/Pass/DreamPass.ush).
				Diagnostics.Info(
					TEXT("DSH7326"),
					File,
					Span,
					FText::Format(
						LOCTEXT("FullscreenShaderBeginViewNoView", "At BeginView neither the scene textures nor the view uniform buffer exist yet: the SceneTextures pass '{0}' sees are placeholders, a use of 'View' in its '.usf' does not compile, and DP_Time gives the time."),
						FText::FromString(Pass.Name)));
			}
			else if (bSlotPass && InjectionIndex == Injection::BeforeBasePass)
			{
				Diagnostics.Info(
					TEXT("DSH7326"),
					File,
					Span,
					FText::Format(
						LOCTEXT("FullscreenShaderBeforeBasePass", "At BeforeBasePass only scene depth exists: the other scene textures pass '{0}' could read are placeholders."),
						FText::FromString(Pass.Name)));
			}
			else if (KindIndex == Kind::Mesh && InjectionIndex == Injection::BeforeBasePass && bTestScene)
			{
				Diagnostics.Info(
					TEXT("DSH7326"),
					File,
					Keys.SpanOf(TEXT("Depth"), Span),
					FText::Format(
						LOCTEXT("MeshBeforeBasePassTestScene", "At BeforeBasePass scene depth holds what the depth prepass drew, which may not be everything; mesh pass '{0}' is meant to run there with 'Depth = None' or 'Depth = Own(...)'."),
						FText::FromString(Pass.Name)));
			}
			else if (KindIndex == Kind::Mesh && IsPostProcessInjection(InjectionIndex) && IsOutputResolutionInjection(InjectionIndex) && bTestScene)
			{
				// Outputs of any size are drawn (V7 below): the runtime brings the scene depth into their pixels, point-sampled.
				Diagnostics.Info(
					TEXT("DSH7326"),
					File,
					Span,
					FText::Format(
						LOCTEXT("MeshAfterUpscaleCopied", "At {0} the view is upscaled and scene depth is still at render resolution; mesh pass '{1}' tests against it, or against a copy of it brought into the pixels of outputs of another size, so its depth test is only as fine as the render resolution."),
						FText::FromString(Pass.Injection),
						FText::FromString(Pass.Name)));
			}

			// V3: built-in textures where they exist.
			for (const IR::FIRPassBinding& Read : Pass.Reads)
			{
				CheckBuiltinBinding(Pass, Read, /* bWrite */ false, InjectionIndex);
			}
			for (const IR::FIRPassBinding& Write : Pass.Writes)
			{
				CheckBuiltinBinding(Pass, Write, /* bWrite */ true, InjectionIndex);
			}

			// V5: one texture is not read and written by one pass; last frame's and this frame's are two. A pass of any kind
			// that writes the chain's colour -- scene colour, or the translucency at PostProcess.TranslucencyAfterDOF -- draws
			// into a scratch texture that is brought back afterwards (02 s8.4; DreamShaderPass, Render/DreamPassBuffers.cpp,
			// ResolveWrite / CommitSceneColor), so it may read that colour as well: a fullscreen or compute pass, or a copy.
			const TCHAR* const ChainName = InjectionIndex == Injection::PostProcessTranslucencyAfterDOF ? TEXT("Translucency") : SceneColorName;
			for (const IR::FIRPassBinding& Read : Pass.Reads)
			{
				if (Read.bPrevious)
				{
					continue;
				}
				const bool bChainColorCopy = Read.Buffer.Equals(SceneColorName, ESearchCase::CaseSensitive) || Read.Buffer.Equals(ChainName, ESearchCase::CaseSensitive);
				if (!bChainColorCopy && PassWritesBuffer(Pass, Read.Buffer))
				{
					Diagnostics.Error(
						TEXT("DSH7333"),
						File,
						Read.Source.Span,
						FText::Format(
							LOCTEXT("ReadAndWrite", "Pass '{0}' reads and writes '{1}', and one pass cannot have one texture as its input and its output; write another buffer, or read '{1}.Previous' of a 'History = true' buffer."),
							FText::FromString(Pass.Name),
							FText::FromString(Read.Buffer)));
				}
			}

			// V7: a mesh pass's targets are drawn together, so they are one size (DreamShaderPass, Render/DreamPassMesh.cpp,
			// ResolveTargets). Its depth asks less of them, and only what the runtime cannot do is refused: `Depth = TestScene`
			// fits targets of any size -- where they do not hold the view at the scene depth's pixels, the runtime tests against
			// a copy of it brought into theirs (ResolveDepth, AddCopyDepthPass) -- and an own depth is bound as it is, so it
			// must not be smaller than the targets (ResolveDepth skips the pass when it is).
			if (KindIndex == Kind::Mesh && Pass.Writes.Num() > 0)
			{
				// The scene's own targets -- scene colour (through its scratch copy), the chain's translucency, the GBuffer at
				// AfterBasePass -- are the scene textures' size, which the renderer rounds up to a multiple of 8 and, in the
				// editor, grows to the largest view so far (Renderer/Private/SceneTextures.cpp), with the view somewhere inside
				// them; a buffer of the pipeline is exactly the view's size. ResolveTargets skips a mesh pass whose targets differ
				// in size or in where they hold the view, and the two meet only by chance -- so they are not mixed.
				const IR::FIRPassBinding* SceneTarget = Pass.Writes.FindByPredicate([](const IR::FIRPassBinding& Write) { return IsBuiltinBuffer(Write.Buffer); });
				const IR::FIRPassBinding* OwnTarget = Pass.Writes.FindByPredicate([this](const IR::FIRPassBinding& Write) { return FindPayloadBuffer(Write.Buffer) != nullptr; });
				const FPipelineSizeClass First = SizeClassOf(Pass.Writes[0].Buffer, InjectionIndex);
				if (SceneTarget && OwnTarget)
				{
					Diagnostics.Error(
						TEXT("DSH7343"),
						File,
						OwnTarget->Source.Span,
						FText::Format(
							LOCTEXT("MeshSceneAndOwnTargets", "Mesh pass '{0}' writes '{1}', a texture of the scene, together with '{2}', a buffer of the pipeline. A mesh pass draws all its targets through one viewport, so they have to be one size holding the view at one place; the scene's textures are usually larger than the view they hold (rounded up, and in the editor grown to the largest view so far) while a buffer is the view's size, and the runtime skips the pass whenever the two differ. Write them in two mesh passes."),
							FText::FromString(Pass.Name),
							FText::FromString(SceneTarget->Buffer),
							FText::FromString(OwnTarget->Buffer)));
				}
				else
				{
					for (int32 Index = 1; Index < Pass.Writes.Num(); ++Index)
					{
						const FPipelineSizeClass Other = SizeClassOf(Pass.Writes[Index].Buffer, InjectionIndex);
						if (!(Other == First))
						{
							Diagnostics.Error(
								TEXT("DSH7343"),
								File,
								Pass.Writes[Index].Source.Span,
								FText::Format(
									LOCTEXT("MeshSizesDiffer", "The outputs of mesh pass '{0}' are drawn together and have one size: '{1}' is {2}, and '{3}' is {4}."),
									FText::FromString(Pass.Name),
									FText::FromString(Pass.Writes[0].Buffer),
									FText::FromString(First.Describe()),
									FText::FromString(Pass.Writes[Index].Buffer),
									FText::FromString(Other.Describe())));
						}
					}
				}
				// An own depth beside the scene's textures works while the view starts at their corner, which is the usual case,
				// and is skipped where it does not (ResolveDepth: the depth, the view's size, does not reach the view's pixels).
				if (SceneTarget && Pass.Depth.Equals(TEXT("Own"), ESearchCase::CaseSensitive) && FindPayloadBuffer(Pass.DepthBuffer))
				{
					Diagnostics.Info(
						TEXT("DSH7326"),
						File,
						Keys.SpanOf(TEXT("Depth"), SceneTarget->Source.Span),
						FText::Format(
							LOCTEXT("MeshOwnDepthSceneTargets", "Mesh pass '{0}' writes '{1}', a texture of the scene, and tests against its own depth '{2}', which is bound from its corner: in a view that does not start at the corner of the scene's textures -- the second view of split screen, the right eye in stereo -- the depth does not reach the view's pixels, and the runtime skips the pass there."),
							FText::FromString(Pass.Name),
							FText::FromString(SceneTarget->Buffer),
							FText::FromString(Pass.DepthBuffer)));
				}
				if (Pass.Depth.Equals(TEXT("Own"), ESearchCase::CaseSensitive) && FindPayloadBuffer(Pass.DepthBuffer))
				{
					const FPipelineSizeClass Depth = SizeClassOf(Pass.DepthBuffer, InjectionIndex);
					if (!Depth.Covers(First))
					{
						Diagnostics.Error(
							TEXT("DSH7343"),
							File,
							Keys.SpanOf(TEXT("Depth"), Pass.Writes[0].Source.Span),
							FText::Format(
								LOCTEXT("MeshOwnDepthSmaller", "Mesh pass '{0}' tests against its own depth '{1}', which is {2}, and its outputs are {3}; an own depth is bound as it is, so it must be at least their size: give it their resolution and a scale no smaller than theirs, or a fixed size no smaller than theirs."),
								FText::FromString(Pass.Name),
								FText::FromString(Pass.DepthBuffer),
								FText::FromString(Depth.Describe()),
								FText::FromString(First.Describe())));
					}
				}
			}

			// V9: whatever a pass writes beside scene colour is drawn with it, at scene colour's size at this point. A mesh pass
			// is held to more by V7 above -- no buffer of the pipeline beside the scene's textures at all -- so only the pixel
			// slot's outputs are left to compare here.
			const bool bDrawsTargets = bFullscreenShader && Pass.Writes.Num() > 1;
			if (bDrawsTargets && PassWritesBuffer(Pass, SceneColorName))
			{
				const FPipelineSizeClass SceneColor = SizeClassOf(SceneColorName, InjectionIndex);
				for (const IR::FIRPassBinding& Write : Pass.Writes)
				{
					if (Write.Buffer.Equals(SceneColorName, ESearchCase::CaseSensitive))
					{
						continue;
					}
					const FPipelineSizeClass Other = SizeClassOf(Write.Buffer, InjectionIndex);
					if (!(Other == SceneColor))
					{
						Diagnostics.Error(
							TEXT("DSH7350"),
							File,
							Write.Source.Span,
							FText::Format(
								LOCTEXT("SceneColorResolution", "Pass '{0}' writes scene colour, which is {1} at {2}, and '{3}' with it, which is {4}; targets drawn together have one size."),
								FText::FromString(Pass.Name),
								FText::FromString(SceneColor.Describe()),
								FText::FromString(Pass.Injection),
								FText::FromString(Write.Buffer),
								FText::FromString(Other.Describe())));
					}
				}
			}
		}

		void FPipelineBinder::CheckFrame()
		{
			int32 FirstTonemapper = INDEX_NONE;
			for (int32 PassIndex = 0; PassIndex < Payload.Passes.Num(); ++PassIndex)
			{
				const IR::FIRPass& Pass = Payload.Passes[PassIndex];
				const int32 KindIndex = PassKindIndices.IsValidIndex(PassIndex) ? PassKindIndices[PassIndex] : INDEX_NONE;
				const int32 InjectionIndex = InjectionIndexOf(Pass);
				if (KindIndex == INDEX_NONE || InjectionIndex == INDEX_NONE)
				{
					continue;
				}

				CheckPassAtInjection(Pass, PassIndex, KindIndex, InjectionIndex);

				// V10: one tonemapper replacement, and it makes the frame's colour.
				if (InjectionIndex == Injection::PostProcessReplaceTonemapper)
				{
					const FLangSpan Span = PassKeys[PassIndex].SpanOf(TEXT("Injection"), Pipeline.PassDecls[PassIndex]->NameSpan);
					if (FirstTonemapper != INDEX_NONE)
					{
						Diagnostics.Error(
							TEXT("DSH7351"),
							File,
							Span,
							FText::Format(
								LOCTEXT("TwoTonemappers", "Pass '{0}' replaces the tonemapper, and so does '{1}'; one view runs one tonemapper, so a pipeline replaces it at most once."),
								FText::FromString(Pass.Name),
								FText::FromString(Payload.Passes[FirstTonemapper].Name)));
					}
					else
					{
						FirstTonemapper = PassIndex;
						if (!PassWritesBuffer(Pass, SceneColorName))
						{
							Diagnostics.Error(
								TEXT("DSH7351"),
								File,
								Span,
								FText::Format(
									LOCTEXT("TonemapperWritesNothing", "Pass '{0}' replaces the tonemapper, which makes the frame's final colour, and writes no 'SceneColor'; add 'write SceneColor;'."),
									FText::FromString(Pass.Name)));
						}
					}
				}
			}
		}

		void FPipelineBinder::CheckBufferLifetimes()
		{
			const TArray<int32> FrameOrder = PassesInFrameOrder(Payload);
			TArray<int32> Rank;
			Rank.Init(INDEX_NONE, Payload.Passes.Num());
			for (int32 Position = 0; Position < FrameOrder.Num(); ++Position)
			{
				Rank[FrameOrder[Position]] = Position;
			}

			for (int32 BufferIndex = 0; BufferIndex < Payload.Buffers.Num(); ++BufferIndex)
			{
				const IR::FIRPassBuffer& Buffer = Payload.Buffers[BufferIndex];
				const FLangSpan DeclSpan = Pipeline.BufferDecls.IsValidIndex(BufferIndex) ? Pipeline.BufferDecls[BufferIndex]->NameSpan : Buffer.Source.Span;

				int32 FirstWriter = INDEX_NONE;
				int32 LastWriter = INDEX_NONE;
				bool bRead = false;
				for (int32 PassIndex = 0; PassIndex < Payload.Passes.Num(); ++PassIndex)
				{
					const IR::FIRPass& Pass = Payload.Passes[PassIndex];
					if (PassWritesBuffer(Pass, Buffer.Name))
					{
						if (FirstWriter == INDEX_NONE || Rank[PassIndex] < Rank[FirstWriter])
						{
							FirstWriter = PassIndex;
						}
						if (LastWriter == INDEX_NONE || Rank[PassIndex] > Rank[LastWriter])
						{
							LastWriter = PassIndex;
						}
					}
					// An own depth is read by the very test that writes it; `.Previous` reads count as reads.
					bRead |= Pass.Depth.Equals(TEXT("Own"), ESearchCase::CaseSensitive) && Pass.DepthBuffer.Equals(Buffer.Name, ESearchCase::CaseSensitive);
					bRead |= Pass.Reads.ContainsByPredicate([&Buffer](const IR::FIRPassBinding& Read) { return Read.Buffer.Equals(Buffer.Name, ESearchCase::CaseSensitive); });
				}

				// V4: a read before every write, in frame order.
				for (int32 PassIndex = 0; PassIndex < Payload.Passes.Num(); ++PassIndex)
				{
					const IR::FIRPass& Pass = Payload.Passes[PassIndex];
					for (const IR::FIRPassBinding& Read : Pass.Reads)
					{
						if (Read.bPrevious || !Read.Buffer.Equals(Buffer.Name, ESearchCase::CaseSensitive))
						{
							continue;
						}
						const bool bBeforeEveryWrite = FirstWriter == INDEX_NONE || Rank[PassIndex] < Rank[FirstWriter];
						if (!bBeforeEveryWrite)
						{
							continue;
						}
						if (!Buffer.bClear)
						{
							Diagnostics.Error(
								TEXT("DSH7331"),
								File,
								Read.Source.Span,
								FirstWriter == INDEX_NONE
									? FText::Format(LOCTEXT("ReadNeverWrittenNoClear", "Pass '{0}' reads '{1}', which no pass writes, and '{1}' is 'Clear = None', so what it reads is undefined."), FText::FromString(Pass.Name), FText::FromString(Buffer.Name))
									: FText::Format(LOCTEXT("ReadBeforeWriteNoClear", "Pass '{0}' reads '{1}' before '{2}' writes it, and '{1}' is 'Clear = None', so what it reads is undefined; move the reader after the writer, or give the buffer a 'Clear' value."), FText::FromString(Pass.Name), FText::FromString(Buffer.Name), FText::FromString(Payload.Passes[FirstWriter].Name)));
						}
						else if (FirstWriter != INDEX_NONE)
						{
							Diagnostics.Warning(
								TEXT("DSH7332"),
								File,
								Read.Source.Span,
								FText::Format(
									LOCTEXT("ReadBeforeWrite", "Pass '{0}' reads '{1}' before '{2}' writes it in the frame, so it reads the buffer's 'Clear' value; move the reader after the writer, or read '{1}.Previous'."),
									FText::FromString(Pass.Name),
									FText::FromString(Buffer.Name),
									FText::FromString(Payload.Passes[FirstWriter].Name)));
						}
					}
				}

				// V13: a buffer with no writer, and one nobody reads or exports.
				if (FirstWriter == INDEX_NONE)
				{
					Diagnostics.Warning(
						TEXT("DSH7356"),
						File,
						DeclSpan,
						FText::Format(
							LOCTEXT("BufferNeverWritten", "No pass writes buffer '{0}', so whoever reads it reads its 'Clear' value."),
							FText::FromString(Buffer.Name)));
				}
				else if (!bRead && !Buffer.bExport)
				{
					Diagnostics.Warning(
						TEXT("DSH7357"),
						File,
						DeclSpan,
						FText::Format(
							LOCTEXT("BufferNeverRead", "No pass reads buffer '{0}' and it is not exported, so writing it is wasted work; read it, export it, or remove it."),
							FText::FromString(Buffer.Name)));
				}

				// V12: who sees an exported buffer's contents of this frame.
				if (Buffer.bExport && LastWriter != INDEX_NONE)
				{
					const int32 InjectionIndex = InjectionIndexOf(Payload.Passes[LastWriter]);
					if (InjectionIndex >= Injection::AfterBasePass)
					{
						Diagnostics.Info(
							TEXT("DSH7354"),
							File,
							DeclSpan,
							FText::Format(
								InjectionIndex >= Injection::BeforePostProcess
									? LOCTEXT("ExportLateFrame", "Buffer '{0}' is exported and last written at {1}: opaque and translucent materials that sample it see the previous frame's contents, UI sees this frame's.")
									: LOCTEXT("ExportMidFrame", "Buffer '{0}' is exported and last written at {1}: opaque materials that sample it see the previous frame's contents, translucent and post-process materials and UI see this frame's."),
								FText::FromString(Buffer.Name),
								FText::FromString(Payload.Passes[LastWriter].Injection)));
					}
				}
			}
		}

		// ------------------------------------------------------------------------------------ product

		void FPipelineBinder::BuildProduct()
		{
			FBoundProduct Product;
			Product.Kind = IR::EIRProductKind::PassPipeline;
			Product.FunctionIndex = INDEX_NONE;
			// A pipeline is no material and has no backend; Graph is what every non-graph product carries.
			Product.Backend = IR::EIRBackend::Graph;
			// Like a `.dsi`'s asset, a pipeline is named after its file.
			Product.AssetName = FPaths::GetBaseFilename(Root.FilePath);
			Bound.Products.Add(MoveTemp(Product));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// FLangBinder
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::BindPipelineModule()
	{
		LangBinderPipelinePrivate::FPipelineBinder PipelineBinder(*this);
		PipelineBinder.Run();
		CurrentFile = RootModule.FilePath;
	}

	void FLangBinder::ReportPipelinePragmaOutsideDsp(const FPragmaDecl& Pragma)
	{
		Diagnostics.Error(
			TEXT("DSH3311"),
			CurrentFile,
			Pragma.Span,
			FText::Format(
				LOCTEXT("PipelinePragmaOutsideDsp", "'#pragma pipeline' configures a Custom Pass pipeline and belongs in a '.dsp' file of its own, and this line is in '{0}'; move it, with the pipeline's buffers and passes, into a '.dsp'."),
				FText::FromString(FPaths::GetCleanFilename(CurrentFile))));
	}

	void FLangBinder::ReportPipelineDeclarationOutsideDsp(const FDecl& Decl)
	{
		if (const FHlslBlockDecl* Block = Decl.As<FHlslBlockDecl>())
		{
			Diagnostics.Error(
				TEXT("DSH3312"),
				CurrentFile,
				Block->KeywordSpan,
				FText::Format(
					LOCTEXT("HlslBlockOutsideDsp", "An 'hlsl' block at file scope is the HLSL of a Custom Pass pipeline, which only a '.dsp' file holds, and '{0}' is not one; a pipeline cannot be included."),
					FText::FromString(FPaths::GetCleanFilename(RootModule.FilePath))));
			return;
		}

		FString Word = TEXT("buffer");
		FString Name;
		FLangSpan Span = Decl.Span;
		if (const FBufferDecl* Buffer = Decl.As<FBufferDecl>())
		{
			Name = Buffer->Name;
			Span = Buffer->NameSpan;
		}
		else if (const FPassDecl* Pass = Decl.As<FPassDecl>())
		{
			Word = TEXT("pass");
			Name = Pass->Name;
			Span = Pass->NameSpan;
		}
		Diagnostics.Error(
			TEXT("DSH3312"),
			CurrentFile,
			Span,
			FText::Format(
				LOCTEXT("PipelineDeclarationOutsideDsp", "'{0} {1}' is a declaration of a Custom Pass pipeline, which only a '.dsp' file holds, and '{2}' is not one; a pipeline cannot be included."),
				FText::FromString(Word),
				FText::FromString(Name),
				FText::FromString(FPaths::GetCleanFilename(RootModule.FilePath))));
	}
}

#undef LOCTEXT_NAMESPACE
