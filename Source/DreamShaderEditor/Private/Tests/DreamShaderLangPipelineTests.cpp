// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Lang2.Pipeline.* -- a `.dsp`, a Custom Pass pipeline, without the compiler:
//
//   Parse      the two declarations a `.dsp` adds, their statements and spans, recovery, trivia, the words that are
//              keywords only where they begin a declaration or a statement, and the references a host resolves
//   Print      the canonical text of a payload (PrintDreamShaderPipeline): byte for byte for the documented example,
//              and print -> parse -> bind giving the payload back for one that leaves no key at its default
//   Format     `dsc fmt` over a `.dsp`: layout, comments, line terminators, the files it leaves alone
//   Bind       the payload the binder builds, every default applied, with and without the host's references, and
//              the slot packing rule against the runtime's own (UE::DreamPass::LayoutSlotParameters)
//   Rules      the rules V1-V13 one diagnostic code at a time, each raised by one case and kept quiet by another
//   Compare    CompareDreamShaderPipelines: float32 numbers, normalised descriptions, effective defaults
//   Rewrite    Adopt's splice into an existing file (RewriteDreamShaderPipelineSource)
//   PassNodes  UE.DreamPassOutput and UE.DreamPassBuffer in a `.dss`: DSH5300-DSH5303
//
// Every assertion about a diagnostic names its code, never its words. Core only, but for Bind's packing case, which
// calls into DreamShaderPass. The Rules cases bind against references built here (MakeRuleReferences): the facts a
// host would have resolved for the materials, shader files and pass layers a source names.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamPassPipeline.h"
#include "DreamPassTypes.h"
#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangFormat.h"
#include "Lang/LangInstanceSource.h"
#include "Lang/LangParser.h"
#include "Lang/LangPipelineSource.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::LangPipelineTests
{
	using namespace ::UE::DreamShader::Lang;
	namespace IR = ::UE::DreamShader::IR;
	namespace Tests = ::UE::DreamShader::Editor::Private::Tests;

	// =============================================================================================
	// Text
	// =============================================================================================

	/** `\r\n` as `\n`: a raw string carries whatever line terminators this file was checked out with. */
	inline FString Lf(const FString& Text)
	{
		return Text.Replace(TEXT("\r\n"), TEXT("\n"), ESearchCase::CaseSensitive);
	}

	/** A raw string as a source text: `\n` line terminators, and the line break right after `R"DSP(` dropped. */
	inline FString Dsp(const TCHAR* Text)
	{
		FString Result = Lf(Text);
		if (Result.StartsWith(TEXT("\n"), ESearchCase::CaseSensitive))
		{
			Result.RightChopInline(1);
		}
		return Result;
	}

	/** Each line followed by `\n`. */
	inline FString Lines(std::initializer_list<const TCHAR*> InLines)
	{
		FString Result;
		for (const TCHAR* Line : InLines)
		{
			Result += Line;
			Result += TEXT("\n");
		}
		return Result;
	}

	/** Text with the first occurrence of From replaced by To; empty when From is not in it, which no expected text is. */
	inline FString ReplaceOnce(const FString& Text, const FString& From, const FString& To)
	{
		const int32 Index = Text.Find(From, ESearchCase::CaseSensitive);
		if (Index == INDEX_NONE)
		{
			return FString();
		}
		return Text.Left(Index) + To + Text.Mid(Index + From.Len());
	}

	/** The 1-based line of the first occurrence of Marker; INDEX_NONE when it is not there. */
	inline int32 FindLineOf(const FString& Text, const FString& Marker)
	{
		const int32 Offset = Text.Find(Marker, ESearchCase::CaseSensitive);
		if (Offset == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		int32 Line = 1;
		for (int32 Index = 0; Index < Offset; ++Index)
		{
			Line += Text[Index] == TEXT('\n') ? 1 : 0;
		}
		return Line;
	}

	inline int32 CountOccurrences(const FString& Text, const FString& Needle)
	{
		int32 Count = 0;
		for (int32 From = Text.Find(Needle, ESearchCase::CaseSensitive); From != INDEX_NONE; From = Text.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, From + Needle.Len()))
		{
			++Count;
		}
		return Count;
	}

	/** Applies sorted, non-overlapping edits to Original; false when they are not that. */
	inline bool ApplyEdits(const FString& Original, const TArray<FLangSourceEdit>& Edits, FString& OutText)
	{
		OutText.Reset();
		int32 Cursor = 0;
		for (const FLangSourceEdit& Edit : Edits)
		{
			if (Edit.Span.Offset < Cursor || Edit.Span.End() > Original.Len())
			{
				return false;
			}
			OutText += Original.Mid(Cursor, Edit.Span.Offset - Cursor);
			OutText += Edit.NewText;
			Cursor = Edit.Span.End();
		}
		OutText += Original.Mid(Cursor);
		return true;
	}

	// =============================================================================================
	// Diagnostics
	// =============================================================================================

	inline bool HasCode(const FLangDiagnosticSink& Sink, const TCHAR* Code)
	{
		return Sink.GetDiagnostics().ContainsByPredicate([Code](const FLangDiagnostic& Diagnostic)
		{
			return Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive);
		});
	}

	inline bool HasCode(const FLangDiagnosticSink& Sink, const TCHAR* Code, const ELangSeverity Severity)
	{
		return Sink.GetDiagnostics().ContainsByPredicate([Code, Severity](const FLangDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == Severity && Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive);
		});
	}

	inline bool HasSeverity(const FLangDiagnosticSink& Sink, const ELangSeverity Severity)
	{
		return Sink.GetDiagnostics().ContainsByPredicate([Severity](const FLangDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == Severity;
		});
	}

	/** Every diagnostic as `[Severity] DSHnnnn: message`, for an info line when an assertion fails. */
	inline FString DescribeDiagnostics(const FLangDiagnosticSink& Sink)
	{
		TArray<FString> Entries;
		for (const FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
		{
			Entries.Add(FString::Printf(TEXT("[%s] %s"), LexToString(Diagnostic.Severity), *FLangDiagnosticSink::ToWireString(Diagnostic)));
		}
		return Entries.Num() > 0 ? FString::Join(Entries, TEXT(" | ")) : FString(TEXT("<none>"));
	}

	/** A case-sensitive string compare that says both sides (FAutomationTestBase::TestEqual on FString ignores case). */
	inline bool ExpectString(FAutomationTestBase& Test, const FString& What, const FString& Actual, const FString& Expected)
	{
		const bool bEqual = Actual.Equals(Expected, ESearchCase::CaseSensitive);
		Test.TestTrue(FString::Printf(TEXT("%s: '%s' (expected '%s')"), *What, *Actual, *Expected), bEqual);
		return bEqual;
	}

	inline bool ExpectText(FAutomationTestBase& Test, const FString& What, const FString& Actual, const FString& Expected)
	{
		const bool bEqual = Actual.Equals(Expected, ESearchCase::CaseSensitive);
		if (!bEqual)
		{
			Test.AddInfo(FString::Printf(TEXT("%s: %s"), *What, *Tests::DescribeDreamShaderTextDifference(Actual, Expected)));
			Test.AddInfo(FString::Printf(TEXT("%s -- the actual text:\n%s"), *What, *Actual));
		}
		Test.TestTrue(What, bEqual);
		return bEqual;
	}

	inline bool ExpectSamePipeline(FAutomationTestBase& Test, const FString& What, const IR::FIRPassPipeline& A, const IR::FIRPassPipeline& B)
	{
		TArray<FString> Differences;
		const bool bSame = CompareDreamShaderPipelines(A, B, &Differences);
		for (const FString& Difference : Differences)
		{
			Test.AddInfo(FString::Printf(TEXT("%s: %s"), *What, *Difference));
		}
		Test.TestTrue(What, bSame);
		return bSame;
	}

	// =============================================================================================
	// Parse and bind
	// =============================================================================================

	/** One parse and bind of a pipeline text. The members are declared in the order the bound module needs them destroyed. */
	struct FPipelineBind
	{
		FPipelineReferences References;
		bool bReferences = false;
		FLangParseResult Parse;
		FLangBindResult Bind;

		bool HasBound() const { return Bind.Bound.IsValid(); }
		const FBoundModule& Module() const { return *Bind.Bound; }
		const FBoundPipeline& Pipeline() const { return Bind.Bound->Pipeline; }
		const IR::FIRPassPipeline& Payload() const { return Bind.Bound->Pipeline.Payload; }
		bool HasErrors() const { return Parse.Diagnostics.HasErrors() || Bind.Diagnostics.HasErrors(); }
		bool Has(const TCHAR* Code) const { return HasCode(Parse.Diagnostics, Code) || HasCode(Bind.Diagnostics, Code); }
		bool Has(const TCHAR* Code, const ELangSeverity Severity) const { return HasCode(Parse.Diagnostics, Code, Severity) || HasCode(Bind.Diagnostics, Code, Severity); }
		FString Describe() const { return DescribeDiagnostics(Parse.Diagnostics) + TEXT(" || ") + DescribeDiagnostics(Bind.Diagnostics); }
	};

	/** Parses Text as Path and binds it: engine-free without References, against a copy of them with. */
	inline void BindPipeline(
		FPipelineBind& Out,
		const FString& Text,
		const TCHAR* Path = TEXT("Test.dsp"),
		const FPipelineReferences* References = nullptr,
		const IR::FBuiltinCatalog* Catalog = nullptr)
	{
		if (References)
		{
			Out.References = *References;
			Out.bReferences = true;
		}
		Out.Parse = ParseDreamShaderLang(FLangSourceText(Path, Text), FLangParseOptions());
		if (!Out.Parse.Module.IsValid())
		{
			return;
		}
		FBindOptions Options;
		Options.Catalog = Catalog ? Catalog : &Tests::GetDreamShaderTestBuiltinCatalog();
		Options.PipelineReferences = Out.bReferences ? &Out.References : nullptr;
		Out.Bind = BindDreamShaderLang(*Out.Parse.Module, Options);
	}

	inline const IR::FIRPass* FindPayloadPass(const IR::FIRPassPipeline& Payload, const TCHAR* Name)
	{
		return Payload.Passes.FindByPredicate([Name](const IR::FIRPass& Pass) { return Pass.Name.Equals(Name, ESearchCase::CaseSensitive); });
	}

	inline const IR::FIRPassBuffer* FindPayloadBuffer(const IR::FIRPassPipeline& Payload, const TCHAR* Name)
	{
		return Payload.Buffers.FindByPredicate([Name](const IR::FIRPassBuffer& Buffer) { return Buffer.Name.Equals(Name, ESearchCase::CaseSensitive); });
	}

	inline const IR::FIRPassParameter* FindPayloadParameter(const IR::FIRPassPipeline& Payload, const TCHAR* Name)
	{
		return Payload.Parameters.FindByPredicate([Name](const IR::FIRPassParameter& Parameter) { return Parameter.Name.Equals(Name, ESearchCase::CaseSensitive); });
	}

	// =============================================================================================
	// Payloads by hand
	// =============================================================================================

	inline IR::FIRPropertyValue MakeFloats(std::initializer_list<double> Values)
	{
		double V[4] = { 0.0, 0.0, 0.0, 0.0 };
		int32 Count = 0;
		for (const double Value : Values)
		{
			if (Count < 4)
			{
				V[Count++] = Value;
			}
		}
		return IR::FIRPropertyValue::MakeFloat4(V, FMath::Max(Count, 1));
	}

	inline IR::FIRPassParameter MakeParameter(const TCHAR* Name, const TCHAR* Type, const IR::FIRPropertyValue& Default)
	{
		IR::FIRPassParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Type;
		Parameter.Default = Default;
		return Parameter;
	}

	inline IR::FIRPassBuffer MakeBuffer(const TCHAR* Name, const TCHAR* Format)
	{
		IR::FIRPassBuffer Buffer;
		Buffer.Name = Name;
		Buffer.Format = Format;
		return Buffer;
	}

	inline IR::FIRPassBinding MakeBinding(const TCHAR* Slot, const TCHAR* Buffer, const bool bPrevious = false)
	{
		IR::FIRPassBinding Binding;
		Binding.Slot = Slot;
		Binding.Buffer = Buffer;
		Binding.bPrevious = bPrevious;
		return Binding;
	}

	inline IR::FIRPassParam MakeParameterParam(const TCHAR* Target, const TCHAR* Parameter, const double Multiplier = 1.0, const double Offset = 0.0)
	{
		IR::FIRPassParam Param;
		Param.Target = Target;
		Param.SourceKind = TEXT("Parameter");
		Param.Parameter = Parameter;
		Param.Multiplier = Multiplier;
		Param.Offset = Offset;
		return Param;
	}

	inline IR::FIRPassParam MakeWeightParam(const TCHAR* Target)
	{
		IR::FIRPassParam Param;
		Param.Target = Target;
		Param.SourceKind = TEXT("Weight");
		return Param;
	}

	inline IR::FIRPassParam MakeConstantParam(const TCHAR* Target, const TCHAR* Type, const IR::FIRPropertyValue& Value)
	{
		IR::FIRPassParam Param;
		Param.Target = Target;
		Param.SourceKind = TEXT("Constant");
		Param.ConstantType = Type;
		Param.Constant = Value;
		return Param;
	}

	inline IR::FIRPassFilterTerm MakeStencilTerm(const int32 Value, const int32 Mask = 255)
	{
		IR::FIRPassFilterTerm Term;
		Term.Kind = TEXT("Stencil");
		Term.StencilValue = Value;
		Term.StencilMask = Mask;
		return Term;
	}

	inline IR::FIRPassFilterTerm MakeLayerTerm(std::initializer_list<const TCHAR*> Layers)
	{
		IR::FIRPassFilterTerm Term;
		Term.Kind = TEXT("Layer");
		for (const TCHAR* Layer : Layers)
		{
			Term.Layers.Add(Layer);
		}
		return Term;
	}

	inline IR::FIRPassFilterTerm MakeListTerm(const TCHAR* List)
	{
		IR::FIRPassFilterTerm Term;
		Term.Kind = TEXT("List");
		Term.List = List;
		return Term;
	}

	/**
	 * The Highlight outline of Docs/examples/custom-pass.md as the binder makes it engine-free: every value it fills in is
	 * filled in here (the mesh mode, the Nanite policy, the compute dispatch, the composite's injection point).
	 */
	inline IR::FIRPassPipeline MakeHighlightPayload()
	{
		IR::FIRPassPipeline Pipeline;
		Pipeline.Order = 100;

		{
			IR::FIRPassParameter Color = MakeParameter(TEXT("OutlineColor"), TEXT("float4"), MakeFloats({ 1.0, 0.6, 0.0, 1.0 }));
			Color.Group = TEXT("Look");
			Pipeline.Parameters.Add(MoveTemp(Color));
		}
		{
			IR::FIRPassParameter Width = MakeParameter(TEXT("OutlineWidth"), TEXT("float"), MakeFloats({ 3.0 }));
			Width.Group = TEXT("Look");
			Width.bHasSlider = true;
			Width.SliderMin = 1.0;
			Width.SliderMax = 8.0;
			Pipeline.Parameters.Add(MoveTemp(Width));
		}

		Pipeline.Buffers.Add(MakeBuffer(TEXT("Mask"), TEXT("R8")));
		{
			IR::FIRPassBuffer Blurred = MakeBuffer(TEXT("Blurred"), TEXT("R8"));
			Blurred.Scale = 0.5;
			Blurred.bExport = true;
			Pipeline.Buffers.Add(MoveTemp(Blurred));
		}

		{
			IR::FIRPass Draw;
			Draw.Name = TEXT("DrawMask");
			Draw.Kind = TEXT("mesh");
			Draw.Injection = TEXT("AfterOpaque");
			Draw.bInjectionWritten = true;
			Draw.Filter.AddDefaulted_GetRef().AllOf.Add(MakeLayerTerm({ TEXT("Highlight") }));
			Draw.MaterialReference = TEXT("M_HighlightMask");
			Draw.MeshMode = TEXT("Override");
			Draw.Depth = TEXT("None");
			Draw.Nanite = TEXT("Skip");
			Draw.Writes.Add(MakeBinding(TEXT("Output0"), TEXT("Mask")));
			Pipeline.Passes.Add(MoveTemp(Draw));
		}
		{
			IR::FIRPass Blur;
			Blur.Name = TEXT("Blur");
			Blur.Kind = TEXT("compute");
			Blur.Injection = TEXT("AfterOpaque");
			Blur.bInjectionWritten = true;
			Blur.ShaderReference = TEXT("BoxBlur.usf");
			Blur.Entry = TEXT("BlurCS");
			Blur.DispatchMode = TEXT("Buffer");
			Blur.DispatchBuffer = TEXT("Blurred");
			Blur.DispatchScale = 1.0;
			Blur.Reads.Add(MakeBinding(TEXT("Source"), TEXT("Mask")));
			Blur.Writes.Add(MakeBinding(TEXT("Result"), TEXT("Blurred")));
			Blur.Params.Add(MakeParameterParam(TEXT("Radius"), TEXT("OutlineWidth")));
			Pipeline.Passes.Add(MoveTemp(Blur));
		}
		{
			IR::FIRPass Composite;
			Composite.Name = TEXT("Composite");
			Composite.Kind = TEXT("fullscreen");
			Composite.Injection = TEXT("BeforePostProcess");
			Composite.MaterialReference = TEXT("PP_OutlineComposite");
			Composite.Reads.Add(MakeBinding(TEXT("Mask"), TEXT("Mask")));
			Composite.Reads.Add(MakeBinding(TEXT("Blurred"), TEXT("Blurred")));
			Composite.Writes.Add(MakeBinding(TEXT("SceneColor"), TEXT("SceneColor")));
			Composite.Params.Add(MakeParameterParam(TEXT("Color"), TEXT("OutlineColor")));
			Pipeline.Passes.Add(MoveTemp(Composite));
		}
		return Pipeline;
	}

	/**
	 * A payload that leaves almost nothing at its default: every pragma key, every parameter type, a multi-line
	 * description, a fixed size, a resolution that is not the inferred one, every mesh key, every dispatch form, written
	 * thread groups, and every shape of `param`. Built as the binder would build it, so that the printed text binds back
	 * to it.
	 */
	inline IR::FIRPassPipeline MakeRichPayload()
	{
		IR::FIRPassPipeline Pipeline;
		Pipeline.Order = -3;
		Pipeline.DefaultInjection = TEXT("PostProcess.AfterDOF");
		Pipeline.Views = { TEXT("Game"), TEXT("SceneCapture") };
		Pipeline.Requires = { TEXT("PostProcess"), TEXT("CustomStencil") };
		Pipeline.EnabledParameter = TEXT("On");

		Pipeline.Parameters.Add(MakeParameter(TEXT("On"), TEXT("bool"), IR::FIRPropertyValue::MakeBool(true)));
		Pipeline.Parameters.Add(MakeParameter(TEXT("Taps"), TEXT("int"), IR::FIRPropertyValue::MakeInt(-2)));
		{
			IR::FIRPassParameter Tint = MakeParameter(TEXT("Tint"), TEXT("float3"), MakeFloats({ 0.25, 0.5, 1.0 }));
			Tint.Group = TEXT("Look");
			Tint.Description = TEXT("Line one.\nLine two.");
			Tint.SortPriority = 5;
			Pipeline.Parameters.Add(MoveTemp(Tint));
		}
		{
			IR::FIRPassParameter Gain = MakeParameter(TEXT("Gain"), TEXT("float"), MakeFloats({ 0.0 }));
			Gain.bHasSlider = true;
			Gain.SliderMin = 0.5;
			Gain.SliderMax = 2.5;
			Pipeline.Parameters.Add(MoveTemp(Gain));
		}
		Pipeline.Parameters.Add(MakeParameter(TEXT("Noise"), TEXT("Texture2D"), IR::FIRPropertyValue::MakeObject(TEXT("/Engine/EngineResources/DefaultTexture"))));
		Pipeline.Parameters.Add(MakeParameter(TEXT("Empty"), TEXT("Texture2D"), IR::FIRPropertyValue::MakeObject(FString())));

		Pipeline.Buffers.Add(MakeBuffer(TEXT("Depth"), TEXT("Depth32")));
		Pipeline.Buffers.Add(MakeBuffer(TEXT("Mask"), TEXT("R8")));
		{
			IR::FIRPassBuffer Field = MakeBuffer(TEXT("Field"), TEXT("RG16F"));
			Field.Resolution = TEXT("Fixed");
			Field.bResolutionWritten = true;
			Field.FixedWidth = 64;
			Field.FixedHeight = 32;
			Field.bClear = false;
			Field.bHistory = true;
			Pipeline.Buffers.Add(MoveTemp(Field));
		}
		{
			IR::FIRPassBuffer Soft = MakeBuffer(TEXT("Soft"), TEXT("RGBA16F"));
			Soft.Scale = 0.25;
			Soft.ClearValue[0] = 1.0;
			Soft.ClearValue[1] = 0.5;
			Soft.ClearValue[2] = 0.0;
			Soft.ClearValue[3] = 1.0;
			Soft.Mips = 3;
			Soft.bExport = true;
			Soft.Description = TEXT("Soft.");
			Pipeline.Buffers.Add(MoveTemp(Soft));
		}
		{
			// Written although Render: its first writer runs after the tonemapper, where the inferred resolution is Output.
			IR::FIRPassBuffer Late = MakeBuffer(TEXT("Late"), TEXT("RGBA8"));
			Late.Resolution = TEXT("Render");
			Late.bResolutionWritten = true;
			Pipeline.Buffers.Add(MoveTemp(Late));
		}
		{
			IR::FIRPassBuffer Grey = MakeBuffer(TEXT("Grey"), TEXT("R16F"));
			for (int32 Channel = 0; Channel < 4; ++Channel)
			{
				Grey.ClearValue[Channel] = 0.5;
			}
			Pipeline.Buffers.Add(MoveTemp(Grey));
		}

		{
			IR::FIRPass Sim;
			Sim.Name = TEXT("Sim");
			Sim.Kind = TEXT("compute");
			Sim.Injection = TEXT("BeginView");
			Sim.bInjectionWritten = true;
			Sim.ShaderReference = TEXT("/Project/Passes/Sim.usf");
			Sim.ShaderVirtualPath = TEXT("/Project/Passes/Sim.usf");
			Sim.Entry = TEXT("SimCS");
			Sim.ThreadsX = 16;
			Sim.ThreadsY = 4;
			Sim.ThreadsZ = 1;
			Sim.bThreadsWritten = true;
			Sim.DispatchMode = TEXT("Buffer");
			Sim.DispatchBuffer = TEXT("Field");
			Sim.DispatchScale = 0.5;
			Sim.Reads.Add(MakeBinding(TEXT("Previous"), TEXT("Field"), /*bPrevious*/ true));
			Sim.Writes.Add(MakeBinding(TEXT("Result"), TEXT("Field")));
			Sim.Params.Add(MakeParameterParam(TEXT("Scaled"), TEXT("Gain"), 2.0, -1.0));
			Sim.Params.Add(MakeWeightParam(TEXT("Weight")));
			// `Gain * -1.0` binds to an offset of -0.0 (0 * -1); the payload says so, so that the comparison below is about
			// the printing and not about the sign of a zero (see the Compare test).
			Sim.Params.Add(MakeParameterParam(TEXT("Neg"), TEXT("Gain"), -1.0, -0.0));
			Sim.Params.Add(MakeConstantParam(TEXT("Count"), TEXT("int"), IR::FIRPropertyValue::MakeInt(3)));
			Sim.Params.Add(MakeConstantParam(TEXT("Flag"), TEXT("bool"), IR::FIRPropertyValue::MakeBool(true)));
			Sim.Params.Add(MakeConstantParam(TEXT("Pair"), TEXT("float2"), MakeFloats({ 0.25, 0.75 })));
			Sim.Params.Add(MakeConstantParam(TEXT("Third"), TEXT("float"), MakeFloats({ 0.3 })));
			Pipeline.Passes.Add(MoveTemp(Sim));
		}
		{
			IR::FIRPass Draw;
			Draw.Name = TEXT("Draw");
			Draw.Kind = TEXT("mesh");
			Draw.Injection = TEXT("AfterOpaque");
			Draw.bInjectionWritten = true;
			Draw.EnabledParameter = TEXT("On");
			{
				IR::FIRPassFilterClause& Clause = Draw.Filter.AddDefaulted_GetRef();
				Clause.AllOf.Add(MakeLayerTerm({ TEXT("Highlight"), TEXT("Other Layer") }));
				Clause.AllOf.Add(MakeStencilTerm(4, 15));
			}
			Draw.Filter.AddDefaulted_GetRef().AllOf.Add(MakeListTerm(TEXT("Enemies")));
			Draw.MaterialReference = TEXT("M_Mask");
			Draw.MeshMode = TEXT("OwnOrOverride");
			Draw.Depth = TEXT("Own");
			Draw.DepthBuffer = TEXT("Depth");
			Draw.Cull = TEXT("Front");
			Draw.Blend = TEXT("Max");
			Draw.Usage = { TEXT("StaticMesh"), TEXT("SplineMesh") };
			Draw.Nanite = TEXT("AssignStencil");
			Draw.AssignedStencilValue = 200;
			Draw.NaniteValue[0] = 1.0;
			Draw.NaniteValue[1] = 0.5;
			Draw.NaniteValue[2] = 0.0;
			Draw.NaniteValue[3] = 1.0;
			Draw.Writes.Add(MakeBinding(TEXT("Output0"), TEXT("Mask")));
			Draw.Params.Add(MakeParameterParam(TEXT("Tint"), TEXT("Tint")));
			Pipeline.Passes.Add(MoveTemp(Draw));
		}
		{
			IR::FIRPass Fill;
			Fill.Name = TEXT("Fill");
			Fill.Kind = TEXT("clear");
			Fill.Injection = TEXT("AfterOpaque");
			Fill.bInjectionWritten = true;
			Fill.ClearValue[0] = 0.25;
			Fill.ClearValue[1] = 0.5;
			Fill.ClearValue[2] = 0.75;
			Fill.ClearValue[3] = 1.0;
			Fill.Writes.Add(MakeBinding(TEXT("Grey"), TEXT("Grey")));
			Pipeline.Passes.Add(MoveTemp(Fill));
		}
		{
			IR::FIRPass Sweep;
			Sweep.Name = TEXT("Sweep");
			Sweep.Kind = TEXT("compute");
			Sweep.Injection = TEXT("BeforePostProcess");
			Sweep.bInjectionWritten = true;
			Sweep.ShaderReference = TEXT("Sweep.usf");
			Sweep.Entry = TEXT("SweepCS");
			Sweep.DispatchMode = TEXT("Fixed");
			Sweep.DispatchX = 64;
			Sweep.DispatchY = 64;
			Sweep.DispatchZ = 1;
			Sweep.Writes.Add(MakeBinding(TEXT("Out"), TEXT("Grey")));
			Pipeline.Passes.Add(MoveTemp(Sweep));
		}
		{
			IR::FIRPass Show;
			Show.Name = TEXT("Show");
			Show.Kind = TEXT("fullscreen");
			Show.Injection = TEXT("BeforePostProcess");
			Show.bInjectionWritten = true;
			Show.MaterialReference = TEXT("PP_Show");
			Show.Reads.Add(MakeBinding(TEXT("Mask"), TEXT("Mask")));
			Show.Writes.Add(MakeBinding(TEXT("SceneColor"), TEXT("SceneColor")));
			Show.Params.Add(MakeParameterParam(TEXT("Color"), TEXT("Tint")));
			Pipeline.Passes.Add(MoveTemp(Show));
		}
		{
			IR::FIRPass Wide;
			Wide.Name = TEXT("Wide");
			Wide.Kind = TEXT("compute");
			Wide.Injection = TEXT("PostProcess.AfterDOF");
			Wide.ShaderReference = TEXT("Sweep.usf");
			Wide.Entry = TEXT("WideCS");
			Wide.DispatchMode = TEXT("Buffer");
			Wide.DispatchBuffer = TEXT("Soft");
			Wide.DispatchScale = 0.3;
			Wide.Reads.Add(MakeBinding(TEXT("Source"), TEXT("Grey")));
			Wide.Writes.Add(MakeBinding(TEXT("Result"), TEXT("Soft")));
			Pipeline.Passes.Add(MoveTemp(Wide));
		}
		{
			IR::FIRPass Tone;
			Tone.Name = TEXT("Tone");
			Tone.Kind = TEXT("fullscreen");
			Tone.Injection = TEXT("PostProcess.AfterDOF");
			Tone.Description = TEXT("Tone it.");
			Tone.ShaderReference = TEXT("Tone.usf");
			Tone.Entry = TEXT("TonePS");
			Tone.Reads.Add(MakeBinding(TEXT("In"), TEXT("Soft")));
			Tone.Writes.Add(MakeBinding(TEXT("Out"), TEXT("SceneColor")));
			Pipeline.Passes.Add(MoveTemp(Tone));
		}
		{
			IR::FIRPass Store;
			Store.Name = TEXT("Store");
			Store.Kind = TEXT("copy");
			Store.Injection = TEXT("PostProcess.AfterTonemap");
			Store.bInjectionWritten = true;
			Store.Reads.Add(MakeBinding(TEXT("SceneColor"), TEXT("SceneColor")));
			Store.Writes.Add(MakeBinding(TEXT("Late"), TEXT("Late")));
			Pipeline.Passes.Add(MoveTemp(Store));
		}
		return Pipeline;
	}

	// =============================================================================================
	// The references a host resolves, for the Rules cases
	// =============================================================================================

	inline FPipelineMaterialInfo MakeMaterialInfo(const TCHAR* Reference, const TCHAR* Domain, const TCHAR* BlendableLocation = TEXT(""))
	{
		FPipelineMaterialInfo Info;
		Info.Reference = Reference;
		Info.ObjectPath = FString::Printf(TEXT("/Game/Rules/%s.%s"), Reference, Reference);
		Info.bFound = true;
		Info.Domain = Domain;
		Info.BlendableLocation = BlendableLocation;
		return Info;
	}

	inline FPipelineShaderInfo MakeShaderInfo(const TCHAR* Reference, const TCHAR* FilePath, const bool bExists = true)
	{
		FPipelineShaderInfo Info;
		Info.Reference = Reference;
		Info.FilePath = FilePath;
		Info.bExists = bExists;
		return Info;
	}

	/**
	 * What a host would have found for every material and shader file the Rules cases name, and the project's pass layers.
	 * bCustomPassAvailable false is an engine older than 5.8, where only a material's domain and blendable location are read.
	 */
	inline FPipelineReferences MakeRuleReferences(const bool bCustomPassAvailable = true)
	{
		FPipelineReferences References;
		References.bCustomPassAvailable = bCustomPassAvailable;
		References.LayerNames = { TEXT("Highlight"), TEXT("XRay") };

		// ---- Post Process materials
		{
			// Two named inputs; its own SceneTexture node takes PostProcessInput0.
			FPipelineMaterialInfo Info = MakeMaterialInfo(TEXT("PP_Two"), TEXT("PostProcess"), TEXT("SceneColorAfterDOF"));
			Info.UserSceneTextureInputs = { TEXT("Mask"), TEXT("Blurred") };
			Info.PostProcessInputsUsed = 0x1;
			References.Materials.Add(MoveTemp(Info));
		}
		{
			// Writes data: the pre-exposure scale is off.
			FPipelineMaterialInfo Info = MakeMaterialInfo(TEXT("PP_Data"), TEXT("PostProcess"), TEXT("SceneColorAfterDOF"));
			Info.UserSceneTextureInputs = { TEXT("Source") };
			Info.bDisablePreExposureScale = true;
			References.Materials.Add(MoveTemp(Info));
		}
		{
			// The same, with the pre-exposure scale left on.
			FPipelineMaterialInfo Info = MakeMaterialInfo(TEXT("PP_NoExposure"), TEXT("PostProcess"), TEXT("SceneColorAfterDOF"));
			Info.UserSceneTextureInputs = { TEXT("Source") };
			References.Materials.Add(MoveTemp(Info));
		}
		{
			// Five named inputs and no SceneTexture node: the five slots are all free.
			FPipelineMaterialInfo Info = MakeMaterialInfo(TEXT("PP_Five"), TEXT("PostProcess"), TEXT("SceneColorAfterDOF"));
			Info.UserSceneTextureInputs = { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E") };
			Info.bDisablePreExposureScale = true;
			References.Materials.Add(MoveTemp(Info));
		}
		{
			// Five named inputs, and two slots taken by its own SceneTexture nodes.
			FPipelineMaterialInfo Info = MakeMaterialInfo(TEXT("PP_FiveBusy"), TEXT("PostProcess"), TEXT("SceneColorAfterDOF"));
			Info.UserSceneTextureInputs = { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E") };
			Info.PostProcessInputsUsed = 0x3;
			Info.bDisablePreExposureScale = true;
			References.Materials.Add(MoveTemp(Info));
		}
		{
			FPipelineMaterialInfo Info = MakeMaterialInfo(TEXT("PP_AfterTonemap"), TEXT("PostProcess"), TEXT("SceneColorAfterTonemapping"));
			Info.UserSceneTextureInputs = { TEXT("Mask") };
			References.Materials.Add(MoveTemp(Info));
		}

		// ---- Surface materials, for mesh passes
		const TArray<FString> EveryUsage = { TEXT("StaticMesh"), TEXT("InstancedStaticMeshes"), TEXT("SkeletalMesh"), TEXT("Landscape") };
		{
			FPipelineMaterialInfo Info = MakeMaterialInfo(TEXT("M_Mask"), TEXT("Surface"));
			Info.bHasPassOutput = true;
			Info.PassOutputsConnected = 0x3;
			Info.Usages = EveryUsage;
			References.Materials.Add(MoveTemp(Info));
		}
		{
			FPipelineMaterialInfo Info = MakeMaterialInfo(TEXT("M_NoOutput"), TEXT("Surface"));
			Info.Usages = EveryUsage;
			References.Materials.Add(MoveTemp(Info));
		}
		{
			FPipelineMaterialInfo Info = MakeMaterialInfo(TEXT("M_NoSkeletal"), TEXT("Surface"));
			Info.bHasPassOutput = true;
			Info.PassOutputsConnected = 0x1;
			Info.Usages = { TEXT("StaticMesh"), TEXT("InstancedStaticMeshes"), TEXT("Landscape") };
			References.Materials.Add(MoveTemp(Info));
		}
		{
			FPipelineMaterialInfo Info = MakeMaterialInfo(TEXT("M_NewTranslator"), TEXT("Surface"));
			Info.bHasPassOutput = true;
			Info.PassOutputsConnected = 0x1;
			Info.Usages = EveryUsage;
			Info.bUsesNewTranslator = true;
			References.Materials.Add(MoveTemp(Info));
		}
		{
			// Named, and nothing found for it.
			FPipelineMaterialInfo Info;
			Info.Reference = TEXT("M_Missing");
			References.Materials.Add(MoveTemp(Info));
		}

		// ---- Shader files
		{
			FPipelineShaderInfo Info = MakeShaderInfo(TEXT("Blur.usf"), TEXT("D:/Rules/Passes/Blur.usf"));
			Info.ComputeEntries.Add(TEXT("BlurCS"), FIntVector(8, 8, 1));
			Info.ComputeEntries.Add(TEXT("WideCS"), FIntVector(16, 16, 1));
			Info.Functions = { TEXT("MainPS"), TEXT("Helper") };
			References.Shaders.Add(MoveTemp(Info));
		}
		{
			// A compute entry whose group size is spelled with macros: read as a plain function.
			FPipelineShaderInfo Info = MakeShaderInfo(TEXT("Macro.usf"), TEXT("D:/Rules/Passes/Macro.usf"));
			Info.Functions = { TEXT("MacroCS") };
			References.Shaders.Add(MoveTemp(Info));
		}
		References.Shaders.Add(MakeShaderInfo(TEXT("Missing.usf"), TEXT("D:/Rules/Passes/Missing.usf"), /*bExists*/ false));
		{
			// Includes a file the host could not follow: what it read is not all the file defines.
			FPipelineShaderInfo Info = MakeShaderInfo(TEXT("Partial.usf"), TEXT("D:/Rules/Passes/Partial.usf"));
			Info.ComputeEntries.Add(TEXT("KnownCS"), FIntVector(8, 8, 1));
			Info.bEntryScanComplete = false;
			References.Shaders.Add(MoveTemp(Info));
		}
		{
			FPipelineShaderInfo Info = MakeShaderInfo(TEXT("/Project/Passes/Virtual.usf"), TEXT("D:/Rules/Shaders/Passes/Virtual.usf"));
			Info.VirtualPath = TEXT("/Project/Passes/Virtual.usf");
			Info.ComputeEntries.Add(TEXT("VirtCS"), FIntVector(4, 4, 4));
			References.Shaders.Add(MoveTemp(Info));
		}
		return References;
	}

	// =============================================================================================
	// The Rules cases
	// =============================================================================================

	enum class EPipelineRuleExpect : uint8
	{
		/** The code is raised as an error. */
		Raised,
		/** The code is raised as a warning. */
		Warned,
		/** The code is raised as an info. */
		Noted,
		/** No error at all, and the code is not raised at any severity. */
		Clean,
		/** The code is raised as an error on the line holding the case's LineMarker. */
		ErrorOnLine,
	};

	enum class EPipelineRuleReferences : uint8
	{
		/** Engine-free: FBindOptions::PipelineReferences is null (DSH7360). */
		None,
		/** MakeRuleReferences(). */
		Rules,
		/** MakeRuleReferences(false): an engine without the Custom Pass runtime. */
		RulesWithoutCustomPass,
	};

	struct FPipelineRuleCase
	{
		FString Name;
		FString Code;
		EPipelineRuleExpect Expect = EPipelineRuleExpect::Raised;
		EPipelineRuleReferences References = EPipelineRuleReferences::None;
		FString Source;
		FString LineMarker;
	};

	struct FPipelineRuleTable
	{
		TArray<FPipelineRuleCase> Cases;

		void Add(const TCHAR* Name, const TCHAR* Code, const EPipelineRuleExpect Expect, const EPipelineRuleReferences References, const FString& Source)
		{
			FPipelineRuleCase& Case = Cases.AddDefaulted_GetRef();
			Case.Name = Name;
			Case.Code = Code;
			Case.Expect = Expect;
			Case.References = References;
			Case.Source = Lf(Source);
		}

		void AddErrorOnLine(const TCHAR* Name, const TCHAR* Code, const EPipelineRuleReferences References, const FString& Source, const TCHAR* LineMarker)
		{
			Add(Name, Code, EPipelineRuleExpect::ErrorOnLine, References, Source);
			Cases.Last().LineMarker = LineMarker;
		}
	};

	/** Head, then one buffer a clear pass writes: the smallest pipeline a declaration can be tested in. */
	inline FString WithBase(const FString& Head)
	{
		return Head + TEXT("\nbuffer BaseMask : R8;\n\npass BaseFill : clear\n{\n    write BaseMask;\n}\n");
	}

	/** `buffer Mask : R8;` and a mesh pass at AfterOpaque writing it through Output0, with Keys before the write. */
	inline FString MeshWith(const TCHAR* Keys)
	{
		return FString::Printf(TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n%s    write Output0 = Mask;\n}\n"), Keys);
	}

	/** A compute pass reading Mask and writing Soft, with Extra statements after its bindings. */
	inline FString ComputeWith(const TCHAR* Extra)
	{
		return FString::Printf(TEXT("buffer Mask : R8;\nbuffer Soft : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    read Source = Mask;\n    write Result = Soft;\n%s}\n"), Extra);
	}

	/** A copy pass at Injection reading Read and writing Write, with `buffer Mask : R8;` declared for either. */
	inline FString CopyAt(const TCHAR* Injection, const TCHAR* Read, const TCHAR* Write)
	{
		return FString::Printf(TEXT("buffer Mask : R8;\n\npass Move : copy\n{\n    Injection = %s;\n    read %s;\n    write %s;\n}\n"), Injection, Read, Write);
	}

	/**
	 * One pass with Reads buffers read and Writes buffers written, all of its own: `compute` and `fullscreen` with a shader
	 * bind them by name (`read R0 = B0;`); a fullscreen material pass reads them under their own names and writes the
	 * scene colour.
	 */
	inline FString MakeManyBindingsSource(const TCHAR* Kind, const bool bMaterial, const int32 Reads, const int32 Writes)
	{
		FString Text;
		for (int32 Index = 0; Index < Reads; ++Index)
		{
			Text += FString::Printf(TEXT("buffer B%d : R8;\n"), Index);
		}
		for (int32 Index = 0; !bMaterial && Index < Writes; ++Index)
		{
			Text += FString::Printf(TEXT("buffer O%d : R8;\n"), Index);
		}
		Text += FString::Printf(TEXT("\npass Many : %s\n{\n"), Kind);
		Text += bMaterial ? FString(TEXT("    Material = \"PP_Show\";\n")) : FString(TEXT("    Shader = \"Many.usf\";\n    Entry = ManyEntry;\n"));
		for (int32 Index = 0; Index < Reads; ++Index)
		{
			Text += bMaterial ? FString::Printf(TEXT("    read B%d;\n"), Index) : FString::Printf(TEXT("    read R%d = B%d;\n"), Index, Index);
		}
		if (bMaterial)
		{
			Text += TEXT("    write SceneColor;\n");
		}
		for (int32 Index = 0; !bMaterial && Index < Writes; ++Index)
		{
			Text += FString::Printf(TEXT("    write W%d = O%d;\n"), Index, Index);
		}
		Text += TEXT("}\n");
		return Text;
	}

	/** A compute pass with one constant `param` per value, named P0, P1, ... in order. */
	inline FString MakePackingSource(const TArray<FString>& Values)
	{
		FString Text = TEXT("buffer Mask : R8;\n\npass Pack : compute\n{\n    Shader = \"Pack.usf\";\n    Entry = PackCS;\n    write Result = Mask;\n");
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			Text += FString::Printf(TEXT("    param P%d = %s;\n"), Index, *Values[Index]);
		}
		Text += TEXT("}\n");
		return Text;
	}

	inline TArray<FString> RepeatValue(const TCHAR* Value, const int32 Count)
	{
		TArray<FString> Values;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Values.Add(Value);
		}
		return Values;
	}

	/** Five buffers A..E and a fullscreen pass drawing Material, reading Reads of them and writing the scene colour. */
	inline FString FiveInputsSource(const TCHAR* Material, const TCHAR* Reads)
	{
		return FString::Printf(TEXT(
			"buffer A : R8;\nbuffer B : R8;\nbuffer C : R8;\nbuffer D : R8;\nbuffer E : R8;\n\n"
			"pass Composite : fullscreen\n{\n    Material = \"%s\";\n%s    write SceneColor;\n}\n"), Material, Reads);
	}

	inline TArray<FPipelineRuleCase> MakePipelineRuleCases()
	{
		FPipelineRuleTable Table;
		constexpr EPipelineRuleExpect Raised = EPipelineRuleExpect::Raised;
		constexpr EPipelineRuleExpect Warned = EPipelineRuleExpect::Warned;
		constexpr EPipelineRuleExpect Noted = EPipelineRuleExpect::Noted;
		constexpr EPipelineRuleExpect Clean = EPipelineRuleExpect::Clean;
		constexpr EPipelineRuleReferences Free = EPipelineRuleReferences::None;
		constexpr EPipelineRuleReferences Refs = EPipelineRuleReferences::Rules;
		constexpr EPipelineRuleReferences NoPass = EPipelineRuleReferences::RulesWithoutCustomPass;

		// ------------------------------------------------------------------ what a `.dsp` holds (DSH3313-DSH3317, DSH7255)
		Table.Add(TEXT("File.DSH3313.SecondPragma"), TEXT("DSH3313"), Raised, Free, WithBase(TEXT("#pragma pipeline(Order = 1)\n#pragma pipeline(Order = 2)\n")));
		Table.Add(TEXT("File.DSH3313.OnePragma"), TEXT("DSH3313"), Clean, Free, WithBase(TEXT("#pragma pipeline(Order = 1)\n")));
		Table.Add(TEXT("File.DSH3314.MaterialPragma"), TEXT("DSH3314"), Raised, Free, WithBase(TEXT("#pragma material(Domain = PostProcess)\n")));
		Table.Add(TEXT("File.DSH3314.Function"), TEXT("DSH3314"), Raised, Free, WithBase(TEXT("float Half(float X)\n{\n    return X * 0.5;\n}\n")));
		Table.Add(TEXT("File.DSH3314.Struct"), TEXT("DSH3314"), Raised, Free, WithBase(TEXT("struct Pair\n{\n    float A;\n    float B;\n};\n")));
		Table.Add(TEXT("File.DSH3314.Include"), TEXT("DSH3314"), Raised, Free, WithBase(TEXT("#include \"Shared.dsh\"\n")));
		Table.Add(TEXT("File.DSH3315.Region"), TEXT("DSH3315"), Warned, Free, WithBase(TEXT("#pragma region Look\n#pragma endregion\n")));
		Table.Add(TEXT("File.DSH7255.InstancePragma"), TEXT("DSH7255"), Raised, Free, WithBase(TEXT("#pragma instance(Parent = \"/Game/M_Parent\")\n")));
		Table.Add(TEXT("File.DSH3316.LooseVariable"), TEXT("DSH3316"), Raised, Free, WithBase(TEXT("float Loose = 1.0;\n")));
		Table.Add(TEXT("File.DSH3316.Constant"), TEXT("DSH3316"), Clean, Free, WithBase(TEXT("static const float Loose = 1.0;\n")));
		Table.Add(TEXT("File.DSH3317.BufferGroup"), TEXT("DSH3317"), Warned, Free, WithBase(TEXT("/// @group Look\nbuffer Mask : R8;\n")));
		Table.Add(TEXT("File.DSH3317.BufferDesc"), TEXT("DSH3317"), Clean, Free, WithBase(TEXT("/// @desc The mask.\nbuffer Mask : R8;\n")));
		Table.Add(TEXT("File.DSH3317.UniformName"), TEXT("DSH3317"), Warned, Free, WithBase(TEXT("/// @name Other\nuniform float Gain = 1.0;\n")));
		Table.Add(TEXT("File.DSH3317.PragmaDoc"), TEXT("DSH3317"), Warned, Free, WithBase(TEXT("/// @desc The whole pipeline.\n#pragma pipeline(Order = 1)\n")));

		// ------------------------------------------------------------------ parameters and constants (DSH4414, DSH7320)
		Table.Add(TEXT("Parameters.DSH4414.WeightDeclared"), TEXT("DSH4414"), Raised, Free, WithBase(TEXT("uniform float DreamPassWeight = 1.0;\n")));
		// The asset names parameters with FNames, which ignore case: two uniforms that differ in case only would be one.
		Table.Add(TEXT("Parameters.DSH4210.CaseOnly"), TEXT("DSH4210"), Raised, Free, WithBase(TEXT("uniform float Gain = 1.0;\nuniform float gain = 2.0;\n")));
		Table.Add(TEXT("Parameters.DSH4210.ConstantCaseOnly"), TEXT("DSH4210"), Clean, Free, WithBase(TEXT("uniform float Gain = 1.0;\nstatic const float gain = 2.0;\n")));
		Table.Add(TEXT("Parameters.DSH7320.Array"), TEXT("DSH7320"), Raised, Free, WithBase(TEXT("uniform float Pair[2];\n")));
		Table.Add(TEXT("Parameters.DSH7320.IntVector"), TEXT("DSH7320"), Raised, Free, WithBase(TEXT("uniform int2 Pair = int2(1, 2);\n")));
		Table.Add(TEXT("Parameters.DSH7320.TextureInitializer"), TEXT("DSH7320"), Raised, Free, WithBase(TEXT("uniform Texture2D Noise = 1;\n")));
		Table.Add(TEXT("Parameters.DSH7320.TextureConstant"), TEXT("DSH7320"), Raised, Free, WithBase(TEXT("static const Texture2D Noise;\n")));
		Table.Add(TEXT("Parameters.DSH7320.ConstantWithoutValue"), TEXT("DSH7320"), Raised, Free, WithBase(TEXT("static const float Half;\n")));
		Table.Add(TEXT("Parameters.DSH7320.EveryType"), TEXT("DSH7320"), Clean, Free, WithBase(Dsp(TEXT(R"DSP(
uniform float A = 1.0;
uniform float2 B = float2(1.0, 2.0);
uniform float3 C;
uniform float4 D = float4(1.0, 0.5, 0.25, 1.0);
uniform int E = 3;
uniform bool F = true;
uniform Texture2D G;
static const float H = 0.5;
static const int2 I = int2(1, 2);
static const bool J = false;
)DSP"))));

		// ------------------------------------------------------------------ #pragma pipeline (DSH7300-DSH7303, DSH4404, DSH4405, DSH7322)
		Table.Add(TEXT("Pragma.DSH7301.KeyTwice"), TEXT("DSH7301"), Raised, Free, WithBase(TEXT("#pragma pipeline(Order = 1, Order = 2)\n")));
		Table.Add(TEXT("Pragma.DSH7300.UnknownKey"), TEXT("DSH7300"), Raised, Free, WithBase(TEXT("#pragma pipeline(Ordre = 1)\n")));
		Table.Add(TEXT("Pragma.DSH7302.OrderNotWhole"), TEXT("DSH7302"), Raised, Free, WithBase(TEXT("#pragma pipeline(Order = 1.5)\n")));
		Table.Add(TEXT("Pragma.DSH7302.NegativeOrder"), TEXT("DSH7302"), Clean, Free, WithBase(TEXT("#pragma pipeline(Order = -2)\n")));
		Table.Add(TEXT("Pragma.DSH7302.UnknownView"), TEXT("DSH7302"), Raised, Free, WithBase(TEXT("#pragma pipeline(Views = Game | Phone)\n")));
		// A quoted flag list is no flag list: refused, where it was once dropped without a word.
		Table.Add(TEXT("Pragma.DSH7302.QuotedViews"), TEXT("DSH7302"), Raised, Free, WithBase(TEXT("#pragma pipeline(Views = \"Game\")\n")));
		Table.Add(TEXT("Pragma.DSH7302.QuotedRequires"), TEXT("DSH7302"), Raised, Free, WithBase(TEXT("#pragma pipeline(Requires = \"PostProcess\")\n")));
		Table.Add(TEXT("Pragma.DSH7302.QuotedEnabled"), TEXT("DSH7302"), Raised, Free, WithBase(TEXT("#pragma pipeline(Enabled = \"On\")\nuniform bool On = true;\n")));
		Table.Add(TEXT("Pragma.DSH7302.NoViews"), TEXT("DSH7302"), Raised, Free, WithBase(TEXT("#pragma pipeline(Views = \"\")\n")));
		Table.Add(TEXT("Pragma.DSH7303.UnknownInjection"), TEXT("DSH7303"), Raised, Free, WithBase(TEXT("#pragma pipeline(Injection = AfterDOF)\n")));
		Table.Add(TEXT("Pragma.DSH7303.DottedInjection"), TEXT("DSH7303"), Clean, Free, WithBase(TEXT("#pragma pipeline(Injection = PostProcess.AfterDOF)\n")));
		Table.Add(TEXT("Pragma.DSH4404.EnabledUnknown"), TEXT("DSH4404"), Raised, Free, WithBase(TEXT("#pragma pipeline(Enabled = Missing)\n")));
		Table.Add(TEXT("Pragma.DSH4405.EnabledNotBool"), TEXT("DSH4405"), Raised, Free, WithBase(TEXT("#pragma pipeline(Enabled = On)\nuniform float On = 1.0;\n")));
		Table.Add(TEXT("Pragma.DSH7322.EnabledFalse"), TEXT("DSH7322"), Raised, Free, WithBase(TEXT("#pragma pipeline(Enabled = false)\n")));
		Table.Add(TEXT("Pragma.DSH7322.ConstantFalse"), TEXT("DSH7322"), Raised, Free, WithBase(TEXT("#pragma pipeline(Enabled = Never)\nstatic const bool Never = false;\n")));
		Table.Add(TEXT("Pragma.DSH7322.UniformBool"), TEXT("DSH7322"), Clean, Free, WithBase(TEXT("#pragma pipeline(Enabled = On)\nuniform bool On = true;\n")));
		Table.Add(TEXT("Pragma.DSH7322.True"), TEXT("DSH7322"), Clean, Free, WithBase(TEXT("#pragma pipeline(Enabled = true)\n")));

		// ------------------------------------------------------------------ buffers (DSH4400, DSH4402, DSH7305-DSH7309, DSH7321, DSH7355)
		Table.Add(TEXT("Buffers.DSH4402.BuiltinName"), TEXT("DSH4402"), Raised, Free, WithBase(TEXT("buffer SceneColor : R8;\n")));
		Table.Add(TEXT("Buffers.DSH4400.Twice"), TEXT("DSH4400"), Raised, Free, WithBase(TEXT("buffer Mask : R8;\nbuffer Mask : R16F;\n")));
		Table.Add(TEXT("Buffers.DSH4400.NameOfAParameter"), TEXT("DSH4400"), Raised, Free, WithBase(TEXT("uniform float Mask = 1.0;\nbuffer Mask : R8;\n")));
		Table.Add(TEXT("Buffers.DSH4400.CaseOnly"), TEXT("DSH4400"), Raised, Free, WithBase(TEXT("buffer Mask : R8;\nbuffer mask : R8;\n")));
		Table.Add(TEXT("Buffers.DSH7305.UnknownFormat"), TEXT("DSH7305"), Raised, Free, WithBase(TEXT("buffer Mask : R7;\n")));
		Table.Add(TEXT("Buffers.DSH7305.FormatCase"), TEXT("DSH7305"), Raised, Free, WithBase(TEXT("buffer Mask : r8;\n")));
		Table.Add(TEXT("Buffers.DSH7321.R32U"), TEXT("DSH7321"), Raised, Free, TEXT("buffer Ids : R32U;\n\npass Fill : clear\n{\n    write Ids;\n}\n"));
		Table.Add(TEXT("Buffers.DSH7321.RG32U"), TEXT("DSH7321"), Raised, Free, TEXT("buffer Ids : RG32U;\n\npass Fill : clear\n{\n    write Ids;\n}\n"));
		Table.Add(TEXT("Buffers.DSH7321.FloatIds"), TEXT("DSH7321"), Clean, Free, TEXT("buffer Ids : R32F;\n\npass Fill : clear\n{\n    write Ids;\n}\n"));
		Table.Add(TEXT("Buffers.DSH7307.KeyTwice"), TEXT("DSH7307"), Raised, Free, WithBase(TEXT("buffer Mask : R8(Mips = 2, Mips = 3);\n")));
		Table.Add(TEXT("Buffers.DSH7306.UnknownKey"), TEXT("DSH7306"), Raised, Free, WithBase(TEXT("buffer Mask : R8(Sclae = 0.5);\n")));
		Table.Add(TEXT("Buffers.DSH7308.ScaleRange"), TEXT("DSH7308"), Raised, Free, WithBase(TEXT("buffer Mask : R8(Scale = 8.0);\n")));
		Table.Add(TEXT("Buffers.DSH7308.ScaleBounds"), TEXT("DSH7308"), Clean, Free, WithBase(TEXT("buffer Big : R8(Scale = 4.0);\nbuffer Small : R8(Scale = 0.0625);\n")));
		Table.Add(TEXT("Buffers.DSH7308.SizeRange"), TEXT("DSH7308"), Raised, Free, WithBase(TEXT("buffer Mask : R8(Size = int2(0, 64));\n")));
		Table.Add(TEXT("Buffers.DSH7308.SizeNotWhole"), TEXT("DSH7308"), Raised, Free, WithBase(TEXT("buffer Mask : R8(Size = float2(1.5, 2.0));\n")));
		Table.Add(TEXT("Buffers.DSH7308.ResolutionFixed"), TEXT("DSH7308"), Raised, Free, WithBase(TEXT("buffer Mask : R8(Resolution = Fixed);\n")));
		Table.Add(TEXT("Buffers.DSH7308.MipsRange"), TEXT("DSH7308"), Raised, Free, WithBase(TEXT("buffer Mask : R8(Mips = 15);\n")));
		Table.Add(TEXT("Buffers.DSH7308.MipsNotWhole"), TEXT("DSH7308"), Raised, Free, WithBase(TEXT("buffer Mask : R8(Mips = 2.0);\n")));
		Table.Add(TEXT("Buffers.DSH7308.HistoryNotBool"), TEXT("DSH7308"), Raised, Free, WithBase(TEXT("buffer Mask : R8(History = 1);\n")));
		Table.Add(TEXT("Buffers.DSH7308.ClearBool"), TEXT("DSH7308"), Raised, Free, WithBase(TEXT("buffer Mask : R8(Clear = true);\n")));
		Table.Add(TEXT("Buffers.DSH7308.EveryKey"), TEXT("DSH7308"), Clean, Free, WithBase(TEXT("buffer Mask : RGBA16F(Scale = 0.5, Resolution = Output, Clear = float4(1.0, 0.0, 0.0, 1.0), Mips = 4, History = true, Export = true);\n")));
		Table.Add(TEXT("Buffers.DSH7309.SizeAndScale"), TEXT("DSH7309"), Raised, Free, WithBase(TEXT("buffer Mask : R8(Size = int2(64, 64), Scale = 0.5);\n")));
		Table.Add(TEXT("Buffers.DSH7309.SizeAndResolution"), TEXT("DSH7309"), Raised, Free, WithBase(TEXT("buffer Mask : R8(Size = int2(64, 64), Resolution = Render);\n")));
		Table.Add(TEXT("Buffers.DSH7355.ExportDepth"), TEXT("DSH7355"), Raised, Free, WithBase(TEXT("buffer Depth : Depth32(Export = true);\n")));
		Table.Add(TEXT("Buffers.DSH7355.ExportFloat"), TEXT("DSH7355"), Clean, Free, WithBase(TEXT("buffer Glow : RGBA16F(Export = true);\n")));

		// ------------------------------------------------------------------ pass keys and values (DSH4401, DSH7304, DSH7310-DSH7313 and the references)
		Table.Add(TEXT("Passes.DSH4401.Twice"), TEXT("DSH4401"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    write Mask;\n}\n\npass Fill : clear\n{\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH4401.CaseOnly"), TEXT("DSH4401"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    write Mask;\n}\n\npass fill : clear\n{\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH7304.KindCase"), TEXT("DSH7304"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Fill : Clear\n{\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH7304.UnknownKind"), TEXT("DSH7304"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Fill : blit\n{\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH7312.KeyTwice"), TEXT("DSH7312"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    Value = 1.0;\n    Value = 0.5;\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH7311.KeyOfAnotherKind"), TEXT("DSH7311"), Raised, Free, ComputeWith(TEXT("    Filter = Stencil(1);\n")));
		Table.Add(TEXT("Passes.DSH7310.UnknownKey"), TEXT("DSH7310"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    Injecton = AfterOpaque;\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH7313.MaterialNotString"), TEXT("DSH7313"), Raised, Free, TEXT("pass Show : fullscreen\n{\n    Material = PP_Show;\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("Passes.DSH7313.ShaderNotString"), TEXT("DSH7313"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = Blur;\n    Entry = BlurCS;\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH7313.EntryNotName"), TEXT("DSH7313"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = 3;\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH7313.ThreadsZero"), TEXT("DSH7313"), Raised, Free, ComputeWith(TEXT("    Threads = uint3(0, 8, 1);\n")));
		Table.Add(TEXT("Passes.DSH7313.ThreadsTooMany"), TEXT("DSH7313"), Raised, Free, ComputeWith(TEXT("    Threads = uint3(64, 64, 1);\n")));
		Table.Add(TEXT("Passes.DSH7313.DispatchFactorZero"), TEXT("DSH7313"), Raised, Free, ComputeWith(TEXT("    Dispatch = Soft * 0.0;\n")));
		Table.Add(TEXT("Passes.DSH7313.DispatchFixedZero"), TEXT("DSH7313"), Raised, Free, ComputeWith(TEXT("    Dispatch = uint3(0, 1, 1);\n")));
		Table.Add(TEXT("Passes.DSH7313.InjectionString"), TEXT("DSH7313"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    Injection = \"AfterOpaque\";\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH7303.InjectionCase"), TEXT("DSH7303"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    Injection = AfterDof;\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH7313.EnabledNumber"), TEXT("DSH7313"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    Enabled = 1;\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH4404.EnabledUnknown"), TEXT("DSH4404"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    Enabled = Nope;\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH4405.EnabledNotBool"), TEXT("DSH4405"), Raised, Free, TEXT("uniform float Gain = 1.0;\nbuffer Mask : R8;\n\npass Fill : clear\n{\n    Enabled = Gain;\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH4407.ShaderExtension"), TEXT("DSH4407"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.hlsl\";\n    Entry = BlurCS;\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH4407.HeaderShader"), TEXT("DSH4407"), Clean, Free, TEXT("buffer Mask : R8;\nbuffer Soft : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.ush\";\n    Entry = BlurCS;\n    read Source = Mask;\n    write Result = Soft;\n}\n"));
		Table.Add(TEXT("Passes.DSH4403.UnknownWrite"), TEXT("DSH4403"), Raised, Free, TEXT("pass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    write Result = Nope;\n}\n"));
		Table.Add(TEXT("Passes.DSH4403.UnknownDispatch"), TEXT("DSH4403"), Raised, Free, ComputeWith(TEXT("    Dispatch = Nope;\n")));
		Table.Add(TEXT("Passes.DSH7318.WritePrevious"), TEXT("DSH7318"), Raised, Free, TEXT("buffer Field : R8(History = true);\n\npass Keep : copy\n{\n    read SceneColor;\n    write Field.Previous;\n}\n"));
		Table.Add(TEXT("Passes.DSH4415.BuiltinPrevious"), TEXT("DSH4415"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Keep : copy\n{\n    read SceneColor.Previous;\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH4415.WithoutHistory"), TEXT("DSH4415"), Raised, Free, TEXT("buffer Field : R8;\nbuffer Mask : R8;\n\npass Keep : copy\n{\n    read Field.Previous;\n    write Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH4415.WithHistory"), TEXT("DSH4415"), Clean, Free, Dsp(TEXT(R"DSP(
buffer Field : R8(History = true);
buffer Mask : R8;

pass Fill : clear
{
    write Field;
}

pass Keep : copy
{
    read Field.Previous;
    write Mask;
}
)DSP")));
		Table.Add(TEXT("Passes.DSH7330.ReadDepth"), TEXT("DSH7330"), Raised, Free, TEXT("buffer ObjDepth : Depth32;\nbuffer Mask : R8;\n\npass Keep : copy\n{\n    read ObjDepth;\n    write Mask;\n}\n"));
		// A clear of a Depth32 buffer resets an own depth before a mesh pass tests against it: no read or write of it by name.
		Table.Add(TEXT("Passes.DSH7330.ClearDepth"), TEXT("DSH7330"), Clean, Free, Dsp(TEXT(R"DSP(
buffer ObjDepth : Depth32;
buffer Mask : R8;

pass Reset : clear
{
    Injection = AfterOpaque;
    write ObjDepth;
}

pass Draw : mesh
{
    Injection = AfterOpaque;
    Filter = Stencil(1);
    Depth = Own(ObjDepth);
    write Output0 = Mask;
}
)DSP")));
		Table.Add(TEXT("Passes.DSH7330.OwnNotDepth"), TEXT("DSH7330"), Raised, Free, TEXT("buffer Mask : R8;\nbuffer Soft : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    Depth = Own(Mask);\n    write Output0 = Soft;\n}\n"));
		Table.Add(TEXT("Passes.DSH7330.OwnBuiltin"), TEXT("DSH7330"), Raised, Free, MeshWith(TEXT("    Filter = Stencil(1);\n    Depth = Own(SceneDepth);\n")));
		Table.Add(TEXT("Passes.DSH7330.OwnDepth"), TEXT("DSH7330"), Clean, Free, TEXT("buffer ObjDepth : Depth32;\nbuffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    Depth = Own(ObjDepth);\n    write Output0 = Mask;\n}\n"));
		Table.Add(TEXT("Passes.DSH7313.OwnWithoutBuffer"), TEXT("DSH7313"), Raised, Free, MeshWith(TEXT("    Filter = Stencil(1);\n    Depth = Own();\n")));

		// ------------------------------------------------------------------ filters (DSH7314)
		Table.Add(TEXT("Filters.DSH7314.Operator"), TEXT("DSH7314"), Raised, Free, MeshWith(TEXT("    Filter = Stencil(1) + Stencil(2);\n")));
		Table.Add(TEXT("Filters.DSH7314.StencilRange"), TEXT("DSH7314"), Raised, Free, MeshWith(TEXT("    Filter = Stencil(300);\n")));
		Table.Add(TEXT("Filters.DSH7314.StencilArity"), TEXT("DSH7314"), Raised, Free, MeshWith(TEXT("    Filter = Stencil();\n")));
		Table.Add(TEXT("Filters.DSH7314.ListOfTwo"), TEXT("DSH7314"), Raised, Free, MeshWith(TEXT("    Filter = List(A | B);\n")));
		Table.Add(TEXT("Filters.DSH7314.UnknownTerm"), TEXT("DSH7314"), Raised, Free, MeshWith(TEXT("    Filter = Tag(Enemies);\n")));
		Table.Add(TEXT("Filters.DSH7314.NamedArgument"), TEXT("DSH7314"), Raised, Free, MeshWith(TEXT("    Filter = Stencil(Value = 1);\n")));
		Table.Add(TEXT("Filters.DSH7314.NotACall"), TEXT("DSH7314"), Raised, Free, MeshWith(TEXT("    Filter = Highlight;\n")));
		Table.Add(TEXT("Filters.DSH7314.NormalForm"), TEXT("DSH7314"), Clean, Free, MeshWith(TEXT("    Filter = Layer(A | B) & Stencil(1, 0xFF) | List(Enemies);\n")));

		// ------------------------------------------------------------------ mesh keys (DSH7313)
		Table.Add(TEXT("Mesh.DSH7313.UsageUnknown"), TEXT("DSH7313"), Raised, Free, MeshWith(TEXT("    Filter = Stencil(1);\n    Usage = StaticMesh | Splines;\n")));
		Table.Add(TEXT("Mesh.DSH7313.UsageString"), TEXT("DSH7313"), Raised, Free, MeshWith(TEXT("    Filter = Stencil(1);\n    Usage = \"StaticMesh\";\n")));
		Table.Add(TEXT("Mesh.DSH7313.AssignStencilRange"), TEXT("DSH7313"), Raised, Free, MeshWith(TEXT("    Filter = Layer(Highlight);\n    Nanite = AssignStencil(0);\n")));
		Table.Add(TEXT("Mesh.DSH7313.StencilMaskWithoutStencil"), TEXT("DSH7313"), Raised, Free, MeshWith(TEXT("    Filter = Layer(Highlight);\n    Nanite = StencilMask;\n")));
		Table.Add(TEXT("Mesh.DSH7313.StencilMaskWithStencil"), TEXT("DSH7313"), Clean, Free, MeshWith(TEXT("    Filter = Stencil(2);\n    Nanite = StencilMask;\n")));
		Table.Add(TEXT("Mesh.DSH7313.ModeUnknown"), TEXT("DSH7313"), Raised, Free, MeshWith(TEXT("    Filter = Stencil(1);\n    Mode = Overide;\n")));
		Table.Add(TEXT("Mesh.DSH7313.CullUnknown"), TEXT("DSH7313"), Raised, Free, MeshWith(TEXT("    Filter = Stencil(1);\n    Cull = Twice;\n")));
		Table.Add(TEXT("Mesh.DSH7313.BlendUnknown"), TEXT("DSH7313"), Raised, Free, MeshWith(TEXT("    Filter = Stencil(1);\n    Blend = Multiply;\n")));
		Table.Add(TEXT("Mesh.DSH7313.DepthUnknown"), TEXT("DSH7313"), Raised, Free, MeshWith(TEXT("    Filter = Stencil(1);\n    Depth = Always;\n")));
		Table.Add(TEXT("Mesh.DSH7313.EveryKey"), TEXT("DSH7313"), Clean, Free, Dsp(TEXT(R"DSP(
buffer Mask : R8;
buffer ObjDepth : Depth32;

pass Draw : mesh
{
    Injection = AfterOpaque;
    Filter = Stencil(4, 0x0F) | List(Enemies);
    Material = "M_Mask";
    Mode = OwnOrOverride;
    Depth = Own(ObjDepth);
    Cull = Front;
    Blend = AlphaBlend;
    Usage = StaticMesh | SplineMesh;
    Nanite = AssignStencil(200);
    NaniteValue = float4(1.0, 0.5, 0.0, 1.0);
    write Output0 = Mask;
}
)DSP")));

		// ------------------------------------------------------------------ V6: what a fullscreen pass is (DSH7315-DSH7317, DSH7336, DSH7346)
		Table.Add(TEXT("Fullscreen.DSH7316.MaterialAndShader"), TEXT("DSH7316"), Raised, Free, TEXT("pass Show : fullscreen\n{\n    Material = \"PP_Show\";\n    Shader = \"Show.usf\";\n    Entry = ShowPS;\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("Fullscreen.DSH7315.Nothing"), TEXT("DSH7315"), Raised, Free, TEXT("pass Show : fullscreen\n{\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("Fullscreen.DSH7315.ShaderWithoutEntry"), TEXT("DSH7315"), Raised, Free, TEXT("pass Show : fullscreen\n{\n    Shader = \"Show.usf\";\n    write Out = SceneColor;\n}\n"));
		Table.Add(TEXT("Fullscreen.DSH7316.EntryWithMaterial"), TEXT("DSH7316"), Raised, Free, TEXT("pass Show : fullscreen\n{\n    Material = \"PP_Show\";\n    Entry = ShowPS;\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("Fullscreen.DSH7317.MaterialWritesTwo"), TEXT("DSH7317"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Show : fullscreen\n{\n    Material = \"PP_Show\";\n    write SceneColor;\n    write Mask;\n}\n"));
		Table.Add(TEXT("Fullscreen.DSH7317.MaterialWriteNamed"), TEXT("DSH7317"), Raised, Free, TEXT("pass Show : fullscreen\n{\n    Material = \"PP_Show\";\n    write Out = SceneColor;\n}\n"));
		Table.Add(TEXT("Fullscreen.DSH7317.ShaderWritesNothing"), TEXT("DSH7317"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Show : fullscreen\n{\n    Shader = \"Show.usf\";\n    Entry = ShowPS;\n    read In = Mask;\n}\n"));
		Table.Add(TEXT("Fullscreen.DSH7336.MaterialSixReads"), TEXT("DSH7336"), Raised, Free, MakeManyBindingsSource(TEXT("fullscreen"), true, 6, 0));
		Table.Add(TEXT("Fullscreen.DSH7336.MaterialFiveReads"), TEXT("DSH7336"), Clean, Free, MakeManyBindingsSource(TEXT("fullscreen"), true, 5, 0));
		Table.Add(TEXT("Fullscreen.DSH7346.ShaderNineReads"), TEXT("DSH7346"), Raised, Free, MakeManyBindingsSource(TEXT("fullscreen"), false, 9, 1));
		// A `.usf` with one output and eight inputs runs in the pixel slot like any other: no wrapper material, no five-slot limit.
		Table.Add(TEXT("Fullscreen.DSH7346.ShaderEightReadsOneWrite"), TEXT("DSH7346"), Clean, Free, MakeManyBindingsSource(TEXT("fullscreen"), false, 8, 1));
		Table.Add(TEXT("Fullscreen.DSH7346.ShaderFiveWrites"), TEXT("DSH7346"), Raised, Free, MakeManyBindingsSource(TEXT("fullscreen"), false, 1, 5));
		Table.Add(TEXT("Fullscreen.DSH7346.ShaderFourWrites"), TEXT("DSH7346"), Clean, Free, MakeManyBindingsSource(TEXT("fullscreen"), false, 1, 4));

		// ------------------------------------------------------------------ V8: what a compute pass is (DSH7315, DSH7317, DSH7346)
		Table.Add(TEXT("Compute.DSH7315.NoShader"), TEXT("DSH7315"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Entry = BlurCS;\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("Compute.DSH7315.NoEntry"), TEXT("DSH7315"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("Compute.DSH7317.WritesNothing"), TEXT("DSH7317"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    read Source = Mask;\n}\n"));
		Table.Add(TEXT("Compute.DSH7346.NineReads"), TEXT("DSH7346"), Raised, Free, MakeManyBindingsSource(TEXT("compute"), false, 9, 1));
		Table.Add(TEXT("Compute.DSH7346.EightReads"), TEXT("DSH7346"), Clean, Free, MakeManyBindingsSource(TEXT("compute"), false, 8, 1));
		Table.Add(TEXT("Compute.DSH7346.FiveWrites"), TEXT("DSH7346"), Raised, Free, MakeManyBindingsSource(TEXT("compute"), false, 1, 5));
		Table.Add(TEXT("Compute.DSH7346.FourWrites"), TEXT("DSH7346"), Clean, Free, MakeManyBindingsSource(TEXT("compute"), false, 1, 4));

		// ------------------------------------------------------------------ V7: what a mesh pass is (DSH7315, DSH7317)
		Table.Add(TEXT("MeshShape.DSH7315.NoFilter"), TEXT("DSH7315"), Raised, Free, MeshWith(TEXT("")));
		Table.Add(TEXT("MeshShape.DSH7315.OverrideWithoutMaterial"), TEXT("DSH7315"), Raised, Free, MeshWith(TEXT("    Filter = Stencil(1);\n    Mode = Override;\n")));
		Table.Add(TEXT("MeshShape.DSH7317.Reads"), TEXT("DSH7317"), Raised, Free, TEXT("buffer Mask : R8;\nbuffer Soft : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    read Mask;\n    write Output0 = Soft;\n}\n"));
		Table.Add(TEXT("MeshShape.DSH7317.WritesNothing"), TEXT("DSH7317"), Raised, Free, TEXT("pass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n}\n"));
		Table.Add(TEXT("MeshShape.DSH7317.FiveWrites"), TEXT("DSH7317"), Raised, Free, Dsp(TEXT(R"DSP(
buffer A : R8;
buffer B : R8;
buffer C : R8;
buffer D : R8;
buffer E : R8;

pass Draw : mesh
{
    Injection = AfterOpaque;
    Filter = Stencil(1);
    write Output0 = A;
    write Output1 = B;
    write Output2 = C;
    write Output3 = D;
    write Output4 = E;
}
)DSP")));
		Table.Add(TEXT("MeshShape.DSH7317.UnnamedWrite"), TEXT("DSH7317"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    write Mask;\n}\n"));
		Table.Add(TEXT("MeshShape.DSH7317.NotAnOutput"), TEXT("DSH7317"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    write Color = Mask;\n}\n"));
		Table.Add(TEXT("MeshShape.DSH7317.IntegerTarget"), TEXT("DSH7317"), Raised, Free, TEXT("buffer Ids : R32U;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    write Output0 = Ids;\n}\n"));
		Table.Add(TEXT("MeshShape.DSH7317.TwoOutputs"), TEXT("DSH7317"), Clean, Free, TEXT("buffer Mask : R8;\nbuffer Soft : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    write Output0 = Mask;\n    write Output1 = Soft;\n}\n"));

		// ------------------------------------------------------------------ params nothing receives (DSH7359, a warning)
		Table.Add(TEXT("Unused.DSH7359.OwnWithMaterial"), TEXT("DSH7359"), Warned, Free, MeshWith(TEXT("    Filter = Stencil(1);\n    Material = \"M_Mask\";\n    Mode = Own;\n")));
		Table.Add(TEXT("Unused.DSH7359.ClearParam"), TEXT("DSH7359"), Warned, Free, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    write Mask;\n    param P = 1.0;\n}\n"));
		Table.Add(TEXT("Unused.DSH7359.CopyParam"), TEXT("DSH7359"), Warned, Free, TEXT("buffer Mask : R8;\n\npass Keep : copy\n{\n    read SceneColor;\n    write Mask;\n    param P = 1.0;\n}\n"));
		Table.Add(TEXT("Unused.DSH7359.OwnParams"), TEXT("DSH7359"), Warned, Free, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    write Output0 = Mask;\n    param P = 1.0;\n}\n"));
		Table.Add(TEXT("Unused.DSH7359.OverrideParams"), TEXT("DSH7359"), Clean, Free, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    Material = \"M_Mask\";\n    write Output0 = Mask;\n    param P = 1.0;\n}\n"));

		// ------------------------------------------------------------------ names inside a pass (DSH7319)
		Table.Add(TEXT("Names.DSH7319.ReadTwice"), TEXT("DSH7319"), Raised, Free, TEXT("buffer Mask : R8;\nbuffer Soft : R8;\nbuffer Data : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    read A = Mask;\n    read A = Soft;\n    write Result = Data;\n}\n"));
		Table.Add(TEXT("Names.DSH7319.ReadAndWriteShareAName"), TEXT("DSH7319"), Raised, Free, TEXT("buffer Mask : R8;\nbuffer Soft : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    read X = Mask;\n    write X = Soft;\n}\n"));
		Table.Add(TEXT("Names.DSH7319.SizeOfARead"), TEXT("DSH7319"), Raised, Free, TEXT("buffer Mask : R8;\nbuffer Soft : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    read Source = Mask;\n    write SourceSize = Soft;\n}\n"));
		Table.Add(TEXT("Names.DSH7319.SlotParameterPrefix"), TEXT("DSH7319"), Raised, Free, ComputeWith(TEXT("    param DP_Gain = 1.0;\n")));
		Table.Add(TEXT("Names.DSH7319.Entry"), TEXT("DSH7319"), Raised, Free, ComputeWith(TEXT("    param BlurCS = 1.0;\n")));
		Table.Add(TEXT("Names.DSH7319.SlotEntry"), TEXT("DSH7319"), Raised, Free, TEXT("buffer Mask : R8;\nbuffer Soft : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    read Source = Mask;\n    write DreamPassMainCS = Soft;\n}\n"));
		Table.Add(TEXT("Names.DSH7319.ViewAtBeginView"), TEXT("DSH7319"), Raised, Free, TEXT("buffer Mask : R8;\nbuffer Soft : R8;\n\npass Blur : compute\n{\n    Injection = BeginView;\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    read Source = Mask;\n    write Result = Soft;\n    param View = 1.0;\n}\n"));
		Table.Add(TEXT("Names.DSH7319.ViewElsewhere"), TEXT("DSH7319"), Clean, Free, TEXT("buffer Mask : R8;\nbuffer Soft : R8;\n\npass Blur : compute\n{\n    Injection = AfterOpaque;\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    read Source = Mask;\n    write Result = Soft;\n    param View = 1.0;\n}\n"));
		// A material pass's reads, write and params are three kinds of name of their own (they are no `#define`s).
		Table.Add(TEXT("Names.DSH7319.MaterialKeepsKindsApart"), TEXT("DSH7319"), Clean, Free, TEXT("buffer Mask : R8;\n\npass Show : fullscreen\n{\n    Material = \"PP_Show\";\n    read Mask;\n    write SceneColor;\n    param Mask = 1.0;\n}\n"));
		Table.Add(TEXT("Names.DSH7319.ParamTwice"), TEXT("DSH7319"), Raised, Free, TEXT("pass Show : fullscreen\n{\n    Material = \"PP_Show\";\n    write SceneColor;\n    param P = 1.0;\n    param P = 2.0;\n}\n"));

		// ------------------------------------------------------------------ V8: the slot's parameter block (DSH7347)
		Table.Add(TEXT("Slot.DSH7347.TextureInCompute"), TEXT("DSH7347"), Raised, Free, TEXT("uniform Texture2D Noise;\nbuffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    write Result = Mask;\n    param Tex = Noise;\n}\n"));
		Table.Add(TEXT("Slot.DSH7347.TextureInMaterial"), TEXT("DSH7347"), Clean, Free, TEXT("uniform Texture2D Noise;\n\npass Show : fullscreen\n{\n    Material = \"PP_Show\";\n    write SceneColor;\n    param Tex = Noise;\n}\n"));
		Table.Add(TEXT("Slot.DSH7347.SeventeenVectors"), TEXT("DSH7347"), Raised, Free, MakePackingSource(RepeatValue(TEXT("float4(1.0, 2.0, 3.0, 4.0)"), 17)));
		Table.Add(TEXT("Slot.DSH7347.SixteenVectors"), TEXT("DSH7347"), Clean, Free, MakePackingSource(RepeatValue(TEXT("float4(1.0, 2.0, 3.0, 4.0)"), 16)));
		{
			// Sixteen float3 leave a w each: a scalar still fits there, a float2 (xy or zw) nowhere.
			TArray<FString> Float3ThenFloat2 = RepeatValue(TEXT("float3(1.0, 2.0, 3.0)"), 16);
			Float3ThenFloat2.Add(TEXT("float2(1.0, 2.0)"));
			Table.Add(TEXT("Slot.DSH7347.PackingLeavesNoHalf"), TEXT("DSH7347"), Raised, Free, MakePackingSource(Float3ThenFloat2));
			TArray<FString> Float3ThenFloat = RepeatValue(TEXT("float3(1.0, 2.0, 3.0)"), 16);
			Float3ThenFloat.Add(TEXT("1.0"));
			Table.Add(TEXT("Slot.DSH7347.PackingFillsAW"), TEXT("DSH7347"), Clean, Free, MakePackingSource(Float3ThenFloat));
		}

		// ------------------------------------------------------------------ V11: `param` values (DSH4404, DSH7352, DSH7353)
		{
			const auto ParamCase = [](const TCHAR* Value)
			{
				return FString::Printf(TEXT(
					"uniform float Width = 2.0;\nuniform bool On = true;\nstatic const float Half = 0.5;\nbuffer Mask : R8;\n\n"
					"pass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    write Result = Mask;\n    param P = %s;\n}\n"), Value);
			};
			Table.Add(TEXT("Params.DSH7352.Product"), TEXT("DSH7352"), Raised, Free, ParamCase(TEXT("Width * Width")));
			Table.Add(TEXT("Params.DSH7352.Call"), TEXT("DSH7352"), Raised, Free, ParamCase(TEXT("sin(Width)")));
			Table.Add(TEXT("Params.DSH7352.Reciprocal"), TEXT("DSH7352"), Raised, Free, ParamCase(TEXT("2.0 / Width")));
			Table.Add(TEXT("Params.DSH7352.Conditional"), TEXT("DSH7352"), Raised, Free, ParamCase(TEXT("Width > 1.0 ? 1.0 : 0.0")));
			Table.Add(TEXT("Params.DSH7353.VectorScale"), TEXT("DSH7353"), Raised, Free, ParamCase(TEXT("Width * float2(1.0, 2.0)")));
			Table.Add(TEXT("Params.DSH7353.ScaledBool"), TEXT("DSH7353"), Raised, Free, ParamCase(TEXT("On * 2.0")));
			Table.Add(TEXT("Params.DSH4404.UnknownName"), TEXT("DSH4404"), Raised, Free, ParamCase(TEXT("Widht * 2.0")));
			Table.Add(TEXT("Params.DSH7352.Affine"), TEXT("DSH7352"), Clean, Free, ParamCase(TEXT("(Width + 1.0) * 0.5")));
			Table.Add(TEXT("Params.DSH7352.NegatedWeight"), TEXT("DSH7352"), Clean, Free, ParamCase(TEXT("-DreamPassWeight")));
			Table.Add(TEXT("Params.DSH7352.FoldedConstant"), TEXT("DSH7352"), Clean, Free, ParamCase(TEXT("Half * 4.0")));
			Table.Add(TEXT("Params.DSH7353.PlainBool"), TEXT("DSH7353"), Clean, Free, ParamCase(TEXT("On")));
		}

		// ------------------------------------------------------------------ V2: which pass runs where (DSH7325, DSH7326)
		Table.Add(TEXT("Injection.DSH7325.MaterialBeforeBasePass"), TEXT("DSH7325"), Raised, Free, TEXT("pass Show : fullscreen\n{\n    Injection = BeforeBasePass;\n    Material = \"PP_Show\";\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("Injection.DSH7325.MaterialAtBeginView"), TEXT("DSH7325"), Raised, Free, TEXT("pass Show : fullscreen\n{\n    Injection = BeginView;\n    Material = \"PP_Show\";\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("Injection.DSH7325.MeshAtBeginView"), TEXT("DSH7325"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = BeginView;\n    Filter = Stencil(1);\n    write Output0 = Mask;\n}\n"));
		Table.Add(TEXT("Injection.DSH7325.MeshAtEndOfView"), TEXT("DSH7325"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = EndOfView;\n    Filter = Stencil(1);\n    write Output0 = Mask;\n}\n"));
		Table.Add(TEXT("Injection.DSH7326.MaterialAfterOpaque"), TEXT("DSH7326"), Noted, Free, TEXT("pass Show : fullscreen\n{\n    Injection = AfterOpaque;\n    Material = \"PP_Show\";\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("Injection.DSH7326.MaterialBeforePostProcess"), TEXT("DSH7326"), Clean, Free, TEXT("pass Show : fullscreen\n{\n    Injection = BeforePostProcess;\n    Material = \"PP_Show\";\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("Injection.DSH7326.ShaderAtBeginView"), TEXT("DSH7326"), Noted, Free, TEXT("buffer Mask : R8;\n\npass Show : fullscreen\n{\n    Injection = BeginView;\n    Shader = \"Show.usf\";\n    Entry = ShowPS;\n    write Out = Mask;\n}\n"));
		Table.Add(TEXT("Injection.DSH7326.ShaderBeforeBasePass"), TEXT("DSH7326"), Noted, Free, TEXT("buffer Mask : R8;\n\npass Show : fullscreen\n{\n    Injection = BeforeBasePass;\n    Shader = \"Show.usf\";\n    Entry = ShowPS;\n    write Out = Mask;\n}\n"));
		Table.Add(TEXT("Injection.DSH7326.MeshBeforeBasePassTestingScene"), TEXT("DSH7326"), Noted, Free, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = BeforeBasePass;\n    Filter = Stencil(1);\n    write Output0 = Mask;\n}\n"));
		Table.Add(TEXT("Injection.DSH7326.MeshBeforeBasePassWithoutDepth"), TEXT("DSH7326"), Clean, Free, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = BeforeBasePass;\n    Filter = Stencil(1);\n    Depth = None;\n    write Output0 = Mask;\n}\n"));
		Table.Add(TEXT("Injection.DSH7326.MeshAfterUpscale"), TEXT("DSH7326"), Noted, Free, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = PostProcess.AfterTonemap;\n    Filter = Stencil(1);\n    write Output0 = Mask;\n}\n"));
		Table.Add(TEXT("Injection.DSH7326.SceneColorAfterBasePass"), TEXT("DSH7326"), Noted, Free, CopyAt(TEXT("AfterBasePass"), TEXT("SceneColor"), TEXT("Mask")));
		// The view uniform buffer does not exist yet at BeginView, for a compute slot as for a pixel one.
		Table.Add(TEXT("Injection.DSH7326.ComputeAtBeginView"), TEXT("DSH7326"), Noted, Free, TEXT("buffer Mask : R8;\n\npass Seed : compute\n{\n    Injection = BeginView;\n    Shader = \"Seed.usf\";\n    Entry = SeedCS;\n    write Result = Mask;\n}\n"));

		// ------------------------------------------------------------------ V3: where the built-in textures exist (DSH7327, DSH7329)
		Table.Add(TEXT("Builtins.DSH7327.SceneColorBeforeBasePass"), TEXT("DSH7327"), Raised, Free, CopyAt(TEXT("BeforeBasePass"), TEXT("SceneColor"), TEXT("Mask")));
		Table.Add(TEXT("Builtins.DSH7327.SceneColorAfterBasePass"), TEXT("DSH7327"), Clean, Free, CopyAt(TEXT("AfterBasePass"), TEXT("SceneColor"), TEXT("Mask")));
		Table.Add(TEXT("Builtins.DSH7327.SceneDepthAtBeginView"), TEXT("DSH7327"), Raised, Free, CopyAt(TEXT("BeginView"), TEXT("SceneDepth"), TEXT("Mask")));
		Table.Add(TEXT("Builtins.DSH7327.SceneDepthBeforeBasePass"), TEXT("DSH7327"), Clean, Free, CopyAt(TEXT("BeforeBasePass"), TEXT("SceneDepth"), TEXT("Mask")));
		Table.Add(TEXT("Builtins.DSH7327.CustomStencilRead"), TEXT("DSH7327"), Raised, Free, CopyAt(TEXT("BeforePostProcess"), TEXT("CustomStencil"), TEXT("Mask")));
		Table.Add(TEXT("Builtins.DSH7327.CustomStencilWrite"), TEXT("DSH7327"), Raised, Free, CopyAt(TEXT("BeforePostProcess"), TEXT("SceneColor"), TEXT("CustomStencil")));
		Table.Add(TEXT("Builtins.DSH7327.TranslucencyAfterOpaque"), TEXT("DSH7327"), Raised, Free, CopyAt(TEXT("AfterOpaque"), TEXT("Translucency"), TEXT("Mask")));
		Table.Add(TEXT("Builtins.DSH7327.TranslucencyOnTheChain"), TEXT("DSH7327"), Clean, Free, CopyAt(TEXT("PostProcess.AfterDOF"), TEXT("Translucency"), TEXT("Mask")));
		Table.Add(TEXT("Builtins.DSH7327.CustomDepthBeforeBasePass"), TEXT("DSH7327"), Raised, Free, CopyAt(TEXT("BeforeBasePass"), TEXT("CustomDepth"), TEXT("Mask")));
		Table.Add(TEXT("Builtins.DSH7329.CustomDepthAfterBasePass"), TEXT("DSH7329"), Noted, Free, CopyAt(TEXT("AfterBasePass"), TEXT("CustomDepth"), TEXT("Mask")));
		Table.Add(TEXT("Builtins.DSH7329.CustomDepthAfterOpaque"), TEXT("DSH7329"), Clean, Free, CopyAt(TEXT("AfterOpaque"), TEXT("CustomDepth"), TEXT("Mask")));

		// ------------------------------------------------------------------ V3: which built-in textures a pass may write (DSH7328)
		Table.Add(TEXT("Writes.DSH7328.SceneColorAtTranslucencyAfterDOF"), TEXT("DSH7328"), Raised, Free, CopyAt(TEXT("PostProcess.TranslucencyAfterDOF"), TEXT("SceneDepth"), TEXT("SceneColor")));
		Table.Add(TEXT("Writes.DSH7328.TranslucencyAfterDOF"), TEXT("DSH7328"), Raised, Free, CopyAt(TEXT("PostProcess.AfterDOF"), TEXT("SceneDepth"), TEXT("Translucency")));
		Table.Add(TEXT("Writes.DSH7328.TranslucencyAtItsPoint"), TEXT("DSH7328"), Clean, Free, CopyAt(TEXT("PostProcess.TranslucencyAfterDOF"), TEXT("SceneDepth"), TEXT("Translucency")));
		Table.Add(TEXT("Writes.DSH7328.Velocity"), TEXT("DSH7328"), Raised, Free, CopyAt(TEXT("AfterBasePass"), TEXT("SceneDepth"), TEXT("Velocity")));
		Table.Add(TEXT("Writes.DSH7328.GBufferAfterOpaque"), TEXT("DSH7328"), Raised, Free, CopyAt(TEXT("AfterOpaque"), TEXT("SceneDepth"), TEXT("GBufferA")));
		Table.Add(TEXT("Writes.DSH7328.GBufferAfterBasePass"), TEXT("DSH7328"), Clean, Free, CopyAt(TEXT("AfterBasePass"), TEXT("SceneDepth"), TEXT("GBufferA")));
		Table.Add(TEXT("Writes.DSH7328.SceneDepth"), TEXT("DSH7328"), Raised, Free, CopyAt(TEXT("AfterOpaque"), TEXT("SceneColor"), TEXT("SceneDepth")));

		// ------------------------------------------------------------------ V4: reads before writes (DSH7331, DSH7332)
		Table.Add(TEXT("Lifetime.DSH7331.ReadBeforeWriteWithoutClear"), TEXT("DSH7331"), Raised, Free, Dsp(TEXT(R"DSP(
buffer Late : RGBA8(Clear = None);

pass Use : copy
{
    Injection = AfterOpaque;
    read Late;
    write SceneColor;
}

pass Fill : copy
{
    Injection = BeforePostProcess;
    read SceneColor;
    write Late;
}
)DSP")));
		Table.Add(TEXT("Lifetime.DSH7331.NeverWrittenWithoutClear"), TEXT("DSH7331"), Raised, Free, TEXT("buffer Late : RGBA8(Clear = None);\n\npass Use : copy\n{\n    read Late;\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("Lifetime.DSH7332.ReadBeforeWrite"), TEXT("DSH7332"), Warned, Free, Dsp(TEXT(R"DSP(
buffer Late : RGBA8;

pass Use : copy
{
    Injection = AfterOpaque;
    read Late;
    write SceneColor;
}

pass Fill : copy
{
    Injection = BeforePostProcess;
    read SceneColor;
    write Late;
}
)DSP")));
		Table.Add(TEXT("Lifetime.DSH7332.ReadAfterWrite"), TEXT("DSH7332"), Clean, Free, Dsp(TEXT(R"DSP(
buffer Late : RGBA8;

pass Fill : copy
{
    Injection = AfterOpaque;
    read SceneColor;
    write Late;
}

pass Use : copy
{
    Injection = BeforePostProcess;
    read Late;
    write SceneColor;
}
)DSP")));
		Table.Add(TEXT("Lifetime.DSH7331.PreviousBeforeWrite"), TEXT("DSH7331"), Clean, Free, Dsp(TEXT(R"DSP(
buffer Field : RGBA8(History = true, Clear = None);

pass Use : copy
{
    Injection = AfterOpaque;
    read Field.Previous;
    write SceneColor;
}

pass Fill : copy
{
    Injection = BeforePostProcess;
    read SceneColor;
    write Field;
}
)DSP")));

		// ------------------------------------------------------------------ V5: a pass reading what it writes (DSH7333)
		Table.Add(TEXT("ReadWrite.DSH7333.ComputeSameBuffer"), TEXT("DSH7333"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    read In = Mask;\n    write Out = Mask;\n}\n"));
		Table.Add(TEXT("ReadWrite.DSH7333.CopySameBuffer"), TEXT("DSH7333"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Keep : copy\n{\n    read Mask;\n    write Mask;\n}\n"));
		// The runtime writes the scene colour through a copy for every kind of pass, so any of them may read it as well.
		Table.Add(TEXT("ReadWrite.DSH7333.ComputeSceneColor"), TEXT("DSH7333"), Clean, Free, TEXT("pass Grade : compute\n{\n    Shader = \"Grade.usf\";\n    Entry = GradeCS;\n    read In = SceneColor;\n    write Out = SceneColor;\n}\n"));
		Table.Add(TEXT("ReadWrite.DSH7333.MaterialSceneColor"), TEXT("DSH7333"), Clean, Free, TEXT("pass Show : fullscreen\n{\n    Material = \"PP_Show\";\n    read SceneColor;\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("ReadWrite.DSH7333.TranslucencyAtItsPoint"), TEXT("DSH7333"), Clean, Free, TEXT("pass Mix : compute\n{\n    Injection = PostProcess.TranslucencyAfterDOF;\n    Shader = \"Mix.usf\";\n    Entry = MixCS;\n    read In = Translucency;\n    write Out = Translucency;\n}\n"));
		Table.Add(TEXT("ReadWrite.DSH7333.LastFrame"), TEXT("DSH7333"), Clean, Free, TEXT("buffer Field : RG16F(History = true);\n\npass Step : compute\n{\n    Shader = \"Step.usf\";\n    Entry = StepCS;\n    read Prev = Field.Previous;\n    write Out = Field;\n}\n"));

		// ------------------------------------------------------------------ V7: the sizes a mesh pass draws at (DSH7343)
		Table.Add(TEXT("Sizes.DSH7343.OutputsDiffer"), TEXT("DSH7343"), Raised, Free, TEXT("buffer Mask : R8;\nbuffer Small : R8(Scale = 0.5);\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    write Output0 = Mask;\n    write Output1 = Small;\n}\n"));
		Table.Add(TEXT("Sizes.DSH7343.OutputsAlike"), TEXT("DSH7343"), Clean, Free, TEXT("buffer Mask : R8;\nbuffer Soft : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    write Output0 = Mask;\n    write Output1 = Soft;\n}\n"));
		Table.Add(TEXT("Sizes.DSH7343.OwnDepthDiffers"), TEXT("DSH7343"), Raised, Free, TEXT("buffer ObjDepth : Depth32(Scale = 0.5);\nbuffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    Depth = Own(ObjDepth);\n    write Output0 = Mask;\n}\n"));
		// Testing against the scene's depth, a mesh pass may draw at another resolution: the runtime does that.
		Table.Add(TEXT("Sizes.DSH7343.TestSceneAtAnotherScale"), TEXT("DSH7343"), Clean, Free, TEXT("buffer Small : R8(Scale = 0.5);\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    write Output0 = Small;\n}\n"));
		// An own depth larger than the targets is bound as it is, and covers them.
		Table.Add(TEXT("Sizes.DSH7343.OwnDepthLarger"), TEXT("DSH7343"), Clean, Free, TEXT("buffer ObjDepth : Depth32;\nbuffer Small : R8(Scale = 0.5);\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    Depth = Own(ObjDepth);\n    write Output0 = Small;\n}\n"));
		// The scene's textures are larger than the view they hold, a buffer is the view's size: not one viewport.
		Table.Add(TEXT("Sizes.DSH7343.SceneColorWithOwnBuffer"), TEXT("DSH7343"), Raised, Free, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    write Output0 = SceneColor;\n    write Output1 = Mask;\n}\n"));

		// ------------------------------------------------------------------ V9: targets drawn beside the scene colour (DSH7350)
		Table.Add(TEXT("SceneColor.DSH7350.BesideRenderResolution"), TEXT("DSH7350"), Raised, Free, TEXT("buffer Data : RGBA16F(Resolution = Render);\n\npass Tone : fullscreen\n{\n    Injection = PostProcess.AfterTonemap;\n    Shader = \"Tone.usf\";\n    Entry = TonePS;\n    write Out0 = SceneColor;\n    write Out1 = Data;\n}\n"));
		Table.Add(TEXT("SceneColor.DSH7350.BesideOutputResolution"), TEXT("DSH7350"), Clean, Free, TEXT("buffer Data : RGBA16F(Resolution = Output);\n\npass Tone : fullscreen\n{\n    Injection = PostProcess.AfterTonemap;\n    Shader = \"Tone.usf\";\n    Entry = TonePS;\n    write Out0 = SceneColor;\n    write Out1 = Data;\n}\n"));

		// ------------------------------------------------------------------ V10: one tonemapper (DSH7351)
		Table.Add(TEXT("Tonemapper.DSH7351.Twice"), TEXT("DSH7351"), Raised, Free, TEXT("pass ToneA : fullscreen\n{\n    Injection = PostProcess.ReplaceTonemapper;\n    Material = \"PP_ToneA\";\n    write SceneColor;\n}\n\npass ToneB : fullscreen\n{\n    Injection = PostProcess.ReplaceTonemapper;\n    Material = \"PP_ToneB\";\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("Tonemapper.DSH7351.WritesNoSceneColor"), TEXT("DSH7351"), Raised, Free, CopyAt(TEXT("PostProcess.ReplaceTonemapper"), TEXT("SceneColor"), TEXT("Mask")));
		Table.Add(TEXT("Tonemapper.DSH7351.Once"), TEXT("DSH7351"), Clean, Free, TEXT("pass Tone : fullscreen\n{\n    Injection = PostProcess.ReplaceTonemapper;\n    Material = \"PP_Tone\";\n    write SceneColor;\n}\n"));

		// ------------------------------------------------------------------ V12 and V13: exports, and buffers nobody writes or reads (DSH7354, DSH7356, DSH7357)
		Table.Add(TEXT("Export.DSH7354.LastWrittenMidFrame"), TEXT("DSH7354"), Noted, Free, TEXT("buffer Glow : R8(Export = true);\n\npass Fill : clear\n{\n    Injection = AfterOpaque;\n    write Glow;\n}\n"));
		Table.Add(TEXT("Export.DSH7354.LastWrittenAtBeginView"), TEXT("DSH7354"), Clean, Free, TEXT("buffer Glow : R8(Export = true);\n\npass Fill : clear\n{\n    Injection = BeginView;\n    write Glow;\n}\n"));
		Table.Add(TEXT("Lifetime.DSH7356.NeverWritten"), TEXT("DSH7356"), Warned, Free, WithBase(TEXT("buffer Unused : R8;\n")));
		Table.Add(TEXT("Lifetime.DSH7357.NeverRead"), TEXT("DSH7357"), Warned, Free, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    write Mask;\n}\n"));
		Table.Add(TEXT("Lifetime.DSH7357.Exported"), TEXT("DSH7357"), Clean, Free, TEXT("buffer Mask : R8(Export = true);\n\npass Fill : clear\n{\n    write Mask;\n}\n"));

		// ------------------------------------------------------------------ the engine facts (V6-V8 against the references)
		Table.Add(TEXT("References.DSH4412.UnknownLayer"), TEXT("DSH4412"), Raised, Refs, MeshWith(TEXT("    Filter = Layer(Ghost);\n")));
		Table.Add(TEXT("References.DSH4412.KnownLayers"), TEXT("DSH4412"), Clean, Refs, MeshWith(TEXT("    Filter = Layer(Highlight | XRay);\n")));
		Table.Add(TEXT("References.DSH4406.UnknownMaterial"), TEXT("DSH4406"), Raised, Refs, TEXT("pass Show : fullscreen\n{\n    Material = \"PP_Unknown\";\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("References.DSH4406.NothingFound"), TEXT("DSH4406"), Raised, Refs, MeshWith(TEXT("    Filter = Stencil(1);\n    Material = \"M_Missing\";\n")));
		// FPipelineReferences::FindMaterial compares case-sensitively: two spellings are two references.
		Table.Add(TEXT("References.DSH4406.ReferenceCase"), TEXT("DSH4406"), Raised, Refs, TEXT("pass Show : fullscreen\n{\n    Material = \"pp_two\";\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("References.DSH7337.SurfaceInFullscreen"), TEXT("DSH7337"), Raised, Refs, TEXT("pass Show : fullscreen\n{\n    Material = \"M_Mask\";\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("References.DSH7334.ReadNotAnInput"), TEXT("DSH7334"), Raised, Refs, TEXT("buffer Mask : R8;\nbuffer Blurred : R8;\nbuffer Other : R8;\n\npass Composite : fullscreen\n{\n    Material = \"PP_Two\";\n    read Mask;\n    read Blurred;\n    read Other;\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("References.DSH7335.InputUnbound"), TEXT("DSH7335"), Warned, Refs, TEXT("buffer Mask : R8;\n\npass Composite : fullscreen\n{\n    Material = \"PP_Two\";\n    read Mask;\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("References.DSH7334.InputsBound"), TEXT("DSH7334"), Clean, Refs, Dsp(TEXT(R"DSP(
buffer Mask : R8;
buffer Blurred : R8;

pass Fill : clear
{
    Injection = AfterOpaque;
    write Mask;
}

pass Soften : clear
{
    Injection = AfterOpaque;
    write Blurred;
}

pass Composite : fullscreen
{
    Material = "PP_Two";
    read Mask;
    read Blurred;
    write SceneColor;
}
)DSP")));
		Table.Add(TEXT("References.DSH7336.InputsOverTheFreeSlots"), TEXT("DSH7336"), Raised, Refs, FiveInputsSource(TEXT("PP_FiveBusy"), TEXT("    read A;\n    read B;\n    read C;\n    read D;\n    read E;\n")));
		// Every named input of the material takes a slot, bound or not: what counts is the material's inputs, not the reads.
		Table.Add(TEXT("References.DSH7336.CountsTheMaterialsInputs"), TEXT("DSH7336"), Raised, Refs, FiveInputsSource(TEXT("PP_FiveBusy"), TEXT("    read A;\n")));
		Table.Add(TEXT("References.DSH7336.InputsFit"), TEXT("DSH7336"), Clean, Refs, FiveInputsSource(TEXT("PP_Five"), TEXT("    read A;\n    read B;\n    read C;\n    read D;\n    read E;\n")));
		Table.Add(TEXT("References.DSH7338.DataWithPreExposure"), TEXT("DSH7338"), Raised, Refs, TEXT("buffer Mask : R8;\nbuffer Data : RGBA16F;\n\npass Bake : fullscreen\n{\n    Material = \"PP_NoExposure\";\n    read Source = Mask;\n    write Data;\n}\n"));
		Table.Add(TEXT("References.DSH7338.DataWithoutPreExposure"), TEXT("DSH7338"), Clean, Refs, TEXT("buffer Mask : R8;\nbuffer Data : RGBA16F;\n\npass Bake : fullscreen\n{\n    Material = \"PP_Data\";\n    read Source = Mask;\n    write Data;\n}\n"));
		Table.Add(TEXT("References.DSH7339.AfterTonemapMaterialEarly"), TEXT("DSH7339"), Warned, Refs, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    Injection = AfterOpaque;\n    write Mask;\n}\n\npass Show : fullscreen\n{\n    Material = \"PP_AfterTonemap\";\n    read Mask;\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("References.DSH7339.AfterTonemapMaterialLate"), TEXT("DSH7339"), Clean, Refs, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    Injection = AfterOpaque;\n    write Mask;\n}\n\npass Show : fullscreen\n{\n    Injection = PostProcess.AfterTonemap;\n    Material = \"PP_AfterTonemap\";\n    read Mask;\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("References.DSH7344.PostProcessInMesh"), TEXT("DSH7344"), Raised, Refs, MeshWith(TEXT("    Filter = Stencil(1);\n    Material = \"PP_Two\";\n")));
		Table.Add(TEXT("References.DSH7340.NoPassOutput"), TEXT("DSH7340"), Raised, Refs, MeshWith(TEXT("    Filter = Stencil(1);\n    Material = \"M_NoOutput\";\n")));
		Table.Add(TEXT("References.DSH7341.OutputNotConnected"), TEXT("DSH7341"), Raised, Refs, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    Material = \"M_Mask\";\n    write Output2 = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH7341.OutputConnected"), TEXT("DSH7341"), Clean, Refs, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    Material = \"M_Mask\";\n    write Output1 = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH7342.UsageMissing"), TEXT("DSH7342"), Raised, Refs, MeshWith(TEXT("    Filter = Stencil(1);\n    Material = \"M_NoSkeletal\";\n")));
		Table.Add(TEXT("References.DSH7342.UsageCovered"), TEXT("DSH7342"), Clean, Refs, MeshWith(TEXT("    Filter = Stencil(1);\n    Material = \"M_NoSkeletal\";\n    Usage = StaticMesh | InstancedStaticMeshes;\n")));
		Table.Add(TEXT("References.DSH7345.NewTranslator"), TEXT("DSH7345"), Raised, Refs, MeshWith(TEXT("    Filter = Stencil(1);\n    Material = \"M_NewTranslator\";\n")));
		Table.Add(TEXT("References.DSH4408.ShaderMissing"), TEXT("DSH4408"), Raised, Refs, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Missing.usf\";\n    Entry = BlurCS;\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH4408.ShaderUnlisted"), TEXT("DSH4408"), Raised, Refs, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Unlisted.usf\";\n    Entry = BlurCS;\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH4408.VirtualShader"), TEXT("DSH4408"), Clean, Refs, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"/Project/Passes/Virtual.usf\";\n    Entry = VirtCS;\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH4410.ComputeEntryNotFound"), TEXT("DSH4410"), Raised, Refs, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = NoSuchCS;\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH4411.NoNumThreads"), TEXT("DSH4411"), Raised, Refs, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Macro.usf\";\n    Entry = MacroCS;\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH4411.ThreadsWritten"), TEXT("DSH4411"), Clean, Refs, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Macro.usf\";\n    Entry = MacroCS;\n    Threads = uint3(8, 8, 1);\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH4413.ThreadsDisagree"), TEXT("DSH4413"), Raised, Refs, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    Threads = uint3(16, 16, 1);\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH4413.ThreadsAgree"), TEXT("DSH4413"), Clean, Refs, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    Threads = uint3(8, 8, 1);\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH4413.ThreadsFromTheShader"), TEXT("DSH4413"), Clean, Refs, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Blur.usf\";\n    Entry = WideCS;\n    write Result = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH4410.PixelEntryNotFound"), TEXT("DSH4410"), Raised, Refs, TEXT("buffer Mask : R8;\n\npass Show : fullscreen\n{\n    Shader = \"Blur.usf\";\n    Entry = NoSuchPS;\n    write Out = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH4410.PixelEntry"), TEXT("DSH4410"), Clean, Refs, TEXT("buffer Mask : R8;\n\npass Show : fullscreen\n{\n    Shader = \"Blur.usf\";\n    Entry = MainPS;\n    write Out = Mask;\n}\n"));
		// A fullscreen pass runs its Entry as a pixel shader: a `[numthreads]` compute function there is refused, on the Entry line.
		Table.AddErrorOnLine(TEXT("References.DSH4410.ComputeEntryInFullscreen"), TEXT("DSH4410"), Refs,
			TEXT("buffer Mask : R8;\n\npass Show : fullscreen\n{\n    Shader = \"Blur.usf\";\n    Entry = BlurCS;\n    write Out = Mask;\n}\n"),
			TEXT("Entry = BlurCS;"));
		// A file whose includes the host could not all follow (bEntryScanComplete false): an entry it did not see is not called
		// missing, but a compute pass then has to say its thread group itself.
		Table.Add(TEXT("References.DSH4410.UnreadIncludeNotMissing"), TEXT("DSH4410"), Clean, Refs, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Partial.usf\";\n    Entry = ElsewhereCS;\n    Threads = uint3(8, 8, 1);\n    write Result = Mask;\n}\n"));
		Table.AddErrorOnLine(TEXT("References.DSH4411.UnreadIncludeNeedsThreads"), TEXT("DSH4411"), Refs,
			TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Partial.usf\";\n    Entry = ElsewhereCS;\n    write Result = Mask;\n}\n"),
			TEXT("Entry = ElsewhereCS;"));
		Table.Add(TEXT("References.DSH4410.UnreadIncludePixelEntry"), TEXT("DSH4410"), Clean, Refs, TEXT("buffer Mask : R8;\n\npass Show : fullscreen\n{\n    Shader = \"Partial.usf\";\n    Entry = ElsewherePS;\n    write Out = Mask;\n}\n"));
		Table.Add(TEXT("References.DSH4411.SeenEntryWithUnreadInclude"), TEXT("DSH4411"), Clean, Refs, TEXT("buffer Mask : R8;\n\npass Blur : compute\n{\n    Shader = \"Partial.usf\";\n    Entry = KnownCS;\n    write Result = Mask;\n}\n"));

		// ------------------------------------------------------------------ an engine without the Custom Pass runtime
		Table.Add(TEXT("WithoutCustomPass.DSH7360.FactsNotRead"), TEXT("DSH7360"), Noted, NoPass, MeshWith(TEXT("    Filter = Stencil(1);\n    Material = \"M_NoOutput\";\n")));
		Table.Add(TEXT("WithoutCustomPass.DSH7340.NotChecked"), TEXT("DSH7340"), Clean, NoPass, MeshWith(TEXT("    Filter = Stencil(1);\n    Material = \"M_NoOutput\";\n")));
		Table.Add(TEXT("WithoutCustomPass.DSH7334.NotChecked"), TEXT("DSH7334"), Clean, NoPass, TEXT("buffer Other : R8;\n\npass Composite : fullscreen\n{\n    Material = \"PP_Two\";\n    read Other;\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("WithoutCustomPass.DSH7337.DomainStillChecked"), TEXT("DSH7337"), Raised, NoPass, TEXT("pass Show : fullscreen\n{\n    Material = \"M_Mask\";\n    write SceneColor;\n}\n"));
		Table.Add(TEXT("EngineFree.DSH7360.Said"), TEXT("DSH7360"), Noted, Free, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    write Mask;\n}\n"));
		Table.Add(TEXT("EngineFree.DSH7360.NotSaidWithReferences"), TEXT("DSH7360"), Clean, Refs, TEXT("buffer Mask : R8;\n\npass Fill : clear\n{\n    write Mask;\n}\n"));

		return MoveTemp(Table.Cases);
	}

	inline const TArray<FPipelineRuleCase>& GetPipelineRuleCases()
	{
		static const TArray<FPipelineRuleCase> Cases = MakePipelineRuleCases();
		return Cases;
	}

	// =============================================================================================
	// The Custom Pass nodes in a `.dss`
	// =============================================================================================

	/** The hand-built test catalog with the two classes DreamShaderPass brings on UE 5.8, as the reflected catalog shapes them. */
	inline IR::FBuiltinCatalog MakePassNodeCatalog()
	{
		IR::FBuiltinCatalog Catalog = Tests::MakeDreamShaderTestBuiltinCatalog();
		{
			IR::FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("DreamPassOutput");
			Expression.ClassName = TEXT("MaterialExpressionDreamPassOutput");
			Expression.ClassPathName = TEXT("/Script/DreamShaderPass.MaterialExpressionDreamPassOutput");
			for (const TCHAR* Pin : { TEXT("Output0"), TEXT("Output1"), TEXT("Output2"), TEXT("Output3") })
			{
				Expression.Inputs.Add(Tests::MakeDreamShaderTestPin(Pin, IR::ECatalogValueType::Numeric));
			}
			// A custom output with no output pin: a statement, never a value.
			Expression.bIsCustomOutput = true;
			Catalog.Expressions.Add(MoveTemp(Expression));
		}
		{
			IR::FCatalogExpression Expression;
			Expression.Namespace = TEXT("UE");
			Expression.ShortName = TEXT("DreamPassBuffer");
			Expression.ClassName = TEXT("MaterialExpressionDreamPassBuffer");
			Expression.ClassPathName = TEXT("/Script/DreamShaderPass.MaterialExpressionDreamPassBuffer");
			Expression.Inputs.Add(Tests::MakeDreamShaderTestPin(TEXT("Coordinates"), IR::ECatalogValueType::Float2));
			Expression.Outputs.Add(Tests::MakeDreamShaderTestPin(TEXT(""), IR::ECatalogValueType::Float4));
			Expression.Properties.Add(Tests::MakeDreamShaderTestProperty(TEXT("Pipeline"), IR::ECatalogValueType::Object));
			Expression.Properties.Add(Tests::MakeDreamShaderTestProperty(TEXT("Buffer"), IR::ECatalogValueType::Name));
			Catalog.Expressions.Add(MoveTemp(Expression));
		}
		return Catalog;
	}

	inline const IR::FBuiltinCatalog& GetPassNodeCatalog()
	{
		static const IR::FBuiltinCatalog Catalog = MakePassNodeCatalog();
		return Catalog;
	}

	/** One parse and bind of a `.dss`. */
	struct FSourceBind
	{
		FLangParseResult Parse;
		FLangBindResult Bind;

		bool Has(const TCHAR* Code, const ELangSeverity Severity) const { return HasCode(Parse.Diagnostics, Code, Severity) || HasCode(Bind.Diagnostics, Code, Severity); }
		bool Has(const TCHAR* Code) const { return HasCode(Parse.Diagnostics, Code) || HasCode(Bind.Diagnostics, Code); }
		bool HasErrors() const { return Parse.Diagnostics.HasErrors() || Bind.Diagnostics.HasErrors(); }
		FString Describe() const { return DescribeDiagnostics(Parse.Diagnostics) + TEXT(" || ") + DescribeDiagnostics(Bind.Diagnostics); }
	};

	inline void BindSource(FSourceBind& Out, const TCHAR* Path, const FString& Text, const IR::FBuiltinCatalog& Catalog)
	{
		Out.Parse = ParseDreamShaderLang(FLangSourceText(Path, Text), FLangParseOptions());
		if (!Out.Parse.Module.IsValid())
		{
			return;
		}
		FBindOptions Options;
		Options.Catalog = &Catalog;
		Out.Bind = BindDreamShaderLang(*Out.Parse.Module, Options);
	}
}

// ---------------------------------------------------------------------------------------------
// Parse
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLangPipelineParseTest,
	"DreamShader.Lang2.Pipeline.Parse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLangPipelineParseTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangPipelineTests;

	// ---- the two declarations, every statement kind, and the spans an edit needs
	{
		const FString Text = Lines({
			TEXT("#pragma pipeline(Order = 2, Injection = PostProcess.AfterDOF, Views = Game|SceneCapture, Enabled = \"On\")"),
			TEXT("buffer Mask : R8();"),
			TEXT("buffer Blurred : RGBA16F(Scale = 0.5, Export = true);"),
			TEXT("pass Blur : compute"),
			TEXT("{"),
			TEXT("    Injection = AfterOpaque;"),
			TEXT("    read Source = Mask;"),
			TEXT("    read Mask.Previous;"),
			TEXT("    write Result = Blurred;"),
			TEXT("    param Gain = Strength * 2.0;"),
			TEXT("    read write;"),
			TEXT("}"),
		});
		const FLangSourceText Source(TEXT("Parse.dsp"), Text);
		const FLangParseResult Parsed = ParseDreamShaderLang(Source, FLangParseOptions());
		if (TestTrue(FString::Printf(TEXT("the pipeline parses (%s)"), *DescribeDiagnostics(Parsed.Diagnostics)), Parsed.Succeeded()))
		{
			const FModule& Module = *Parsed.Module;
			TestTrue(TEXT("a .dsp is a pipeline file"), Module.FileKind == ELangFileKind::Dsp);
			TestEqual(TEXT("one pragma, two buffers, one pass"), Module.Declarations.Num(), 4);

			const FPragmaDecl* Pragma = Module.Declarations.IsValidIndex(0) ? Module.Declarations[0]->As<FPragmaDecl>() : nullptr;
			if (TestNotNull(TEXT("the pragma"), Pragma))
			{
				TestTrue(TEXT("#pragma pipeline is of its own kind"), Pragma->PragmaKind == EPragmaKind::Pipeline);
				if (TestEqual(TEXT("four keyed arguments"), Pragma->Arguments.Num(), 4))
				{
					ExpectString(*this, TEXT("Order"), Pragma->Arguments[0].Value, TEXT("2"));
					ExpectString(*this, TEXT("a dotted injection point is one value"), Pragma->Arguments[1].Value, TEXT("PostProcess.AfterDOF"));
					ExpectString(*this, TEXT("a bar list is one value, one space around each bar"), Pragma->Arguments[2].Value, TEXT("Game | SceneCapture"));
					ExpectString(*this, TEXT("a quoted value is kept without its quotes"), Pragma->Arguments[3].Value, TEXT("On"));
					TestTrue(TEXT("and says it was quoted"), Pragma->Arguments[3].bQuoted);
					TestFalse(TEXT("an unquoted one does not"), Pragma->Arguments[2].bQuoted);
				}
			}

			const FBufferDecl* Mask = Module.Declarations.IsValidIndex(1) ? Module.Declarations[1]->As<FBufferDecl>() : nullptr;
			if (TestNotNull(TEXT("buffer Mask"), Mask))
			{
				ExpectString(*this, TEXT("the buffer's name span"), Source.Slice(Mask->NameSpan), TEXT("Mask"));
				ExpectString(*this, TEXT("the buffer's format, as written"), Mask->Format, TEXT("R8"));
				ExpectString(*this, TEXT("the format span"), Source.Slice(Mask->FormatSpan), TEXT("R8"));
				TestTrue(TEXT("an empty argument list is remembered"), Mask->bHasArgumentList);
				TestEqual(TEXT("and holds nothing"), Mask->Arguments.Num(), 0);
				ExpectString(*this, TEXT("the argument list span"), Source.Slice(Mask->ArgumentListSpan), TEXT("()"));
			}

			const FBufferDecl* Blurred = Module.Declarations.IsValidIndex(2) ? Module.Declarations[2]->As<FBufferDecl>() : nullptr;
			if (TestNotNull(TEXT("buffer Blurred"), Blurred) && TestEqual(TEXT("two arguments"), Blurred->Arguments.Num(), 2))
			{
				ExpectString(*this, TEXT("the first key"), Blurred->Arguments[0].Key, TEXT("Scale"));
				ExpectString(*this, TEXT("the key's span"), Source.Slice(Blurred->Arguments[0].KeySpan), TEXT("Scale"));
				ExpectString(*this, TEXT("the argument's span, separator excluded"), Source.Slice(Blurred->Arguments[0].Span), TEXT("Scale = 0.5"));
				ExpectString(*this, TEXT("the second key"), Blurred->Arguments[1].Key, TEXT("Export"));
				TestTrue(TEXT("a value is an expression"), Blurred->Arguments[1].Value.IsValid());
			}

			const FPassDecl* Blur = Module.Declarations.IsValidIndex(3) ? Module.Declarations[3]->As<FPassDecl>() : nullptr;
			if (TestNotNull(TEXT("pass Blur"), Blur))
			{
				ExpectString(*this, TEXT("the pass's kind, as written"), Blur->PassKind, TEXT("compute"));
				ExpectString(*this, TEXT("the pass kind's span"), Source.Slice(Blur->PassKindSpan), TEXT("compute"));
				const FString Body = Source.Slice(Blur->BodySpan);
				TestTrue(TEXT("the body span runs from '{' to '}'"), Body.StartsWith(TEXT("{")) && Body.EndsWith(TEXT("}")));
				if (TestEqual(TEXT("six statements"), Blur->Statements.Num(), 6))
				{
					const FPassStmt& Setting = *Blur->Statements[0];
					TestTrue(TEXT("a setting"), Setting.StmtKind == EPassStmtKind::Setting);
					ExpectString(*this, TEXT("its key"), Setting.Name, TEXT("Injection"));
					TestTrue(TEXT("its value an identifier"), Setting.Value.IsValid() && Setting.Value->Is<FIdentifierExpr>());

					const FPassStmt& Named = *Blur->Statements[1];
					TestTrue(TEXT("a read"), Named.StmtKind == EPassStmtKind::Read);
					ExpectString(*this, TEXT("its name inside the pass"), Named.Name, TEXT("Source"));
					ExpectString(*this, TEXT("its buffer"), Named.Buffer, TEXT("Mask"));
					ExpectString(*this, TEXT("the name's span"), Source.Slice(Named.NameSpan), TEXT("Source"));
					ExpectString(*this, TEXT("the buffer's span"), Source.Slice(Named.BufferSpan), TEXT("Mask"));
					TestFalse(TEXT("this frame's"), Named.bPrevious);

					const FPassStmt& Previous = *Blur->Statements[2];
					TestTrue(TEXT("a read of last frame"), Previous.StmtKind == EPassStmtKind::Read && Previous.bPrevious);
					TestTrue(TEXT("under the buffer's own name"), Previous.Name.IsEmpty());
					ExpectString(*this, TEXT("the .Previous span, the dot included"), Source.Slice(Previous.PreviousSpan), TEXT(".Previous"));

					const FPassStmt& Write = *Blur->Statements[3];
					TestTrue(TEXT("a write"), Write.StmtKind == EPassStmtKind::Write);
					ExpectString(*this, TEXT("its name"), Write.Name, TEXT("Result"));

					const FPassStmt& Param = *Blur->Statements[4];
					TestTrue(TEXT("a param"), Param.StmtKind == EPassStmtKind::Param);
					ExpectString(*this, TEXT("its target"), Param.Name, TEXT("Gain"));
					TestTrue(TEXT("its value an expression"), Param.Value.IsValid() && Param.Value->Is<FBinaryExpr>());

					// `read` begins a binding where a statement begins, and the name after it is a buffer even when it is `write`.
					const FPassStmt& Contextual = *Blur->Statements[5];
					TestTrue(TEXT("`read write;` reads a buffer called write"), Contextual.StmtKind == EPassStmtKind::Read && Contextual.Buffer.Equals(TEXT("write"), ESearchCase::CaseSensitive));
				}
			}
		}
	}

	// ---- in a `.dss` the pipeline words are names, and a pipeline declaration says where it belongs
	{
		const FLangParseResult Names = ParseDreamShaderLang(FLangSourceText(TEXT("Names.dss"), Lines({
			TEXT("uniform float pass = 1.0;"),
			TEXT("static const float buffer = 2.0;"),
			TEXT("float read(float write)"),
			TEXT("{"),
			TEXT("    float param = write;"),
			TEXT("    return param;"),
			TEXT("}"),
		})));
		TestTrue(FString::Printf(TEXT("pass, buffer, read, write and param are names in a .dss (%s)"), *DescribeDiagnostics(Names.Diagnostics)), Names.Succeeded());
		if (Names.Module.IsValid())
		{
			TestTrue(TEXT("a .dss is no pipeline file"), Names.Module->FileKind == ELangFileKind::Dss);
			TestEqual(TEXT("three declarations"), Names.Module->Declarations.Num(), 3);
			TestEqual(TEXT("no buffer declaration"), Names.Module->CountDecls(ENodeKind::BufferDecl), 0);
			TestEqual(TEXT("no pass declaration"), Names.Module->CountDecls(ENodeKind::PassDecl), 0);
		}

		const FLangParseResult Buffer = ParseDreamShaderLang(FLangSourceText(TEXT("Buffer.dss"), TEXT("buffer Mask : R8;\n")));
		TestTrue(TEXT("a buffer declaration in a .dss is DSH3310"), HasCode(Buffer.Diagnostics, TEXT("DSH3310"), ELangSeverity::Error));
		const FLangParseResult Pass = ParseDreamShaderLang(FLangSourceText(TEXT("Pass.dss"), TEXT("pass Fill : clear\n{\n    write Mask;\n}\n")));
		TestTrue(TEXT("a pass declaration in a .dss is DSH3310"), HasCode(Pass.Diagnostics, TEXT("DSH3310"), ELangSeverity::Error));
	}

	// ---- recovery: what one broken statement costs
	{
		const FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(TEXT("Recover.dsp"), Lines({
			TEXT("buffer A : R8;"),
			TEXT("buffer B : R8;"),
			TEXT("pass P : copy"),
			TEXT("{"),
			TEXT("    42;"),
			TEXT("    read A;"),
			TEXT("    write B;"),
			TEXT("}"),
		})));
		TestTrue(TEXT("a number is no statement: DSH2301"), HasCode(Parsed.Diagnostics, TEXT("DSH2301"), ELangSeverity::Error));
		const FPassDecl* Pass = (Parsed.Module.IsValid() && Parsed.Module->Declarations.IsValidIndex(2)) ? Parsed.Module->Declarations[2]->As<FPassDecl>() : nullptr;
		if (TestNotNull(TEXT("the pass survives its bad statement"), Pass))
		{
			TestEqual(TEXT("and keeps the two statements after it"), Pass->Statements.Num(), 2);
		}
	}
	{
		const FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(TEXT("Unclosed.dsp"), Lines({
			TEXT("buffer A : R8;"),
			TEXT("pass First : clear"),
			TEXT("{"),
			TEXT("    write A;"),
			TEXT("pass Second : clear"),
			TEXT("{"),
			TEXT("    write A;"),
			TEXT("}"),
		})));
		TestTrue(TEXT("a missing '}' is DSH2314"), HasCode(Parsed.Diagnostics, TEXT("DSH2314"), ELangSeverity::Error));
		if (Parsed.Module.IsValid() && TestEqual(TEXT("and both passes are kept"), Parsed.Module->Declarations.Num(), 3))
		{
			const FPassDecl* First = Parsed.Module->Declarations[1]->As<FPassDecl>();
			const FPassDecl* Second = Parsed.Module->Declarations[2]->As<FPassDecl>();
			TestTrue(TEXT("the first with its statement"), First && First->Statements.Num() == 1);
			TestTrue(TEXT("the second whole"), Second && Second->Name.Equals(TEXT("Second"), ESearchCase::CaseSensitive) && Second->Statements.Num() == 1);
		}
	}
	{
		const FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(TEXT("Semicolon.dsp"), Lines({
			TEXT("buffer A : R8;"),
			TEXT("pass First : clear"),
			TEXT("{"),
			TEXT("    write A"),
			TEXT("}"),
			TEXT("buffer B : R8;"),
			TEXT("pass Fine : copy"),
			TEXT("{"),
			TEXT("    read A;"),
			TEXT("    write B;"),
			TEXT("}"),
		})));
		TestTrue(TEXT("a statement without ';' is DSH2303"), HasCode(Parsed.Diagnostics, TEXT("DSH2303"), ELangSeverity::Error));
		if (Parsed.Module.IsValid() && TestEqual(TEXT("the recovery stops at '}' and loses nothing after it"), Parsed.Module->Declarations.Num(), 4))
		{
			const FPassDecl* Fine = Parsed.Module->Declarations[3]->As<FPassDecl>();
			TestTrue(TEXT("the pass after it is whole"), Fine && Fine->Statements.Num() == 2);
		}
	}
	{
		const FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(TEXT("Directive.dsp"), Lines({
			TEXT("buffer Mask : R8;"),
			TEXT("pass Fill : clear"),
			TEXT("{"),
			TEXT("    #pragma region Inside"),
			TEXT("    write Mask;"),
			TEXT("}"),
		})));
		TestTrue(TEXT("a '#' line in a pass is DSH2312"), HasCode(Parsed.Diagnostics, TEXT("DSH2312"), ELangSeverity::Error));
		const FPassDecl* Pass = (Parsed.Module.IsValid() && Parsed.Module->Declarations.IsValidIndex(1)) ? Parsed.Module->Declarations[1]->As<FPassDecl>() : nullptr;
		if (TestNotNull(TEXT("the pass"), Pass))
		{
			// The directive is the one line it costs: the statement on the next line is still read.
			TestTrue(TEXT("the statement after a '#' line is still parsed"),
				Pass->Statements.Num() == 1 && Pass->Statements[0]->StmtKind == EPassStmtKind::Write && Pass->Statements[0]->Buffer.Equals(TEXT("Mask"), ESearchCase::CaseSensitive));
		}
	}
	{
		// `read`, `write` and `param` at the start of a statement begin a binding, whatever follows them.
		struct FBindingWordCase
		{
			const TCHAR* Statement;
			const TCHAR* Code;
		};
		const FBindingWordCase Cases[] =
		{
			{ TEXT("read;"), TEXT("DSH2304") },
			{ TEXT("write;"), TEXT("DSH2304") },
			{ TEXT("param;"), TEXT("DSH2306") },
			{ TEXT("param = 3;"), TEXT("DSH2306") },
		};
		for (const FBindingWordCase& Case : Cases)
		{
			const FString Text = FString::Printf(TEXT("buffer Mask : R8;\npass Fill : clear\n{\n    %s\n    write Mask;\n}\n"), Case.Statement);
			const FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(TEXT("Words.dsp"), Text));
			TestTrue(FString::Printf(TEXT("`%s` is %s (%s)"), Case.Statement, Case.Code, *DescribeDiagnostics(Parsed.Diagnostics)), HasCode(Parsed.Diagnostics, Case.Code, ELangSeverity::Error));
			TestFalse(FString::Printf(TEXT("`%s` is no setting (no DSH2302)"), Case.Statement), HasCode(Parsed.Diagnostics, TEXT("DSH2302")));
			const FPassDecl* Pass = (Parsed.Module.IsValid() && Parsed.Module->Declarations.IsValidIndex(1)) ? Parsed.Module->Declarations[1]->As<FPassDecl>() : nullptr;
			TestTrue(FString::Printf(TEXT("`%s` costs only itself"), Case.Statement), Pass && Pass->Statements.Num() == 1);
		}
	}

	// ---- trivia: a statement's trailing comment, and the block's own after its last statement
	{
		FLangParseOptions Options;
		Options.bKeepTrivia = true;
		const FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(TEXT("Trivia.dsp"), Lines({
			TEXT("buffer Mask : R8;"),
			TEXT("pass Fill : clear"),
			TEXT("{"),
			TEXT("    Value = 1.0; // after the setting"),
			TEXT("    write Mask;"),
			TEXT("    // inside, after the last statement"),
			TEXT("}"),
		})), Options);
		const FPassDecl* Pass = (Parsed.Module.IsValid() && Parsed.Module->Declarations.IsValidIndex(1)) ? Parsed.Module->Declarations[1]->As<FPassDecl>() : nullptr;
		if (TestNotNull(TEXT("the pass with trivia"), Pass) && TestEqual(TEXT("two statements"), Pass->Statements.Num(), 2))
		{
			const FLangTrivia* Statement = Parsed.Module->Trivia.Find(Pass->Statements[0].Get());
			TestTrue(TEXT("a comment after a statement is its Trailing"),
				Statement && Statement->Trailing.IsSet() && Statement->Trailing.GetValue().Text.Equals(TEXT("// after the setting"), ESearchCase::CaseSensitive));
			const FLangTrivia* Block = Parsed.Module->Trivia.Find(Pass);
			TestTrue(TEXT("a comment after the last statement is the pass's Inner"),
				Block && Block->Inner.Num() == 1 && Block->Inner[0].Text.Equals(TEXT("// inside, after the last statement"), ESearchCase::CaseSensitive));
		}
	}

	// ---- the references a host resolves before the bind
	{
		const FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(TEXT("References.dsp"), Lines({
			TEXT("buffer M : R8;"),
			TEXT("pass A : fullscreen { Material = \"PP_X\"; write SceneColor; }"),
			TEXT("pass B : fullscreen { Material = (\"PP_X\"); write SceneColor; }"),
			TEXT("pass C : fullscreen { Material = \"pp_x\"; write SceneColor; }"),
			TEXT("pass D : compute { Shader = \" Blur.usf \"; Entry = BlurCS; write R = M; }"),
			TEXT("pass E : compute { Shader = \"Blur.usf\"; Entry = BlurCS; write R = M; }"),
			TEXT("pass F : mesh { Filter = Stencil(1); Material = Unquoted; write Output0 = M; }"),
		})));
		if (TestTrue(FString::Printf(TEXT("the reference source parses (%s)"), *DescribeDiagnostics(Parsed.Diagnostics)), Parsed.Succeeded()))
		{
			TArray<FString> Materials;
			TArray<FString> Shaders;
			CollectDreamShaderPipelineReferences(*Parsed.Module, Materials, Shaders);
			// Each once, through parentheses and trimmed; two spellings are two references; a value that is no string is none.
			ExpectString(*this, TEXT("the materials"), FString::Join(Materials, TEXT(", ")), TEXT("PP_X, pp_x"));
			ExpectString(*this, TEXT("the shader files"), FString::Join(Shaders, TEXT(", ")), TEXT("Blur.usf"));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Print
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLangPipelinePrintTest,
	"DreamShader.Lang2.Pipeline.Print",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLangPipelinePrintTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangPipelineTests;

	// ---- the documented example, byte for byte: every key the binder fills in by itself is left out
	{
		const IR::FIRPassPipeline Highlight = MakeHighlightPayload();
		const FString Printed = PrintDreamShaderPipeline(Highlight, TEXT("P_Highlight.dsp"));
		const FString Expected = Lines({
			TEXT("#pragma pipeline(Order = 100)"),
			TEXT(""),
			TEXT("/// @group Look"),
			TEXT("uniform float4 OutlineColor = float4(1.0, 0.6, 0.0, 1.0);"),
			TEXT(""),
			TEXT("/// @group Look"),
			TEXT("/// @slider 1 8"),
			TEXT("uniform float OutlineWidth = 3.0;"),
			TEXT(""),
			TEXT("buffer Mask : R8;"),
			TEXT("buffer Blurred : R8(Scale = 0.5, Export = true);"),
			TEXT(""),
			TEXT("pass DrawMask : mesh"),
			TEXT("{"),
			TEXT("    Injection = AfterOpaque;"),
			TEXT("    Filter = Layer(Highlight);"),
			TEXT("    Material = \"M_HighlightMask\";"),
			TEXT("    Depth = None;"),
			TEXT("    write Output0 = Mask;"),
			TEXT("}"),
			TEXT(""),
			TEXT("pass Blur : compute"),
			TEXT("{"),
			TEXT("    Injection = AfterOpaque;"),
			TEXT("    Shader = \"BoxBlur.usf\";"),
			TEXT("    Entry = BlurCS;"),
			TEXT("    read Source = Mask;"),
			TEXT("    write Result = Blurred;"),
			TEXT("    param Radius = OutlineWidth;"),
			TEXT("}"),
			TEXT(""),
			TEXT("pass Composite : fullscreen"),
			TEXT("{"),
			TEXT("    Material = \"PP_OutlineComposite\";"),
			TEXT("    read Mask;"),
			TEXT("    read Blurred;"),
			TEXT("    write SceneColor;"),
			TEXT("    param Color = OutlineColor;"),
			TEXT("}"),
		});
		ExpectText(*this, TEXT("the canonical text of the Highlight example"), Printed, Expected);

		FPipelineBind Reread;
		BindPipeline(Reread, Printed, TEXT("P_Highlight.dsp"));
		if (TestTrue(FString::Printf(TEXT("the printed example binds (%s)"), *Reread.Describe()), Reread.HasBound() && !Reread.HasErrors()))
		{
			ExpectSamePipeline(*this, TEXT("print -> parse -> bind gives the example's payload back"), Highlight, Reread.Payload());
		}
	}

	// ---- a payload at nothing's default: each key printed in its canonical spelling, and the whole read back
	{
		const IR::FIRPassPipeline Rich = MakeRichPayload();
		const FString Printed = PrintDreamShaderPipeline(Rich, TEXT("Rich.dsp"));

		const TCHAR* const ExpectedLines[] =
		{
			TEXT("#pragma pipeline(Order = -3, Injection = PostProcess.AfterDOF, Views = Game | SceneCapture, Requires = PostProcess | CustomStencil, Enabled = On)\n"),
			TEXT("\nuniform bool On = true;\nuniform int Taps = -2;\n"),
			TEXT("\n/// Line one.\n/// Line two.\n/// @group Look\n/// @sort 5\nuniform float3 Tint = float3(0.25, 0.5, 1.0);\n"),
			TEXT("\n/// @slider 0.5 2.5\nuniform float Gain;\n"),
			TEXT("\n/// @default /Engine/EngineResources/DefaultTexture\nuniform Texture2D Noise;\nuniform Texture2D Empty;\n"),
			TEXT("\nbuffer Depth : Depth32;\nbuffer Mask : R8;\nbuffer Field : RG16F(Size = int2(64, 32), Clear = None, History = true);\n"),
			TEXT("\n/// @desc Soft.\nbuffer Soft : RGBA16F(Scale = 0.25, Clear = float4(1.0, 0.5, 0.0, 1.0), Mips = 3, Export = true);\nbuffer Late : RGBA8(Resolution = Render);\nbuffer Grey : R16F(Clear = 0.5);\n"),
			TEXT("    Injection = BeginView;\n    Shader = \"/Project/Passes/Sim.usf\";\n    Entry = SimCS;\n    Threads = uint3(16, 4, 1);\n    Dispatch = Field / 2;\n    read Previous = Field.Previous;\n    write Result = Field;\n"),
			TEXT("    param Scaled = Gain * 2.0 - 1.0;\n    param Weight = DreamPassWeight;\n    param Neg = Gain * -1.0;\n    param Count = 3;\n    param Flag = true;\n    param Pair = float2(0.25, 0.75);\n    param Third = 0.3;\n"),
			TEXT("    Filter = Layer(Highlight | \"Other Layer\") & Stencil(4, 15) | List(Enemies);\n"),
			TEXT("    Mode = OwnOrOverride;\n    Depth = Own(Depth);\n    Cull = Front;\n    Blend = Max;\n    Usage = StaticMesh | SplineMesh;\n    Nanite = AssignStencil(200);\n    NaniteValue = float4(1.0, 0.5, 0.0, 1.0);\n"),
			TEXT("    Value = float4(0.25, 0.5, 0.75, 1.0);\n"),
			TEXT("    Dispatch = uint3(64, 64, 1);\n"),
			TEXT("    Dispatch = Soft * 0.3;\n"),
			TEXT("\n/// @desc Tone it.\npass Tone : fullscreen\n"),
		};
		for (const TCHAR* Expected : ExpectedLines)
		{
			TestTrue(FString::Printf(TEXT("the printed text holds:\n%s"), Expected), Printed.Contains(Expected, ESearchCase::CaseSensitive));
		}
		TestEqual(TEXT("a thread group is written where it was written, and only there"), CountOccurrences(Printed, TEXT("Threads = ")), 1);
		TestFalse(TEXT("an injection point that is the pipeline's is left out"), Printed.Contains(TEXT("    Injection = PostProcess.AfterDOF;"), ESearchCase::CaseSensitive));
		if (HasAnyErrors())
		{
			AddInfo(FString::Printf(TEXT("The printed text:\n%s"), *Printed));
		}

		FPipelineBind Reread;
		BindPipeline(Reread, Printed, TEXT("Rich.dsp"));
		if (TestTrue(FString::Printf(TEXT("the printed text binds (%s)"), *Reread.Describe()), Reread.HasBound() && !Reread.HasErrors()))
		{
			ExpectSamePipeline(*this, TEXT("print -> parse -> bind gives the payload back"), Rich, Reread.Payload());
			ExpectText(*this, TEXT("and printing it again gives the same text"), PrintDreamShaderPipeline(Reread.Payload(), TEXT("Rich.dsp")), Printed);
		}
	}

	// ---- an empty pipeline: no pragma when every key is at its default
	{
		IR::FIRPassPipeline Empty;
		Empty.Buffers.Add(MakeBuffer(TEXT("Mask"), TEXT("R8")));
		ExpectText(*this, TEXT("a payload at every default prints no pragma"), PrintDreamShaderPipeline(Empty, TEXT("Empty.dsp")), Lines({ TEXT("buffer Mask : R8;") }));
	}

	// ---- the terminator the caller asks for
	{
		FLangPrintOptions Options;
		Options.NewLine = TEXT("\r\n");
		const FString Printed = PrintDreamShaderPipeline(MakeHighlightPayload(), TEXT("P_Highlight.dsp"), Options);
		TestTrue(TEXT("every line ends in \\r\\n"), Printed.Contains(TEXT("}\r\n"), ESearchCase::CaseSensitive) && !Printed.Replace(TEXT("\r\n"), TEXT("")).Contains(TEXT("\n")));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Format
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLangPipelineFormatTest,
	"DreamShader.Lang2.Pipeline.Format",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLangPipelineFormatTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangPipelineTests;

	const FString Messy = Lines({
		TEXT("// head comment"),
		TEXT("#pragma pipeline( Order=3 , Views = Game|Editor )"),
		TEXT("uniform float  Gain=2.0; // trailing"),
		TEXT("buffer   Mask:R8( Clear=0 );"),
		TEXT("/* block comment */"),
		TEXT("pass   Draw:clear{"),
		TEXT("  Value=1;   // inner trailing"),
		TEXT("     write Mask;"),
		TEXT("  // inner comment at end"),
		TEXT("}"),
	});
	const FString Expected = Lines({
		TEXT("// head comment"),
		TEXT("#pragma pipeline(Order = 3, Views = Game | Editor)"),
		TEXT("uniform float Gain = 2.0; // trailing"),
		TEXT("buffer Mask : R8(Clear = 0);"),
		TEXT("/* block comment */"),
		TEXT("pass Draw : clear"),
		TEXT("{"),
		TEXT("    Value = 1; // inner trailing"),
		TEXT("    write Mask;"),
		TEXT("    // inner comment at end"),
		TEXT("}"),
	});

	// ---- a messy file is rewritten in the printer's layout, every comment kept
	FString Formatted;
	{
		FLangDiagnosticSink Diagnostics;
		const ELangFormatOutcome Outcome = FormatDreamShaderLangSource(FLangSourceText(TEXT("Format.dsp"), Messy), FLangFormatOptions(), Formatted, Diagnostics);
		if (!ExpectString(*this, TEXT("a messy .dsp is Changed"), LexToString(Outcome), LexToString(ELangFormatOutcome::Changed)))
		{
			AddInfo(DescribeDiagnostics(Diagnostics));
		}
		ExpectText(*this, TEXT("the formatted .dsp"), Formatted, Expected);
	}

	// ---- and a formatted file is left as it is
	{
		FString Again;
		FLangDiagnosticSink Diagnostics;
		const ELangFormatOutcome Outcome = FormatDreamShaderLangSource(FLangSourceText(TEXT("Format.dsp"), Expected), FLangFormatOptions(), Again, Diagnostics);
		ExpectString(*this, TEXT("a formatted .dsp is Unchanged"), LexToString(Outcome), LexToString(ELangFormatOutcome::Unchanged));
		ExpectText(*this, TEXT("and its text comes back as it was"), Again, Expected);
	}

	// ---- the source's own line terminator
	{
		FString Crlf;
		FLangDiagnosticSink Diagnostics;
		const ELangFormatOutcome Outcome = FormatDreamShaderLangSource(FLangSourceText(TEXT("Format.dsp"), Messy.Replace(TEXT("\n"), TEXT("\r\n"))), FLangFormatOptions(), Crlf, Diagnostics);
		ExpectString(*this, TEXT("a messy CRLF .dsp is Changed"), LexToString(Outcome), LexToString(ELangFormatOutcome::Changed));
		ExpectText(*this, TEXT("and keeps \\r\\n"), Crlf, Expected.Replace(TEXT("\n"), TEXT("\r\n")));
	}

	// ---- what it leaves alone: a file that does not parse, and one that uses the preprocessor
	{
		FString Nothing;
		FLangDiagnosticSink Diagnostics;
		const ELangFormatOutcome Outcome = FormatDreamShaderLangSource(FLangSourceText(TEXT("Broken.dsp"), TEXT("buffer Mask : R8;\npass Fill : clear\n{\n    write Mask;\n")), FLangFormatOptions(), Nothing, Diagnostics);
		ExpectString(*this, TEXT("a .dsp that does not parse is Failed"), LexToString(Outcome), LexToString(ELangFormatOutcome::Failed));
		TestTrue(TEXT("with the parser's own code"), HasCode(Diagnostics, TEXT("DSH2150"), ELangSeverity::Error));
		TestTrue(TEXT("and no text"), Nothing.IsEmpty());
	}
	{
		FString Nothing;
		FLangDiagnosticSink Diagnostics;
		const ELangFormatOutcome Outcome = FormatDreamShaderLangSource(FLangSourceText(TEXT("Conditional.dsp"), TEXT("#if 1\nbuffer Mask : R8;\n#endif\npass Fill : clear\n{\n    write Mask;\n}\n")), FLangFormatOptions(), Nothing, Diagnostics);
		ExpectString(*this, TEXT("a .dsp with an #if is Skipped"), LexToString(Outcome), LexToString(ELangFormatOutcome::Skipped));
		TestTrue(TEXT("saying DSH9043"), HasCode(Diagnostics, TEXT("DSH9043"), ELangSeverity::Info));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Bind
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLangPipelineBindTest,
	"DreamShader.Lang2.Pipeline.Bind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLangPipelineBindTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangPipelineTests;

	// ---- the pragma, the parameters, the product and the parallel arrays
	{
		FPipelineBind Run;
		BindPipeline(Run, Dsp(TEXT(R"DSP(
#pragma pipeline(Order = 4, Injection = AfterOpaque, Views = Editor | Game, Requires = SceneResolve | PostProcess, Enabled = On)

uniform bool On = true;
uniform int Taps = 3;
/// @desc   Spaces around a description are trimmed.
uniform float2 Offset = float2(0.5, -1.0);
/// @default None
uniform Texture2D Noise;
/// @default /Engine/EngineResources/DefaultTexture
uniform Texture2D Fallback;
static const float Quarter = 0.25;

buffer Mask : R8(Scale = Quarter * 2.0);

pass Fill : clear
{
    write Mask;
}
)DSP")), TEXT("BindPragma.dsp"));

		if (TestTrue(FString::Printf(TEXT("the pragma case binds (%s)"), *Run.Describe()), Run.HasBound() && !Run.HasErrors()))
		{
			const IR::FIRPassPipeline& Payload = Run.Payload();
			TestTrue(TEXT("a .dsp binds as a pipeline"), Run.Pipeline().bIsPipeline);
			TestEqual(TEXT("Order"), Payload.Order, 4);
			ExpectString(*this, TEXT("the default injection point"), Payload.DefaultInjection, TEXT("AfterOpaque"));
			TestEqual(TEXT("Editor | Game is the default set of views, said by saying none"), Payload.Views.Num(), 0);
			ExpectString(*this, TEXT("the requirements, in their table's order"), FString::Join(Payload.Requires, TEXT("|")), TEXT("PostProcess|SceneResolve"));
			ExpectString(*this, TEXT("Enabled names the uniform bool"), Payload.EnabledParameter, TEXT("On"));

			if (TestEqual(TEXT("five parameters; a constant is none"), Payload.Parameters.Num(), 5))
			{
				const IR::FIRPassParameter& On = Payload.Parameters[0];
				ExpectString(*this, TEXT("bool type"), On.Type, TEXT("bool"));
				TestTrue(TEXT("a bool default"), On.Default.Kind == IR::EIRPropertyKind::Bool && On.Default.B);
				const IR::FIRPassParameter& Taps = Payload.Parameters[1];
				ExpectString(*this, TEXT("int type"), Taps.Type, TEXT("int"));
				TestTrue(TEXT("an int default"), Taps.Default.Kind == IR::EIRPropertyKind::Int && Taps.Default.I == 3);
				const IR::FIRPassParameter& Offset = Payload.Parameters[2];
				ExpectString(*this, TEXT("float2 type"), Offset.Type, TEXT("float2"));
				TestTrue(TEXT("a float2 default"), Offset.Default.Kind == IR::EIRPropertyKind::Float4 && Offset.Default.N == 2 && Offset.Default.V[0] == 0.5 && Offset.Default.V[1] == -1.0);
				ExpectString(*this, TEXT("the description, trimmed"), Offset.Description, TEXT("Spaces around a description are trimmed."));
				const IR::FIRPassParameter& Noise = Payload.Parameters[3];
				TestTrue(TEXT("`@default None` is no texture"), Noise.Default.Kind == IR::EIRPropertyKind::Object && Noise.Default.S.IsEmpty());
				const IR::FIRPassParameter& Fallback = Payload.Parameters[4];
				ExpectString(*this, TEXT("a texture default"), Fallback.Default.S, TEXT("/Engine/EngineResources/DefaultTexture"));
			}
			if (const IR::FIRPassBuffer* Mask = FindPayloadBuffer(Payload, TEXT("Mask")))
			{
				TestEqual(TEXT("a Scale folded from a constant"), Mask->Scale, 0.5);
			}
			if (const IR::FIRPass* Fill = FindPayloadPass(Payload, TEXT("Fill")))
			{
				ExpectString(*this, TEXT("a pass that names no injection point takes the pragma's"), Fill->Injection, TEXT("AfterOpaque"));
				TestFalse(TEXT("and does not say it wrote one"), Fill->bInjectionWritten);
			}

			const FBoundPipeline& Pipeline = Run.Pipeline();
			TestEqual(TEXT("ParameterGlobals runs beside the parameters"), Pipeline.ParameterGlobals.Num(), Payload.Parameters.Num());
			TestEqual(TEXT("BufferDecls beside the buffers"), Pipeline.BufferDecls.Num(), Payload.Buffers.Num());
			TestEqual(TEXT("PassDecls beside the passes"), Pipeline.PassDecls.Num(), Payload.Passes.Num());
			TestNotNull(TEXT("the pragma is kept"), Pipeline.Pragma);
			TestEqual(TEXT("FindParameter"), Pipeline.FindParameter(TEXT("Taps")), 1);
			TestEqual(TEXT("FindBuffer"), Pipeline.FindBuffer(TEXT("Mask")), 0);
			TestEqual(TEXT("FindPass"), Pipeline.FindPass(TEXT("Fill")), 0);
			TestEqual(TEXT("names are case-sensitive"), Pipeline.FindPass(TEXT("fill")), INDEX_NONE);

			const FBoundModule& Module = Run.Module();
			if (TestEqual(TEXT("one product"), Module.Products.Num(), 1))
			{
				TestTrue(TEXT("a PassPipeline"), Module.Products[0].Kind == IR::EIRProductKind::PassPipeline);
				ExpectString(*this, TEXT("named after its file"), Module.Products[0].AssetName, TEXT("BindPragma"));
			}
			TestTrue(TEXT("an engine-free bind says what it could not check (DSH7360)"), Run.Has(TEXT("DSH7360"), ELangSeverity::Info));
		}
	}

	// ---- the defaults of a pass: mesh mode, Nanite policy, usage, dispatch, thread group
	{
		FPipelineBind Run;
		BindPipeline(Run, Dsp(TEXT(R"DSP(
buffer Mask : R8;
buffer Out : R8;

pass Own : mesh
{
    Injection = AfterOpaque;
    Filter = List(Enemies);
    write Output0 = Mask;
}

pass Over : mesh
{
    Injection = AfterOpaque;
    Filter = Stencil(2);
    Material = "M_Mask";
    Usage = SkeletalMesh | StaticMesh | InstancedStaticMeshes;
    write Output0 = Mask;
}

pass Run : compute
{
    Shader = "Run.usf";
    Entry = RunCS;
    Threads = uint2(16, 8);
    read Source = Mask;
    write Result = Out;
}

pass Enabled : clear
{
    Enabled = true;
    write Out;
}
)DSP")), TEXT("BindDefaults.dsp"));

		if (TestTrue(FString::Printf(TEXT("the defaults case binds (%s)"), *Run.Describe()), Run.HasBound() && !Run.HasErrors()))
		{
			const IR::FIRPassPipeline& Payload = Run.Payload();
			if (const IR::FIRPass* Own = FindPayloadPass(Payload, TEXT("Own")))
			{
				ExpectString(*this, TEXT("a mesh pass without a Material draws each object with its own"), Own->MeshMode, TEXT("Own"));
				ExpectString(*this, TEXT("and with no Stencil term skips Nanite"), Own->Nanite, TEXT("Skip"));
				ExpectString(*this, TEXT("its depth test is the scene's"), Own->Depth, TEXT("TestScene"));
			}
			if (const IR::FIRPass* Over = FindPayloadPass(Payload, TEXT("Over")))
			{
				ExpectString(*this, TEXT("with a Material it overrides"), Over->MeshMode, TEXT("Override"));
				ExpectString(*this, TEXT("with a Stencil term Nanite is a stencil mask"), Over->Nanite, TEXT("StencilMask"));
				TestEqual(TEXT("the default usage set, written in any order, is said by saying none"), Over->Usage.Num(), 0);
			}
			if (const IR::FIRPass* Compute = FindPayloadPass(Payload, TEXT("Run")))
			{
				ExpectString(*this, TEXT("a compute pass dispatches over its first write"), Compute->DispatchBuffer, TEXT("Out"));
				ExpectString(*this, TEXT("by buffer"), Compute->DispatchMode, TEXT("Buffer"));
				TestEqual(TEXT("one thread per texel"), Compute->DispatchScale, 1.0);
				TestTrue(TEXT("two numbers of Threads leave z at 1"), Compute->ThreadsX == 16 && Compute->ThreadsY == 8 && Compute->ThreadsZ == 1);
				TestTrue(TEXT("and say they were written"), Compute->bThreadsWritten);
				ExpectString(*this, TEXT("a pass without a pragma runs before post processing"), Compute->Injection, TEXT("BeforePostProcess"));
			}
			if (const IR::FIRPass* Switched = FindPayloadPass(Payload, TEXT("Enabled")))
			{
				TestTrue(TEXT("`Enabled = true` is no switch"), Switched->EnabledParameter.IsEmpty());
			}
		}
	}

	// ---- a filter in its normal form
	{
		FPipelineBind Run;
		BindPipeline(Run, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = (Layer(A | B | A) | List(L)) & Stencil(1, 0x0F);\n    write Output0 = Mask;\n}\n"), TEXT("BindFilter.dsp"));
		const IR::FIRPass* Draw = Run.HasBound() ? FindPayloadPass(Run.Payload(), TEXT("Draw")) : nullptr;
		if (TestNotNull(TEXT("the filter case binds"), Draw) && TestEqual(TEXT("`&` distributes over `|`: two clauses"), Draw->Filter.Num(), 2))
		{
			const IR::FIRPassFilterClause& First = Draw->Filter[0];
			const IR::FIRPassFilterClause& Second = Draw->Filter[1];
			TestTrue(TEXT("the first: the layers, each once, and the stencil"),
				First.AllOf.Num() == 2 && First.AllOf[0].Kind == TEXT("Layer") && FString::Join(First.AllOf[0].Layers, TEXT("|")).Equals(TEXT("A|B"), ESearchCase::CaseSensitive)
				&& First.AllOf[1].Kind == TEXT("Stencil") && First.AllOf[1].StencilValue == 1 && First.AllOf[1].StencilMask == 15);
			TestTrue(TEXT("the second: the list and the stencil"),
				Second.AllOf.Num() == 2 && Second.AllOf[0].List.Equals(TEXT("L"), ESearchCase::CaseSensitive) && Second.AllOf[1].StencilMask == 15);
		}
	}

	// ---- `param` folded to `source * a + b`
	{
		FPipelineBind Run;
		BindPipeline(Run, Dsp(TEXT(R"DSP(
uniform float Width = 2.0;
static const float Twice = 2.0;
buffer Out : R8;

pass Blur : compute
{
    Shader = "Blur.usf";
    Entry = MainCS;
    write Result = Out;
    param B = (Width - 1.0) * 0.25;
    param D = 1.0 - Width;
    param F = Twice * 3.0;
    param W = DreamPassWeight / 4.0;
}
)DSP")), TEXT("BindParams.dsp"));
		const IR::FIRPass* Blur = Run.HasBound() ? FindPayloadPass(Run.Payload(), TEXT("Blur")) : nullptr;
		if (TestNotNull(TEXT("the param case binds"), Blur) && TestEqual(TEXT("four params"), Blur->Params.Num(), 4))
		{
			TestTrue(TEXT("(Width - 1) * 0.25 is Width * 0.25 - 0.25"), Blur->Params[0].Multiplier == 0.25 && Blur->Params[0].Offset == -0.25);
			TestTrue(TEXT("1 - Width is Width * -1 + 1"), Blur->Params[1].Multiplier == -1.0 && Blur->Params[1].Offset == 1.0);
			TestTrue(TEXT("a constant expression is a constant"), Blur->Params[2].SourceKind == TEXT("Constant") && Blur->Params[2].ConstantType == TEXT("float") && Blur->Params[2].Constant.V[0] == 6.0);
			TestTrue(TEXT("the weight, divided"), Blur->Params[3].SourceKind == TEXT("Weight") && Blur->Params[3].Multiplier == 0.25);
		}
	}

	// ---- against the host's references: object paths, shader files, thread groups from [numthreads]
	{
		const FPipelineReferences References = MakeRuleReferences();
		FPipelineBind Run;
		BindPipeline(Run, Dsp(TEXT(R"DSP(
buffer Mask : R8;
buffer Blurred : R8(Scale = 0.5);

pass DrawMask : mesh
{
    Injection = AfterOpaque;
    Filter = Layer(Highlight);
    Material = "M_Mask";
    write Output0 = Mask;
}

pass Blur : compute
{
    Injection = AfterOpaque;
    Shader = "Blur.usf";
    Entry = WideCS;
    read Source = Mask;
    write Result = Blurred;
}

pass Virt : compute
{
    Injection = AfterOpaque;
    Shader = "/Project/Passes/Virtual.usf";
    Entry = VirtCS;
    read Source = Blurred;
    write Result = Mask;
}

pass Composite : fullscreen
{
    Material = "PP_Two";
    read Mask;
    read Blurred;
    write SceneColor;
}
)DSP")), TEXT("BindReferences.dsp"), &References);

		if (TestTrue(FString::Printf(TEXT("the references case binds (%s)"), *Run.Describe()), Run.HasBound() && !Run.HasErrors()))
		{
			const IR::FIRPassPipeline& Payload = Run.Payload();
			TestFalse(TEXT("a bind with references says nothing of what it could not check"), Run.Has(TEXT("DSH7360")));
			TestTrue(TEXT("the bound pipeline points at what it was checked against"), Run.Pipeline().References == &Run.References);
			if (const IR::FIRPass* Draw = FindPayloadPass(Payload, TEXT("DrawMask")))
			{
				ExpectString(*this, TEXT("a mesh pass's material resolves to its object path"), Draw->MaterialObjectPath, TEXT("/Game/Rules/M_Mask.M_Mask"));
				ExpectString(*this, TEXT("and keeps the reference as written"), Draw->MaterialReference, TEXT("M_Mask"));
			}
			if (const IR::FIRPass* Blur = FindPayloadPass(Payload, TEXT("Blur")))
			{
				ExpectString(*this, TEXT("a relative shader resolves to its file"), Blur->ShaderFilePath, TEXT("D:/Rules/Passes/Blur.usf"));
				TestTrue(TEXT("and has no virtual path"), Blur->ShaderVirtualPath.IsEmpty());
				TestTrue(TEXT("its thread group is the entry's [numthreads]"), Blur->ThreadsX == 16 && Blur->ThreadsY == 16 && Blur->ThreadsZ == 1 && !Blur->bThreadsWritten);
			}
			if (const IR::FIRPass* Virt = FindPayloadPass(Payload, TEXT("Virt")))
			{
				ExpectString(*this, TEXT("a virtual shader path is kept"), Virt->ShaderVirtualPath, TEXT("/Project/Passes/Virtual.usf"));
				TestTrue(TEXT("and its thread group read"), Virt->ThreadsX == 4 && Virt->ThreadsY == 4 && Virt->ThreadsZ == 4);
			}
			if (const IR::FIRPass* Composite = FindPayloadPass(Payload, TEXT("Composite")))
			{
				ExpectString(*this, TEXT("a fullscreen material resolves too"), Composite->MaterialObjectPath, TEXT("/Game/Rules/PP_Two.PP_Two"));
			}
		}
	}

	// ---- on an engine without the Custom Pass runtime: said once, and the facts it lacks are not checked
	{
		const FPipelineReferences References = MakeRuleReferences(/*bCustomPassAvailable*/ false);
		FPipelineBind Run;
		BindPipeline(Run, TEXT("buffer Mask : R8;\n\npass Draw : mesh\n{\n    Injection = AfterOpaque;\n    Filter = Stencil(1);\n    Material = \"M_NoOutput\";\n    write Output0 = Mask;\n}\n"), TEXT("BindOldEngine.dsp"), &References);
		TestTrue(TEXT("DSH7360 says the facts were not read"), Run.Has(TEXT("DSH7360"), ELangSeverity::Info));
		TestFalse(TEXT("and a material without UE.DreamPassOutput is not refused for it"), Run.Has(TEXT("DSH7340")));
	}

	// ---- a pipeline outside a `.dsp`
	{
		FSourceBind Run;
		BindSource(Run, TEXT("Pragma.dss"), TEXT("#pragma pipeline(Order = 1)\n\nexport void M_PL2Pragma(inout material m)\n{\n    m.EmissiveColor = float3(1.0, 0.0, 0.0);\n}\n"), Tests::GetDreamShaderTestBuiltinCatalog());
		TestTrue(FString::Printf(TEXT("#pragma pipeline in a .dss is DSH3311 (%s)"), *Run.Describe()), Run.Has(TEXT("DSH3311"), ELangSeverity::Error));
	}
	{
		// What the parser never makes outside a `.dsp`: a buffer declaration in a `.dss` tree, as an include of a `.dsp` or a
		// hand-built tree would bring it.
		FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(TEXT("Built.dss"), TEXT("export void M_PL2Built(inout material m)\n{\n    m.EmissiveColor = float3(1.0, 0.0, 0.0);\n}\n")));
		if (TestTrue(TEXT("the .dss parses"), Parsed.Succeeded()))
		{
			TUniquePtr<FBufferDecl> Buffer = MakeUnique<FBufferDecl>();
			Buffer->Name = TEXT("Mask");
			Buffer->Format = TEXT("R8");
			Parsed.Module->Declarations.Emplace(MoveTemp(Buffer));

			FBindOptions Options;
			Options.Catalog = &Tests::GetDreamShaderTestBuiltinCatalog();
			const FLangBindResult Bound = BindDreamShaderLang(*Parsed.Module, Options);
			TestTrue(FString::Printf(TEXT("a buffer declaration reaching a .dss binder is DSH3312 (%s)"), *DescribeDiagnostics(Bound.Diagnostics)), HasCode(Bound.Diagnostics, TEXT("DSH3312"), ELangSeverity::Error));
		}
	}

	// ---- V8: the binder packs `param` values the way the runtime does (UE::DreamPass::LayoutSlotParameters)
	{
		static const TCHAR* const Values[] =
		{
			TEXT("1.0"), TEXT("float2(1.0, 2.0)"), TEXT("float3(1.0, 2.0, 3.0)"), TEXT("float4(1.0, 2.0, 3.0, 4.0)"), TEXT("3"), TEXT("true"),
		};
		static const EDreamPassParameterType RuntimeTypes[] =
		{
			EDreamPassParameterType::Float, EDreamPassParameterType::Float2, EDreamPassParameterType::Float3,
			EDreamPassParameterType::Float4, EDreamPassParameterType::Int, EDreamPassParameterType::Bool,
		};

		uint32 State = 0x2545F491u;
		const auto Next = [&State]()
		{
			State = State * 1664525u + 1013904223u;
			return State >> 8;
		};

		int32 Fitting = 0;
		int32 Overflowing = 0;
		for (int32 Sequence = 0; Sequence < 160; ++Sequence)
		{
			const int32 Count = 1 + static_cast<int32>(Next() % 34u);
			TArray<FString> Params;
			TArray<FName> Names;
			TArray<EDreamPassParameterType> Types;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const int32 Type = static_cast<int32>(Next() % UE_ARRAY_COUNT(Values));
				Params.Add(Values[Type]);
				Names.Add(FName(*FString::Printf(TEXT("P%d"), Index)));
				Types.Add(RuntimeTypes[Type]);
			}

			FPipelineBind Run;
			BindPipeline(Run, MakePackingSource(Params), TEXT("Packing.dsp"));
			const bool bBinderFits = !Run.Has(TEXT("DSH7347"));
			TArray<FDreamPassSlotParamLocation> Locations;
			const bool bRuntimeFits = UE::DreamPass::LayoutSlotParameters(Names, Types, Locations);
			if (bBinderFits != bRuntimeFits)
			{
				AddError(FString::Printf(TEXT("the binder says %s and the runtime %s for: %s"),
					bBinderFits ? TEXT("fits") : TEXT("does not fit"), bRuntimeFits ? TEXT("fits") : TEXT("does not fit"), *FString::Join(Params, TEXT("; "))));
			}
			(bRuntimeFits ? Fitting : Overflowing) += 1;
		}
		TestTrue(TEXT("the sequences hold some that fit"), Fitting > 0);
		TestTrue(TEXT("and some that do not"), Overflowing > 0);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Rules: one sub-test per case
// ---------------------------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderLangPipelineRulesTest,
	"DreamShader.Lang2.Pipeline.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderLangPipelineRulesTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	using namespace UE::DreamShader::Editor::Private::LangPipelineTests;
	for (const FPipelineRuleCase& Case : GetPipelineRuleCases())
	{
		OutBeautifiedNames.Add(Case.Name);
		OutTestCommands.Add(Case.Name);
	}
}

bool FDreamShaderLangPipelineRulesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangPipelineTests;

	const FPipelineRuleCase* Case = GetPipelineRuleCases().FindByPredicate([&Parameters](const FPipelineRuleCase& Candidate)
	{
		return Candidate.Name.Equals(Parameters, ESearchCase::CaseSensitive);
	});
	if (!Case)
	{
		AddError(FString::Printf(TEXT("No pipeline rule case is named '%s'."), *Parameters));
		return false;
	}

	FPipelineReferences References;
	const FPipelineReferences* ReferencesToUse = nullptr;
	if (Case->References != EPipelineRuleReferences::None)
	{
		References = MakeRuleReferences(Case->References == EPipelineRuleReferences::Rules);
		ReferencesToUse = &References;
	}

	FPipelineBind Run;
	BindPipeline(Run, Case->Source, TEXT("Rules.dsp"), ReferencesToUse);
	if (!Run.HasBound())
	{
		AddError(FString::Printf(TEXT("[%s] the source did not get as far as the binder: %s"), *Case->Name, *Run.Describe()));
		return false;
	}

	const TCHAR* Code = *Case->Code;
	switch (Case->Expect)
	{
	case EPipelineRuleExpect::Raised:
		TestTrue(FString::Printf(TEXT("%s is raised as an error"), Code), Run.Has(Code, ELangSeverity::Error));
		break;
	case EPipelineRuleExpect::Warned:
		TestTrue(FString::Printf(TEXT("%s is raised as a warning"), Code), Run.Has(Code, ELangSeverity::Warning));
		break;
	case EPipelineRuleExpect::Noted:
		TestTrue(FString::Printf(TEXT("%s is raised as an info"), Code), Run.Has(Code, ELangSeverity::Info));
		break;
	case EPipelineRuleExpect::Clean:
		TestFalse(TEXT("no error"), Run.HasErrors());
		TestFalse(FString::Printf(TEXT("and no %s"), Code), Run.Has(Code));
		break;
	case EPipelineRuleExpect::ErrorOnLine:
	{
		const int32 Line = FindLineOf(Case->Source, Case->LineMarker);
		const bool bErrorThere = Run.Bind.Diagnostics.GetDiagnostics().ContainsByPredicate([Line, Code](const FLangDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == ELangSeverity::Error && Diagnostic.Span.Line == Line && Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive);
		});
		TestTrue(FString::Printf(TEXT("%s is raised as an error on line %d ('%s')"), Code, Line, *Case->LineMarker), bErrorThere);
		break;
	}
	}

	if (HasAnyErrors())
	{
		AddInfo(FString::Printf(TEXT("[%s] diagnostics: %s"), *Case->Name, *Run.Describe()));
		AddInfo(FString::Printf(TEXT("[%s] source:\n%s"), *Case->Name, *Case->Source));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Compare
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLangPipelineCompareTest,
	"DreamShader.Lang2.Pipeline.Compare",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLangPipelineCompareTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangPipelineTests;

	FPipelineBind Run;
	BindPipeline(Run, Dsp(TEXT(R"DSP(
#pragma pipeline(Order = 2)

uniform float Gain = 0.1;
/// @desc Mood.
uniform Texture2D Noise;

buffer Mask : R8(Scale = 0.1);
buffer Soft : R8;

pass Draw : mesh
{
    Injection = AfterOpaque;
    Filter = Layer(A);
    Material = "M_Mask";
    write Output0 = Mask;
}

pass Blur : compute
{
    Shader = "Blur.usf";
    Entry = BlurCS;
    read Source = Mask;
    write Result = Soft;
    param Gain = Gain * 0.1;
    param Flip = 0.0 - Gain;
}

pass Show : fullscreen
{
    Material = "PP_Show";
    read Soft;
    write SceneColor;
}
)DSP")), TEXT("Compare.dsp"));
	if (!TestTrue(FString::Printf(TEXT("the base payload binds (%s)"), *Run.Describe()), Run.HasBound() && !Run.HasErrors()))
	{
		return false;
	}
	const IR::FIRPassPipeline& Base = Run.Payload();

	const auto Same = [this](const TCHAR* What, const IR::FIRPassPipeline& A, const IR::FIRPassPipeline& B)
	{
		ExpectSamePipeline(*this, What, A, B);
	};
	const auto Differ = [this](const TCHAR* What, const IR::FIRPassPipeline& A, const IR::FIRPassPipeline& B, const int32 ExpectedDifferences)
	{
		TArray<FString> Differences;
		TestFalse(What, CompareDreamShaderPipelines(A, B, &Differences));
		if (ExpectedDifferences > 0)
		{
			TestEqual(FString::Printf(TEXT("%s: one line per difference"), What), Differences.Num(), ExpectedDifferences);
		}
	};

	TArray<FString> NoDifferences;
	TestTrue(TEXT("a payload is itself"), CompareDreamShaderPipelines(Base, Base, &NoDifferences));
	TestEqual(TEXT("with nothing to say"), NoDifferences.Num(), 0);

	// ---- numbers at float32
	{
		IR::FIRPassPipeline Other = Base;
		Other.Buffers[0].Scale = static_cast<double>(static_cast<float>(0.1));
		Same(TEXT("0.1 and the float32 nearest it are one scale"), Base, Other);
		Other.Buffers[0].Scale = 0.1000001;
		Differ(TEXT("0.1000001 is another scale"), Base, Other, 1);
	}
	{
		IR::FIRPassPipeline Other = Base;
		Other.Parameters[0].Default = MakeFloats({ static_cast<double>(static_cast<float>(0.1)) });
		Same(TEXT("a parameter default at float32"), Base, Other);
	}

	// ---- descriptions as an asset holds them
	{
		IR::FIRPassPipeline A = Base;
		IR::FIRPassPipeline B = Base;
		A.Parameters[1].Description = TEXT("Mood.");
		B.Parameters[1].Description = TEXT("  Mood.\r\n");
		Same(TEXT("a description's surrounding blanks do not count"), A, B);
		A.Parameters[1].Description = TEXT("Mood\nTwo");
		B.Parameters[1].Description = TEXT("Mood\r\nTwo");
		Same(TEXT("nor its line terminators"), A, B);
		B.Parameters[1].Description = TEXT("Other");
		Differ(TEXT("its words do"), A, B, 1);
	}

	// ---- sets of flags, and values at their effective defaults
	{
		IR::FIRPassPipeline Other = Base;
		Other.Views = { TEXT("Editor"), TEXT("Game") };
		Same(TEXT("Game | Editor in any order is the default set of views"), Base, Other);
		Other.Views = { TEXT("Game") };
		Differ(TEXT("Game alone is not"), Base, Other, 1);
	}
	{
		IR::FIRPassPipeline A = Base;
		IR::FIRPassPipeline B = Base;
		A.Requires = { TEXT("CustomStencil"), TEXT("PostProcess") };
		B.Requires = { TEXT("PostProcess"), TEXT("CustomStencil") };
		Same(TEXT("requirements in any order"), A, B);
		Differ(TEXT("a requirement more is a difference"), Base, B, 1);
	}
	{
		IR::FIRPassPipeline Other = Base;
		IR::FIRPass& Draw = Other.Passes[0];
		Draw.MeshMode.Reset();
		Draw.Nanite.Reset();
		Draw.Depth.Reset();
		Draw.Cull.Reset();
		Draw.Blend.Reset();
		Draw.Usage = { TEXT("StaticMesh"), TEXT("InstancedStaticMeshes"), TEXT("SkeletalMesh") };
		Same(TEXT("a mesh pass's keys at their effective defaults"), Base, Other);
	}
	{
		IR::FIRPassPipeline Other = Base;
		Other.Passes[2].Reads[0].Slot.Reset();
		Same(TEXT("an empty slot is the buffer's own name"), Base, Other);
	}

	// ---- what a comparison ignores: where things were written, and whether a default was spelled out
	{
		IR::FIRPassPipeline Other = Base;
		Other.Passes[0].bInjectionWritten = !Other.Passes[0].bInjectionWritten;
		Other.Passes[0].Source.File = TEXT("Elsewhere.dsp");
		Other.Buffers[0].bResolutionWritten = !Other.Buffers[0].bResolutionWritten;
		Other.Passes[1].bThreadsWritten = !Other.Passes[1].bThreadsWritten;
		Other.Parameters[0].Source.Span.Line = 99;
		Same(TEXT("source references and the written flags do not count"), Base, Other);
	}

	// ---- assets and files, each by the identity the engine gives them
	{
		IR::FIRPassPipeline A = Base;
		IR::FIRPassPipeline B = Base;
		A.Passes[0].MaterialObjectPath = TEXT("/Game/X/M_Mask.M_Mask");
		B.Passes[0].MaterialObjectPath = TEXT("/game/x/m_mask.m_mask");
		Same(TEXT("an object path in another case is the same material"), A, B);
		B.Passes[0].MaterialObjectPath = TEXT("/Game/X/M_Other.M_Other");
		Differ(TEXT("another object path is another material"), A, B, 1);
	}
	{
		IR::FIRPassPipeline A = Base;
		IR::FIRPassPipeline B = Base;
		A.Passes[1].ShaderFilePath = TEXT("C:/Project/DShader/Passes/../Blur.usf");
		B.Passes[1].ShaderFilePath = TEXT("C:/Project/DShader/Blur.usf");
		Same(TEXT("one shader file spelled two ways"), A, B);
		B.Passes[1].ShaderFilePath = TEXT("C:/Project/DShader/Other.usf");
		Differ(TEXT("another file"), A, B, 1);
	}
	{
		IR::FIRPassPipeline A = Base;
		IR::FIRPassPipeline B = Base;
		A.Parameters[1].Default = IR::FIRPropertyValue::MakeObject(TEXT("/Game/T/Noise"));
		B.Parameters[1].Default = IR::FIRPropertyValue::MakeObject(TEXT("/Game/T/Noise.Noise"));
		Same(TEXT("a package path and its object path are one texture"), A, B);
		A.Parameters[1].Default = IR::FIRPropertyValue::MakeObject(TEXT("None"));
		B.Parameters[1].Default = IR::FIRPropertyValue::MakeObject(FString());
		Same(TEXT("`None` is no texture"), A, B);
	}
	{
		IR::FIRPassPipeline Other = Base;
		Other.Passes[1].Params[0].ConstantType.Reset();
		Same(TEXT("a param's constant type matters only for a constant"), Base, Other);
	}

	// ---- -0 and +0 are one float32. `0.0 - Gain` folds to an offset of +0, its decompiled `Gain * -1.0` to one of -0;
	// the comparison formats the offset as text and tells the two apart (KeepEquivalentPipelineSpellings hides it from the
	// decompiler's own check, a direct comparison does not).
	{
		IR::FIRPassPipeline Other = Base;
		const IR::FIRPassParam& Flip = Base.Passes[1].Params[1];
		TestTrue(TEXT("`0.0 - Gain` folds to Gain * -1 + 0"), Flip.Multiplier == -1.0 && Flip.Offset == 0.0);
		Other.Passes[1].Params[1].Offset = -0.0;
		Same(TEXT("an offset of -0 is one of +0"), Base, Other);
	}

	// ---- shape
	{
		IR::FIRPassPipeline Other = Base;
		Other.Passes[2].Name = TEXT("Present");
		Differ(TEXT("a pass of another name at the same place"), Base, Other, 1);
	}
	{
		IR::FIRPassPipeline Other = Base;
		Other.Passes.RemoveAt(2);
		Differ(TEXT("a pass fewer"), Base, Other, 0);
	}
	{
		IR::FIRPassPipeline Other = Base;
		Other.DefaultInjection = TEXT("AfterOpaque");
		Differ(TEXT("another default injection point"), Base, Other, 1);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Rewrite
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLangPipelineRewriteTest,
	"DreamShader.Lang2.Pipeline.Rewrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLangPipelineRewriteTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangPipelineTests;

	const FString Original = Lines({
		TEXT("// The pipeline Adopt edits."),
		TEXT("#pragma pipeline(Order = 1) // order comment"),
		TEXT(""),
		TEXT("uniform bool On = true;"),
		TEXT("/// @group Look"),
		TEXT("uniform float Gain = 2.0; // gain comment"),
		TEXT("uniform float3 Tint = float3(1.0, 0.5, 0.25);"),
		TEXT(""),
		TEXT("buffer Mask : R8(Clear = 0); // mask comment"),
		TEXT("buffer Soft : R8(Scale = 0.5);"),
		TEXT(""),
		TEXT("pass First : clear"),
		TEXT("{"),
		TEXT("    Injection = AfterOpaque; // where it runs"),
		TEXT("    Value = 1.0;"),
		TEXT("    write Mask;"),
		TEXT("}"),
		TEXT(""),
		TEXT("// Between the passes."),
		TEXT("pass Second : copy"),
		TEXT("{"),
		TEXT("    Injection = AfterOpaque;"),
		TEXT("    read Mask;"),
		TEXT("    write Soft;"),
		TEXT("}"),
	});

	/** Binds Text, lets Edit change its payload, rewrites; checks the edits against the text and the text against Expected. */
	const auto Rewrite = [this](const TCHAR* What, const FString& Text, const TFunctionRef<void(IR::FIRPassPipeline&)> Edit, const FString& Expected) -> bool
	{
		FPipelineBind Run;
		BindPipeline(Run, Text, TEXT("Rewrite.dsp"));
		if (!TestTrue(FString::Printf(TEXT("%s: the file binds (%s)"), What, *Run.Describe()), Run.HasBound() && !Run.HasErrors()))
		{
			return false;
		}
		IR::FIRPassPipeline Desired = Run.Payload();
		Edit(Desired);

		TArray<FLangSourceEdit> Edits;
		FString Rewritten;
		FLangDiagnosticSink Diagnostics;
		const bool bRewritten = RewriteDreamShaderPipelineSource(FLangSourceText(TEXT("Rewrite.dsp"), Text), *Run.Parse.Module, Run.Module(), Desired, Edits, Rewritten, Diagnostics);
		if (!TestTrue(FString::Printf(TEXT("%s: the rewrite succeeds (%s)"), What, *DescribeDiagnostics(Diagnostics)), bRewritten))
		{
			return false;
		}

		FString Applied;
		TestTrue(FString::Printf(TEXT("%s: the edits are sorted and do not overlap"), What), ApplyEdits(Text, Edits, Applied));
		TestTrue(FString::Printf(TEXT("%s: applying them gives the text"), What), Applied.Equals(Rewritten, ESearchCase::CaseSensitive));
		ExpectText(*this, FString::Printf(TEXT("%s: the text"), What), Rewritten, Expected);

		// The text means what was asked for.
		FPipelineBind Reread;
		BindPipeline(Reread, Rewritten, TEXT("Rewrite.dsp"));
		if (TestTrue(FString::Printf(TEXT("%s: the rewritten file binds (%s)"), What, *Reread.Describe()), Reread.HasBound() && !Reread.HasErrors()))
		{
			ExpectSamePipeline(*this, FString::Printf(TEXT("%s: the rewritten file states the payload"), What), Desired, Reread.Payload());
		}
		return true;
	};

	// ---- a value spliced over its own text; the comment on its line stays
	Rewrite(TEXT("a pragma value"), Original, [](IR::FIRPassPipeline& Desired) { Desired.Order = 5; },
		ReplaceOnce(Original, TEXT("#pragma pipeline(Order = 1) // order comment\n"), TEXT("#pragma pipeline(Order = 5) // order comment\n")));
	Rewrite(TEXT("a parameter default"), Original, [](IR::FIRPassPipeline& Desired) { Desired.Parameters[1].Default = MakeFloats({ 3.0 }); },
		ReplaceOnce(Original, TEXT("uniform float Gain = 2.0; // gain comment\n"), TEXT("uniform float Gain = 3.0; // gain comment\n")));
	Rewrite(TEXT("a clear value"), Original, [](IR::FIRPassPipeline& Desired)
		{
			IR::FIRPass& First = Desired.Passes[0];
			First.ClearValue[0] = 1.0;
			First.ClearValue[1] = 0.0;
			First.ClearValue[2] = 0.0;
			First.ClearValue[3] = 1.0;
		},
		ReplaceOnce(Original, TEXT("    Value = 1.0;\n"), TEXT("    Value = float4(1.0, 0.0, 0.0, 1.0);\n")));

	// ---- a value equal at float32 is no change: nothing is touched
	{
		FPipelineBind Run;
		BindPipeline(Run, Original, TEXT("Rewrite.dsp"));
		if (Run.HasBound())
		{
			IR::FIRPassPipeline Desired = Run.Payload();
			Desired.Parameters[1].Default = MakeFloats({ 2.0000000001 });
			TArray<FLangSourceEdit> Edits;
			FString Rewritten;
			FLangDiagnosticSink Diagnostics;
			TestTrue(TEXT("an unchanged float32 rewrites"), RewriteDreamShaderPipelineSource(FLangSourceText(TEXT("Rewrite.dsp"), Original), *Run.Parse.Module, Run.Module(), Desired, Edits, Rewritten, Diagnostics));
			TestEqual(TEXT("with no edit"), Edits.Num(), 0);
			ExpectText(*this, TEXT("and the text as it was"), Rewritten, Original);
		}
	}

	// ---- a key back at the binder's default goes -- with its line -- unless the payload says it was written
	Rewrite(TEXT("an injection point back at the default"), Original, [](IR::FIRPassPipeline& Desired)
		{
			Desired.Passes[0].Injection = TEXT("BeforePostProcess");
			Desired.Passes[0].bInjectionWritten = false;
		},
		ReplaceOnce(Original, TEXT("    Injection = AfterOpaque; // where it runs\n"), TEXT("")));
	Rewrite(TEXT("an injection point at the default, written"), Original, [](IR::FIRPassPipeline& Desired)
		{
			Desired.Passes[0].Injection = TEXT("BeforePostProcess");
			Desired.Passes[0].bInjectionWritten = true;
		},
		ReplaceOnce(Original, TEXT("    Injection = AfterOpaque; // where it runs\n"), TEXT("    Injection = BeforePostProcess; // where it runs\n")));

	// ---- a new key goes after the pass's last setting
	const FString EnabledInserted = ReplaceOnce(Original, TEXT("    Value = 1.0;\n    write Mask;\n"), TEXT("    Value = 1.0;\n    Enabled = On;\n    write Mask;\n"));
	Rewrite(TEXT("a new key"), Original, [](IR::FIRPassPipeline& Desired) { Desired.Passes[0].EnabledParameter = TEXT("On"); }, EnabledInserted);

	// ---- a new declaration goes after the one before it, or before the first of its group
	{
		IR::FIRPass Third;
		Third.Name = TEXT("Third");
		Third.Kind = TEXT("clear");
		Third.Injection = TEXT("AfterOpaque");
		Third.bInjectionWritten = true;
		Third.Writes.Add(MakeBinding(TEXT("Soft"), TEXT("Soft")));
		Rewrite(TEXT("a new pass"), Original, [&Third](IR::FIRPassPipeline& Desired) { Desired.Passes.Insert(Third, 1); },
			ReplaceOnce(Original, TEXT("    write Mask;\n}\n\n// Between"), TEXT("    write Mask;\n}\n\npass Third : clear\n{\n    Injection = AfterOpaque;\n    write Soft;\n}\n\n// Between")));
	}
	{
		IR::FIRPassBuffer Glow = MakeBuffer(TEXT("Glow"), TEXT("R16F"));
		Glow.bExport = true;
		Rewrite(TEXT("a new buffer"), Original, [&Glow](IR::FIRPassPipeline& Desired) { Desired.Buffers.Add(Glow); },
			ReplaceOnce(Original, TEXT("buffer Soft : R8(Scale = 0.5);\n"), TEXT("buffer Soft : R8(Scale = 0.5);\nbuffer Glow : R16F(Export = true);\n")));
	}
	Rewrite(TEXT("a new first parameter"), Original, [](IR::FIRPassPipeline& Desired) { Desired.Parameters.Insert(MakeParameter(TEXT("Strength"), TEXT("float"), MakeFloats({ 0.5 })), 0); },
		ReplaceOnce(Original, TEXT("uniform bool On = true;\n"), TEXT("uniform float Strength = 0.5;\nuniform bool On = true;\n")));

	// ---- a declaration that goes takes its lines; a comment above it is the author's and stays
	Rewrite(TEXT("a pass removed"), Original, [](IR::FIRPassPipeline& Desired) { Desired.Passes.RemoveAt(1); },
		ReplaceOnce(Original, TEXT("pass Second : copy\n{\n    Injection = AfterOpaque;\n    read Mask;\n    write Soft;\n}\n"), TEXT("")));

	// ---- the file's own line terminator
	Rewrite(TEXT("a new key in a CRLF file"), Original.Replace(TEXT("\n"), TEXT("\r\n")), [](IR::FIRPassPipeline& Desired) { Desired.Passes[0].EnabledParameter = TEXT("On"); },
		EnabledInserted.Replace(TEXT("\n"), TEXT("\r\n")));

	// ---- what cannot be rewritten
	{
		const FString Shared = Lines({
			TEXT("uniform float A = 1.0, B = 2.0;"),
			TEXT(""),
			TEXT("buffer Mask : R8;"),
			TEXT(""),
			TEXT("pass Fill : clear"),
			TEXT("{"),
			TEXT("    write Mask;"),
			TEXT("}"),
		});
		FPipelineBind Run;
		BindPipeline(Run, Shared, TEXT("Shared.dsp"));
		if (TestTrue(FString::Printf(TEXT("the shared declaration binds (%s)"), *Run.Describe()), Run.HasBound() && !Run.HasErrors()))
		{
			IR::FIRPassPipeline Desired = Run.Payload();
			Desired.Parameters[0].Default = MakeFloats({ 3.0 });
			TArray<FLangSourceEdit> Edits;
			FString Rewritten;
			FLangDiagnosticSink Diagnostics;
			TestFalse(TEXT("a value in a `uniform a, b;` statement cannot be spliced"), RewriteDreamShaderPipelineSource(FLangSourceText(TEXT("Shared.dsp"), Shared), *Run.Parse.Module, Run.Module(), Desired, Edits, Rewritten, Diagnostics));
			TestTrue(TEXT("DSH9107"), HasCode(Diagnostics, TEXT("DSH9107"), ELangSeverity::Error));
			TestEqual(TEXT("no edit is handed out"), Edits.Num(), 0);
			ExpectText(*this, TEXT("and the text is the file's"), Rewritten, Shared);
		}
	}
	{
		const FString Material = TEXT("export void M_PL2Rewrite(inout material m)\n{\n    m.EmissiveColor = float3(1.0, 0.0, 0.0);\n}\n");
		FSourceBind Run;
		BindSource(Run, TEXT("Rewrite.dss"), Material, Tests::GetDreamShaderTestBuiltinCatalog());
		if (TestTrue(TEXT("the .dss binds"), Run.Bind.Bound.IsValid() && Run.Parse.Module.IsValid()))
		{
			TArray<FLangSourceEdit> Edits;
			FString Rewritten;
			FLangDiagnosticSink Diagnostics;
			TestFalse(TEXT("a .dss is no pipeline to rewrite"), RewriteDreamShaderPipelineSource(FLangSourceText(TEXT("Rewrite.dss"), Material), *Run.Parse.Module, *Run.Bind.Bound, IR::FIRPassPipeline(), Edits, Rewritten, Diagnostics));
			TestTrue(TEXT("DSH9109"), HasCode(Diagnostics, TEXT("DSH9109"), ELangSeverity::Error));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// PassNodes: UE.DreamPassOutput and UE.DreamPassBuffer in a `.dss`
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLangPipelinePassNodesTest,
	"DreamShader.Lang2.Pipeline.PassNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLangPipelinePassNodesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangPipelineTests;

	const FString OneOutput = Lines({
		TEXT("export void M_PL2Output(inout material m)"),
		TEXT("{"),
		TEXT("    m.EmissiveColor = float3(0.0, 0.0, 0.0);"),
		TEXT("    UE.DreamPassOutput(Output0 = float4(1.0, 0.0, 0.0, 0.0));"),
		TEXT("}"),
	});
	const FString ReadsABuffer = Lines({
		TEXT("export void M_PL2Read(inout material m)"),
		TEXT("{"),
		TEXT("    float Glow = UE.DreamPassBuffer(Pipeline = \"CP_PL2Nodes\", Buffer = \"Glow\").r;"),
		TEXT("    m.EmissiveColor = float3(Glow, Glow, Glow);"),
		TEXT("}"),
	});

	// ---- DSH5300: a catalog without the classes (an engine older than 5.8) says what the node needs
	{
		FSourceBind Run;
		BindSource(Run, TEXT("M_PL2Output.dss"), OneOutput, Tests::GetDreamShaderTestBuiltinCatalog());
		TestTrue(FString::Printf(TEXT("UE.DreamPassOutput without its class is DSH5300 (%s)"), *Run.Describe()), Run.Has(TEXT("DSH5300"), ELangSeverity::Error));
	}
	{
		FSourceBind Run;
		BindSource(Run, TEXT("M_PL2Read.dss"), ReadsABuffer, Tests::GetDreamShaderTestBuiltinCatalog());
		TestTrue(FString::Printf(TEXT("UE.DreamPassBuffer without its class is DSH5300 (%s)"), *Run.Describe()), Run.Has(TEXT("DSH5300"), ELangSeverity::Error));
	}

	// ---- with the classes: one output node in a material is what a mesh pass draws
	{
		FSourceBind Run;
		BindSource(Run, TEXT("M_PL2Output.dss"), OneOutput, GetPassNodeCatalog());
		TestFalse(FString::Printf(TEXT("one UE.DreamPassOutput binds (%s)"), *Run.Describe()), Run.HasErrors());
	}
	{
		FSourceBind Run;
		BindSource(Run, TEXT("M_PL2Read.dss"), ReadsABuffer, GetPassNodeCatalog());
		TestFalse(FString::Printf(TEXT("a UE.DreamPassBuffer read binds (%s)"), *Run.Describe()), Run.HasErrors());
	}

	// ---- DSH5303: a second output node, however it comes about
	{
		FSourceBind Run;
		BindSource(Run, TEXT("M_PL2Twice.dss"), Lines({
			TEXT("export void M_PL2Twice(inout material m)"),
			TEXT("{"),
			TEXT("    m.EmissiveColor = float3(0.0, 0.0, 0.0);"),
			TEXT("    UE.DreamPassOutput(Output0 = float4(1.0, 0.0, 0.0, 0.0));"),
			TEXT("    UE.DreamPassOutput(Output1 = float4(0.0, 1.0, 0.0, 0.0));"),
			TEXT("}"),
		}), GetPassNodeCatalog());
		TestTrue(FString::Printf(TEXT("two calls are DSH5303 (%s)"), *Run.Describe()), Run.Has(TEXT("DSH5303"), ELangSeverity::Error));
	}
	{
		FSourceBind Run;
		BindSource(Run, TEXT("M_PL2Helper.dss"), Lines({
			TEXT("void WriteMask(inout material m, float Value)"),
			TEXT("{"),
			TEXT("    m.EmissiveColor = float3(Value, 0.0, 0.0);"),
			TEXT("    UE.DreamPassOutput(Output0 = float4(Value, 0.0, 0.0, 0.0));"),
			TEXT("}"),
			TEXT(""),
			TEXT("export void M_PL2Helper(inout material m)"),
			TEXT("{"),
			TEXT("    WriteMask(m, 1.0);"),
			TEXT("    WriteMask(m, 0.5);"),
			TEXT("}"),
		}), GetPassNodeCatalog());
		TestTrue(FString::Printf(TEXT("a helper inlined twice is DSH5303 (%s)"), *Run.Describe()), Run.Has(TEXT("DSH5303"), ELangSeverity::Error));
	}
	{
		FSourceBind Run;
		BindSource(Run, TEXT("M_PL2Loop.dss"), Lines({
			TEXT("export void M_PL2Loop(inout material m)"),
			TEXT("{"),
			TEXT("    m.EmissiveColor = float3(0.0, 0.0, 0.0);"),
			TEXT("    for (int i = 0; i < 2; ++i)"),
			TEXT("    {"),
			TEXT("        UE.DreamPassOutput(Output0 = float4(1.0, 0.0, 0.0, 0.0));"),
			TEXT("    }"),
			TEXT("}"),
		}), GetPassNodeCatalog());
		TestTrue(FString::Printf(TEXT("a call in a loop is DSH5303 (%s)"), *Run.Describe()), Run.Has(TEXT("DSH5303"), ELangSeverity::Error));
	}

	// ---- DSH5302: in a material function the node does nothing (a warning)
	{
		FSourceBind Run;
		BindSource(Run, TEXT("MF_PL2Out.dss"), Lines({
			TEXT("export float3 MF_PL2Out(float3 Color)"),
			TEXT("{"),
			TEXT("    UE.DreamPassOutput(Output0 = float4(Color, 1.0));"),
			TEXT("    return Color;"),
			TEXT("}"),
		}), GetPassNodeCatalog());
		TestTrue(FString::Printf(TEXT("UE.DreamPassOutput in a material function is DSH5302 (%s)"), *Run.Describe()), Run.Has(TEXT("DSH5302"), ELangSeverity::Warning));
		TestFalse(TEXT("and only a warning"), Run.Has(TEXT("DSH5302"), ELangSeverity::Error));
	}

	// ---- DSH5301: the new material translator compiles neither node
	{
		FSourceBind Run;
		BindSource(Run, TEXT("M_PL2NewTranslator.dss"), TEXT("#pragma material(bEnableNewHLSLGenerator = true)\n\n") + OneOutput, GetPassNodeCatalog());
		TestTrue(FString::Printf(TEXT("UE.DreamPassOutput with the new translator is DSH5301 (%s)"), *Run.Describe()), Run.Has(TEXT("DSH5301"), ELangSeverity::Error));
	}
	{
		FSourceBind Run;
		BindSource(Run, TEXT("M_PL2NewTranslatorRead.dss"), TEXT("#pragma material(bEnableNewHLSLGenerator = true)\n\n") + ReadsABuffer, GetPassNodeCatalog());
		TestTrue(FString::Printf(TEXT("UE.DreamPassBuffer with the new translator is DSH5301 (%s)"), *Run.Describe()), Run.Has(TEXT("DSH5301"), ELangSeverity::Error));
	}
	{
		FSourceBind Run;
		BindSource(Run, TEXT("M_PL2OldTranslator.dss"), TEXT("#pragma material(bEnableNewHLSLGenerator = false)\n\n") + OneOutput, GetPassNodeCatalog());
		TestFalse(FString::Printf(TEXT("with the setting off there is no DSH5301 (%s)"), *Run.Describe()), Run.Has(TEXT("DSH5301")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
