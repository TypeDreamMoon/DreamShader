#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderVersionCompat.h"

#include "Misc/Crc.h"

#include "EdGraph/EdGraphNode.h"
#include "Interfaces/IPluginManager.h"
#include "MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/Texture.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCube.h"
#include "Engine/VolumeTexture.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "ObjectTools.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace UE::DreamShader::Editor::Private
{

	bool TryGetUEBuiltinArgument(const FTextShaderPropertyDefinition& Property, const TCHAR* Key, FString& OutValue)
	{
		if (const FString* Value = Property.UEBuiltinArguments.Find(UE::DreamShader::NormalizeSettingKey(Key)))
		{
			OutValue = *Value;
			return true;
		}

		return false;
	}

	bool ValidateUEBuiltinArgumentNames(
		const FTextShaderPropertyDefinition& Property,
		TConstArrayView<const TCHAR*> AllowedArgumentNames,
		FString& OutError)
	{
		for (const TPair<FString, FString>& Argument : Property.UEBuiltinArguments)
		{
			bool bKnownArgument = false;
			for (const TCHAR* AllowedName : AllowedArgumentNames)
			{
				if (Argument.Key == UE::DreamShader::NormalizeSettingKey(AllowedName))
				{
					bKnownArgument = true;
					break;
				}
			}

			if (!bKnownArgument)
			{
				OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("UE.%s for property '%s' does not support argument '%s'."),
					*Property.UEBuiltinFunctionName,
					*Property.Name,
					*Argument.Key);
				return false;
			}
		}

		return true;
	}

	bool TryResolvePositionOrigin(const FString& InValue, EPositionOrigin& OutValue)
	{
		FString Value = InValue;
		Value.TrimStartAndEndInline();
		Value.ToLowerInline();
		Value.ReplaceInline(TEXT(" "), TEXT(""));

		if (Value == TEXT("absolute") || Value == TEXT("world"))
		{
			OutValue = EPositionOrigin::Absolute;
			return true;
		}

		if (Value == TEXT("camerarelative"))
		{
			OutValue = EPositionOrigin::CameraRelative;
			return true;
		}

		return false;
	}

	bool TryResolvePropertyReference(
		const FString& InReferenceName,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		UMaterialExpression*& OutExpression)
	{
		const FString ReferenceName = InReferenceName.TrimStartAndEnd();
		if (ReferenceName.IsEmpty())
		{
			return false;
		}

		if (UMaterialExpression* const* ExactMatch = AvailableExpressions.Find(ReferenceName))
		{
			OutExpression = *ExactMatch;
			return true;
		}

		for (const TPair<FString, UMaterialExpression*>& Pair : AvailableExpressions)
		{
			if (Pair.Key.Equals(ReferenceName, ESearchCase::IgnoreCase))
			{
				OutExpression = Pair.Value;
				return true;
			}
		}

		return false;
	}

	UMaterialExpression* CreateOwnedMaterialExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		UClass* ExpressionClass,
		const int32 PositionX,
		const int32 PositionY)
	{
		return UMaterialEditingLibrary::CreateMaterialExpressionEx(Material, MaterialFunction, ExpressionClass, nullptr, PositionX, PositionY, false);
	}

	void EnsureExpressionCanBeDeleted(UMaterialExpression* Expression)
	{
		if (Expression && Expression->IsRooted())
		{
			Expression->RemoveFromRoot();
		}
	}

	static TConstArrayView<TObjectPtr<UMaterialExpressionComment>> GetMaterialEditorComments(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction)
	{
		if (Material)
		{
			return Material->GetEditorComments();
		}

		if (MaterialFunction)
		{
			return MaterialFunction->GetEditorComments();
		}

		return TConstArrayView<TObjectPtr<UMaterialExpressionComment>>();
	}

	void ClearDreamShaderGeneratedComments(UMaterial* Material, UMaterialFunction* MaterialFunction)
	{
		if (!Material && !MaterialFunction)
		{
			return;
		}

		TArray<UMaterialExpressionComment*> CommentsToRemove;
		for (const TObjectPtr<UMaterialExpressionComment>& Comment : GetMaterialEditorComments(Material, MaterialFunction))
		{
			if (Comment && Comment->Text.StartsWith(TEXT("DreamShader: "), ESearchCase::CaseSensitive))
			{
				CommentsToRemove.Add(Comment.Get());
			}
		}

		for (UMaterialExpressionComment* Comment : CommentsToRemove)
		{
			if (Material)
			{
				Material->GetExpressionCollection().RemoveComment(Comment);
			}
			else if (MaterialFunction)
			{
				MaterialFunction->GetExpressionCollection().RemoveComment(Comment);
			}
			EnsureExpressionCanBeDeleted(Comment);
			Comment->MarkAsGarbage();
		}
	}

	UMaterialExpression* CreateScalarLiteralExpressionEx(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const double Value,
		const int32 PositionY)
	{
		auto* Expression = Cast<UMaterialExpressionConstant>(
			CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionConstant::StaticClass(), -1120, PositionY));
		if (Expression)
		{
			Expression->R = static_cast<float>(Value);
		}
		return Expression;
	}

	UMaterialExpression* CreateScalarLiteralExpression(UMaterial* Material, const double Value, const int32 PositionY)
	{
		return CreateScalarLiteralExpressionEx(Material, nullptr, Value, PositionY);
	}

	UMaterialExpression* CreateVectorLiteralExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const TArray<double>& Components,
		const int32 ExpectedComponentCount,
		const int32 PositionY)
	{
		if (ExpectedComponentCount == 2)
		{
			auto* Expression = Cast<UMaterialExpressionConstant2Vector>(
				CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionConstant2Vector::StaticClass(), -1120, PositionY));
			if (Expression)
			{
				Expression->R = static_cast<float>(Components[0]);
				Expression->G = static_cast<float>(Components[1]);
			}
			return Expression;
		}

		if (ExpectedComponentCount == 3)
		{
			auto* Expression = Cast<UMaterialExpressionConstant3Vector>(
				CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionConstant3Vector::StaticClass(), -1120, PositionY));
			if (Expression)
			{
				Expression->Constant = FLinearColor(
					static_cast<float>(Components[0]),
					static_cast<float>(Components[1]),
					static_cast<float>(Components[2]),
					1.0f);
			}
			return Expression;
		}

		auto* Expression = Cast<UMaterialExpressionConstant4Vector>(
			CreateOwnedMaterialExpression(Material, MaterialFunction, UMaterialExpressionConstant4Vector::StaticClass(), -1120, PositionY));
		if (Expression)
		{
			Expression->Constant = FLinearColor(
				static_cast<float>(Components[0]),
				static_cast<float>(Components[1]),
				static_cast<float>(Components[2]),
				static_cast<float>(Components[3]));
		}
		return Expression;
	}

	static UMaterialExpression* CreateLiteralExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FString& InValueText,
		const int32 ExpectedComponentCount,
		const int32 PositionY,
		FString& OutError)
	{
		if (ExpectedComponentCount == 1)
		{
			double ParsedValue = 0.0;
			if (!ParseScalarLiteral(InValueText, ParsedValue))
			{
				OutError = FString::Printf(TEXT("Expected a scalar literal but got '%s'."), *InValueText); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return nullptr;
			}

			UMaterialExpression* Expression = CreateScalarLiteralExpressionEx(Material, MaterialFunction, ParsedValue, PositionY);
			if (!Expression)
			{
				OutError = TEXT("Failed to create a scalar constant expression.");
			}
			return Expression;
		}

		TArray<double> Components;
		if (!ParseVectorLiteral(InValueText, Components))
		{
			OutError = FString::Printf(TEXT("Expected a float%d-style literal like '(...)' but got '%s'."), ExpectedComponentCount, *InValueText); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return nullptr;
		}

		if (Components.Num() != ExpectedComponentCount)
		{
			OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("Expected %d components but got %d in literal '%s'."),
				ExpectedComponentCount,
				Components.Num(),
				*InValueText);
			return nullptr;
		}

		UMaterialExpression* Expression = CreateVectorLiteralExpression(Material, MaterialFunction, Components, ExpectedComponentCount, PositionY);
		if (!Expression)
		{
			OutError = FString::Printf(TEXT("Failed to create a float%d constant expression."), ExpectedComponentCount); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}
		return Expression;
	}

	bool ResolveExpressionInputValue(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FString& InValueText,
		const TMap<FString, UMaterialExpression*>& AvailableExpressions,
		const int32 ExpectedComponentCount,
		const int32 PositionY,
		UMaterialExpression*& OutExpression,
		FString& OutError)
	{
		if (TryResolvePropertyReference(InValueText, AvailableExpressions, OutExpression))
		{
			return true;
		}

		OutExpression = CreateLiteralExpression(Material, MaterialFunction, InValueText, ExpectedComponentCount, PositionY, OutError);
		if (OutExpression)
		{
			return true;
		}

		OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
			TEXT("%s It must reference a previously declared property or use a compatible literal."),
			*OutError);
		return false;
	}

	FCodeValue CreateOutputRerouteValue(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FCodeValue& SourceValue,
		const FString& RouteName,
		const int32 RouteIndex)
	{
		FCodeValue RoutedValue = SourceValue;
		if (!SourceValue.Expression || (!Material && !MaterialFunction))
		{
			return RoutedValue;
		}

		const FString BaseName = RouteName.TrimStartAndEnd().IsEmpty()
			? TEXT("Output")
			: RouteName.TrimStartAndEnd();
		FString SanitizedName = UE::DreamShader::SanitizeIdentifier(BaseName);
		if (RouteIndex >= 0)
		{
			SanitizedName += FString::Printf(TEXT("_%d"), RouteIndex); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}
		SanitizedName = FString::Printf(TEXT("DS_%s"), *SanitizedName); /* I18N-EXEMPT: deferred codegen or compatibility path */

		const int32 SourceX = SourceValue.Expression->MaterialExpressionEditorX;
		const int32 SourceY = SourceValue.Expression->MaterialExpressionEditorY;
		const int32 DeclarationX = SourceX + 420;
		const int32 DeclarationY = SourceY;
		const int32 UsageX = 720;
		const int32 UsageY = -120 + FMath::Max(RouteIndex, 0) * 180;

		auto* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(
			CreateOwnedMaterialExpression(
				Material,
				MaterialFunction,
				UMaterialExpressionNamedRerouteDeclaration::StaticClass(),
				DeclarationX,
				DeclarationY));
		auto* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(
			CreateOwnedMaterialExpression(
				Material,
				MaterialFunction,
				UMaterialExpressionNamedRerouteUsage::StaticClass(),
				UsageX,
				UsageY));
		if (!Declaration || !Usage)
		{
			return RoutedValue;
		}

		Declaration->Name = FName(*SanitizedName);
		if (!Declaration->VariableGuid.IsValid())
		{
			Declaration->VariableGuid = FGuid::NewGuid();
		}
		ConnectCodeValueToInput(Declaration->Input, SourceValue);

		Usage->Declaration = Declaration;
		Usage->DeclarationGuid = Declaration->VariableGuid;

		RoutedValue.Expression = Usage;
		RoutedValue.OutputIndex = 0;
		ClearCodeValueInputMask(RoutedValue);
		return RoutedValue;
	}

	void ResetMaterialToDefaults(UMaterial* Material)
	{
		check(Material);

		Material->BlendMode = BLEND_Opaque;
		Material->MaterialDomain = MD_Surface;
		Material->SetShadingModel(MSM_DefaultLit);
		Material->TwoSided = false;
		Material->OpacityMaskClipValue = 0.3333f;
		Material->Wireframe = false;
		Material->DitheredLODTransition = false;
		Material->DitherOpacityMask = false;
		Material->bAllowNegativeEmissiveColor = false;
		Material->bCastDynamicShadowAsMasked = false;
		Material->bCastRayTracedShadows = true;
		Material->bEnableResponsiveAA = false;
		Material->bScreenSpaceReflections = false;
		Material->bContactShadows = false;
		Material->bDisableDepthTest = false;
		Material->bOutputTranslucentVelocity = false;
		Material->bWriteOnlyAlpha = false;
		Material->BlendableOutputAlpha = false;
		Material->TranslucencyLightingMode = TLM_VolumetricNonDirectional;
		Material->bTangentSpaceNormal = true;
		Material->bAlwaysEvaluateWorldPositionOffset = false;
		Material->bFullyRough = false;
		Material->bIsSky = false;
		Material->bIsThinSurface = false;
		Material->MaterialDecalResponse = MDR_ColorNormalRoughness;
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
		Material->bHasPixelAnimation = false;
#endif
		Material->NumCustomizedUVs = 0;
	}

}
