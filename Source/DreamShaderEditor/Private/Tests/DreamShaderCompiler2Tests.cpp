// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Compiler2.* -- the 2.0 pipeline end to end, and the parity oracle against the 1.x
// generator it replaces (plan section 8, item 1).
//
// The smoke tests drive a `.dss` through UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile -- whose `.dss`
// hook routes it into the new pipeline and never into the 1.x code -- and assert on the asset that
// came out: the graph, the provenance metadata, the source-span table, the skip-when-current rule,
// and the divergence refusal. They deliberately reuse the 1.x provenance helpers rather than
// reimplementing them: reusing the digest and the metadata is the whole shape of batch 1, and a
// test that accepted a second implementation of them would not notice if the new pipeline had one.
//
// The parity tests compile the SAME material twice -- once from its 1.x `.dsm`/`.dsf` twin in the
// DShader roots and once from its 2.0 `.dss` in Tests/Corpus/Lang/Examples -- dump both with
// BuildDreamShaderGraphDumpJson, normalise away the two things that name WHERE an asset came from,
// and diff. A difference is a bug in the new pipeline until proven otherwise; the only differences
// this file forgives are the ones that come from the two SOURCES being written differently rather
// than compiled differently, and each of those is named at its use with the reason.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Commandlet/DreamShaderGraphDump.h"
#include "Compiler/DreamShaderIREmitterInternal.h"
#include "Decompiler/DreamShaderGraphDecompilerHelpers.h"
#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"
#include "MaterialAssetGeneration/DreamShaderGeneratedAssetDigest.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComponentMask.h"
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

	/** The batch-1 smoke material: a helper to inline, two uniforms, a reflected call, a swizzle. */
	inline FString MakeSmokeMaterialSource(const FString& AssetName)
	{
		return FString::Printf(TEXT(
			"// The batch-1 end-to-end material.\n"
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
	// The 1.x twins
	// =============================================================================================

	/** Every DShader source root the project has, in the order the plan lists them. */
	inline TArray<FString> GetDreamShaderSourceRoots()
	{
		TArray<FString> Roots;
		Roots.Add(UE::DreamShader::GetSourceShaderDirectory());

		const TCHAR* PluginNames[] = { TEXT("MoonToon"), TEXT("DreamGUI"), TEXT("DreamDynamicWorld") };
		for (const TCHAR* PluginName : PluginNames)
		{
			if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
			{
				Roots.Add(FPaths::Combine(Plugin->GetBaseDir(), TEXT("DShader")));
			}
		}
		return Roots;
	}

	/** The first file with this leaf name under any DShader root, or empty. */
	inline FString FindLegacyTwin(const FString& LeafFileName)
	{
		for (const FString& Root : GetDreamShaderSourceRoots())
		{
			if (Root.IsEmpty() || !IFileManager::Get().DirectoryExists(*Root))
			{
				continue;
			}

			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(Files, *Root, *LeafFileName, true, false, false);
			if (Files.Num() > 0)
			{
				Files.Sort();
				return FPaths::ConvertRelativePathToFull(Files[0]);
			}
		}
		return FString();
	}

	/**
	 * Retarget a 1.x source at a scratch asset path.
	 *
	 * A parity run must NOT compile the twin where it really lives: `M_TeleportGlow.dsm` names
	 * `/Game/FX/M_TeleportGlow`, which is a real asset in this project, and a transient request for
	 * an asset that exists on disk is downgraded to a persisted one (IsGeneratedAssetPersisted) --
	 * so "just compile it in memory" would rewrite the user's material. Rewriting the block header's
	 * `Name=` and `Root=` moves the whole thing somewhere nothing else lives, which is also what
	 * makes the two dumps comparable: neither side is then named after where it came from.
	 *
	 * Only the LAST block header is rewritten, which is the product block: a `.dsf` opens with the
	 * `VirtualFunction(Name="...")` prototypes whose names are call targets, not asset paths.
	 */
	inline bool RetargetLegacySource(const FString& Source, const FString& AssetPath, FString& OutSource)
	{
		const TCHAR* BlockKeywords[] = { TEXT("ShaderLayerBlend("), TEXT("ShaderLayer("), TEXT("ShaderFunction("), TEXT("Shader(") };

		int32 BlockStart = INDEX_NONE;
		for (const TCHAR* Keyword : BlockKeywords)
		{
			const int32 Index = Source.Find(Keyword, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (Index != INDEX_NONE && Index > BlockStart)
			{
				BlockStart = Index;
			}
		}
		if (BlockStart == INDEX_NONE)
		{
			return false;
		}

		const int32 HeaderEnd = Source.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, BlockStart);
		if (HeaderEnd == INDEX_NONE)
		{
			return false;
		}

		FString Header = Source.Mid(BlockStart, HeaderEnd - BlockStart + 1);

		auto ReplaceKey = [&Header](const TCHAR* Key, const FString& Value) -> bool
		{
			const FString Needle = FString::Printf(TEXT("%s=\""), Key);
			const int32 Start = Header.Find(Needle, ESearchCase::CaseSensitive);
			if (Start == INDEX_NONE)
			{
				return false;
			}
			const int32 ValueStart = Start + Needle.Len();
			const int32 ValueEnd = Header.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
			if (ValueEnd == INDEX_NONE)
			{
				return false;
			}
			Header = Header.Left(ValueStart) + Value + Header.Mid(ValueEnd);
			return true;
		};

		if (!ReplaceKey(TEXT("Name"), AssetPath))
		{
			return false;
		}
		// Root is optional in 1.x; when it is there it has to come back to Game so the retargeted
		// path means what it says.
		ReplaceKey(TEXT("Root"), TEXT("Game"));

		OutSource = Source.Left(BlockStart) + Header + Source.Mid(HeaderEnd + 1);
		return true;
	}

	/** One parity pair, and the keys whose difference is the SOURCES' fault rather than a compiler's. */
	struct FParityPair
	{
		/** Leaf name under Tests/Corpus/Lang/Examples. */
		const TCHAR* LangExample;
		/** Leaf name of the 1.x twin, looked for under every DShader root. */
		const TCHAR* LegacyTwin;
		/** The asset leaf both sides are retargeted to produce. */
		const TCHAR* AssetLeaf;
		/** Dump keys dropped from BOTH sides, with the reason, before the diff. */
		TArray<FString> SourceLevelDeltas;
		/**
		 * Rewrite every inline mask of the 1.x graph by the emitter's own swizzle rule before it is
		 * dumped (Plan/v2-parity-deltas.md PD-1). The 2.0 emitter never writes an inline mask, so without
		 * this every `.rgb` of a 1.x graph reads as a structural difference that is not one.
		 */
		bool bNormaliseLegacyInlineMasks = true;
		/**
		 * Whole-line substitutions applied to the 1.x dump: the left side is matched against the trimmed
		 * line and replaced keeping the indentation (PS-1, a pin the two sources name differently). An
		 * entry that matches no line fails the pair, because a stale substitution would be a silent lie.
		 */
		TArray<TPair<FString, FString>> LegacyLineSubstitutions;
	};

	/**
	 * Rewrites every inline mask of a 1.x graph the way the 2.0 emitter spells the same swizzle
	 * (Plan/v2-parity-deltas.md PD-1): an identity mask disappears, a leading-channel mask moves the wire
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

			// The operand's channels: a masked output's own, else every channel of its width.
			TArray<int32> Channels;
			const FExpressionOutput* Output = Source->Outputs.IsValidIndex(OutputIndex) ? &Source->Outputs[OutputIndex] : nullptr;
			if (Output && Output->Mask)
			{
				if (Output->MaskR) { Channels.Add(0); }
				if (Output->MaskG) { Channels.Add(1); }
				if (Output->MaskB) { Channels.Add(2); }
				if (Output->MaskA) { Channels.Add(3); }
			}
			else
			{
				const int32 Width = FMath::Clamp(UE::DreamShader::Editor::Private::GetExpressionOutputComponentCount(Source, OutputIndex), 1, 4);
				for (int32 Channel = 0; Channel < Width; ++Channel)
				{
					Channels.Add(Channel);
				}
			}

			// The inline mask relative to the operand, spelled the way the IR spells a Swizzle.
			const bool bPicked[4] = { Input.MaskR != 0, Input.MaskG != 0, Input.MaskB != 0, Input.MaskA != 0 };
			FString Relative;
			for (int32 Position = 0; Position < Channels.Num(); ++Position)
			{
				if (bPicked[Channels[Position]])
				{
					Relative.AppendChar(TEXT("xyzw")[Position]);
				}
			}

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
			if (Relative.Len() == Channels.Num())
			{
				// The identity: the IR builder never makes that Swizzle.
				Rewire(Source, OutputIndex);
			}
			else if (UE::DreamShader::Editor::Compiler::TryResolveSwizzleAsNamedOutput(Source, OutputIndex, Channels.Num(), Relative, Selected))
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

	/**
	 * Compile one source, dump every asset it produced, and hand back the normalised text.
	 *
	 * The caller owns the scratch fixture, so both halves of a parity pair are cleaned up together
	 * whichever of them failed.
	 */
	inline bool CompileAndDump(
		FAutomationTestBase& Test,
		const FString& SourceFilePath,
		const FString& PackagePath,
		TArray<FString>& InOutCleanupPaths,
		FString& OutDump,
		FString& OutError,
		const bool bNormaliseInlineMasks = false)
	{
		UE::DreamShader::FDreamShaderError Error;
		const bool bCompiled = UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(
			SourceFilePath, Error, /*bForce*/ true, /*bTransient*/ false);
		OutError = Error.Code.IsEmpty() ? Error.Message : FString::Printf(TEXT("%s: %s"), *Error.Code, *Error.Message);

		if (!bCompiled)
		{
			return false;
		}

		FAssetRegistryModule& AssetRegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		TArray<FString> ScanPaths;
		ScanPaths.Add(PackagePath);
		AssetRegistry.ScanPathsSynchronous(ScanPaths, /*bForceRescan*/ true);

		FARFilter Filter;
		Filter.PackagePaths.Add(FName(*PackagePath));
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);

		TArray<TPair<FString, FString>> Dumps;
		for (const FAssetData& AssetData : Assets)
		{
			UObject* Asset = AssetData.GetAsset();
			if (!Asset)
			{
				continue;
			}
			InOutCleanupPaths.AddUnique(Asset->GetPathName());
			if (bNormaliseInlineMasks)
			{
				NormaliseLegacyInlineMasks(Asset);
			}
			Dumps.Add({ Asset->GetName(), NormaliseDreamShaderGraphDumpJson(
				UE::DreamShader::Editor::Private::BuildDreamShaderGraphDumpJson(Asset, FString(), nullptr)) });
		}

		if (Dumps.Num() == 0)
		{
			OutError = TEXT("the compile reported success but produced no asset the registry could see");
			return false;
		}

		Dumps.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B)
		{
			return A.Key.Compare(B.Key, ESearchCase::CaseSensitive) < 0;
		});

		TArray<FString> Parts;
		for (const TPair<FString, FString>& Dump : Dumps)
		{
			Parts.Add(FString::Printf(TEXT("=== %s ==="), *Dump.Key));
			Parts.Add(Dump.Value);
		}
		OutDump = FString::Join(Parts, TEXT("\n"));
		return true;
	}

	/** Delete every asset a parity run made, and the two scratch sources. */
	inline void CleanUpParityRun(const TArray<FString>& ObjectPaths, const TArray<FString>& SourceFilePaths)
	{
		TArray<UObject*> ObjectsToDelete;
		for (const FString& ObjectPath : ObjectPaths)
		{
			if (UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath))
			{
				ObjectsToDelete.Add(Asset);
			}
		}
		if (ObjectsToDelete.Num() > 0)
		{
			ObjectTools::DeleteObjectsUnchecked(ObjectsToDelete);
		}

		for (const FString& SourceFilePath : SourceFilePaths)
		{
			IFileManager::Get().Delete(*SourceFilePath, false, true);
		}
	}

	/** `<Corpus>/Lang/Examples/<Leaf>`. */
	inline FString GetLangExamplePath(const TCHAR* Leaf)
	{
		const FString Root = GetDreamShaderCorpusRoot();
		return Root.IsEmpty() ? FString() : FPaths::Combine(Root, TEXT("Lang"), TEXT("Examples"), Leaf);
	}

	/** The scratch source path a parity half writes to. */
	inline FString MakeParitySourcePath(const FString& Area, const FString& Leaf)
	{
		return UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(
			UE::DreamShader::GetSourceShaderDirectory(),
			TEXT("DreamShaderTests"),
			TEXT("Parity"),
			Area,
			Leaf));
	}

	/** The package path that scratch source's assets land in. */
	inline FString MakeParityPackagePath(const FString& Area)
	{
		return FString::Printf(TEXT("/Game/DreamShaderTests/Parity/%s"), *Area);
	}

	/** Run one parity pair. Returns false only when the comparison itself could not be made. */
	inline bool RunParityPair(FAutomationTestBase& Test, const FParityPair& Pair)
	{
		const FString ExamplePath = GetLangExamplePath(Pair.LangExample);
		FString ExampleText;
		if (!FFileHelper::LoadFileToString(ExampleText, *ExamplePath))
		{
			Test.AddError(FString::Printf(TEXT("Cannot read the 2.0 example '%s'."), *ExamplePath));
			return false;
		}

		const FString TwinPath = FindLegacyTwin(Pair.LegacyTwin);
		if (TwinPath.IsEmpty())
		{
			// Not a failure of the compiler: the oracle simply has nothing to compare against in
			// this checkout. A warning, never silence -- an oracle that stops looking without
			// saying so is worse than no oracle.
			Test.AddWarning(FString::Printf(
				TEXT("Parity skipped for '%s': no 1.x twin named '%s' under any DShader root."),
				Pair.LangExample, Pair.LegacyTwin));
			return true;
		}

		FString TwinText;
		if (!FFileHelper::LoadFileToString(TwinText, *TwinPath))
		{
			Test.AddError(FString::Printf(TEXT("Cannot read the 1.x twin '%s'."), *TwinPath));
			return false;
		}

		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
		const FString LegacyArea = FString::Printf(TEXT("Legacy_%s"), *Unique);
		const FString Lang2Area = FString::Printf(TEXT("Lang2_%s"), *Unique);

		FString RetargetedTwin;
		if (!RetargetLegacySource(
				TwinText,
				FString::Printf(TEXT("DreamShaderTests/Parity/%s/%s"), *LegacyArea, Pair.AssetLeaf),
				RetargetedTwin))
		{
			Test.AddError(FString::Printf(
				TEXT("Could not retarget the 1.x twin '%s'; it has no recognisable block header."), *TwinPath));
			return false;
		}

		const FString LegacySourcePath = MakeParitySourcePath(LegacyArea, FPaths::GetCleanFilename(TwinPath));
		const FString Lang2SourcePath = MakeParitySourcePath(Lang2Area, FString(Pair.LangExample));
		const FString LegacyPackagePath = MakeParityPackagePath(LegacyArea);
		const FString Lang2PackagePath = MakeParityPackagePath(Lang2Area);

		TArray<FString> CleanupObjects;
		TArray<FString> CleanupSources;
		CleanupSources.Add(LegacySourcePath);
		CleanupSources.Add(Lang2SourcePath);
		ON_SCOPE_EXIT { CleanUpParityRun(CleanupObjects, CleanupSources); };

		// Suppressions, never requirements: the new-asset probe fires on some engine builds only.
		Test.AddExpectedError(LegacyPackagePath, EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(Lang2PackagePath, EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(LegacySourcePath), true);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Lang2SourcePath), true);
		if (!FFileHelper::SaveStringToFile(RetargetedTwin, *LegacySourcePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
			|| !FFileHelper::SaveStringToFile(ExampleText, *Lang2SourcePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			Test.AddError(TEXT("Could not write the parity scratch sources."));
			return false;
		}

		// The goldens of both halves describe GRAPH-backend output.
		FScopedDreamShaderGraphBackendPin BackendPin;

		FString LegacyDump;
		FString LegacyError;
		if (!CompileAndDump(Test, LegacySourcePath, LegacyPackagePath, CleanupObjects, LegacyDump, LegacyError, Pair.bNormaliseLegacyInlineMasks))
		{
			Test.AddWarning(FString::Printf(
				TEXT("Parity skipped for '%s': the 1.x twin did not compile in this checkout (%s)."),
				Pair.LangExample, *LegacyError));
			return true;
		}

		FString Lang2Dump;
		FString Lang2Error;
		if (!CompileAndDump(Test, Lang2SourcePath, Lang2PackagePath, CleanupObjects, Lang2Dump, Lang2Error))
		{
			Test.AddError(FString::Printf(
				TEXT("The 2.0 pipeline failed to compile '%s': %s"), Pair.LangExample, *Lang2Error));
			return false;
		}

		FString LegacyFiltered = FilterDreamShaderGraphDumpKeys(LegacyDump, Pair.SourceLevelDeltas);
		if (Pair.LegacyLineSubstitutions.Num() > 0)
		{
			TArray<FString> Lines;
			LegacyFiltered.ParseIntoArray(Lines, TEXT("\n"), /*bCullEmpty*/ false);
			for (const TPair<FString, FString>& Substitution : Pair.LegacyLineSubstitutions)
			{
				int32 Hits = 0;
				for (FString& Line : Lines)
				{
					if (Line.TrimStartAndEnd().Equals(Substitution.Key, ESearchCase::CaseSensitive))
					{
						const int32 Indent = Line.Len() - Line.TrimStart().Len();
						Line = Line.Left(Indent) + Substitution.Value;
						++Hits;
					}
				}
				if (Hits == 0)
				{
					Test.AddError(FString::Printf(
						TEXT("'%s': the 1.x dump has no line '%s' to substitute, so the substitution is stale."),
						Pair.LangExample, *Substitution.Key));
				}
			}
			LegacyFiltered = FString::Join(Lines, TEXT("\n"));
		}
		const FString Lang2Filtered = FilterDreamShaderGraphDumpKeys(Lang2Dump, Pair.SourceLevelDeltas);

		const bool bEqual = LegacyFiltered.Equals(Lang2Filtered, ESearchCase::CaseSensitive);
		Test.TestTrue(
			FString::Printf(TEXT("'%s' compiles to the same graph through both pipelines"), Pair.LangExample),
			bEqual);
		if (!bEqual)
		{
			Test.AddInfo(FString::Printf(
				TEXT("'%s': %s"), Pair.LangExample,
				*DescribeDreamShaderTextDifference(Lang2Filtered, LegacyFiltered)));
			Test.AddInfo(FString::Printf(TEXT("1.x dump:\n%s"), *LegacyFiltered));
			Test.AddInfo(FString::Printf(TEXT("2.0 dump:\n%s"), *Lang2Filtered));
		}

		return true;
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
			UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(Fixture.GetSourceFilePath(), Error, /*bForce*/ true)))
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
	// and makes no node (CONTRACT 6.13 #22; the earlier "is a ComponentMask node" predates #22 and
	// asserted the wrong half of plan 3.3). What plan 3.3 rules out is the inline FExpressionInput
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
// The source-span table -- decision 11 #10, plan 13.2
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
			UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(Fixture.GetSourceFilePath(), Error, /*bForce*/ true)))
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

		// CONTRACT 6.13 #6: `callFile` is written only when the call site lies in a DIFFERENT file
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
			UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(Fixture.GetSourceFilePath(), Error, /*bForce*/ true)))
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
		UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(Fixture.GetSourceFilePath(), SkipError, /*bForce*/ false));
	TestTrue(TEXT("and the source is still current"),
		IsGeneratedAssetSourceCurrent(Material, Fixture.GetSourceFilePath(), FirstHash));

	// -Force rebuilds, and an identical rebuild reproduces the digest byte for byte. A digest that
	// moved without the source moving would make every asset in a project read as hand-edited.
	UE::DreamShader::FDreamShaderError ForceError;
	if (TestTrue(TEXT("-Force rebuilds"),
			UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(Fixture.GetSourceFilePath(), ForceError, /*bForce*/ true)))
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
		UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(Fixture.GetSourceFilePath(), UnchangedError, /*bForce*/ false));
	TestTrue(TEXT("and the skip left the edit alone"), Material->TwoSided != 0);

	// Move the source. Without this the compile never reaches the gate, and the refusal below would
	// prove nothing.
	if (!Fixture.WriteSource(*this, MakeSmokeMaterialSource(AssetName).Replace(TEXT("Intensity = 0.7"), TEXT("Intensity = 0.8"))))
	{
		return false;
	}

	UE::DreamShader::FDreamShaderError DivergedError;
	const bool bRebuilt = UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(
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
		UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(Fixture.GetSourceFilePath(), ForcedError, /*bForce*/ true));
	TestTrue(TEXT("and still leaves the edit alone"), Material->TwoSided != 0);

	// Revert: what the user's confirmation of the Revert dialog reaches the generator as.
	bool bReverted = false;
	{
		FScopedDreamShaderRevertDiverged RevertScope;
		UE::DreamShader::FDreamShaderError RevertError;
		bReverted = UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(Fixture.GetSourceFilePath(), RevertError, /*bForce*/ true);
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
			UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(Fixture.GetSourceFilePath(), Error, /*bForce*/ true)))
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
			UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(Fixture.GetSourceFilePath(), Error, /*bForce*/ true)))
	{
		AddInfo(FString::Printf(TEXT("the pipeline reported: %s: %s"), *Error.Code, *Error.Message));
		return false;
	}

	// One asset per export, and every one of them a UMaterialFunction (CONTRACT 6.9).
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

	// The inner function's inputs keep declaration order with dense sort priorities (plan 6.2):
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
// Parity with the 1.x generator -- plan section 8, item 1
// =================================================================================================

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2ParityMaterialTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Parity.Material",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2ParityMaterialTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FParityPair Pair;
	Pair.LangExample = TEXT("M_TeleportGlow.dss");
	Pair.LegacyTwin = TEXT("M_TeleportGlow.dsm");
	Pair.AssetLeaf = TEXT("M_TeleportGlow");
	// The two sources are not a literal translation of one another -- the 1.x one predates the 2.0
	// syntax and gives its parameters sort priorities and slightly different prose. These three keys
	// are therefore a difference of AUTHORSHIP, not of compilation, and dropping them is what leaves
	// the graph itself as the thing being compared. Nothing structural is on this list: a node, a
	// connection, a class, a default value or a material setting that differs is a real difference
	// and fails.
	Pair.SourceLevelDeltas.Add(TEXT("SortPriority"));   // the .dsm sets 10/20/30; the .dss sets none
	Pair.SourceLevelDeltas.Add(TEXT("Group"));          // "Glow | Look" vs "Glow|Look"
	Pair.SourceLevelDeltas.Add(TEXT("Description"));    // "brightness pulse" vs "breathing pulse"
	// PD-3 (Plan/v2-parity-deltas.md): the one Custom node differs in FORM, not in value. 1.x puts the
	// `Function` body in a generated include and the node calls DreamShaderFn_GlowMask from it; 2.0 writes
	// the body into the node between source markers (CONTRACT 6.13 #3). The node's class, inputs, output
	// type and connections all stay compared; only the two keys that carry the form are dropped.
	Pair.SourceLevelDeltas.Add(TEXT("Code"));
	Pair.SourceLevelDeltas.Add(TEXT("IncludeFilePaths"));

	return RunParityPair(*this, Pair);
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompiler2ParityFunctionTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Parity.Function",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompiler2ParityFunctionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Compiler2Tests;

	FParityPair Pair;
	Pair.LangExample = TEXT("MF_ToonUV.dss");
	Pair.LegacyTwin = TEXT("MF_ToonUV.dsf");
	Pair.AssetLeaf = TEXT("MF_ToonUV");
	// Two differences between the graphs are not compiler bugs, and both are absorbed exactly rather than
	// by dropping a key (Plan/v2-parity-deltas.md):
	//  - PD-1: `ScaleOffset.rg` / `.ba` are inline masks on the FunctionInput's wires in 1.x and
	//    ComponentMask nodes in 2.0, which never writes an inline mask. The 1.x graph is normalised by the
	//    emitter's own rule before its dump (bNormaliseLegacyInlineMasks, on by default), which also
	//    settles `outputs[].type` (PD-2): the dump infers a width without seeing an inline mask.
	//  - PS-1: the output is `UV` in the .dsf and `Result` in the .dss, which returns its value
	//    (CONTRACT 6.9). A pin name is structural and `name` keys every input pin as well, so the four
	//    lines that carry it are substituted in the 1.x dump instead of a key being dropped.
	Pair.LegacyLineSubstitutions.Add({ TEXT("\"OutputName\": \"UV\","), TEXT("\"OutputName\": \"Result\",") });
	Pair.LegacyLineSubstitutions.Add({ TEXT("\"name\": \"UV\","), TEXT("\"name\": \"Result\",") });
	Pair.LegacyLineSubstitutions.Add({ TEXT("\"Name\": \"DS_UV_0\""), TEXT("\"Name\": \"DS_Result_0\"") });
	Pair.LegacyLineSubstitutions.Add({ TEXT("\"declaration\": \"DS_UV_0\""), TEXT("\"declaration\": \"DS_Result_0\"") });
	return RunParityPair(*this, Pair);
}

#endif // WITH_DEV_AUTOMATION_TESTS
