// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderVersionCompat.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Decompiler/DreamShaderInstanceDecompiler.h"
#include "DreamShaderBuiltinCatalog.h"
#include "DreamShaderInstanceSchema.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCube.h"
#include "Engine/VolumeTexture.h"
#include "Lang/LangParser.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureSampleParameter2DArray.h"
#include "Materials/MaterialExpressionTextureSampleParameterCube.h"
#include "Materials/MaterialExpressionTextureSampleParameterVolume.h"
#include "Materials/MaterialInstanceConstant.h"
#if DREAMSHADER_WITH_MATERIAL_PARAMETERS_HEADER
#include "Materials/MaterialParameters.h"
#else
#include "MaterialTypes.h"
#endif
#include "Misc/AutomationTest.h"
#include "Semantic/LangBound.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderInstanceTextureRoundTripTest,
	"DreamShader.Compiler2.Decompile.TextureDimensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderInstanceTextureRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	namespace Compiler = UE::DreamShader::Editor::Compiler;
	namespace IR = UE::DreamShader::IR;
	namespace Lang = UE::DreamShader::Lang;

	struct FTextureCase
	{
		const TCHAR* TypeName;
		Lang::ETextureKind Kind;
		UClass* TextureClass;
		UClass* ParameterClass;
	};
	const FTextureCase Cases[] =
	{
		{ TEXT("Texture2D"), Lang::ETextureKind::Texture2D, UTexture2D::StaticClass(), UMaterialExpressionTextureSampleParameter2D::StaticClass() },
		{ TEXT("TextureCube"), Lang::ETextureKind::TextureCube, UTextureCube::StaticClass(), UMaterialExpressionTextureSampleParameterCube::StaticClass() },
		{ TEXT("Texture3D"), Lang::ETextureKind::Texture3D, UVolumeTexture::StaticClass(), UMaterialExpressionTextureSampleParameterVolume::StaticClass() },
		{ TEXT("Texture2DArray"), Lang::ETextureKind::Texture2DArray, UTexture2DArray::StaticClass(), UMaterialExpressionTextureSampleParameter2DArray::StaticClass() },
	};
	for (const FTextureCase& Case : Cases)
	{
		const FString What(Case.TypeName);
		TStrongObjectPtr<UMaterial> Parent(NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient));
		TStrongObjectPtr<UTexture> DefaultTexture(NewObject<UTexture>(GetTransientPackage(), Case.TextureClass, NAME_None, RF_Transient));
		TStrongObjectPtr<UTexture> OverrideTexture(NewObject<UTexture>(GetTransientPackage(), Case.TextureClass, NAME_None, RF_Transient));
		TStrongObjectPtr<UMaterialInstanceConstant> Instance(NewObject<UMaterialInstanceConstant>(GetTransientPackage(), NAME_None, RF_Transient));
		UMaterialExpressionTextureSampleParameter* Parameter = Cast<UMaterialExpressionTextureSampleParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Parent.Get(), Case.ParameterClass));
		if (!TestNotNull(What + TEXT(": the parameter exists"), Parameter))
		{
			return false;
		}
		Parameter->ParameterName = TEXT("Source");
		Parameter->Texture = DefaultTexture.Get();
		Parent->UpdateCachedExpressionData();
		Instance->SetParentEditorOnly(Parent.Get(), /*RecacheShader*/ false);
		Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Source")), OverrideTexture.Get());
		UTexture* Effective = nullptr;
		if (!TestTrue(What + TEXT(": the engine returns the overridden texture"),
			Instance->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Source")), Effective) && Effective == OverrideTexture.Get()))
		{
			return false;
		}

		IR::FIRParameterSchema Schema;
		TestTrue(What + TEXT(": the actual parent schema can be read"), Compiler::BuildParameterSchemaFromAsset(Parent.Get(), Schema));
		const int32 SchemaIndex = Schema.Find(TEXT("Source"));
		if (!TestTrue(What + TEXT(": the schema contains Source"), Schema.Parameters.IsValidIndex(SchemaIndex)))
		{
			return false;
		}
		TestTrue(What + TEXT(": the schema records the texture dimension"), Schema.Parameters[SchemaIndex].TextureKind == Case.Kind);

		for (const EInstanceDecompileFilter Filter : { EInstanceDecompileFilter::DifferingFromParent, EInstanceDecompileFilter::OverriddenOnly })
		{
			const FString FilterWhat = What + (Filter == EInstanceDecompileFilter::OverriddenOnly ? TEXT(" (Adopt)") : TEXT(" (export)"));
			FInstanceDecompileOptions Options;
			Options.Filter = Filter;
			FString Text;
			Lang::FLangDiagnosticSink Diagnostics(TEXT("MI_TextureDimensions.dsi"));
			if (!TestTrue(FilterWhat + TEXT(": decompiles"), DecompileMaterialInstanceToText(Instance.Get(), Options, Text, Diagnostics)))
			{
				return false;
			}
			TestTrue(FilterWhat + TEXT(": preserves the texture object"), Text.Contains(TEXT("@default ") + OverrideTexture->GetPathName()));
			TestTrue(FString::Printf(TEXT("%s: preserves the declared texture dimension\n%s"), *FilterWhat, *Text),
				Text.Contains(FString::Printf(TEXT("uniform %s Source;"), Case.TypeName)));
			const Lang::FLangParseResult Parsed = Lang::ParseDreamShaderLang(Lang::FLangSourceText(TEXT("MI_TextureDimensions.dsi"), Text));
			if (!TestTrue(FilterWhat + TEXT(": the exported source parses"), Parsed.Succeeded()))
			{
				return false;
			}
			Lang::FBindOptions BindOptions;
			BindOptions.Catalog = &Compiler::GetDreamShaderBuiltinCatalog();
			BindOptions.ParentSchema = &Schema;
			BindOptions.ParentObjectPath = Parent->GetPathName();
			const Lang::FLangBindResult Bound = Lang::BindDreamShaderLang(*Parsed.Module, BindOptions);
			FString Errors;
			for (const Lang::FLangDiagnostic& Diagnostic : Bound.Diagnostics.GetDiagnostics())
			{
				if (Diagnostic.Severity == Lang::ELangSeverity::Error)
				{
					Errors += Lang::FLangDiagnosticSink::ToWireString(Diagnostic) + TEXT("\n");
				}
			}
			TestTrue(FString::Printf(TEXT("%s: exported source binds against its real parent\n%s"), *FilterWhat, *Errors), Bound.Succeeded());
		}
	}
	return true;
}

#endif
