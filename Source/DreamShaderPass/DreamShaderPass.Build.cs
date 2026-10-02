using System.IO;
using UnrealBuildTool;

// The Custom Pass runtime: the pipeline asset a `.dsp` compiles to, the subsystem that decides which pipelines
// apply to a view, and the scene view extension that runs their passes inside the renderer.
//
// UE 5.8 only, and gated here rather than in the descriptor, which has no per-module engine field. On an older
// engine the module still builds -- every reflected type in it is plain data and compiles anywhere, so the
// assets, settings and components load and save -- but DREAMSHADER_WITH_CUSTOM_PASS is 0 and everything that
// touches the renderer is compiled out. The define is public: DreamShaderCompiler and DreamShaderEditor ask the
// same question of it instead of re-deriving it.
public class DreamShaderPass : ModuleRules
{
	public DreamShaderPass(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bool bWithCustomPass = Target.Version.MajorVersion > 5
			|| (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 8);
		PublicDefinitions.Add("DREAMSHADER_WITH_CUSTOM_PASS=" + (bWithCustomPass ? "1" : "0"));

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"DreamShader",
				"Engine",
				"RenderCore",
				"RHI"
			});

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"Projects",
				"Renderer"
			});

		if (bWithCustomPass)
		{
			// FPostProcessingInputs -- the argument of PrePostProcessPass_RenderThread, the BeforePostProcess
			// injection point -- is declared in the Renderer's Internal folder. UBT hands Internal folders to modules
			// of the same rules scope only (UEBuildModule.cs), so an engine plugin would see it and this project
			// plugin does not; the path is added by hand. Only Render/DreamPassRendererInternal.cpp includes from it.
			PrivateIncludePaths.Add(Path.Combine(GetModuleDirectory("Renderer"), "Internal"));
		}
	}
}
