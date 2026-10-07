// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Compiler2.* -- the 2.0 pipeline end to end, and the parity oracle against the 1.x
// generator it replaces.
//
// The smoke tests drive a `.dss` through the compiler service (CompileDreamShaderTestAssets, the test compile
// facade of DreamShaderTestCommon.h) and assert on the asset that came out: the graph, the provenance metadata,
// the source-span table, the skip-when-current rule, and the divergence refusal. They deliberately reuse the
// provenance helpers rather than reimplementing them: reusing the digest and the metadata is the whole shape of
// the 2.0 pipeline, and a test that accepted a second implementation of them would not notice if it had one.
//
// The parity tests used to compile the SAME material twice -- once from its 1.x `.dsm`/`.dsf` twin in the DShader
// roots and once from its 2.0 `.dss` in Tests/Corpus/Lang/Examples -- and diff the two dumps. The 1.x generator is
// gone, so nothing can compile the twin any more: each pair is now the 2.0 compile of the example
// against a compile golden under Tests/Corpus/Parity, and a filled golden is reviewed against the
// twin's frozen B2 dump (Saved/DreamShader/GraphBaseline/v2-6c2e0b6-formal) with Tools/Parity/graph_parity.py, which
// applies the registered normalisations the live comparison used to apply here.

#include "DreamShaderTestCommon.h"
#include "DreamShaderTestCorpusLayers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Commandlet/DreamShaderGraphDump.h"
#include "DreamShaderIREmitter.h"
#include "Decompiler/DreamShaderGraphDecompilerHelpers.h"
// ResolveDreamShaderInlineMask: what an inline mask means, shared with the decompiler's importer (A10).
#include "Decompiler/DreamShaderInlineMask.h"
#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"
#include "DreamShaderGeneratedAssetDigest.h"
#include "DreamShaderCompilerService.h"
#include "DreamShaderGeneratedAssets.h"

// The B5 / B7 smoke tests and the extern-to-layer compile.
#include "DreamShaderCompilePipeline.h"
#include "IR/IRCustomHlsl.h"
#include "Lang/LangDiagnostic.h"
#include "Tools/DreamShaderShaderCheck.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialFunction.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

// This file's own namespace: the module builds as a unity blob, so a helper defined here under the
// shared ...::Tests namespace would be a redefinition of the identically named one in
// DreamShaderAutomationTests.cpp rather than a local convenience.
namespace UE::DreamShader::Editor::Private::Compiler2Tests
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	// =============================================================================================
	// Sources
	// =============================================================================================

	/** The smoke material: a helper to inline, two uniforms, a reflected call, a swizzle. */
	inline FString MakeSmokeMaterialSource(const FString& AssetName)
	{
		return FString::Printf(TEXT(
			"// The end-to-end smoke material.\n"
			"#pragma material(ShadingModel = Unlit, BlendMode = Additive)\n"
			"\n"
			"/// @group Glow|Look @desc Multiplied on top of the particle colour\n"
			"uniform float4 Tint = float4(1, 1, 1, 1);\n"
			"\n"
			"/// @group Glow|Look @desc Overall emissive gain\n"
			"uniform float Intensity = 0.7;\n"
			"\n"
			"float GlowMask(float2 UV)\n"
			"{\n"
			"    float2 P = UV * 2.0 - 1.0;\n"
			"    return saturate(1.0 - dot(P, P));\n"
			"}\n"
			"\n"
			"export void %s(inout material m)\n"
			"{\n"
			"    float2 UV = UE.TextureCoordinate(CoordinateIndex = 0);\n"
			"    m.EmissiveColor = Tint.rgb * GlowMask(UV) * Intensity;\n"
			"}\n"), *AssetName);
	}

	inline FString MakeThinCustomMaterialSource(const FString& AssetName)
	{
		return FString::Printf(TEXT(
			"#pragma material(Backend = ThinCustom, ShadingModel = Unlit, BlendMode = Opaque)\n"
			"\n"
			"uniform float4 Tint = float4(1, 0.5, 0.25, 1);\n"
			"uniform float Boost = 0.5;\n"
			"\n"
			"export void %s(inout material m)\n"
			"{\n"
			"    m.EmissiveColor = Tint.rgb * Boost;\n"
			"}\n"), *AssetName);
	}

	/** A library: two exports, one calling the other, so the local FunctionCall path is exercised. */
	inline FString MakeFunctionLibrarySource(const FString& Prefix)
	{
		return FString::Printf(TEXT(
			"/// @desc Scales a UV pair.\n"
			"export float2 %s_Scale(float2 UV, float Scale = 1.0)\n"
			"{\n"
			"    return UV * Scale;\n"
			"}\n"
			"\n"
			"/// @desc Scales, then offsets.\n"
			"export float2 %s_ScaleOffset(float2 UV, float Scale = 1.0, float2 Offset = float2(0, 0))\n"
			"{\n"
			"    return %s_Scale(UV, Scale) + Offset;\n"
			"}\n"), *Prefix, *Prefix, *Prefix);
	}

	// =============================================================================================
	// Asset queries
	// =============================================================================================

	template <typename TExpression>
	int32 CountExpressionsOfClass(UMaterial* Material)
	{
		int32 Count = 0;
		if (Material)
		{
			for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
			{
				if (Cast<TExpression>(Expression.Get()) != nullptr)
				{
					++Count;
				}
			}
		}
		return Count;
	}

	/**
	 * The expression really driving a material property, with any named reroutes walked through.
	 *
	 * A reroute is a wire, not a value, so a test that stopped at one would be asserting on the
	 * routing rather than on the compiler. The hop limit is a guard, not a rule: a reroute chain
	 * longer than eight is itself a bug, and looping forever on a cyclic one would hang the run.
	 */
	inline UMaterialExpression* ResolveDrivingExpression(UMaterialExpression* Expression)
	{
		for (int32 Hop = 0; Hop < 8 && Expression; ++Hop)
		{
			UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression);
			if (!Usage || !Usage->Declaration)
			{
				break;
			}
			Expression = Usage->Declaration->Input.Expression;
		}
		return Expression;
	}

	/** One package metadata value, through the same version fork the generator writes it with. */
	inline FString GetAssetMetadata(UObject* Asset, const TCHAR* Key)
	{
		if (!Asset)
		{
			return FString();
		}
		UPackage* Package = Asset->GetOutermost();
		if (!Package)
		{
			return FString();
		}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
		return Package->GetMetaData().GetValue(Asset, Key);
#else
		if (UMetaData* MetaData = Package->GetMetaData())
		{
			return MetaData->GetValue(Asset, Key);
		}
		return FString();
#endif
	}

	// =============================================================================================
	// Inline masks of a foreign graph
	// =============================================================================================

	// What an inline mask MEANS is the decompiler's ResolveDreamShaderInlineMask (Decompiler/DreamShaderInlineMask.h):
	// the importer and this oracle read a foreign graph with one rule. What is left here is what only
	// an oracle does with the answer -- rewrite the asset the way the 2.0 emitter would have spelled the same swizzle.
	// Called by DreamShader.Compiler2.Smoke.InlineMaskNormalisation below; the parity tests stopped compiling the 1.x twin
	// in 2.0.

	/**
	 * Rewrites every inline mask of a 1.x graph the way the 2.0 emitter spells the same swizzle
	 * (Tools/Parity/README.md, PD-1): an identity mask disappears, a leading-channel mask moves the wire
	 * to the named output (TryResolveSwizzleAsNamedOutput, the emitter's own rule), and any other mask
	 * becomes one ComponentMask node per source, output and mask, which the emitter dedupes the same way.
	 * Only ever run on a scratch asset that the parity run deletes afterwards.
	 */
	inline void NormaliseLegacyInlineMasks(UObject* Asset)
	{
		UMaterial* Material = Cast<UMaterial>(Asset);
		UMaterialFunction* Function = Cast<UMaterialFunction>(Asset);
		if (!Material && !Function)
		{
			return;
		}

		TMap<FString, UMaterialExpression*> MaskNodes;
		const auto Normalise = [Material, Function, &MaskNodes](FExpressionInput& Input)
		{
			UMaterialExpression* Source = Input.Expression;
			if (!Source || Input.Mask == 0)
			{
				return;
			}
			const int32 OutputIndex = Input.OutputIndex;

			// The inline mask relative to the operand, spelled the way the IR spells a Swizzle: the shared rule.
			const FExpressionOutput* Output = Source->Outputs.IsValidIndex(OutputIndex) ? &Source->Outputs[OutputIndex] : nullptr;
			const UE::DreamShader::Editor::Private::FDreamShaderInlineMask Resolved = UE::DreamShader::Editor::Private::ResolveDreamShaderInlineMask(
				Input,
				Output,
				FMath::Clamp(UE::DreamShader::Editor::Private::GetExpressionOutputComponentCount(Source, OutputIndex), 1, 4));
			if (!Resolved.bMasked)
			{
				return;
			}
			const FString& Relative = Resolved.Relative;

			const auto Rewire = [&Input](UMaterialExpression* Expression, const int32 Index)
			{
				Input.Expression = Expression;
				Input.OutputIndex = Index;
				Input.Mask = 0;
				Input.MaskR = 0;
				Input.MaskG = 0;
				Input.MaskB = 0;
				Input.MaskA = 0;
			};

			int32 Selected = INDEX_NONE;
			if (Resolved.bIdentity)
			{
				// The identity: the IR builder never makes that Swizzle.
				Rewire(Source, OutputIndex);
			}
			else if (UE::DreamShader::Editor::Compiler::TryResolveSwizzleAsNamedOutput(Source, OutputIndex, Resolved.OperandWidth, Relative, Selected))
			{
				Rewire(Source, Selected);
			}
			else
			{
				UMaterialExpression*& Node = MaskNodes.FindOrAdd(FString::Printf(TEXT("%s#%d#%s"), *Source->GetPathName(), OutputIndex, *Relative));
				if (!Node)
				{
					UMaterialExpressionComponentMask* MaskNode = Cast<UMaterialExpressionComponentMask>(
						UE::DreamShader::Editor::Private::CreateOwnedMaterialExpression(
							Material, Function, UMaterialExpressionComponentMask::StaticClass(), 0, 0));
					if (!MaskNode)
					{
						return;
					}
					MaskNode->R = Relative.Contains(TEXT("x")) ? 1U : 0U;
					MaskNode->G = Relative.Contains(TEXT("y")) ? 1U : 0U;
					MaskNode->B = Relative.Contains(TEXT("z")) ? 1U : 0U;
					MaskNode->A = Relative.Contains(TEXT("w")) ? 1U : 0U;
					MaskNode->Input.Expression = Source;
					MaskNode->Input.OutputIndex = OutputIndex;
					Node = MaskNode;
				}
				Rewire(Node, 0);
			}
		};

		// Copied first: normalising adds ComponentMask expressions to the very list being walked.
		TArray<UMaterialExpression*> Existing;
		if (Material)
		{
			for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
			{
				Existing.Add(Expression.Get());
			}
		}
		else
		{
			for (const TObjectPtr<UMaterialExpression>& Expression : Function->GetExpressions())
			{
				Existing.Add(Expression.Get());
			}
		}

		for (UMaterialExpression* Expression : Existing)
		{
			for (int32 InputIndex = 0; Expression && Expression->GetInput(InputIndex); ++InputIndex)
			{
				Normalise(*Expression->GetInput(InputIndex));
			}
		}
		if (Material)
		{
			for (int32 Property = 0; Property < MP_MAX; ++Property)
			{
				if (FExpressionInput* Input = Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(Property)))
				{
					Normalise(*Input);
				}
			}
		}
	}

	// =============================================================================================
	// Parity goldens
	// =============================================================================================

	/** One parity pair: a 2.0 example and where its 1.x twin's frozen dump lives. */
	struct FParityGoldenPair
	{
		/** Leaf name under Tests/Corpus/Lang/Examples. */
		const TCHAR* LangExample = nullptr;
		/** The twin's dump, relative to <Project>/Saved/DreamShader/GraphBaseline/v2-6c2e0b6-formal: what a filled golden is compared with. */
		const TCHAR* BaselineDump = nullptr;
	};

	/**
	 * Compile the example exactly as a Compile corpus fixture is compiled -- a scratch copy under the project's DShader
	 * root, the Graph backend pinned, forced, every produced asset dumped, normalised and deleted again -- and judge it
	 * against `Tests/Corpus/Parity/<stem>.expected.json` rather than the golden beside the example, which belongs to the
	 * Lang layer. Reusing RunDreamShaderCompileCorpusCase gives the pair that runner's cleanup, `<package>/` rule,
	 * pending flag and -DreamShaderUpdateGolden behaviour unchanged.
	 *
	 * The golden starts graphPending. Once filled, its `graphDump` is compared with BaselineDump using
	 * `python Tools/Parity/graph_parity.py pair <baseline> <candidate>`; only a reviewed pair drops the flag.
	 */
	inline bool RunParityGoldenPair(FAutomationTestBase& Test, const FParityGoldenPair& Pair)
	{
		const FString Root = GetDreamShaderCorpusRoot();
		if (Root.IsEmpty())
		{
			Test.AddError(TEXT("The DreamShader test corpus root could not be located."));
			return false;
		}

		const FString Stem = FPaths::GetBaseFilename(FString(Pair.LangExample));
		FCorpusCase Case = MakeDreamShaderCorpusCase(FPaths::Combine(Root, TEXT("Lang"), TEXT("Examples"), Pair.LangExample));
		Case.RelativeName = FString::Printf(TEXT("Parity/%s"), *Stem);
		Case.ExpectedPath = FPaths::Combine(Root, TEXT("Parity"), Stem + TEXT(".expected.json"));
		Case.bHasExpectationFile = IFileManager::Get().FileExists(*Case.ExpectedPath);
		if (!Test.TestTrue(TEXT("the parity golden exists (Tests/Corpus/Parity)"), Case.bHasExpectationFile))
		{
			return false;
		}

		Test.AddInfo(FString::Printf(
			TEXT("Parity pair '%s': review the golden's graphDump against Saved/DreamShader/GraphBaseline/v2-6c2e0b6-formal/%s with Tools/Parity/graph_parity.py pair."),
			Pair.LangExample,
			Pair.BaselineDump));
		return RunDreamShaderCompileCorpusCase(Test, Case);
	}
}

// =================================================================================================
// End to end: one `.dss` becomes one UMaterial
// =================================================================================================

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2MaterialEndToEndTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.MaterialEndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2MaterialEndToEndTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	const FString AssetName = FString::Printf(TEXT("M_C2Glow_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FDreamShaderCompile2Fixture Fixture(AssetName, TEXT("Compiler2"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	if (!Fixture.WriteSource(*this, MakeSmokeMaterialSource(AssetName)))
	{
		return false;
	}

	UE::DreamShader::FDreamShaderError Error;
	if (!TestTrue(
			FString::Printf(TEXT("a .dss compiles through the 2.0 pipeline: %s"), *Error.Message),
			::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, /*bForce*/ true)))
	{
		AddInfo(FString::Printf(TEXT("the pipeline reported: %s: %s"), *Error.Code, *Error.Message));
		return false;
	}

	const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *Fixture.GetPackagePath(), *AssetName, *AssetName);
	Fixture.TrackObjectPath(ObjectPath);

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("the .dss produced a UMaterial"), Material))
	{
		return false;
	}

	// The expression driving EmissiveColor is the Multiply the source's last operator wrote. This
	// is the one assertion that says the whole chain worked: bind, lower, dedupe, emit, connect.
	FExpressionInput* EmissiveInput = Material->GetExpressionInputForProperty(MP_EmissiveColor);
	if (TestNotNull(TEXT("EmissiveColor has an input"), EmissiveInput)
		&& TestNotNull(TEXT("and something is connected to it"), EmissiveInput->Expression))
	{
		UMaterialExpression* Driving = ResolveDrivingExpression(EmissiveInput->Expression);
		TestTrue(
			FString::Printf(TEXT("a Multiply drives EmissiveColor (found: %s)"),
				Driving ? *Driving->GetClass()->GetName() : TEXT("<null>")),
			Driving && Driving->IsA<UMaterialExpressionMultiply>());
	}

	// The helper was inlined, so its `saturate`/`dot` are nodes of THIS graph and there is no
	// material function call anywhere.
	TestEqual(TEXT("an inlined helper leaves no MaterialFunctionCall"),
		CountExpressionsOfClass<UMaterialExpressionMaterialFunctionCall>(Material), 0);
	// `Tint.rgb` takes the leading three channels of the whole float4 parameter, and a VectorParameter
	// publishes exactly those channels as its own named RGB output -- so the swizzle IS that output
	// and makes no node. What 2.0 rules out is the inline FExpressionInput
	// mask: when the material editor rebuilds a graph, UMaterialGraph::GetValidOutputIndex re-points a
	// masked wire on output 0 at whichever output matches the mask (DSK2). Choosing an output is what
	// the editor itself writes when a wire is dragged from that pin, so it survives the rebuild. Both
	// halves are asserted: the wire leaves Tint's RGB pin, and no pin in the graph carries a mask.
	UMaterialExpression* Tint = nullptr;
	for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
	{
		if (Expression && Expression->GetParameterName() == FName(TEXT("Tint")))
		{
			Tint = Expression.Get();
			break;
		}
	}
	if (TestNotNull(TEXT("the Tint parameter is in the graph"), Tint))
	{
		const int32 RGBIndex = Tint->Outputs.IndexOfByPredicate([](const FExpressionOutput& Output)
		{
			return Output.OutputName == FName(TEXT("RGB"));
		});

		TArray<FString> TintReads;
		bool bReadsRGBOnly = true;
		int32 MaskedPins = 0;
		const auto InspectPin = [&](const FExpressionInput* Input)
		{
			if (!Input || !Input->Expression)
			{
				return;
			}
			if (Input->Mask != 0)
			{
				++MaskedPins;
			}
			if (Input->Expression == Tint)
			{
				TintReads.Add(FString::FromInt(Input->OutputIndex));
				bReadsRGBOnly = bReadsRGBOnly && Input->OutputIndex == RGBIndex;
			}
		};
		for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
		{
			if (!Expression)
			{
				continue;
			}
			int32 InputIndex = 0;
			while (const FExpressionInput* Input = Expression->GetInput(InputIndex++))
			{
				InspectPin(Input);
			}
		}
		InspectPin(EmissiveInput);

		const FString ReadList = TintReads.IsEmpty() ? FString(TEXT("nothing")) : FString::Join(TintReads, TEXT(", "));
		TestTrue(
			FString::Printf(TEXT("the swizzle reads Tint through its named RGB output %d (read from output: %s)"), RGBIndex, *ReadList),
			RGBIndex != INDEX_NONE && TintReads.Num() > 0 && bReadsRGBOnly);
		TestEqual(TEXT("and no pin in the graph carries an inline component mask"), MaskedPins, 0);
	}

	// Provenance: the 2.0 pipeline stamps the same metadata the 1.x one does, because the digest,
	// the divergence gate and the Adopt action all read it.
	TestTrue(TEXT("the asset carries DreamShader source metadata"), HasDreamShaderSourceMetadata(Material));
	TestFalse(TEXT("and a source path"), GetGeneratedAssetSourceFile(Material).IsEmpty());
	TestFalse(TEXT("and a source hash"), GetGeneratedAssetSourceHash(Material).IsEmpty());
	TestEqual(
		TEXT("a freshly compiled material classifies as Generated"),
		static_cast<int32>(ClassifyGeneratedAsset(Material)),
		static_cast<int32>(EDreamShaderDigestState::Generated));

	return true;
}

// =================================================================================================
// The source-span table
// =================================================================================================

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2SourceSpansTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.SourceSpans",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2SourceSpansTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	const FString AssetName = FString::Printf(TEXT("M_C2Spans_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FDreamShaderCompile2Fixture Fixture(AssetName, TEXT("Compiler2"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	if (!Fixture.WriteSource(*this, MakeSmokeMaterialSource(AssetName)))
	{
		return false;
	}

	UE::DreamShader::FDreamShaderError Error;
	if (!TestTrue(TEXT("the material compiles"),
			::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, /*bForce*/ true)))
	{
		AddInfo(FString::Printf(TEXT("the pipeline reported: %s: %s"), *Error.Code, *Error.Message));
		return false;
	}

	const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *Fixture.GetPackagePath(), *AssetName, *AssetName);
	Fixture.TrackObjectPath(ObjectPath);

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("the material loads"), Material))
	{
		return false;
	}

	// Decision 11 #10: the table is asset METADATA under one key, as JSON. Never `Desc` -- Desc is
	// the user's to write, and the digest treats it as cosmetic.
	const FString Spans = GetAssetMetadata(Material, TEXT("DreamShader.SourceSpans"));
	if (!TestFalse(TEXT("the asset carries a DreamShader.SourceSpans table"), Spans.IsEmpty()))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Spans);
	if (!TestTrue(TEXT("and it parses as JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
	{
		AddInfo(FString::Printf(TEXT("DreamShader.SourceSpans was: %s"), *Spans));
		return false;
	}

	TestTrue(TEXT("the table is not empty"), Root->Values.Num() > 0);

	// One entry per expression guid, each with the four fields the editor jumps with. An entry made
	// while inlining also carries the call site, which is what gives the menu two lines to offer.
	int32 WithCallSite = 0;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
	{
		FGuid Guid;
		TestTrue(
			FString::Printf(TEXT("'%s' is an expression guid"), *Pair.Key),
			FGuid::Parse(Pair.Key, Guid));

		const TSharedPtr<FJsonObject>* Entry = nullptr;
		if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Entry) || !Entry)
		{
			AddError(FString::Printf(TEXT("the entry for '%s' is not an object"), *Pair.Key));
			continue;
		}

		FString File;
		double Line = 0.0;
		double Column = 0.0;
		TestTrue(FString::Printf(TEXT("'%s' names a file"), *Pair.Key), (*Entry)->TryGetStringField(TEXT("file"), File));
		TestTrue(FString::Printf(TEXT("'%s' names a line"), *Pair.Key), (*Entry)->TryGetNumberField(TEXT("line"), Line));
		TestTrue(FString::Printf(TEXT("'%s' names a column"), *Pair.Key), (*Entry)->TryGetNumberField(TEXT("col"), Column));
		TestTrue(FString::Printf(TEXT("'%s' has a 1-based line"), *Pair.Key), Line >= 1.0);

		double CallLine = 0.0;
		if ((*Entry)->TryGetNumberField(TEXT("callLine"), CallLine))
		{
			++WithCallSite;
		}

		// `callFile` is written only when the call site lies in a DIFFERENT file
		// from the span -- a helper inlined out of an included `.dsh`. This source includes nothing,
		// so the key must not appear; an entry that carried it would mean the writer stopped
		// comparing the two paths and started writing both unconditionally.
		FString CallFile;
		TestFalse(
			FString::Printf(TEXT("'%s' carries no callFile (this source includes nothing)"), *Pair.Key),
			(*Entry)->TryGetStringField(TEXT("callFile"), CallFile));
	}

	// The smoke source calls a helper, so something in the table must have been made while inlining.
	TestTrue(TEXT("at least one node records the call site it was inlined at"), WithCallSite > 0);
	return true;
}

// =================================================================================================
// Skip when current, rebuild on force, refuse when diverged
// =================================================================================================

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2RebuildRulesTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.RebuildRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2RebuildRulesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	const FString AssetName = FString::Printf(TEXT("M_C2Rebuild_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FDreamShaderCompile2Fixture Fixture(AssetName, TEXT("Compiler2"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	if (!Fixture.WriteSource(*this, MakeSmokeMaterialSource(AssetName)))
	{
		return false;
	}

	UE::DreamShader::FDreamShaderError Error;
	if (!TestTrue(TEXT("the first compile succeeds"),
			::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, /*bForce*/ true)))
	{
		AddInfo(FString::Printf(TEXT("the pipeline reported: %s: %s"), *Error.Code, *Error.Message));
		return false;
	}

	const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *Fixture.GetPackagePath(), *AssetName, *AssetName);
	Fixture.TrackObjectPath(ObjectPath);

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("the material loads"), Material))
	{
		return false;
	}

	const FString FirstHash = GetGeneratedAssetSourceHash(Material);
	const FString FirstDigest = BuildOutputDigest(Material);
	TestFalse(TEXT("the first compile stamped a source hash"), FirstHash.IsEmpty());

	// Without -Force an unchanged source is a no-op, which is what makes compile-on-save cheap.
	// The pipeline reports SUCCESS for a skip: nothing failed, there was simply nothing to do.
	UE::DreamShader::FDreamShaderError SkipError;
	TestTrue(TEXT("recompiling an unchanged source succeeds (as a skip)"),
		::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), SkipError, /*bForce*/ false));
	TestTrue(TEXT("and the source is still current"),
		IsGeneratedAssetSourceCurrent(Material, Fixture.GetSourceFilePath(), FirstHash));

	// -Force rebuilds, and an identical rebuild reproduces the digest byte for byte. A digest that
	// moved without the source moving would make every asset in a project read as hand-edited.
	UE::DreamShader::FDreamShaderError ForceError;
	if (TestTrue(TEXT("-Force rebuilds"),
			::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), ForceError, /*bForce*/ true)))
	{
		Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
		if (TestNotNull(TEXT("the material still loads after the rebuild"), Material))
		{
			TestEqualSensitive(TEXT("an identical rebuild reproduces the digest"), BuildOutputDigest(Material), FirstDigest);
			TestEqual(
				TEXT("and the material is still Generated"),
				static_cast<int32>(ClassifyGeneratedAsset(Material)),
				static_cast<int32>(EDreamShaderDigestState::Generated));
		}
	}

	// A hand edit survives every compile that is not a Revert. The 2.0 pipeline reuses the 1.x
	// divergence gate unchanged (Docs/generation/divergence.md), so it answers exactly as 1.x does.
	Material->TwoSided = true;
	TestEqual(
		TEXT("a hand edit reads as divergence"),
		static_cast<int32>(ClassifyGeneratedAsset(Material)),
		static_cast<int32>(EDreamShaderDigestState::Diverged));

	// An unchanged source is skipped by the source hash long before the gate: nothing is in danger,
	// so nothing is refused and nothing is reported.
	UE::DreamShader::FDreamShaderError UnchangedError;
	TestTrue(TEXT("an unchanged source over a diverged asset is still a skip"),
		::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), UnchangedError, /*bForce*/ false));
	TestTrue(TEXT("and the skip left the edit alone"), Material->TwoSided != 0);

	// Move the source. Without this the compile never reaches the gate, and the refusal below would
	// prove nothing.
	if (!Fixture.WriteSource(*this, MakeSmokeMaterialSource(AssetName).Replace(TEXT("Intensity = 0.7"), TEXT("Intensity = 0.8"))))
	{
		return false;
	}

	UE::DreamShader::FDreamShaderError DivergedError;
	const bool bRebuilt = ::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(
		Fixture.GetSourceFilePath(), DivergedError, /*bForce*/ false);
	TestFalse(TEXT("a changed source does NOT rebuild over a diverged asset"), bRebuilt);
	TestTrue(TEXT("and the refused asset is still TwoSided, untouched"), Material->TwoSided != 0);
	TestEqualSensitive(TEXT("the refusal comes from the divergence gate"), DivergedError.Code, FString(TEXT("DSH8207")));
	TestTrue(TEXT("and it carries the sentence the divergence notice parses"), DivergedError.Message.Contains(TEXT("edited by hand")));
	AddInfo(FString::Printf(TEXT("the refusal said: %s: %s"), *DivergedError.Code, *DivergedError.Message));

	// -Force alone is not a Revert: bForce answers "is the source hash stale", and Recompile All
	// forces every file in the project.
	UE::DreamShader::FDreamShaderError ForcedError;
	TestFalse(TEXT("-Force alone does not overwrite a diverged asset"),
		::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), ForcedError, /*bForce*/ true));
	TestTrue(TEXT("and still leaves the edit alone"), Material->TwoSided != 0);

	// Revert: what the user's confirmation of the Revert dialog reaches the generator as.
	bool bReverted = false;
	{
		FScopedDreamShaderRevertDiverged RevertScope;
		UE::DreamShader::FDreamShaderError RevertError;
		bReverted = ::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), RevertError, /*bForce*/ true);
		if (!bReverted)
		{
			AddInfo(FString::Printf(TEXT("the revert reported: %s: %s"), *RevertError.Code, *RevertError.Message));
		}
	}
	if (TestTrue(TEXT("a revert-scoped -Force reverts a diverged asset"), bReverted))
	{
		Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
		if (TestNotNull(TEXT("the material loads after the revert"), Material))
		{
			TestEqual(
				TEXT("and is Generated again"),
				static_cast<int32>(ClassifyGeneratedAsset(Material)),
				static_cast<int32>(EDreamShaderDigestState::Generated));
		}
	}

	return true;
}

// =================================================================================================
// The ThinCustom backend
// =================================================================================================

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2EphemeralBaseIdentityTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.EphemeralBaseIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2EphemeralBaseIdentityTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	const FString Prefix = TEXT("BaseIdentity_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FDreamShaderCompile2Fixture First(Prefix + TEXT("_B"), TEXT("Compiler2"));
	FDreamShaderCompile2Fixture Second(Prefix, TEXT("Compiler2"));
	const FString FirstPath = First.MakeObjectPath(TEXT("M"));
	const FString SecondPath = Second.MakeObjectPath(TEXT("B_M"));
	First.TrackObjectPath(FirstPath);
	Second.TrackObjectPath(SecondPath);
	AddExpectedError(First.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(Second.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);
	TestEqual(TEXT("the distinct package paths collide under the old name encoding"),
		UE::DreamShader::SanitizeIdentifier(FPackageName::ObjectPathToPackageName(FirstPath)),
		UE::DreamShader::SanitizeIdentifier(FPackageName::ObjectPathToPackageName(SecondPath)));
	if (!First.WriteSource(*this, MakeThinCustomMaterialSource(TEXT("M")))
		|| !Second.WriteSource(*this, MakeThinCustomMaterialSource(TEXT("B_M")).Replace(TEXT("float4(1, 0.5, 0.25, 1)"), TEXT("float4(0, 1, 0, 1)"))))
	{
		return false;
	}

	FString Message;
	if (!TestTrue(TEXT("first ephemeral compile"), CompileDreamShaderTestAssets(First.GetSourceFilePath(), Message, true, true)))
	{
		AddInfo(Message);
		return false;
	}
	UDreamShaderMaterialInstance* A = LoadObject<UDreamShaderMaterialInstance>(nullptr, *FirstPath);
	UMaterial* BaseA = A ? Cast<UMaterial>(A->Parent) : nullptr;
	if (!TestNotNull(TEXT("first transient base"), BaseA)) { return false; }
	const FString DigestA = BuildMaterialDigestText(BaseA);
	if (!TestTrue(TEXT("second ephemeral compile"), CompileDreamShaderTestAssets(Second.GetSourceFilePath(), Message, true, true)))
	{
		AddInfo(Message);
		return false;
	}
	UDreamShaderMaterialInstance* B = LoadObject<UDreamShaderMaterialInstance>(nullptr, *SecondPath);
	UMaterial* BaseB = B ? Cast<UMaterial>(B->Parent) : nullptr;
	if (!TestNotNull(TEXT("second transient base"), BaseB)) { return false; }
	TestTrue(TEXT("different instances own different bases despite their colliding labels"), BaseA != BaseB);
	TestEqual(TEXT("compiling B did not replace A's graph"), BuildMaterialDigestText(BaseA), DigestA);
	const FString DigestB = BuildMaterialDigestText(BaseB);
	TestTrue(TEXT("rebuilding A succeeds"), CompileDreamShaderTestAssets(First.GetSourceFilePath(), Message, true, true));
	TestEqual(TEXT("A reuses its attached base"), A->Parent.Get(), static_cast<UMaterialInterface*>(BaseA));
	TestEqual(TEXT("rebuilding A leaves B alone"), BuildMaterialDigestText(BaseB), DigestB);
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2ThinCustomTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.ThinCustomBackend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2ThinCustomTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	const FString AssetName = FString::Printf(TEXT("M_C2Thin_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FDreamShaderCompile2Fixture Fixture(AssetName, TEXT("Compiler2"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	if (!Fixture.WriteSource(*this, MakeThinCustomMaterialSource(AssetName)))
	{
		return false;
	}

	UE::DreamShader::FDreamShaderError Error;
	if (!TestTrue(TEXT("a ThinCustom .dss compiles"),
			::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, /*bForce*/ true)))
	{
		AddInfo(FString::Printf(TEXT("the pipeline reported: %s: %s"), *Error.Code, *Error.Message));
		return false;
	}

	const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *Fixture.GetPackagePath(), *AssetName, *AssetName);
	Fixture.TrackObjectPath(ObjectPath);

	// `#pragma material(Backend = ThinCustom)` produces the instance, not a plain UMaterial.
	UDreamShaderMaterialInstance* Instance = LoadObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("the ThinCustom .dss produced a DreamShader material instance"), Instance))
	{
		return false;
	}

	TestTrue(TEXT("the instance forces a static permutation"), Instance->HasOverridenBaseProperties());

	UMaterial* Base = Cast<UMaterial>(Instance->Parent);
	if (TestNotNull(TEXT("the instance is parented to a real base UMaterial"), Base))
	{
		TestTrue(TEXT("the base is the hidden ThinCustom base"),
			Base->GetName().StartsWith(TEXT("MB_DreamThinBase_"), ESearchCase::CaseSensitive));
		TestTrue(TEXT("the base carries the real graph"),
			CountExpressionsOfClass<UMaterialExpressionMultiply>(Base) >= 1);

		// Convergence: the base is a subobject of the instance's package, never a browsable sibling.
		TestEqual(TEXT("the base is a subobject of the instance"), Base->GetOuter(), static_cast<UObject*>(Instance));
		TestEqual(TEXT("and shares its package"), Base->GetOutermost(), Instance->GetOutermost());
		TestFalse(TEXT("and is not an independently browsable asset"), Base->IsAsset());
	}

	return true;
}

// =================================================================================================
// A function library, and a material that calls one of its functions
// =================================================================================================

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2FunctionLibraryTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.FunctionLibrary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2FunctionLibraryTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	const FString Prefix = FString::Printf(TEXT("MF_C2_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
	FDreamShaderCompile2Fixture Fixture(Prefix, TEXT("Compiler2"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	if (!Fixture.WriteSource(*this, MakeFunctionLibrarySource(Prefix)))
	{
		return false;
	}

	UE::DreamShader::FDreamShaderError Error;
	if (!TestTrue(TEXT("a function library .dss compiles"),
			::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, /*bForce*/ true)))
	{
		AddInfo(FString::Printf(TEXT("the pipeline reported: %s: %s"), *Error.Code, *Error.Message));
		return false;
	}

	// One asset per export, and every one of them a UMaterialFunction.
	TArray<FDreamShaderCompiledAsset> Assets;
	Fixture.CollectProducedAssets(Assets);

	TestEqual(TEXT("two exports produce two assets"), Assets.Num(), 2);
	for (const FDreamShaderCompiledAsset& Asset : Assets)
	{
		TestEqualSensitive(
			*FString::Printf(TEXT("'%s' is a MaterialFunction"), *Asset.Name),
			Asset.Kind,
			FString(TEXT("MaterialFunction")));
	}

	const FString ScalePath = FString::Printf(TEXT("%s/%s_Scale.%s_Scale"), *Fixture.GetPackagePath(), *Prefix, *Prefix);
	const FString OuterPath = FString::Printf(TEXT("%s/%s_ScaleOffset.%s_ScaleOffset"), *Fixture.GetPackagePath(), *Prefix, *Prefix);
	Fixture.TrackObjectPath(ScalePath);
	Fixture.TrackObjectPath(OuterPath);

	UMaterialFunction* Scale = LoadObject<UMaterialFunction>(nullptr, *ScalePath);
	UMaterialFunction* ScaleOffset = LoadObject<UMaterialFunction>(nullptr, *OuterPath);
	if (!TestNotNull(TEXT("the inner function loads"), Scale)
		|| !TestNotNull(TEXT("the outer function loads"), ScaleOffset))
	{
		return false;
	}

	// The inner function's inputs keep declaration order with dense sort priorities:
	// tied priorities are what silently reordered a function's pins in 1.x.
	TArray<int32> InputPriorities;
	for (const TObjectPtr<UMaterialExpression>& Expression : ScaleOffset->GetExpressions())
	{
		if (const UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(Expression.Get()))
		{
			InputPriorities.Add(Input->SortPriority);
		}
	}
	InputPriorities.Sort();
	TestEqual(TEXT("three function inputs"), InputPriorities.Num(), 3);
	for (int32 Index = 0; Index < InputPriorities.Num(); ++Index)
	{
		TestEqual(
			FString::Printf(TEXT("input %d has a dense sort priority"), Index),
			InputPriorities[Index], Index);
	}

	// A call to a sibling export is a MaterialFunctionCall pointing at that export's asset, never
	// an inline copy of its body.
	int32 CallCount = 0;
	bool bCallsTheSibling = false;
	for (const TObjectPtr<UMaterialExpression>& Expression : ScaleOffset->GetExpressions())
	{
		if (const UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expression.Get()))
		{
			++CallCount;
			bCallsTheSibling |= (Call->MaterialFunction == Scale);
		}
	}
	TestEqual(TEXT("one MaterialFunctionCall"), CallCount, 1);
	TestTrue(TEXT("and it points at the sibling export's asset"), bCallsTheSibling);
	return true;
}

// =================================================================================================
// Parity with the 1.x generator
// =================================================================================================

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2ParityMaterialTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Parity.Material",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2ParityMaterialTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FParityGoldenPair Pair;
	Pair.LangExample = TEXT("M_TeleportGlow.dss");
	Pair.BaselineDump = TEXT("Project/Materials/FX/M_TeleportGlow.dsm.M_TeleportGlow.graph.json");
	// What the live comparison forgave, and the review of the filled golden has to forgive the same way: the two
	// sources are not a literal translation of one another, so SortPriority (10/20/30 in the .dsm, none in the .dss),
	// Group ("Glow | Look" vs "Glow|Look") and Description ("brightness pulse" vs "breathing pulse") differ by
	// authorship; PD-3 keeps the Custom node's Code and IncludeFilePaths out (graph_parity.py's default key filter).
	// Nothing structural is forgiven: a node, a connection, a class, a default value or a material setting that
	// differs is a real difference.
	return RunParityGoldenPair(*this, Pair);
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2ParityFunctionTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Parity.Function",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2ParityFunctionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FParityGoldenPair Pair;
	Pair.LangExample = TEXT("MF_ToonUV.dss");
	Pair.BaselineDump = TEXT("MoonToon/MaterialFunctions/Shared/MF_ToonUV.dsf.MF_ToonUV.graph.json");
	// Two differences between the graphs are not compiler bugs (Tools/Parity/README.md), and the review absorbs them
	// exactly rather than by dropping a key:
	//  - PD-1: `ScaleOffset.rg` / `.ba` are inline masks on the FunctionInput's wires in 1.x and ComponentMask nodes in
	//    2.0, which never writes an inline mask; graph_parity.py folds both into one channel list (normalisation M),
	//    which also settles `outputs[].type` (PD-2).
	//  - PS-1: the output is `UV` in the .dsf and `Result` in the .dss, which returns its value. The pin
	//    name, the output reroute `DS_UV_0` / `DS_Result_0` and every input that names it differ by that rename only.
	return RunParityGoldenPair(*this, Pair);
}

// =================================================================================================
// What the dump cannot see, and what must not depend on the machine
// =================================================================================================

namespace UE::DreamShader::Editor::Private::Compiler2Tests
{
	/** Writes and compiles Source into the fixture and loads the asset named AssetName; null with the reason on Test. */
	template <typename TAsset>
	inline TAsset* CompileSmokeFixture(FAutomationTestBase& Test, FDreamShaderCompile2Fixture& Fixture, const FString& Source, const FString& AssetName)
	{
		Test.AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);
		if (!Fixture.WriteSource(Test, Source))
		{
			return nullptr;
		}

		UE::DreamShader::FDreamShaderError Error;
		if (!::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, /*bForce*/ true))
		{
			Test.AddError(FString::Printf(TEXT("the fixture does not compile: %s: %s"), *Error.Code, *Error.Message));
			return nullptr;
		}

		const FString ObjectPath = Fixture.MakeObjectPath(AssetName);
		Fixture.TrackObjectPath(ObjectPath);
		TAsset* Asset = LoadObject<TAsset>(nullptr, *ObjectPath);
		if (!Asset)
		{
			Test.AddError(FString::Printf(TEXT("the compile did not make '%s'."), *ObjectPath));
		}
		return Asset;
	}

	/** True when Inner's box lies inside Outer's. */
	inline bool CommentContains(const UMaterialExpressionComment& Outer, const UMaterialExpressionComment& Inner)
	{
		return Inner.MaterialExpressionEditorX >= Outer.MaterialExpressionEditorX
			&& Inner.MaterialExpressionEditorY >= Outer.MaterialExpressionEditorY
			&& Inner.MaterialExpressionEditorX + Inner.SizeX <= Outer.MaterialExpressionEditorX + Outer.SizeX
			&& Inner.MaterialExpressionEditorY + Inner.SizeY <= Outer.MaterialExpressionEditorY + Outer.SizeY;
	}
}

// B5: nothing the compile writes into an asset names this machine.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2MachineIndependentPathsTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.MachineIndependentPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2MachineIndependentPathsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	const FString AssetName = TEXT("M_C2Paths");
	FDreamShaderCompile2Fixture Fixture(AssetName, TEXT("Compiler2"));
	UMaterial* Material = CompileSmokeFixture<UMaterial>(*this, Fixture, TEXT(
		"#pragma material(ShadingModel = Unlit, BlendMode = Opaque)\n"
		"\n"
		"/// @custom\n"
		"float3 Posterise(float3 Colour, float Steps)\n"
		"{\n"
		"    return floor(Colour * Steps) / Steps;\n"
		"}\n"
		"\n"
		"uniform float3 Tint = float3(1, 0.5, 0.25);\n"
		"\n"
		"export void M_C2Paths(inout material m)\n"
		"{\n"
		"    m.EmissiveColor = Posterise(Tint, 4);\n"
		"}\n"), AssetName);
	if (!Material)
	{
		return false;
	}

	const FString ProjectDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	int32 CustomNodes = 0;
	for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
	{
		const UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression.Get());
		if (!Custom)
		{
			continue;
		}
		++CustomNodes;
		TestTrue(TEXT("the code names its source"), Custom->Code.Contains(TEXT("// Begin DreamShader source: ")));
		TestFalse(FString::Printf(TEXT("the code holds no project directory\n%s"), *Custom->Code), Custom->Code.Contains(ProjectDirectory, ESearchCase::IgnoreCase));
		TestFalse(TEXT("the code holds no drive letter"), Custom->Code.Contains(TEXT(":/")) || Custom->Code.Contains(TEXT(":\\")));
		TestTrue(TEXT("the marker path is the project-relative one"), Custom->Code.Contains(MakeProjectRelativeSourcePath(Fixture.GetSourceFilePath())));
	}
	TestEqual(TEXT("one Custom node"), CustomNodes, 1);

	// The source spans the navigation reads.
	const FString Spans = GetAssetMetadata(Material, TEXT("DreamShader.SourceSpans"));
	TestFalse(TEXT("DreamShader.SourceSpans is written"), Spans.IsEmpty());
	TestFalse(FString::Printf(TEXT("and names no project directory\n%s"), *Spans), Spans.Contains(ProjectDirectory, ESearchCase::IgnoreCase));

	// The stamped source file is the same relative spelling.
	const FString SourceFile = GetAssetMetadata(Material, TEXT("DreamShader.SourceFile"));
	TestTrue(FString::Printf(TEXT("DreamShader.SourceFile is project-relative ('%s')"), *SourceFile), FPaths::IsRelative(SourceFile));
	return true;
}

// B7: what the graph dump cannot see -- comment boxes and node descriptions.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2RegionsAndDescriptionsTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.RegionsAndDescriptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2RegionsAndDescriptionsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	// A material.
	{
		const FString AssetName = TEXT("M_C2Boxes");
		FDreamShaderCompile2Fixture Fixture(AssetName, TEXT("Compiler2"));
		UMaterial* Material = CompileSmokeFixture<UMaterial>(*this, Fixture, TEXT(
			"#pragma material(ShadingModel = Unlit, BlendMode = Opaque)\n"
			"\n"
			"/// @desc The overall gain\n"
			"uniform float Gain = 0.5;\n"
			"uniform float Bias = 0.25;\n"
			"\n"
			"export void M_C2Boxes(inout material m)\n"
			"{\n"
			"    #pragma region Outer\n"
			"    float Scaled = Gain * 2;\n"
			"    #pragma region Inner\n"
			"    float Shifted = Scaled + Bias;\n"
			"    #pragma endregion\n"
			"    float Final = Shifted * Scaled;\n"
			"    #pragma endregion\n"
			"    m.EmissiveColor = float3(Final, Final, Final);\n"
			"}\n"), AssetName);
		if (!Material)
		{
			return false;
		}

		const UMaterialExpressionComment* Outer = nullptr;
		const UMaterialExpressionComment* Inner = nullptr;
		int32 Boxes = 0;
		for (const TObjectPtr<UMaterialExpressionComment>& Comment : Material->GetEditorComments())
		{
			if (!Comment)
			{
				continue;
			}
			++Boxes;
			// A region's box carries the generator's prefix, as every box a build makes has since 1.x.
			if (Comment->Text == TEXT("DreamShader: Outer")) { Outer = Comment.Get(); }
			if (Comment->Text == TEXT("DreamShader: Inner")) { Inner = Comment.Get(); }
		}
		// The build boxes its output blocks too ("Output: EmissiveColor", "Material Output"); the regions are two of them.
		TestTrue(TEXT("the two region boxes are among the boxes"), Boxes >= 2);
		if (!Outer || !Inner)
		{
			for (const TObjectPtr<UMaterialExpressionComment>& Comment : Material->GetEditorComments())
			{
				AddInfo(FString::Printf(TEXT("a comment box titled '%s'"), Comment ? *Comment->Text : TEXT("<null>")));
			}
		}
		if (TestNotNull(TEXT("the Outer box"), Outer) && TestNotNull(TEXT("the Inner box"), Inner))
		{
			TestTrue(TEXT("Inner lies inside Outer"), CommentContains(*Outer, *Inner));
			TestFalse(TEXT("and not the other way round"), CommentContains(*Inner, *Outer));
		}

		const UMaterialExpression* Gain = nullptr;
		for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
		{
			if (Expression && Expression->GetParameterName() == FName(TEXT("Gain")))
			{
				Gain = Expression.Get();
			}
		}
		if (TestNotNull(TEXT("the Gain parameter"), Gain))
		{
			TestEqual(TEXT("`@desc` is the parameter node's description"), Gain->Desc, FString(TEXT("The overall gain")));
		}
	}

	// A function: the same boxes, on the function's own comment list.
	{
		const FString AssetName = TEXT("MF_C2Boxes");
		FDreamShaderCompile2Fixture Fixture(AssetName, TEXT("Compiler2"));
		UMaterialFunction* Function = CompileSmokeFixture<UMaterialFunction>(*this, Fixture, TEXT(
			"export float MF_C2Boxes(float Value, float Scale = 2.0)\n"
			"{\n"
			"    #pragma region Maths\n"
			"    float Scaled = Value * Scale;\n"
			"    #pragma endregion\n"
			"    return Scaled + 1;\n"
			"}\n"), AssetName);
		if (!Function)
		{
			return false;
		}
		int32 Boxes = 0;
		bool bFoundMaths = false;
		for (const TObjectPtr<UMaterialExpressionComment>& Comment : Function->GetEditorComments())
		{
			if (Comment)
			{
				++Boxes;
				bFoundMaths = bFoundMaths || Comment->Text == TEXT("DreamShader: Maths");
			}
		}
		TestTrue(TEXT("the function has its boxes"), Boxes >= 1);
		TestTrue(TEXT("titled after its region"), bFoundMaths);
	}
	return true;
}

// Substrate sugar S2 and S4 against the engine's own nodes. Both read the project: `Substrate = Bridge` folds only where
// Substrate is on, so the test asks the same question the compiler does (`r.Substrate`, DS_SUBSTRATE's source) and holds
// either way. The nodes are found by class name and read by reflection -- the Substrate expression classes are
// MinimalAPI, and nothing here needs more of them than their properties.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2SubstrateBridgeAndSelectTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.SubstrateBridgeAndSelect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2SubstrateBridgeAndSelectTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	const IConsoleVariable* SubstrateVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Substrate"));
	const bool bSubstrate = SubstrateVar && SubstrateVar->GetInt() != 0;
	AddInfo(FString::Printf(TEXT("r.Substrate is %s in this project."), bSubstrate ? TEXT("on") : TEXT("off")));

	const auto FindByClass = [](const UMaterial& Material, const TCHAR* ClassName, int32& OutCount) -> UMaterialExpression*
	{
		UMaterialExpression* Found = nullptr;
		OutCount = 0;
		for (const TObjectPtr<UMaterialExpression>& Expression : Material.GetExpressions())
		{
			if (Expression && Expression->GetClass()->GetName().Equals(ClassName, ESearchCase::CaseSensitive))
			{
				Found = Found ? Found : Expression.Get();
				++OutCount;
			}
		}
		return Found;
	};
	const auto IsPinConnected = [](UMaterialExpression& Expression, const TCHAR* PinName) -> bool
	{
		for (int32 Index = 0; ; ++Index)
		{
			const FExpressionInput* Input = Expression.GetInput(Index);
			if (!Input)
			{
				return false;
			}
			if (Expression.GetInputName(Index).ToString().Replace(TEXT(" "), TEXT("")).Equals(PinName, ESearchCase::IgnoreCase))
			{
				return Input->Expression != nullptr;
			}
		}
	};

	// S4: the legacy shading attributes of a Bridge material, in a Substrate project, are one ShadingModels node.
	{
		const FString AssetName = TEXT("M_C2Bridge");
		FDreamShaderCompile2Fixture Fixture(AssetName, TEXT("Compiler2"));
		UMaterial* Material = CompileSmokeFixture<UMaterial>(*this, Fixture, TEXT(
			"#pragma material(ShadingModel = ClearCoat, Substrate = Bridge)\n"
			"\n"
			"uniform float3 Tint = float3(0.8, 0.2, 0.2);\n"
			"\n"
			"export void M_C2Bridge(inout material m)\n"
			"{\n"
			"    m.BaseColor = Tint;\n"
			"    m.Roughness = 0.4;\n"
			"    m.ClearCoat = 1.0;\n"
			"    m.WorldPositionOffset = float3(0, 0, 1);\n"
			"}\n"), AssetName);
		if (!Material)
		{
			return false;
		}

		int32 Count = 0;
		UMaterialExpression* ShadingModels = FindByClass(*Material, TEXT("MaterialExpressionSubstrateShadingModels"), Count);
		const FExpressionInput* BaseColor = Material->GetExpressionInputForProperty(MP_BaseColor);
		const FExpressionInput* Front = Material->GetExpressionInputForProperty(MP_FrontMaterial);
		const FExpressionInput* Offset = Material->GetExpressionInputForProperty(MP_WorldPositionOffset);
		TestTrue(TEXT("WorldPositionOffset stays on the material in either project"), Offset && Offset->Expression != nullptr);
		if (bSubstrate)
		{
			TestEqual(TEXT("one ShadingModels node"), Count, 1);
			TestTrue(TEXT("FrontMaterial is driven"), Front && Front->Expression != nullptr);
			TestTrue(TEXT("BaseColor left the material"), BaseColor && BaseColor->Expression == nullptr);
			if (ShadingModels)
			{
				TestTrue(TEXT("...for the node's BaseColor"), IsPinConnected(*ShadingModels, TEXT("BaseColor")));
				TestTrue(TEXT("Roughness with it"), IsPinConnected(*ShadingModels, TEXT("Roughness")));
				TestTrue(TEXT("ClearCoat with it"), IsPinConnected(*ShadingModels, TEXT("ClearCoat")));

				const FByteProperty* Override = FindFProperty<FByteProperty>(ShadingModels->GetClass(), TEXT("ShadingModelOverride"));
				if (TestNotNull(TEXT("the node has a ShadingModelOverride"), Override))
				{
					TestEqual(TEXT("the material's shading model is the node's"),
						static_cast<int32>(Override->GetPropertyValue_InContainer(ShadingModels)), static_cast<int32>(MSM_ClearCoat));
				}
			}
		}
		else
		{
			TestEqual(TEXT("no ShadingModels node where Substrate is off"), Count, 0);
			TestTrue(TEXT("BaseColor is on the material"), BaseColor && BaseColor->Expression != nullptr);
		}
	}

	// S2: a run-time choice between two Substrate values is the engine's Select, its three pins driven.
	{
		const FString AssetName = TEXT("M_C2Select");
		FDreamShaderCompile2Fixture Fixture(AssetName, TEXT("Compiler2"));
		UMaterial* Material = CompileSmokeFixture<UMaterial>(*this, Fixture, TEXT(
			"uniform float Blend = 0.75;\n"
			"\n"
			"export void M_C2Select(inout material m)\n"
			"{\n"
			"    Substrate Rough = Substrate.Slab(Roughness = 0.9);\n"
			"    Substrate Smooth = Substrate.Slab(Roughness = 0.1);\n"
			"    m.FrontMaterial = Blend > 0.5 ? Smooth : Rough;\n"
			"}\n"), AssetName);
		if (!Material)
		{
			return false;
		}

		int32 Count = 0;
		UMaterialExpression* Select = FindByClass(*Material, TEXT("MaterialExpressionSubstrateSelect"), Count);
		TestEqual(TEXT("one Select node"), Count, 1);
		if (Select)
		{
			TestTrue(TEXT("A is driven"), IsPinConnected(*Select, TEXT("A")));
			TestTrue(TEXT("B is driven"), IsPinConnected(*Select, TEXT("B")));
			TestTrue(TEXT("SelectValue is driven"), IsPinConnected(*Select, TEXT("SelectValue")));
		}
		int32 Slabs = 0;
		FindByClass(*Material, TEXT("MaterialExpressionSubstrateSlabBSDF"), Slabs);
		TestEqual(TEXT("between the two slabs"), Slabs, 2);
	}
	return true;
}

// The Blocks layout style on a live graph: the reroutes the layout counts on are made, the constant two boxes share is
// repeated, no wire runs from one box into another, and what the decompiler reads back is still the source's graph.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2BlocksLayoutTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.BlocksLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2BlocksLayoutTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	// Blocks is the default, and a project may have chosen another style: this test is about this one.
	FScopedDreamShaderLayoutStylePin LayoutStyle(EDreamShaderGraphLayoutStyle::Blocks);

	static const TCHAR* const Source = TEXT(
		"#pragma material(ShadingModel = Unlit)\n"
		"\n"
		"uniform float Gain = 2;\n"
		"uniform float3 Tint = float3(1, 0.5, 0.25);\n"
		"\n"
		"export void M_C2Blocks(inout material m)\n"
		"{\n"
		"    #pragma region Base\n"
		"    float3 Albedo = Tint * 0.5;\n"
		"    #pragma endregion\n"
		"    #pragma region Glow\n"
		"    float3 Lit = Albedo + Gain;\n"
		"    float Half = Gain * 0.5;\n"
		"    #pragma endregion\n"
		"    m.EmissiveColor = Lit * Half;\n"
		"}\n");

	const FString AssetName = TEXT("M_C2Blocks");
	FDreamShaderCompile2Fixture Fixture(AssetName, TEXT("Compiler2"));
	UMaterial* Material = CompileSmokeFixture<UMaterial>(*this, Fixture, Source, AssetName);
	if (!Material)
	{
		return false;
	}

	// ----- the reroute for the value that crosses, and the constant written twice
	const UMaterialExpressionNamedRerouteDeclaration* AlbedoDeclaration = nullptr;
	int32 AlbedoUsages = 0;
	int32 HalfConstants = 0;
	for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
	{
		if (const auto* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Expression))
		{
			if (Declaration->Name == FName(TEXT("DS_Albedo")))
			{
				AlbedoDeclaration = Declaration;
			}
		}
		else if (const auto* Constant = Cast<UMaterialExpressionConstant>(Expression))
		{
			HalfConstants += FMath::IsNearlyEqual(Constant->R, 0.5f) ? 1 : 0;
		}
	}
	if (TestNotNull(TEXT("Albedo leaves its box through a reroute named after it"), AlbedoDeclaration))
	{
		TestTrue(TEXT("...fed by the node that makes it"), AlbedoDeclaration->Input.Expression != nullptr);
		for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
		{
			const auto* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression);
			AlbedoUsages += Usage && Usage->Declaration == AlbedoDeclaration ? 1 : 0;
		}
		TestEqual(TEXT("...and read through one usage in the box that reads it"), AlbedoUsages, 1);
	}
	TestEqual(TEXT("the 0.5 both boxes use is written in each"), HalfConstants, 2);

	// ----- no wire from one box into another: a box is a generated comment, and a node is in the innermost one around it
	TArray<const UMaterialExpressionComment*> Boxes;
	for (const TObjectPtr<UMaterialExpressionComment>& Comment : Material->GetEditorComments())
	{
		if (Comment && Comment->Text.StartsWith(TEXT("DreamShader: ")))
		{
			Boxes.Add(Comment.Get());
		}
	}
	TestTrue(TEXT("the two regions and the run after them are boxes"), Boxes.Num() >= 3);
	const auto BoxOf = [&Boxes](const UMaterialExpression* Expression) -> const UMaterialExpressionComment*
	{
		const UMaterialExpressionComment* Best = nullptr;
		for (const UMaterialExpressionComment* Box : Boxes)
		{
			const bool bInside = Expression->MaterialExpressionEditorX >= Box->MaterialExpressionEditorX
				&& Expression->MaterialExpressionEditorY >= Box->MaterialExpressionEditorY
				&& Expression->MaterialExpressionEditorX < Box->MaterialExpressionEditorX + Box->SizeX
				&& Expression->MaterialExpressionEditorY < Box->MaterialExpressionEditorY + Box->SizeY;
			if (bInside && (!Best || Box->SizeX * Box->SizeY < Best->SizeX * Best->SizeY))
			{
				Best = Box;
			}
		}
		return Best;
	};
	int32 Wires = 0;
	for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
	{
		if (!Expression)
		{
			continue;
		}
		for (int32 InputIndex = 0; ; ++InputIndex)
		{
			const FExpressionInput* Input = Expression->GetInput(InputIndex);
			if (!Input)
			{
				break;
			}
			if (!Input->Expression)
			{
				continue;
			}
			++Wires;
			const UMaterialExpressionComment* From = BoxOf(Input->Expression);
			const UMaterialExpressionComment* To = BoxOf(Expression);
			// The usages beside the material's node stand in no box, and neither does what they feed.
			if (From && To)
			{
				TestTrue(FString::Printf(TEXT("a wire into '%s' starts in the same box ('%s' -> '%s')"), *Expression->GetName(), *From->Text, *To->Text), From == To);
			}
		}
	}
	TestTrue(TEXT("the graph has wires to check"), Wires >= 5);

	// ----- and the decompiler still reads the source's graph: reroutes are looked through, the two constants are one
	FString LoadError;
	UObject* Product = LoadDreamShaderDecompileSourceProduct(Fixture.GetSourceFilePath(), LoadError);
	if (!TestNotNull(FString::Printf(TEXT("the product loads for the decompile (%s)"), *LoadError), Product))
	{
		return false;
	}
	::UE::DreamShader::Editor::FDreamShaderDecompileRequest Request;
	Request.Asset = Product;
	Request.Format = ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Dss;
	Request.SourceFilePath = Fixture.GetSourceFilePath();
	const ::UE::DreamShader::Editor::FDreamShaderDecompileResult Decompiled = RunDreamShaderDecompileRequest(Request);
	if (!TestTrue(FString::Printf(TEXT("the decompile succeeds (%s)"), *DescribeDreamShaderDecompileFailure(Decompiled)), Decompiled.bSucceeded))
	{
		return false;
	}
	TestFalse(TEXT("the text names no layout reroute"), Decompiled.SourceText.Contains(TEXT("DS_Albedo")));

	FDreamShaderIRRunOptions RunOptions;
	FDreamShaderIRRun Original;
	FDreamShaderIRRun ReadBack;
	RunDreamShaderIRPipeline(TEXT("M_C2Blocks.dss"), Source, RunOptions, Original);
	RunDreamShaderIRPipeline(TEXT("M_C2Blocks.dss"), Decompiled.SourceText, RunOptions, ReadBack);
	if (TestTrue(FString::Printf(TEXT("both texts lower (%s | %s)"), *Original.ErrorText(), *ReadBack.ErrorText()), Original.Succeeded() && ReadBack.Succeeded()))
	{
		UE::DreamShader::IR::FIRCompareOptions CompareOptions;
		FString Difference;
		const bool bSame = UE::DreamShader::IR::AreDreamShaderIRModulesEquivalent(*Original.Module, *ReadBack.Module, CompareOptions, Difference);
		TestTrue(FString::Printf(TEXT("the decompiled text is the source's graph (%s)"), *Difference), bSame);
		if (!bSame)
		{
			AddInfo(Decompiled.SourceText);
		}
	}
	return true;
}

// A10: the oracle's normalisation of a foreign graph's inline masks, on the decompiler's reading of what a mask means.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2InlineMaskNormalisationTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.InlineMaskNormalisation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2InlineMaskNormalisationTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	const FString AssetName = TEXT("M_C2InlineMask");
	FDreamShaderCompile2Fixture Fixture(AssetName, TEXT("Compiler2"));
	UMaterial* Material = CompileSmokeFixture<UMaterial>(*this, Fixture, TEXT(
		"#pragma material(ShadingModel = Unlit, BlendMode = Opaque)\n"
		"uniform float4 Tint = float4(1, 0.5, 0.25, 1);\n"
		"export void M_C2InlineMask(inout material m)\n"
		"{\n"
		"    m.EmissiveColor = Tint.rgb;\n"
		"    m.Opacity = Tint.a;\n"
		"}\n"), AssetName);
	if (!Material)
	{
		return false;
	}

	UMaterialExpression* Tint = nullptr;
	for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
	{
		if (Expression && Expression->GetParameterName() == FName(TEXT("Tint")))
		{
			Tint = Expression.Get();
		}
	}
	FExpressionInput* Emissive = Material->GetExpressionInputForProperty(MP_EmissiveColor);
	FExpressionInput* Opacity = Material->GetExpressionInputForProperty(MP_Opacity);
	if (!TestNotNull(TEXT("the Tint parameter"), Tint) || !TestNotNull(TEXT("EmissiveColor"), Emissive) || !TestNotNull(TEXT("Opacity"), Opacity))
	{
		return false;
	}

	// Rewire both pins the way 1.x did: the whole value behind an inline mask. On a VectorParameter the whole value is the
	// RGBA output (index 5); output 0 is RGB, which has no alpha for `.ga` to keep.
	const int32 RGBAIndex = Tint->Outputs.IndexOfByPredicate([](const FExpressionOutput& Output) { return Output.OutputName == FName(TEXT("RGBA")); });
	if (!TestTrue(TEXT("the parameter publishes an RGBA output"), RGBAIndex != INDEX_NONE))
	{
		return false;
	}
	const auto MaskOnWhole = [Tint, RGBAIndex](FExpressionInput& Input, const bool bR, const bool bG, const bool bB, const bool bA)
	{
		Input.Expression = Tint;
		Input.OutputIndex = RGBAIndex;
		Input.Mask = 1;
		Input.MaskR = bR ? 1 : 0;
		Input.MaskG = bG ? 1 : 0;
		Input.MaskB = bB ? 1 : 0;
		Input.MaskA = bA ? 1 : 0;
	};
	MaskOnWhole(*Emissive, true, true, true, false);
	// `.ga`: no named output publishes that, so it has to become a ComponentMask node.
	MaskOnWhole(*Opacity, false, true, false, true);

	NormaliseLegacyInlineMasks(Material);

	TestEqual(TEXT("no inline mask is left on EmissiveColor"), Emissive->Mask, 0);
	TestEqual(TEXT("no inline mask is left on Opacity"), Opacity->Mask, 0);

	const int32 RGBIndex = Tint->Outputs.IndexOfByPredicate([](const FExpressionOutput& Output) { return Output.OutputName == FName(TEXT("RGB")); });
	TestTrue(TEXT("`.rgb` of the whole value is the parameter's own RGB output"), Emissive->Expression == Tint && Emissive->OutputIndex == RGBIndex);

	const UMaterialExpressionComponentMask* MaskNode = Cast<UMaterialExpressionComponentMask>(Opacity->Expression);
	if (TestNotNull(TEXT("`.ga` is a ComponentMask node"), MaskNode))
	{
		TestTrue(TEXT("keeping G and A"), !MaskNode->R && MaskNode->G && !MaskNode->B && MaskNode->A);
		TestTrue(TEXT("of the whole value"), MaskNode->Input.Expression == Tint && MaskNode->Input.OutputIndex == RGBAIndex);
	}
	return true;
}

// L10 (CO): a 1.x block lands where its Name= says, never where its file is.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2LegacyDestinationTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.LegacyDestination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2LegacyDestinationTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	FDreamShaderCompile2Fixture Fixture(TEXT("LegacyDestination"), TEXT("Compiler2"), TEXT("dsm"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	// A folder inside Name=, and a file stem that is not the asset's name: neither the source folder nor the file name
	// may show up in the destination.
	const FString Source = FString::Printf(TEXT(
		"Shader(Name=\"%s\", Root=\"Game\")\n"
		"{\n"
		"    Settings = { ShadingModel = \"Unlit\"; Backend = \"Graph\"; }\n"
		"    Outputs = { vec3 Color; Base.EmissiveColor = Color; }\n"
		"    Graph = { Color = vec3(1.0, 0.5, 0.25); }\n"
		"}\n"), *Fixture.MakeLegacyAssetName(TEXT("Folder/M_C2LegacyDest")));
	if (!Fixture.WriteSource(*this, Source))
	{
		return false;
	}

	UE::DreamShader::FDreamShaderError Error;
	if (!TestTrue(FString::Printf(TEXT("the `.dsm` compiles (%s: %s)"), *Error.Code, *Error.Message),
			::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, /*bForce*/ true)))
	{
		return false;
	}

	const FString Expected = FString::Printf(TEXT("%s/Folder/M_C2LegacyDest.M_C2LegacyDest"), *Fixture.GetPackagePath());
	Fixture.TrackObjectPath(Expected);
	TestNotNull(TEXT("the asset is where Name= says"), LoadObject<UMaterial>(nullptr, *Expected));
	TestNull(TEXT("and not where the file's own name would put it"), FindObject<UMaterial>(nullptr, *Fixture.MakeObjectPath(TEXT("LegacyDestination"))));
	return true;
}

// `extern` to a material layer, called from an entry.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2ExternLayerTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Smoke.ExternLayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2ExternLayerTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	// The layer first, as an asset of its own file.
	FDreamShaderCompile2Fixture LayerFixture(TEXT("ML_C2ExternLayer"), TEXT("Compiler2"));
	UMaterialFunctionMaterialLayer* Layer = CompileSmokeFixture<UMaterialFunctionMaterialLayer>(*this, LayerFixture, TEXT(
		"/// @layer\n"
		"export void ML_C2ExternLayer(inout material m)\n"
		"{\n"
		"    m.BaseColor = m.BaseColor * float3(1, 0.5, 0.25);\n"
		"}\n"), TEXT("ML_C2ExternLayer"));
	if (!Layer)
	{
		return false;
	}

	// Then a material that reaches it through a prototype.
	FDreamShaderCompile2Fixture MaterialFixture(TEXT("M_C2ExternLayer"), TEXT("Compiler2"));
	const FString Source = FString::Printf(TEXT(
		"#pragma material(ShadingModel = DefaultLit, BlendMode = Opaque)\n"
		"\n"
		"/// @asset %s/ML_C2ExternLayer\n"
		"extern void ML_C2ExternLayer(inout material m);\n"
		"\n"
		"export void M_C2ExternLayer(inout material m)\n"
		"{\n"
		"    m.BaseColor = float3(1, 1, 1);\n"
		"    ML_C2ExternLayer(m);\n"
		"    m.Roughness = 0.5;\n"
		"}\n"), *LayerFixture.GetPackagePath());
	UMaterial* Material = CompileSmokeFixture<UMaterial>(*this, MaterialFixture, Source, TEXT("M_C2ExternLayer"));
	if (!Material)
	{
		return false;
	}

	int32 Calls = 0;
	for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
	{
		const UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expression.Get());
		if (Call)
		{
			++Calls;
			TestTrue(TEXT("the call is bound to the layer asset"), Call->MaterialFunction == Layer);
		}
	}
	TestEqual(TEXT("one call to the layer"), Calls, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
