using System.IO;
using System.Linq;
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

		// -ProjectDefine:DREAMSHADER_FORCE_NO_CUSTOM_PASS on the UBT command line builds what an older engine builds --
		// the asset types without the renderer half -- on any engine: how a machine with 5.8 alone compiles the other
		// side of every gate. A command line change invalidates UBT's makefile, so switching needs nothing else. An
		// environment variable would not: UBT keeps the rules it evaluated last until a rules file changes, and 5.8
		// loads the makefile even under -NoUBTMakefiles.
		bool bForcedOff = Target.ProjectDefinitions.Any(Definition =>
			Definition == "DREAMSHADER_FORCE_NO_CUSTOM_PASS" || Definition == "DREAMSHADER_FORCE_NO_CUSTOM_PASS=1");
		bool bWithCustomPass = !bForcedOff
			&& (Target.Version.MajorVersion > 5 || (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 8));
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
