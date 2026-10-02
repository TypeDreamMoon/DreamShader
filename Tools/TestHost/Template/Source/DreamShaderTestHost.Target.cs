// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/*
 * The game target of the DreamShader test host.
 *
 * Nothing builds this one: every DreamShader automation test carries EditorContext, so the runner only
 * ever builds DreamShaderTestHostEditor. It is here so the host is an ordinary, complete project --
 * project files generate, and a packaged-game check (the Custom Pass global shaders are cooked into the
 * global shader cache, and only a cooked run proves they are there) can be added without touching the
 * template again.
 *
 * The settings are DevProject's (DevProject.Target.cs): the host has to build under exactly the rules the
 * project it stands in for builds under.
 */
public class DreamShaderTestHostTarget : TargetRules
{
	public DreamShaderTestHostTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("DreamShaderTestHost");
	}
}
