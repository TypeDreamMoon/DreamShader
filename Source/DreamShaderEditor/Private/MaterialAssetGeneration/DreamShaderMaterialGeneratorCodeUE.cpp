#include "DreamShaderMaterialGeneratorCodeShared.h"

#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
#include "Materials/MaterialExpressionSubstrate.h"
#endif

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		struct FSubstrateBuiltinDescriptor
		{
			const TCHAR* Name = TEXT("");
			const TCHAR* ClassName = TEXT("");
			bool bIsSubstrateOutput = true;
		};

#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
		static const FSubstrateBuiltinDescriptor* FindSubstrateBuiltinDescriptor(const FString& FunctionName)
		{
			static const FSubstrateBuiltinDescriptor Builtins[] =
			{
				{ TEXT("ShadingModels"), TEXT("MaterialExpressionSubstrateShadingModels"), true },
				{ TEXT("Slab"), TEXT("MaterialExpressionSubstrateSlabBSDF"), true },
				{ TEXT("SimpleClearCoat"), TEXT("MaterialExpressionSubstrateSimpleClearCoatBSDF"), true },
				{ TEXT("VolumetricFogCloud"), TEXT("MaterialExpressionSubstrateVolumetricFogCloudBSDF"), true },
				{ TEXT("Unlit"), TEXT("MaterialExpressionSubstrateUnlitBSDF"), true },
				{ TEXT("Hair"), TEXT("MaterialExpressionSubstrateHairBSDF"), true },
				{ TEXT("Eye"), TEXT("MaterialExpressionSubstrateEyeBSDF"), true },
				{ TEXT("SingleLayerWater"), TEXT("MaterialExpressionSubstrateSingleLayerWaterBSDF"), true },
				{ TEXT("LightFunction"), TEXT("MaterialExpressionSubstrateLightFunction"), true },
				{ TEXT("PostProcess"), TEXT("MaterialExpressionSubstratePostProcess"), true },
				{ TEXT("UI"), TEXT("MaterialExpressionSubstrateUI"), true },
				{ TEXT("ConvertMaterialAttributes"), TEXT("MaterialExpressionSubstrateConvertMaterialAttributes"), true },
				{ TEXT("ConvertToDecal"), TEXT("MaterialExpressionSubstrateConvertToDecal"), true },
				{ TEXT("HorizontalMix"), TEXT("MaterialExpressionSubstrateHorizontalMixing"), true },
				{ TEXT("HorizontalMixing"), TEXT("MaterialExpressionSubstrateHorizontalMixing"), true },
				{ TEXT("VerticalLayer"), TEXT("MaterialExpressionSubstrateVerticalLayering"), true },
				{ TEXT("VerticalLayering"), TEXT("MaterialExpressionSubstrateVerticalLayering"), true },
				{ TEXT("Add"), TEXT("MaterialExpressionSubstrateAdd"), true },
				{ TEXT("Weight"), TEXT("MaterialExpressionSubstrateWeight"), true },
				{ TEXT("Select"), TEXT("MaterialExpressionSubstrateSelect"), true },
				{ TEXT("TransmittanceToMFP"), TEXT("MaterialExpressionSubstrateTransmittanceToMFP"), false },
				{ TEXT("MetalnessToDiffuseAlbedoF0"), TEXT("MaterialExpressionSubstrateMetalnessToDiffuseAlbedoF0"), false },
				{ TEXT("HazinessToSecondaryRoughness"), TEXT("MaterialExpressionSubstrateHazinessToSecondaryRoughness"), false },
				{ TEXT("ThinFilm"), TEXT("MaterialExpressionSubstrateThinFilm"), false },
			};

			for (const FSubstrateBuiltinDescriptor& Builtin : Builtins)
			{
				if (FunctionName.Equals(Builtin.Name, ESearchCase::IgnoreCase))
				{
					return &Builtin;
				}
			}
			return nullptr;
		}

		static bool IsSubstrateExpressionClass(const UClass* ExpressionClass)
		{
			return ExpressionClass
				&& (ExpressionClass->IsChildOf(UMaterialExpressionSubstrateBSDF::StaticClass())
					|| ExpressionClass->IsChildOf(UMaterialExpressionSubstrateUtilityBase::StaticClass()));
		}
#else
		static bool IsSubstrateExpressionClass(const UClass* ExpressionClass)
		{
			(void)ExpressionClass;
			return false;
		}
#endif
	}

	bool FCodeGraphBuilder::EvaluateUEBuiltinCall(
		const FString& CalleeName,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FString& OutError)
	{
		const bool bIsSubstrateBuiltinCall = CalleeName.StartsWith(TEXT("Substrate."), ESearchCase::IgnoreCase);
		const FString FunctionName = bIsSubstrateBuiltinCall ? CalleeName.Mid(10) : CalleeName.Mid(3);
		const TCHAR* CallNamespace = bIsSubstrateBuiltinCall ? TEXT("Substrate") : TEXT("UE");

		auto HandleIntegerLiteralArgument = [&](const TCHAR* ArgumentName, int32& TargetField) -> bool
		{
			const FCodeCallArgument* Argument = FindNamedArgument(Arguments, ArgumentName);
			if (!Argument)
			{
				return true;
			}

			if (!TryExtractIntegerLiteral(Argument->Expression, TargetField))
			{
				OutError = FString::Printf(TEXT("UE.%s %s must be an integer literal."), *FunctionName, ArgumentName);
				return false;
			}

			return true;
		};

		auto HandleScalarLiteralArgument = [&](const TCHAR* ArgumentName, float& TargetField) -> bool
		{
			const FCodeCallArgument* Argument = FindNamedArgument(Arguments, ArgumentName);
			if (!Argument)
			{
				return true;
			}

			double ParsedValue = 0.0;
			if (!TryExtractScalarLiteral(Argument->Expression, ParsedValue))
			{
				OutError = FString::Printf(TEXT("UE.%s %s must be a numeric literal."), *FunctionName, ArgumentName);
				return false;
			}

			TargetField = static_cast<float>(ParsedValue);
			return true;
		};

		auto HandleBooleanLiteralArgument = [&](const TCHAR* ArgumentName, TFunctionRef<void(bool)> ApplyValue) -> bool
		{
			const FCodeCallArgument* Argument = FindNamedArgument(Arguments, ArgumentName);
			if (!Argument)
			{
				return true;
			}

			bool bParsedValue = false;
			if (!TryExtractBooleanLiteral(Argument->Expression, bParsedValue))
			{
				OutError = FString::Printf(TEXT("UE.%s %s must be a boolean literal."), *FunctionName, ArgumentName);
				return false;
			}

			ApplyValue(bParsedValue);
			return true;
		};

		auto HandleOptionalTextLiteralArgument = [&](const TCHAR* ArgumentName, FString& TargetValue) -> bool
		{
			const FCodeCallArgument* Argument = FindNamedArgument(Arguments, ArgumentName);
			if (!Argument)
			{
				return true;
			}

			if (!TryExtractTextLiteral(Argument->Expression, TargetValue))
			{
				OutError = FString::Printf(TEXT("UE.%s %s must be a text value."), *FunctionName, ArgumentName);
				return false;
			}

			return true;
		};

		auto HandleOptionalInputArgument = [&](const TCHAR* ArgumentName, FExpressionInput& TargetInput) -> bool
		{
			const FCodeCallArgument* Argument = FindNamedArgument(Arguments, ArgumentName);
			if (!Argument)
			{
				return true;
			}

			FCodeValue InputValue;
			if (!EvaluateExpression(Argument->Expression, InputValue, OutError))
			{
				return false;
			}

			ConnectCodeValueToInput(TargetInput, InputValue);
			return true;
		};

		auto HandleRequiredInputArgument = [&](const TCHAR* ArgumentName, const int32 PositionalIndex, FExpressionInput& TargetInput) -> bool
		{
			const FCodeCallArgument* Argument = FindNamedArgument(Arguments, ArgumentName);
			if (!Argument)
			{
				Argument = FindPositionalArgument(Arguments, PositionalIndex);
			}
			if (!Argument)
			{
				OutError = FString::Printf(TEXT("UE.%s requires parameter: %s"), *FunctionName, ArgumentName);
				return false;
			}

			FCodeValue InputValue;
			if (!EvaluateExpression(Argument->Expression, InputValue, OutError))
			{
				return false;
			}

			ConnectCodeValueToInput(TargetInput, InputValue);
			return true;
		};

		if (!bIsSubstrateBuiltinCall && FunctionName.Equals(TEXT("StaticSwitchParameter"), ESearchCase::IgnoreCase))
		{
			const FCodeCallArgument* NameArgument = FindNamedArgument(Arguments, TEXT("Name"));
			if (!NameArgument)
			{
				NameArgument = FindNamedArgument(Arguments, TEXT("ParameterName"));
			}
			if (!NameArgument)
			{
				OutError = TEXT("UE.StaticSwitchParameter requires Name=\"ParameterName\".");
				return false;
			}

			FString ParameterName;
			if (!TryExtractTextLiteral(NameArgument->Expression, ParameterName) || ParameterName.TrimStartAndEnd().IsEmpty())
			{
				OutError = TEXT("UE.StaticSwitchParameter Name must be a text value.");
				return false;
			}

			FTextShaderPropertyDefinition SwitchProperty;
			SwitchProperty.Name = ParameterName.TrimStartAndEnd();
			SwitchProperty.ParameterNodeType = TEXT("StaticSwitchParameter");
			const FCodeCallArgument* DefaultArgument = FindNamedArgument(Arguments, TEXT("Default"));
			if (!DefaultArgument)
			{
				DefaultArgument = FindNamedArgument(Arguments, TEXT("DefaultValue"));
			}
			if (DefaultArgument)
			{
				bool bDefault = false;
				if (!TryExtractBooleanLiteral(DefaultArgument->Expression, bDefault))
				{
					OutError = TEXT("UE.StaticSwitchParameter Default/DefaultValue must be true or false.");
					return false;
				}
				SwitchProperty.bHasDefaultValue = true;
				SwitchProperty.ScalarDefaultValue = bDefault ? 1.0 : 0.0;
			}
			if (const FCodeCallArgument* GroupArgument = FindNamedArgument(Arguments, TEXT("Group")))
			{
				(void)TryExtractTextLiteral(GroupArgument->Expression, SwitchProperty.Metadata.Group);
			}
			if (const FCodeCallArgument* DescriptionArgument = FindNamedArgument(Arguments, TEXT("Description")))
			{
				(void)TryExtractTextLiteral(DescriptionArgument->Expression, SwitchProperty.Metadata.Description);
			}
			if (const FCodeCallArgument* SortArgument = FindNamedArgument(Arguments, TEXT("SortPriority")))
			{
				int32 SortPriority = 32;
				if (!TryExtractIntegerLiteral(SortArgument->Expression, SortPriority))
				{
					OutError = TEXT("UE.StaticSwitchParameter SortPriority must be an integer literal.");
					return false;
				}
				SwitchProperty.Metadata.bHasSortPriority = true;
				SwitchProperty.Metadata.SortPriority = SortPriority;
			}

			return EvaluateStaticSwitchParameterCall(SwitchProperty, Arguments, OutValue, OutError);
		}

		if (!bIsSubstrateBuiltinCall
			&& (FunctionName.Equals(TEXT("CollectionParam"), ESearchCase::IgnoreCase)
			|| FunctionName.Equals(TEXT("CollectionParameter"), ESearchCase::IgnoreCase))
			)
		{
			const FCodeCallArgument* CollectionArgument = FindNamedArgument(Arguments, TEXT("Collection"));
			if (!CollectionArgument)
			{
				CollectionArgument = FindNamedArgument(Arguments, TEXT("Asset"));
			}
			if (!CollectionArgument)
			{
				OutError = TEXT("UE.CollectionParam requires Collection=Path(...).");
				return false;
			}

			FString CollectionText;
			if (!TryExtractAssetReferenceText(CollectionArgument->Expression, CollectionText))
			{
				OutError = TEXT("UE.CollectionParam Collection must be Path(...) or an Unreal object path.");
				return false;
			}

			FString CollectionObjectPath;
			if (!TryResolveDreamShaderAssetReference(CollectionText, CollectionObjectPath, OutError))
			{
				OutError = FString::Printf(TEXT("UE.CollectionParam Collection is invalid: %s"), *OutError);
				return false;
			}

			UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(nullptr, *CollectionObjectPath);
			if (!Collection)
			{
				OutError = FString::Printf(TEXT("UE.CollectionParam could not load MaterialParameterCollection '%s'."), *CollectionObjectPath);
				return false;
			}

			const FCodeCallArgument* ParameterArgument = FindNamedArgument(Arguments, TEXT("Parameter"));
			if (!ParameterArgument)
			{
				ParameterArgument = FindNamedArgument(Arguments, TEXT("ParameterName"));
			}
			if (!ParameterArgument)
			{
				OutError = TEXT("UE.CollectionParam requires Parameter=\"Name\".");
				return false;
			}

			FString ParameterText;
			if (!TryExtractTextLiteral(ParameterArgument->Expression, ParameterText) || ParameterText.TrimStartAndEnd().IsEmpty())
			{
				OutError = TEXT("UE.CollectionParam Parameter must be a text value.");
				return false;
			}

			const FName ParameterName(*ParameterText.TrimStartAndEnd());
			const bool bIsScalarParameter = Collection->GetScalarParameterByName(ParameterName) != nullptr;
			const bool bIsVectorParameter = Collection->GetVectorParameterByName(ParameterName) != nullptr;
			if (!bIsScalarParameter && !bIsVectorParameter)
			{
				OutError = FString::Printf(
					TEXT("UE.CollectionParam collection '%s' does not contain parameter '%s'."),
					*CollectionObjectPath,
					*ParameterText);
				return false;
			}

			auto* Expression = Cast<UMaterialExpressionCollectionParameter>(
				CreateExpression(UMaterialExpressionCollectionParameter::StaticClass(), -520, ConsumeNodeY()));
			if (!Expression)
			{
				OutError = TEXT("Failed to create UE.CollectionParam node.");
				return false;
			}

			Expression->Collection = Collection;
			Expression->ParameterName = ParameterName;
			Expression->ParameterId = Collection->GetParameterId(ParameterName);
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 7)
			if (!Expression->ExpressionGUID.IsValid())
			{
				Expression->ExpressionGUID = FGuid::NewGuid();
			}
#endif

			FString GroupText;
			if (HandleOptionalTextLiteralArgument(TEXT("Group"), GroupText) && !GroupText.IsEmpty())
			{
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 7)
				Expression->Group = FName(*GroupText);
#endif
			}
			else if (!OutError.IsEmpty())
			{
				return false;
			}

			if (const FCodeCallArgument* SortArgument = FindNamedArgument(Arguments, TEXT("SortPriority")))
			{
				int32 SortPriority = 32;
				if (!TryExtractIntegerLiteral(SortArgument->Expression, SortPriority))
				{
					OutError = TEXT("UE.CollectionParam SortPriority must be an integer literal.");
					return false;
				}
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 7)
				Expression->SortPriority = SortPriority;
#endif
			}

			FString DescriptionText;
			if (HandleOptionalTextLiteralArgument(TEXT("Description"), DescriptionText) && !DescriptionText.IsEmpty())
			{
				Expression->Desc = DescriptionText;
			}
			else if (!OutError.IsEmpty())
			{
				return false;
			}

			OutValue.Expression = Expression;
			OutValue.OutputIndex = 0;
			OutValue.ComponentCount = bIsVectorParameter ? 4 : 1;
			OutValue.bIsTextureObject = false;
			OutValue.bIsMaterialAttributes = false;
			OutValue.bIsSubstrateMaterial = false;
			return true;
		}

		struct FUEBuiltinDescriptor
		{
			const TCHAR* Name = TEXT("");
			TSubclassOf<UMaterialExpression> ExpressionClass;
			int32 OutputComponents = 1;
			TFunction<bool(UMaterialExpression*, FString&)> Configure;
		};

		auto TryEvaluateRegisteredBuiltin = [&](const FUEBuiltinDescriptor& Builtin) -> bool
		{
			UMaterialExpression* Expression = CreateExpression(Builtin.ExpressionClass, 520, ConsumeNodeY());
			if (!Expression)
			{
				OutError = FString::Printf(TEXT("Failed to create UE.%s."), Builtin.Name);
				return false;
			}

			if (Builtin.Configure && !Builtin.Configure(Expression, OutError))
			{
				return false;
			}

			int32 OutputComponents = Builtin.OutputComponents;
			if (Expression->Outputs.IsValidIndex(0))
			{
				int32 KnownOutputComponents = 0;
				if (TryResolveKnownExpressionOutputComponentCount(Expression, 0, KnownOutputComponents) && KnownOutputComponents > 0)
				{
					OutputComponents = KnownOutputComponents;
				}
			}

			OutValue.Expression = Expression;
			OutValue.OutputIndex = 0;
			OutValue.ComponentCount = OutputComponents;
			OutValue.bIsTextureObject = false;
			OutValue.bIsMaterialAttributes = false;
			OutValue.bIsSubstrateMaterial = false;
			OutValue.bHasAuthoritativeComponentCount = true;
			return true;
		};

		// Register explicit DreamShader UE.* sugar here. Anything not listed falls back
		// to the reflected generic MaterialExpression path below.
		TArray<FUEBuiltinDescriptor> Builtins;
		Builtins.Reserve(10);

		Builtins.Add({
			TEXT("TexCoord"),
			UMaterialExpressionTextureCoordinate::StaticClass(),
			2,
			[&](UMaterialExpression* RawExpression, FString&) -> bool
			{
				auto* Expression = CastChecked<UMaterialExpressionTextureCoordinate>(RawExpression);
				return HandleIntegerLiteralArgument(TEXT("Index"), Expression->CoordinateIndex)
					&& HandleScalarLiteralArgument(TEXT("UTiling"), Expression->UTiling)
					&& HandleScalarLiteralArgument(TEXT("VTiling"), Expression->VTiling)
					&& HandleBooleanLiteralArgument(TEXT("UnMirrorU"), [&](const bool bValue) { Expression->UnMirrorU = bValue ? 1U : 0U; })
					&& HandleBooleanLiteralArgument(TEXT("UnMirrorV"), [&](const bool bValue) { Expression->UnMirrorV = bValue ? 1U : 0U; });
			}
		});

		Builtins.Add({
			TEXT("Time"),
			UMaterialExpressionTime::StaticClass(),
			1,
			[&](UMaterialExpression* RawExpression, FString& Error) -> bool
			{
				auto* Expression = CastChecked<UMaterialExpressionTime>(RawExpression);
				if (const FCodeCallArgument* Argument = FindNamedArgument(Arguments, TEXT("Period")))
				{
					double Period = 0.0;
					if (!TryExtractScalarLiteral(Argument->Expression, Period))
					{
						Error = FString::Printf(TEXT("UE.%s Period must be a numeric literal."), *FunctionName);
						return false;
					}

					Expression->bOverride_Period = true;
					Expression->Period = static_cast<float>(Period);
				}

				return HandleBooleanLiteralArgument(TEXT("IgnorePause"), [&](const bool bValue) { Expression->bIgnorePause = bValue ? 1U : 0U; });
			}
		});

		Builtins.Add({
			TEXT("Panner"),
			UMaterialExpressionPanner::StaticClass(),
			2,
			[&](UMaterialExpression* RawExpression, FString&) -> bool
			{
				auto* Expression = CastChecked<UMaterialExpressionPanner>(RawExpression);
				return HandleOptionalInputArgument(TEXT("Coordinate"), Expression->Coordinate)
					&& HandleOptionalInputArgument(TEXT("Time"), Expression->Time)
					&& HandleOptionalInputArgument(TEXT("Speed"), Expression->Speed)
					&& HandleScalarLiteralArgument(TEXT("SpeedX"), Expression->SpeedX)
					&& HandleScalarLiteralArgument(TEXT("SpeedY"), Expression->SpeedY)
					&& HandleBooleanLiteralArgument(TEXT("FractionalPart"), [&](const bool bValue) { Expression->bFractionalPart = bValue ? 1U : 0U; });
			}
		});

		Builtins.Add({ TEXT("WorldPosition"), UMaterialExpressionWorldPosition::StaticClass(), 3, {} });
		if (UClass* ObjectPositionClass = GetDreamShaderObjectPositionExpressionClass())
		{
			Builtins.Add({ TEXT("ObjectPositionWS"), ObjectPositionClass, 3, {} });
		}
		Builtins.Add({ TEXT("CameraVectorWS"), UMaterialExpressionCameraVectorWS::StaticClass(), 3, {} });
		Builtins.Add({ TEXT("VertexNormalWS"), UMaterialExpressionVertexNormalWS::StaticClass(), 3, {} });
		Builtins.Add({ TEXT("VertexTangentWS"), UMaterialExpressionVertexTangentWS::StaticClass(), 3, {} });
		if (UClass* ScreenPositionClass = GetDreamShaderScreenPositionExpressionClass())
		{
			Builtins.Add({ TEXT("ScreenPosition"), ScreenPositionClass, 2, {} });
		}
		Builtins.Add({ TEXT("VertexColor"), UMaterialExpressionVertexColor::StaticClass(), 4, {} });
		// Scene reads as REAL nodes (Stage 4 of the ThinCustom convergence): the same DSL surface the
		// Instance backend lowers to named value-input chunks. No-arg = current pixel (no UV/offset
		// input wired), matching the Instance chunks' INDEX_NONE compilation. The engine's own
		// translucency validation replaces the Instance backend's hand-written pre-flight gate.
		Builtins.Add({ TEXT("PixelDepth"), UMaterialExpressionPixelDepth::StaticClass(), 1, {} });
		Builtins.Add({ TEXT("SceneDepth"), UMaterialExpressionSceneDepth::StaticClass(), 1, {} });
		Builtins.Add({ TEXT("SceneColor"), UMaterialExpressionSceneColor::StaticClass(), 4, {} });

		// The Instance backend's no-arg state-read wave (Stage 6 of the ThinCustom convergence): every
		// name the .ush lowering accepted resolves to a real node with matching semantics, so Instance
		// sources keep compiling unchanged through the alias. Semantics pinned against the DS_ macros
		// in DreamShaderBuiltins.ush.
		Builtins.Add({
			// GetTranslatedWorldPosition(Parameters) == camera-relative world position; the node's
			// default WPT_Default is ABSOLUTE world position, so the shader-offset enum must be forced.
			TEXT("TranslatedWorldPosition"),
			UMaterialExpressionWorldPosition::StaticClass(),
			3,
			[&](UMaterialExpression* RawExpression, FString&) -> bool
			{
				CastChecked<UMaterialExpressionWorldPosition>(RawExpression)->WorldPositionShaderOffset = WPT_CameraRelative;
				return true;
			}
		});
		if (UClass* ObjectPositionClass = GetDreamShaderObjectPositionExpressionClass())
		{
			// Alias of ObjectPositionWS (the Instance surface spells it without the WS suffix).
			Builtins.Add({ TEXT("ObjectPosition"), ObjectPositionClass, 3, {} });
		}
		Builtins.Add({ TEXT("ObjectRadius"), UMaterialExpressionObjectRadius::StaticClass(), 1, {} });
		Builtins.Add({ TEXT("ObjectBounds"), UMaterialExpressionObjectBounds::StaticClass(), 3, {} });
		Builtins.Add({ TEXT("CameraVector"), UMaterialExpressionCameraVectorWS::StaticClass(), 3, {} });
		Builtins.Add({ TEXT("CameraPosition"), UMaterialExpressionCameraPositionWS::StaticClass(), 3, {} });
		Builtins.Add({ TEXT("ReflectionVector"), UMaterialExpressionReflectionVectorWS::StaticClass(), 3, {} });
		Builtins.Add({ TEXT("PixelNormalWS"), UMaterialExpressionPixelNormalWS::StaticClass(), 3, {} });
		Builtins.Add({ TEXT("TwoSidedSign"), UMaterialExpressionTwoSidedSign::StaticClass(), 1, {} });
		Builtins.Add({ TEXT("PerInstanceRandom"), UMaterialExpressionPerInstanceRandom::StaticClass(), 1, {} });
		Builtins.Add({ TEXT("PerInstanceFadeAmount"), UMaterialExpressionPerInstanceFadeAmount::StaticClass(), 1, {} });
		if (UClass* ScreenPositionClass = GetDreamShaderScreenPositionExpressionClass())
		{
			// ViewportUV == the ScreenPosition node's output 0 ("ViewportUV", GetViewportUV in the
			// translator); the engine has no dedicated ViewportUV expression class.
			Builtins.Add({ TEXT("ViewportUV"), ScreenPositionClass, 2, {} });
		}

		Builtins.Add({
			TEXT("TransformVector"),
			UMaterialExpressionTransform::StaticClass(),
			3,
			[&](UMaterialExpression* RawExpression, FString& Error) -> bool
			{
				auto* Expression = CastChecked<UMaterialExpressionTransform>(RawExpression);
				if (!HandleRequiredInputArgument(TEXT("Input"), 0, Expression->Input))
				{
					return false;
				}

				FString SourceText = TEXT("Tangent");
				if (!HandleOptionalTextLiteralArgument(TEXT("Source"), SourceText))
				{
					return false;
				}

				FString DestinationText = TEXT("World");
				if (!HandleOptionalTextLiteralArgument(TEXT("Destination"), DestinationText))
				{
					return false;
				}

				EMaterialVectorCoordTransformSource SourceBasis = TRANSFORMSOURCE_Tangent;
				EMaterialVectorCoordTransform DestinationBasis = TRANSFORM_World;
				if (!TryResolveVectorTransformBasis(SourceText, SourceBasis)
					|| !TryResolveVectorTransformTarget(DestinationText, DestinationBasis))
				{
					Error = TEXT("UE.TransformVector Source/Destination is invalid.");
					return false;
				}

				Expression->TransformSourceType = SourceBasis;
				Expression->TransformType = DestinationBasis;
				return true;
			}
		});

		Builtins.Add({
			TEXT("TransformPosition"),
			UMaterialExpressionTransformPosition::StaticClass(),
			3,
			[&](UMaterialExpression* RawExpression, FString& Error) -> bool
			{
				auto* Expression = CastChecked<UMaterialExpressionTransformPosition>(RawExpression);
				if (!HandleRequiredInputArgument(TEXT("Input"), 0, Expression->Input))
				{
					return false;
				}

				FString SourceText = TEXT("Local");
				if (!HandleOptionalTextLiteralArgument(TEXT("Source"), SourceText))
				{
					return false;
				}

				FString DestinationText = TEXT("World");
				if (!HandleOptionalTextLiteralArgument(TEXT("Destination"), DestinationText))
				{
					return false;
				}

				EMaterialPositionTransformSource SourceBasis = TRANSFORMPOSSOURCE_Local;
				EMaterialPositionTransformSource DestinationBasis = TRANSFORMPOSSOURCE_World;
				if (!TryResolvePositionTransformBasis(SourceText, SourceBasis)
					|| !TryResolvePositionTransformBasis(DestinationText, DestinationBasis))
				{
					Error = TEXT("UE.TransformPosition Source/Destination is invalid.");
					return false;
				}

				Expression->TransformSourceType = SourceBasis;
				Expression->TransformType = DestinationBasis;

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 5)
				if (!HandleOptionalInputArgument(TEXT("PeriodicWorldTileSize"), Expression->PeriodicWorldTileSize))
				{
					return false;
				}
#endif

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
				return HandleOptionalInputArgument(TEXT("FirstPersonInterpolationAlpha"), Expression->FirstPersonInterpolationAlpha);
#else
				if (FindNamedArgument(Arguments, TEXT("FirstPersonInterpolationAlpha")))
				{
					Error = TEXT("UE.TransformPosition FirstPersonInterpolationAlpha requires Unreal Engine 5.6 or newer.");
					return false;
				}
				return true;
#endif
			}
		});

		if (!bIsSubstrateBuiltinCall)
		{
			for (const FUEBuiltinDescriptor& Builtin : Builtins)
			{
				if (FunctionName.Equals(Builtin.Name, ESearchCase::IgnoreCase))
				{
					return TryEvaluateRegisteredBuiltin(Builtin);
				}
			}
		}

		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (!Argument.bIsNamed)
			{
				OutError = FString::Printf(TEXT("Generic %s.%s calls require named arguments."), CallNamespace, *FunctionName);
				return false;
			}
		}

		const FSubstrateBuiltinDescriptor* SubstrateBuiltin = nullptr;
		if (bIsSubstrateBuiltinCall)
		{
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
			SubstrateBuiltin = FindSubstrateBuiltinDescriptor(FunctionName);
			if (!SubstrateBuiltin)
			{
				OutError = FString::Printf(TEXT("Unsupported Substrate builtin call '%s' in Graph."), *CalleeName);
				return false;
			}
#else
			OutError = FString::Printf(TEXT("Substrate builtin call '%s' requires Unreal Engine 5.4 or newer."), *CalleeName);
			return false;
#endif
		}

		const FCodeCallArgument* OutputTypeArgument = FindNamedArgument(Arguments, TEXT("OutputType"));
		if (!OutputTypeArgument)
		{
			OutputTypeArgument = FindNamedArgument(Arguments, TEXT("ResultType"));
		}
		if (!OutputTypeArgument && !SubstrateBuiltin)
		{
			OutError = FString::Printf(
				TEXT("Unsupported UE builtin call '%s' in Graph. For generic MaterialExpression calls, add OutputType=\"float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture/Substrate\"."),
				*CalleeName);
			return false;
		}

		FString OutputTypeText;
		int32 OutputComponents = 0;
		bool bIsTextureObject = false;
		ETextShaderTextureType TextureType = ETextShaderTextureType::Texture2D;
		bool bIsSubstrateMaterial = false;
		bool bHasAuthoritativeComponentCount = false;
		if (SubstrateBuiltin)
		{
			OutputTypeText = SubstrateBuiltin->bIsSubstrateOutput ? TEXT("Substrate") : TEXT("auto");
			OutputComponents = SubstrateBuiltin->bIsSubstrateOutput ? 0 : 1;
			bIsSubstrateMaterial = SubstrateBuiltin->bIsSubstrateOutput;
		}
		else
		{
			if (!TryExtractLiteralText(OutputTypeArgument->Expression, OutputTypeText))
			{
				OutError = FString::Printf(TEXT("UE.%s OutputType must be a literal value."), *FunctionName);
				return false;
			}

			if (!TryResolveCodeDeclaredType(OutputTypeText, OutputComponents, bIsTextureObject, TextureType, bIsSubstrateMaterial))
			{
				if (IsSubstrateMaterialType(OutputTypeText) && !IsSubstrateMaterialTypeSupported())
				{
					OutError = FString::Printf(TEXT("UE.%s OutputType=\"Substrate\" requires Unreal Engine 5.4 or newer."), *FunctionName);
					return false;
				}
				OutError = FString::Printf(TEXT("UE.%s OutputType '%s' is not supported."), *FunctionName, *OutputTypeText);
				return false;
			}
		}

		FString ClassSpecifier = SubstrateBuiltin ? FString(SubstrateBuiltin->ClassName) : FunctionName;
		if (!SubstrateBuiltin)
		{
			if (const FCodeCallArgument* ClassArgument = FindNamedArgument(Arguments, TEXT("Class")))
			{
				if (!TryExtractLiteralText(ClassArgument->Expression, ClassSpecifier))
				{
					OutError = FString::Printf(TEXT("UE.%s Class must be a literal value."), *FunctionName);
					return false;
				}
			}
			else if (FunctionName.Equals(TEXT("Expression"), ESearchCase::IgnoreCase))
			{
				OutError = TEXT("UE.Expression requires Class=\"MaterialExpressionName\".");
				return false;
			}
		}
		else if (FindNamedArgument(Arguments, TEXT("Class")))
		{
			OutError = FString::Printf(TEXT("Substrate.%s uses a fixed MaterialExpression class and does not accept Class."), *FunctionName);
			return false;
		}

		UClass* ExpressionClass = ResolveMaterialExpressionClass(ClassSpecifier);
		if (!ExpressionClass)
		{
			OutError = FString::Printf(TEXT("UE.%s could not resolve MaterialExpression class '%s'."), *FunctionName, *ClassSpecifier);
			return false;
		}
		if (SubstrateBuiltin && !IsSubstrateExpressionClass(ExpressionClass))
		{
			OutError = FString::Printf(TEXT("Substrate.%s resolved to non-Substrate class '%s'."), *FunctionName, *ExpressionClass->GetName());
			return false;
		}

		const FCodeCallArgument* OutputNameArgument = FindNamedArgument(Arguments, TEXT("Output"));
		if (!OutputNameArgument)
		{
			OutputNameArgument = FindNamedArgument(Arguments, TEXT("OutputName"));
		}
		const FCodeCallArgument* OutputIndexArgument = FindNamedArgument(Arguments, TEXT("OutputIndex"));
		if (OutputNameArgument && OutputIndexArgument)
		{
			OutError = FString::Printf(TEXT("UE.%s cannot use OutputName/Output together with OutputIndex."), *FunctionName);
			return false;
		}

		const bool bCanReuseExpressionNode = !ExpressionClass->IsChildOf(UMaterialExpressionCustom::StaticClass());
		FString ExpressionReuseKey;
		FString OutputReuseKey;
		if (bCanReuseExpressionNode)
		{
			TSet<FString> ExcludedReuseArguments;
			ExcludedReuseArguments.Add(UE::DreamShader::NormalizeSettingKey(TEXT("Class")));
			ExcludedReuseArguments.Add(UE::DreamShader::NormalizeSettingKey(TEXT("OutputType")));
			ExcludedReuseArguments.Add(UE::DreamShader::NormalizeSettingKey(TEXT("ResultType")));
			ExcludedReuseArguments.Add(UE::DreamShader::NormalizeSettingKey(TEXT("Output")));
			ExcludedReuseArguments.Add(UE::DreamShader::NormalizeSettingKey(TEXT("OutputName")));
			ExcludedReuseArguments.Add(UE::DreamShader::NormalizeSettingKey(TEXT("OutputIndex")));

			if (TryBuildReusableCallKey(CallNamespace, FunctionName, Arguments, ExcludedReuseArguments, ExpressionReuseKey))
			{
				ExpressionReuseKey = FString::Printf(
					TEXT("%s|Class=%s|OutputType=%s"),
					*ExpressionReuseKey,
					*ExpressionClass->GetName(),
					*NormalizeCodeReuseLiteralText(OutputTypeText));
				if (OutputNameArgument)
				{
					FString OutputNameText;
					if (TryExtractLiteralText(OutputNameArgument->Expression, OutputNameText))
					{
						OutputReuseKey = ExpressionReuseKey + FString::Printf(TEXT("|OutputName=%s"), *NormalizeCodeReuseLiteralText(OutputNameText));
					}
				}
				else if (OutputIndexArgument)
				{
					FString OutputIndexText;
					if (TryExtractLiteralText(OutputIndexArgument->Expression, OutputIndexText))
					{
						OutputReuseKey = ExpressionReuseKey + FString::Printf(TEXT("|OutputIndex=%s"), *NormalizeCodeReuseLiteralText(OutputIndexText));
					}
				}
				else
				{
					OutputReuseKey = ExpressionReuseKey + TEXT("|OutputIndex=0");
				}
				if (TryFindReusableExpressionValue(OutputReuseKey, OutValue))
				{
					return true;
				}
			}
		}

		UMaterialExpression* Expression = nullptr;
		FCodeValue ReusableExpressionNodeValue;
		bool bReusedExpressionNode = false;
		if (TryFindReusableExpressionValue(ExpressionReuseKey, ReusableExpressionNodeValue))
		{
			Expression = Cast<UMaterialExpression>(ReusableExpressionNodeValue.Expression);
			bReusedExpressionNode = Expression != nullptr;
		}

		if (!Expression)
		{
			Expression = Cast<UMaterialExpression>(CreateExpression(ExpressionClass, 520, ConsumeNodeY()));
		}
		if (!Expression)
		{
			OutError = FString::Printf(TEXT("UE.%s failed to create '%s'."), *FunctionName, *ExpressionClass->GetName());
			return false;
		}

		UMaterialExpressionCustom* CustomExpression = Cast<UMaterialExpressionCustom>(Expression);
		if (CustomExpression && bIsSubstrateMaterial)
		{
			OutError = FString::Printf(TEXT("UE.%s OutputType=\"Substrate\" is not supported by UMaterialExpressionCustom."), *FunctionName);
			return false;
		}
		if (CustomExpression && !bReusedExpressionNode)
		{
			ECustomMaterialOutputType CustomOutputType = CMOT_Float1;
			if (!TryResolveCustomOutputType(OutputTypeText, CustomOutputType))
			{
				OutError = FString::Printf(TEXT("UE.%s OutputType '%s' is not a valid Custom node output type."), *FunctionName, *OutputTypeText);
				return false;
			}
			CustomExpression->OutputType = CustomOutputType;
			CustomExpression->Inputs.Reset();
			CustomExpression->AdditionalOutputs.Reset();
		}

		TArray<TPair<FName, FCodeValue>> BoundInputValues;
		for (const FCodeCallArgument& Argument : Arguments)
		{
			const FString NormalizedArgumentName = UE::DreamShader::NormalizeSettingKey(Argument.Name);
			if (NormalizedArgumentName == UE::DreamShader::NormalizeSettingKey(TEXT("Class"))
				|| NormalizedArgumentName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputType"))
				|| NormalizedArgumentName == UE::DreamShader::NormalizeSettingKey(TEXT("ResultType"))
				|| NormalizedArgumentName == UE::DreamShader::NormalizeSettingKey(TEXT("Output"))
				|| NormalizedArgumentName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputName"))
				|| NormalizedArgumentName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputIndex")))
			{
				continue;
			}

			FExpressionInput* BoundInputByPinName = nullptr;
			int32 BoundInputIndexByPinName = INDEX_NONE;
			for (int32 InputIndex = 0; InputIndex < GetDreamShaderExpressionInputCount(Expression); ++InputIndex)
			{
				FExpressionInput* CandidateInput = Expression->GetInput(InputIndex);
				const FName InputName = Expression->GetInputName(InputIndex);
				if (CandidateInput
					&& !InputName.IsNone()
					&& UE::DreamShader::NormalizeSettingKey(InputName.ToString()) == NormalizedArgumentName)
				{
					BoundInputByPinName = CandidateInput;
					BoundInputIndexByPinName = InputIndex;
					break;
				}
			}

			FProperty* BoundProperty = BoundInputByPinName ? nullptr : FindMaterialExpressionArgumentProperty(ExpressionClass, Argument.Name);
			if (!BoundInputByPinName && !BoundProperty)
			{
				if (!CustomExpression)
				{
					OutError = FString::Printf(TEXT("UE.%s: '%s' is not a property on '%s'."), *FunctionName, *Argument.Name, *ExpressionClass->GetName());
					return false;
				}

				FCodeValue InputValue;
				if (!EvaluateExpression(Argument.Expression, InputValue, OutError))
				{
					OutError = FString::Printf(TEXT("UE.%s input '%s': %s"), *FunctionName, *Argument.Name, *OutError);
					return false;
				}
				if (InputValue.bIsSubstrateMaterial)
				{
					OutError = FString::Printf(TEXT("UE.%s Custom input '%s' does not accept Substrate values."), *FunctionName, *Argument.Name);
					return false;
				}

				FCustomInput CustomInput;
				CustomInput.InputName = FName(*Argument.Name);
				if (!bReusedExpressionNode)
				{
					CustomExpression->Inputs.Add(CustomInput);
					ConnectCodeValueToInput(CustomExpression->Inputs.Last().Input, InputValue);
				}
				BoundInputValues.Add(TPair<FName, FCodeValue>(FName(*NormalizedArgumentName), InputValue));
				continue;
			}

			if (BoundInputByPinName || IsMaterialExpressionInputProperty(BoundProperty))
			{
				if (bReusedExpressionNode)
				{
					continue;
				}

				FCodeValue InputValue;
				if (!EvaluateExpression(Argument.Expression, InputValue, OutError))
				{
					OutError = FString::Printf(TEXT("UE.%s input '%s': %s"), *FunctionName, *Argument.Name, *OutError);
					return false;
				}

				FExpressionInput* Input = BoundInputByPinName
					? BoundInputByPinName
					: BoundProperty->ContainerPtrToValuePtr<FExpressionInput>(Expression);
				if (!Input)
				{
					OutError = FString::Printf(TEXT("UE.%s failed to bind input '%s'."), *FunctionName, *Argument.Name);
					return false;
				}

				int32 BoundInputIndex = BoundInputIndexByPinName;
				if (BoundInputIndex == INDEX_NONE)
				{
					for (int32 InputIndex = 0; InputIndex < GetDreamShaderExpressionInputCount(Expression); ++InputIndex)
					{
						if (Expression->GetInput(InputIndex) == Input)
						{
							BoundInputIndex = InputIndex;
							break;
						}
					}
				}
				if (BoundInputIndex != INDEX_NONE)
				{
					const EMaterialValueType InputValueType = GetDreamShaderExpressionInputValueType(Expression, BoundInputIndex);
					if (IsSubstrateMaterialValueType(InputValueType))
					{
						if (!InputValue.bIsSubstrateMaterial)
						{
							OutError = FString::Printf(TEXT("%s.%s input '%s' expects a Substrate value."), CallNamespace, *FunctionName, *Argument.Name);
							return false;
						}
					}
					else if (InputValueType == MCT_MaterialAttributes)
					{
						if (!InputValue.bIsMaterialAttributes)
						{
							OutError = FString::Printf(TEXT("%s.%s input '%s' expects a MaterialAttributes value."), CallNamespace, *FunctionName, *Argument.Name);
							return false;
						}
					}
					else if (InputValue.bIsSubstrateMaterial)
					{
						OutError = FString::Printf(TEXT("%s.%s input '%s' does not accept Substrate values."), CallNamespace, *FunctionName, *Argument.Name);
						return false;
					}
					else if (InputValue.bIsMaterialAttributes)
					{
						OutError = FString::Printf(TEXT("%s.%s input '%s' does not accept MaterialAttributes values."), CallNamespace, *FunctionName, *Argument.Name);
						return false;
					}
				}

				ConnectCodeValueToInput(*Input, InputValue);
				BoundInputValues.Add(TPair<FName, FCodeValue>(FName(*NormalizedArgumentName), InputValue));
			}
			else
			{
				if (bReusedExpressionNode)
				{
					continue;
				}

				FString LiteralText;
				const bool bWantsAssetReference = CastField<FObjectPropertyBase>(BoundProperty) != nullptr;
				if (bWantsAssetReference)
				{
					if (!TryExtractAssetReferenceText(Argument.Expression, LiteralText))
					{
						OutError = FString::Printf(TEXT("UE.%s property '%s' must use Path(...) or an Unreal object path."), *FunctionName, *Argument.Name);
						return false;
					}
				}
				else if (!TryExtractLiteralText(Argument.Expression, LiteralText))
				{
					OutError = FString::Printf(TEXT("UE.%s property '%s' must use a literal value."), *FunctionName, *Argument.Name);
					return false;
				}

				FString LiteralError;
				if (!SetMaterialExpressionLiteralProperty(Expression, BoundProperty, LiteralText, LiteralError))
				{
					OutError = FString::Printf(TEXT("UE.%s property '%s': %s"), *FunctionName, *Argument.Name, *LiteralError);
					return false;
				}
			}
		}

		if (CustomExpression)
		{
			ECustomMaterialOutputType CustomOutputType = CustomExpression->OutputType;
			FString CustomOutputName;
			int32 RequestedCustomOutputIndex = 0;
			if (OutputNameArgument)
			{
				if (!TryExtractLiteralText(OutputNameArgument->Expression, CustomOutputName) || CustomOutputName.TrimStartAndEnd().IsEmpty())
				{
					OutError = FString::Printf(TEXT("UE.%s OutputName must be a non-empty literal value."), *FunctionName);
					return false;
				}
				RequestedCustomOutputIndex = 1;
			}
			else if (OutputIndexArgument)
			{
				if (!TryExtractIntegerLiteral(OutputIndexArgument->Expression, RequestedCustomOutputIndex) || RequestedCustomOutputIndex < 0)
				{
					OutError = FString::Printf(TEXT("UE.%s OutputIndex is out of range for '%s'."), *FunctionName, *ExpressionClass->GetName());
					return false;
				}
			}

			// A node that declared its outputs explicitly (AdditionalOutputs=...) already has the one this
			// call selects, with its real name and type. Only synthesize a placeholder set when the
			// selector is all we were given.
			const bool bRequestedOutputAlreadyDeclared = CustomOutputName.IsEmpty()
				? CustomExpression->AdditionalOutputs.Num() >= RequestedCustomOutputIndex
				: CustomExpression->AdditionalOutputs.ContainsByPredicate(
					[&CustomOutputName](const FCustomOutput& AdditionalOutput)
					{
						return AdditionalOutput.OutputName.ToString().Equals(CustomOutputName, ESearchCase::IgnoreCase);
					});

			for (int32 AdditionalOutputIndex = 0; !bRequestedOutputAlreadyDeclared && AdditionalOutputIndex < RequestedCustomOutputIndex; ++AdditionalOutputIndex)
			{
				FCustomOutput CustomOutput;
				CustomOutput.OutputName = FName(*(
					AdditionalOutputIndex == RequestedCustomOutputIndex - 1 && !CustomOutputName.IsEmpty()
						? CustomOutputName
						: FString::Printf(TEXT("Output%d"), AdditionalOutputIndex + 1)));
				CustomOutput.OutputType = CustomOutputType;
				CustomExpression->AdditionalOutputs.Add(CustomOutput);
			}

			RebuildDreamShaderCustomOutputs(CustomExpression);
		}

		int32 ResolvedOutputIndex = 0;
		if (OutputIndexArgument)
		{
			if (!TryExtractIntegerLiteral(OutputIndexArgument->Expression, ResolvedOutputIndex)
				|| ResolvedOutputIndex < 0
				|| !Expression->Outputs.IsValidIndex(ResolvedOutputIndex))
			{
				OutError = FString::Printf(TEXT("UE.%s OutputIndex is out of range for '%s'."), *FunctionName, *ExpressionClass->GetName());
				return false;
			}
		}
		else if (OutputNameArgument)
		{
			FString OutputNameText;
			if (!TryExtractLiteralText(OutputNameArgument->Expression, OutputNameText))
			{
				OutError = FString::Printf(TEXT("UE.%s OutputName must be a literal value."), *FunctionName);
				return false;
			}

			if (!TryResolveExpressionOutputIndex(Expression, OutputNameText, ResolvedOutputIndex))
			{
				OutError = FString::Printf(TEXT("UE.%s output '%s' was not found on '%s'."), *FunctionName, *OutputNameText, *ExpressionClass->GetName());
				return false;
			}
		}
		else if (!Expression->Outputs.IsValidIndex(0))
		{
			OutError = FString::Printf(TEXT("UE.%s created '%s', but it has no material outputs."), *FunctionName, *ExpressionClass->GetName());
			return false;
		}

		if (UMaterialExpressionStaticSwitchParameter* StaticSwitchExpression = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
		{
			if (!StaticSwitchExpression->ParameterName.IsNone())
			{
				if (!StaticSwitchExpression->ExpressionGUID.IsValid())
				{
					StaticSwitchExpression->ExpressionGUID = FGuid::NewGuid();
				}
				if (Material)
				{
					Material->SetStaticSwitchParameterValueEditorOnly(
						StaticSwitchExpression->ParameterName,
						StaticSwitchExpression->DefaultValue != 0,
						StaticSwitchExpression->ExpressionGUID);
				}
				else if (MaterialFunction)
				{
					MaterialFunction->SetStaticSwitchParameterValueEditorOnly(
						StaticSwitchExpression->ParameterName,
						StaticSwitchExpression->DefaultValue != 0,
						StaticSwitchExpression->ExpressionGUID);
				}
			}
		}

		if (UMaterialExpressionStaticComponentMaskParameter* StaticComponentMaskExpression = Cast<UMaterialExpressionStaticComponentMaskParameter>(Expression))
		{
			if (!StaticComponentMaskExpression->ParameterName.IsNone())
			{
				if (!StaticComponentMaskExpression->ExpressionGUID.IsValid())
				{
					StaticComponentMaskExpression->ExpressionGUID = FGuid::NewGuid();
				}
				if (Material)
				{
					Material->SetStaticComponentMaskParameterValueEditorOnly(
						StaticComponentMaskExpression->ParameterName,
						StaticComponentMaskExpression->DefaultR != 0,
						StaticComponentMaskExpression->DefaultG != 0,
						StaticComponentMaskExpression->DefaultB != 0,
						StaticComponentMaskExpression->DefaultA != 0,
						StaticComponentMaskExpression->ExpressionGUID);
				}
				else if (MaterialFunction)
				{
					MaterialFunction->SetStaticComponentMaskParameterValueEditorOnly(
						StaticComponentMaskExpression->ParameterName,
						StaticComponentMaskExpression->DefaultR != 0,
						StaticComponentMaskExpression->DefaultG != 0,
						StaticComponentMaskExpression->DefaultB != 0,
						StaticComponentMaskExpression->DefaultA != 0,
						StaticComponentMaskExpression->ExpressionGUID);
				}
			}
		}

		auto FindBoundInputValue = [&BoundInputValues](const TCHAR* InputName) -> const FCodeValue*
		{
			const FName NormalizedInputName(*UE::DreamShader::NormalizeSettingKey(InputName));
			for (const TPair<FName, FCodeValue>& BoundInputValue : BoundInputValues)
			{
				if (BoundInputValue.Key == NormalizedInputName)
				{
					return &BoundInputValue.Value;
				}
			}
			return nullptr;
		};

		auto ApplyNumericInputComponentCount = [&OutputComponents, &bIsTextureObject, &bHasAuthoritativeComponentCount](const FCodeValue* InputValue)
		{
			if (InputValue && !InputValue->bIsTextureObject && !InputValue->bIsMaterialAttributes && !InputValue->bIsSubstrateMaterial && InputValue->ComponentCount > 0)
			{
				OutputComponents = InputValue->ComponentCount;
				bIsTextureObject = false;
				bHasAuthoritativeComponentCount = true;
			}
		};

		auto ApplyMaxNumericInputComponentCount = [&OutputComponents, &bIsTextureObject, &bHasAuthoritativeComponentCount](const FCodeValue* FirstInput, const FCodeValue* SecondInput)
		{
			int32 MaxComponentCount = 0;
			for (const FCodeValue* InputValue : { FirstInput, SecondInput })
			{
				if (InputValue && !InputValue->bIsTextureObject && !InputValue->bIsMaterialAttributes && !InputValue->bIsSubstrateMaterial)
				{
					MaxComponentCount = FMath::Max(MaxComponentCount, InputValue->ComponentCount);
				}
			}
			if (MaxComponentCount > 0)
			{
				OutputComponents = MaxComponentCount;
				bIsTextureObject = false;
				bHasAuthoritativeComponentCount = true;
			}
		};

		const EMaterialValueType ActualOutputValueType = GetDreamShaderExpressionOutputValueType(Expression, ResolvedOutputIndex);
		if (bIsSubstrateMaterial && !IsSubstrateMaterialValueType(ActualOutputValueType))
		{
			OutError = FString::Printf(TEXT("%s.%s output is not a Substrate value."), CallNamespace, *FunctionName);
			return false;
		}

		if (IsSubstrateMaterialValueType(ActualOutputValueType))
		{
			OutputComponents = 0;
			bIsTextureObject = false;
			bIsSubstrateMaterial = true;
			bHasAuthoritativeComponentCount = true;
		}
		else if (ActualOutputValueType == MCT_MaterialAttributes)
		{
			OutputComponents = 0;
			bIsTextureObject = false;
			bIsSubstrateMaterial = false;
			bHasAuthoritativeComponentCount = true;
		}
		else if (IsTextureMaterialValueType(ActualOutputValueType))
		{
			OutputComponents = 0;
			bIsTextureObject = true;
			bIsSubstrateMaterial = false;
			bHasAuthoritativeComponentCount = true;
		}
		else if (SubstrateBuiltin)
		{
			const int32 ActualOutputComponents = GetComponentCountForMaterialValueType(ActualOutputValueType);
			if (ActualOutputComponents > 0)
			{
				OutputComponents = ActualOutputComponents;
				bIsTextureObject = false;
				bIsSubstrateMaterial = false;
				bHasAuthoritativeComponentCount = true;
			}
		}
		else if (CustomExpression)
		{
			// OutputType declares the node's return type; a secondary output carries its own type in
			// AdditionalOutputs, and UMaterialExpressionCustom::GetOutputValueType reports whichever one
			// this call reads. Sizing the value from the declared OutputType would make every read of a
			// secondary output claim the return type's width.
			const int32 ActualOutputComponents = GetComponentCountForMaterialValueType(ActualOutputValueType);
			if (ActualOutputComponents > 0)
			{
				OutputComponents = ActualOutputComponents;
				bIsTextureObject = false;
				bHasAuthoritativeComponentCount = true;
			}
		}
		else if (Expression->IsA<UMaterialExpressionTextureCoordinate>()
			|| Expression->IsA<UMaterialExpressionPanner>()
			|| IsDreamShaderRotatorExpression(Expression))
		{
			OutputComponents = 2;
			bIsTextureObject = false;
		}
		else
		{
			int32 KnownOutputComponents = 0;
			if (TryResolveKnownExpressionOutputComponentCount(Expression, ResolvedOutputIndex, KnownOutputComponents) && KnownOutputComponents > 0)
			{
				OutputComponents = KnownOutputComponents;
				bIsTextureObject = false;
			}
		}
		if (Expression->IsA<UMaterialExpressionSaturate>())
		{
			ApplyNumericInputComponentCount(FindBoundInputValue(TEXT("Input")));
		}
		else if (Expression->IsA<UMaterialExpressionStaticSwitchParameter>())
		{
			ApplyMaxNumericInputComponentCount(FindBoundInputValue(TEXT("True")), FindBoundInputValue(TEXT("False")));
		}
		else if (Expression->IsA<UMaterialExpressionIf>())
		{
			int32 MaxBranchComponentCount = 0;
			for (const TCHAR* BranchInputName : { TEXT("AGreaterThanB"), TEXT("AEqualsB"), TEXT("ALessThanB") })
			{
				const FCodeValue* BranchValue = FindBoundInputValue(BranchInputName);
				if (BranchValue && !BranchValue->bIsTextureObject && !BranchValue->bIsMaterialAttributes && !BranchValue->bIsSubstrateMaterial)
				{
					MaxBranchComponentCount = FMath::Max(MaxBranchComponentCount, BranchValue->ComponentCount);
				}
			}
			if (MaxBranchComponentCount > 0)
			{
				OutputComponents = MaxBranchComponentCount;
				bIsTextureObject = false;
				bHasAuthoritativeComponentCount = true;
			}
		}
		else if (UMaterialExpressionStaticComponentMaskParameter* StaticComponentMaskExpression = Cast<UMaterialExpressionStaticComponentMaskParameter>(Expression))
		{
			const int32 MaskComponentCount =
				(StaticComponentMaskExpression->DefaultR ? 1 : 0)
				+ (StaticComponentMaskExpression->DefaultG ? 1 : 0)
				+ (StaticComponentMaskExpression->DefaultB ? 1 : 0)
				+ (StaticComponentMaskExpression->DefaultA ? 1 : 0);
			OutputComponents = MaskComponentCount > 0 ? MaskComponentCount : 1;
			bIsTextureObject = false;
		}
		else if (Expression->IsA<UMaterialExpressionCurveAtlasRowParameter>())
		{
			int32 MaskComponentCount = 0;
			if (Expression->Outputs.IsValidIndex(ResolvedOutputIndex))
			{
				const FExpressionOutput& Output = Expression->Outputs[ResolvedOutputIndex];
				MaskComponentCount =
					(Output.MaskR ? 1 : 0)
					+ (Output.MaskG ? 1 : 0)
					+ (Output.MaskB ? 1 : 0)
					+ (Output.MaskA ? 1 : 0);
			}
			OutputComponents = MaskComponentCount > 0 ? MaskComponentCount : 1;
			bIsTextureObject = false;
		}

		OutValue.Expression = Expression;
		OutValue.OutputIndex = ResolvedOutputIndex;
		OutValue.ComponentCount = OutputComponents;
		OutValue.bIsTextureObject = bIsTextureObject;
		OutValue.TextureType = TextureType;
		OutValue.bIsMaterialAttributes = IsMaterialAttributesComponentType(OutputComponents, bIsTextureObject, bIsSubstrateMaterial);
		OutValue.bIsSubstrateMaterial = bIsSubstrateMaterial;
		OutValue.bHasAuthoritativeComponentCount = bHasAuthoritativeComponentCount;
		if (!OutValue.bHasAuthoritativeComponentCount && !bIsTextureObject && OutputComponents > 0)
		{
			int32 KnownOutputComponents = 0;
			if (TryResolveKnownExpressionOutputComponentCount(Expression, ResolvedOutputIndex, KnownOutputComponents)
				&& KnownOutputComponents > 0
				&& KnownOutputComponents == OutputComponents)
			{
				OutValue.bHasAuthoritativeComponentCount = true;
			}
		}
		if (!ExpressionReuseKey.IsEmpty())
		{
			FCodeValue ExpressionNodeValue;
			ExpressionNodeValue.Expression = Expression;
			ExpressionNodeValue.OutputIndex = 0;
			ExpressionNodeValue.ComponentCount = 0;
			AddReusableExpressionValue(ExpressionReuseKey, ExpressionNodeValue);
			AddReusableExpressionValue(OutputReuseKey, OutValue);
		}
		return true;
	}
}
