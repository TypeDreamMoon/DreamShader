using System.IO;
using UnrealBuildTool;

public class DreamShaderEditor : ModuleRules
{
	public DreamShaderEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// MoonEngine extends EMaterialProperty with MP_MoonEncodedAttribute0..4. Detect it from the
		// engine headers so this module still builds against a stock Unreal Engine, where those
		// enumerators do not exist and the MOON_ENGINE blocks must stay compiled out.
		string SceneTypesHeader = Path.Combine(EngineDirectory, "Source", "Runtime", "Engine", "Public", "SceneTypes.h");
		if (File.Exists(SceneTypesHeader) && File.ReadAllText(SceneTypesHeader).Contains("MP_MoonEncodedAttribute0"))
		{
			PrivateDefinitions.Add("MOON_ENGINE=1");
		}

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"ApplicationCore",
				"AssetRegistry",
				"AssetTools",
				"ContentBrowser",
				"Core",
				"CoreUObject",
				"DesktopPlatform",
				"DirectoryWatcher",
				"DreamShader",
				"DreamShaderCompiler",
				"Engine",
				"InputCore",
				"Json",
				"MaterialEditor",
				"Projects",
				"RHI",
				"RenderCore",
				"Renderer",
				"Settings",
				"Slate",
				"SlateCore",
				"SQLiteCore",
				"ToolMenus",
				"ToolWidgets",
				"UnrealEd",
				"WebSocketNetworking",
				"WorkspaceMenuStructure"
			});
	}
}
