#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"

#include "DreamShaderSettings.generated.h"

UCLASS(Config=Engine, DefaultConfig, meta=(DisplayName="DreamShader"))
class DREAMSHADER_API UDreamShaderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDreamShaderSettings();

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("DreamShader"); }

	bool TryResolveShadingModel(const FString& InName, EMaterialShadingModel& OutShadingModel) const;
	bool TryResolveBlendMode(const FString& InName, EBlendMode& OutBlendMode) const;
	bool TryResolveMaterialDomain(const FString& InName, EMaterialDomain& OutMaterialDomain) const;

	UPROPERTY(Config, EditAnywhere, Category="Mappings")
	TMap<FString, TEnumAsByte<EMaterialShadingModel>> ShadingModelMappings;

	UPROPERTY(Config, EditAnywhere, Category="Mappings")
	TMap<FString, TEnumAsByte<EBlendMode>> BlendModeMappings;

	UPROPERTY(Config, EditAnywhere, Category="Mappings")
	TMap<FString, TEnumAsByte<EMaterialDomain>> MaterialDomainMappings;

	UPROPERTY(Config, EditAnywhere, Category="Paths", meta=(RelativeToGameDir))
	FDirectoryPath SourceDirectory;

	UPROPERTY(Config, EditAnywhere, Category="Paths", meta=(RelativeToGameDir))
	FDirectoryPath BuiltinLibraryDirectory;

	UPROPERTY(Config, EditAnywhere, Category="Paths", meta=(RelativeToGameDir))
	FDirectoryPath GeneratedShaderDirectory;

	UPROPERTY(Config, EditAnywhere, Category="Compiler")
	bool bAutoCompileOnSave = true;

	UPROPERTY(Config, EditAnywhere, Category="Compiler", meta=(ClampMin="0.05", ClampMax="10.0", UIMin="0.05", UIMax="2.0"))
	float SaveDebounceSeconds = 0.25f;

	UPROPERTY(Config, EditAnywhere, Category="Compiler")
	bool bVerboseLogs = false;

private:
	static FString NormalizeShadingModelKey(const FString& InName);
	static FString NormalizeBlendModeKey(const FString& InName);
	static FString NormalizeMaterialDomainKey(const FString& InName);
};
