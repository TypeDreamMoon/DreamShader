using UnrealBuildTool;

public class DreamShaderCompiler : ModuleRules
{
	public DreamShaderCompiler(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// An Editor-type module since the compiler relocation (DreamShader.uplugin): the 2.0 pipeline, the IR
		// emitter and the asset layer under them, moved down from DreamShaderEditor.
		//
		// Public, because the public headers expose them: UMaterial* / EMaterialProperty /
		// ECustomMaterialOutputType (Engine); FTextShaderDefinition, DREAMSHADER_WITH_MOON_ENGINE and the
		// compiler interface (DreamShader); FLangDiagnosticSink / FModule / FIRModule (DreamShaderLang).
		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"DreamShader",
				"DreamShaderLang",
				"Engine"
			});

		// Private: what the moved files include (research-relocation section 4.2). AssetRegistry
		// (AssetRegistry/AssetRegistryModule.h), AssetTools (AssetViewUtils.h), Json (the diagnostics wire
		// JSON), MaterialEditor (MaterialEditingLibrary.h), Projects (Interfaces/IPluginManager.h) and
		// UnrealEd (Editor.h, Factories/MaterialFactoryNew.h, ObjectTools.h, FileHelpers.h, MaterialGraph/*).
		// UnrealEd also keeps the shared PCH these files compiled with inside DreamShaderEditor.
		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"AssetRegistry",
				"AssetTools",
				"Json",
				"MaterialEditor",
				"Projects",
				"UnrealEd"
			});
	}
}
