// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderVersionCompat.h"

#if WITH_DEV_AUTOMATION_TESTS && DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)

#include "Decompiler/DreamShaderInstanceDecompiler.h"
#include "DreamShaderInstanceSchema.h"
#include "Engine/TextureCollection.h"
#include "Lang/LangInstanceSource.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureCollectionParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#if DREAMSHADER_WITH_MATERIAL_PARAMETERS_HEADER
#include "Materials/MaterialParameters.h"
#else
#include "MaterialTypes.h"
#endif
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderTextureCollectionDecompileTest,
	"DreamShader.Compiler2.Decompile.TextureCollection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderTextureCollectionDecompileTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	namespace IR = UE::DreamShader::IR;
	namespace Lang = UE::DreamShader::Lang;

	TStrongObjectPtr<UMaterial> Parent(NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient));
	TStrongObjectPtr<UTextureCollection> DefaultCollection(NewObject<UTextureCollection>(GetTransientPackage(), NAME_None, RF_Transient));
	TStrongObjectPtr<UTextureCollection> OverrideCollection(NewObject<UTextureCollection>(GetTransientPackage(), NAME_None, RF_Transient));
	TStrongObjectPtr<UMaterialInstanceConstant> Instance(NewObject<UMaterialInstanceConstant>(GetTransientPackage(), NAME_None, RF_Transient));
	UMaterialExpressionTextureCollectionParameter* Parameter = Cast<UMaterialExpressionTextureCollectionParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Parent.Get(), UMaterialExpressionTextureCollectionParameter::StaticClass()));
	if (!TestNotNull(TEXT("the texture collection parameter"), Parameter))
	{
		return false;
	}
	Parameter->ParameterName = TEXT("Collection");
	Parameter->TextureCollection = DefaultCollection.Get();
	Parent->UpdateCachedExpressionData();
	Instance->SetParentEditorOnly(Parent.Get(), /*RecacheShader*/ false);
	Instance->SetTextureCollectionParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Collection")), OverrideCollection.Get());

	// Verify the fixture through the engine before asking the plugin to read it.
	UTextureCollection* Effective = nullptr;
	if (!TestTrue(TEXT("the engine finds the collection override"), Instance->GetTextureCollectionParameterValue(FMaterialParameterInfo(TEXT("Collection")), Effective))
		|| !TestTrue(TEXT("the engine returns the non-null override"), Effective == OverrideCollection.Get()))
	{
		return false;
	}

	IR::FIRParameterSchema Schema;
	TestTrue(TEXT("the parent schema can be read"), UE::DreamShader::Editor::Compiler::BuildParameterSchemaFromAsset(Parent.Get(), Schema));
	const int32 SchemaIndex = Schema.Find(TEXT("Collection"));
	if (!TestTrue(TEXT("the parent schema contains the collection"), Schema.Parameters.IsValidIndex(SchemaIndex)))
	{
		return false;
	}
	TestEqual(TEXT("the schema preserves the collection default object"), Schema.Parameters[SchemaIndex].ParentValue.S, DefaultCollection->GetPathName());

	// Both normal export and Adopt read this parameter; neither may turn it into an explicit None.
	for (const EInstanceDecompileFilter Filter : { EInstanceDecompileFilter::DifferingFromParent, EInstanceDecompileFilter::OverriddenOnly })
	{
		FInstanceDecompileOptions Options;
		Options.Filter = Filter;
		IR::FIRInstance Decompiled;
		Lang::FLangDiagnosticSink Diagnostics(TEXT("MI_Collection.dsi"));
		if (!TestTrue(TEXT("the instance decompiles"), DecompileMaterialInstance(Instance.Get(), Options, Decompiled, Diagnostics)))
		{
			return false;
		}
		const IR::FIRInstanceOverride* Collection = Decompiled.Overrides.FindByPredicate([](const IR::FIRInstanceOverride& Override)
		{
			return Override.ParameterName == TEXT("Collection");
		});
		if (!TestNotNull(TEXT("the decompiler keeps the collection override"), Collection))
		{
			return false;
		}
		TestEqual(TEXT("the override preserves its object path"), Collection->Value.S, OverrideCollection->GetPathName());
		const FString Text = Lang::PrintDreamShaderInstance(Decompiled, TEXT("MI_Collection.dsi"), FString());
		TestTrue(FString::Printf(TEXT("the .dsi names the overridden collection\n%s"), *Text), Text.Contains(TEXT("@default ") + OverrideCollection->GetPathName()));
		TestFalse(TEXT("a non-null collection never prints as None"), Text.Contains(TEXT("@default None")));
	}

	struct FBoundaryCase
	{
		const TCHAR* Name;
		UMaterialInterface* Parent;
		UTextureCollection* Value;
		bool bSetOverride;
		bool bDiffersFromParent;
	};
	const FBoundaryCase Cases[] =
	{
		{ TEXT("explicit None"), Parent.Get(), nullptr, true, true },
		{ TEXT("pinned to the parent value"), Parent.Get(), DefaultCollection.Get(), true, false },
		{ TEXT("inherited through an instance"), Instance.Get(), OverrideCollection.Get(), false, false },
	};
	for (const FBoundaryCase& Case : Cases)
	{
		// Use a fresh instance: setting nullptr on an existing non-null override is ignored by some
		// engine versions, while a newly created override slot correctly represents an explicit None.
		TStrongObjectPtr<UMaterialInstanceConstant> Child(NewObject<UMaterialInstanceConstant>(GetTransientPackage(), NAME_None, RF_Transient));
		Child->SetParentEditorOnly(Case.Parent, /*RecacheShader*/ false);
		if (Case.bSetOverride)
		{
			Child->SetTextureCollectionParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Collection")), Case.Value);
		}
		const FString What(Case.Name);
		TMap<FMaterialParameterInfo, FMaterialParameterMetadata> EngineParameters;
		Child->GetAllParametersOfType(EMaterialParameterType::TextureCollection, EngineParameters);
		const FMaterialParameterMetadata* EngineValue = EngineParameters.Find(FMaterialParameterInfo(TEXT("Collection")));
		if (!TestNotNull(What + TEXT(": the engine exposes the parameter"), EngineValue))
		{
			return false;
		}
		TestTrue(What + TEXT(": the engine preserves the collection value"), EngineValue->Value.TextureCollection == Case.Value);
		TestEqual(What + TEXT(": the engine identifies local overrides"), EngineValue->bOverride, Case.bSetOverride);
		if (Case.Value)
		{
			Effective = nullptr;
			TestTrue(What + TEXT(": the engine getter resolves the collection"),
				Child->GetTextureCollectionParameterValue(FMaterialParameterInfo(TEXT("Collection")), Effective) && Effective == Case.Value);
		}

		const FString ExpectedPath = Case.Value ? Case.Value->GetPathName() : FString();
		IR::FIRParameterSchema ChildSchema;
		TestTrue(What + TEXT(": the instance schema can be read"), UE::DreamShader::Editor::Compiler::BuildParameterSchemaFromAsset(Child.Get(), ChildSchema));
		const int32 ChildSchemaIndex = ChildSchema.Find(TEXT("Collection"));
		if (!TestTrue(What + TEXT(": the instance schema contains the collection"), ChildSchema.Parameters.IsValidIndex(ChildSchemaIndex)))
		{
			return false;
		}
		TestEqual(What + TEXT(": the schema preserves the collection or None"), ChildSchema.Parameters[ChildSchemaIndex].ParentValue.S, ExpectedPath);

		for (const EInstanceDecompileFilter Filter : { EInstanceDecompileFilter::DifferingFromParent, EInstanceDecompileFilter::OverriddenOnly })
		{
			const bool bExpectOverride = Filter == EInstanceDecompileFilter::OverriddenOnly ? Case.bSetOverride : Case.bDiffersFromParent;
			const FString FilterWhat = What + (Filter == EInstanceDecompileFilter::OverriddenOnly ? TEXT(" (Adopt)") : TEXT(" (export)"));
			FInstanceDecompileOptions Options;
			Options.Filter = Filter;
			IR::FIRInstance Decompiled;
			Lang::FLangDiagnosticSink Diagnostics(TEXT("MI_CollectionBoundary.dsi"));
			if (!TestTrue(FilterWhat + TEXT(": decompiles"), DecompileMaterialInstance(Child.Get(), Options, Decompiled, Diagnostics)))
			{
				return false;
			}
			TestEqual(FilterWhat + TEXT(": keeps exactly the required overrides"), Decompiled.Overrides.Num(), bExpectOverride ? 1 : 0);
			if (bExpectOverride && Decompiled.Overrides.Num() == 1)
			{
				TestEqual(FilterWhat + TEXT(": preserves the override value"), Decompiled.Overrides[0].Value.S, ExpectedPath);
				const FString Text = Lang::PrintDreamShaderInstance(Decompiled, TEXT("MI_CollectionBoundary.dsi"), FString());
				TestTrue(FilterWhat + TEXT(": prints the collection or explicit None"),
					Text.Contains(TEXT("@default ") + (ExpectedPath.IsEmpty() ? FString(TEXT("None")) : ExpectedPath)));
			}
		}
	}
	return true;
}

#endif
