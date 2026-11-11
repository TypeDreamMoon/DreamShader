# AI agents

> [DreamShader](../index.md) » [Tools](index.md) » **AI agents**

How a coding agent writes DreamShaderLang in your project and checks its own work: the agent guide, the six
skills, how to publish them into a project, the optional MCP server, and the loop that works.

| | |
| :-- | :-- |
| Agent guide | [`AGENTS.md`](https://github.com/TypeDreamMoon/DreamShader/blob/main/AGENTS.md) at the repository root; `CLAUDE.md` points to it |
| Skills | `Plugins/DreamShader/.skill/<skill>/SKILL.md` — plain Markdown |
| Driver | `Plugins/DreamShader/.skill/dsc.ps1` — the headless [commandlet](commandlet.md), with a clean verdict |
| Shell | PowerShell 7 (`pwsh`) |

DreamShaderLang is not in any model's training data. An agent that writes it from memory guesses HLSL or GLSL
habits, and the guesses fail. What makes an agent useful here is the loop: read the reference, write, compile
headlessly, read the diagnostics, fix.

## The agent guide

`AGENTS.md` is the entry point for any agent, with or without skills. It has two halves: authoring sources in a
game project, and changing the plugin itself. Tell your agent to read it first. Claude Code reads it through
`CLAUDE.md`; other tools that read `AGENTS.md` find it by name.

## Skills

| Skill | Use it when |
| :-- | :-- |
| `dream-shader-create` | writing a new material, function, instance or Custom Pass pipeline from a description |
| `dream-shader-verify` | checking or compiling one source or the whole `DShader/` tree |
| `dream-shader-diagnose` | a build failed with a `DSHnnnn` code, or the result looks wrong |
| `dream-shader-decompile` | turning an existing material, function, instance or pipeline asset into source |
| `dream-shader-optimize` | making decompiled or migrated source read like hand-written source |
| `dream-shader-migrate` | rewriting 1.x `.dsm` / `.dsf` / `.dsh` as 2.0 `.dss` |

Each skill runs `dsc.ps1` and links into `.skill/reference/` — `dss.md` for `.dss` and `.dsi`, `dsp.md` for
`.dsp`, `legacy.md` for 1.x sources. The full verb list of the driver is in
[`.skill/README.md`](https://github.com/TypeDreamMoon/DreamShader/blob/main/.skill/README.md).

## Publishing the skills into a project

Claude Code loads skills from `.claude/skills/` at or above the folder the agent works in. The plugin keeps them
in `.skill/`, so publish them once, from the project root:

```powershell
pwsh -File Plugins/DreamShader/.skill/sync-skills.ps1
```

The script copies each skill into `<project>/.claude/skills/` and rewrites its links for the new location.

| Flag | Effect |
| :-- | :-- |
| *(none)* | publish to the nearest `.uproject` above the plugin |
| `-Target <dir>` | publish to another `.claude/skills` directory |
| `-Check` | compare and write nothing; exit `1` when the published copy is stale |
| `-Prune` | also remove published files whose source is gone |

> [!IMPORTANT]
> **The published copy does not update itself.** After you update the plugin, run
> `sync-skills.ps1 -Check`, and run `sync-skills.ps1` again when it exits `1`. An agent that reads a stale
> skill writes stale syntax.

Agents other than Claude Code do not load skills by themselves. Point them at the `SKILL.md` they need, or at
`AGENTS.md`; the commands are the same.

## The MCP server (optional)

[dream-mcp](https://github.com/TypeDreamMoon/dream-mcp) is a separate MCP server for DreamShader and DreamFX.
When an editor is open on the project, it sends builds to that editor instead of starting a commandlet, which
is faster and avoids two processes writing the same packages. It also answers material-expression signatures
from disk. Install and register it as its README says. Without it, the skills and `dsc.ps1` work on their own.

## The loop

Every `dsc.ps1` run starts the engine and takes tens of seconds, and a run over many files costs about the same
as one. So write first, then check in one run.

1. Read the reference for the kind of file: `.skill/reference/dss.md`, `dsp.md` or `legacy.md`.
2. Write the sources under `DShader/`. Mirror the folders and names the project already uses.
3. Check, which writes nothing:

   ```powershell
   pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check -All
   ```

4. Look up each `DSHnnnn` code in the [diagnostics](../diagnostics/index.md), fix, and check again.
5. Compile each new file once, and let the run delete what it created:

   ```powershell
   pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/UI/M_Panel.dss -Force -CleanNew
   ```

6. For a `/// @custom` body or a `.dsp` with HLSL passes, also run `check <file> -Shaders`. It is the only step
   that compiles that HLSL, and it saves assets.
7. Open the material in the editor to look at it. A headless run cannot judge how it looks.

> [!WARNING]
> **Never let an agent run `compile -All` in your project.** It rebuilds the sources of every plugin into their
> `Content/` folders and can overwrite assets you did not mean to touch. `check -All` is safe.

## When the agent gets it wrong

Open an issue with the **AI wrote it wrong / AI 写错了** form. Give the tool and model, the prompt, and the
source it wrote. These reports improve `AGENTS.md`, the skills and the examples. For bugs in the plugin itself,
first run `pwsh -File Plugins/DreamShader/.skill/dsc.ps1 info` and paste its output into the bug form. See
[CONTRIBUTING.md](https://github.com/TypeDreamMoon/DreamShader/blob/main/CONTRIBUTING.md).

## See also

- [Commandlet](commandlet.md) — every `-run=DreamShader` verb and flag behind `dsc.ps1`
- [Diagnostics index](../diagnostics/index.md) — every code
- [DreamShaderLang 2.0](../language-v2/index.md) — the language reference
- [Contributing](../contributing/index.md) — changing the plugin itself
