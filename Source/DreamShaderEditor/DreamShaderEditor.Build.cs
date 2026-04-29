using UnrealBuildTool;

public class DreamShaderEditor : ModuleRules
{
	public DreamShaderEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"ApplicationCore",
				"AssetRegistry",
				"ContentBrowser",
				"Core",
				"CoreUObject",
				"DirectoryWatcher",
				"DreamShader",
				"Engine",
				"Json",
				"MaterialEditor",
				"Projects",
				"RHI",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd"
			});
	}
}
