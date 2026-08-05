using System.IO;
using UnrealBuildTool;

public class DreamShader : ModuleRules
{
	public DreamShader(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"Projects",
				"RenderCore"
			});

		// Moon Engine adds material attributes (MP_MoonEncodedAttribute0..4) that stock Unreal does
		// not have, and DreamShader exposes them as MaterialAttributes members. Referencing those
		// enumerators unconditionally would stop the plugin building on a launcher engine, so the
		// support is compiled behind this define and every site that names them is guarded by it.
		//
		// Detected from the engine's own header rather than from a version or a folder name: it is
		// the enumerator itself that the code needs, the check therefore cannot drift from what it
		// is protecting, and it works for any fork that carries the attributes. Installed engines
		// ship these headers too, so the probe is valid there as well.
		//
		// PublicDefinitions, on the lowest module in the plugin: DreamShaderEditor and anything else
		// depending on DreamShader inherits one answer instead of re-detecting it.
		bool bHasMoonMaterialAttributes = false;
		string SceneTypesHeader = Path.Combine(
			EngineDirectory, "Source", "Runtime", "Engine", "Public", "SceneTypes.h");
		if (File.Exists(SceneTypesHeader))
		{
			bHasMoonMaterialAttributes = File.ReadAllText(SceneTypesHeader).Contains("MP_MoonEncodedAttribute0");
		}

		PublicDefinitions.Add("DREAMSHADER_WITH_MOON_ENGINE=" + (bHasMoonMaterialAttributes ? "1" : "0"));
	}
}
