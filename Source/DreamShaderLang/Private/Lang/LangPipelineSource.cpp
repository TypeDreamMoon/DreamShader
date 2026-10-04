// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// `.dsp` source text without the compiler (Public/Lang/LangPipelineSource.h).
//
// Three directions, like LangInstanceSource.cpp's for a `.dsi`.
//
//   * BuildDreamShaderPipelineModule turns a payload -- what the decompiler reads off a UDreamPassPipeline -- into the
//     ordinary tree, in the canonical form of DreamShader_Plan/05 s8: keys in the order of the key tables, a pass's
//     settings before its bindings and those in the order `read`, `write`, `param`. A key is left out exactly when the
//     binder would fill the same value back in -- a pass's injection point when it is the pipeline's, a buffer's
//     resolution when it is the one its first writer infers, Mode, Nanite and Dispatch at what the pass implies, Usage
//     and Views at their default sets -- by the rules Lang/LangPipelineInternal.h keeps for the binder and for this file
//     alike, so decompile -> print -> parse -> bind gives the payload back. `Threads` is the one key whose default (the
//     `.usf`'s [numthreads]) no payload carries: it is written exactly when FIRPass::bThreadsWritten says so. Inline HLSL
//     (DreamShader_Plan/10) is carried as text: the file's `hlsl` block after the buffers, a pass's after its `param`
//     lines, each as it was written; `Entry` is left out at a block's default `Main`.
//     PrintDreamShaderPipeline prints the tree; the text is deterministic.
//   * RewriteDreamShaderPipelineSource goes the other way, for Adopt: it changes an existing file to state another
//     payload and never reprints the file. Each change is an edit over the parsed spans -- one key's value, one argument
//     list, one group of bindings, one `///` block, one declaration -- and everything between the edits is the author's
//     text byte for byte. Whether a value changed is asked of its canonical text, so `Scale = HalfRes * 0.5` stays as
//     written while its value stays. Here the payload's "was written" flags (bInjectionWritten, bResolutionWritten,
//     bThreadsWritten) count as well: a key the author wrote at its default stays written, also in a declaration that
//     has to be printed anew.
//   * CompareDreamShaderPipelines says whether two payloads make the same asset: field by field, numbers at float32,
//     every value at its effective default; source references and the "was written" flags are not compared, and the
//     text of inline HLSL is compared with its line terminators as `\n`.
//
// The reference collector completes the header: the host resolves what it lists before it binds. The spelling tables
// are handed out by Semantic/LangBinderPipeline.cpp, beside the binder that reads them.
//
// Diagnostics owned by this file: none of its own range. A rewrite that cannot be made raises the codes the `.dsi`
// rewrite raises for the same failures (LangInstanceSource.cpp): DSH9107 (a `uniform a, b;` statement holds a value to
// change), DSH9108 (edits that overlap -- a fault of this file) and DSH9109 (the file is not a bound `.dsp`).

#include "Lang/LangPipelineSource.h"

#include "LangParserInternal.h"
#include "LangPipelineInternal.h"

#include "IR/IR.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangInstanceSource.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.PipelineSource"

namespace UE::DreamShader::Lang
{
	// A named namespace, not an anonymous one: the module builds as one unity blob.
	namespace Private::PipelineSource
	{
		using namespace UE::DreamShader::Lang::Private::PipelineVocabulary;

		// -----------------------------------------------------------------------------------------
		// Expressions
		// -----------------------------------------------------------------------------------------

		static FExprPtr MakePipelineIdentifier(const FString& Name)
		{
			return LegacyAst::MakeIdentifier(Name, FLangSpan());
		}

		static FExprPtr MakePipelineString(const FString& Text)
		{
			return LegacyAst::MakeStringLiteral(Text, FLangSpan());
		}

		static FExprPtr MakePipelineInt(const int64 Value)
		{
			return LegacyAst::MakeIntLiteral(Value, FLangSpan());
		}

		/** The shortest text that reads back as the same float32; a negative one as `-` before it. */
		static FExprPtr MakePipelineFloat(const double Value)
		{
			return LegacyAst::MakeFloatLiteral(FormatDreamShaderFloatLiteral(Value), Value, FLangSpan());
		}

		static FExprPtr MakePipelineBool(const bool bValue)
		{
			return LegacyAst::MakeBoolLiteral(bValue, FLangSpan());
		}

		/** A word the 2.0 lexer reads as one identifier that is neither a keyword nor a type. */
		static bool IsPipelineWord(const FString& Text)
		{
			ELangKeyword Keyword = ELangKeyword::None;
			return IsPipelineIdentifierText(Text) && !TryGetLangKeyword(Text, Keyword) && !FLangParser::IsBuiltinTypeName(Text);
		}

		/** A name as an identifier, or as a string when the lexer would not read it as a plain word (`"My Layer"`). */
		static FExprPtr MakePipelineWordOrString(const FString& Text)
		{
			return IsPipelineWord(Text) ? MakePipelineIdentifier(Text) : MakePipelineString(Text);
		}

		static FExprPtr MakePipelineBinary(const EBinaryOp Op, FExprPtr Left, FExprPtr Right)
		{
			TUniquePtr<FBinaryExpr> Binary = MakeUnique<FBinaryExpr>();
			Binary->Op = Op;
			Binary->Left = MoveTemp(Left);
			Binary->Right = MoveTemp(Right);
			return Binary;
		}

		/** `Name(a, b)` with an identifier callee; a null argument is left out. */
		static FExprPtr MakePipelineCall(const FString& Callee, TArray<FExprPtr>&& Arguments)
		{
			TUniquePtr<FCallExpr> Call = MakeUnique<FCallExpr>();
			Call->Callee = MakePipelineIdentifier(Callee);
			for (FExprPtr& Argument : Arguments)
			{
				if (Argument)
				{
					FArgument& Entry = Call->Arguments.AddDefaulted_GetRef();
					Entry.Value = MoveTemp(Argument);
				}
			}
			return Call;
		}

		static FExprPtr MakePipelineCall(const FString& Callee, FExprPtr Argument)
		{
			TArray<FExprPtr> Arguments;
			Arguments.Add(MoveTemp(Argument));
			return MakePipelineCall(Callee, MoveTemp(Arguments));
		}

		/** `uint3(8, 8, 1)`, `float2(0.5, 1.0)`: a constructor of numbers. */
		static FExprPtr MakePipelineConstructor(const FString& TypeName, const double* Values, const int32 Count, const bool bIntegers)
		{
			TUniquePtr<FTypeExpr> Callee = MakeUnique<FTypeExpr>();
			FLangParser::ClassifyTypeName(TypeName, Callee->Type);
			TUniquePtr<FCallExpr> Call = MakeUnique<FCallExpr>();
			Call->Callee = MoveTemp(Callee);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FArgument& Entry = Call->Arguments.AddDefaulted_GetRef();
				Entry.Value = bIntegers ? MakePipelineInt(static_cast<int64>(FMath::RoundToDouble(Values[Index]))) : MakePipelineFloat(Values[Index]);
			}
			return Call;
		}

		/** `a | b | c`; null for no words. */
		static FExprPtr MakePipelineBarList(const TArray<FString>& Words)
		{
			FExprPtr Result;
			for (const FString& Word : Words)
			{
				FExprPtr Leaf = MakePipelineWordOrString(Word);
				Result = Result ? MakePipelineBinary(EBinaryOp::BitwiseOr, MoveTemp(Result), MoveTemp(Leaf)) : MoveTemp(Leaf);
			}
			return Result;
		}

		/** `PostProcess.AfterDOF` as the member expression the parser makes of it, `BeginView` as an identifier. */
		static FExprPtr MakePipelineDottedName(const FString& Name)
		{
			TArray<FString> Parts;
			Name.ParseIntoArray(Parts, TEXT("."), /* bCullEmpty */ true);
			FExprPtr Result;
			for (const FString& Part : Parts)
			{
				Result = Result ? LegacyAst::MakeMember(MoveTemp(Result), Part, FLangSpan()) : MakePipelineIdentifier(Part);
			}
			return Result ? MoveTemp(Result) : MakePipelineIdentifier(Name);
		}

		/** A colour-like value: one number when the four channels are one (the binder broadcasts it back), `float4(...)` otherwise. */
		static FExprPtr MakePipelineColor(const double Value[4])
		{
			if (SameFloat(Value[0], Value[1]) && SameFloat(Value[0], Value[2]) && SameFloat(Value[0], Value[3]))
			{
				return MakePipelineFloat(Value[0]);
			}
			return MakePipelineConstructor(TEXT("float4"), Value, 4, /* bIntegers */ false);
		}

		/** A number in a `///` directive: `16` for a whole one, the float text otherwise. */
		static FString PipelineNumberText(const double Value)
		{
			if (FMath::Abs(Value) < 1.0e9 && FMath::RoundToDouble(Value) == Value)
			{
				return FString::Printf(TEXT("%lld"), static_cast<long long>(Value));
			}
			return FormatDreamShaderFloatLiteral(Value);
		}

		/** 2, 3 or 4 for `float2` .. `float4`; 1 for anything else. */
		static int32 PipelineFloatWidth(const FString& Type)
		{
			if (Type.Equals(TEXT("float2"), ESearchCase::CaseSensitive))
			{
				return 2;
			}
			if (Type.Equals(TEXT("float3"), ESearchCase::CaseSensitive))
			{
				return 3;
			}
			if (Type.Equals(TEXT("float4"), ESearchCase::CaseSensitive))
			{
				return 4;
			}
			return 1;
		}

		/** The float channels of a value: Float4 as the payload holds them, Float or a number of another kind as channel 0. */
		static void PipelineChannels(const IR::FIRPropertyValue& Value, double OutChannels[4])
		{
			for (int32 Index = 0; Index < 4; ++Index)
			{
				OutChannels[Index] = 0.0;
			}
			switch (Value.Kind)
			{
			case IR::EIRPropertyKind::Float4:
				for (int32 Index = 0; Index < 4; ++Index)
				{
					OutChannels[Index] = Value.V[Index];
				}
				break;
			case IR::EIRPropertyKind::Float:
				OutChannels[0] = Value.F;
				break;
			case IR::EIRPropertyKind::Int:
				OutChannels[0] = static_cast<double>(Value.I);
				break;
			case IR::EIRPropertyKind::Bool:
				OutChannels[0] = Value.B ? 1.0 : 0.0;
				break;
			default:
				break;
			}
		}

		static int64 PipelineIntOf(const IR::FIRPropertyValue& Value)
		{
			if (Value.Kind == IR::EIRPropertyKind::Int)
			{
				return Value.I;
			}
			double Channels[4];
			PipelineChannels(Value, Channels);
			return static_cast<int64>(FMath::RoundToDouble(Channels[0]));
		}

		static bool PipelineBoolOf(const IR::FIRPropertyValue& Value)
		{
			if (Value.Kind == IR::EIRPropertyKind::Bool)
			{
				return Value.B;
			}
			double Channels[4];
			PipelineChannels(Value, Channels);
			return Channels[0] != 0.0;
		}

		static FString PrintPipelineExpr(const FExprPtr& Expr)
		{
			return Expr ? PrintDreamShaderLangExpr(*Expr) : FString();
		}

		// -----------------------------------------------------------------------------------------
		// The canonical tree
		// -----------------------------------------------------------------------------------------

		static void AddPipelineDirective(FDocBlock& Doc, const TCHAR* Key, const FString& Value)
		{
			FDocDirective Entry;
			Entry.Key = Key;
			Entry.Value = Value;
			Doc.Directives.Add(MoveTemp(Entry));
		}

		/** `@desc` for a description of one line, free text for a longer one: the binder reads either back as the description. */
		static void AddPipelineDescription(FDocBlock& Doc, const FString& Description)
		{
			FString Text = Description.Replace(TEXT("\r\n"), TEXT("\n"));
			Text.TrimStartAndEndInline();
			if (Text.IsEmpty())
			{
				return;
			}
			if (!Text.Contains(TEXT("\n")))
			{
				AddPipelineDirective(Doc, Directive::Desc, Text);
				return;
			}
			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines, /* bCullEmpty */ false);
			for (const FString& Line : Lines)
			{
				Doc.FreeText.Add(Line.TrimEnd());
			}
		}

		static bool IsPipelineDefaultViews(const TArray<FString>& Views)
		{
			return Views.Num() == static_cast<int32>(UE_ARRAY_COUNT(DefaultViews))
				&& Views[0].Equals(DefaultViews[0], ESearchCase::CaseSensitive)
				&& Views[1].Equals(DefaultViews[1], ESearchCase::CaseSensitive);
		}

		static TArray<FString> PipelineRequires(const IR::FIRPassPipeline& Pipeline)
		{
			TArray<FString> Requires;
			for (const TCHAR* Name : RequirementNames)
			{
				if (Pipeline.Requires.ContainsByPredicate([Name](const FString& Written) { return Written.Equals(Name, ESearchCase::CaseSensitive); }))
				{
					Requires.Add(Name);
				}
			}
			return Requires;
		}

		/** One key of `#pragma pipeline(...)` as the pragma spells it; empty at its default. */
		static FString PragmaKeyValue(const IR::FIRPassPipeline& Pipeline, const FString& Key)
		{
			if (Key.Equals(TEXT("Order"), ESearchCase::CaseSensitive))
			{
				return Pipeline.Order != 0 ? FString::FromInt(Pipeline.Order) : FString();
			}
			if (Key.Equals(TEXT("Injection"), ESearchCase::CaseSensitive))
			{
				return (Pipeline.DefaultInjection.IsEmpty() || Pipeline.DefaultInjection.Equals(DefaultInjectionName, ESearchCase::CaseSensitive))
					? FString()
					: Pipeline.DefaultInjection;
			}
			if (Key.Equals(TEXT("Views"), ESearchCase::CaseSensitive))
			{
				const TArray<FString> Views = EffectiveViews(Pipeline);
				return IsPipelineDefaultViews(Views) ? FString() : FString::Join(Views, TEXT(" | "));
			}
			if (Key.Equals(TEXT("Requires"), ESearchCase::CaseSensitive))
			{
				return FString::Join(PipelineRequires(Pipeline), TEXT(" | "));
			}
			if (Key.Equals(TEXT("Enabled"), ESearchCase::CaseSensitive))
			{
				return Pipeline.EnabledParameter;
			}
			return FString();
		}

		/** `#pragma pipeline(...)` with the keys not at their default, in the key table's order; null when all are. */
		static TUniquePtr<FPragmaDecl> BuildPipelinePragma(const IR::FIRPassPipeline& Pipeline)
		{
			TUniquePtr<FPragmaDecl> Pragma = MakeUnique<FPragmaDecl>();
			Pragma->PragmaKind = EPragmaKind::Pipeline;
			Pragma->Name = TEXT("pipeline");
			for (const TCHAR* Key : PragmaKeys)
			{
				const FString Value = PragmaKeyValue(Pipeline, Key);
				if (!Value.IsEmpty())
				{
					FPragmaArgument& Argument = Pragma->Arguments.AddDefaulted_GetRef();
					Argument.Key = Key;
					Argument.Value = Value;
					Argument.bQuoted = false;
				}
			}
			if (Pragma->Arguments.Num() == 0)
			{
				return nullptr;
			}
			return Pragma;
		}

		/** The initializer of a parameter: by its type; null for a texture, and at a zero default unless bEvenAtZero. */
		static FExprPtr MakePipelineParameterInitializer(const IR::FIRPassParameter& Parameter, const bool bEvenAtZero)
		{
			const FString& Type = Parameter.Type;
			if (Type.Equals(TEXT("Texture2D"), ESearchCase::CaseSensitive))
			{
				return nullptr;
			}
			if (Type.Equals(TEXT("int"), ESearchCase::CaseSensitive))
			{
				const int64 Value = PipelineIntOf(Parameter.Default);
				return (Value != 0 || bEvenAtZero) ? MakePipelineInt(Value) : nullptr;
			}
			if (Type.Equals(TEXT("bool"), ESearchCase::CaseSensitive))
			{
				const bool bValue = PipelineBoolOf(Parameter.Default);
				return (bValue || bEvenAtZero) ? MakePipelineBool(bValue) : nullptr;
			}

			double Channels[4];
			PipelineChannels(Parameter.Default, Channels);
			const int32 Width = PipelineFloatWidth(Type);
			bool bZero = true;
			for (int32 Index = 0; Index < Width; ++Index)
			{
				bZero &= Channels[Index] == 0.0;
			}
			if (bZero && !bEvenAtZero)
			{
				return nullptr;
			}
			return Width == 1
				? MakePipelineFloat(Channels[0])
				: MakePipelineConstructor(FString::Printf(TEXT("float%d"), Width), Channels, Width, /* bIntegers */ false);
		}

		/** `/// @group ...` and `uniform float4 Tint = float4(...);` for one parameter. */
		static TUniquePtr<FVariableDecl> BuildPipelineParameterDecl(const IR::FIRPassParameter& Parameter)
		{
			TUniquePtr<FVariableDecl> Variable = MakeUnique<FVariableDecl>();
			Variable->Storage = EStorageClass::Uniform;
			FLangParser::ClassifyTypeName(Parameter.Type.IsEmpty() ? FString(TEXT("float")) : Parameter.Type, Variable->Type);
			Variable->Declarator.Name = Parameter.Name;

			if (!Parameter.Group.IsEmpty())
			{
				AddPipelineDirective(Variable->Doc, Directive::Group, Parameter.Group);
			}
			AddPipelineDescription(Variable->Doc, Parameter.Description);
			if (Parameter.bHasSlider)
			{
				AddPipelineDirective(Variable->Doc, Directive::Slider, PipelineNumberText(Parameter.SliderMin) + TEXT(" ") + PipelineNumberText(Parameter.SliderMax));
			}
			if (Parameter.SortPriority != 0)
			{
				AddPipelineDirective(Variable->Doc, Directive::Sort, FString::FromInt(Parameter.SortPriority));
			}
			if (Parameter.Default.Kind == IR::EIRPropertyKind::Object && !Parameter.Default.S.TrimStartAndEnd().IsEmpty())
			{
				AddPipelineDirective(Variable->Doc, Directive::Default, Parameter.Default.S.TrimStartAndEnd());
			}

			Variable->Declarator.Initializer = MakePipelineParameterInitializer(Parameter, /* bEvenAtZero */ false);
			return Variable;
		}

		/**
		 * One argument of a buffer in its canonical spelling; null at its default. bKeepWrittenDefaults also writes a
		 * `Resolution` the source wrote at the inferred value (Adopt).
		 */
		static FExprPtr MakeBufferKeyValue(const IR::FIRPassPipeline& Pipeline, const IR::FIRPassBuffer& Buffer, const FString& Key, const bool bKeepWrittenDefaults)
		{
			const auto Is = [&Key](const TCHAR* Name) { return Key.Equals(Name, ESearchCase::CaseSensitive); };
			const bool bFixed = Buffer.Resolution.Equals(ResolutionFixed, ESearchCase::CaseSensitive);

			if (Is(TEXT("Scale")))
			{
				return (!bFixed && !SameFloat(Buffer.Scale, 1.0)) ? MakePipelineFloat(Buffer.Scale) : nullptr;
			}
			if (Is(TEXT("Size")))
			{
				if (!bFixed)
				{
					return nullptr;
				}
				const double Size[2] = { static_cast<double>(Buffer.FixedWidth), static_cast<double>(Buffer.FixedHeight) };
				return MakePipelineConstructor(TEXT("int2"), Size, 2, /* bIntegers */ true);
			}
			if (Is(TEXT("Resolution")))
			{
				if (bFixed || Buffer.Resolution.IsEmpty())
				{
					return nullptr;
				}
				const bool bInferred = Buffer.Resolution.Equals(InferBufferResolution(Pipeline, Buffer.Name), ESearchCase::CaseSensitive);
				return (!bInferred || (bKeepWrittenDefaults && Buffer.bResolutionWritten)) ? MakePipelineIdentifier(Buffer.Resolution) : nullptr;
			}
			if (Is(TEXT("Clear")))
			{
				if (!Buffer.bClear)
				{
					return MakePipelineIdentifier(TEXT("None"));
				}
				return IsZero4(Buffer.ClearValue) ? nullptr : MakePipelineColor(Buffer.ClearValue);
			}
			if (Is(TEXT("Mips")))
			{
				return Buffer.Mips != 1 ? MakePipelineInt(Buffer.Mips) : nullptr;
			}
			if (Is(TEXT("History")))
			{
				return Buffer.bHistory ? MakePipelineBool(true) : nullptr;
			}
			if (Is(TEXT("Export")))
			{
				return Buffer.bExport ? MakePipelineBool(true) : nullptr;
			}
			return nullptr;
		}

		/** `buffer Name : Format(Key = Value, ...);` with the arguments not at their default, in the key table's order. */
		static TUniquePtr<FBufferDecl> BuildPipelineBufferDecl(const IR::FIRPassPipeline& Pipeline, const IR::FIRPassBuffer& Buffer, const bool bKeepWrittenDefaults)
		{
			TUniquePtr<FBufferDecl> Decl = MakeUnique<FBufferDecl>();
			Decl->Name = Buffer.Name;
			Decl->Format = Buffer.Format;
			AddPipelineDescription(Decl->Doc, Buffer.Description);
			for (const TCHAR* Key : BufferKeys)
			{
				if (FExprPtr Value = MakeBufferKeyValue(Pipeline, Buffer, Key, bKeepWrittenDefaults))
				{
					FPipelineKeyValue& Argument = Decl->Arguments.AddDefaulted_GetRef();
					Argument.Key = Key;
					Argument.Value = MoveTemp(Value);
				}
			}
			Decl->bHasArgumentList = Decl->Arguments.Num() > 0;
			return Decl;
		}

		static FExprPtr MakePipelineFilterTerm(const IR::FIRPassFilterTerm& Term)
		{
			if (Term.Kind.Equals(FilterStencil, ESearchCase::CaseSensitive))
			{
				TArray<FExprPtr> Arguments;
				Arguments.Add(MakePipelineInt(Term.StencilValue));
				if (Term.StencilMask != 255)
				{
					Arguments.Add(MakePipelineInt(Term.StencilMask));
				}
				return MakePipelineCall(FilterStencil, MoveTemp(Arguments));
			}
			if (Term.Kind.Equals(FilterLayer, ESearchCase::CaseSensitive))
			{
				return MakePipelineCall(FilterLayer, MakePipelineBarList(Term.Layers));
			}
			return MakePipelineCall(FilterList, MakePipelineWordOrString(Term.List));
		}

		/** The normal form back as text: clauses joined by `|`, the terms of one by `&`, which binds tighter. */
		static FExprPtr MakePipelineFilter(const TArray<IR::FIRPassFilterClause>& Filter)
		{
			FExprPtr Result;
			for (const IR::FIRPassFilterClause& Clause : Filter)
			{
				FExprPtr ClauseExpr;
				for (const IR::FIRPassFilterTerm& Term : Clause.AllOf)
				{
					FExprPtr TermExpr = MakePipelineFilterTerm(Term);
					ClauseExpr = ClauseExpr ? MakePipelineBinary(EBinaryOp::BitwiseAnd, MoveTemp(ClauseExpr), MoveTemp(TermExpr)) : MoveTemp(TermExpr);
				}
				if (ClauseExpr)
				{
					Result = Result ? MakePipelineBinary(EBinaryOp::BitwiseOr, MoveTemp(Result), MoveTemp(ClauseExpr)) : MoveTemp(ClauseExpr);
				}
			}
			return Result;
		}

		/**
		 * One key of a pass in its canonical spelling; null when the binder would fill the same value back in, or when the
		 * key is not one of the pass's. bKeepWrittenDefaults also writes an `Injection` the source wrote at the pipeline's
		 * default (Adopt). `Threads` follows bThreadsWritten either way: its default lives in the `.usf`.
		 */
		static FExprPtr MakePassKeyValue(const IR::FIRPassPipeline& Pipeline, const IR::FIRPass& Pass, const FString& Key, const bool bKeepWrittenDefaults)
		{
			const auto Is = [&Key](const TCHAR* Name) { return Key.Equals(Name, ESearchCase::CaseSensitive); };

			if (Is(TEXT("Injection")))
			{
				if (Pass.Injection.IsEmpty())
				{
					return nullptr;
				}
				const bool bDefault = Pass.Injection.Equals(Pipeline.DefaultInjection, ESearchCase::CaseSensitive);
				return (!bDefault || (bKeepWrittenDefaults && Pass.bInjectionWritten)) ? MakePipelineDottedName(Pass.Injection) : nullptr;
			}
			if (Is(TEXT("Enabled")))
			{
				return Pass.EnabledParameter.IsEmpty() ? nullptr : MakePipelineIdentifier(Pass.EnabledParameter);
			}
			if (Is(TEXT("Material")))
			{
				const FString& Reference = !Pass.MaterialReference.IsEmpty() ? Pass.MaterialReference : Pass.MaterialObjectPath;
				return Reference.IsEmpty() ? nullptr : MakePipelineString(Reference);
			}
			if (Is(TEXT("Shader")))
			{
				const FString& Reference = !Pass.ShaderReference.IsEmpty() ? Pass.ShaderReference : Pass.ShaderVirtualPath;
				return Reference.IsEmpty() ? nullptr : MakePipelineString(Reference);
			}
			if (Is(TEXT("Entry")))
			{
				// A pass's own block of whole functions runs `Main` unless it says otherwise.
				const bool bBlockDefault = Pass.HlslSource.Equals(IR::PassHlslSource::Block, ESearchCase::CaseSensitive)
					&& Pass.Entry.Equals(TEXT("Main"), ESearchCase::CaseSensitive);
				return (Pass.Entry.IsEmpty() || bBlockDefault) ? nullptr : MakePipelineWordOrString(Pass.Entry);
			}
			if (Is(TEXT("Threads")))
			{
				if (!Pass.bThreadsWritten)
				{
					return nullptr;
				}
				const double Threads[3] = { static_cast<double>(Pass.ThreadsX), static_cast<double>(Pass.ThreadsY), static_cast<double>(Pass.ThreadsZ) };
				return MakePipelineConstructor(TEXT("uint3"), Threads, 3, /* bIntegers */ true);
			}
			if (Is(TEXT("Dispatch")))
			{
				if (Pass.DispatchMode.Equals(TEXT("Fixed"), ESearchCase::CaseSensitive))
				{
					const double Size[3] = { static_cast<double>(Pass.DispatchX), static_cast<double>(Pass.DispatchY), static_cast<double>(Pass.DispatchZ) };
					return MakePipelineConstructor(TEXT("uint3"), Size, 3, /* bIntegers */ true);
				}
				if (Pass.DispatchBuffer.IsEmpty())
				{
					return nullptr;
				}
				const bool bUnit = SameFloat(Pass.DispatchScale, 1.0) || Pass.DispatchScale <= 0.0;
				if (bUnit && Pass.DispatchBuffer.Equals(DefaultDispatchBuffer(Pass), ESearchCase::CaseSensitive))
				{
					return nullptr;
				}
				FExprPtr Buffer = MakePipelineIdentifier(Pass.DispatchBuffer);
				if (bUnit)
				{
					return Buffer;
				}
				// `B / 2` when it reads back as the same float32 (the binder takes 1 / 2), `B * s` otherwise.
				const double Divisor = FMath::RoundToDouble(1.0 / Pass.DispatchScale);
				if (Pass.DispatchScale < 1.0 && Divisor >= 2.0 && Divisor < 1.0e6 && SameFloat(1.0 / Divisor, Pass.DispatchScale))
				{
					return MakePipelineBinary(EBinaryOp::Divide, MoveTemp(Buffer), MakePipelineInt(static_cast<int64>(Divisor)));
				}
				return MakePipelineBinary(EBinaryOp::Multiply, MoveTemp(Buffer), MakePipelineFloat(Pass.DispatchScale));
			}
			if (Is(TEXT("Filter")))
			{
				return Pass.Filter.Num() == 0 ? nullptr : MakePipelineFilter(Pass.Filter);
			}
			if (Is(TEXT("Mode")))
			{
				const FString Mode = EffectiveMeshMode(Pass);
				return Mode.Equals(DefaultMeshMode(Pass), ESearchCase::CaseSensitive) ? nullptr : MakePipelineIdentifier(Mode);
			}
			if (Is(TEXT("Depth")))
			{
				if (Pass.Depth.IsEmpty() || Pass.Depth.Equals(TEXT("TestScene"), ESearchCase::CaseSensitive))
				{
					return nullptr;
				}
				if (Pass.Depth.Equals(TEXT("Own"), ESearchCase::CaseSensitive))
				{
					return MakePipelineCall(TEXT("Own"), MakePipelineIdentifier(Pass.DepthBuffer));
				}
				return MakePipelineIdentifier(Pass.Depth);
			}
			if (Is(TEXT("Cull")))
			{
				return (Pass.Cull.IsEmpty() || Pass.Cull.Equals(TEXT("Auto"), ESearchCase::CaseSensitive)) ? nullptr : MakePipelineIdentifier(Pass.Cull);
			}
			if (Is(TEXT("Blend")))
			{
				return (Pass.Blend.IsEmpty() || Pass.Blend.Equals(TEXT("Replace"), ESearchCase::CaseSensitive)) ? nullptr : MakePipelineIdentifier(Pass.Blend);
			}
			if (Is(TEXT("Usage")))
			{
				const TArray<FString> Usage = EffectiveUsage(Pass);
				bool bDefault = Usage.Num() == static_cast<int32>(UE_ARRAY_COUNT(DefaultUsage));
				for (int32 Index = 0; bDefault && Index < Usage.Num(); ++Index)
				{
					bDefault = Usage[Index].Equals(DefaultUsage[Index], ESearchCase::CaseSensitive);
				}
				return (bDefault || Usage.Num() == 0) ? nullptr : MakePipelineBarList(Usage);
			}
			if (Is(TEXT("Nanite")))
			{
				const FString Policy = Pass.Nanite.IsEmpty() ? DefaultNanitePolicy(Pass) : Pass.Nanite;
				if (Policy.Equals(TEXT("AssignStencil"), ESearchCase::CaseSensitive))
				{
					return MakePipelineCall(TEXT("AssignStencil"), MakePipelineInt(Pass.AssignedStencilValue));
				}
				return Policy.Equals(DefaultNanitePolicy(Pass), ESearchCase::CaseSensitive) ? nullptr : MakePipelineIdentifier(Policy);
			}
			if (Is(TEXT("NaniteValue")))
			{
				const FString Policy = Pass.Nanite.IsEmpty() ? DefaultNanitePolicy(Pass) : Pass.Nanite;
				static const double DefaultNaniteValue[4] = { 1.0, 0.0, 0.0, 0.0 };
				if (Policy.Equals(TEXT("Skip"), ESearchCase::CaseSensitive) || SameFloat4(Pass.NaniteValue, DefaultNaniteValue))
				{
					return nullptr;
				}
				return MakePipelineColor(Pass.NaniteValue);
			}
			if (Is(TEXT("Value")))
			{
				return IsZero4(Pass.ClearValue) ? nullptr : MakePipelineColor(Pass.ClearValue);
			}
			return nullptr;
		}

		/** The keys of a pass in the canonical order: the common ones, then its kind's. */
		static TArray<FString> PipelinePassKeys(const IR::FIRPass& Pass)
		{
			TArray<FString> Keys;
			for (const TCHAR* Key : CommonPassKeys)
			{
				Keys.Add(Key);
			}
			for (const TCHAR* Key : KeysOfKind(FindName(PassKinds, Pass.Kind)))
			{
				Keys.Add(Key);
			}
			return Keys;
		}

		/** The value of a `param`: the constant by its type, or the source times the multiplier plus the offset, identities left out. */
		static FExprPtr MakePipelineParamValue(const IR::FIRPassParam& Param)
		{
			if (Param.SourceKind.Equals(TEXT("Constant"), ESearchCase::CaseSensitive))
			{
				const IR::FIRPropertyValue& Constant = Param.Constant;
				const bool bUntyped = Param.ConstantType.IsEmpty();
				if (Param.ConstantType.Equals(TEXT("int"), ESearchCase::CaseSensitive) || (bUntyped && Constant.Kind == IR::EIRPropertyKind::Int))
				{
					return MakePipelineInt(PipelineIntOf(Constant));
				}
				if (Param.ConstantType.Equals(TEXT("bool"), ESearchCase::CaseSensitive) || (bUntyped && Constant.Kind == IR::EIRPropertyKind::Bool))
				{
					return MakePipelineBool(PipelineBoolOf(Constant));
				}
				double Channels[4];
				PipelineChannels(Constant, Channels);
				const int32 Width = bUntyped
					? (Constant.Kind == IR::EIRPropertyKind::Float4 ? FMath::Clamp(Constant.N, 1, 4) : 1)
					: PipelineFloatWidth(Param.ConstantType);
				return Width == 1
					? MakePipelineFloat(Channels[0])
					: MakePipelineConstructor(FString::Printf(TEXT("float%d"), Width), Channels, Width, /* bIntegers */ false);
			}

			const bool bWeight = Param.SourceKind.Equals(TEXT("Weight"), ESearchCase::CaseSensitive);
			FExprPtr Value = MakePipelineIdentifier(bWeight ? FString(WeightParameterName) : Param.Parameter);
			if (!SameFloat(Param.Multiplier, 1.0))
			{
				Value = MakePipelineBinary(EBinaryOp::Multiply, MoveTemp(Value), MakePipelineFloat(Param.Multiplier));
			}
			if (!SameFloat(Param.Offset, 0.0))
			{
				Value = Param.Offset < 0.0
					? MakePipelineBinary(EBinaryOp::Subtract, MoveTemp(Value), MakePipelineFloat(-Param.Offset))
					: MakePipelineBinary(EBinaryOp::Add, MoveTemp(Value), MakePipelineFloat(Param.Offset));
			}
			return Value;
		}

		static TUniquePtr<FPassStmt> MakePipelineSettingStatement(const FString& Key, FExprPtr Value)
		{
			TUniquePtr<FPassStmt> Statement = MakeUnique<FPassStmt>();
			Statement->StmtKind = EPassStmtKind::Setting;
			Statement->Name = Key;
			Statement->Value = MoveTemp(Value);
			return Statement;
		}

		static TUniquePtr<FPassStmt> MakePipelineBindingStatement(const bool bRead, const IR::FIRPassBinding& Binding)
		{
			TUniquePtr<FPassStmt> Statement = MakeUnique<FPassStmt>();
			Statement->StmtKind = bRead ? EPassStmtKind::Read : EPassStmtKind::Write;
			// `read B;` when the name inside the pass is the buffer's own (an empty slot is the runtime's spelling of that).
			Statement->Name = (Binding.Slot.IsEmpty() || Binding.Slot.Equals(Binding.Buffer, ESearchCase::CaseSensitive)) ? FString() : Binding.Slot;
			Statement->Buffer = Binding.Buffer;
			Statement->bPrevious = Binding.bPrevious;
			return Statement;
		}

		static TUniquePtr<FPassStmt> MakePipelineParamStatement(const IR::FIRPassParam& Param)
		{
			TUniquePtr<FPassStmt> Statement = MakeUnique<FPassStmt>();
			Statement->StmtKind = EPassStmtKind::Param;
			Statement->Name = Param.Target;
			Statement->Value = MakePipelineParamValue(Param);
			return Statement;
		}

		/** A pass whose code is its own `hlsl` block (Block or Body): the block, as it was written. */
		static bool PassHasOwnHlslBlock(const IR::FIRPass& Pass)
		{
			return Pass.HlslSource.Equals(IR::PassHlslSource::Block, ESearchCase::CaseSensitive)
				|| Pass.HlslSource.Equals(IR::PassHlslSource::Body, ESearchCase::CaseSensitive);
		}

		static TUniquePtr<FPassStmt> MakePipelineHlslStatement(const FString& RawBody)
		{
			TUniquePtr<FPassStmt> Statement = MakeUnique<FPassStmt>();
			Statement->StmtKind = EPassStmtKind::Hlsl;
			Statement->Name = TEXT("hlsl");
			Statement->RawBody = RawBody;
			return Statement;
		}

		/** Text of inline HLSL as the comparison reads it, and as a rewrite compares it: line terminators as `\n`. */
		static FString NormalizePipelineHlsl(const FString& Text)
		{
			return Text.Replace(TEXT("\r\n"), TEXT("\n"), ESearchCase::CaseSensitive);
		}

		/** Text of inline HLSL with the line terminators of the file it goes into. */
		static FString PipelineHlslWithNewLine(const FString& Text, const FString& NewLine)
		{
			FString Result = NormalizePipelineHlsl(Text);
			if (!NewLine.Equals(TEXT("\n"), ESearchCase::CaseSensitive))
			{
				Result.ReplaceInline(TEXT("\n"), *NewLine, ESearchCase::CaseSensitive);
			}
			return Result;
		}

		/** `pass Name : kind { settings; reads; writes; params; hlsl { ... } }` in the canonical order. */
		static TUniquePtr<FPassDecl> BuildPipelinePassDecl(const IR::FIRPassPipeline& Pipeline, const IR::FIRPass& Pass, const bool bKeepWrittenDefaults)
		{
			TUniquePtr<FPassDecl> Decl = MakeUnique<FPassDecl>();
			Decl->Name = Pass.Name;
			Decl->PassKind = Pass.Kind;
			AddPipelineDescription(Decl->Doc, Pass.Description);

			for (const FString& Key : PipelinePassKeys(Pass))
			{
				if (FExprPtr Value = MakePassKeyValue(Pipeline, Pass, Key, bKeepWrittenDefaults))
				{
					Decl->Statements.Add(MakePipelineSettingStatement(Key, MoveTemp(Value)));
				}
			}
			for (const IR::FIRPassBinding& Read : Pass.Reads)
			{
				Decl->Statements.Add(MakePipelineBindingStatement(/* bRead */ true, Read));
			}
			for (const IR::FIRPassBinding& Write : Pass.Writes)
			{
				Decl->Statements.Add(MakePipelineBindingStatement(/* bRead */ false, Write));
			}
			for (const IR::FIRPassParam& Param : Pass.Params)
			{
				Decl->Statements.Add(MakePipelineParamStatement(Param));
			}
			if (PassHasOwnHlslBlock(Pass))
			{
				Decl->Statements.Add(MakePipelineHlslStatement(Pass.InlineHlsl));
			}
			return Decl;
		}

		// -----------------------------------------------------------------------------------------
		// Text positions
		// -----------------------------------------------------------------------------------------

		static FString DetectPipelineNewLine(const FString& Text)
		{
			const int32 LineFeed = Text.Find(TEXT("\n"));
			return (LineFeed > 0 && Text[LineFeed - 1] == TEXT('\r')) ? FString(TEXT("\r\n")) : FString(TEXT("\n"));
		}

		static int32 PipelineLineStart(const FString& Text, int32 Offset)
		{
			Offset = FMath::Clamp(Offset, 0, Text.Len());
			while (Offset > 0 && Text[Offset - 1] != TEXT('\n') && Text[Offset - 1] != TEXT('\r'))
			{
				--Offset;
			}
			return Offset;
		}

		/** Just past the line terminator of the line holding Offset; the text's end on the last line. */
		static int32 PipelineLineEndWithTerminator(const FString& Text, int32 Offset)
		{
			Offset = FMath::Clamp(Offset, 0, Text.Len());
			while (Offset < Text.Len() && Text[Offset] != TEXT('\n') && Text[Offset] != TEXT('\r'))
			{
				++Offset;
			}
			if (Offset < Text.Len() && Text[Offset] == TEXT('\r'))
			{
				++Offset;
			}
			if (Offset < Text.Len() && Text[Offset] == TEXT('\n'))
			{
				++Offset;
			}
			return Offset;
		}

		static FString PipelineLineIndent(const FString& Text, const int32 Offset)
		{
			const int32 Start = PipelineLineStart(Text, Offset);
			int32 End = Start;
			while (End < Text.Len() && (Text[End] == TEXT(' ') || Text[End] == TEXT('\t')))
			{
				++End;
			}
			return Text.Mid(Start, End - Start);
		}

		/** Whether only blanks are between the start of Offset's line and Offset. */
		static bool IsPipelineBlankBefore(const FString& Text, const int32 Offset)
		{
			for (int32 Index = PipelineLineStart(Text, Offset); Index < Offset && Index < Text.Len(); ++Index)
			{
				if (!FChar::IsWhitespace(Text[Index]))
				{
					return false;
				}
			}
			return true;
		}

		/** Whether only blanks, or blanks and a `//` comment, are between Offset and the end of its line. */
		static bool IsPipelineBlankAfter(const FString& Text, const int32 Offset)
		{
			int32 Index = FMath::Clamp(Offset, 0, Text.Len());
			while (Index < Text.Len() && Text[Index] != TEXT('\n') && Text[Index] != TEXT('\r'))
			{
				if (Text[Index] == TEXT('/') && Index + 1 < Text.Len() && Text[Index + 1] == TEXT('/'))
				{
					return true;
				}
				if (!FChar::IsWhitespace(Text[Index]))
				{
					return false;
				}
				++Index;
			}
			return true;
		}

		static bool IsPipelineAloneOnItsLines(const FString& Text, const int32 Start, const int32 End)
		{
			return IsPipelineBlankBefore(Text, Start) && IsPipelineBlankAfter(Text, End);
		}

		/** Where a declaration's text starts: its `///` block when it has one. */
		static int32 PipelineDeclStart(const FDecl& Decl)
		{
			return Decl.Doc.Span.Length > 0 ? FMath::Min(Decl.Doc.Span.Offset, Decl.Span.Offset) : Decl.Span.Offset;
		}

		/** Just past the line a node ends on. */
		static int32 PipelineLineAfter(const FString& Text, const FLangSpan& Span)
		{
			return PipelineLineEndWithTerminator(Text, FMath::Max(Span.End() - 1, Span.Offset));
		}

		// -----------------------------------------------------------------------------------------
		// Printing pieces
		// -----------------------------------------------------------------------------------------

		static FLangPrintOptions PipelinePrintOptions(const FString& NewLine)
		{
			FLangPrintOptions Options;
			Options.NewLine = NewLine;
			Options.bPrintTrivia = false;
			return Options;
		}

		static FString PrintPipelineDecl(const FDecl& Decl, const FString& NewLine)
		{
			return PrintDreamShaderLangDecl(Decl, PipelinePrintOptions(NewLine));
		}

		/** The `///` lines a declaration prints with, each ending in NewLine; empty without a block. */
		static FString PrintPipelineDocText(FDecl& Decl, const FString& NewLine)
		{
			if (Decl.Doc.IsEmpty())
			{
				return FString();
			}
			const FString Full = PrintPipelineDecl(Decl, NewLine);
			FDocBlock Saved = MoveTemp(Decl.Doc);
			Decl.Doc = FDocBlock();
			const FString Bare = PrintPipelineDecl(Decl, NewLine);
			Decl.Doc = MoveTemp(Saved);
			return Full.EndsWith(Bare, ESearchCase::CaseSensitive) ? Full.LeftChop(Bare.Len()) : FString();
		}

		/** The declaration as it prints without its `///` block. */
		static FString PrintPipelineDeclWithoutDoc(FDecl& Decl, const FString& NewLine)
		{
			FDocBlock Saved = MoveTemp(Decl.Doc);
			Decl.Doc = FDocBlock();
			const FString Bare = PrintPipelineDecl(Decl, NewLine);
			Decl.Doc = MoveTemp(Saved);
			return Bare;
		}

		/** One pass statement as the printer writes it, `;` included. */
		static FString PrintPipelineStatement(const FPassStmt& Statement)
		{
			switch (Statement.StmtKind)
			{
			case EPassStmtKind::Read:
			case EPassStmtKind::Write:
			{
				FString Text = Statement.StmtKind == EPassStmtKind::Read ? FString(TEXT("read ")) : FString(TEXT("write "));
				if (!Statement.Name.IsEmpty())
				{
					Text += Statement.Name + TEXT(" = ");
				}
				Text += Statement.Buffer;
				if (Statement.bPrevious)
				{
					Text += TEXT(".Previous");
				}
				return Text + TEXT(";");
			}
			case EPassStmtKind::Param:
				return FString::Printf(TEXT("param %s = %s;"), *Statement.Name, *PrintPipelineExpr(Statement.Value));
			case EPassStmtKind::Hlsl:
				// Lines of its own; a rewrite splices the text between the braces (RewritePipelinePass).
				return FString::Printf(TEXT("hlsl {%s}"), *Statement.RawBody);
			case EPassStmtKind::Setting:
			default:
				return FString::Printf(TEXT("%s = %s;"), *Statement.Name, *PrintPipelineExpr(Statement.Value));
			}
		}

		/**
		 * Lines with Indent in front of each, each ending in NewLine. An empty line stays and takes no indent: one in an
		 * `hlsl` block is the author's, and the block's text is written as it was.
		 */
		static FString IndentPipelineLines(const FString& Lines, const FString& Indent, const FString& NewLine)
		{
			TArray<FString> Parts;
			Lines.ParseIntoArray(Parts, *NewLine, /* bCullEmpty */ false);
			// The text's last terminator ends its last line; it does not open one more.
			if (Parts.Num() > 0 && Parts.Last().IsEmpty())
			{
				Parts.Pop();
			}
			FString Result;
			for (const FString& Part : Parts)
			{
				Result += (Part.IsEmpty() ? FString() : Indent + Part) + NewLine;
			}
			return Result;
		}

		// -----------------------------------------------------------------------------------------
		// Edits
		// -----------------------------------------------------------------------------------------

		struct FPipelineEdits
		{
			FPipelineEdits(const FLangSourceText& InOriginal, const FString& InNewLine)
				: Original(InOriginal)
				, NewLine(InNewLine)
			{
			}

			const FLangSourceText& Original;
			FString NewLine;
			/** Zero-length insertions, kept apart so that they sort in front of a change that starts where they are. */
			TArray<FLangSourceEdit> Insertions;
			TArray<FLangSourceEdit> Changes;

			const FString& GetText() const
			{
				return Original.GetText();
			}

			void Insert(const int32 Offset, const FString& NewText)
			{
				if (NewText.IsEmpty())
				{
					return;
				}
				FLangSourceEdit& Edit = Insertions.AddDefaulted_GetRef();
				Edit.Span = Original.MakeSpan(Offset, 0);
				Edit.NewText = NewText;
			}

			void Replace(const int32 Offset, const int32 Length, const FString& NewText)
			{
				FLangSourceEdit& Edit = Changes.AddDefaulted_GetRef();
				Edit.Span = Original.MakeSpan(Offset, Length);
				Edit.NewText = NewText;
			}

			/** Removes [Start, End): whole lines when nothing else is on them, else exactly that text. */
			void Delete(const int32 Start, const int32 End)
			{
				const FString& Text = GetText();
				if (IsPipelineAloneOnItsLines(Text, Start, End))
				{
					const int32 LineStart = PipelineLineStart(Text, Start);
					const int32 LineEnd = PipelineLineEndWithTerminator(Text, FMath::Max(End - 1, Start));
					Replace(LineStart, LineEnd - LineStart, FString());
					return;
				}
				Replace(Start, End - Start, FString());
			}

			void DeleteDecl(const FDecl& Decl)
			{
				if (Decl.Span.Length > 0)
				{
					Delete(PipelineDeclStart(Decl), Decl.Span.End());
				}
			}

			/** Lines (each ending in NewLine) after the line Decl ends on. */
			void InsertAfterDecl(const FDecl& Decl, const FString& Lines)
			{
				const FString& Text = GetText();
				const int32 At = PipelineLineAfter(Text, Decl.Span);
				const bool bNeedsBreak = At == Text.Len() && Text.Len() > 0 && Text[Text.Len() - 1] != TEXT('\n') && Text[Text.Len() - 1] != TEXT('\r');
				Insert(At, bNeedsBreak ? NewLine + Lines : Lines);
			}

			/** Lines (each ending in NewLine) in front of the line Decl, or its `///` block, starts on. */
			void InsertBeforeDecl(const FDecl& Decl, const FString& Lines)
			{
				Insert(PipelineLineStart(GetText(), PipelineDeclStart(Decl)), Lines);
			}

			TArray<FLangSourceEdit> TakeAll()
			{
				TArray<FLangSourceEdit> All = MoveTemp(Insertions);
				All.Append(MoveTemp(Changes));
				Insertions.Reset();
				Changes.Reset();
				return All;
			}
		};

		/** Sorts, checks for overlap (DSH9108) and applies. OutText stays Original's text when this fails. */
		static bool ApplyPipelineEdits(const FLangSourceText& Original, TArray<FLangSourceEdit>& Edits, FString& OutText, FLangDiagnosticSink& Diagnostics)
		{
			// Stable: an insertion stays in front of a change that starts where it is (FPipelineEdits::TakeAll lists them first).
			Edits.StableSort([](const FLangSourceEdit& A, const FLangSourceEdit& B) { return A.Span.Offset < B.Span.Offset; });

			const FString& Text = Original.GetText();
			for (int32 Index = 0; Index < Edits.Num(); ++Index)
			{
				const bool bOutside = Edits[Index].Span.Offset < 0 || Edits[Index].Span.Length < 0 || Edits[Index].Span.End() > Text.Len();
				const bool bOverlaps = Index > 0 && Edits[Index].Span.Offset < Edits[Index - 1].Span.End();
				if (bOutside || bOverlaps)
				{
					Diagnostics.Error(
						TEXT("DSH9108"),
						Original.GetPath(),
						Edits[Index].Span,
						FText::Format(
							LOCTEXT("PipelineOverlappingEdits", "Expected every change to this pipeline to touch its own stretch of text, found an edit at line {0} that overlaps another or runs past the end; the file was left unchanged."),
							FText::AsNumber(Edits[Index].Span.Line)));
					Edits.Reset();
					OutText = Text;
					return false;
				}
			}

			FString Result;
			Result.Reserve(Text.Len() + 256);
			int32 Cursor = 0;
			for (const FLangSourceEdit& Edit : Edits)
			{
				Result += Text.Mid(Cursor, Edit.Span.Offset - Cursor);
				Result += Edit.NewText;
				Cursor = Edit.Span.End();
			}
			Result += Text.Mid(Cursor);
			OutText = MoveTemp(Result);
			return true;
		}

		/** Replaces Decl's `///` block with DocText (`///` lines, each ending in NewLine; empty removes it), or puts one in front. */
		static void SplicePipelineDoc(FPipelineEdits& Edits, const FDecl& Decl, const FString& DocText)
		{
			const FString& Text = Edits.GetText();
			const FString Indent = PipelineLineIndent(Text, Decl.Span.Offset);
			const FString Lines = IndentPipelineLines(DocText, Indent, Edits.NewLine);

			if (Decl.Doc.Span.Length > 0)
			{
				const bool bOwnLine = IsPipelineBlankBefore(Text, Decl.Doc.Span.Offset);
				const int32 Start = bOwnLine ? PipelineLineStart(Text, Decl.Doc.Span.Offset) : Decl.Doc.Span.Offset;
				const int32 End = PipelineLineEndWithTerminator(Text, Decl.Doc.Span.End() - 1);
				Edits.Replace(Start, End - Start, bOwnLine ? Lines : Lines.TrimStart());
				return;
			}
			if (Lines.IsEmpty())
			{
				return;
			}
			if (IsPipelineBlankBefore(Text, Decl.Span.Offset))
			{
				Edits.Insert(PipelineLineStart(Text, Decl.Span.Offset), Lines);
			}
			else
			{
				// The declaration shares its line with something before it: the block goes on lines of its own in between.
				Edits.Insert(Decl.Span.Offset, Edits.NewLine + Lines + Indent);
			}
		}

		/** Whether a `uniform a, b;` statement holds Decl (DSH9107: one name cannot be spliced out of it). */
		static bool PipelineDeclSharesStatement(const FModule& Parsed, const FVariableDecl& Decl)
		{
			if (Decl.bSharesDeclarationWithPrevious)
			{
				return true;
			}
			for (int32 Index = 0; Index + 1 < Parsed.Declarations.Num(); ++Index)
			{
				if (Parsed.Declarations[Index].Get() == &Decl)
				{
					const FVariableDecl* Next = Parsed.Declarations[Index + 1].IsValid() ? Parsed.Declarations[Index + 1]->As<FVariableDecl>() : nullptr;
					return Next && Next->bSharesDeclarationWithPrevious;
				}
			}
			return false;
		}

		static void ReportPipelineSharedStatement(const FLangSourceText& Original, const FVariableDecl& Decl, FLangDiagnosticSink& Diagnostics)
		{
			Diagnostics.Error(
				TEXT("DSH9107"),
				Original.GetPath(),
				Decl.Declarator.NameSpan,
				FText::Format(
					LOCTEXT("PipelineSharedStatement", "Expected '{0}' to be declared alone to rewrite or remove it, found it in a declaration shared with other names; split the declaration first."),
					FText::FromString(Decl.Declarator.Name)));
		}

		/** The end of a `#pragma` line's own text: before a trailing `//` comment, which stays, and its blanks. */
		static int32 PipelinePragmaReplaceEnd(const FString& Text, const FPragmaDecl& Pragma)
		{
			const int32 SpanEnd = FMath::Min(Pragma.Span.End(), Text.Len());
			int32 ReplaceEnd = SpanEnd;
			bool bInString = false;
			for (int32 Index = Pragma.Span.Offset; Index + 1 < SpanEnd; ++Index)
			{
				if (Text[Index] == TEXT('"'))
				{
					bInString = !bInString;
				}
				else if (!bInString && Text[Index] == TEXT('/') && Text[Index + 1] == TEXT('/'))
				{
					ReplaceEnd = Index;
					break;
				}
			}
			while (ReplaceEnd > Pragma.Span.Offset && FChar::IsWhitespace(Text[ReplaceEnd - 1]))
			{
				--ReplaceEnd;
			}
			return ReplaceEnd;
		}

		// -----------------------------------------------------------------------------------------
		// Adopt, one declaration at a time
		// -----------------------------------------------------------------------------------------

		/**
		 * `#pragma pipeline`: a changed value is spliced over its `Key = Value`; when keys come or go the argument list is
		 * written anew with the untouched arguments as they were, and a pragma left with none goes.
		 */
		static void RewritePipelinePragma(FPipelineEdits& Edits, const FPragmaDecl* Pragma, const IR::FIRPassPipeline& Current, const IR::FIRPassPipeline& Desired, const FDecl* FirstDecl)
		{
			const FString& Text = Edits.GetText();
			if (!Pragma || Pragma->Span.Length <= 0)
			{
				if (const TUniquePtr<FPragmaDecl> Wanted = BuildPipelinePragma(Desired))
				{
					const FString Line = PrintPipelineDecl(*Wanted, Edits.NewLine) + Edits.NewLine;
					if (FirstDecl)
					{
						Edits.InsertBeforeDecl(*FirstDecl, Line);
					}
					else
					{
						Edits.Insert(0, Line);
					}
				}
				return;
			}

			TArray<FString> Arguments;
			TArray<FString> Seen;
			TArray<TPair<const FPragmaArgument*, FString>> ValueSplices;
			bool bStructural = false;
			for (const FPragmaArgument& Argument : Pragma->Arguments)
			{
				Seen.Add(Argument.Key);
				const FString Have = PragmaKeyValue(Current, Argument.Key);
				const FString Want = PragmaKeyValue(Desired, Argument.Key);
				const FString Written = Argument.Span.Length > 0 ? Text.Mid(Argument.Span.Offset, Argument.Span.Length) : Argument.Key + TEXT(" = ") + Argument.Value;
				if (!HasName(PragmaKeys, Argument.Key) || Have.Equals(Want, ESearchCase::CaseSensitive))
				{
					Arguments.Add(Written);
				}
				else if (Want.IsEmpty())
				{
					bStructural = true;
				}
				else
				{
					Arguments.Add(Argument.Key + TEXT(" = ") + Want);
					ValueSplices.Emplace(&Argument, Want);
				}
			}
			for (const TCHAR* Key : PragmaKeys)
			{
				if (Seen.ContainsByPredicate([Key](const FString& Written) { return Written.Equals(Key, ESearchCase::CaseSensitive); }))
				{
					continue;
				}
				const FString Want = PragmaKeyValue(Desired, Key);
				if (!Want.IsEmpty() && !Want.Equals(PragmaKeyValue(Current, Key), ESearchCase::CaseSensitive))
				{
					Arguments.Add(FString(Key) + TEXT(" = ") + Want);
					bStructural = true;
				}
			}

			if (!bStructural)
			{
				for (const TPair<const FPragmaArgument*, FString>& Splice : ValueSplices)
				{
					if (Splice.Key->Span.Length > 0)
					{
						Edits.Replace(Splice.Key->Span.Offset, Splice.Key->Span.Length, Splice.Key->Key + TEXT(" = ") + Splice.Value);
					}
				}
				return;
			}
			if (Arguments.Num() == 0)
			{
				Edits.DeleteDecl(*Pragma);
				return;
			}
			const int32 End = PipelinePragmaReplaceEnd(Text, *Pragma);
			Edits.Replace(Pragma->Span.Offset, End - Pragma->Span.Offset, FString::Printf(TEXT("#pragma %s(%s)"), *Pragma->Name, *FString::Join(Arguments, TEXT(", "))));
		}

		/** A uniform whose name stays: its `///` block, its type and its value, each replaced on its own when it changed. */
		static bool RewritePipelineParameter(
			FPipelineEdits& Edits,
			const FModule& Parsed,
			const FVariableDecl& Decl,
			const IR::FIRPassParameter& Have,
			const IR::FIRPassParameter& Want,
			FLangDiagnosticSink& Diagnostics)
		{
			const FString& NewLine = Edits.NewLine;
			const TUniquePtr<FVariableDecl> HaveDecl = BuildPipelineParameterDecl(Have);
			const TUniquePtr<FVariableDecl> WantDecl = BuildPipelineParameterDecl(Want);
			if (PrintPipelineDecl(*HaveDecl, NewLine).Equals(PrintPipelineDecl(*WantDecl, NewLine), ESearchCase::CaseSensitive))
			{
				return true;
			}
			if (PipelineDeclSharesStatement(Parsed, Decl))
			{
				ReportPipelineSharedStatement(Edits.Original, Decl, Diagnostics);
				return false;
			}

			const FString HaveDoc = PrintPipelineDocText(*HaveDecl, NewLine);
			const FString WantDoc = PrintPipelineDocText(*WantDecl, NewLine);
			if (!HaveDoc.Equals(WantDoc, ESearchCase::CaseSensitive))
			{
				SplicePipelineDoc(Edits, Decl, WantDoc);
			}

			if (!Have.Type.Equals(Want.Type, ESearchCase::CaseSensitive))
			{
				// Another type: the declaration itself is printed anew, under the same name.
				Edits.Replace(Decl.Span.Offset, Decl.Span.Length, PrintPipelineDeclWithoutDoc(*WantDecl, NewLine));
				return true;
			}

			const FString HaveValue = PrintPipelineExpr(MakePipelineParameterInitializer(Have, /* bEvenAtZero */ true));
			const FString WantValue = PrintPipelineExpr(MakePipelineParameterInitializer(Want, /* bEvenAtZero */ true));
			if (!HaveValue.Equals(WantValue, ESearchCase::CaseSensitive) && !WantValue.IsEmpty())
			{
				if (Decl.Declarator.Initializer && Decl.Declarator.Initializer->Span.Length > 0)
				{
					Edits.Replace(Decl.Declarator.Initializer->Span.Offset, Decl.Declarator.Initializer->Span.Length, WantValue);
				}
				else
				{
					Edits.Insert(Decl.Declarator.NameSpan.End(), TEXT(" = ") + WantValue);
				}
			}
			return true;
		}

		/**
		 * A buffer whose name stays: its `///` block and its format each replaced on their own; a changed argument value
		 * spliced in place, and the argument list written anew -- untouched arguments as they were -- when keys come or go.
		 */
		static void RewritePipelineBuffer(
			FPipelineEdits& Edits,
			const FBufferDecl& Decl,
			const IR::FIRPassPipeline& Current,
			const IR::FIRPassBuffer& Have,
			const IR::FIRPassPipeline& Desired,
			const IR::FIRPassBuffer& Want)
		{
			const FString& Text = Edits.GetText();
			const FString& NewLine = Edits.NewLine;

			const TUniquePtr<FBufferDecl> HaveDecl = BuildPipelineBufferDecl(Current, Have, /* bKeepWrittenDefaults */ true);
			const TUniquePtr<FBufferDecl> WantDecl = BuildPipelineBufferDecl(Desired, Want, /* bKeepWrittenDefaults */ true);
			if (PrintPipelineDecl(*HaveDecl, NewLine).Equals(PrintPipelineDecl(*WantDecl, NewLine), ESearchCase::CaseSensitive))
			{
				return;
			}

			const FString HaveDoc = PrintPipelineDocText(*HaveDecl, NewLine);
			const FString WantDoc = PrintPipelineDocText(*WantDecl, NewLine);
			if (!HaveDoc.Equals(WantDoc, ESearchCase::CaseSensitive))
			{
				SplicePipelineDoc(Edits, Decl, WantDoc);
			}
			if (!Have.Format.Equals(Want.Format, ESearchCase::CaseSensitive) && Decl.FormatSpan.Length > 0)
			{
				Edits.Replace(Decl.FormatSpan.Offset, Decl.FormatSpan.Length, Want.Format);
			}

			TArray<FString> Arguments;
			TArray<FString> Seen;
			TArray<TPair<const FPipelineKeyValue*, FString>> ValueSplices;
			bool bStructural = false;
			for (const FPipelineKeyValue& Argument : Decl.Arguments)
			{
				Seen.Add(Argument.Key);
				const FString HaveText = PrintPipelineExpr(MakeBufferKeyValue(Current, Have, Argument.Key, /* bKeepWrittenDefaults */ true));
				const FString WantText = PrintPipelineExpr(MakeBufferKeyValue(Desired, Want, Argument.Key, /* bKeepWrittenDefaults */ true));
				const FString Written = Argument.Span.Length > 0 ? Text.Mid(Argument.Span.Offset, Argument.Span.Length) : Argument.Key;
				if (!HasName(BufferKeys, Argument.Key) || HaveText.Equals(WantText, ESearchCase::CaseSensitive))
				{
					Arguments.Add(Written);
				}
				else if (WantText.IsEmpty())
				{
					bStructural = true;
				}
				else
				{
					Arguments.Add(Argument.Key + TEXT(" = ") + WantText);
					ValueSplices.Emplace(&Argument, WantText);
				}
			}
			for (const TCHAR* Key : BufferKeys)
			{
				if (Seen.ContainsByPredicate([Key](const FString& Written) { return Written.Equals(Key, ESearchCase::CaseSensitive); }))
				{
					continue;
				}
				const FString WantText = PrintPipelineExpr(MakeBufferKeyValue(Desired, Want, Key, /* bKeepWrittenDefaults */ true));
				const FString HaveText = PrintPipelineExpr(MakeBufferKeyValue(Current, Have, Key, /* bKeepWrittenDefaults */ true));
				if (!WantText.IsEmpty() && !WantText.Equals(HaveText, ESearchCase::CaseSensitive))
				{
					Arguments.Add(FString(Key) + TEXT(" = ") + WantText);
					bStructural = true;
				}
			}

			if (!bStructural)
			{
				for (const TPair<const FPipelineKeyValue*, FString>& Splice : ValueSplices)
				{
					const FExpr* Value = Splice.Key->Value.Get();
					if (Value && Value->Span.Length > 0)
					{
						Edits.Replace(Value->Span.Offset, Value->Span.Length, Splice.Value);
					}
				}
				return;
			}

			const FString List = Arguments.Num() > 0 ? FString::Printf(TEXT("(%s)"), *FString::Join(Arguments, TEXT(", "))) : FString();
			if (Decl.bHasArgumentList && Decl.ArgumentListSpan.Length > 0)
			{
				Edits.Replace(Decl.ArgumentListSpan.Offset, Decl.ArgumentListSpan.Length, List);
			}
			else if (Decl.FormatSpan.Length > 0)
			{
				Edits.Insert(Decl.FormatSpan.End(), List);
			}
		}

		/**
		 * Whether a pass block can take edits line by line: its `{` ends a line, its `}` starts one, and every statement
		 * has its lines to itself. A block written on one line is printed anew instead.
		 */
		static bool IsPipelinePassSpliceable(const FString& Text, const FPassDecl& Decl)
		{
			const int32 Open = Decl.BodySpan.Offset;
			const int32 Close = Decl.Span.End() - 1;
			if (Open < 0 || Close <= Open || Close >= Text.Len() || Text[Open] != TEXT('{') || Text[Close] != TEXT('}'))
			{
				return false;
			}
			if (!IsPipelineBlankAfter(Text, Open + 1) || !IsPipelineBlankBefore(Text, Close))
			{
				return false;
			}
			for (const TUniquePtr<FPassStmt>& Statement : Decl.Statements)
			{
				if (Statement && Statement->Span.Length > 0 && !IsPipelineAloneOnItsLines(Text, Statement->Span.Offset, Statement->Span.End()))
				{
					return false;
				}
			}
			return true;
		}

		/**
		 * A pass whose name stays. Its `///` block is replaced on its own when the description changed. Another kind, or a
		 * block that is not one statement per line, is printed anew; otherwise each key whose canonical value differs is
		 * spliced, inserted or removed, and each group of bindings (reads, writes, params) that differs is replaced as a group.
		 */
		static void RewritePipelinePass(
			FPipelineEdits& Edits,
			const FPassDecl& Decl,
			const IR::FIRPassPipeline& Current,
			const IR::FIRPass& Have,
			const IR::FIRPassPipeline& Desired,
			const IR::FIRPass& Want)
		{
			const FString& Text = Edits.GetText();
			const FString& NewLine = Edits.NewLine;

			const TUniquePtr<FPassDecl> HaveDecl = BuildPipelinePassDecl(Current, Have, /* bKeepWrittenDefaults */ true);
			const TUniquePtr<FPassDecl> WantDecl = BuildPipelinePassDecl(Desired, Want, /* bKeepWrittenDefaults */ true);
			if (PrintPipelineDecl(*HaveDecl, NewLine).Equals(PrintPipelineDecl(*WantDecl, NewLine), ESearchCase::CaseSensitive))
			{
				return;
			}

			const FString HaveDoc = PrintPipelineDocText(*HaveDecl, NewLine);
			const FString WantDoc = PrintPipelineDocText(*WantDecl, NewLine);
			if (!HaveDoc.Equals(WantDoc, ESearchCase::CaseSensitive))
			{
				SplicePipelineDoc(Edits, Decl, WantDoc);
			}

			if (!Have.Kind.Equals(Want.Kind, ESearchCase::CaseSensitive) || !IsPipelinePassSpliceable(Text, Decl))
			{
				const FString Printed = PrintPipelineDeclWithoutDoc(*WantDecl, NewLine);
				if (!Printed.Equals(PrintPipelineDeclWithoutDoc(*HaveDecl, NewLine), ESearchCase::CaseSensitive))
				{
					// At the declaration's own indentation; the first line has it from the text in front of the declaration.
					const FString Indent = PipelineLineIndent(Text, Decl.Span.Offset);
					FString Indented = IndentPipelineLines(Printed, Indent, NewLine);
					Indented.TrimStartInline();
					Indented.LeftChopInline(NewLine.Len());
					Edits.Replace(Decl.Span.Offset, Decl.Span.Length, Indented);
				}
				return;
			}

			// The block's indentation: its first statement's, else the declaration's and one level more.
			const FPassStmt* FirstStatement = nullptr;
			for (const TUniquePtr<FPassStmt>& Statement : Decl.Statements)
			{
				if (Statement && Statement->Span.Length > 0)
				{
					FirstStatement = Statement.Get();
					break;
				}
			}
			const FString Indent = FirstStatement
				? PipelineLineIndent(Text, FirstStatement->Span.Offset)
				: PipelineLineIndent(Text, Decl.Span.Offset) + PipelinePrintOptions(NewLine).Indent;
			const int32 AfterOpenBrace = PipelineLineEndWithTerminator(Text, Decl.BodySpan.Offset);

			// -- settings, key by key
			const FPassStmt* LastSetting = nullptr;
			for (const TUniquePtr<FPassStmt>& Statement : Decl.Statements)
			{
				if (Statement && Statement->StmtKind == EPassStmtKind::Setting && Statement->Span.Length > 0)
				{
					LastSetting = Statement.Get();
				}
			}

			FString InsertedSettings;
			for (const FString& Key : PipelinePassKeys(Want))
			{
				const FString HaveText = PrintPipelineExpr(MakePassKeyValue(Current, Have, Key, /* bKeepWrittenDefaults */ true));
				const FString WantText = PrintPipelineExpr(MakePassKeyValue(Desired, Want, Key, /* bKeepWrittenDefaults */ true));
				if (HaveText.Equals(WantText, ESearchCase::CaseSensitive))
				{
					continue;
				}

				const FPassStmt* Existing = nullptr;
				for (const TUniquePtr<FPassStmt>& Statement : Decl.Statements)
				{
					if (Statement && Statement->StmtKind == EPassStmtKind::Setting && Statement->Name.Equals(Key, ESearchCase::CaseSensitive))
					{
						Existing = Statement.Get();
						break;
					}
				}

				if (WantText.IsEmpty())
				{
					// Back where the binder puts it by itself: the key goes.
					if (Existing && Existing->Span.Length > 0)
					{
						Edits.Delete(Existing->Span.Offset, Existing->Span.End());
					}
				}
				else if (Existing && Existing->Value && Existing->Value->Span.Length > 0)
				{
					Edits.Replace(Existing->Value->Span.Offset, Existing->Value->Span.Length, WantText);
				}
				else
				{
					InsertedSettings += Indent + Key + TEXT(" = ") + WantText + TEXT(";") + NewLine;
				}
			}
			if (!InsertedSettings.IsEmpty())
			{
				Edits.Insert(LastSetting ? PipelineLineAfter(Text, LastSetting->Span) : AfterOpenBrace, InsertedSettings);
			}

			// -- bindings, group by group
			const auto GroupText = [&Indent, &NewLine](const FPassDecl& Canonical, const EPassStmtKind StatementKind)
			{
				FString Result;
				for (const TUniquePtr<FPassStmt>& Statement : Canonical.Statements)
				{
					if (Statement && Statement->StmtKind == StatementKind)
					{
						Result += Indent + PrintPipelineStatement(*Statement) + NewLine;
					}
				}
				return Result;
			};

			// A group with no statement yet goes after the last statement before it, or after the `{`.
			const FPassStmt* LastBefore = LastSetting;
			const EPassStmtKind Groups[] = { EPassStmtKind::Read, EPassStmtKind::Write, EPassStmtKind::Param };
			for (const EPassStmtKind Group : Groups)
			{
				TArray<const FPassStmt*> Existing;
				for (const TUniquePtr<FPassStmt>& Statement : Decl.Statements)
				{
					if (Statement && Statement->StmtKind == Group && Statement->Span.Length > 0)
					{
						Existing.Add(Statement.Get());
					}
				}

				const FString HaveGroup = GroupText(*HaveDecl, Group);
				const FString WantGroup = GroupText(*WantDecl, Group);
				if (!HaveGroup.Equals(WantGroup, ESearchCase::CaseSensitive))
				{
					if (Existing.Num() > 0)
					{
						// The first statement's lines take the whole group; the others go.
						const FPassStmt& First = *Existing[0];
						const int32 LineStart = PipelineLineStart(Text, First.Span.Offset);
						Edits.Replace(LineStart, PipelineLineAfter(Text, First.Span) - LineStart, WantGroup);
						for (int32 Index = 1; Index < Existing.Num(); ++Index)
						{
							Edits.Delete(Existing[Index]->Span.Offset, Existing[Index]->Span.End());
						}
					}
					else
					{
						Edits.Insert(LastBefore ? PipelineLineAfter(Text, LastBefore->Span) : AfterOpenBrace, WantGroup);
					}
				}
				if (Existing.Num() > 0)
				{
					LastBefore = Existing.Last();
				}
			}

			// -- the pass's own `hlsl` block: the text between its braces, the block itself when it comes or goes
			const FPassStmt* ExistingBlock = nullptr;
			const FPassStmt* LastStatement = nullptr;
			for (const TUniquePtr<FPassStmt>& Statement : Decl.Statements)
			{
				if (Statement && Statement->Span.Length > 0)
				{
					LastStatement = Statement.Get();
					if (Statement->StmtKind == EPassStmtKind::Hlsl && !ExistingBlock)
					{
						ExistingBlock = Statement.Get();
					}
				}
			}
			const bool bHaveBlock = PassHasOwnHlslBlock(Have);
			const bool bWantBlock = PassHasOwnHlslBlock(Want);
			if (bWantBlock && ExistingBlock && ExistingBlock->BodySpan.Length >= 2)
			{
				if (!bHaveBlock || !NormalizePipelineHlsl(Have.InlineHlsl).Equals(NormalizePipelineHlsl(Want.InlineHlsl), ESearchCase::CaseSensitive))
				{
					Edits.Replace(ExistingBlock->BodySpan.Offset + 1, ExistingBlock->BodySpan.Length - 2, PipelineHlslWithNewLine(Want.InlineHlsl, NewLine));
				}
			}
			else if (bWantBlock)
			{
				const FString Block = Indent + TEXT("hlsl") + NewLine + Indent + TEXT("{") + PipelineHlslWithNewLine(Want.InlineHlsl, NewLine) + TEXT("}") + NewLine;
				Edits.Insert(LastStatement ? PipelineLineAfter(Text, LastStatement->Span) : AfterOpenBrace, LastStatement ? NewLine + Block : Block);
			}
			else if (ExistingBlock)
			{
				Edits.Delete(ExistingBlock->Span.Offset, ExistingBlock->Span.End());
			}
		}

		// -----------------------------------------------------------------------------------------
		// Adopt, one group of declarations at a time
		// -----------------------------------------------------------------------------------------

		/** Desired item -> the current item of the same name (case-sensitive), INDEX_NONE for a new one; each matched once. */
		static TArray<int32> MatchPipelineItems(const TArray<FString>& CurrentNames, const TArray<FString>& DesiredNames, TArray<bool>& OutMatched)
		{
			TArray<int32> Result;
			OutMatched.Init(false, CurrentNames.Num());
			for (const FString& Name : DesiredNames)
			{
				int32 Found = INDEX_NONE;
				for (int32 Index = 0; Index < CurrentNames.Num() && Found == INDEX_NONE; ++Index)
				{
					if (!OutMatched[Index] && CurrentNames[Index].Equals(Name, ESearchCase::CaseSensitive))
					{
						Found = Index;
					}
				}
				if (Found != INDEX_NONE)
				{
					OutMatched[Found] = true;
				}
				Result.Add(Found);
			}
			return Result;
		}

		/**
		 * One group -- the uniforms, the buffers or the passes, whose order is the payload's. Matched in order: each matched
		 * item is rewritten in place, a removed one deleted, a new one inserted after the item before it. Matched out of
		 * order: the group's declarations go, and the whole group is printed after where the last of them was.
		 * InOutAnchor is where a group with nothing in the file goes, and becomes this group's last declaration.
		 */
		template <typename TPrintDesired, typename TRewriteItem, typename TCanRemove>
		static bool RewritePipelineGroup(
			FPipelineEdits& Edits,
			const TArray<FString>& CurrentNames,
			const TArray<FString>& DesiredNames,
			const TArray<const FDecl*>& CurrentDecls,
			const FDecl* FirstDeclOfFile,
			const FDecl*& InOutAnchor,
			const bool bBlankLineBeforeNew,
			TPrintDesired&& PrintDesired,
			TRewriteItem&& RewriteItem,
			TCanRemove&& CanRemove)
		{
			const FString& NewLine = Edits.NewLine;
			TArray<bool> Matched;
			const TArray<int32> CurrentOfDesired = MatchPipelineItems(CurrentNames, DesiredNames, Matched);

			const FDecl* LastDecl = nullptr;
			for (const FDecl* Decl : CurrentDecls)
			{
				if (Decl && Decl->Span.Length > 0 && (!LastDecl || Decl->Span.End() > LastDecl->Span.End()))
				{
					LastDecl = Decl;
				}
			}

			const auto NewItemText = [&](const int32 DesiredIndex)
			{
				return (bBlankLineBeforeNew ? NewLine : FString()) + PrintDesired(DesiredIndex) + NewLine;
			};
			const auto InsertAfterAnchor = [&](const FDecl* After, const FString& Lines)
			{
				if (After)
				{
					Edits.InsertAfterDecl(*After, Lines);
				}
				else if (FirstDeclOfFile)
				{
					Edits.InsertBeforeDecl(*FirstDeclOfFile, Lines);
				}
				else
				{
					const FString& Text = Edits.GetText();
					const bool bNeedsBreak = Text.Len() > 0 && Text[Text.Len() - 1] != TEXT('\n') && Text[Text.Len() - 1] != TEXT('\r');
					Edits.Insert(Text.Len(), bNeedsBreak ? NewLine + Lines : Lines);
				}
			};

			bool bInOrder = true;
			{
				int32 Previous = INDEX_NONE;
				for (const int32 Index : CurrentOfDesired)
				{
					if (Index == INDEX_NONE)
					{
						continue;
					}
					bInOrder &= Index > Previous;
					Previous = Index;
				}
			}

			if (!bInOrder)
			{
				for (int32 Index = 0; Index < CurrentDecls.Num(); ++Index)
				{
					if (CurrentDecls[Index] && !CanRemove(Index))
					{
						return false;
					}
				}
				FString Group;
				for (int32 DesiredIndex = 0; DesiredIndex < DesiredNames.Num(); ++DesiredIndex)
				{
					Group += NewItemText(DesiredIndex);
				}
				for (const FDecl* Decl : CurrentDecls)
				{
					if (Decl)
					{
						Edits.DeleteDecl(*Decl);
					}
				}
				InsertAfterAnchor(LastDecl ? LastDecl : InOutAnchor, Group);
				if (LastDecl)
				{
					InOutAnchor = LastDecl;
				}
				return true;
			}

			for (int32 Index = 0; Index < CurrentNames.Num(); ++Index)
			{
				const FDecl* Decl = CurrentDecls.IsValidIndex(Index) ? CurrentDecls[Index] : nullptr;
				if (Matched[Index] || !Decl)
				{
					continue;
				}
				if (!CanRemove(Index))
				{
					return false;
				}
				Edits.DeleteDecl(*Decl);
			}

			const FDecl* Previous = nullptr;
			FString Pending;
			for (int32 DesiredIndex = 0; DesiredIndex < DesiredNames.Num(); ++DesiredIndex)
			{
				const int32 CurrentIndex = CurrentOfDesired[DesiredIndex];
				if (CurrentIndex == INDEX_NONE)
				{
					Pending += NewItemText(DesiredIndex);
					continue;
				}
				const FDecl* Decl = CurrentDecls.IsValidIndex(CurrentIndex) ? CurrentDecls[CurrentIndex] : nullptr;
				if (!Pending.IsEmpty())
				{
					// New items in front of the first matched one go in front of it; later ones after the one before them.
					if (Previous)
					{
						Edits.InsertAfterDecl(*Previous, Pending);
					}
					else if (Decl)
					{
						Edits.InsertBeforeDecl(*Decl, Pending);
					}
					else
					{
						InsertAfterAnchor(InOutAnchor, Pending);
					}
					Pending.Reset();
				}
				if (!RewriteItem(CurrentIndex, DesiredIndex))
				{
					return false;
				}
				if (Decl)
				{
					Previous = Decl;
				}
			}
			if (!Pending.IsEmpty())
			{
				InsertAfterAnchor(Previous ? Previous : (LastDecl ? LastDecl : InOutAnchor), Pending);
			}

			if (LastDecl)
			{
				InOutAnchor = LastDecl;
			}
			return true;
		}

		// -----------------------------------------------------------------------------------------
		// Comparison
		// -----------------------------------------------------------------------------------------

		/** Differences, one line each: "<where><what> is 'a' on one side and 'b' on the other". */
		struct FPipelineDifferences
		{
			TArray<FString>* Out = nullptr;
			bool bEqual = true;

			void Add(const FString& Line)
			{
				bEqual = false;
				if (Out)
				{
					Out->Add(Line);
				}
			}

			void Text(const FString& Where, const TCHAR* What, const FString& A, const FString& B)
			{
				if (!A.Equals(B, ESearchCase::CaseSensitive))
				{
					Add(FString::Printf(TEXT("%s%s is '%s' on one side and '%s' on the other"), *Where, What, *A, *B));
				}
			}

			/** An asset or file path, which the engine resolves ignoring case. */
			void Path(const FString& Where, const TCHAR* What, const FString& A, const FString& B)
			{
				if (!A.Equals(B, ESearchCase::IgnoreCase))
				{
					Add(FString::Printf(TEXT("%s%s is '%s' on one side and '%s' on the other"), *Where, What, *A, *B));
				}
			}

			void Number(const FString& Where, const TCHAR* What, const double A, const double B)
			{
				if (!SameFloat(A, B))
				{
					Add(FString::Printf(TEXT("%s%s is %s on one side and %s on the other"), *Where, What, *FormatDreamShaderFloatLiteral(A), *FormatDreamShaderFloatLiteral(B)));
				}
			}

			void Int(const FString& Where, const TCHAR* What, const int64 A, const int64 B)
			{
				if (A != B)
				{
					Add(FString::Printf(TEXT("%s%s is %lld on one side and %lld on the other"), *Where, What, static_cast<long long>(A), static_cast<long long>(B)));
				}
			}

			void Bool(const FString& Where, const TCHAR* What, const bool A, const bool B)
			{
				if (A != B)
				{
					Add(FString::Printf(TEXT("%s%s is %s on one side and %s on the other"), *Where, What, A ? TEXT("true") : TEXT("false"), B ? TEXT("true") : TEXT("false")));
				}
			}

			void Numbers(const FString& Where, const TCHAR* What, const double* A, const double* B, const int32 Count)
			{
				for (int32 Index = 0; Index < Count; ++Index)
				{
					if (!SameFloat(A[Index], B[Index]))
					{
						Add(FString::Printf(TEXT("%s%s[%d] is %s on one side and %s on the other"), *Where, What, Index, *FormatDreamShaderFloatLiteral(A[Index]), *FormatDreamShaderFloatLiteral(B[Index])));
						return;
					}
				}
			}

			void List(const FString& Where, const TCHAR* What, const TArray<FString>& A, const TArray<FString>& B)
			{
				Text(Where, What, FString::Join(A, TEXT(" | ")), FString::Join(B, TEXT(" | ")));
			}
		};

		/** Display text as an asset built from text holds it: line breaks as `\n`, no blanks around. */
		static FString NormalizePipelineDescription(const FString& Description)
		{
			FString Result = Description.Replace(TEXT("\r\n"), TEXT("\n"));
			Result.TrimStartAndEndInline();
			return Result;
		}

		/**
		 * A number as the comparison reads it: the shortest text of its float32, with -0 and +0 one value -- `Gain * -1.0`
		 * folds to an offset of -0 and `0.0 - Gain` to +0, and the asset makes the same pass of either.
		 */
		static FString PipelineComparedNumberText(const double Value)
		{
			return FormatDreamShaderFloatLiteral(Value == 0.0 ? 0.0 : Value);
		}

		/** A parameter default or a `param` constant as the asset holds it: by type, float32 channels as wide as the type. */
		static FString PipelineValueText(const IR::FIRPropertyValue& Value, const FString& Type)
		{
			if (Type.Equals(TEXT("Texture2D"), ESearchCase::CaseSensitive) || Value.Kind == IR::EIRPropertyKind::Object)
			{
				FString Path = Value.S.TrimStartAndEnd();
				if (Path.Equals(TEXT("None"), ESearchCase::IgnoreCase))
				{
					return FString();
				}
				// `/Game/T/Noise` and `/Game/T/Noise.Noise` are one asset: a source writes the package path as often as not,
				// and the decompiler reads the object path back off the asset.
				int32 LastSlash = INDEX_NONE;
				if (Path.StartsWith(TEXT("/"), ESearchCase::CaseSensitive) && !Path.Contains(TEXT("."), ESearchCase::CaseSensitive)
					&& Path.FindLastChar(TEXT('/'), LastSlash) && LastSlash + 1 < Path.Len())
				{
					Path += TEXT(".") + Path.Mid(LastSlash + 1);
				}
				return Path.ToLower();
			}
			if (Type.Equals(TEXT("int"), ESearchCase::CaseSensitive))
			{
				return FString::Printf(TEXT("%lld"), static_cast<long long>(PipelineIntOf(Value)));
			}
			if (Type.Equals(TEXT("bool"), ESearchCase::CaseSensitive))
			{
				return PipelineBoolOf(Value) ? TEXT("true") : TEXT("false");
			}
			double Channels[4];
			PipelineChannels(Value, Channels);
			FString Result;
			const int32 Width = PipelineFloatWidth(Type);
			for (int32 Index = 0; Index < Width; ++Index)
			{
				Result += Index > 0 ? TEXT(", ") : TEXT("");
				Result += PipelineComparedNumberText(Channels[Index]);
			}
			return Result;
		}

		/** The name a binding has inside its pass: the buffer's own when the slot is empty (the runtime's spelling of it). */
		static FString PipelineSlotOf(const IR::FIRPassBinding& Binding)
		{
			return Binding.Slot.IsEmpty() ? Binding.Buffer : Binding.Slot;
		}

		static void ComparePipelineBindings(FPipelineDifferences& Diff, const FString& Where, const TCHAR* What, const TArray<IR::FIRPassBinding>& A, const TArray<IR::FIRPassBinding>& B)
		{
			const auto Render = [](const TArray<IR::FIRPassBinding>& Bindings)
			{
				TArray<FString> Lines;
				for (const IR::FIRPassBinding& Binding : Bindings)
				{
					Lines.Add(FString::Printf(TEXT("%s = %s%s"), *PipelineSlotOf(Binding), *Binding.Buffer, Binding.bPrevious ? TEXT(".Previous") : TEXT("")));
				}
				return FString::Join(Lines, TEXT("; "));
			};
			Diff.Text(Where, What, Render(A), Render(B));
		}

		static FString PipelineParamText(const IR::FIRPassParam& Param)
		{
			if (Param.SourceKind.Equals(TEXT("Constant"), ESearchCase::CaseSensitive))
			{
				const FString Type = Param.ConstantType.IsEmpty() ? FString(TEXT("float")) : Param.ConstantType;
				return FString::Printf(TEXT("%s = %s(%s)"), *Param.Target, *Type, *PipelineValueText(Param.Constant, Type));
			}
			const FString Source = Param.SourceKind.Equals(TEXT("Weight"), ESearchCase::CaseSensitive) ? FString(WeightParameterName) : Param.Parameter;
			return FString::Printf(TEXT("%s = %s * %s + %s"), *Param.Target, *Source, *PipelineComparedNumberText(Param.Multiplier), *PipelineComparedNumberText(Param.Offset));
		}

		static FString PipelineFilterText(const TArray<IR::FIRPassFilterClause>& Filter)
		{
			return PrintPipelineExpr(MakePipelineFilter(Filter));
		}

		/** A material: the object path when both sides have one, else the reference as written. */
		static void ComparePipelineMaterial(FPipelineDifferences& Diff, const FString& Where, const IR::FIRPass& A, const IR::FIRPass& B)
		{
			if (!A.MaterialObjectPath.IsEmpty() && !B.MaterialObjectPath.IsEmpty())
			{
				Diff.Path(Where, TEXT("the material"), A.MaterialObjectPath, B.MaterialObjectPath);
				return;
			}
			Diff.Text(
				Where,
				TEXT("the material"),
				A.MaterialReference.IsEmpty() ? A.MaterialObjectPath : A.MaterialReference,
				B.MaterialReference.IsEmpty() ? B.MaterialObjectPath : B.MaterialReference);
		}

		/** A shader file: the file when both sides resolved one (it may live anywhere), else the virtual path, else the reference. */
		static void ComparePipelineShader(FPipelineDifferences& Diff, const FString& Where, const IR::FIRPass& A, const IR::FIRPass& B)
		{
			if (!A.ShaderFilePath.IsEmpty() && !B.ShaderFilePath.IsEmpty())
			{
				// One file, however each side spelled it: the compiler's absolute path, or a `../../../` standard filename.
				const auto File = [](const FString& Path)
				{
					FString Result = FPaths::ConvertRelativePathToFull(Path);
					Result.ReplaceInline(TEXT("\\"), TEXT("/"));
					FPaths::CollapseRelativeDirectories(Result);
					FPaths::RemoveDuplicateSlashes(Result);
					return Result;
				};
				Diff.Path(Where, TEXT("the shader"), File(A.ShaderFilePath), File(B.ShaderFilePath));
				return;
			}
			if (!A.ShaderVirtualPath.IsEmpty() && !B.ShaderVirtualPath.IsEmpty())
			{
				Diff.Path(Where, TEXT("the shader"), A.ShaderVirtualPath, B.ShaderVirtualPath);
				return;
			}
			Diff.Text(
				Where,
				TEXT("the shader"),
				A.ShaderReference.IsEmpty() ? A.ShaderVirtualPath : A.ShaderReference,
				B.ShaderReference.IsEmpty() ? B.ShaderVirtualPath : B.ShaderReference);
		}

		/** Where an HLSL pass's code is, File when the payload does not say (a payload from before inline HLSL); empty for any other pass. */
		static FString EffectivePipelineHlslSource(const IR::FIRPass& Pass)
		{
			if (IR::PassHlslSource::IsInline(Pass.HlslSource))
			{
				return Pass.HlslSource;
			}
			return PassHasShader(Pass) ? FString(IR::PassHlslSource::File) : FString();
		}

		static void ComparePipelinePasses(FPipelineDifferences& Diff, const IR::FIRPass& A, const IR::FIRPass& B)
		{
			const FString Where = FString::Printf(TEXT("pass '%s': "), *A.Name);
			Diff.Text(Where, TEXT("the kind"), A.Kind, B.Kind);
			Diff.Text(Where, TEXT("the injection point"), A.Injection, B.Injection);
			Diff.Text(Where, TEXT("Enabled"), A.EnabledParameter, B.EnabledParameter);
			Diff.Text(Where, TEXT("the description"), NormalizePipelineDescription(A.Description), NormalizePipelineDescription(B.Description));
			ComparePipelineBindings(Diff, Where, TEXT("the reads"), A.Reads, B.Reads);
			ComparePipelineBindings(Diff, Where, TEXT("the writes"), A.Writes, B.Writes);

			TArray<FString> ParamsA;
			TArray<FString> ParamsB;
			for (const IR::FIRPassParam& Param : A.Params)
			{
				ParamsA.Add(PipelineParamText(Param));
			}
			for (const IR::FIRPassParam& Param : B.Params)
			{
				ParamsB.Add(PipelineParamText(Param));
			}
			Diff.Text(Where, TEXT("the params"), FString::Join(ParamsA, TEXT("; ")), FString::Join(ParamsB, TEXT("; ")));

			if (!A.Kind.Equals(B.Kind, ESearchCase::CaseSensitive))
			{
				return;
			}
			const int32 KindIndex = FindName(PassKinds, A.Kind);

			if (KindIndex == Kind::Fullscreen || KindIndex == Kind::Mesh)
			{
				ComparePipelineMaterial(Diff, Where, A, B);
			}
			if (KindIndex == Kind::Fullscreen || KindIndex == Kind::Compute)
			{
				ComparePipelineShader(Diff, Where, A, B);
				Diff.Text(Where, TEXT("the entry"), A.Entry, B.Entry);
				Diff.Text(Where, TEXT("where the HLSL is"), EffectivePipelineHlslSource(A), EffectivePipelineHlslSource(B));
				if (PassHasOwnHlslBlock(A) && PassHasOwnHlslBlock(B)
					&& !NormalizePipelineHlsl(A.InlineHlsl).Equals(NormalizePipelineHlsl(B.InlineHlsl), ESearchCase::CaseSensitive))
				{
					Diff.Add(FString::Printf(TEXT("%sthe text of its 'hlsl' block differs"), *Where));
				}
			}
			if (KindIndex == Kind::Compute)
			{
				Diff.Int(Where, TEXT("Threads.x"), A.ThreadsX, B.ThreadsX);
				Diff.Int(Where, TEXT("Threads.y"), A.ThreadsY, B.ThreadsY);
				Diff.Int(Where, TEXT("Threads.z"), A.ThreadsZ, B.ThreadsZ);
				Diff.Text(Where, TEXT("the dispatch mode"), A.DispatchMode, B.DispatchMode);
				if (A.DispatchMode.Equals(TEXT("Fixed"), ESearchCase::CaseSensitive))
				{
					Diff.Int(Where, TEXT("Dispatch.x"), A.DispatchX, B.DispatchX);
					Diff.Int(Where, TEXT("Dispatch.y"), A.DispatchY, B.DispatchY);
					Diff.Int(Where, TEXT("Dispatch.z"), A.DispatchZ, B.DispatchZ);
				}
				else
				{
					Diff.Text(Where, TEXT("the dispatch buffer"), A.DispatchBuffer, B.DispatchBuffer);
					Diff.Number(Where, TEXT("the dispatch scale"), A.DispatchScale, B.DispatchScale);
				}
			}
			if (KindIndex == Kind::Mesh)
			{
				const auto Or = [](const FString& Value, const TCHAR* Default) { return Value.IsEmpty() ? FString(Default) : Value; };
				Diff.Text(Where, TEXT("the filter"), PipelineFilterText(A.Filter), PipelineFilterText(B.Filter));
				Diff.Text(Where, TEXT("Mode"), EffectiveMeshMode(A), EffectiveMeshMode(B));
				Diff.Text(Where, TEXT("Depth"), Or(A.Depth, TEXT("TestScene")), Or(B.Depth, TEXT("TestScene")));
				if (A.Depth.Equals(TEXT("Own"), ESearchCase::CaseSensitive))
				{
					Diff.Text(Where, TEXT("the depth buffer"), A.DepthBuffer, B.DepthBuffer);
				}
				Diff.Text(Where, TEXT("Cull"), Or(A.Cull, TEXT("Auto")), Or(B.Cull, TEXT("Auto")));
				Diff.Text(Where, TEXT("Blend"), Or(A.Blend, TEXT("Replace")), Or(B.Blend, TEXT("Replace")));
				Diff.List(Where, TEXT("Usage"), EffectiveUsage(A), EffectiveUsage(B));
				const FString NaniteA = A.Nanite.IsEmpty() ? DefaultNanitePolicy(A) : A.Nanite;
				const FString NaniteB = B.Nanite.IsEmpty() ? DefaultNanitePolicy(B) : B.Nanite;
				Diff.Text(Where, TEXT("Nanite"), NaniteA, NaniteB);
				if (NaniteA.Equals(TEXT("AssignStencil"), ESearchCase::CaseSensitive))
				{
					Diff.Int(Where, TEXT("the assigned stencil value"), A.AssignedStencilValue, B.AssignedStencilValue);
				}
				if (!NaniteA.Equals(TEXT("Skip"), ESearchCase::CaseSensitive))
				{
					Diff.Numbers(Where, TEXT("NaniteValue"), A.NaniteValue, B.NaniteValue, 4);
				}
			}
			if (KindIndex == Kind::Clear)
			{
				Diff.Numbers(Where, TEXT("Value"), A.ClearValue, B.ClearValue, 4);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Public entry points
	// ---------------------------------------------------------------------------------------------

	void CollectDreamShaderPipelineReferences(const FModule& Module, TArray<FString>& OutMaterials, TArray<FString>& OutShaders)
	{
		const auto AddUnique = [](TArray<FString>& List, const FString& Value)
		{
			// Case-sensitive: an FString compares ignoring case, and two spellings are two references to resolve.
			if (!List.ContainsByPredicate([&Value](const FString& Existing) { return Existing.Equals(Value, ESearchCase::CaseSensitive); }))
			{
				List.Add(Value);
			}
		};

		Module.ForEachDecl(ENodeKind::PassDecl, [&](const FDecl& Decl)
		{
			for (const TUniquePtr<FPassStmt>& Statement : static_cast<const FPassDecl&>(Decl).Statements)
			{
				if (!Statement || Statement->StmtKind != EPassStmtKind::Setting)
				{
					continue;
				}
				const bool bMaterial = Statement->Name.Equals(TEXT("Material"), ESearchCase::CaseSensitive);
				const bool bShader = Statement->Name.Equals(TEXT("Shader"), ESearchCase::CaseSensitive);
				if (!bMaterial && !bShader)
				{
					continue;
				}
				// Read through parentheses and trimmed, as the binder reads it.
				const FExpr* Value = Statement->Value.Get();
				while (Value && Value->Is<FParenExpr>())
				{
					Value = static_cast<const FParenExpr*>(Value)->Inner.Get();
				}
				const FLiteralExpr* Literal = Value ? Value->As<FLiteralExpr>() : nullptr;
				if (!Literal || Literal->LiteralKind != ELiteralKind::String)
				{
					continue;
				}
				const FString Reference = Literal->Text.TrimStartAndEnd();
				if (!Reference.IsEmpty())
				{
					AddUnique(bMaterial ? OutMaterials : OutShaders, Reference);
				}
			}
		});
	}

	TUniquePtr<FModule> BuildDreamShaderPipelineModule(const IR::FIRPassPipeline& Pipeline, const FString& FilePath)
	{
		using namespace Private::PipelineSource;

		TUniquePtr<FModule> Module = MakeUnique<FModule>();
		Module->FilePath = FilePath;
		Module->FileKind = ELangFileKind::Dsp;

		// The layout is the trivia's: a blank line between the groups, before a declaration with a `///` block, and
		// between passes. Every declaration has an entry, so the printer always lays the file out by them.
		const auto AddDecl = [&Module](FDeclPtr Decl, const bool bBlankLineBefore)
		{
			const bool bFirst = Module->Declarations.Num() == 0;
			Module->Trivia.FindOrAdd(static_cast<const FNode*>(Decl.Get())).BlankLinesBefore = (!bFirst && bBlankLineBefore) ? 1 : 0;
			Module->Declarations.Add(MoveTemp(Decl));
		};

		if (TUniquePtr<FPragmaDecl> Pragma = BuildPipelinePragma(Pipeline))
		{
			AddDecl(MoveTemp(Pragma), false);
		}
		for (int32 Index = 0; Index < Pipeline.Parameters.Num(); ++Index)
		{
			TUniquePtr<FVariableDecl> Decl = BuildPipelineParameterDecl(Pipeline.Parameters[Index]);
			const bool bBlank = Index == 0 || !Decl->Doc.IsEmpty();
			AddDecl(MoveTemp(Decl), bBlank);
		}
		for (int32 Index = 0; Index < Pipeline.Buffers.Num(); ++Index)
		{
			// Canonical: a key the binder fills in by itself is left out, whatever the flags say.
			TUniquePtr<FBufferDecl> Decl = BuildPipelineBufferDecl(Pipeline, Pipeline.Buffers[Index], /* bKeepWrittenDefaults */ false);
			const bool bBlank = Index == 0 || !Decl->Doc.IsEmpty();
			AddDecl(MoveTemp(Decl), bBlank);
		}
		if (Pipeline.bHasSharedHlsl)
		{
			// The shared functions and entries every inline pass below compiles with, as they were written.
			TUniquePtr<FHlslBlockDecl> Block = MakeUnique<FHlslBlockDecl>();
			Block->RawBody = Pipeline.SharedHlsl;
			AddDecl(MoveTemp(Block), true);
		}
		for (const IR::FIRPass& Pass : Pipeline.Passes)
		{
			TUniquePtr<FPassDecl> Decl = BuildPipelinePassDecl(Pipeline, Pass, /* bKeepWrittenDefaults */ false);
			// A blank line between the pass's keys and lines and its `hlsl` block.
			for (const TUniquePtr<FPassStmt>& Statement : Decl->Statements)
			{
				if (Statement->StmtKind == EPassStmtKind::Hlsl && Decl->Statements.Num() > 1)
				{
					Module->Trivia.FindOrAdd(static_cast<const FNode*>(Statement.Get())).BlankLinesBefore = 1;
				}
			}
			AddDecl(MoveTemp(Decl), true);
		}
		return Module;
	}

	FString PrintDreamShaderPipeline(const IR::FIRPassPipeline& Pipeline, const FString& FilePath, const FLangPrintOptions& Options)
	{
		const TUniquePtr<FModule> Module = BuildDreamShaderPipelineModule(Pipeline, FilePath);
		return PrintDreamShaderLang(*Module, Options);
	}

	bool RewriteDreamShaderPipelineSource(
		const FLangSourceText& Original,
		const FModule& Parsed,
		const FBoundModule& Bound,
		const IR::FIRPassPipeline& Desired,
		TArray<FLangSourceEdit>& OutEdits,
		FString& OutText,
		FLangDiagnosticSink& Diagnostics)
	{
		using namespace Private::PipelineSource;

		OutEdits.Reset();
		OutText = Original.GetText();

		if (!Bound.Pipeline.bIsPipeline || Parsed.FileKind != ELangFileKind::Dsp)
		{
			return Diagnostics.Error(
				TEXT("DSH9109"),
				Original.GetPath(),
				Original.MakeSpan(0, 0),
				LOCTEXT("NotAPipeline", "Expected a '.dsp' file bound as a pipeline to rewrite, found another kind of file; the file was left unchanged."));
		}

		const FBoundPipeline& Pipeline = Bound.Pipeline;
		const IR::FIRPassPipeline& Current = Pipeline.Payload;
		FPipelineEdits Edits(Original, DetectPipelineNewLine(Original.GetText()));
		const FString NewLine = Edits.NewLine;

		const FDecl* FirstDecl = nullptr;
		for (const FDeclPtr& Declaration : Parsed.Declarations)
		{
			if (Declaration.IsValid() && Declaration->Span.Length > 0 && (!FirstDecl || PipelineDeclStart(*Declaration) < PipelineDeclStart(*FirstDecl)))
			{
				FirstDecl = Declaration.Get();
			}
		}

		// -- `#pragma pipeline`
		RewritePipelinePragma(Edits, Pipeline.Pragma, Current, Desired, FirstDecl);
		const FDecl* Anchor = (Pipeline.Pragma && Pipeline.Pragma->Span.Length > 0) ? Pipeline.Pragma : nullptr;

		// -- uniforms
		{
			TArray<FString> CurrentNames;
			TArray<FString> DesiredNames;
			TArray<const FDecl*> CurrentDecls;
			for (int32 Index = 0; Index < Current.Parameters.Num(); ++Index)
			{
				CurrentNames.Add(Current.Parameters[Index].Name);
				const int32 GlobalIndex = Pipeline.ParameterGlobals.IsValidIndex(Index) ? Pipeline.ParameterGlobals[Index] : INDEX_NONE;
				const FVariableDecl* Decl = Bound.Globals.IsValidIndex(GlobalIndex) ? Bound.Globals[GlobalIndex].Decl : nullptr;
				CurrentDecls.Add((Decl && Decl->Span.Length > 0) ? Decl : nullptr);
			}
			for (const IR::FIRPassParameter& Parameter : Desired.Parameters)
			{
				DesiredNames.Add(Parameter.Name);
			}

			const bool bOk = RewritePipelineGroup(
				Edits, CurrentNames, DesiredNames, CurrentDecls, FirstDecl, Anchor, /* bBlankLineBeforeNew */ false,
				[&](const int32 DesiredIndex)
				{
					return PrintPipelineDecl(*BuildPipelineParameterDecl(Desired.Parameters[DesiredIndex]), NewLine);
				},
				[&](const int32 CurrentIndex, const int32 DesiredIndex)
				{
					const FDecl* Decl = CurrentDecls[CurrentIndex];
					return !Decl || RewritePipelineParameter(
						Edits, Parsed, static_cast<const FVariableDecl&>(*Decl), Current.Parameters[CurrentIndex], Desired.Parameters[DesiredIndex], Diagnostics);
				},
				[&](const int32 CurrentIndex)
				{
					const FDecl* Decl = CurrentDecls[CurrentIndex];
					if (Decl && PipelineDeclSharesStatement(Parsed, static_cast<const FVariableDecl&>(*Decl)))
					{
						ReportPipelineSharedStatement(Original, static_cast<const FVariableDecl&>(*Decl), Diagnostics);
						return false;
					}
					return true;
				});
			if (!bOk)
			{
				OutEdits.Reset();
				return false;
			}
		}

		// -- buffers
		{
			TArray<FString> CurrentNames;
			TArray<FString> DesiredNames;
			TArray<const FDecl*> CurrentDecls;
			for (int32 Index = 0; Index < Current.Buffers.Num(); ++Index)
			{
				CurrentNames.Add(Current.Buffers[Index].Name);
				const FBufferDecl* Decl = Pipeline.BufferDecls.IsValidIndex(Index) ? Pipeline.BufferDecls[Index] : nullptr;
				CurrentDecls.Add((Decl && Decl->Span.Length > 0) ? Decl : nullptr);
			}
			for (const IR::FIRPassBuffer& Buffer : Desired.Buffers)
			{
				DesiredNames.Add(Buffer.Name);
			}

			RewritePipelineGroup(
				Edits, CurrentNames, DesiredNames, CurrentDecls, FirstDecl, Anchor, /* bBlankLineBeforeNew */ false,
				[&](const int32 DesiredIndex)
				{
					return PrintPipelineDecl(*BuildPipelineBufferDecl(Desired, Desired.Buffers[DesiredIndex], /* bKeepWrittenDefaults */ true), NewLine);
				},
				[&](const int32 CurrentIndex, const int32 DesiredIndex)
				{
					if (const FDecl* Decl = CurrentDecls[CurrentIndex])
					{
						RewritePipelineBuffer(Edits, static_cast<const FBufferDecl&>(*Decl), Current, Current.Buffers[CurrentIndex], Desired, Desired.Buffers[DesiredIndex]);
					}
					return true;
				},
				[](const int32) { return true; });
		}

		// -- the file's `hlsl` block: the text between its braces, the block itself when it comes or goes
		{
			const FHlslBlockDecl* Block = nullptr;
			for (const FDeclPtr& Declaration : Parsed.Declarations)
			{
				if (const FHlslBlockDecl* Candidate = Declaration.IsValid() ? Declaration->As<FHlslBlockDecl>() : nullptr)
				{
					Block = Candidate;
					break;
				}
			}
			const bool bChanged = Current.bHasSharedHlsl != Desired.bHasSharedHlsl
				|| !NormalizePipelineHlsl(Current.SharedHlsl).Equals(NormalizePipelineHlsl(Desired.SharedHlsl), ESearchCase::CaseSensitive);
			if (bChanged && Block && Block->BodySpan.Length >= 2 && Desired.bHasSharedHlsl)
			{
				Edits.Replace(Block->BodySpan.Offset + 1, Block->BodySpan.Length - 2, PipelineHlslWithNewLine(Desired.SharedHlsl, NewLine));
			}
			else if (bChanged && Block && !Desired.bHasSharedHlsl)
			{
				Edits.DeleteDecl(*Block);
			}
			else if (bChanged && !Block && Desired.bHasSharedHlsl)
			{
				FHlslBlockDecl Printed;
				Printed.RawBody = Desired.SharedHlsl;
				const FString Lines = NewLine + PrintPipelineDecl(Printed, NewLine) + NewLine;
				if (Anchor)
				{
					Edits.InsertAfterDecl(*Anchor, Lines);
				}
				else if (FirstDecl)
				{
					Edits.InsertBeforeDecl(*FirstDecl, PrintPipelineDecl(Printed, NewLine) + NewLine + NewLine);
				}
				else
				{
					Edits.Insert(Original.GetText().Len(), Lines);
				}
			}
		}

		// -- passes; their order within an injection point is the frame's
		{
			TArray<FString> CurrentNames;
			TArray<FString> DesiredNames;
			TArray<const FDecl*> CurrentDecls;
			for (int32 Index = 0; Index < Current.Passes.Num(); ++Index)
			{
				CurrentNames.Add(Current.Passes[Index].Name);
				const FPassDecl* Decl = Pipeline.PassDecls.IsValidIndex(Index) ? Pipeline.PassDecls[Index] : nullptr;
				CurrentDecls.Add((Decl && Decl->Span.Length > 0) ? Decl : nullptr);
			}
			for (const IR::FIRPass& Pass : Desired.Passes)
			{
				DesiredNames.Add(Pass.Name);
			}

			RewritePipelineGroup(
				Edits, CurrentNames, DesiredNames, CurrentDecls, FirstDecl, Anchor, /* bBlankLineBeforeNew */ true,
				[&](const int32 DesiredIndex)
				{
					return PrintPipelineDecl(*BuildPipelinePassDecl(Desired, Desired.Passes[DesiredIndex], /* bKeepWrittenDefaults */ true), NewLine);
				},
				[&](const int32 CurrentIndex, const int32 DesiredIndex)
				{
					if (const FDecl* Decl = CurrentDecls[CurrentIndex])
					{
						RewritePipelinePass(Edits, static_cast<const FPassDecl&>(*Decl), Current, Current.Passes[CurrentIndex], Desired, Desired.Passes[DesiredIndex]);
					}
					return true;
				},
				[](const int32) { return true; });
		}

		OutEdits = Edits.TakeAll();
		return ApplyPipelineEdits(Original, OutEdits, OutText, Diagnostics);
	}

	bool CompareDreamShaderPipelines(const IR::FIRPassPipeline& A, const IR::FIRPassPipeline& B, TArray<FString>* OutDifferences)
	{
		using namespace Private::PipelineSource;

		FPipelineDifferences Diff;
		Diff.Out = OutDifferences;
		const FString Top;

		Diff.Int(Top, TEXT("Order"), A.Order, B.Order);
		Diff.Text(Top, TEXT("the default injection point"), A.DefaultInjection, B.DefaultInjection);
		Diff.List(Top, TEXT("Views"), EffectiveViews(A), EffectiveViews(B));
		Diff.List(Top, TEXT("Requires"), PipelineRequires(A), PipelineRequires(B));
		Diff.Text(Top, TEXT("Enabled"), A.EnabledParameter, B.EnabledParameter);
		Diff.Bool(Top, TEXT("the file's 'hlsl' block"), A.bHasSharedHlsl, B.bHasSharedHlsl);
		if (A.bHasSharedHlsl && B.bHasSharedHlsl
			&& !NormalizePipelineHlsl(A.SharedHlsl).Equals(NormalizePipelineHlsl(B.SharedHlsl), ESearchCase::CaseSensitive))
		{
			Diff.Add(TEXT("the text of the file's 'hlsl' block differs"));
		}

		Diff.Int(Top, TEXT("the number of parameters"), A.Parameters.Num(), B.Parameters.Num());
		for (int32 Index = 0; Index < FMath::Min(A.Parameters.Num(), B.Parameters.Num()); ++Index)
		{
			const IR::FIRPassParameter& PA = A.Parameters[Index];
			const IR::FIRPassParameter& PB = B.Parameters[Index];
			const FString Where = FString::Printf(TEXT("parameter %d '%s': "), Index, *PA.Name);
			Diff.Text(Where, TEXT("the name"), PA.Name, PB.Name);
			Diff.Text(Where, TEXT("the type"), PA.Type, PB.Type);
			Diff.Text(Where, TEXT("the default"), PipelineValueText(PA.Default, PA.Type), PipelineValueText(PB.Default, PA.Type));
			Diff.Text(Where, TEXT("the group"), PA.Group, PB.Group);
			Diff.Text(Where, TEXT("the description"), NormalizePipelineDescription(PA.Description), NormalizePipelineDescription(PB.Description));
			Diff.Bool(Where, TEXT("the slider"), PA.bHasSlider, PB.bHasSlider);
			if (PA.bHasSlider && PB.bHasSlider)
			{
				Diff.Number(Where, TEXT("the slider's minimum"), PA.SliderMin, PB.SliderMin);
				Diff.Number(Where, TEXT("the slider's maximum"), PA.SliderMax, PB.SliderMax);
			}
			Diff.Int(Where, TEXT("the sort priority"), PA.SortPriority, PB.SortPriority);
		}

		Diff.Int(Top, TEXT("the number of buffers"), A.Buffers.Num(), B.Buffers.Num());
		for (int32 Index = 0; Index < FMath::Min(A.Buffers.Num(), B.Buffers.Num()); ++Index)
		{
			const IR::FIRPassBuffer& BA = A.Buffers[Index];
			const IR::FIRPassBuffer& BB = B.Buffers[Index];
			const FString Where = FString::Printf(TEXT("buffer '%s': "), *BA.Name);
			Diff.Text(Where, TEXT("the name"), BA.Name, BB.Name);
			Diff.Text(Where, TEXT("the format"), BA.Format, BB.Format);
			Diff.Text(Where, TEXT("the resolution"), BA.Resolution, BB.Resolution);
			if (BA.Resolution.Equals(ResolutionFixed, ESearchCase::CaseSensitive))
			{
				Diff.Int(Where, TEXT("the width"), BA.FixedWidth, BB.FixedWidth);
				Diff.Int(Where, TEXT("the height"), BA.FixedHeight, BB.FixedHeight);
			}
			else
			{
				Diff.Number(Where, TEXT("Scale"), BA.Scale, BB.Scale);
			}
			Diff.Bool(Where, TEXT("Clear"), BA.bClear, BB.bClear);
			if (BA.bClear && BB.bClear)
			{
				Diff.Numbers(Where, TEXT("the clear value"), BA.ClearValue, BB.ClearValue, 4);
			}
			Diff.Int(Where, TEXT("Mips"), BA.Mips, BB.Mips);
			Diff.Bool(Where, TEXT("History"), BA.bHistory, BB.bHistory);
			Diff.Bool(Where, TEXT("Export"), BA.bExport, BB.bExport);
			Diff.Text(Where, TEXT("the description"), NormalizePipelineDescription(BA.Description), NormalizePipelineDescription(BB.Description));
		}

		Diff.Int(Top, TEXT("the number of passes"), A.Passes.Num(), B.Passes.Num());
		for (int32 Index = 0; Index < FMath::Min(A.Passes.Num(), B.Passes.Num()); ++Index)
		{
			if (!A.Passes[Index].Name.Equals(B.Passes[Index].Name, ESearchCase::CaseSensitive))
			{
				Diff.Add(FString::Printf(TEXT("pass %d is '%s' on one side and '%s' on the other"), Index, *A.Passes[Index].Name, *B.Passes[Index].Name));
				continue;
			}
			ComparePipelinePasses(Diff, A.Passes[Index], B.Passes[Index]);
		}

		return Diff.bEqual;
	}
}

#undef LOCTEXT_NAMESPACE
