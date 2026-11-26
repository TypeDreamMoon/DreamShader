---
title: Project Layout
description: How DreamSource/Shader/ is organized, what each of the six source kinds holds, how a plugin contributes a source root of its own, which directories the plugin owns, and the package rules that decide what gets compiled.
---

DreamShaderLang sources live in one directory under the project root — `DreamSource/Shader` by default, set by
*Project Settings ▸ Dream Source ▸ Shader ▸ Source Directory* (`Config/DefaultDreamSource.ini`). A relative path
resolves against the project directory. Every Dream plugin keeps its sources side by side under `DreamSource/`.

A project from before the DreamSource layout keeps its sources in `DShader/`, which is still read while
`DreamSource/Shader/` does not exist; the editor offers to move it — see
[DreamSource layout](../settings/project.md#dreamsource-layout).

The source directory, its `Packages` subfolder and the generated-shader directory are created by the plugin at module
startup — the first time the editor starts, whether or not you have written any source.

(since 1.6.0) It is no longer the only root: **every enabled plugin with a `DreamSource/Shader` folder (or a legacy
`DShader` folder) contributes a source root of its own** — see [Plugin source roots](#plugin-source-roots) below.

## Six kinds of source file

The folders are yours to choose; the extension is what matters.

| Extension | What it is | Produces |
| --- | --- | --- |
| `.dss` (since 2.0.0) | a DreamShaderLang 2.0 compilation unit: a material **or** material functions (layers and blends included) | one asset per `export` |
| `.dsi` (since 2.0.0) | a material instance: the parent and the overrides | one `UMaterialInstanceConstant` |
| `.dsp` (since 2.1.0) | a Custom Pass pipeline (UE 5.8) | one `UDreamPassPipeline` |
| `.dsm` | a 1.x material | its `Shader`, plus the function and layer assets declared beside it |
| `.dsf` | a 1.x material function file | the function assets it declares |
| `.dsh` | a header: helpers, `VirtualFunction` / `extern` declarations, shared constants; holds both dialects | nothing — it enters a compile only through a file that imports or includes it |

Write new sources as `.dss`; a `.dsm` / `.dsf` is the 1.x syntax, which still builds through the same compiler, so a
project can migrate one file at a time with [`dsc migrate`](../tools/migrate.md). What each kind may contain is
on [File model](../language/source-files.md).

## The tree

```text
MyProject/
├─ DreamSource/
│  └─ Shader/
│     ├─ Materials/
│     │  ├─ M_Sample.dss
│     │  └─ M_Legacy.dsm
│     ├─ Functions/
│     │  ├─ F_Tint.dss
│     │  └─ F_Old.dsf
│     ├─ Instances/
│     │  └─ MI_Sample_Red.dsi
│     ├─ Passes/
│     │  ├─ P_Outline.dsp
│     │  └─ M_OutlineComposite.dss
│     ├─ Shared/
│     │  ├─ Common.dsh
│     │  └─ Color.dsh
│     ├─ VirtualFunctions/
│     │  └─ Engine.dsh
│     ├─ Decompiled/
│     │  ├─ Materials/
│     │  └─ Functions/
│     ├─ Packages/
│     │  └─ @typedreammoon/
│     │     └─ dream-noise/
│     │        ├─ dreamshader.package.json
│     │        └─ Library/
│     │           └─ Noise.dsh
│     ├─ .dreampass/
│     ├─ DreamShader.code-workspace
│     └─ dreamshader.lock.json
├─ Intermediate/
│  └─ DreamSource/
│     ├─ shader.db
│     └─ Shader/
│        ├─ Bridge/
│        ├─ Preview/
│        └─ GeneratedShaders/
└─ Plugins/
   └─ DreamShader/
```

## Plugin source roots

(since 1.6.0)

A plugin can ship the sources that build its materials instead of parking them in the project's tree.

```text
Plugins/
└─ MoonToon/
   ├─ DreamSource/
   │  └─ Shader/
   │     ├─ Materials/
   │     │  └─ M_Toon.dss
   │     ├─ Shared/
   │     │  └─ Toon.dsh
   │     └─ Packages/
   └─ MoonToon.uplugin
```

| Behaviour | Rule |
| :-- | :-- |
| Discovery | No configuration: `<Plugin>/DreamSource/Shader`, else a legacy `<Plugin>/DShader` (logged as deprecated; never both). The switch is *Project Settings ▸ … ▸ Paths ▸ **Scan Plugin Source Directories*** (on by default) |
| Default asset target | A file under a plugin root generates into **its own plugin's mount point** — `/MoonToon`, not `/Game`. In a 1.x file only an absent or all-whitespace `Root` is defaulted, so `Root="/"` opts back into `/Game` |
| Imports | **A relative path never crosses roots.** A file resolves against its own root and that root's `Packages` only; to use a header in another root, write a path starting with `/` (its own root first, then the others), copy it, or ship it as a package — see [Imports](../language/import.md#crossing-a-root-deliberately) |
| Writability | **Only the project root is writable.** VirtualFunction sync skips files under plugin roots, and *Adopt* is disabled for a source a plugin ships: a plugin ships its definitions as authored |
| Watchers | One watch per root, so a plugin's sources get *Auto Compile On Save* too |
| Workspace | `DreamShader.code-workspace` has one `folders` entry per root — `"."` for the project, `Plugin: <Name>` for a plugin |
| Material Content Browser | The **Root** column names *Project* or the owning plugin, and the search box matches root names — typing a plugin's name filters to everything it ships |
| Cook | A failure under another plugin's own sources (a `Root` naming an engine or marketplace plugin, `DSH8090`) no longer aborts the host project's cook; it is logged as `[Cook] Not this project's to build` (since 2.1.2). The project's own sources still abort it |

**Warning:**

A plugin source folder that **overlaps** an existing root is ignored with a warning rather than giving one file two
owners. That happens when *Source Directory* points into a plugin folder, or at a folder that contains one.

**Note:**

The root list is cached and rebuilt only when *Source Directory* or the scan switch changes, or on a refresh in the
Material Content Browser — which is how to pick up a plugin source folder you just created or a plugin mounted mid-session.
But **watchers are registered once, at startup**, so *Auto Compile On Save* for a root that appeared mid-session starts
working on the next editor start.

## Directory roles

| Directory | Owner | Contents |
| --- | --- | --- |
| `DreamSource/Shader/` | you | The source root. Organize it however you like; the layout here is a convention, not a rule. |
| `DreamSource/Shader/Materials/` | you | Material sources, `.dss` or `.dsm`, usually one material per file. |
| `DreamSource/Shader/Functions/` | you | Material functions: `.dss` with exported functions, or 1.x `.dsf`. |
| `DreamSource/Shader/Instances/` | you | `.dsi` material instances. |
| `DreamSource/Shader/Shared/` | you | Project-internal `.dsh` headers. |
| `DreamSource/Shader/VirtualFunctions/` | editor | Where *CreateVirtualFunction* writes declarations for existing `UMaterialFunction`s. |
| `DreamSource/Shader/Decompiled/` | editor | Output of *Export .dss* / *Export .dsi* / *Export Legacy .dsm / .dsf*, split into `Materials`, `Functions`, `Layers`, `LayerBlends`. |
| `DreamSource/Shader/Packages/` | extension | Installed shared libraries. Always the literal subfolder `Packages` of the source root; not configurable on its own. |
| `DreamSource/Shader/.dreampass/` (since 2.1.0) | plugin | Snapshots of Custom Pass HLSL; only those that pass the pre-check reach the global shader slots. Commit it with the sources. |
| `DreamSource/Shader/DreamShader.code-workspace` | plugin | Rewritten whole on every *Open Dream Shader Workspace*. |
| `DreamSource/Shader/dreamshader.lock.json` | extension | Records installed package versions. The plugin neither reads nor writes it. |
| `Intermediate/DreamSource/Shader/GeneratedShaders/` | plugin | Mapped to `/DreamShaderGenerated`. (since 2.0.0) A `Function`'s HLSL is written into its Custom node, so no compile writes here; a `.ush` found here was left by a 1.x build, and *Clean Generated Shaders* deletes it. |
| `Intermediate/DreamSource/shader.db` | plugin | Diagnostics, completion manifests and the symbol index for editor extensions; rebuilt by the editor — see [Bridge](../tools/bridge.md#shaderdb). |
| `Intermediate/DreamSource/Shader/Bridge/`, `…/Preview/` | plugin | Bridge request and response files, `status.json`, `owner.lock`; one-shot preview images. |

**Warning:**

`DreamShader.code-workspace` is serialized from a fixed object on every invocation — never read, merged or preserved.
Hand-added `launch`, `tasks`, `extensions` or extra `settings` are destroyed. Keep personal configuration in
`DreamSource/Shader/.vscode/settings.json`, which the command never touches.

## Naming conventions

| Kind | Convention |
| --- | --- |
| Materials | `M_*.dss` / `M_*.dsm` |
| Material functions | `F_*.dss` / `F_*.dsf`; layers `L_*`, blends `LB_*` |
| Material instances | `MI_*.dsi` |
| Custom Pass pipelines | `.dsp`, kept beside their pass materials |
| Shared headers | named by domain — `Texture.dsh`, `Color.dsh` |
| Package entry points | `Library/<Name>.dsh` |

A `.dss` asset is named after its `export` function, so keeping the file name and the `export` name the same saves
surprises. Extension matching is case-insensitive, so `M_Water.DSM` is a material file too. Identifiers are another
matter: the plugin replaces non-ASCII characters when it sanitizes namespace and function names into HLSL and asset
identifiers, so keep those names ASCII.

## Imports and includes

Keep each material's imports to a few stable entry points:

```c
// 1.x
import "Shared/Common.dsh";
import "@typedreammoon/dream-noise/Library/Noise.dsh";
```

```hlsl
// .dss
#include "Shared/Common.dsh"
#include "@typedreammoon/dream-noise/Library/Noise.dsh"
```

A specifier is tried against three candidates: the importing file's directory, the **owning root**'s source directory,
and that root's `Packages` directory. The first that exists **and stays inside its containment root** wins — a file in a
package cannot `../` its way out. A specifier without an extension gets `.dsh` appended; only a header can be brought in
(importing a `.dsf` / `.dsm` is `DSH2252`). One that resolves nowhere is `DSH8292`. The full rules are on
[Imports and namespaces](../language/import.md).

Shallow imports pay off in another way: the build key covers every header a file brings in, so editing one header
invalidates every file that includes it.

## What is compiled, and what is not

Every build path takes the same files *(after 2.1.2)* — the startup sweep, a save and the dependents it
queues, *Recompile All*, the commandlet's `compile -All`, the cook and the Material Content Browser:

| Where | What is built |
| --- | --- |
| a source root, outside its `Packages` | every `.dss`, `.dsi`, `.dsp`, `.dsm` and `.dsf`; a `.dsh` only through the sources that include it |
| a package's `Assets/` folder | its `.dss`, `.dsf` and `.dsi`, into the package's asset root (`/Game/DreamShaderPackages/<scope>/<name>`) |
| a package header that asks for material functions — `/// @generate`, `#pragma generate`, a 1.x `Generate =` | those material functions |
| anything else in a package — `Examples/`, a `.dss` or `.dsf` outside `Assets` | nothing, on any path |

To use an example, copy it from `DreamSource/Shader/Packages` to somewhere under `DreamSource/Shader/`. Through 2.1.2 a package's
`.dsf`, `.dss`, `.dsi` or `.dsp` outside `Assets` was compiled by the file watcher and *Recompile DSM* but by none
of `compile -All`, the cook and the Material Content Browser; move such sources under `Assets`. See
[Packages ▸ Package assets](../tools/packages.md#package-assets).

**Note:**

A source marked `NoGeneration` (`#pragma nogeneration`, `/// @nogeneration`, a 1.x `NoGeneration = true`) is
enumerated like any other, but only a compile that names it builds its assets — see
[NoGeneration](../generation/no-generation.md).

A package `.dsh` header can be imported, but it never takes part in the `VirtualFunction` declaration scan — a
`VirtualFunction` shipped in a package is never checked against its asset.

## Source control

| Content | Commit? |
| --- | --- |
| Every source — `.dss`, `.dsi`, `.dsp`, `.dsh`, `.dsm`, `.dsf` | yes — this is the material logic |
| `DreamSource/Shader/.dreampass/` | yes, when you use Custom Pass — the snapshots of HLSL passes |
| `dreamshader.lock.json` | yes, if the team installs packages |
| `Config/DefaultEngine.ini`, `Config/DefaultDreamSource.ini` | yes — the project settings live here and are shared |
| `DreamSource/Shader/Packages/` | team's choice: commit it, or reinstall from the lock file |
| Generated `.uasset`s | either answer works — commit only the sources and regenerate in CI, or commit the generated assets too; see [Source control](../generation/source-control.md) |
| `Intermediate/DreamSource/` (and a leftover `Intermediate/DreamShader/`) | no |

Agree on one *Source Directory* across the team so imports resolve the same on every machine. The build key records
sources by their project-relative path (since 2.1.1), so a second checkout does not rebuild every asset. A plugin's
`DreamSource/Shader` folder is committed with the plugin — it is part of the same deliverable as the plugin's `Content/`.

## Next

- [File model](../language/source-files.md) — what each 1.x extension may contain
- [DreamShaderLang 2.0](../language-v2/index.md) — `.dss`, `.dsi` and `.dsp`
- [Imports and namespaces](../language/import.md) — specifier syntax and cycle handling
- [Packages](../tools/packages.md) — the install root, and the half the plugin does not implement
- [Daily workflow](workflow.md) — what one save actually triggers
