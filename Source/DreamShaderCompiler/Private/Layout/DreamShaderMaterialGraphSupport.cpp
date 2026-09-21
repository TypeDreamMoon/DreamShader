#include "DreamShaderMaterialExpressionCompat.h"

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

	FString MakeDreamShaderOutputRerouteName(const FString& RouteName, const int32 RouteIndex)
	{
		const FString TrimmedName = RouteName.TrimStartAndEnd();
		FString SanitizedName = UE::DreamShader::SanitizeIdentifier(TrimmedName.IsEmpty() ? FString(TEXT("Output")) : TrimmedName);
		if (RouteIndex >= 0)
		{
			SanitizedName += FString::Printf(TEXT("_%d"), RouteIndex); /* I18N-EXEMPT: generated node name, not display text */
		}
		return FString::Printf(TEXT("DS_%s"), *SanitizedName); /* I18N-EXEMPT: generated node name, not display text */
	}

	bool CreateOutputRerouteValue(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		UMaterialExpression* SourceExpression,
		const int32 SourceOutputIndex,
		const FString& RouteName,
		const int32 RouteIndex,
		UMaterialExpression*& OutExpression,
		int32& OutOutputIndex)
	{
		// The failure answer first: the caller then wires the source directly.
		OutExpression = SourceExpression;
		OutOutputIndex = SourceOutputIndex;
		if (!SourceExpression || (!Material && !MaterialFunction))
		{
			return false;
		}

		const int32 SourceX = SourceExpression->MaterialExpressionEditorX;
		const int32 SourceY = SourceExpression->MaterialExpressionEditorY;
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
			return false;
		}

		Declaration->Name = FName(*MakeDreamShaderOutputRerouteName(RouteName, RouteIndex));
		if (!Declaration->VariableGuid.IsValid())
		{
			Declaration->VariableGuid = FGuid::NewGuid();
		}

		// No inline mask, exactly as the emitter connects every other pin (FIREmitter::ConnectValueToInput):
		// the value arriving here is already the output it names.
		Declaration->Input.Connect(SourceOutputIndex, SourceExpression);
		Declaration->Input.Mask = 0;
		Declaration->Input.MaskR = 0;
		Declaration->Input.MaskG = 0;
		Declaration->Input.MaskB = 0;
		Declaration->Input.MaskA = 0;

		Usage->Declaration = Declaration;
		Usage->DeclarationGuid = Declaration->VariableGuid;

		OutExpression = Usage;
		OutOutputIndex = 0;
		return true;
	}

	void ResetMaterialToDefaults(UMaterial* Material)
	{
		check(Material);

		Material->BlendMode = BLEND_Opaque;
		Material->MaterialDomain = MD_Surface;
		Material->SetShadingModel(MSM_DefaultLit);
		Material->TwoSided = false;
		// Set by the Base.MaterialAttributes binding, and only ever set -- so a rebuild whose source
		// no longer carries that binding kept the previous generation's true. That is exactly the
		// Substrate `#if` shape (Base.FrontMaterial plus individual root pins): with the flag still
		// on, the engine compiled every root pin from the now-unconnected MaterialAttributes input
		// and the individual bindings were ignored -- opacity mask, ambient occlusion and the
		// ray-tracing custom data all read their defaults.
		Material->bUseMaterialAttributes = false;
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
