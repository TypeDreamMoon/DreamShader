#include "Decompiler/DreamShaderGraphDecompiler.h"
#include "DreamShaderGraphDecompilerImpl.h"
#include "DreamShaderGraphDecompilerHelpers.h"

#include "Decompiler/DreamShaderDecompileService.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderMaterialExpressionCompat.h"
#include "DreamShaderGeneratedAssets.h"
#include "VirtualFunction/DreamShaderVirtualFunctionService.h"

#include "CoreGlobals.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveLinearColorAtlas.h"
#include "Engine/Texture.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCeil.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionCurveAtlasRowParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionFloor.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSquareRoot.h"
#include "Materials/MaterialExpressionStaticComponentMaskParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
#include "Materials/MaterialExpressionSubstrate.h"
#endif
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVertexTangentWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "MaterialShared.h"
#include "MaterialValueType.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/EnumProperty.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{

		class FBridgeGraphDecompiler final : public UE::DreamShader::Editor::IDreamShaderDecompiler
		{
		public:
			virtual bool DecompileMaterial(UMaterial* Material, const FString& DecompiledName, FString& OutSourceText, FString& OutError) override
			{
				FDreamShaderGraphDecompiler Decompiler;
				return Decompiler.DecompileMaterial(Material, DecompiledName, OutSourceText, OutError);
			}

			virtual bool DecompileFunction(
				UMaterialFunction* MaterialFunction,
				const FString& DecompiledName,
				EDreamShaderDecompiledFunctionKind FunctionKind,
				FString& OutSourceText,
				FString& OutError) override
			{
				FDreamShaderGraphDecompiler Decompiler;
				return Decompiler.DecompileFunction(MaterialFunction, DecompiledName, FunctionKind, OutSourceText, OutError);
			}
		};

	}

	bool BuildGraphDecompilerVirtualFunctionDefinition(const UMaterialFunction* MaterialFunction, FString& OutDefinition, FString& OutError)
	{
		return BuildVirtualFunctionDefinition(MaterialFunction, OutDefinition, OutError);
	}

	UE::DreamShader::Editor::IDreamShaderDecompiler& GetGraphDecompiler()
	{
		static FBridgeGraphDecompiler Decompiler;
		return Decompiler;
	}
}
