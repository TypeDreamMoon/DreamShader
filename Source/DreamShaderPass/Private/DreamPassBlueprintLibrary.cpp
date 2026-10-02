#include "DreamPassBlueprintLibrary.h"

#include "DreamPassPipeline.h"

#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"

UDreamPassSubsystem* UDreamPassBlueprintLibrary::GetSubsystem(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return UDreamPassSubsystem::Get(World);
}

FDreamPassHandle UDreamPassBlueprintLibrary::AddPipeline(const UObject* WorldContextObject, UDreamPassPipeline* Pipeline, float Priority, const TArray<FDreamPassParameterOverride>& Overrides, int32 PlayerIndex)
{
	UDreamPassSubsystem* Subsystem = GetSubsystem(WorldContextObject);
	return Subsystem ? Subsystem->AddPipeline(Pipeline, Priority, Overrides, PlayerIndex) : FDreamPassHandle();
}

bool UDreamPassBlueprintLibrary::RemovePipeline(const UObject* WorldContextObject, FDreamPassHandle Handle)
{
	UDreamPassSubsystem* Subsystem = GetSubsystem(WorldContextObject);
	return Subsystem && Subsystem->RemovePipeline(Handle);
}

bool UDreamPassBlueprintLibrary::SetPipelineWeight(const UObject* WorldContextObject, FDreamPassHandle Handle, float Weight)
{
	UDreamPassSubsystem* Subsystem = GetSubsystem(WorldContextObject);
	return Subsystem && Subsystem->SetWeight(Handle, Weight);
}

bool UDreamPassBlueprintLibrary::SetParameter(const UObject* WorldContextObject, FDreamPassHandle Handle, FName Name, const FDreamPassParameterValue& Value)
{
	UDreamPassSubsystem* Subsystem = GetSubsystem(WorldContextObject);
	return Subsystem && Subsystem->SetParameter(Handle, Name, Value);
}

bool UDreamPassBlueprintLibrary::SetFloatParameter(const UObject* WorldContextObject, FDreamPassHandle Handle, FName Name, float Value)
{
	return SetParameter(WorldContextObject, Handle, Name, FDreamPassParameterValue::MakeFloat(Value));
}

bool UDreamPassBlueprintLibrary::SetColorParameter(const UObject* WorldContextObject, FDreamPassHandle Handle, FName Name, FLinearColor Value)
{
	return SetParameter(WorldContextObject, Handle, Name, MakeColorValue(Value));
}

bool UDreamPassBlueprintLibrary::SetBoolParameter(const UObject* WorldContextObject, FDreamPassHandle Handle, FName Name, bool Value)
{
	return SetParameter(WorldContextObject, Handle, Name, FDreamPassParameterValue::MakeBool(Value));
}

bool UDreamPassBlueprintLibrary::SetIntParameter(const UObject* WorldContextObject, FDreamPassHandle Handle, FName Name, int32 Value)
{
	return SetParameter(WorldContextObject, Handle, Name, FDreamPassParameterValue::MakeInt(Value));
}

bool UDreamPassBlueprintLibrary::SetTextureParameter(const UObject* WorldContextObject, FDreamPassHandle Handle, FName Name, UTexture* Value)
{
	return SetParameter(WorldContextObject, Handle, Name, FDreamPassParameterValue::MakeTexture(Value));
}

void UDreamPassBlueprintLibrary::AddToList(const UObject* WorldContextObject, FName List, UPrimitiveComponent* Primitive)
{
	if (UDreamPassSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->AddToList(List, Primitive);
	}
}

void UDreamPassBlueprintLibrary::RemoveFromList(const UObject* WorldContextObject, FName List, UPrimitiveComponent* Primitive)
{
	if (UDreamPassSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->RemoveFromList(List, Primitive);
	}
}

void UDreamPassBlueprintLibrary::ClearList(const UObject* WorldContextObject, FName List)
{
	if (UDreamPassSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->ClearList(List);
	}
}

UTextureRenderTarget2D* UDreamPassBlueprintLibrary::GetExportTarget(UDreamPassPipeline* Pipeline, FName Buffer)
{
	return Pipeline ? Pipeline->GetExportTarget(Buffer) : nullptr;
}
