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

		// Private: what the moved files include. AssetRegistry
		// (AssetRegistry/AssetRegistryModule.h), AssetTools (AssetViewUtils.h), Json (the diagnostics wire
		// JSON), MaterialEditor (MaterialEditingLibrary.h), Projects (Interfaces/IPluginManager.h) and
		// UnrealEd (Editor.h, Factories/MaterialFactoryNew.h, ObjectTools.h, FileHelpers.h, MaterialGraph/*).
		// UnrealEd also keeps the shared PCH these files compiled with inside DreamShaderEditor.
		//
		// The `.dsp` half (Custom Pass pipelines): DreamShaderPass for the asset a `.dsp` compiles to, its
		// settings and its slot paths (DREAMSHADER_WITH_CUSTOM_PASS is a public define of it); RenderCore, RHI and
		// TargetPlatform for the HLSL slots -- the shader source mappings (AllShaderSourceDirectoryMappings), the
		// pre-check's in-process compile (ShaderCompilerCore.h: PreprocessShader / CompileShader), the global shader
		// types it finds by name, the shader platforms (RHIStrings.h, RHIGlobals.h) and the targeted shader formats
		// (Interfaces/ITargetPlatformManagerModule.h). Private: no public header of this module names them.
		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"AssetRegistry",
				"AssetTools",
				"DreamShaderPass",
				"Json",
				"MaterialEditor",
				"Projects",
				"RenderCore",
				"RHI",
				"TargetPlatform",
				"UnrealEd"
			});
	}
}
