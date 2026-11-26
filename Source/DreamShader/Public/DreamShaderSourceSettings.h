#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"

#include "DreamShaderSourceSettings.generated.h"

/**
 * Where the project's DreamShader sources live.
 *
 * Its own class, in its own ini, rather than a field on UDreamShaderSettings: every Dream plugin keeps its source
 * directory in `Config/DefaultDreamSource.ini`, one section each, so that Project Settings shows them together under
 * "Dream Source" and the VSCode extensions -- which cannot ask the engine -- have one file to read. The section
 * (`[/Script/DreamShader.DreamShaderSourceSettings]`) and the key are a contract with dreamshader-language-support.
 *
 * Never read directly: UE::DreamShader::ResolveProjectSourceDirectory applies the rest of the rule -- the legacy
 * `UDreamShaderSettings::SourceDirectory` key, and the legacy `DShader/` directory, which still answers while the
 * configured one does not exist.
 */
UCLASS(Config=DreamSource, DefaultConfig, meta=(DisplayName="Shader"))
class DREAMSHADER_API UDreamShaderSourceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDreamShaderSourceSettings();

	/**
	 * The directory holding the project's .dss/.dsf/.dsh/.dsi/.dsp files, relative to the project directory. A plugin's
	 * own sources are not configurable: they live in `<Plugin>/DreamSource/Shader`. Takes effect after an editor restart
	 * -- the Custom Pass shader directory is mapped at startup.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Source", meta=(RelativeToGameDir))
	FDirectoryPath SourceDirectory;

	/** The default value, `DreamSource/Shader`. */
	static const TCHAR* GetDefaultSourceDirectory();

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Dream Source"); }
	virtual FName GetSectionName() const override { return TEXT("Shader"); }
};
