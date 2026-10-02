#include "DreamPassPipeline.h"

#include "DreamShaderPassModule.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "DreamShader.Pass.Pipeline"

FOnDreamPassPipelineChanged UDreamPassPipeline::OnPipelineChanged;

const FDreamPassBufferDesc* UDreamPassPipeline::FindBuffer(FName Name) const
{
	return Buffers.FindByPredicate([Name](const FDreamPassBufferDesc& Buffer) { return Buffer.Name == Name; });
}

const FDreamPassParameterDesc* UDreamPassPipeline::FindParameter(FName Name) const
{
	return Parameters.FindByPredicate([Name](const FDreamPassParameterDesc& Parameter) { return Parameter.Name == Name; });
}

int32 UDreamPassPipeline::FindPassIndex(FName Name) const
{
	return Passes.IndexOfByPredicate([Name](const FDreamPassDesc& Pass) { return Pass.Name == Name; });
}

UTextureRenderTarget2D* UDreamPassPipeline::GetExportTarget(FName Buffer) const
{
	const TObjectPtr<UTextureRenderTarget2D>* Found = ExportTargets.Find(Buffer);
	return Found ? Found->Get() : nullptr;
}

namespace UE::DreamPass::Private
{
	static bool ValidatePass(const UDreamPassPipeline& Pipeline, const FDreamPassDesc& Pass, TArray<FText>* OutProblems)
	{
		bool bOk = true;
		auto Problem = [&](FText Text)
		{
			bOk = false;
			if (OutProblems)
			{
				OutProblems->Add(FText::Format(LOCTEXT("PassProblem", "Pass '{0}': {1}"), FText::FromName(Pass.Name), Text));
			}
		};

		auto CheckBuffer = [&](const FDreamPassBufferBinding& Binding)
		{
			if (Binding.Buffer.IsNone())
			{
				Problem(LOCTEXT("BindingNoBuffer", "a binding names no buffer."));
				return;
			}
			if (IsBuiltinBuffer(Binding.Buffer))
			{
				if (Binding.bPrevious)
				{
					Problem(FText::Format(LOCTEXT("BuiltinPrevious", "'{0}.Previous': a built-in buffer has no history."), FText::FromName(Binding.Buffer)));
				}
				return;
			}
			const FDreamPassBufferDesc* Buffer = Pipeline.FindBuffer(Binding.Buffer);
			if (!Buffer)
			{
				Problem(FText::Format(LOCTEXT("UnknownBuffer", "'{0}' is not a buffer of this pipeline."), FText::FromName(Binding.Buffer)));
				return;
			}
			if (Binding.bPrevious && !Buffer->bHistory)
			{
				Problem(FText::Format(LOCTEXT("PreviousWithoutHistory", "'{0}.Previous': the buffer keeps no history."), FText::FromName(Binding.Buffer)));
			}
		};

		for (const FDreamPassBufferBinding& Binding : Pass.Reads)
		{
			CheckBuffer(Binding);
		}
		for (const FDreamPassBufferBinding& Binding : Pass.Writes)
		{
			CheckBuffer(Binding);
			if (Binding.bPrevious)
			{
				Problem(LOCTEXT("WritePrevious", "a write cannot target '.Previous'."));
			}
		}

		switch (Pass.Kind)
		{
		case EDreamPassKind::Fullscreen:
			if (!Pass.Fullscreen.Material && Pass.Fullscreen.PixelSlot == INDEX_NONE)
			{
				Problem(LOCTEXT("FullscreenNothing", "it has neither a material nor a pixel shader slot."));
			}
			if (Pass.Fullscreen.PixelSlot != INDEX_NONE && (Pass.Fullscreen.PixelSlot < 0 || Pass.Fullscreen.PixelSlot >= GetPixelSlotCount()))
			{
				Problem(LOCTEXT("PixelSlotRange", "its pixel shader slot is out of range."));
			}
			if (Pass.Fullscreen.Material && Pass.Writes.Num() != 1)
			{
				Problem(LOCTEXT("FullscreenOneWrite", "a material pass writes exactly one buffer."));
			}
			if (Pass.Writes.Num() > MaxSlotOutputs)
			{
				Problem(LOCTEXT("FullscreenTooManyWrites", "it writes more buffers than a pixel shader slot has outputs."));
			}
			break;

		case EDreamPassKind::Compute:
			if (Pass.Compute.Slot < 0 || Pass.Compute.Slot >= GetComputeSlotCount())
			{
				Problem(LOCTEXT("ComputeSlot", "it has no compute shader slot."));
			}
			if (Pass.Writes.IsEmpty())
			{
				Problem(LOCTEXT("ComputeNoWrite", "a compute pass writes at least one buffer."));
			}
			if (Pass.Reads.Num() > MaxSlotInputs || Pass.Writes.Num() > MaxSlotOutputs)
			{
				Problem(LOCTEXT("ComputeTooMany", "it binds more buffers than a compute shader slot has."));
			}
			if (Pass.Compute.DispatchMode == EDreamPassDispatchMode::Buffer && Pass.Compute.DispatchBuffer.IsNone())
			{
				Problem(LOCTEXT("ComputeDispatchBuffer", "it dispatches over a buffer it does not name."));
			}
			break;

		case EDreamPassKind::Mesh:
			if (Pass.Mesh.Filter.IsEmpty())
			{
				Problem(LOCTEXT("MeshNoFilter", "a mesh pass selects nothing."));
			}
			if (Pass.Mesh.Mode != EDreamPassMeshMode::Own && !Pass.Mesh.OverrideMaterial)
			{
				Problem(LOCTEXT("MeshNoMaterial", "Override and OwnOrOverride need a material."));
			}
			if (Pass.Writes.IsEmpty() || Pass.Writes.Num() > MaxMeshOutputs)
			{
				Problem(LOCTEXT("MeshWrites", "a mesh pass writes one to four buffers."));
			}
			if (Pass.Mesh.Depth == EDreamPassDepthMode::Own)
			{
				const FDreamPassBufferDesc* Depth = Pipeline.FindBuffer(Pass.Mesh.DepthBuffer);
				if (!Depth || Depth->Format != EDreamPassBufferFormat::Depth32)
				{
					Problem(LOCTEXT("MeshDepthBuffer", "Depth = Own names no Depth32 buffer."));
				}
			}
			break;

		case EDreamPassKind::Clear:
			if (Pass.Writes.Num() != 1)
			{
				Problem(LOCTEXT("ClearOneWrite", "a clear writes exactly one buffer."));
			}
			break;

		case EDreamPassKind::Copy:
			if (Pass.Reads.Num() != 1 || Pass.Writes.Num() != 1)
			{
				Problem(LOCTEXT("CopyOneEach", "a copy reads one buffer and writes one."));
			}
			break;
		}

		for (const FDreamPassParamBinding& Param : Pass.Params)
		{
			if (Param.Source == EDreamPassParamSource::Parameter && !Pipeline.FindParameter(Param.Parameter))
			{
				Problem(FText::Format(LOCTEXT("UnknownParameter", "'{0}' is not a parameter of this pipeline."), FText::FromName(Param.Parameter)));
			}
		}
		if (!Pass.EnabledParameter.IsNone())
		{
			const FDreamPassParameterDesc* Enabled = Pipeline.FindParameter(Pass.EnabledParameter);
			if (!Enabled || Enabled->Default.Type != EDreamPassParameterType::Bool)
			{
				Problem(FText::Format(LOCTEXT("EnabledParameter", "'{0}' is not a Bool parameter."), FText::FromName(Pass.EnabledParameter)));
			}
		}

		return bOk;
	}
}

bool UDreamPassPipeline::Validate(TArray<FText>* OutProblems) const
{
	bool bOk = true;
	auto Problem = [&](FText Text)
	{
		bOk = false;
		if (OutProblems)
		{
			OutProblems->Add(MoveTemp(Text));
		}
	};

	TSet<FName> Seen;
	for (const FDreamPassBufferDesc& Buffer : Buffers)
	{
		bool bAlreadyIn = false;
		Seen.Add(Buffer.Name, &bAlreadyIn);
		if (Buffer.Name.IsNone() || bAlreadyIn || UE::DreamPass::IsBuiltinBuffer(Buffer.Name))
		{
			Problem(FText::Format(LOCTEXT("BufferName", "Buffer '{0}': the name is empty, taken twice, or a built-in one."), FText::FromName(Buffer.Name)));
		}
		if (Buffer.bExport && !GetExportTarget(Buffer.Name))
		{
			Problem(FText::Format(LOCTEXT("ExportTarget", "Buffer '{0}' is exported but has no render target."), FText::FromName(Buffer.Name)));
		}
	}

	Seen.Reset();
	for (const FDreamPassParameterDesc& Parameter : Parameters)
	{
		bool bAlreadyIn = false;
		Seen.Add(Parameter.Name, &bAlreadyIn);
		if (Parameter.Name.IsNone() || bAlreadyIn)
		{
			Problem(FText::Format(LOCTEXT("ParameterName", "Parameter '{0}': the name is empty or taken twice."), FText::FromName(Parameter.Name)));
		}
	}

	Seen.Reset();
	for (const FDreamPassDesc& Pass : Passes)
	{
		bool bAlreadyIn = false;
		Seen.Add(Pass.Name, &bAlreadyIn);
		if (Pass.Name.IsNone() || bAlreadyIn)
		{
			Problem(FText::Format(LOCTEXT("PassName", "Pass '{0}': the name is empty or taken twice."), FText::FromName(Pass.Name)));
		}
		if (!UE::DreamPass::Private::ValidatePass(*this, Pass, OutProblems))
		{
			bOk = false;
		}
	}

	return bOk;
}

void UDreamPassPipeline::RefreshUsablePasses()
{
	UsablePasses.Init(false, Passes.Num());
	for (int32 Index = 0; Index < Passes.Num(); ++Index)
	{
		TArray<FText> Problems;
		const bool bUsable = UE::DreamPass::Private::ValidatePass(*this, Passes[Index], &Problems);
		UsablePasses[Index] = bUsable;
		for (const FText& Problem : Problems)
		{
			UE_LOG(LogDreamPass, Warning, TEXT("%s: %s The pass is skipped."), *GetPathName(), *Problem.ToString());
		}
	}
}

void UDreamPassPipeline::NotifyChanged()
{
	RefreshUsablePasses();
	++Revision;
	OnPipelineChanged.Broadcast(this);
}

void UDreamPassPipeline::PostLoad()
{
	Super::PostLoad();
	NotifyChanged();
}

#if WITH_EDITOR
void UDreamPassPipeline::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	NotifyChanged();
}
#endif

namespace UE::DreamPass
{
	EDreamPassParameterType GetParamBindingType(const UDreamPassPipeline& Pipeline, const FDreamPassParamBinding& Binding)
	{
		switch (Binding.Source)
		{
		case EDreamPassParamSource::Parameter:
			if (const FDreamPassParameterDesc* Desc = Pipeline.FindParameter(Binding.Parameter))
			{
				return Desc->Default.Type;
			}
			return EDreamPassParameterType::Float;
		case EDreamPassParamSource::Constant:
			return Binding.Constant.Type;
		default:
			return EDreamPassParameterType::Float;
		}
	}

	bool LayoutSlotParameters(TConstArrayView<FName> Names, TConstArrayView<EDreamPassParameterType> Types, TArray<FDreamPassSlotParamLocation>& OutLocations)
	{
		OutLocations.Reset();
		if (Names.Num() != Types.Num())
		{
			return false;
		}

		// Four bits per vector, one per component in use.
		uint8 UsedMask[MaxSlotParamVectors] = {};

		for (int32 Index = 0; Index < Types.Num(); ++Index)
		{
			int32 Width = 0;
			switch (Types[Index])
			{
			case EDreamPassParameterType::Float:
			case EDreamPassParameterType::Int:
			case EDreamPassParameterType::Bool:   Width = 1; break;
			case EDreamPassParameterType::Float2: Width = 2; break;
			case EDreamPassParameterType::Float3: Width = 3; break;
			case EDreamPassParameterType::Float4: Width = 4; break;
			default: return false;
			}

			int32 FoundVector = INDEX_NONE;
			int32 FoundComponent = 0;
			for (int32 Vector = 0; Vector < MaxSlotParamVectors && FoundVector == INDEX_NONE; ++Vector)
			{
				const uint8 Mask = UsedMask[Vector];
				if (Width >= 3)
				{
					if (Mask == 0)
					{
						FoundVector = Vector;
						FoundComponent = 0;
					}
				}
				else if (Width == 2)
				{
					if ((Mask & 0x3) == 0)
					{
						FoundVector = Vector;
						FoundComponent = 0;
					}
					else if ((Mask & 0xC) == 0)
					{
						FoundVector = Vector;
						FoundComponent = 2;
					}
				}
				else
				{
					for (int32 Component = 0; Component < 4; ++Component)
					{
						if ((Mask & (1 << Component)) == 0)
						{
							FoundVector = Vector;
							FoundComponent = Component;
							break;
						}
					}
				}
			}

			if (FoundVector == INDEX_NONE)
			{
				return false;
			}

			UsedMask[FoundVector] |= uint8(((1 << Width) - 1) << FoundComponent);

			FDreamPassSlotParamLocation& Location = OutLocations.AddDefaulted_GetRef();
			Location.Name = Names[Index];
			Location.Type = Types[Index];
			Location.Vector = FoundVector;
			Location.Component = FoundComponent;
			Location.Width = Width;
		}
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
