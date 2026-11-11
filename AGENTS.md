# DreamShader — agent guide

DreamShader is an Unreal Engine plugin that builds materials from text. The language is DreamShaderLang:
`.dss` (materials and material functions), `.dsi` (material instances), `.dsp` (Custom Pass pipelines, UE 5.8 only),
`.dsh` (headers), and the 1.x `.dsm` / `.dsf`. A compile produces ordinary `UMaterial`, `UMaterialFunction` and
`UMaterialInstanceConstant` assets, and for a `.dsp` a pipeline asset.

Two kinds of work happen here. Pick the section that matches yours.

- **Authoring `.dss` / `.dsi` / `.dsp` / `.dsm` / `.dsf` in a game project** that has this plugin installed → [Authoring](#authoring).
- **Changing the plugin itself** (C++, tools, docs) → [Changing the plugin](#changing-the-plugin).

## Authoring

DreamShaderLang is not in your training data. It looks like HLSL but is not HLSL. Read the reference first,
copy from its examples, and do not guess syntax from other shader languages.

Paths in this section are relative to the game project root. The plugin is at `Plugins/DreamShader/`.

1. Read: `Plugins/DreamShader/.skill/reference/dss.md` (`.dss`, `.dsi`), `Plugins/DreamShader/.skill/reference/dsp.md`
   (`.dsp`), `Plugins/DreamShader/.skill/reference/legacy.md` (`.dsm`, `.dsf`, `.dsh` in 1.x).
2. Look up what exists: `Plugins/DreamShader/Docs/builtins/index.md` lists the builtin calls. For the exact
   node catalog of this engine, run
   `pwsh -File Plugins/DreamShader/.skill/dsc.ps1 export-catalog`; it writes
   `Saved/DreamShader/Bridge/dreamshader-builtin-catalog.json`.
3. Write the source under `DShader/`. The asset path follows the file: `DShader/UI/M_Panel.dss` builds `/Game/UI/M_Panel`.
4. Build it. First the fast check, which writes nothing, then the real compile:
   `pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check DShader/UI/M_Panel.dss`
   `pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/UI/M_Panel.dss -Force -CleanNew`
5. Read the diagnostics. Every message has a code `DSHnnnn`; look it up in
   `Plugins/DreamShader/Docs/diagnostics/README.md`. Fix, and repeat until both commands print `dsc: OK (exit 0)`.

### Skills

The plugin ships six skills. Each `SKILL.md` is plain Markdown, so any agent can read it and run the same commands.

| Skill | Use it when |
| :-- | :-- |
| `dream-shader-create` | writing a new material, function, instance or Custom Pass pipeline |
| `dream-shader-verify` | checking or compiling one source or the whole `DShader/` tree |
| `dream-shader-diagnose` | a build failed, or the result looks wrong |
| `dream-shader-decompile` | turning an existing material asset into source |
| `dream-shader-optimize` | cleaning up decompiled or migrated source |
| `dream-shader-migrate` | rewriting 1.x `.dsm` / `.dsf` / `.dsh` as 2.0 `.dss` |

The source of each skill is `Plugins/DreamShader/.skill/<skill>/SKILL.md`. Claude Code loads skills from
`.claude/skills/`, so publish them once into the project:

`pwsh -File Plugins/DreamShader/.skill/sync-skills.ps1`

The published copy does not update itself. After the plugin is updated it may be stale. Check, and publish again if it is:

`pwsh -File Plugins/DreamShader/.skill/sync-skills.ps1 -Check`

### Rules

- Run `check` before `compile`. `check` writes no asset.
- Keep `-CleanNew` on every `compile`. It deletes the `.uasset` files the run created; a leftover file overrides
  the material the editor keeps in memory.
- Never run `compile -All` in a user's project. It rebuilds every source of every plugin and can overwrite assets
  you do not own. To check everything, run `check -All`.
- Every run starts the engine and takes tens of seconds. Write all files first, then run one `check -All`.
- `check` does not compile HLSL inside a `/// @custom` body. Run `check <file> -Shaders` for that; it saves assets.
- Use PowerShell, not Git Bash, for commands with an asset path such as `/Game/X`. Git Bash rewrites the path.
- A `.dsp` needs UE 5.8, and the materials its passes use must also be written as `.dss`.

## Changing the plugin

Paths in this section are relative to the repository root.

### Layout

| Module | Type | Does |
| :-- | :-- | :-- |
| `DreamShaderLang` | Runtime (`Core` only) | the language: 2.0 and 1.x front ends, binder, IR, printer |
| `DreamShader` | Runtime | settings, source roots, preprocessor defines, the compiler interface |
| `DreamShaderPass` | Runtime | the Custom Pass runtime (UE 5.8) |
| `DreamShaderCompiler` | Editor | turns sources into material assets |
| `DreamShaderEditor` | Editor | editor UI, bridge, decompiler, the `-run=DreamShader` commandlet, the automation tests |

Other folders: `.skill/` (skills, the `dsc.ps1` driver, build scripts), `Docs/` (the manual), `Tests/Corpus/`
(test fixtures), `Tools/` (test host, test runner, localization tools), `Shaders/` (plugin shader files).

### Build

Work in a test host, not in a project whose editor is open. Create one (it holds a git worktree of this repository):

`pwsh -NoProfile -File Tools/TestHost/New-DreamShaderTestHost.ps1 -Root <host dir> -Branch <branch>`

The test runner below builds the host's editor target before it runs. Details: `Tools/TestHost/README.md`.

### Test

Run this from the host's `Plugins/DreamShader/` before every pull request. It builds, then runs the whole
suite headlessly; the suite itself takes about a minute and a half:

`pwsh -NoProfile -File Tools/Tests/Invoke-DreamShaderTests.ps1 -Preset Suite`

The exit code is the number of unexpected failures. If you changed `.skill/dsc.ps1`, also run these two
(seconds, no engine):

`pwsh -NoProfile -File Tools/Tests/Test-DscAssetCleanup.ps1`
`pwsh -NoProfile -File Tools/Tests/Test-DscInfo.ps1`

Presets, filters and the RHI gate: `Docs/contributing/testing.md`.

### Rules

- Added, renamed or removed a `DSHnnnn` diagnostic: run `pwsh -File .skill/gen-diagnostics.ps1`. It rewrites only
  the generated half of `Docs/diagnostics/DSH*.md`. `-Check` reports drift without writing.
- Changed user-facing C++ text: run `pwsh -File Tools/Localization/localization_lint.ps1`. Text that reaches the
  bridge or `diagnostics.json` goes through `ToInvariantWireString()`.
- Version-dependent C++ uses the macros in `Source/DreamShader/Public/DreamShaderVersionCompat.h`, never raw
  `ENGINE_MAJOR_VERSION`. Custom Pass code tests `DREAMSHADER_WITH_CUSTOM_PASS`.
- Supported engines are UE 5.3–5.8. To prove a version gate, build on each engine:
  `pwsh -File .skill/build-plugin.ps1 -Engine <engine root>,<engine root>`.
- A user-visible change gets an entry under `## Unreleased` in `CHANGELOG.md`.
- Behavior changed: update the English page under `Docs/` in the same pull request. `Docs/zh-CN/` is translated
  by CI after merge.
- Edit skills in `.skill/`. A copy under `.claude/skills/` is generated.
- No absolute local paths, user names or e-mail addresses in any committed file.
- One topic per commit. Message style: `fix(scope): what changed`, as in `git log --oneline`.

### Where things are documented

- `CONTRIBUTING.md` — how to report bugs, propose features, and open pull requests
- `Docs/contributing/index.md` — modules, build rules, engine matrix, localization
- `Docs/contributing/testing.md` — the automation suite and the fixture corpus
- `Docs/contributing/release.md` — tags, the release workflow, the archive
- `Docs/tools/agents.md` — skills, publishing them, and the optional MCP server
- `Docs/tools/commandlet.md` — every `-run=DreamShader` verb and flag
- `Docs/language-v2/index.md` — the 2.0 language reference
- `.skill/README.md` — the `dsc.ps1` driver and its verbs
- <https://shader.toolchain.64hz.cn> — the published manual, English and Chinese

## Don't

- Don't run `compile -All` in a user's project.
- Don't hand-edit the generated half of `Docs/diagnostics/` or a published skill under `.claude/skills/`.
- Don't run the test suite on a project that has an editor open.
- Don't open a pull request without the result line of `Invoke-DreamShaderTests.ps1 -Preset Suite`.
- Don't write syntax you have not seen in the reference or in `Tests/Corpus/`.
