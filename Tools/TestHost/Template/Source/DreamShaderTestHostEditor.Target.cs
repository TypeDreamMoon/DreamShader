// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/*
 * The editor target of the DreamShader test host -- the one the test runner builds and launches.
 *
 * SHARED BUILD ENVIRONMENT is the point of this file. An editor target is modular, and a modular target
 * shares the engine's build products: the engine modules already compiled into Engine/Binaries/Win64 are
 * linked against as they are, and only this project's module and its project plugin (DreamShader, whose
 * Binaries and Intermediate live in its own worktree) are compiled. That is the default already; it is
 * written out so that nobody "fixes" it to Unique, which would compile the entire engine a second time.
 *
 * The other settings are DevProject's (DevProjectEditor.Target.cs) line for line, so the two projects agree
 * on every engine module being up to date and neither rebuilds the other's.
 */
public class DreamShaderTestHostEditorTarget : TargetRules
{
	public DreamShaderTestHostEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		BuildEnvironment = TargetBuildEnvironment.Shared;
		ExtraModuleNames.Add("DreamShaderTestHost");
	}
}
