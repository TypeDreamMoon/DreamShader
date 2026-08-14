#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "MaterialDomain.h"

#include "DreamShaderSettings.generated.h"

/** Backend used for source files that do not specify Settings = { Backend = "..." } themselves. */
UENUM()
enum class EDreamShaderDefaultBackend : uint8
{
	/** Build a UMaterial node graph per material (full DSL feature surface). */
	Graph,
	/**
	 * DEPRECATED alias for ThinCustom (kept for one deprecation window so existing configs and
	 * Settings = { Backend = "Instance" } sources keep working). The legacy graphless-host instance
	 * backend is retired; "Instance" now generates the ThinCustom chain.
	 */
	Instance,
	/**
	 * Build the material graph on a hidden per-material base UMaterial and emit a lightweight
	 * material instance of it. Full Graph feature surface (the construction is shared) plus the
	 * instance's in-memory hiding and root shader-map ownership. The default.
	 */
	ThinCustom,
};

UCLASS(Config=Engine, DefaultConfig, meta=(DisplayName="DreamShader"))
class DREAMSHADER_API UDreamShaderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDreamShaderSettings();

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("DreamPlugin"); }
	virtual FName GetSectionName() const override { return TEXT("DreamShader"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override { return NSLOCTEXT("DreamShaderEditor.Settings", "SectionText", "Dream Shader"); }
	virtual FText GetSectionDescription() const override { return NSLOCTEXT("DreamShaderEditor.Settings", "SectionDescription", "Dream Shader Settings"); }
#endif

	bool TryResolveShadingModel(const FString& InName, EMaterialShadingModel& OutShadingModel) const;
	bool TryResolveBlendMode(const FString& InName, EBlendMode& OutBlendMode) const;
	bool TryResolveMaterialDomain(const FString& InName, EMaterialDomain& OutMaterialDomain) const;

	static FString NormalizeMappingKey(const FString& InName);
	static void BuildDefaultShadingModelMappings(TMap<FString, TEnumAsByte<EMaterialShadingModel>>& OutMappings);
	static void BuildDefaultBlendModeMappings(TMap<FString, TEnumAsByte<EBlendMode>>& OutMappings);
	static void BuildDefaultMaterialDomainMappings(TMap<FString, TEnumAsByte<EMaterialDomain>>& OutMappings);

	UPROPERTY(Config, EditAnywhere, Category="Mappings")
	TMap<FString, TEnumAsByte<EMaterialShadingModel>> ShadingModelMappings;

	UPROPERTY(Config, EditAnywhere, Category="Mappings")
	TMap<FString, TEnumAsByte<EBlendMode>> BlendModeMappings;

	UPROPERTY(Config, EditAnywhere, Category="Mappings")
	TMap<FString, TEnumAsByte<EMaterialDomain>> MaterialDomainMappings;

	UPROPERTY(Config, EditAnywhere, Category="Paths", meta=(RelativeToGameDir))
	FDirectoryPath SourceDirectory;

	UPROPERTY(Config, EditAnywhere, Category="Paths", meta=(RelativeToGameDir))
	FDirectoryPath GeneratedShaderDirectory;

	UPROPERTY(Config, EditAnywhere, Category="Paths",
		meta=(DisplayName="Scan Plugin Source Directories",
			ToolTip="When enabled, every enabled plugin that ships a DShader folder contributes its own source root, so a plugin can carry the .dsm/.dsf/.dsh files that build its materials. Plugin roots are discovered and compiled but never rewritten by the editor -- only the project's own source directory is writable. Imports never cross roots: a file resolves its imports against its own root and that root's Packages folder."))
	bool bScanPluginSourceDirectories = true;

	// The single compiler knob. DreamShader always generates materials in memory in the editor
	// (source files are the authoring surface; the editor never writes per-material .uasset files) and
	// materializes them as persistent assets during cooking — so there is no in-memory on/off toggle.
	UPROPERTY(Config, EditAnywhere, Category="Compiler",
		meta=(DisplayName="Default Compiler Backend",
			ToolTip="How DreamShader materializes a source file that does not specify Settings = { Backend = \"...\" }. ThinCustom (the default) builds the material graph on a hidden per-material base and emits a lightweight, memory-only material instance of it -- full feature surface, no visible per-material asset. Graph builds a visible UMaterial node graph. Instance is a deprecated alias for ThinCustom."))
	EDreamShaderDefaultBackend DefaultBackend = EDreamShaderDefaultBackend::ThinCustom;

	UPROPERTY(Config, EditAnywhere, Category="Compiler",
		meta=(DisplayName="Show In-Memory Materials In Content Browser",
			ToolTip="Applies to the ThinCustom/Instance backend only -- it is the only one that can hide itself, via UDreamShaderMaterialInstance::IsAsset. When enabled, those memory-only materials appear in the Content Browser like unsaved assets. Disabled by default: the source files are the intended authoring surface, and hiding the materials also prevents accidental Save actions from materializing them to disk. Graph-backend materials are plain UMaterials with no way to opt out of asset enumeration, so they are always visible and this setting does not affect them."))
	bool bShowInMemoryMaterialsInContentBrowser = false;

	UPROPERTY(Config, EditAnywhere, Category="Compiler",
		meta=(DisplayName="Lay Out In-Memory Graphs",
			ToolTip="Whether the graph placement pass -- node positions and comment boxes -- also runs for the memory-only materials an interactive compile produces. Off, those graphs keep the fixed coordinates the construction pass assigned, which is a single tall column of nodes: readable only once the material is materialized or cooked. On (the default), what you see after a save matches what the generated asset will look like. Costs one placement pass per compile; graphs at or above the large-graph threshold skip it either way."))
	bool bLayoutInMemoryGraphs = true;

	UPROPERTY(Config, EditAnywhere, Category="Compiler")
	bool bAutoCompileOnSave = true;

	UPROPERTY(Config, EditAnywhere, Category="Compiler", meta=(ClampMin="0.05", ClampMax="10.0", UIMin="0.05", UIMax="2.0"))
	float SaveDebounceSeconds = 0.25f;

	UPROPERTY(Config, EditAnywhere, Category="Compiler")
	bool bVerboseLogs = false;

	UPROPERTY(Config, EditAnywhere, Category="Decompiler")
	bool bExportDecompiledLayout = true;
	
	UPROPERTY(Config, EditAnywhere, Category="Editor")
	bool bOpenInNewWindow = true;

	UPROPERTY(Config, EditAnywhere, Category="Editor",
		meta=(DisplayName="Material Instance Subfolder",
			ToolTip="Subfolder, relative to the parent material's folder, where the Material Content Browser creates new material instances. Leave empty to create them alongside the parent."))
	FString InstanceSubfolder = TEXT("Instances");

private:
	static FString NormalizeShadingModelKey(const FString& InName);
	static FString NormalizeBlendModeKey(const FString& InName);
	static FString NormalizeMaterialDomainKey(const FString& InName);
};
