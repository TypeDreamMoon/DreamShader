# DreamShader test host

A minimal Unreal project that exists only to build DreamShader and run its automation suite, away from any
project you actually work in.

## Why

- **The working project's editor stays open.** Building links the plugin's DLLs and a running editor holds
  them; a run also needs an editor process of its own on the project it tests.
- **Work in progress never reaches the working project.** The host's `Plugins/DreamShader` is a separate
  git worktree with its own `Binaries/` and `Intermediate/`, so a branch that does not compile yet never
  breaks the project whose materials DreamShader builds.
- **"The project builds" is not "the plugin builds".** The host enables nothing but DreamShader (and the two
  plugins its descriptor asks for), so a dependency the plugin forgot to declare shows up here.

## Layout

```
DreamShaderTestHost/
  DreamShaderTestHost.uproject           from Template/
  Config/DefaultEngine.ini               from Template/ -- the renderer settings are DevProject's, verbatim
  Config/DefaultGame.ini                 from Template/
  Source/DreamShaderTestHost*.Target.cs  from Template/
  Source/DreamShaderTestHost/            from Template/ -- an empty primary game module
  DShader/                               the host's DreamShader source root; tests write under it
  Plugins/DreamShader/                   a git worktree of the DreamShader repository
  .dreamshader-testhost.json             what the host was made from
  Binaries/ Intermediate/ Saved/         created by the first build and run
```

The template lives here, in `Tools/TestHost/Template/`. UnrealBuildTool looks for module and target rules
inside a plugin only under its `Source/` folder, so no project that has DreamShader in its `Plugins/` ever
picks the template's `.Build.cs` or `.Target.cs` files up.

## Creating it

```powershell
pwsh -NoProfile -File Tools\TestHost\New-DreamShaderTestHost.ps1 -Root <host dir> -Branch <branch> -WhatIf
pwsh -NoProfile -File Tools\TestHost\New-DreamShaderTestHost.ps1 -Root <host dir> -Branch <branch>
```

| Parameter | Default | |
| --- | --- | --- |
| `-Root` | `$env:DREAMSHADER_TEST_HOST` | The host directory |
| `-RepoPath` | the repository holding the script | Any working tree of the repository |
| `-Branch` | `main` | What the worktree must have, or is created with |
| `-StartPoint` | `HEAD` | Where `-Branch` starts when it does not exist yet |
| `-Detach` | off | Follow `-Branch` detached: a branch can be checked out in one worktree at a time, so this is how to test a branch the working project has checked out |
| `-EngineRoot` | the installed engine the `.uproject` associates with | |
| `-Force` | off | Replace template files that differ; each old one is kept as `<name>.bak-<timestamp>` |
| `-WhatIf` | off | Report only |

The script never removes, resets or switches an existing worktree; move it with git:

```powershell
git -C <host>\Plugins\DreamShader switch <branch>
git -C <host>\Plugins\DreamShader switch --detach <commit>
```

Exit codes: `0` the host matches the template; `1` a check failed; `2` usable, but it does not match the
template (files kept without `-Force`, or `-WhatIf` found work to do).

## Building and running

```powershell
& '<engine>\Engine\Build\BatchFiles\Build.bat' DreamShaderTestHostEditor Win64 Development "-Project=<host>\DreamShaderTestHost.uproject" -WaitMutex -NoHotReloadFromIDE -NoEngineChanges
pwsh -NoProfile -File <host>\Plugins\DreamShader\Tools\Tests\Invoke-DreamShaderTests.ps1 -Preset Suite
```

`Invoke-DreamShaderTests.ps1` builds the editor target itself unless `-NoBuild`, and has three presets:
`Suite` (everything, `-nullrhi`), `Fast` (the Core-only layers and the Custom Pass logic tests) and `Rhi`
(everything that renders, with a real RHI and no window). Reports land in
`<host>/Saved/DreamShaderTests/<preset>-<stamp>/`. `-WithoutCustomPass` builds and runs the plugin as an
engine before 5.8 would, with the Custom Pass renderer compiled out; the next build without it puts the
renderer back.

The editor target uses the **shared** build environment: the engine's own modules are linked as they are,
and only the host module and the plugin are compiled. Never build the game target casually: a game target
is monolithic and compiles its own copy of every engine module it uses.

## How the host differs from DevProject

| | DevProject | Host |
| --- | --- | --- |
| Plugins | DreamShader, DreamGUI and the project's own | DreamShader (and WebSocketNetworking, SQLiteCore, which it asks for) |
| Startup map | the engine's Open World template | `/Engine/Maps/Entry` |
| Content | the project's own | none; tests create and delete theirs under `/Game/DreamShaderTests` |
| Renderer settings | its own | the same, verbatim, so both read one derived data cache |
