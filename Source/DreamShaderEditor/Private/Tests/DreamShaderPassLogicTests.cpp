// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Pass.Logic.* -- the Custom Pass runtime's decisions that need no renderer: the spellings the runtime and
// the language share, how parameter values blend, how an HLSL slot packs its parameters, what a pipeline asset's own
// checks refuse, and which pipelines the subsystem says apply to a view, in which order, with which values.
//
// Nothing here renders, so it all runs under -nullrhi. What the passes draw is DreamShader.Pass.Render.*.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamPassComponent.h"
#include "DreamPassPipeline.h"
#include "DreamPassSettings.h"
#include "DreamPassSubsystem.h"
#include "DreamPassTypes.h"
#include "DreamPassVolume.h"
#include "Lang/LangPipelineSource.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::PassLogicTests
{
	static FDreamPassParameterDesc MakeParameter(FName Name, const FDreamPassParameterValue& Default)
	{
		FDreamPassParameterDesc Desc;
		Desc.Name = Name;
		Desc.Default = Default;
		return Desc;
	}

	static FDreamPassBufferBinding MakeBinding(FName Slot, FName Buffer, bool bPrevious = false)
	{
		FDreamPassBufferBinding Binding;
		Binding.Slot = Slot;
		Binding.Buffer = Buffer;
		Binding.bPrevious = bPrevious;
		return Binding;
	}

	static FDreamPassParameterOverride MakeOverride(FName Name, const FDreamPassParameterValue& Value)
	{
		FDreamPassParameterOverride Override;
		Override.Name = Name;
		Override.Value = Value;
		return Override;
	}

	/** A pipeline with a Gain float, a Flag bool and one copy pass, which passes its own checks. */
	static UDreamPassPipeline* MakePipeline(const TCHAR* Name, int32 Order)
	{
		UDreamPassPipeline* Pipeline = NewObject<UDreamPassPipeline>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UDreamPassPipeline::StaticClass(), Name), RF_Transient);
		Pipeline->Order = Order;
		Pipeline->Parameters.Add(MakeParameter(TEXT("Gain"), FDreamPassParameterValue::MakeFloat(1.0f)));
		Pipeline->Parameters.Add(MakeParameter(TEXT("Flag"), FDreamPassParameterValue::MakeBool(true)));

		FDreamPassBufferDesc& Buffer = Pipeline->Buffers.AddDefaulted_GetRef();
		Buffer.Name = TEXT("Grab");
		Buffer.Format = EDreamPassBufferFormat::RGBA16F;

		FDreamPassDesc& Pass = Pipeline->Passes.AddDefaulted_GetRef();
		Pass.Name = TEXT("Copy");
		Pass.Kind = EDreamPassKind::Copy;
		Pass.Injection = EDreamPassInjection::BeforePostProcess;
		Pass.Reads.Add(MakeBinding(TEXT("SceneColor"), UE::DreamPass::BuiltinBuffers::SceneColor));
		Pass.Writes.Add(MakeBinding(TEXT("Grab"), TEXT("Grab")));
		return Pipeline;
	}

	/** A view query for a first local player's game view that offers everything. */
	static FDreamPassViewQuery MakeGameQuery()
	{
		FDreamPassViewQuery Query;
		Query.ViewKind = EDreamPassViewFlags::Game;
		Query.PlayerIndex = 0;
		Query.Capabilities = EDreamPassRequirementFlags::PostProcess | EDreamPassRequirementFlags::SceneResolve;
		return Query;
	}

	/** A game world with its subsystems, torn down with its world context when the scope ends. */
	struct FScopedTestWorld
	{
		UWorld* World = nullptr;

		FScopedTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
		}

		~FScopedTestWorld()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(/*bInformEngineOfWorld*/ false);
			}
		}

		UDreamPassSubsystem* GetSubsystem() const { return UDreamPassSubsystem::Get(World); }
	};

	/** An activation source that is not a volume or a component: one fixed activation, for any view. */
	struct FFixedSource : public IDreamPassActivationSource
	{
		UDreamPassPipeline* Pipeline = nullptr;
		float Priority = 0.0f;
		float Weight = 1.0f;
		TArray<FDreamPassParameterOverride> Overrides;

		virtual void GatherDreamPassActivations(const FDreamPassViewQuery& View, TArray<FDreamPassActivation>& OutActivations) const override
		{
			FDreamPassActivation& Activation = OutActivations.AddDefaulted_GetRef();
			Activation.Pipeline = Pipeline;
			Activation.Priority = Priority;
			Activation.Weight = Weight;
			Activation.Overrides = Overrides;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassInjectionNamesTest,
	"DreamShader.Pass.Logic.InjectionNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPassInjectionNamesTest::RunTest(const FString& Parameters)
{
	// The runtime (DreamPassTypes.cpp) and the language (LangPipelineSource.cpp) each keep the `.dsp` spellings, the
	// runtime to read assets and the language to bind sources without an engine. They must be one table.
	const TConstArrayView<const TCHAR*> LangInjections = UE::DreamShader::Lang::GetDreamShaderPassInjectionNames();
	TestEqual(TEXT("one spelling per injection point"), LangInjections.Num(), int32(EDreamPassInjection::Count));
	for (int32 Index = 0; Index < int32(EDreamPassInjection::Count) && Index < LangInjections.Num(); ++Index)
	{
		const EDreamPassInjection Injection = EDreamPassInjection(Index);
		const FString RuntimeName = UE::DreamPass::LexToString(Injection);
		TestEqual(FString::Printf(TEXT("injection %d"), Index), FString(LangInjections[Index]), RuntimeName);

		EDreamPassInjection Parsed = EDreamPassInjection::Count;
		TestTrue(FString::Printf(TEXT("%s parses"), *RuntimeName), UE::DreamPass::LexTryParse(RuntimeName, Parsed) && Parsed == Injection);
	}

	const TConstArrayView<const TCHAR*> LangFormats = UE::DreamShader::Lang::GetDreamShaderPassFormatNames();
	const int32 FormatCount = int32(EDreamPassBufferFormat::Depth32) + 1;
	TestEqual(TEXT("one spelling per buffer format"), LangFormats.Num(), FormatCount);
	for (int32 Index = 0; Index < FormatCount && Index < LangFormats.Num(); ++Index)
	{
		const EDreamPassBufferFormat Format = EDreamPassBufferFormat(Index);
		TestEqual(FString::Printf(TEXT("format %d"), Index), FString(LangFormats[Index]), FString(UE::DreamPass::LexToString(Format)));
	}

	EDreamPassInjection Unused;
	TestFalse(TEXT("spellings are case-sensitive"), UE::DreamPass::LexTryParse(TEXT("beforepostprocess"), Unused));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassParameterBlendTest,
	"DreamShader.Pass.Logic.ParameterBlend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPassParameterBlendTest::RunTest(const FString& Parameters)
{
	const FDreamPassParameterValue A = FDreamPassParameterValue::MakeVector(EDreamPassParameterType::Float3, FVector4f(0.0f, 2.0f, 4.0f, 9.0f));
	const FDreamPassParameterValue B = FDreamPassParameterValue::MakeVector(EDreamPassParameterType::Float3, FVector4f(1.0f, 4.0f, 8.0f, 9.0f));

	TestEqual(TEXT("a narrower vector keeps its unused components at 0"), A.Vector.W, 0.0f);
	TestTrue(TEXT("numbers lerp"), A.BlendTowards(B, 0.5f).Vector.Equals(FVector4f(0.5f, 3.0f, 6.0f, 0.0f)));
	TestTrue(TEXT("weight 0 keeps"), A.BlendTowards(B, 0.0f).Identical(A));
	TestTrue(TEXT("weight 1 takes"), A.BlendTowards(B, 1.0f).Identical(B));

	const FDreamPassParameterValue Off = FDreamPassParameterValue::MakeBool(false);
	const FDreamPassParameterValue On = FDreamPassParameterValue::MakeBool(true);
	TestFalse(TEXT("a bool below half weight keeps"), Off.BlendTowards(On, 0.49f).Bool);
	TestTrue(TEXT("a bool from half weight switches"), Off.BlendTowards(On, 0.5f).Bool);

	const FDreamPassParameterValue Two = FDreamPassParameterValue::MakeInt(2);
	const FDreamPassParameterValue Six = FDreamPassParameterValue::MakeInt(6);
	TestEqual(TEXT("an int switches rather than lerping"), Two.BlendTowards(Six, 0.75f).Int, 6);
	TestEqual(TEXT("an int below half weight keeps"), Two.BlendTowards(Six, 0.25f).Int, 2);

	const FDreamPassParameterValue Float = FDreamPassParameterValue::MakeFloat(1.0f);
	TestTrue(TEXT("a value of another type does not blend in"), Float.BlendTowards(On, 1.0f).Identical(Float));

	TestTrue(TEXT("a bool reads as 1 in a vector"), On.AsVector().Equals(FVector4f(1.0f, 0.0f, 0.0f, 0.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassSlotLayoutTest,
	"DreamShader.Pass.Logic.SlotLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPassSlotLayoutTest::RunTest(const FString& Parameters)
{
	using EType = EDreamPassParameterType;

	auto Layout = [](const TArray<EType>& Types, TArray<FDreamPassSlotParamLocation>& Out)
	{
		TArray<FName> Names;
		for (int32 Index = 0; Index < Types.Num(); ++Index)
		{
			Names.Add(*FString::Printf(TEXT("P%d"), Index));
		}
		return UE::DreamPass::LayoutSlotParameters(Names, Types, Out);
	};

	auto Expect = [this](const TArray<FDreamPassSlotParamLocation>& Locations, int32 Index, int32 Vector, int32 Component, const TCHAR* What)
	{
		if (!TestTrue(FString::Printf(TEXT("%s: location %d exists"), What, Index), Locations.IsValidIndex(Index)))
		{
			return;
		}
		TestEqual(FString::Printf(TEXT("%s: vector of %d"), What, Index), Locations[Index].Vector, Vector);
		TestEqual(FString::Printf(TEXT("%s: component of %d"), What, Index), Locations[Index].Component, Component);
	};

	TArray<FDreamPassSlotParamLocation> Locations;

	// Scalars fill a vector in order.
	TestTrue(TEXT("four scalars"), Layout(TArray<EType>{ EType::Float, EType::Int, EType::Bool, EType::Float }, Locations));
	Expect(Locations, 0, 0, 0, TEXT("four scalars"));
	Expect(Locations, 3, 0, 3, TEXT("four scalars"));

	// A float3 takes xyz of a fresh vector and leaves w to the next scalar.
	TestTrue(TEXT("float3 then float"), Layout(TArray<EType>{ EType::Float3, EType::Float }, Locations));
	Expect(Locations, 0, 0, 0, TEXT("float3 then float"));
	Expect(Locations, 1, 0, 3, TEXT("float3 then float"));

	// A float2 takes the first free half; a float3 never shares a vector with one.
	TestTrue(TEXT("float2 float3 float2"), Layout(TArray<EType>{ EType::Float2, EType::Float3, EType::Float2 }, Locations));
	Expect(Locations, 0, 0, 0, TEXT("float2 float3 float2"));
	Expect(Locations, 1, 1, 0, TEXT("float2 float3 float2"));
	Expect(Locations, 2, 0, 2, TEXT("float2 float3 float2"));

	// Sixteen float4 fill the block exactly; a seventeenth does not fit.
	TArray<EType> Sixteen;
	Sixteen.Init(EType::Float4, UE::DreamPass::MaxSlotParamVectors);
	TestTrue(TEXT("sixteen float4 fit"), Layout(Sixteen, Locations));
	Sixteen.Add(EType::Float);
	TestFalse(TEXT("one more does not"), Layout(Sixteen, Locations));

	TestFalse(TEXT("a texture is not a slot parameter"), Layout(TArray<EType>{ EType::Texture }, Locations));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassPipelineValidateTest,
	"DreamShader.Pass.Logic.PipelineValidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPassPipelineValidateTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassLogicTests;

	UDreamPassPipeline* Pipeline = MakePipeline(TEXT("CP_LogicValid"), 0);
	TArray<FText> Problems;
	TestTrue(TEXT("a copy into a declared buffer passes"), Pipeline->Validate(&Problems));
	TestEqual(TEXT("no problem reported"), Problems.Num(), 0);

	Pipeline->NotifyChanged();
	TestTrue(TEXT("its pass is usable"), Pipeline->IsPassUsable(0));

	// Each change below breaks one rule the runtime relies on.
	auto ExpectInvalid = [this](const TCHAR* What, TFunctionRef<void(UDreamPassPipeline&)> Break)
	{
		UDreamPassPipeline* Broken = MakePipeline(TEXT("CP_LogicBroken"), 0);
		Break(*Broken);
		TArray<FText> Found;
		TestFalse(What, Broken->Validate(&Found));
		TestTrue(FString::Printf(TEXT("%s: says why"), What), Found.Num() > 0);
	};

	ExpectInvalid(TEXT("a read of an undeclared buffer"), [](UDreamPassPipeline& P) { P.Passes[0].Reads[0].Buffer = TEXT("Nowhere"); });
	ExpectInvalid(TEXT("a buffer declared twice"), [](UDreamPassPipeline& P) { P.Buffers.Add(P.Buffers[0]); });
	ExpectInvalid(TEXT("a buffer named like a built-in one"), [](UDreamPassPipeline& P) { P.Buffers[0].Name = UE::DreamPass::BuiltinBuffers::SceneDepth; });
	ExpectInvalid(TEXT("'.Previous' of a buffer without history"), [](UDreamPassPipeline& P) { P.Passes[0].Reads.Add(MakeBinding(TEXT("Old"), TEXT("Grab"), true)); });
	ExpectInvalid(TEXT("a fullscreen pass with nothing to draw"), [](UDreamPassPipeline& P) { P.Passes[0].Kind = EDreamPassKind::Fullscreen; });
	ExpectInvalid(TEXT("a compute pass without a slot"), [](UDreamPassPipeline& P) { P.Passes[0].Kind = EDreamPassKind::Compute; });
	ExpectInvalid(TEXT("a mesh pass that selects nothing"), [](UDreamPassPipeline& P) { P.Passes[0].Kind = EDreamPassKind::Mesh; });
	ExpectInvalid(TEXT("an exported buffer without its render target"), [](UDreamPassPipeline& P) { P.Buffers[0].bExport = true; });
	ExpectInvalid(TEXT("a param bound to an unknown parameter"), [](UDreamPassPipeline& P)
	{
		FDreamPassParamBinding& Binding = P.Passes[0].Params.AddDefaulted_GetRef();
		Binding.Target = TEXT("Radius");
		Binding.Parameter = TEXT("NoSuchParameter");
	});
	ExpectInvalid(TEXT("an EnabledParameter that is not a Bool"), [](UDreamPassPipeline& P) { P.Passes[0].EnabledParameter = TEXT("Gain"); });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassResolveTest,
	"DreamShader.Pass.Logic.Resolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPassResolveTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassLogicTests;

	FScopedTestWorld TestWorld;
	UDreamPassSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (!TestNotNull(TEXT("a game world has the subsystem"), Subsystem))
	{
		return false;
	}

	UDreamPassPipeline* Late = MakePipeline(TEXT("CP_LogicLate"), 10);
	UDreamPassPipeline* Early = MakePipeline(TEXT("CP_LogicEarly"), -5);

	// Two activations of one pipeline: priority decides whose values win, each blended in by its own weight.
	const TArray<FDreamPassParameterOverride> NoOverrides;
	const FDreamPassHandle Low = Subsystem->AddPipeline(Late, 0.0f, TArray<FDreamPassParameterOverride>{ MakeOverride(TEXT("Gain"), FDreamPassParameterValue::MakeFloat(3.0f)) });
	const FDreamPassHandle High = Subsystem->AddPipeline(Late, 1.0f, TArray<FDreamPassParameterOverride>{ MakeOverride(TEXT("Gain"), FDreamPassParameterValue::MakeFloat(5.0f)) });
	Subsystem->SetWeight(High, 0.5f);
	Subsystem->AddPipeline(Early, 0.0f, NoOverrides);

	TArray<FDreamPassResolvedPipeline> Resolved;
	Subsystem->ResolveView(MakeGameQuery(), Resolved);
	if (!TestEqual(TEXT("each pipeline once"), Resolved.Num(), 2))
	{
		return false;
	}
	TestTrue(TEXT("Order decides the execution order"), Resolved[0].Pipeline == Early && Resolved[1].Pipeline == Late);

	const FDreamPassParameterValue* Gain = Resolved[1].FindValue(TEXT("Gain"));
	TestTrue(TEXT("Gain: 1 -> 3 at full weight, then halfway to 5"), Gain && FMath::IsNearlyEqual(Gain->Vector.X, 4.0f));
	TestEqual(TEXT("the pipeline's weight is its largest"), Resolved[1].Weight, 1.0f);

	// A player-specific activation applies to that player's views only.
	Subsystem->RemovePipeline(Low);
	Subsystem->RemovePipeline(High);
	Subsystem->AddPipeline(Late, 0.0f, NoOverrides, /*PlayerIndex*/ 1);
	Subsystem->ResolveView(MakeGameQuery(), Resolved);
	TestTrue(TEXT("player 1's pipeline is not player 0's"), !Resolved.ContainsByPredicate([Late](const FDreamPassResolvedPipeline& R) { return R.Pipeline == Late; }));

	// What a view does not offer leaves a pipeline out: its kind, its requirements, a false EnabledParameter.
	Early->Views = int32(EDreamPassViewFlags::Game);
	FDreamPassViewQuery EditorQuery = MakeGameQuery();
	EditorQuery.ViewKind = EDreamPassViewFlags::Editor;
	Subsystem->ResolveView(EditorQuery, Resolved);
	TestTrue(TEXT("a Game-only pipeline does not run in an editor view"), Resolved.IsEmpty());

	Early->Requires = int32(EDreamPassRequirementFlags::CustomStencil);
	Subsystem->ResolveView(MakeGameQuery(), Resolved);
	TestTrue(TEXT("a requirement the view lacks leaves the pipeline out"), Resolved.IsEmpty());
	Early->Requires = 0;

	Early->EnabledParameter = TEXT("Flag");
	const FDreamPassHandle Disabling = Subsystem->AddPipeline(Early, 2.0f, TArray<FDreamPassParameterOverride>{ MakeOverride(TEXT("Flag"), FDreamPassParameterValue::MakeBool(false)) });
	Subsystem->ResolveView(MakeGameQuery(), Resolved);
	TestTrue(TEXT("an EnabledParameter resolving false leaves the pipeline out"), Resolved.IsEmpty());
	Subsystem->RemovePipeline(Disabling);

	// A registered source is gathered like the built-in ones.
	FFixedSource Source;
	Source.Pipeline = Late;
	Source.Overrides.Add(MakeOverride(TEXT("Gain"), FDreamPassParameterValue::MakeFloat(7.0f)));
	UObject* SourceKey = NewObject<UDreamPassPipeline>(GetTransientPackage(), NAME_None, RF_Transient);
	Subsystem->RegisterSource(SourceKey, &Source);
	Subsystem->ResolveView(MakeGameQuery(), Resolved);
	const FDreamPassResolvedPipeline* FromSource = Resolved.FindByPredicate([Late](const FDreamPassResolvedPipeline& R) { return R.Pipeline == Late; });
	TestTrue(TEXT("a source's activation applies"), FromSource && FromSource->FindValue(TEXT("Gain")) && FMath::IsNearlyEqual(FromSource->FindValue(TEXT("Gain"))->Vector.X, 7.0f));
	Subsystem->UnregisterSource(SourceKey);
	Subsystem->ResolveView(MakeGameQuery(), Resolved);
	TestTrue(TEXT("an unregistered source no longer applies"), !Resolved.ContainsByPredicate([Late](const FDreamPassResolvedPipeline& R) { return R.Pipeline == Late; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassLayerMaskTest,
	"DreamShader.Pass.Logic.LayerMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPassLayerMaskTest::RunTest(const FString& Parameters)
{
	UDreamPassSettings* Settings = GetMutableDefault<UDreamPassSettings>();
	const TArray<FName> SavedNames = Settings->LayerNames;
	ON_SCOPE_EXIT { Settings->LayerNames = SavedNames; };

	Settings->LayerNames.Reset();
	Settings->LayerNames.Add(TEXT("Highlight"));
	Settings->LayerNames.Add(TEXT("XRay"));
	Settings->LayerNames.Add(TEXT("Outline"));

	TArray<FName> Asked;
	Asked.Add(TEXT("Outline"));
	Asked.Add(TEXT("Highlight"));
	Asked.Add(TEXT("Missing"));
	TArray<FName> Unknown;
	const uint32 Mask = Settings->MakeLayerMask(Asked, &Unknown);
	TestEqual(TEXT("bit i is LayerNames[i]"), Mask, (1u << 2) | (1u << 0));
	TestEqual(TEXT("an unknown name is reported"), Unknown.Num(), 1);
	TestEqual(TEXT("none is no layer"), Settings->FindLayerIndex(NAME_None), int32(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPassSourcesTest,
	"DreamShader.Pass.Logic.Sources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPassSourcesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::PassLogicTests;

	FScopedTestWorld TestWorld;
	UDreamPassSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (!TestNotNull(TEXT("a game world has the subsystem"), Subsystem))
	{
		return false;
	}

	UDreamPassPipeline* Pipeline = MakePipeline(TEXT("CP_LogicSources"), 0);
	Pipeline->Parameters.Add(MakeParameter(TEXT("Tint"), FDreamPassParameterValue::MakeVector(EDreamPassParameterType::Float3, FVector4f(1.0f, 1.0f, 1.0f, 0.0f))));

	auto Resolve = [Subsystem, Pipeline](const FDreamPassViewQuery& Query) -> TOptional<FDreamPassResolvedPipeline>
	{
		TArray<FDreamPassResolvedPipeline> Resolved;
		Subsystem->ResolveView(Query, Resolved);
		const FDreamPassResolvedPipeline* Found = Resolved.FindByPredicate([Pipeline](const FDreamPassResolvedPipeline& R) { return R.Pipeline == Pipeline; });
		return Found ? TOptional<FDreamPassResolvedPipeline>(*Found) : TOptional<FDreamPassResolvedPipeline>();
	};

	// A component registers itself with its world's subsystem when it is registered, and leaves when it is not.
	AActor* Owner = TestWorld.World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("an actor spawns in the test world"), Owner))
	{
		return false;
	}
	UDreamPassComponent* Component = NewObject<UDreamPassComponent>(Owner);
	Component->Pipeline = Pipeline;
	Component->RegisterComponent();
	TestTrue(TEXT("a registered component's pipeline applies"), Resolve(MakeGameQuery()).IsSet());

	TestTrue(TEXT("SetColorParameter takes the float3 the pipeline declares"), Component->SetColorParameter(TEXT("Tint"), FLinearColor(0.0f, 0.5f, 1.0f, 1.0f)));
	TestFalse(TEXT("SetParameter refuses another type"), Component->SetParameter(TEXT("Gain"), FDreamPassParameterValue::MakeBool(true)));
	TestFalse(TEXT("SetParameter refuses an unknown name"), Component->SetParameter(TEXT("NoSuchParameter"), FDreamPassParameterValue::MakeFloat(1.0f)));
	{
		const TOptional<FDreamPassResolvedPipeline> Resolved = Resolve(MakeGameQuery());
		const FDreamPassParameterValue* Tint = Resolved.IsSet() ? Resolved->FindValue(TEXT("Tint")) : nullptr;
		TestTrue(TEXT("the override reaches the view"), Tint && Tint->Vector.Equals(FVector4f(0.0f, 0.5f, 1.0f, 0.0f)));
	}

	Component->SetWeight(0.0f);
	TestFalse(TEXT("weight 0 does not run"), Resolve(MakeGameQuery()).IsSet());
	Component->SetWeight(2.0f);
	TestEqual(TEXT("a weight is clamped to 1"), Component->Weight, 1.0f);

	Component->PlayerIndex = 1;
	TestFalse(TEXT("player 1's component does not run in player 0's view"), Resolve(MakeGameQuery()).IsSet());
	Component->PlayerIndex = INDEX_NONE;

	Component->Scope = EDreamPassComponentScope::ViewTarget;
	FDreamPassViewQuery Looking = MakeGameQuery();
	Looking.ViewActorUniqueId = Owner->GetUniqueID();
	TestTrue(TEXT("ViewTarget: the owner's view target runs it"), Resolve(Looking).IsSet());
	TestFalse(TEXT("ViewTarget: a view without a view target does not"), Resolve(MakeGameQuery()).IsSet());
	Component->Scope = EDreamPassComponentScope::World;

	Component->SetEnabled(false);
	TestFalse(TEXT("a disabled component does not run"), Resolve(MakeGameQuery()).IsSet());
	Component->SetEnabled(true);

	Component->DestroyComponent();
	TestFalse(TEXT("a destroyed component no longer applies"), Resolve(MakeGameQuery()).IsSet());

	// A volume spawned at runtime has no brush: it applies only when unbound, and then everywhere at its blend weight.
	ADreamPassVolume* Volume = TestWorld.World->SpawnActor<ADreamPassVolume>();
	if (!TestNotNull(TEXT("a volume spawns in the test world"), Volume))
	{
		return false;
	}
	Volume->Pipeline = Pipeline;
	TestFalse(TEXT("a bounded volume without a brush applies nowhere"), Resolve(MakeGameQuery()).IsSet());
	TestEqual(TEXT("its weight is 0 everywhere"), Volume->GetWeightAt(FVector::ZeroVector), 0.0f);

	Volume->bUnbound = true;
	Volume->SetBlendWeight(0.25f);
	{
		const TOptional<FDreamPassResolvedPipeline> Resolved = Resolve(MakeGameQuery());
		TestTrue(TEXT("an unbound volume applies anywhere at its blend weight"), Resolved.IsSet() && FMath::IsNearlyEqual(Resolved->Weight, 0.25f));
	}

	// A second source of the same pipeline: still one pipeline, at the larger weight.
	Subsystem->AddPipeline(Pipeline, 0.0f, TArray<FDreamPassParameterOverride>());
	{
		const TOptional<FDreamPassResolvedPipeline> Resolved = Resolve(MakeGameQuery());
		TestTrue(TEXT("the larger weight of two sources"), Resolved.IsSet() && FMath::IsNearlyEqual(Resolved->Weight, 1.0f));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
