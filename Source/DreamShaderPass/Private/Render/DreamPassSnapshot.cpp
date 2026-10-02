#include "Render/DreamPassSnapshot.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "DreamPassConsole.h"
#include "DreamPassPipeline.h"
#include "DreamPassSubsystem.h"
#include "DreamShaderPassModule.h"
#include "Render/DreamPassMesh.h"

#include "CoreGlobals.h"
#include "Engine/Texture.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SceneInterface.h"

namespace UE::DreamPass
{
	EDreamPassViewFlags ClassifyView(const FSceneViewFamily& Family, const FSceneView& View)
	{
		// Hit proxies, virtual texture views and the like draw ids or pages, not pictures: never a pipeline's.
		if (Family.EngineShowFlags.HitProxies || View.bIsVirtualTexture)
		{
			return EDreamPassViewFlags::None;
		}
		if (View.bIsReflectionCapture)
		{
			return EDreamPassViewFlags::ReflectionCapture;
		}
		if (View.bIsPlanarReflection)
		{
			return EDreamPassViewFlags::PlanarReflection;
		}
		if (View.bIsSceneCapture || View.bIsSceneCaptureCube)
		{
			return EDreamPassViewFlags::SceneCapture;
		}
		if (View.bIsGameView)
		{
			return EDreamPassViewFlags::Game;
		}
		// An editor viewport keeps a view state; a thumbnail, an asset preview render or a one-off capture does not.
		return View.State ? EDreamPassViewFlags::Editor : EDreamPassViewFlags::Thumbnail;
	}

	EDreamPassRequirementFlags GetViewCapabilities(const FSceneViewFamily& Family, const FSceneView& View)
	{
		EDreamPassRequirementFlags Capabilities = EDreamPassRequirementFlags::None;
		if (Family.EngineShowFlags.PostProcessing)
		{
			Capabilities |= EDreamPassRequirementFlags::PostProcess;
		}
		if (Family.bResolveScene)
		{
			Capabilities |= EDreamPassRequirementFlags::SceneResolve;
		}
		static const auto* CustomDepthVariable = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.CustomDepth"));
		if (CustomDepthVariable && CustomDepthVariable->GetValueOnGameThread() == 3)
		{
			Capabilities |= EDreamPassRequirementFlags::CustomStencil;
		}
		return Capabilities;
	}

	namespace Private
	{
		/** The value a `param` binding has in a view: its source, scaled and offset when it is a number. */
		static FDreamPassParameterValue ResolveParamBinding(const FDreamPassResolvedPipeline& Resolved, const FDreamPassParamBinding& Binding)
		{
			FDreamPassParameterValue Value;
			switch (Binding.Source)
			{
			case EDreamPassParamSource::Constant:
				return Binding.Constant;

			case EDreamPassParamSource::Weight:
				Value = FDreamPassParameterValue::MakeFloat(Resolved.Weight);
				break;

			case EDreamPassParamSource::Parameter:
				if (const FDreamPassParameterValue* Found = Resolved.FindValue(Binding.Parameter))
				{
					Value = *Found;
				}
				break;
			}

			if (Binding.Multiplier != 1.0f || Binding.Offset != 0.0f)
			{
				switch (Value.Type)
				{
				case EDreamPassParameterType::Float:
				case EDreamPassParameterType::Float2:
				case EDreamPassParameterType::Float3:
				case EDreamPassParameterType::Float4:
					Value.Vector = Value.Vector * Binding.Multiplier + FVector4f(Binding.Offset, Binding.Offset, Binding.Offset, Binding.Offset);
					break;
				case EDreamPassParameterType::Int:
					Value.Int = FMath::RoundToInt(float(Value.Int) * Binding.Multiplier + Binding.Offset);
					break;
				default:
					break;
				}
			}
			return Value;
		}

		static void ApplyMaterialParameter(UMaterialInstanceDynamic& Instance, FName Name, const FDreamPassParameterValue& Value)
		{
			switch (Value.Type)
			{
			case EDreamPassParameterType::Float:
				Instance.SetScalarParameterValue(Name, Value.Vector.X);
				break;
			case EDreamPassParameterType::Int:
				Instance.SetScalarParameterValue(Name, float(Value.Int));
				break;
			case EDreamPassParameterType::Bool:
				Instance.SetScalarParameterValue(Name, Value.Bool ? 1.0f : 0.0f);
				break;
			case EDreamPassParameterType::Texture:
				Instance.SetTextureParameterValue(Name, Value.Texture);
				break;
			default:
				Instance.SetVectorParameterValue(Name, FLinearColor(Value.Vector.X, Value.Vector.Y, Value.Vector.Z, Value.Vector.W));
				break;
			}
		}

		/** A material instance of Base for this use, with the pass's parameters and the pipeline's weight set on it. */
		static UMaterialInstanceDynamic* MakePassMaterial(UDreamPassSubsystem& Subsystem, UMaterialInterface* Base, const FDreamPassResolvedPipeline& Resolved, const FDreamPassDesc& Pass)
		{
			UMaterialInstanceDynamic* Instance = Subsystem.AcquireMaterialInstance(Base);
			if (!Instance)
			{
				return nullptr;
			}
			Instance->SetScalarParameterValue(WeightParameterName, Resolved.Weight);
			for (const FDreamPassParamBinding& Binding : Pass.Params)
			{
				ApplyMaterialParameter(*Instance, Binding.Target, ResolveParamBinding(Resolved, Binding));
			}
			return Instance;
		}

		static void PackSlotParameters(const FDreamPassResolvedPipeline& Resolved, const FDreamPassDesc& Pass, TArray<FVector4f>& OutParams)
		{
			OutParams.Reset();
			if (Pass.Params.IsEmpty())
			{
				return;
			}

			TArray<FName> Names;
			TArray<EDreamPassParameterType> Types;
			for (const FDreamPassParamBinding& Binding : Pass.Params)
			{
				Names.Add(Binding.Target);
				Types.Add(GetParamBindingType(*Resolved.Pipeline, Binding));
			}

			TArray<FDreamPassSlotParamLocation> Locations;
			if (!LayoutSlotParameters(Names, Types, Locations))
			{
				return;
			}

			int32 VectorCount = 0;
			for (const FDreamPassSlotParamLocation& Location : Locations)
			{
				VectorCount = FMath::Max(VectorCount, Location.Vector + 1);
			}
			OutParams.SetNumZeroed(VectorCount);

			for (int32 Index = 0; Index < Locations.Num(); ++Index)
			{
				const FDreamPassSlotParamLocation& Location = Locations[Index];
				const FDreamPassParameterValue Value = ResolveParamBinding(Resolved, Pass.Params[Index]);
				FVector4f& Vector = OutParams[Location.Vector];

				// An int or a bool travels as its bits, which the registry reads back with asint / asuint.
				if (Location.Type == EDreamPassParameterType::Int)
				{
					Vector[Location.Component] = FMath::AsFloat(uint32(Value.Int));
				}
				else if (Location.Type == EDreamPassParameterType::Bool)
				{
					Vector[Location.Component] = FMath::AsFloat(Value.Bool ? 1u : 0u);
				}
				else
				{
					for (int32 Component = 0; Component < Location.Width; ++Component)
					{
						Vector[Location.Component + Component] = Value.Vector[Component];
					}
				}
			}
		}

		static bool IsPassEnabled(const FDreamPassResolvedPipeline& Resolved, const FDreamPassDesc& Pass)
		{
			if (Pass.EnabledParameter.IsNone())
			{
				return true;
			}
			const FDreamPassParameterValue* Enabled = Resolved.FindValue(Pass.EnabledParameter);
			return !Enabled || Enabled->Type != EDreamPassParameterType::Bool || Enabled->Bool;
		}

		static void BuildPipeline(UDreamPassSubsystem& Subsystem, const FDreamPassResolvedPipeline& Resolved, FSnapshotPipeline& Out)
		{
			const UDreamPassPipeline& Pipeline = *Resolved.Pipeline;
			Out.DebugName = Pipeline.GetName();
			Out.Identity = &Pipeline;
			Out.Order = Pipeline.Order;
			Out.Weight = Resolved.Weight;

			Out.Buffers.Reserve(Pipeline.Buffers.Num());
			for (const FDreamPassBufferDesc& Desc : Pipeline.Buffers)
			{
				FSnapshotBuffer& Buffer = Out.Buffers.AddDefaulted_GetRef();
				Buffer.Desc = Desc;
				Buffer.bNeedsRenderTarget = Desc.Format != EDreamPassBufferFormat::Depth32;
			}

			for (int32 PassIndex = 0; PassIndex < Pipeline.Passes.Num(); ++PassIndex)
			{
				const FDreamPassDesc& Desc = Pipeline.Passes[PassIndex];
				if (!Pipeline.IsPassUsable(PassIndex) || !IsPassEnabled(Resolved, Desc))
				{
					continue;
				}

				FSnapshotPass Pass;
				Pass.Name = Desc.Name;
				Pass.PassIndex = PassIndex;
				Pass.Kind = Desc.Kind;
				Pass.Injection = Desc.Injection;
				Pass.Reads = Desc.Reads;
				Pass.Writes = Desc.Writes;
				Pass.Weight = Resolved.Weight;

				switch (Desc.Kind)
				{
				case EDreamPassKind::Fullscreen:
					if (Desc.Fullscreen.Material)
					{
						Pass.Material = MakePassMaterial(Subsystem, Desc.Fullscreen.Material, Resolved, Desc);
						if (!Pass.Material)
						{
							continue;
						}
					}
					else
					{
						Pass.Slot = Desc.Fullscreen.PixelSlot;
						PackSlotParameters(Resolved, Desc, Pass.SlotParams);
					}
					break;

				case EDreamPassKind::Compute:
					Pass.Slot = Desc.Compute.Slot;
					Pass.ThreadGroupSize = Desc.Compute.ThreadGroupSize;
					Pass.DispatchMode = Desc.Compute.DispatchMode;
					Pass.DispatchBuffer = Desc.Compute.DispatchBuffer;
					Pass.DispatchScale = Desc.Compute.DispatchScale;
					Pass.DispatchSize = Desc.Compute.DispatchSize;
					PackSlotParameters(Resolved, Desc, Pass.SlotParams);
					for (const FDreamPassBufferBinding& Write : Desc.Writes)
					{
						const int32 BufferIndex = Out.FindBuffer(Write.Buffer);
						if (Out.Buffers.IsValidIndex(BufferIndex))
						{
							Out.Buffers[BufferIndex].bNeedsUAV = true;
						}
					}
					break;

				case EDreamPassKind::Mesh:
					Pass.Mesh = Desc.Mesh;
					if (Desc.Mesh.OverrideMaterial)
					{
						if (UMaterialInstanceDynamic* Instance = MakePassMaterial(Subsystem, Desc.Mesh.OverrideMaterial, Resolved, Desc))
						{
							Pass.OverrideMaterialProxy = Instance->GetRenderProxy();
						}
					}
					break;

				case EDreamPassKind::Clear:
					Pass.ClearValue = Desc.Clear.Value;
					break;

				case EDreamPassKind::Copy:
					break;
				}

				Out.Passes.Add(MoveTemp(Pass));
			}
		}

		static void ScheduleView(FViewSnapshot& View)
		{
			View.Order.Reset();
			View.InjectionMask = 0;

			for (int32 Injection = 0; Injection < int32(EDreamPassInjection::Count); ++Injection)
			{
				const int32 Start = View.Order.Num();
				// Pipelines are already in execution order (ResolveView sorts them by Order, then path).
				for (int32 PipelineIndex = 0; PipelineIndex < View.Pipelines.Num(); ++PipelineIndex)
				{
					const FSnapshotPipeline& Pipeline = View.Pipelines[PipelineIndex];
					for (int32 PassIndex = 0; PassIndex < Pipeline.Passes.Num(); ++PassIndex)
					{
						if (int32(Pipeline.Passes[PassIndex].Injection) == Injection)
						{
							View.Order.Add({ PipelineIndex, PassIndex });
						}
					}
				}
				View.RangeByInjection[Injection] = TPair<int32, int32>(Start, View.Order.Num());
				if (View.Order.Num() > Start)
				{
					View.InjectionMask |= 1u << uint32(Injection);
				}
			}

			for (int32 OrderIndex = 0; OrderIndex < View.Order.Num(); ++OrderIndex)
			{
				FSnapshotPipeline& Pipeline = View.Pipelines[View.Order[OrderIndex].Pipeline];
				const FSnapshotPass& Pass = Pipeline.Passes[View.Order[OrderIndex].Pass];
				for (const FDreamPassBufferBinding& Write : Pass.Writes)
				{
					const int32 BufferIndex = Pipeline.FindBuffer(Write.Buffer);
					if (Pipeline.Buffers.IsValidIndex(BufferIndex))
					{
						Pipeline.Buffers[BufferIndex].LastWriterOrder = OrderIndex;
					}
				}
				if (Pass.Kind == EDreamPassKind::Mesh && Pass.Mesh.Depth == EDreamPassDepthMode::Own)
				{
					const int32 DepthIndex = Pipeline.FindBuffer(Pass.Mesh.DepthBuffer);
					if (Pipeline.Buffers.IsValidIndex(DepthIndex))
					{
						Pipeline.Buffers[DepthIndex].LastWriterOrder = OrderIndex;
					}
				}
			}
		}
	}

	FFamilySnapshotPtr BuildFamilySnapshot(UDreamPassSubsystem& Subsystem, FSceneViewFamily& Family)
	{
		TSharedRef<FFamilySnapshot, ESPMode::ThreadSafe> Snapshot = MakeShared<FFamilySnapshot, ESPMode::ThreadSafe>();
		Snapshot->FrameNumber = GFrameCounter;
		Snapshot->Views.SetNum(Family.Views.Num());

		Subsystem.BeginMaterialFrame();

		TArray<FDreamPassResolvedPipeline> Resolved;
		for (int32 ViewIndex = 0; ViewIndex < Family.Views.Num(); ++ViewIndex)
		{
			const FSceneView* View = Family.Views[ViewIndex];
			if (!View)
			{
				continue;
			}

			FDreamPassViewQuery Query;
			Query.ViewKind = ClassifyView(Family, *View);
			if (Query.ViewKind == EDreamPassViewFlags::None)
			{
				continue;
			}
			// Before anything decides whether pipelines apply: a view that could claim the export rendered, which is when
			// an export it did not fill this frame is cleared (UDreamPassSubsystem::NoteExportTarget).
			Subsystem.NoteViewForExport(*View, Query.ViewKind);
			Query.ViewLocation = View->ViewMatrices.GetViewOrigin();
			Query.PlayerIndex = View->PlayerIndex;
			Query.ViewActorUniqueId = View->ViewActor.ActorUniqueId;
			Query.Capabilities = GetViewCapabilities(Family, *View);

			Subsystem.ResolveView(Query, Resolved);
			if (Resolved.IsEmpty())
			{
				continue;
			}

			FViewSnapshot& ViewSnapshot = Snapshot->Views[ViewIndex];
			ViewSnapshot.ViewKey = View->GetViewKey();
			// Only a view with something to export claims the export, so a view whose pipelines export nothing cannot keep
			// the one that does from writing its render targets this frame.
			const bool bHasExports = Resolved.ContainsByPredicate([](const FDreamPassResolvedPipeline& Pipeline)
			{
				return Pipeline.Pipeline->Buffers.ContainsByPredicate([](const FDreamPassBufferDesc& Buffer) { return Buffer.bExport; });
			});
			ViewSnapshot.bExportView = bHasExports && Subsystem.TryClaimExportView(*View, Query.ViewKind);
			ViewSnapshot.Pipelines.SetNum(Resolved.Num());
			for (int32 PipelineIndex = 0; PipelineIndex < Resolved.Num(); ++PipelineIndex)
			{
				Private::BuildPipeline(Subsystem, Resolved[PipelineIndex], ViewSnapshot.Pipelines[PipelineIndex]);
				if (ViewSnapshot.bExportView)
				{
					PrepareExportTargets(Subsystem, ViewSnapshot.Pipelines[PipelineIndex], *Resolved[PipelineIndex].Pipeline, *View);
				}
			}

			Private::ScheduleView(ViewSnapshot);
			ViewSnapshot.bActive = ViewSnapshot.InjectionMask != 0;
			Snapshot->InjectionMask |= ViewSnapshot.InjectionMask;
		}

		if (Snapshot->InjectionMask == 0)
		{
			return nullptr;
		}

		Subsystem.GatherPrimitiveSelections(Snapshot->LayersByPrimitiveId, Snapshot->ListMembers);
		UpdateNaniteStencilAssignments(Subsystem, *Snapshot);
		Snapshot->bCustomStencilWritten = EnumHasAnyFlags(GetViewCapabilities(Family, *Family.Views[0]), EDreamPassRequirementFlags::CustomStencil);
		Snapshot->VisualizeTarget = GetVisualizeTarget();
		return Snapshot;
	}
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
