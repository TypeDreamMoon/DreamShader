// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Graph layout for generated DreamShader materials / material functions: collects expressions,
// builds dependency/consumer maps, assigns owner blocks, inserts cross-block named-reroutes, places
// region/explicit-layout comments, and positions the material root. Extracted byte-for-byte from
// DreamShaderMaterialGeneratorSupport.cpp. The only cross-TU dependency is the now-exposed
// CreateOwnedMaterialExpression (declared in DreamShaderMaterialGeneratorPrivate.h).

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
#include "DreamShaderMaterialGeneratorPrivate.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		constexpr int32 FastLayoutExpressionThreshold = 1200;
	}

	namespace
	{
		static void CollectMaterialExpressions(UMaterial* Material, UMaterialFunction* MaterialFunction, TArray<UMaterialExpression*>& OutExpressions)
		{
			OutExpressions.Reset();
			if (Material)
			{
				OutExpressions.Reserve(Material->GetExpressions().Num());
				for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
				{
					if (Expression)
					{
						OutExpressions.Add(Expression.Get());
					}
				}
				return;
			}

			if (MaterialFunction)
			{
				OutExpressions.Reserve(MaterialFunction->GetExpressions().Num());
				for (const TObjectPtr<UMaterialExpression>& Expression : MaterialFunction->GetExpressions())
				{
					if (Expression)
					{
						OutExpressions.Add(Expression.Get());
					}
				}
			}
		}

		static bool TryAddUniqueExpression(TArray<UMaterialExpression*>& Expressions, UMaterialExpression* Expression)
		{
			if (!Expression || Expressions.Contains(Expression))
			{
				return false;
			}

			Expressions.Add(Expression);
			return true;
		}

		static UMaterialExpression* GetDirectInputExpression(const FExpressionInput& Input)
		{
			if (Input.Expression)
			{
				return Input.Expression;
			}

			const FExpressionInput TracedInput = Input.GetTracedInput();
			return TracedInput.Expression;
		}

		static void SetGeneratedExpressionPosition(UMaterialExpression* Expression, const int32 PositionX, const int32 PositionY)
		{
			if (!Expression)
			{
				return;
			}

			Expression->MaterialExpressionEditorX = PositionX;
			Expression->MaterialExpressionEditorY = PositionY;
			if (Expression->GraphNode)
			{
				Expression->GraphNode->NodePosX = PositionX;
				Expression->GraphNode->NodePosY = PositionY;
			}
		}

		static void BuildExpressionDependencyMaps(
			const TArray<UMaterialExpression*>& Expressions,
			const TSet<UMaterialExpression*>& ExpressionSet,
			TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& OutDependencies,
			TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& OutConsumers)
		{
			OutDependencies.Reset();
			OutConsumers.Reset();

			for (UMaterialExpression* Expression : Expressions)
			{
				if (!Expression)
				{
					continue;
				}

				for (int32 InputIndex = 0; InputIndex < GetDreamShaderExpressionInputCount(Expression); ++InputIndex)
				{
					FExpressionInput* Input = Expression->GetInput(InputIndex);
					if (!Input)
					{
						continue;
					}

					UMaterialExpression* SourceExpression = GetDirectInputExpression(*Input);
					if (!SourceExpression || SourceExpression == Expression || !ExpressionSet.Contains(SourceExpression))
					{
						continue;
					}

					TryAddUniqueExpression(OutDependencies.FindOrAdd(Expression), SourceExpression);
					TryAddUniqueExpression(OutConsumers.FindOrAdd(SourceExpression), Expression);
				}

				if (UMaterialExpressionNamedRerouteUsage* NamedRerouteUsage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression))
				{
					if (UMaterialExpressionNamedRerouteDeclaration* Declaration = NamedRerouteUsage->Declaration)
					{
						if (ExpressionSet.Contains(Declaration))
						{
							TryAddUniqueExpression(OutDependencies.FindOrAdd(Expression), Declaration);
							TryAddUniqueExpression(OutConsumers.FindOrAdd(Declaration), Expression);
						}
					}
				}
			}
		}
	}

	namespace
	{
		struct FLayoutBounds
		{
			int32 MinX = MAX_int32;
			int32 MinY = MAX_int32;
			int32 MaxX = MIN_int32;
			int32 MaxY = MIN_int32;

			bool IsValid() const
			{
				return MinX <= MaxX && MinY <= MaxY;
			}

			void IncludeNode(const int32 PositionX, const int32 PositionY)
			{
				constexpr int32 NodeWidth = 320;
				constexpr int32 NodeHeight = 150;
				MinX = FMath::Min(MinX, PositionX);
				MinY = FMath::Min(MinY, PositionY);
				MaxX = FMath::Max(MaxX, PositionX + NodeWidth);
				MaxY = FMath::Max(MaxY, PositionY + NodeHeight);
			}
		};

		struct FGeneratedLayoutBlock
		{
			FString Title;
			TSet<UMaterialExpression*> ExpressionSet;
			int32 SortKey = 0;
		};

		static void ClearExpressionInputMask(FExpressionInput& Input)
		{
			Input.Mask = 0;
			Input.MaskR = 0;
			Input.MaskG = 0;
			Input.MaskB = 0;
			Input.MaskA = 0;
		}

		static FString MakeLayoutBridgeKey(const UMaterialExpression* SourceExpression, const int32 SourceOutputIndex)
		{
			return FString::Printf(
				TEXT("%llu:%d"),
				static_cast<unsigned long long>(reinterpret_cast<UPTRINT>(SourceExpression)),
				SourceOutputIndex);
		}

		static FString MakeLayoutBridgeUsageKey(
			const UMaterialExpressionNamedRerouteDeclaration* Declaration,
			const int32 ConsumerBlockIndex)
		{
			return FString::Printf(
				TEXT("%llu:%d"),
				static_cast<unsigned long long>(reinterpret_cast<UPTRINT>(Declaration)),
				ConsumerBlockIndex);
		}

		static bool IsDistantLayoutConnection(
			const UMaterialExpression* SourceExpression,
			const UMaterialExpression* ConsumerExpression)
		{
			if (!SourceExpression || !ConsumerExpression)
			{
				return false;
			}

			constexpr int32 MinBridgeDistanceX = 900;
			constexpr int32 MinBridgeDistanceY = 540;
			const int32 DeltaX = FMath::Abs(SourceExpression->MaterialExpressionEditorX - ConsumerExpression->MaterialExpressionEditorX);
			const int32 DeltaY = FMath::Abs(SourceExpression->MaterialExpressionEditorY - ConsumerExpression->MaterialExpressionEditorY);
			return DeltaX >= MinBridgeDistanceX || DeltaY >= MinBridgeDistanceY;
		}

		static void ConnectInputToExpressionPreservingMask(
			FExpressionInput& Input,
			UMaterialExpression* Expression,
			const int32 OutputIndex)
		{
			const int32 SavedMask = Input.Mask;
			const int32 SavedMaskR = Input.MaskR;
			const int32 SavedMaskG = Input.MaskG;
			const int32 SavedMaskB = Input.MaskB;
			const int32 SavedMaskA = Input.MaskA;

			Input.Connect(OutputIndex, Expression);
			Input.Mask = SavedMask;
			Input.MaskR = SavedMaskR;
			Input.MaskG = SavedMaskG;
			Input.MaskB = SavedMaskB;
			Input.MaskA = SavedMaskA;
		}

		static FString GetMaterialPropertyLayoutName(const EMaterialProperty Property)
		{
			switch (Property)
			{
			case MP_BaseColor:
				return TEXT("BaseColor");
			case MP_MaterialAttributes:
				return TEXT("MaterialAttributes");
			case MP_EmissiveColor:
				return TEXT("EmissiveColor");
			case MP_Opacity:
				return TEXT("Opacity");
			case MP_OpacityMask:
				return TEXT("OpacityMask");
			case MP_Metallic:
				return TEXT("Metallic");
			case MP_Specular:
				return TEXT("Specular");
			case MP_Roughness:
				return TEXT("Roughness");
			case MP_Normal:
				return TEXT("Normal");
			case MP_AmbientOcclusion:
				return TEXT("AmbientOcclusion");
			case MP_Refraction:
				return TEXT("Refraction");
			case MP_WorldPositionOffset:
				return TEXT("WorldPositionOffset");
			case MP_PixelDepthOffset:
				return TEXT("PixelDepthOffset");
			case MP_SubsurfaceColor:
				return TEXT("SubsurfaceColor");
			case MP_CustomData0:
				return TEXT("CustomData0");
			case MP_CustomData1:
				return TEXT("CustomData1");
			case MP_DiffuseColor:
				return TEXT("DiffuseColor");
			case MP_SpecularColor:
				return TEXT("SpecularColor");
			case MP_SurfaceThickness:
				return TEXT("SurfaceThickness");
			case MP_Displacement:
				return TEXT("Displacement");
			case MP_CustomizedUVs0:
				return TEXT("CustomizedUV0");
			case MP_CustomizedUVs1:
				return TEXT("CustomizedUV1");
			case MP_CustomizedUVs2:
				return TEXT("CustomizedUV2");
			case MP_CustomizedUVs3:
				return TEXT("CustomizedUV3");
			case MP_CustomizedUVs4:
				return TEXT("CustomizedUV4");
			case MP_CustomizedUVs5:
				return TEXT("CustomizedUV5");
			case MP_CustomizedUVs6:
				return TEXT("CustomizedUV6");
			case MP_CustomizedUVs7:
				return TEXT("CustomizedUV7");
#if DREAMSHADER_WITH_MOON_ENGINE
			// Emitted names, so they use the engine's current spelling. The parser still accepts the
			// older Mooa one, which is what pre-rename sources contain.
			case MP_MoonEncodedAttribute0:
				return TEXT("MoonEncodedAttribute0");
			case MP_MoonEncodedAttribute1:
				return TEXT("MoonEncodedAttribute1");
			case MP_MoonEncodedAttribute2:
				return TEXT("MoonEncodedAttribute2");
			case MP_MoonEncodedAttribute3:
				return TEXT("MoonEncodedAttribute3");
			case MP_MoonEncodedAttribute4:
				return TEXT("MoonEncodedAttribute4");
#endif
			case MP_Anisotropy:
				return TEXT("Anisotropy");
			case MP_Tangent:
				return TEXT("Tangent");
			default:
				return FString::Printf(TEXT("MaterialProperty%d"), static_cast<int32>(Property));
			}
		}

		static void CollectDependencySubgraph(
			UMaterialExpression* Expression,
			const TSet<UMaterialExpression*>& ValidExpressions,
			const TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& Dependencies,
			TSet<UMaterialExpression*>& OutExpressions)
		{
			if (!Expression || !ValidExpressions.Contains(Expression) || OutExpressions.Contains(Expression))
			{
				return;
			}

			OutExpressions.Add(Expression);
			if (const TArray<UMaterialExpression*>* ExpressionDependencies = Dependencies.Find(Expression))
			{
				for (UMaterialExpression* Dependency : *ExpressionDependencies)
				{
					CollectDependencySubgraph(Dependency, ValidExpressions, Dependencies, OutExpressions);
				}
			}
		}

		static void AddLayoutBlock(
			TArray<FGeneratedLayoutBlock>& Blocks,
			TSet<UMaterialExpression*>& OutputSinkExpressions,
			const FString& Title,
			UMaterialExpression* SinkExpression,
			const int32 SortKey,
			const TSet<UMaterialExpression*>& ValidExpressions,
			const TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& Dependencies)
		{
			if (!SinkExpression || !ValidExpressions.Contains(SinkExpression) || OutputSinkExpressions.Contains(SinkExpression))
			{
				return;
			}

			FGeneratedLayoutBlock& Block = Blocks.AddDefaulted_GetRef();
			Block.Title = Title;
			Block.SortKey = SortKey;
			CollectDependencySubgraph(SinkExpression, ValidExpressions, Dependencies, Block.ExpressionSet);
			OutputSinkExpressions.Add(SinkExpression);
		}

		static void AddExpressionToOwnedBlock(
			TArray<FGeneratedLayoutBlock>& Blocks,
			TMap<UMaterialExpression*, int32>& OwnerBlockByExpression,
			UMaterialExpression* Expression,
			const int32 OwnerBlockIndex)
		{
			if (!Expression || !Blocks.IsValidIndex(OwnerBlockIndex))
			{
				return;
			}

			OwnerBlockByExpression.Add(Expression, OwnerBlockIndex);
			Blocks[OwnerBlockIndex].ExpressionSet.Add(Expression);
		}

		static int32 ChooseOwnerBlockByDirectConsumers(
			UMaterialExpression* Expression,
			const TArray<int32>& CandidateBlocks,
			const TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& Consumers,
			const TMap<UMaterialExpression*, int32>& OwnerBlockByExpression)
		{
			if (!Expression || CandidateBlocks.IsEmpty())
			{
				return INDEX_NONE;
			}

			int32 BestBlockIndex = INDEX_NONE;
			int32 BestScore = MIN_int32;
			if (const TArray<UMaterialExpression*>* ExpressionConsumers = Consumers.Find(Expression))
			{
				for (const int32 CandidateBlockIndex : CandidateBlocks)
				{
					int32 Score = 0;
					for (UMaterialExpression* Consumer : *ExpressionConsumers)
					{
						if (const int32* ConsumerBlockIndex = OwnerBlockByExpression.Find(Consumer))
						{
							if (*ConsumerBlockIndex == CandidateBlockIndex)
							{
								++Score;
							}
						}
					}

					if (Score > BestScore)
					{
						BestScore = Score;
						BestBlockIndex = CandidateBlockIndex;
					}
				}
			}

			return BestScore > 0 ? BestBlockIndex : INDEX_NONE;
		}

		static void AssignLayoutBlockOwners(
			const TArray<UMaterialExpression*>& Expressions,
			const TArray<FGeneratedLayoutBlock>& Blocks,
			const TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& Consumers,
			TMap<UMaterialExpression*, int32>& OwnerBlockByExpression)
		{
			OwnerBlockByExpression.Reset();

			TMap<UMaterialExpression*, TArray<int32>> CandidateBlocksByExpression;
			for (int32 BlockIndex = 0; BlockIndex < Blocks.Num(); ++BlockIndex)
			{
				for (UMaterialExpression* Expression : Blocks[BlockIndex].ExpressionSet)
				{
					if (Expression)
					{
						CandidateBlocksByExpression.FindOrAdd(Expression).AddUnique(BlockIndex);
					}
				}
			}

			for (const TPair<UMaterialExpression*, TArray<int32>>& Pair : CandidateBlocksByExpression)
			{
				if (Pair.Value.Num() == 1)
				{
					OwnerBlockByExpression.Add(Pair.Key, Pair.Value[0]);
				}
			}

			bool bChanged = true;
			for (int32 Iteration = 0; bChanged && Iteration < 16; ++Iteration)
			{
				bChanged = false;
				for (UMaterialExpression* Expression : Expressions)
				{
					const TArray<int32>* CandidateBlocks = CandidateBlocksByExpression.Find(Expression);
					if (!CandidateBlocks || CandidateBlocks->IsEmpty() || OwnerBlockByExpression.Contains(Expression))
					{
						continue;
					}

					const int32 OwnerBlockIndex = ChooseOwnerBlockByDirectConsumers(
						Expression,
						*CandidateBlocks,
						Consumers,
						OwnerBlockByExpression);
					if (OwnerBlockIndex != INDEX_NONE)
					{
						OwnerBlockByExpression.Add(Expression, OwnerBlockIndex);
						bChanged = true;
					}
				}
			}

			for (UMaterialExpression* Expression : Expressions)
			{
				if (!Expression || OwnerBlockByExpression.Contains(Expression))
				{
					continue;
				}

				if (const TArray<int32>* CandidateBlocks = CandidateBlocksByExpression.Find(Expression))
				{
					if (!CandidateBlocks->IsEmpty())
					{
						OwnerBlockByExpression.Add(Expression, (*CandidateBlocks)[0]);
					}
				}
			}

			for (UMaterialExpression* Expression : Expressions)
			{
				UMaterialExpressionNamedRerouteDeclaration* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Expression);
				if (!Declaration)
				{
					continue;
				}

				UMaterialExpression* SourceExpression = GetDirectInputExpression(Declaration->Input);
				if (!SourceExpression)
				{
					continue;
				}

				if (const int32* SourceBlockIndex = OwnerBlockByExpression.Find(SourceExpression))
				{
					OwnerBlockByExpression.Add(Declaration, *SourceBlockIndex);
				}
			}
		}

		static UMaterialExpressionNamedRerouteDeclaration* FindOrCreateLayoutBridgeDeclaration(
			UMaterial* Material,
			UMaterialFunction* MaterialFunction,
			UMaterialExpression* SourceExpression,
			const int32 SourceOutputIndex,
			const int32 BridgeIndex,
			TMap<FString, UMaterialExpressionNamedRerouteDeclaration*>& DeclarationsBySourceKey,
			TArray<UMaterialExpression*>& Expressions,
			TSet<UMaterialExpression*>& ExpressionSet,
			TMap<UMaterialExpression*, int32>& OwnerBlockByExpression,
			TArray<FGeneratedLayoutBlock>& Blocks,
			const int32 SourceBlockIndex)
		{
			if (!SourceExpression || !Blocks.IsValidIndex(SourceBlockIndex) || (!Material && !MaterialFunction))
			{
				return nullptr;
			}

			const FString SourceKey = MakeLayoutBridgeKey(SourceExpression, SourceOutputIndex);
			if (UMaterialExpressionNamedRerouteDeclaration* const* ExistingDeclaration = DeclarationsBySourceKey.Find(SourceKey))
			{
				return *ExistingDeclaration;
			}

			auto* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(
				CreateOwnedMaterialExpression(
					Material,
					MaterialFunction,
					UMaterialExpressionNamedRerouteDeclaration::StaticClass(),
					SourceExpression->MaterialExpressionEditorX + 360,
					SourceExpression->MaterialExpressionEditorY));
			if (!Declaration)
			{
				return nullptr;
			}

			Declaration->Name = FName(*FString::Printf(TEXT("DS_Shared_%d"), BridgeIndex));
			if (!Declaration->VariableGuid.IsValid())
			{
				Declaration->VariableGuid = FGuid::NewGuid();
			}
			Declaration->Input.Connect(SourceOutputIndex, SourceExpression);
			ClearExpressionInputMask(Declaration->Input);

			DeclarationsBySourceKey.Add(SourceKey, Declaration);
			Expressions.Add(Declaration);
			ExpressionSet.Add(Declaration);
			AddExpressionToOwnedBlock(Blocks, OwnerBlockByExpression, Declaration, SourceBlockIndex);
			return Declaration;
		}

		static UMaterialExpressionNamedRerouteUsage* FindOrCreateLayoutBridgeUsage(
			UMaterial* Material,
			UMaterialFunction* MaterialFunction,
			UMaterialExpressionNamedRerouteDeclaration* Declaration,
			const int32 ConsumerBlockIndex,
			TMap<FString, UMaterialExpressionNamedRerouteUsage*>& UsagesByDeclarationAndBlock,
			TMap<int32, int32>& UsageSlotByBlock,
			TArray<UMaterialExpression*>& Expressions,
			TSet<UMaterialExpression*>& ExpressionSet,
			TMap<UMaterialExpression*, int32>& OwnerBlockByExpression,
			TArray<FGeneratedLayoutBlock>& Blocks,
			const UMaterialExpression* ConsumerExpression)
		{
			if (!Declaration || !Blocks.IsValidIndex(ConsumerBlockIndex) || (!Material && !MaterialFunction))
			{
				return nullptr;
			}

			const FString UsageKey = MakeLayoutBridgeUsageKey(Declaration, ConsumerBlockIndex);
			if (UMaterialExpressionNamedRerouteUsage* const* ExistingUsage = UsagesByDeclarationAndBlock.Find(UsageKey))
			{
				return *ExistingUsage;
			}

			const int32 SlotIndex = UsageSlotByBlock.FindOrAdd(ConsumerBlockIndex)++;
			const int32 SlotStep = ((SlotIndex + 1) / 2) * 80;
			const int32 SlotOffsetY = SlotIndex == 0
				? 0
				: ((SlotIndex % 2) == 0 ? -SlotStep : SlotStep);
			const int32 UsageX = ConsumerExpression
				? ConsumerExpression->MaterialExpressionEditorX - 360
				: Declaration->MaterialExpressionEditorX;
			const int32 UsageY = ConsumerExpression
				? ConsumerExpression->MaterialExpressionEditorY + SlotOffsetY
				: Declaration->MaterialExpressionEditorY + SlotOffsetY;

			auto* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(
				CreateOwnedMaterialExpression(
					Material,
					MaterialFunction,
					UMaterialExpressionNamedRerouteUsage::StaticClass(),
					UsageX,
					UsageY));
			if (!Usage)
			{
				return nullptr;
			}

			Usage->Declaration = Declaration;
			Usage->DeclarationGuid = Declaration->VariableGuid;

			UsagesByDeclarationAndBlock.Add(UsageKey, Usage);
			Expressions.Add(Usage);
			ExpressionSet.Add(Usage);
			AddExpressionToOwnedBlock(Blocks, OwnerBlockByExpression, Usage, ConsumerBlockIndex);
			return Usage;
		}

		static void InsertCrossBlockReroutes(
			UMaterial* Material,
			UMaterialFunction* MaterialFunction,
			TArray<UMaterialExpression*>& Expressions,
			TSet<UMaterialExpression*>& ExpressionSet,
			TArray<FGeneratedLayoutBlock>& Blocks,
			TMap<UMaterialExpression*, int32>& OwnerBlockByExpression,
			TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& Dependencies,
			TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& Consumers,
			const bool bOnlyDistantConnections)
		{
			TMap<FString, UMaterialExpressionNamedRerouteDeclaration*> DeclarationsBySourceKey;
			TMap<FString, UMaterialExpressionNamedRerouteUsage*> UsagesByDeclarationAndBlock;
			TMap<int32, int32> UsageSlotByBlock;
			int32 BridgeIndex = 0;

			TArray<UMaterialExpression*> ConsumerSnapshot = Expressions;
			for (UMaterialExpression* ConsumerExpression : ConsumerSnapshot)
			{
				if (!ConsumerExpression)
				{
					continue;
				}

				const int32* ConsumerBlockIndex = OwnerBlockByExpression.Find(ConsumerExpression);
				if (!ConsumerBlockIndex)
				{
					continue;
				}

				for (int32 InputIndex = 0; InputIndex < GetDreamShaderExpressionInputCount(ConsumerExpression); ++InputIndex)
				{
					FExpressionInput* Input = ConsumerExpression->GetInput(InputIndex);
					if (!Input || !Input->Expression)
					{
						continue;
					}

					UMaterialExpression* SourceExpression = Input->Expression;
					const int32* SourceBlockIndex = OwnerBlockByExpression.Find(SourceExpression);
					if (!SourceBlockIndex || *SourceBlockIndex == *ConsumerBlockIndex)
					{
						continue;
					}

					if (Cast<UMaterialExpressionNamedRerouteUsage>(SourceExpression))
					{
						continue;
					}

					if (bOnlyDistantConnections && !IsDistantLayoutConnection(SourceExpression, ConsumerExpression))
					{
						continue;
					}

					UMaterialExpressionNamedRerouteDeclaration* Declaration = FindOrCreateLayoutBridgeDeclaration(
						Material,
						MaterialFunction,
						SourceExpression,
						Input->OutputIndex,
						BridgeIndex++,
						DeclarationsBySourceKey,
						Expressions,
						ExpressionSet,
						OwnerBlockByExpression,
						Blocks,
						*SourceBlockIndex);
					UMaterialExpressionNamedRerouteUsage* Usage = FindOrCreateLayoutBridgeUsage(
						Material,
						MaterialFunction,
						Declaration,
						*ConsumerBlockIndex,
						UsagesByDeclarationAndBlock,
						UsageSlotByBlock,
						Expressions,
						ExpressionSet,
						OwnerBlockByExpression,
						Blocks,
						ConsumerExpression);
					if (!Usage)
					{
						continue;
					}

					ConnectInputToExpressionPreservingMask(*Input, Usage, 0);
				}
			}

			if (BridgeIndex > 0)
			{
				BuildExpressionDependencyMaps(Expressions, ExpressionSet, Dependencies, Consumers);
			}
		}

		static void PositionMaterialRootNearOutputs(
			UMaterial* Material,
			const TArray<FLayoutBounds>& BlockBounds)
		{
			if (!Material)
			{
				return;
			}

			FLayoutBounds CombinedBounds;
			for (const FLayoutBounds& Bounds : BlockBounds)
			{
				if (!Bounds.IsValid())
				{
					continue;
				}

				CombinedBounds.MinX = FMath::Min(CombinedBounds.MinX, Bounds.MinX);
				CombinedBounds.MinY = FMath::Min(CombinedBounds.MinY, Bounds.MinY);
				CombinedBounds.MaxX = FMath::Max(CombinedBounds.MaxX, Bounds.MaxX);
				CombinedBounds.MaxY = FMath::Max(CombinedBounds.MaxY, Bounds.MaxY);
			}

			if (!CombinedBounds.IsValid())
			{
				return;
			}

			constexpr int32 RootGapX = 520;
			const int32 RootX = CombinedBounds.MaxX + RootGapX;
			const int32 RootY = (CombinedBounds.MinY + CombinedBounds.MaxY) / 2 - 240;
			Material->EditorX = RootX;
			Material->EditorY = RootY;

			if (Material->MaterialGraph && Material->MaterialGraph->RootNode)
			{
				Material->MaterialGraph->RootNode->NodePosX = RootX;
				Material->MaterialGraph->RootNode->NodePosY = RootY;
			}
		}

		static void PositionMaterialRootNearConnectedOutputs(UMaterial* Material)
		{
			if (!Material)
			{
				return;
			}

			FLayoutBounds OutputBounds;
			for (int32 MaterialPropertyIndex = 0; MaterialPropertyIndex < MP_MAX; ++MaterialPropertyIndex)
			{
				const EMaterialProperty MaterialProperty = static_cast<EMaterialProperty>(MaterialPropertyIndex);
				FExpressionInput* MaterialInput = Material->GetExpressionInputForProperty(MaterialProperty);
				if (!MaterialInput || !MaterialInput->IsConnected())
				{
					continue;
				}

				if (UMaterialExpression* OutputExpression = GetDirectInputExpression(*MaterialInput))
				{
					OutputBounds.IncludeNode(
						OutputExpression->MaterialExpressionEditorX,
						OutputExpression->MaterialExpressionEditorY);
				}
			}

			if (OutputBounds.IsValid())
			{
				TArray<FLayoutBounds> Bounds;
				Bounds.Add(OutputBounds);
				PositionMaterialRootNearOutputs(Material, Bounds);
			}
		}

		static void CreateDreamShaderLayoutComment(
			UMaterial* Material,
			UMaterialFunction* MaterialFunction,
			const FString& Title,
			const FLayoutBounds& Bounds)
		{
			if (!Bounds.IsValid() || (!Material && !MaterialFunction))
			{
				return;
			}

			UObject* Outer = Material ? static_cast<UObject*>(Material) : static_cast<UObject*>(MaterialFunction);
			UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(Outer, NAME_None, RF_Transactional);
			if (!Comment)
			{
				return;
			}

			constexpr int32 PaddingX = 110;
			constexpr int32 PaddingY = 90;
			Comment->Text = FString::Printf(TEXT("DreamShader: %s"), *Title);
			Comment->MaterialExpressionEditorX = Bounds.MinX - PaddingX;
			Comment->MaterialExpressionEditorY = Bounds.MinY - PaddingY;
			Comment->SizeX = FMath::Max(420, Bounds.MaxX - Bounds.MinX + PaddingX * 2);
			Comment->SizeY = FMath::Max(240, Bounds.MaxY - Bounds.MinY + PaddingY * 2);
			Comment->FontSize = 24;
			Comment->CommentColor = FLinearColor(0.10f, 0.16f, 0.22f, 0.35f);
			Comment->bCommentBubbleVisible_InDetailsPanel = true;
			Comment->bColorCommentBubble = true;
			Comment->bGroupMode = true;

			if (Material)
			{
				Material->GetExpressionCollection().AddComment(Comment);
			}
			else
			{
				MaterialFunction->GetExpressionCollection().AddComment(Comment);
			}
		}

		static void CollectOutputBridgeUsages(UMaterial* Material, TArray<UMaterialExpressionNamedRerouteUsage*>& OutUsages)
		{
			if (!Material)
			{
				return;
			}

			for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
			{
				FExpressionInput* MaterialInput = Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(PropertyIndex));
				if (MaterialInput && MaterialInput->Expression)
				{
					if (UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(MaterialInput->Expression))
					{
						OutUsages.AddUnique(Usage);
					}
				}
			}
		}

		// Regroup the named-reroute usages that feed the material output: move them into a column to the
		// right of the graph body, place the material root just past them, and wrap usages + root in one
		// "Material Output" comment box. The usages are removed from their layout blocks beforehand, so
		// the per-property computation boxes stay tight and never overlap this one.
		static void GroupOutputBridgeUsages(UMaterial* Material, UMaterialFunction* MaterialFunction, const TArray<UMaterialExpression*>& Expressions)
		{
			if (!Material)
			{
				return;
			}

			TArray<UMaterialExpressionNamedRerouteUsage*> OutputUsages;
			CollectOutputBridgeUsages(Material, OutputUsages);
			if (OutputUsages.IsEmpty())
			{
				return;
			}

			TSet<UMaterialExpression*> OutputUsageSet;
			for (UMaterialExpressionNamedRerouteUsage* Usage : OutputUsages)
			{
				OutputUsageSet.Add(Usage);
			}

			FLayoutBounds BodyBounds;
			for (UMaterialExpression* Expression : Expressions)
			{
				if (!Expression || OutputUsageSet.Contains(Expression))
				{
					continue;
				}
				BodyBounds.IncludeNode(Expression->MaterialExpressionEditorX, Expression->MaterialExpressionEditorY);
			}
			if (!BodyBounds.IsValid())
			{
				return;
			}

			constexpr int32 GapBodyToUsages = 420;
			constexpr int32 GapUsagesToRoot = 360;
			constexpr int32 UsageSpacingY = 130;
			const int32 UsageX = BodyBounds.MaxX + GapBodyToUsages;
			const int32 CentreY = (BodyBounds.MinY + BodyBounds.MaxY) / 2;
			const int32 ColumnTopY = CentreY - ((OutputUsages.Num() - 1) * UsageSpacingY) / 2;

			FLayoutBounds GroupBounds;
			for (int32 Index = 0; Index < OutputUsages.Num(); ++Index)
			{
				const int32 PositionY = ColumnTopY + Index * UsageSpacingY;
				OutputUsages[Index]->MaterialExpressionEditorX = UsageX;
				OutputUsages[Index]->MaterialExpressionEditorY = PositionY;
				GroupBounds.IncludeNode(UsageX, PositionY);
			}

			const int32 RootX = UsageX + GapUsagesToRoot;
			const int32 RootY = CentreY - 240;
			Material->EditorX = RootX;
			Material->EditorY = RootY;
			if (Material->MaterialGraph && Material->MaterialGraph->RootNode)
			{
				Material->MaterialGraph->RootNode->NodePosX = RootX;
				Material->MaterialGraph->RootNode->NodePosY = RootY;
			}
			GroupBounds.IncludeNode(RootX, RootY);
			GroupBounds.IncludeNode(RootX + 320, RootY + 320);

			CreateDreamShaderLayoutComment(Material, MaterialFunction, TEXT("Material Output"), GroupBounds);
		}

		static void CreateDreamShaderCommentAt(
			UMaterial* Material,
			UMaterialFunction* MaterialFunction,
			const FString& Title,
			const int32 X,
			const int32 Y,
			const int32 W,
			const int32 H,
			const FLinearColor& Color)
		{
			if ((!Material && !MaterialFunction) || Title.TrimStartAndEnd().IsEmpty())
			{
				return;
			}

			UObject* Outer = Material ? static_cast<UObject*>(Material) : static_cast<UObject*>(MaterialFunction);
			UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(Outer, NAME_None, RF_Transactional);
			if (!Comment)
			{
				return;
			}

			Comment->Text = FString::Printf(TEXT("DreamShader: %s"), *Title);
			Comment->MaterialExpressionEditorX = X;
			Comment->MaterialExpressionEditorY = Y;
			Comment->SizeX = FMath::Max(120, W);
			Comment->SizeY = FMath::Max(80, H);
			Comment->FontSize = 24;
			Comment->CommentColor = Color;
			Comment->bCommentBubbleVisible_InDetailsPanel = true;
			Comment->bColorCommentBubble = true;
			Comment->bGroupMode = true;

			if (Material)
			{
				Material->GetExpressionCollection().AddComment(Comment);
			}
			else
			{
				MaterialFunction->GetExpressionCollection().AddComment(Comment);
			}
		}

		static bool ApplyExplicitDreamShaderLayout(
			UMaterial* Material,
			UMaterialFunction* MaterialFunction,
			const FTextShaderLayout* Layout,
			const TMap<FString, UMaterialExpression*>* ExpressionsByVariable,
			TSet<UMaterialExpression*>& OutPositionedExpressions)
		{
			OutPositionedExpressions.Reset();
			if (!Layout || (Layout->Nodes.IsEmpty() && Layout->Comments.IsEmpty()))
			{
				return false;
			}

			bool bAppliedAnyNode = false;
			if (ExpressionsByVariable)
			{
				for (const FTextShaderLayoutNode& Node : Layout->Nodes)
				{
					if (UMaterialExpression* const* Expression = ExpressionsByVariable->Find(Node.Var))
					{
						SetGeneratedExpressionPosition(*Expression, Node.X, Node.Y);
						OutPositionedExpressions.Add(*Expression);
						bAppliedAnyNode = true;
					}
				}
			}

			for (const FTextShaderLayoutComment& Comment : Layout->Comments)
			{
				CreateDreamShaderCommentAt(
					Material,
					MaterialFunction,
					Comment.Name,
					Comment.X,
					Comment.Y,
					Comment.W,
					Comment.H,
					Comment.Color);
			}

			return bAppliedAnyNode || !Layout->Comments.IsEmpty();
		}

		static void PositionUnmatchedExplicitLayoutExpressions(
			const TArray<UMaterialExpression*>& Expressions,
			const TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& Dependencies,
			const TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& Consumers,
			TSet<UMaterialExpression*>& InOutPositionedExpressions)
		{
			TSet<UMaterialExpression*> PendingExpressions;
			for (UMaterialExpression* Expression : Expressions)
			{
				if (Expression && !InOutPositionedExpressions.Contains(Expression))
				{
					PendingExpressions.Add(Expression);
				}
			}

			if (PendingExpressions.IsEmpty())
			{
				return;
			}

			TMap<FString, int32> SlotUseCount;
			auto BuildSlotKey = [](const int32 X, const int32 Y)
			{
				return FString::Printf(TEXT("%d:%d"), X / 80, Y / 80);
			};
			auto FanOutY = [&SlotUseCount, &BuildSlotKey](const int32 X, const int32 Y)
			{
				const FString SlotKey = BuildSlotKey(X, Y);
				const int32 SlotIndex = SlotUseCount.FindOrAdd(SlotKey)++;
				if (SlotIndex == 0)
				{
					return Y;
				}

				const int32 Step = ((SlotIndex + 1) / 2) * 120;
				return Y + ((SlotIndex % 2) == 0 ? -Step : Step);
			};
			auto AveragePosition = [&InOutPositionedExpressions](
				const TArray<UMaterialExpression*>* Neighbors,
				int32& OutX,
				int32& OutY)
			{
				if (!Neighbors || Neighbors->IsEmpty())
				{
					return false;
				}

				int64 SumX = 0;
				int64 SumY = 0;
				int32 Count = 0;
				for (UMaterialExpression* Neighbor : *Neighbors)
				{
					if (!Neighbor || !InOutPositionedExpressions.Contains(Neighbor))
					{
						continue;
					}

					SumX += Neighbor->MaterialExpressionEditorX;
					SumY += Neighbor->MaterialExpressionEditorY;
					++Count;
				}

				if (Count <= 0)
				{
					return false;
				}

				OutX = static_cast<int32>(SumX / Count);
				OutY = static_cast<int32>(SumY / Count);
				return true;
			};

			bool bChanged = true;
			const int32 MaxPropagationPasses = FMath::Max(32, PendingExpressions.Num());
			for (int32 PassIndex = 0; bChanged && PassIndex < MaxPropagationPasses; ++PassIndex)
			{
				bChanged = false;
				TArray<UMaterialExpression*> PendingSnapshot = PendingExpressions.Array();
				for (UMaterialExpression* Expression : PendingSnapshot)
				{
					if (!Expression)
					{
						PendingExpressions.Remove(Expression);
						continue;
					}

					int32 DependencyX = 0;
					int32 DependencyY = 0;
					const bool bHasDependencyAnchor = AveragePosition(Dependencies.Find(Expression), DependencyX, DependencyY);
					int32 ConsumerX = 0;
					int32 ConsumerY = 0;
					const bool bHasConsumerAnchor = AveragePosition(Consumers.Find(Expression), ConsumerX, ConsumerY);
					if (!bHasDependencyAnchor && !bHasConsumerAnchor)
					{
						continue;
					}

					int32 PositionX = Expression->MaterialExpressionEditorX;
					int32 PositionY = Expression->MaterialExpressionEditorY;
					if (bHasDependencyAnchor && bHasConsumerAnchor)
					{
						PositionX = (DependencyX + ConsumerX) / 2;
						PositionY = (DependencyY + ConsumerY) / 2;
					}
					else if (bHasConsumerAnchor)
					{
						PositionX = ConsumerX - 360;
						PositionY = ConsumerY;
					}
					else
					{
						PositionX = DependencyX + 360;
						PositionY = DependencyY;
					}

					SetGeneratedExpressionPosition(Expression, PositionX, FanOutY(PositionX, PositionY));
					InOutPositionedExpressions.Add(Expression);
					PendingExpressions.Remove(Expression);
					bChanged = true;
				}
			}

			if (PendingExpressions.IsEmpty() || InOutPositionedExpressions.IsEmpty())
			{
				return;
			}

			FLayoutBounds PositionedBounds;
			for (UMaterialExpression* Expression : InOutPositionedExpressions)
			{
				if (Expression)
				{
					PositionedBounds.IncludeNode(Expression->MaterialExpressionEditorX, Expression->MaterialExpressionEditorY);
				}
			}

			const int32 FallbackX = PositionedBounds.IsValid() ? PositionedBounds.MinX - 480 : -1200;
			int32 FallbackY = PositionedBounds.IsValid() ? PositionedBounds.MaxY + 240 : -620;
			for (UMaterialExpression* Expression : PendingExpressions)
			{
				if (!Expression)
				{
					continue;
				}

				SetGeneratedExpressionPosition(Expression, FallbackX, FallbackY);
				FallbackY += 180;
				InOutPositionedExpressions.Add(Expression);
			}
		}

		static bool IsExpressionInsideExplicitLayoutComment(
			const UMaterialExpression* Expression,
			const FTextShaderLayoutComment& Comment)
		{
			if (!Expression)
			{
				return false;
			}

			const int32 X = Expression->MaterialExpressionEditorX;
			const int32 Y = Expression->MaterialExpressionEditorY;
			return X >= Comment.X
				&& Y >= Comment.Y
				&& X <= Comment.X + Comment.W
				&& Y <= Comment.Y + Comment.H;
		}

		static int32 FindOrAddExplicitLayoutBlock(
			TArray<FGeneratedLayoutBlock>& Blocks,
			TMap<int32, int32>& BlockIndexByCommentIndex,
			const FTextShaderLayoutComment& Comment,
			const int32 CommentIndex)
		{
			if (const int32* ExistingBlockIndex = BlockIndexByCommentIndex.Find(CommentIndex))
			{
				return *ExistingBlockIndex;
			}

			FGeneratedLayoutBlock& Block = Blocks.AddDefaulted_GetRef();
			Block.Title = Comment.Name;
			Block.SortKey = CommentIndex;
			const int32 BlockIndex = Blocks.Num() - 1;
			BlockIndexByCommentIndex.Add(CommentIndex, BlockIndex);
			return BlockIndex;
		}

		static int32 FindOrAddNamedExplicitLayoutBlock(
			TArray<FGeneratedLayoutBlock>& Blocks,
			TMap<FString, int32>& BlockIndexByName,
			const FString& Title,
			const int32 SortKey)
		{
			if (const int32* ExistingBlockIndex = BlockIndexByName.Find(Title))
			{
				return *ExistingBlockIndex;
			}

			FGeneratedLayoutBlock& Block = Blocks.AddDefaulted_GetRef();
			Block.Title = Title;
			Block.SortKey = SortKey;
			const int32 BlockIndex = Blocks.Num() - 1;
			BlockIndexByName.Add(Title, BlockIndex);
			return BlockIndex;
		}

		static void BuildExplicitLayoutOwnershipBlocks(
			const TArray<UMaterialExpression*>& Expressions,
			const FTextShaderLayout* Layout,
			const TMap<FString, UMaterialExpression*>* ExpressionsByVariable,
			const TMap<FString, FString>* RegionByVariable,
			TArray<FGeneratedLayoutBlock>& OutBlocks,
			TMap<UMaterialExpression*, int32>& OutOwnerBlockByExpression)
		{
			OutBlocks.Reset();
			OutOwnerBlockByExpression.Reset();

			if (Layout)
			{
				TMap<int32, int32> BlockIndexByCommentIndex;
				for (UMaterialExpression* Expression : Expressions)
				{
					if (!Expression)
					{
						continue;
					}

					int32 BestCommentIndex = INDEX_NONE;
					int32 BestCommentArea = MAX_int32;
					for (int32 CommentIndex = 0; CommentIndex < Layout->Comments.Num(); ++CommentIndex)
					{
						const FTextShaderLayoutComment& Comment = Layout->Comments[CommentIndex];
						if (!IsExpressionInsideExplicitLayoutComment(Expression, Comment))
						{
							continue;
						}

						const int32 Area = FMath::Max(1, Comment.W) * FMath::Max(1, Comment.H);
						if (BestCommentIndex == INDEX_NONE || Area < BestCommentArea)
						{
							BestCommentIndex = CommentIndex;
							BestCommentArea = Area;
						}
					}

					if (BestCommentIndex == INDEX_NONE)
					{
						continue;
					}

					const int32 BlockIndex = FindOrAddExplicitLayoutBlock(
						OutBlocks,
						BlockIndexByCommentIndex,
						Layout->Comments[BestCommentIndex],
						BestCommentIndex);
					AddExpressionToOwnedBlock(OutBlocks, OutOwnerBlockByExpression, Expression, BlockIndex);
				}
			}

			if (!ExpressionsByVariable || !RegionByVariable || RegionByVariable->IsEmpty())
			{
				return;
			}

			TSet<UMaterialExpression*> ExpressionSet;
			for (UMaterialExpression* Expression : Expressions)
			{
				if (Expression)
				{
					ExpressionSet.Add(Expression);
				}
			}

			TMap<FString, int32> BlockIndexByRegion;
			for (const TPair<FString, FString>& Pair : *RegionByVariable)
			{
				UMaterialExpression* const* Expression = ExpressionsByVariable->Find(Pair.Key);
				if (!Expression || !*Expression || !ExpressionSet.Contains(*Expression) || OutOwnerBlockByExpression.Contains(*Expression))
				{
					continue;
				}

				const int32 BlockIndex = FindOrAddNamedExplicitLayoutBlock(
					OutBlocks,
					BlockIndexByRegion,
					Pair.Value,
					100000 + BlockIndexByRegion.Num());
				AddExpressionToOwnedBlock(OutBlocks, OutOwnerBlockByExpression, *Expression, BlockIndex);
			}
		}

		static void InsertExplicitLayoutReroutes(
			UMaterial* Material,
			UMaterialFunction* MaterialFunction,
			const FTextShaderLayout* Layout,
			const TMap<FString, UMaterialExpression*>* ExpressionsByVariable,
			const TMap<FString, FString>* RegionByVariable,
			TArray<UMaterialExpression*>& Expressions,
			TSet<UMaterialExpression*>& ExpressionSet,
			TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& Dependencies,
			TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& Consumers)
		{
			TArray<FGeneratedLayoutBlock> Blocks;
			TMap<UMaterialExpression*, int32> OwnerBlockByExpression;
			BuildExplicitLayoutOwnershipBlocks(
				Expressions,
				Layout,
				ExpressionsByVariable,
				RegionByVariable,
				Blocks,
				OwnerBlockByExpression);
			if (Blocks.Num() < 2 || OwnerBlockByExpression.Num() < 2)
			{
				return;
			}

			InsertCrossBlockReroutes(
				Material,
				MaterialFunction,
				Expressions,
				ExpressionSet,
				Blocks,
				OwnerBlockByExpression,
				Dependencies,
				Consumers,
				true);
		}

		static void AddRegionLayoutBlocks(
			const TArray<UMaterialExpression*>& Expressions,
			const TMap<FString, UMaterialExpression*>* ExpressionsByVariable,
			const TMap<FString, FString>* RegionByVariable,
			TArray<FGeneratedLayoutBlock>& InOutBlocks,
			TSet<UMaterialExpression*>& InOutOutputSinkExpressions)
		{
			if (!ExpressionsByVariable || !RegionByVariable || RegionByVariable->IsEmpty())
			{
				return;
			}

			TSet<UMaterialExpression*> ExpressionSet;
			for (UMaterialExpression* Expression : Expressions)
			{
				if (Expression)
				{
					ExpressionSet.Add(Expression);
				}
			}

			TMap<FString, int32> BlockIndexByRegion;
			for (const TPair<FString, FString>& Pair : *RegionByVariable)
			{
				UMaterialExpression* const* Expression = ExpressionsByVariable->Find(Pair.Key);
				if (!Expression || !*Expression || !ExpressionSet.Contains(*Expression))
				{
					continue;
				}

				int32 BlockIndex = INDEX_NONE;
				if (const int32* ExistingIndex = BlockIndexByRegion.Find(Pair.Value))
				{
					BlockIndex = *ExistingIndex;
				}
				else
				{
					FGeneratedLayoutBlock& Block = InOutBlocks.AddDefaulted_GetRef();
					Block.Title = Pair.Value;
					Block.SortKey = InOutBlocks.Num();
					BlockIndex = InOutBlocks.Num() - 1;
					BlockIndexByRegion.Add(Pair.Value, BlockIndex);
				}

				InOutBlocks[BlockIndex].ExpressionSet.Add(*Expression);
				InOutOutputSinkExpressions.Add(*Expression);
			}
		}

		static FLayoutBounds LayoutExpressionBlock(
			const TArray<UMaterialExpression*>& BlockExpressions,
			const TMap<UMaterialExpression*, TArray<UMaterialExpression*>>& GlobalDependencies,
			const TMap<UMaterialExpression*, int32>& OriginalOrder,
			const int32 BlockTopY)
		{
			FLayoutBounds Bounds;
			if (BlockExpressions.IsEmpty())
			{
				return Bounds;
			}

			TSet<UMaterialExpression*> BlockSet;
			BlockSet.Reserve(BlockExpressions.Num());
			for (UMaterialExpression* Expression : BlockExpressions)
			{
				if (Expression)
				{
					BlockSet.Add(Expression);
				}
			}

			TMap<UMaterialExpression*, TArray<UMaterialExpression*>> Dependencies;
			TMap<UMaterialExpression*, TArray<UMaterialExpression*>> Consumers;
			for (UMaterialExpression* Expression : BlockExpressions)
			{
				if (const TArray<UMaterialExpression*>* ExpressionDependencies = GlobalDependencies.Find(Expression))
				{
					for (UMaterialExpression* Dependency : *ExpressionDependencies)
					{
						if (BlockSet.Contains(Dependency))
						{
							TryAddUniqueExpression(Dependencies.FindOrAdd(Expression), Dependency);
							TryAddUniqueExpression(Consumers.FindOrAdd(Dependency), Expression);
						}
					}
				}
			}

			TMap<UMaterialExpression*, int32> RankByExpression;
			TSet<UMaterialExpression*> Resolving;
			TFunction<int32(UMaterialExpression*)> ResolveRank;
			ResolveRank = [&](UMaterialExpression* Expression) -> int32
			{
				if (!Expression)
				{
					return 0;
				}

				if (const int32* ExistingRank = RankByExpression.Find(Expression))
				{
					return *ExistingRank;
				}

				if (Resolving.Contains(Expression))
				{
					return 0;
				}

				Resolving.Add(Expression);
				int32 Rank = 0;
				if (const TArray<UMaterialExpression*>* ExpressionConsumers = Consumers.Find(Expression))
				{
					for (UMaterialExpression* Consumer : *ExpressionConsumers)
					{
						Rank = FMath::Max(Rank, ResolveRank(Consumer) + 1);
					}
				}
				Resolving.Remove(Expression);

				RankByExpression.Add(Expression, Rank);
				return Rank;
			};

			int32 MaxRank = 0;
			for (UMaterialExpression* Expression : BlockExpressions)
			{
				MaxRank = FMath::Max(MaxRank, ResolveRank(Expression));
			}

			TMap<int32, TArray<UMaterialExpression*>> Layers;
			for (UMaterialExpression* Expression : BlockExpressions)
			{
				Layers.FindOrAdd(RankByExpression.FindRef(Expression)).Add(Expression);
			}

			for (TPair<int32, TArray<UMaterialExpression*>>& LayerPair : Layers)
			{
				LayerPair.Value.StableSort([&OriginalOrder](UMaterialExpression& Left, UMaterialExpression& Right)
				{
					if (Left.MaterialExpressionEditorY != Right.MaterialExpressionEditorY)
					{
						return Left.MaterialExpressionEditorY < Right.MaterialExpressionEditorY;
					}
					return OriginalOrder.FindRef(&Left) < OriginalOrder.FindRef(&Right);
				});
			}

			TMap<UMaterialExpression*, int32> OrderInLayer;
			auto RefreshOrder = [&]()
			{
				OrderInLayer.Reset();
				for (const TPair<int32, TArray<UMaterialExpression*>>& LayerPair : Layers)
				{
					const TArray<UMaterialExpression*>& Layer = LayerPair.Value;
					for (int32 Index = 0; Index < Layer.Num(); ++Index)
					{
						OrderInLayer.Add(Layer[Index], Index);
					}
				}
			};

			auto AverageNeighborOrder = [&OrderInLayer, &OriginalOrder](
				UMaterialExpression* Expression,
				const TArray<UMaterialExpression*>* Neighbors) -> float
			{
				if (!Expression || !Neighbors || Neighbors->IsEmpty())
				{
					return static_cast<float>(OriginalOrder.FindRef(Expression));
				}

				float Sum = 0.0f;
				int32 Count = 0;
				for (UMaterialExpression* Neighbor : *Neighbors)
				{
					if (const int32* NeighborOrder = OrderInLayer.Find(Neighbor))
					{
						Sum += static_cast<float>(*NeighborOrder);
						++Count;
					}
				}

				return Count > 0
					? Sum / static_cast<float>(Count)
					: static_cast<float>(OriginalOrder.FindRef(Expression));
			};

			RefreshOrder();
			for (int32 Iteration = 0; Iteration < 4; ++Iteration)
			{
				for (int32 Rank = MaxRank - 1; Rank >= 0; --Rank)
				{
					if (TArray<UMaterialExpression*>* Layer = Layers.Find(Rank))
					{
						Layer->StableSort([&](UMaterialExpression& Left, UMaterialExpression& Right)
						{
							const float LeftOrder = AverageNeighborOrder(&Left, Consumers.Find(&Left));
							const float RightOrder = AverageNeighborOrder(&Right, Consumers.Find(&Right));
							return LeftOrder == RightOrder
								? OriginalOrder.FindRef(&Left) < OriginalOrder.FindRef(&Right)
								: LeftOrder < RightOrder;
						});
					}
				}
				RefreshOrder();

				for (int32 Rank = 1; Rank <= MaxRank; ++Rank)
				{
					if (TArray<UMaterialExpression*>* Layer = Layers.Find(Rank))
					{
						Layer->StableSort([&](UMaterialExpression& Left, UMaterialExpression& Right)
						{
							const float LeftOrder = AverageNeighborOrder(&Left, Dependencies.Find(&Left));
							const float RightOrder = AverageNeighborOrder(&Right, Dependencies.Find(&Right));
							return LeftOrder == RightOrder
								? OriginalOrder.FindRef(&Left) < OriginalOrder.FindRef(&Right)
								: LeftOrder < RightOrder;
						});
					}
				}
				RefreshOrder();
			}

			int32 MaxLayerSize = 1;
			for (const TPair<int32, TArray<UMaterialExpression*>>& LayerPair : Layers)
			{
				MaxLayerSize = FMath::Max(MaxLayerSize, LayerPair.Value.Num());
			}

			constexpr int32 OutputX = 900;
			constexpr int32 ColumnSpacing = 420;
			constexpr int32 RowSpacing = 220;
			const int32 CenterY = BlockTopY + ((MaxLayerSize - 1) * RowSpacing) / 2;
			for (int32 Rank = 0; Rank <= MaxRank; ++Rank)
			{
				TArray<UMaterialExpression*>* Layer = Layers.Find(Rank);
				if (!Layer || Layer->IsEmpty())
				{
					continue;
				}

				const int32 PositionX = OutputX - Rank * ColumnSpacing;
				const int32 StartY = CenterY - ((Layer->Num() - 1) * RowSpacing) / 2;
				for (int32 Index = 0; Index < Layer->Num(); ++Index)
				{
					const int32 PositionY = StartY + Index * RowSpacing;
					SetGeneratedExpressionPosition((*Layer)[Index], PositionX, PositionY);
					Bounds.IncludeNode(PositionX, PositionY);
				}
			}

			return Bounds;
		}
	}

	void LayoutGeneratedExpressions(UMaterial* Material, UMaterialFunction* MaterialFunction)
	{
		LayoutGeneratedExpressions(Material, MaterialFunction, nullptr, nullptr, nullptr);
	}

	void LayoutGeneratedExpressions(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FTextShaderLayout* Layout,
		const TMap<FString, UMaterialExpression*>* ExpressionsByVariable,
		const TMap<FString, FString>* RegionByVariable)
	{
		TArray<UMaterialExpression*> Expressions;
		CollectMaterialExpressions(Material, MaterialFunction, Expressions);
		TSet<UMaterialExpression*> ExplicitlyPositionedExpressions;
		if (ApplyExplicitDreamShaderLayout(Material, MaterialFunction, Layout, ExpressionsByVariable, ExplicitlyPositionedExpressions))
		{
			TSet<UMaterialExpression*> ExpressionSet;
			ExpressionSet.Reserve(Expressions.Num());
			for (UMaterialExpression* Expression : Expressions)
			{
				if (Expression)
				{
					ExpressionSet.Add(Expression);
				}
			}

			TMap<UMaterialExpression*, TArray<UMaterialExpression*>> Dependencies;
			TMap<UMaterialExpression*, TArray<UMaterialExpression*>> Consumers;
			BuildExpressionDependencyMaps(Expressions, ExpressionSet, Dependencies, Consumers);

			if (!ExplicitlyPositionedExpressions.IsEmpty() && ExplicitlyPositionedExpressions.Num() < Expressions.Num())
			{
				PositionUnmatchedExplicitLayoutExpressions(Expressions, Dependencies, Consumers, ExplicitlyPositionedExpressions);
			}

			InsertExplicitLayoutReroutes(
				Material,
				MaterialFunction,
				Layout,
				ExpressionsByVariable,
				RegionByVariable,
				Expressions,
				ExpressionSet,
				Dependencies,
				Consumers);
			PositionMaterialRootNearConnectedOutputs(Material);
			GroupOutputBridgeUsages(Material, MaterialFunction, Expressions);
			return;
		}

		if (Expressions.Num() < 2)
		{
			return;
		}

		if (Expressions.Num() >= FastLayoutExpressionThreshold)
		{
			UE_LOG(
				LogDreamShader,
				Display,
				TEXT("Skipping automatic layout for large DreamShader graph (%d nodes). Existing generated positions will be used."),
				Expressions.Num());
			return;
		}

		FScopedSlowTask LayoutSlowTask(
			FMath::Max(1.0f, static_cast<float>(Expressions.Num())),
			FText::FromString(TEXT("Laying out DreamShader material graph...")));

		TSet<UMaterialExpression*> ExpressionSet;
		TMap<UMaterialExpression*, int32> OriginalOrder;
		ExpressionSet.Reserve(Expressions.Num());
		OriginalOrder.Reserve(Expressions.Num());
		for (int32 Index = 0; Index < Expressions.Num(); ++Index)
		{
			ExpressionSet.Add(Expressions[Index]);
			OriginalOrder.Add(Expressions[Index], Index);
		}

		TMap<UMaterialExpression*, TArray<UMaterialExpression*>> Dependencies;
		TMap<UMaterialExpression*, TArray<UMaterialExpression*>> Consumers;
		BuildExpressionDependencyMaps(Expressions, ExpressionSet, Dependencies, Consumers);

		TArray<FGeneratedLayoutBlock> Blocks;
		TSet<UMaterialExpression*> OutputSinkExpressions;
		AddRegionLayoutBlocks(Expressions, ExpressionsByVariable, RegionByVariable, Blocks, OutputSinkExpressions);
		if (Material)
		{
			for (int32 MaterialPropertyIndex = 0; MaterialPropertyIndex < MP_MAX; ++MaterialPropertyIndex)
			{
				const EMaterialProperty MaterialProperty = static_cast<EMaterialProperty>(MaterialPropertyIndex);
				FExpressionInput* MaterialInput = Material->GetExpressionInputForProperty(MaterialProperty);
				if (!MaterialInput || !MaterialInput->IsConnected())
				{
					continue;
				}

				AddLayoutBlock(
					Blocks,
					OutputSinkExpressions,
					FString::Printf(TEXT("Output: %s"), *GetMaterialPropertyLayoutName(MaterialProperty)),
					GetDirectInputExpression(*MaterialInput),
					MaterialPropertyIndex,
					ExpressionSet,
					Dependencies);
			}
		}
		else if (MaterialFunction)
		{
			for (UMaterialExpression* Expression : Expressions)
			{
				UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Expression);
				if (!FunctionOutput)
				{
					continue;
				}

				const FString OutputName = FunctionOutput->OutputName.IsNone()
					? TEXT("FunctionOutput")
					: FunctionOutput->OutputName.ToString();
				AddLayoutBlock(
					Blocks,
					OutputSinkExpressions,
					FString::Printf(TEXT("Output: %s"), *OutputName),
					FunctionOutput,
					1000 + OriginalOrder.FindRef(FunctionOutput),
					ExpressionSet,
					Dependencies);
			}
		}

		FGeneratedLayoutBlock LooseOutputBlock;
		LooseOutputBlock.Title = TEXT("Generated Outputs");
		LooseOutputBlock.SortKey = 100000;
		for (UMaterialExpression* Expression : Expressions)
		{
			const TArray<UMaterialExpression*>* ExpressionConsumers = Consumers.Find(Expression);
			if (!OutputSinkExpressions.Contains(Expression)
				&& (!ExpressionConsumers || ExpressionConsumers->IsEmpty()))
			{
				CollectDependencySubgraph(Expression, ExpressionSet, Dependencies, LooseOutputBlock.ExpressionSet);
				OutputSinkExpressions.Add(Expression);
			}
		}
		if (!LooseOutputBlock.ExpressionSet.IsEmpty())
		{
			Blocks.Add(MoveTemp(LooseOutputBlock));
		}

		if (Blocks.IsEmpty())
		{
			FGeneratedLayoutBlock& Block = Blocks.AddDefaulted_GetRef();
			Block.Title = TEXT("Graph");
			Block.SortKey = 0;
			for (UMaterialExpression* Expression : Expressions)
			{
				Block.ExpressionSet.Add(Expression);
			}
		}

		TSet<UMaterialExpression*> SinkExpressions = OutputSinkExpressions;
		TMap<UMaterialExpression*, int32> BlockUseCount;
		for (const FGeneratedLayoutBlock& Block : Blocks)
		{
			for (UMaterialExpression* Expression : Block.ExpressionSet)
			{
				if (Expression && !SinkExpressions.Contains(Expression))
				{
					BlockUseCount.FindOrAdd(Expression)++;
				}
			}
		}

		TSet<UMaterialExpression*> SharedExpressions;
		for (const TPair<UMaterialExpression*, int32>& Pair : BlockUseCount)
		{
			if (Pair.Value > 1)
			{
				SharedExpressions.Add(Pair.Key);
			}
		}

		bool bAddedSharedReroute = true;
		while (bAddedSharedReroute)
		{
			bAddedSharedReroute = false;
			for (UMaterialExpression* Expression : Expressions)
			{
				UMaterialExpressionNamedRerouteDeclaration* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Expression);
				if (!Declaration || SharedExpressions.Contains(Declaration))
				{
					continue;
				}

				if (UMaterialExpression* SourceExpression = GetDirectInputExpression(Declaration->Input))
				{
					if (SharedExpressions.Contains(SourceExpression))
					{
						SharedExpressions.Add(Declaration);
						bAddedSharedReroute = true;
					}
				}
			}
		}

		Blocks.StableSort([](const FGeneratedLayoutBlock& Left, const FGeneratedLayoutBlock& Right)
		{
			return Left.SortKey < Right.SortKey;
		});

		TArray<FGeneratedLayoutBlock> LayoutBlocks;
		LayoutBlocks.Reserve(Blocks.Num() + 1);
		TMap<UMaterialExpression*, int32> OwnerBlockByExpression;
		AssignLayoutBlockOwners(Expressions, Blocks, Consumers, OwnerBlockByExpression);

		for (int32 BlockIndex = 0; BlockIndex < Blocks.Num(); ++BlockIndex)
		{
			const FGeneratedLayoutBlock& Block = Blocks[BlockIndex];
			FGeneratedLayoutBlock& LayoutBlock = LayoutBlocks.AddDefaulted_GetRef();
			LayoutBlock.Title = Block.Title;
			LayoutBlock.SortKey = Block.SortKey;
			for (UMaterialExpression* Expression : Block.ExpressionSet)
			{
				if (!Expression)
				{
					continue;
				}

				if (OwnerBlockByExpression.FindRef(Expression) != BlockIndex)
				{
					continue;
				}

				LayoutBlock.ExpressionSet.Add(Expression);
			}
		}

		TSet<UMaterialExpression*> AssignedExpressions;
		for (const FGeneratedLayoutBlock& LayoutBlock : LayoutBlocks)
		{
			for (UMaterialExpression* Expression : LayoutBlock.ExpressionSet)
			{
				AssignedExpressions.Add(Expression);
			}
		}

		FGeneratedLayoutBlock LooseSharedBlock;
		LooseSharedBlock.Title = TEXT("Shared Inputs");
		LooseSharedBlock.SortKey = -1000;
		for (UMaterialExpression* Expression : Expressions)
		{
			if (Expression && SharedExpressions.Contains(Expression) && !AssignedExpressions.Contains(Expression))
			{
				LooseSharedBlock.ExpressionSet.Add(Expression);
			}
		}
		if (!LooseSharedBlock.ExpressionSet.IsEmpty())
		{
			LayoutBlocks.Insert(MoveTemp(LooseSharedBlock), 0);
			for (UMaterialExpression* Expression : LayoutBlocks[0].ExpressionSet)
			{
				OwnerBlockByExpression.Add(Expression, 0);
			}
			for (TPair<UMaterialExpression*, int32>& Pair : OwnerBlockByExpression)
			{
				if (!LayoutBlocks[0].ExpressionSet.Contains(Pair.Key))
				{
					++Pair.Value;
				}
			}
		}

		InsertCrossBlockReroutes(
			Material,
			MaterialFunction,
			Expressions,
			ExpressionSet,
			LayoutBlocks,
			OwnerBlockByExpression,
			Dependencies,
			Consumers,
			false);

		// Output-bridge usages are regrouped beside the root by GroupOutputBridgeUsages; drop them from
		// their layout blocks now so the per-block comment boxes stay tight around the computation.
		if (Material)
		{
			TArray<UMaterialExpressionNamedRerouteUsage*> RegroupedOutputUsages;
			CollectOutputBridgeUsages(Material, RegroupedOutputUsages);
			for (UMaterialExpressionNamedRerouteUsage* Usage : RegroupedOutputUsages)
			{
				for (FGeneratedLayoutBlock& Block : LayoutBlocks)
				{
					Block.ExpressionSet.Remove(Usage);
				}
				OwnerBlockByExpression.Remove(Usage);
			}
		}

		OriginalOrder.Reserve(Expressions.Num());
		for (int32 Index = 0; Index < Expressions.Num(); ++Index)
		{
			if (Expressions[Index] && !OriginalOrder.Contains(Expressions[Index]))
			{
				OriginalOrder.Add(Expressions[Index], Index);
			}
		}

		int32 PositionedCount = 0;
		int32 NextBlockTopY = -620;
		constexpr int32 BlockSpacing = 420;
		TArray<FLayoutBounds> BlockBounds;
		BlockBounds.Reserve(LayoutBlocks.Num());

		// The positioning loop below calls EnterProgressFrame(1.0f) once per node in every
		// LayoutBlock. Inserted cross-block reroute nodes mean that count can exceed the
		// original Expressions.Num() the slow task was constructed with, so reconcile the
		// total here to the actual node count to avoid overrunning the slow task budget.
		int32 NodesToPosition = 0;
		for (const FGeneratedLayoutBlock& Block : LayoutBlocks)
		{
			NodesToPosition += Block.ExpressionSet.Num();
		}
		LayoutSlowTask.TotalAmountOfWork = FMath::Max(1.0f, static_cast<float>(NodesToPosition));
		LayoutSlowTask.CompletedWork = 0.0f;

		for (const FGeneratedLayoutBlock& Block : LayoutBlocks)
		{
			TArray<UMaterialExpression*> BlockExpressions;
			BlockExpressions.Reserve(Block.ExpressionSet.Num());
			for (UMaterialExpression* Expression : Block.ExpressionSet)
			{
				BlockExpressions.Add(Expression);
			}

			BlockExpressions.StableSort([&OriginalOrder](UMaterialExpression& Left, UMaterialExpression& Right)
			{
				return OriginalOrder.FindRef(&Left) < OriginalOrder.FindRef(&Right);
			});

			for (int32 Index = 0; Index < BlockExpressions.Num(); ++Index)
			{
				(void)Index;
				LayoutSlowTask.EnterProgressFrame(1.0f, FText::FromString(FString::Printf(
					TEXT("Positioning node %d of %d..."),
					++PositionedCount,
					Expressions.Num())));
			}

			FLayoutBounds Bounds = LayoutExpressionBlock(BlockExpressions, Dependencies, OriginalOrder, NextBlockTopY);
			CreateDreamShaderLayoutComment(Material, MaterialFunction, Block.Title, Bounds);
			if (Bounds.IsValid())
			{
				BlockBounds.Add(Bounds);
			}
			NextBlockTopY = Bounds.IsValid() ? Bounds.MaxY + BlockSpacing : NextBlockTopY + BlockSpacing;
		}

		PositionMaterialRootNearOutputs(Material, BlockBounds);
		GroupOutputBridgeUsages(Material, MaterialFunction, Expressions);
	}
}
