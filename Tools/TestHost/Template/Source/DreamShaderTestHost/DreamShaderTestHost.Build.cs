// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

using UnrealBuildTool;

/*
 * The test host's only module, and it does nothing.
 *
 * A project needs a primary game module to be a code project, and a code project is what gives the host
 * its own DreamShaderTestHostEditor target -- which is what builds the DreamShader plugin sitting in
 * Plugins/. Everything the tests need comes from the plugin itself, so this module depends on no more
 * than a module has to: a dependency here would be a second, silent way for the plugin's own
 * dependencies to be satisfied, and the host exists partly to prove the plugin declares everything it uses.
 */
public class DreamShaderTestHost : ModuleRules
{
	public DreamShaderTestHost(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
