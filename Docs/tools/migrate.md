# `dsc migrate` — 1.x sources to `.dss`

> [DreamShader](../index.md) » [Tools](index.md) » **Migrate**

`dsc migrate` rewrites `.dsm`, `.dsf` and `.dsh` files as DreamShaderLang 2.0. Nothing forces a
migration: the legacy front end keeps building 1.x sources through the same compiler. Migrate when
you want what only a `.dss` has — real expressions and control flow, `#include`, material instances
beside the material, comments that survive tooling.

```powershell
./dsc.ps1 migrate DShader/Materials/M_Foo.dsm -Check    # prove the rewrite, write nothing
./dsc.ps1 migrate DShader/Materials/M_Foo.dsm           # write M_Foo.dss, move M_Foo.dsm away
./dsc.ps1 migrate -All -Check                           # every 1.x source of the writable roots
./dsc.ps1 migrate -Root MoonToon -Check                 # one source root, by its or its plugin's name
```

| Switch | |
| :-- | :-- |
| `<file>` | one 1.x source. A header it imports is migrated with it only when nothing else includes that header. |
| `-All` | every 1.x source under the **writable** source roots (the project's). A plugin's root is left out. |
| `-Root <name>` | every 1.x source of one root, named by the root's display name or its plugin's name. |
| `-Check` | do everything except write. The exit code says whether every file would migrate. |
| `-DryRun` | report what would be written; no proofs are skipped. |
| `-Out <dir>` | write the `.dss` files under this directory, mirroring the source tree, and leave the 1.x files alone. |
| `-NoBackup` | delete the 1.x file instead of moving it to `Saved/DreamShader/Migrated/`. |

In a set, headers go first, and a header is migrated only when every file that includes it is
migrated in the same run: a `.dsh` may hold both dialects, so a half-migrated project keeps building.

## What is proved before anything is written

1. **No comment is lost.** The comments of the source are counted against the comments of the result
   (`DSH9092`).
2. **The text builds as 2.0.** It is parsed, bound and lowered by the 2.0 front end (`DSH9097`). A
   rejected text is kept under `Saved/DreamShader/Migrated/Rejected/`, and the diagnostics point into it.
3. **It builds the same graph.** The IR of the 1.x file and the IR of the new text are compared from
   their roots (`DSH9098`, a warning with the first differing node). What is one asset written two
   ways does not count: a literal on a pin against its `Const*` twin, an identity swizzle, the
   spacing of Custom node code, `\r\n` against `\n` in a description, a number at float precision.
4. **The asset stays where it is.** If the new file's place would name another asset path, the
   export gets the `/// @name` that keeps it (`DSH9098` says so when it cannot).

A source with `#if` lines is refused (`DSH9090`): only the branch taken today would survive.

Measured on the four source roots of the development project (71 files): all 71 build as 2.0, 68
compare equal, and the three warnings are true differences — two 1.x decompiler artefacts that
declared a node narrower than it is, and one explicit `.x` into a float pin.

## What the rewrite does

The legacy front end already reads every 1.x construct into the 2.0 tree. What is left to write out
is what only the **legacy rules** make work, because a `.dss` is bound without them. Each rule is also
a diagnostic on the 1.x source, so `dsc check` on a `.dsm` shows what a migration will change.

| 1.x | `.dss` | Rule |
| :-- | :-- | :-- |
| `import "x.dsh";` | `#include "x.dsh"` | |
| `Shader(Name = "...") { Properties / Settings / Outputs / Graph }` | `#pragma material(...)`, `uniform`s with `///` directives, one `export void M(inout material Base)` | |
| `ShaderFunction` inputs and outputs | parameters; a first output named `Result` is the return value, the others are `out` | |
| `Function` / `GraphFunction` | `/// @custom`, body verbatim; `UE.` calls in a GraphFunction body are respelled in place | L8 |
| `Namespace(Name = "N") { Function F }` | `N_F`, with `/// @name N::F` so the Custom node keeps its title; `N::F(` calls follow | |
| `VirtualFunction` | `/// @asset` + `extern` prototype | |
| `mix`, `fract`, `mod`, `vec3` | `lerp`, `frac`, `fmod`, `float3` | L2 (`DSH5277`) |
| `F(a).Out`, `F(a, Output = "Out")`, `[k]` on a function | one call statement with a local per output, shared by equal calls | L3b |
| `x = UE.Node()` with several outputs | `x = UE.Node().FirstOutput` | L3c (`DSH5287`) |
| `F(a, b, R, O)` with `R` the return value | `R = F(a, b, O)`; an undeclared receiver is declared | L5 (`DSH5283`) |
| `opt float S;` | `float S = 0.0` | L7 |
| `SAMPLERTYPE_Normal`, `PPI_SceneColor` | `Normal`, `SceneColor` | L12 (`DSH5278`) |
| a name in another case | the declaration's spelling | L19 (`DSH5275`, `DSH5276`) |
| a `float4` into a `float3` place | `.rgb` | L22 (`DSH5289`) |
| a block named like a function it can call | the block's function is `X_Asset`, `/// @name` keeps the asset | L23 (`DSH5290`) |
| `F(a, default, c)` | `F(a, C = c)` | L25 |
| `x = value;` with `x` declared nowhere | `T x = value;` | L26 (`DSH5292`) |
| `Description = "a\r\nb"` | the doc block's free text, one `///` line per line | |
| a property without `SortPriority` | `/// @sort 32`, the priority 1.x left it at | |
| `Backend = "Instance"` / `""` | `ThinCustom` / `Graph` | |

Comments stay with the statement or declaration they stood by; the comment above a 1.x block goes
above the first declaration that block became.

## What can stop a file

| | |
| :-- | :-- |
| `DSH9090` | the source uses `#if` |
| `DSH9091` | an `import` names a source root in front of its path |
| `DSH9097` | the migrated text does not build: a construct the migrator does not handle, or a 1.x leniency with no 2.0 spelling. Fix the 1.x source, migrate again. |
| `DSH9099` | the `.dss` already exists, or a file could not be written or moved. The 1.x file is never deleted unless the `.dss` was written. |

## See also

- [The 2.0 language](../language-v2/index.md) · [Commandlet](commandlet.md) · [Decompiler](decompiler.md)
- [Diagnostics `DSH9090`–`DSH9099`](../diagnostics/DSH9xxx.md)
