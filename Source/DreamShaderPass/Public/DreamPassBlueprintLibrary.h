#pragma once

#include "CoreMinimal.h"
#include "DreamPassSubsystem.h"
#include "DreamPassTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "DreamPassBlueprintLibrary.generated.h"

class UDreamPassPipeline;
class UPrimitiveComponent;
class UTexture;
class UTextureRenderTarget2D;

/** Blueprint access to UDreamPassSubsystem: activating pipelines, setting their parameters, filling mesh pass lists. */
UCLASS()
class DREAMSHADERPASS_API UDreamPassBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Runs Pipeline in the world's views -- every view, or PlayerIndex's only -- until RemovePipeline. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Overrides"))
	static FDreamPassHandle AddPipeline(const UObject* WorldContextObject, UDreamPassPipeline* Pipeline, float Priority, const TArray<FDreamPassParameterOverride>& Overrides, int32 PlayerIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (WorldContext = "WorldContextObject"))
	static bool RemovePipeline(const UObject* WorldContextObject, FDreamPassHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (WorldContext = "WorldContextObject"))
	static bool SetPipelineWeight(const UObject* WorldContextObject, FDreamPassHandle Handle, float Weight);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (WorldContext = "WorldContextObject"))
	static bool SetParameter(const UObject* WorldContextObject, FDreamPassHandle Handle, FName Name, const FDreamPassParameterValue& Value);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (WorldContext = "WorldContextObject"))
	static bool SetFloatParameter(const UObject* WorldContextObject, FDreamPassHandle Handle, FName Name, float Value);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (WorldContext = "WorldContextObject"))
	static bool SetColorParameter(const UObject* WorldContextObject, FDreamPassHandle Handle, FName Name, FLinearColor Value);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (WorldContext = "WorldContextObject"))
	static bool SetBoolParameter(const UObject* WorldContextObject, FDreamPassHandle Handle, FName Name, bool Value);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (WorldContext = "WorldContextObject"))
	static bool SetIntParameter(const UObject* WorldContextObject, FDreamPassHandle Handle, FName Name, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (WorldContext = "WorldContextObject"))
	static bool SetTextureParameter(const UObject* WorldContextObject, FDreamPassHandle Handle, FName Name, UTexture* Value);

	/** Puts a primitive in a named list, which a mesh pass selects with `Filter = List(Name)`. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (WorldContext = "WorldContextObject"))
	static void AddToList(const UObject* WorldContextObject, FName List, UPrimitiveComponent* Primitive);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (WorldContext = "WorldContextObject"))
	static void RemoveFromList(const UObject* WorldContextObject, FName List, UPrimitiveComponent* Primitive);

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass", meta = (WorldContext = "WorldContextObject"))
	static void ClearList(const UObject* WorldContextObject, FName List);

	/** The render target an exported buffer is copied into, for UMG, Niagara or a material parameter. */
	UFUNCTION(BlueprintPure, Category = "DreamShader|Custom Pass")
	static UTextureRenderTarget2D* GetExportTarget(UDreamPassPipeline* Pipeline, FName Buffer);

	UFUNCTION(BlueprintPure, Category = "DreamShader|Custom Pass|Values")
	static FDreamPassParameterValue MakeFloatValue(float Value) { return FDreamPassParameterValue::MakeFloat(Value); }

	UFUNCTION(BlueprintPure, Category = "DreamShader|Custom Pass|Values")
	static FDreamPassParameterValue MakeColorValue(FLinearColor Value) { return FDreamPassParameterValue::MakeVector(EDreamPassParameterType::Float4, FVector4f(Value.R, Value.G, Value.B, Value.A)); }

	UFUNCTION(BlueprintPure, Category = "DreamShader|Custom Pass|Values")
	static FDreamPassParameterValue MakeVector2Value(FVector2D Value) { return FDreamPassParameterValue::MakeVector(EDreamPassParameterType::Float2, FVector4f(float(Value.X), float(Value.Y), 0.0f, 0.0f)); }

	UFUNCTION(BlueprintPure, Category = "DreamShader|Custom Pass|Values")
	static FDreamPassParameterValue MakeVector3Value(FVector Value) { return FDreamPassParameterValue::MakeVector(EDreamPassParameterType::Float3, FVector4f(float(Value.X), float(Value.Y), float(Value.Z), 0.0f)); }

	UFUNCTION(BlueprintPure, Category = "DreamShader|Custom Pass|Values")
	static FDreamPassParameterValue MakeIntValue(int32 Value) { return FDreamPassParameterValue::MakeInt(Value); }

	UFUNCTION(BlueprintPure, Category = "DreamShader|Custom Pass|Values")
	static FDreamPassParameterValue MakeBoolValue(bool Value) { return FDreamPassParameterValue::MakeBool(Value); }

	UFUNCTION(BlueprintPure, Category = "DreamShader|Custom Pass|Values")
	static FDreamPassParameterValue MakeTextureValue(UTexture* Value) { return FDreamPassParameterValue::MakeTexture(Value); }

private:
	static UDreamPassSubsystem* GetSubsystem(const UObject* WorldContextObject);
};
