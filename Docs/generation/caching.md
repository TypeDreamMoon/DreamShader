# Caching

> [DreamShader](../index.md) » [Generation](index.md) » **Caching**

The source-hash short circuit: a stored fingerprint of the compiled text that lets an unchanged
source skip regeneration entirely.

| | |
| :-- | :-- |
| Kind | generation optimization |
| Hashed text | the **preprocessed** text of the file and of every `.dsh` it imports, plus the context that compiles it |
| Algorithm | `FCrc::StrCrc32`, formatted `%08x` — eight lowercase hex digits |
| Stored in | the generated asset's package metadata, keyed by the asset object |
| Bypassed by | the `bForce` flag on the generation entry points |

## Synopsis

```text
digest text    := for each imported header, in the order the include resolver first read it:
                    "// Begin DreamShader source: <absolute path>\n" <preprocessed header>
                    "\n// End DreamShader source: <absolute path>\n\n"
                  then the same block for the file itself
                  (a `.dsi` adds its resolved parent, a `.dsp` what it references)
build key      := "DSK3|Plugin=<version>|Engine=<major>.<minor>|" <settings> "Defines=<read defines>|"
                  "\n--\n" <digest text>
build key      ->  CRC32  ->  "%08x"  ->  DreamShader.SourceHash   e.g. "9f2c41ab"
source path    ->  project-relative, forward slashes  ->  DreamShader.SourceFile
```

## What is hashed

The hash covers the text the compile actually reads, **after** preprocessing — not the bytes of the
file on disk. *(since 2.0.0)* Nothing is inlined into the parse any more: each imported `.dsh` is
preprocessed and parsed on its own, and the build key hashes each header's text in its own
`// Begin DreamShader source: <path>` / `// End DreamShader source: <path>` block, then the file's,
including its `import` lines. A header that two imports reach is hashed once.

| Change | Changes the hash of |
| :-- | :-- |
| edit `M_Foo.dsm` | `M_Foo.dsm` |
| edit `Common.dsh`, imported by `M_Foo.dsm` and `M_Bar.dsm` | both `M_Foo.dsm` and `M_Bar.dsm` |
| move the project to another directory | **every** source — the blocks name each file by its absolute path. The stored `DreamShader.SourceFile` is project-relative and still matches |
| rename the source file | the stored path no longer matches, so nothing is skipped |
| reformat whitespace or edit a comment | the hash — the text is compared byte for byte, not semantically |
| change **Default Compiler Backend** | **every** source *(since 1.8.0)* |
| change a shading-model / blend-mode / domain **mapping** | **every** source *(since 1.8.0)* |
| upgrade the plugin, or the engine | **every** source *(since 1.8.0)* |
| change a [preprocessor define](../language/preprocessor.md) | every source that **read** that define *(since 1.9.0)* — including one that read it while it was undefined |

### It is a build key, not just a source hash

The stamp fingerprints the source **in the context that compiles it** *(since 1.8.0)*. Anything
that changes what a given source produces is folded in, because leaving it out makes the skip
check answer "still current" about an asset that is not:

| Input | Why it is in the key |
| :-- | :-- |
| the preprocessed text of the file and of every header it imports | the compile's actual input — which is why a changed `.dsh` needs nothing else here |
| [Default Compiler Backend](../settings/project.md) | decides whether a `Shader` block becomes a `UMaterial` or a thin instance |
| the mapping tables | decide what a `Settings` key resolves to |
| plugin version, plus a hand-bumped format tag — `DSK3` *(since 1.9.0)* | upgrading the generator invalidates what the old one wrote |
| engine version | what is generable moves with it (Substrate, for one) |
| the [preprocessor defines this source read](../language/preprocessor.md#rebuilds) *(since 1.9.0)* | they decide which branches of the source were compiled at all |

The defines are folded in **precisely**: only the names the file actually read, each with the value
it had, and an explicit `<undef>` sentinel for one that was read while nothing defined it. Folding
the whole table instead would rebuild the world every time any project touched any switch; omitting
the sentinel would be worse than either, because then *adding* a define would not move the hash and
the source would quietly start taking a different branch with its asset never rebuilt.

Two properties of that fold are load-bearing. It is **sorted** by name, case-sensitively, since
`TMap` iteration order is not stable and an unsorted fold would hash differently run to run — which
reads as "every asset is stale, every time". And it is **injective**: a naive `Name=Value;` join is
not, because a value may legitimately contain `=` and `;` (`-Define=A=1;B=2` is a valid define), so
`{A: "1;B=2"}` and `{A: "1", B: "2"}` would fold to the same string and one of them would silently
reuse the other's cached asset.

> [!NOTE]
> Before this, the key hashed the source text alone, so changing the backend left every already
> generated asset looking current — which is why that one setting had its own forced rebuild
> sweep bolted on. The sweep no longer forces: each affected asset now fails the skip check on
> its own, and — just as importantly — one the setting does *not* affect is still skipped instead
> of being needlessly rebuilt.
>
> Changing the composition of the key invalidates every existing stamp. That costs one rebuild
> per asset, once, and is the intended effect.

> [!NOTE]
> Editing a `.dsh` invalidates every source that imports it, but a header never generates anything
> by itself: handed to the compiler directly it is [`DSH8296`](../diagnostics/DSH8xxx.md#dsh8296).
> With **Auto Compile On Save** on, saving a header queues every source that imports it, directly or
> through another header, and those compiles find their keys moved. Otherwise the dependents are
> rebuilt when they are themselves compiled: on their own save, by the startup sweep or the sweep
> after a change to a setting the build key covers, the Material Content Browser's Compile button,
> the [commandlet](../tools/commandlet.md), or a cook.

When a batch contains both a file and one it depends on — a header it imports, the parent of a
`.dsi`, a material a `.dsp` names — the batch is **ordered so that dependencies compile first**
*(since 1.8.0)*. This is not a nicety: a material that calls a `ShaderFunction` binds its call node
against the live `UMaterialFunction` asset — the emitter reads the pins off the object, not off the
source — so compiling the caller first binds it against the *previous* version of that function's
interface. Inside one file the emitter keeps the same order, building a function another product of
the file calls before the caller; two that call each other are
[`DSH8299`](../diagnostics/DSH8xxx.md#dsh8299). Files in an import cycle are left in the order the
walk reached them; the compile rejects the cycle itself with
[`DSH4211`](../diagnostics/DSH4xxx.md#dsh4211).

## Custom Pass pipelines

*(since 2.1.0)* A [`.dsp`](../language-v2/passes.md) is keyed like any 2.0 source — its preprocessed text,
the defines it read, and every setting and version above — plus what its compile read outside the file,
because a pipeline whose text did not change still has to be rebuilt when one of those moves:

| Folded into a `.dsp`'s key | Moves when |
| :-- | :-- |
| the project's pass layer names, in bit order | a layer is added, removed, renamed or moved in *Project Settings ▸ DreamShader Custom Pass*: a `Layer(...)` filter is compiled to bits |
| whether the engine has the Custom Pass runtime | the project opens on an engine on the other side of 5.8 |
| per `Material = "..."`: the reference as written, the object path it resolved to, the build key that material was last built under (its `DreamShader.SourceHash`), and the facts the passes were checked against — its domain and blendable location, `bDisablePreExposureScale`, the `UE.UserSceneTexture` names it reads, the post-process inputs its `SceneTexture` nodes take, its `UE.DreamPassOutput` and which pins are connected, its usage flags, whether it asks for the new translator | the material is rebuilt from a changed source, appears, or changes one of those facts |
| per `Shader = "..."`: the reference, its virtual path, and a hash of the snapshot's inputs — the text and relative path of the file and of everything it includes by a relative path, and the text behind every include by a virtual path that is neither `/Engine/` nor `/Plugin/` | the `.usf`, or anything it includes, is edited |
| per file an `hlsl` block includes: its path from the `.dsp`'s folder and the same hash of its inputs — the HLSL written in the `.dsp` is in the key already, as the `.dsp`'s text | the included file, or anything it includes, is edited |

That is what lets the [bridge](../tools/bridge.md#custom-pass-shader-files) queue a `.dsp` unforced on every
`.usf` save and after every rebuild of a material it names: a change that moves none of these is skipped.
The shader formats a slot is pre-checked for are not in the key.

A matching key is not enough to skip a pipeline: the slot registry must also still hold every HLSL pass
of it, in the slot the asset records, with a snapshot that passed a pre-check and its files on disk, and
every exported buffer must have its render target. A registry somebody deleted, or a merge that lost a
slot, is rebuilt by the next compile rather than hidden behind an unchanged key.

A `.dss` that reads an exported buffer through [`UE.DreamPassBuffer`](../builtins/dream-pass.md#uedreampassbuffer)
folds in, per node, the pipeline's object path, the buffer, whether it is exported, its format and the
render target's path, so a `.dsp` that changes the export rebuilds the material.

## Where the metadata lives

Two keys are written into the generated asset's **package metadata**, keyed by the asset object.

| Key | Value |
| :-- | :-- |
| `DreamShader.SourceFile` | the source path made relative to the project directory, with forward slashes. A source outside the project keeps its absolute path. |
| `DreamShader.SourceHash` | the eight-hex-digit CRC32. Written only when non-empty. |

Storing the *project-relative* path is deliberate: a checkout on another machine, or a moved project
directory, still recognizes its generated assets as its own — the [ownership
guard](regeneration.md#ownership-guard) and *Make Ephemeral* read this key. The hash does not travel
as well: the build key names each file by its absolute path, so a checkout at another location
rebuilds every asset once.

Which assets get stamped, and when:

| Asset | Stamped |
| :-- | :-- |
| `UDreamShaderMaterialInstance` (ThinCustom) | **always** — Ephemeral and Materialized alike |
| the hidden `MB_DreamThinBase_*` base | Materialized only |
| `UMaterial` (Graph backend) | **always** *(since the release after 1.8.0; before it, persist mode only)* |
| `UMaterialFunction` / layer / layer blend | **always** *(same)* |
| a `.dsi`'s `UMaterialInstanceConstant` | **always** |
| `UDreamPassPipeline` *(2.1.0)* | **always** — the hash only on a build that may write to disk. A pipeline built in memory because writing is refused (another editor owns writes, or `dump-graph`'s guard) is stamped with the path alone, so no later compile skips on it |
| an exported buffer's render target *(2.1.0)* | the path alone, never a hash — it would move with every edit of the `.dsp` — and `DreamShader.PassPipeline`, the object path of the pipeline it belongs to |

> [!NOTE]
> Until 1.8.0 a memory-only Graph material or function was stamped with the source **path** only,
> so that the skip could never fire on it. The cost was that nothing could say whether such an asset
> was current — the Material Content Browser reported every one as stale forever. Every generated
> asset now carries its hash in both modes, and the entry points that *mean* "rebuild regardless"
> force past the skip instead (see below).

A ThinCustom instance additionally carries the source path and the hash as read-only `UPROPERTY`s —
`SourceFilePath` and `SourceHash`, category `DreamShader` — so they are visible in the details panel
without inspecting package metadata. `SourceFilePath` holds the **full normalized** source path, not
the project-relative form the metadata stores; `SourceHash` is the same eight-hex-digit value. See
[`UDreamShaderMaterialInstance`](../api/material-instance.md).

> [!NOTE]
> `DreamShader.SourceFile` doubles as the **ownership marker**. Its presence is what tells
> DreamShader that an asset is its own to overwrite, and what *Make Ephemeral*
> filters on. See [Regeneration](regeneration.md#ownership-guard).

## When regeneration is skipped

The short circuit fires only when **all** of the following hold:

| # | Condition |
| :-- | :-- |
| 1 | the generation call did not set `bForce` |
| 2 | the asset exists and the newly computed hash is non-empty |
| 3 | the stored `DreamShader.SourceFile` is present and non-empty |
| 4 | the stored source path equals the project-relative path of the source being compiled, **ignoring case** |
| 5 | the stored `DreamShader.SourceHash` equals the new hash, **case-sensitively** |

A skip is reported as the info [`DSH8237`](../diagnostics/DSH8xxx.md#dsh8237), and the product's result
line reads `Skipped {ObjectPath} from {File}; source hash is unchanged (build key {BuildKey}).` Before
the hash is compared, a build that would write to disk is skipped when another editor owns writing
this project's generated assets ([`DSH8209`](../diagnostics/DSH8xxx.md#dsh8209)).

Per asset kind:

| Asset | Skip point | Extra condition | Result line |
| :-- | :-- | :-- | :-- |
| ThinCustom material | after the instance is created or reused, **before** the hidden base is created | — | `Skipped …` |
| `Graph`-backend material | after the material is created or reused | — | `Skipped …` |
| Material function | after the function asset is created or reused | the asset's material-function usage must already match the one the block requires | `Skipped …` *(since 2.0.0; through 1.x a function's skip was silent)* |
| `UDreamPassPipeline` *(2.1.0)* | after the pipeline is reused, before its HLSL slots are planned | the slot registry holds every HLSL pass of the pipeline in the slot the asset records, with a snapshot that passed a pre-check and its files on disk, and every exported buffer has its render target — see [Custom Pass pipelines](#custom-pass-pipelines) | `Skipped …` |

Placing the ThinCustom check before the base is created is what makes a skip cheap: no base
material, no ownership check, no graph teardown.

> [!NOTE]
> A material function whose usage does not match — a `ShaderLayer` block whose asset is still marked
> `Default`, for instance — is regenerated even when the hash matches, and the usage is corrected.

## Forcing regeneration

| Path | Force |
| :-- | :-- |
| Auto-compile on save | **no** — the hash short circuit is active |
| The startup sweep (`GenerateAllSources`) | **no** — see [Ephemeral materials](ephemeral.md#when-the-asset-already-exists-on-disk) |
| The same sweep after a change to the backend, a mapping table or the preprocessor defines | **no** — the key covers each of them, so only what they affect is rebuilt |
| *Tools ▸ DreamShader ▸ Recompile DSM*, *Clean Generated Shaders* | yes — queued through the bridge as forced |
| Bridge `recompile` request (`scope: "file"` / `"all"`) | yes — an explicit request means "rebuild" |
| Material Content Browser Compile / thumbnail refresh | yes |
| Live preview render | through the bridge's `previewMaterial`, yes; through the preview WebSocket, only when the request sets `force` |
| *Materialize*, and child-instance creation | yes |
| Cook | yes |
| Commandlet `-run=DreamShader` | only with [`-Force`](../tools/commandlet.md#compile--generate); otherwise it reports `Skipped {ObjectPath} from {SourceFile}; source hash is unchanged (build key {BuildKey}).` |
| A `.usf` / `.ush` save, through the bridge *(2.1.0)* | no — the `.dsp`s that compile from the file are queued, and their key covers its text |
| A `.dsp` or a material it names compiled, through the bridge's dependents queue *(2.1.0)* | no |
| `dsc pass-registry -Rebuild` *(2.1.0)* | yes, every `.dsp` |

There is no way to clear the stored hash from the source language. To force a rebuild without a
force-capable entry point, either change the source text (any change, including whitespace), or
delete the generated asset.

## Notes

- The hash is a CRC32, not a cryptographic digest. It detects edits; it is not a security or
  integrity mechanism.
- There is no generated `.ush` include to keep in step *(since 2.0.0)*: a `Function`'s HLSL is
  written into the Custom node of each call, inside the asset, and an unchanged source skips it with
  everything else. See [Generated HLSL](generated-hlsl.md).
- The comparison is per asset. One source file that declares a material and three functions stores
  the same hash on four assets, and each is skipped independently.
- On UE 5.6 and newer the package metadata is accessed through the engine's value-typed metadata
  API; earlier engines use the object-typed one. The stored keys and values are identical.
- Nothing writes a generation timestamp. Besides these two keys a build stamps
  `DreamShader.OutputDigest` and `DreamShader.OutputDigestClasses` ([Divergence](divergence.md)),
  `DreamShader.SourceSpans` and `DreamShader.DecompileHints` on an asset with a graph, and
  `DreamShader.PassPipeline` on a pipeline's render target. None of them takes part in the skip.

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH8237` | *(info)* the short circuit skipped a product; the result line reads `Skipped …` |
| `DSH8209` | *(info)* another editor owns writing this project's generated assets, so a build that would write to disk was skipped |
| `DSH8296` | a `.dsh` was handed to the compiler directly |

## Example

```c
// DShader/Common.dsh
Function float Remap01(in float value) { return saturate(value * 0.5 + 0.5); }
```

```c
// DShader/M_Ramp.dsm
import "Common.dsh";

Shader(Name="Materials/M_Ramp")
{
    Properties { ScalarParameter Input = 0.25; }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }
    Graph      { float R = Remap01(Input); Color = vec3(R, R, R); }
}
```

Observed sequence:

```text
save M_Ramp.dsm      Generated Material /Game/Materials/M_Ramp.M_Ramp from ...M_Ramp.dsm.
save M_Ramp.dsm      Skipped /Game/Materials/M_Ramp.M_Ramp from ...M_Ramp.dsm; source hash is unchanged (build key …).
save Common.dsh      (the header builds nothing; it queues M_Ramp.dsm, whose key moved)
                     Generated Material /Game/Materials/M_Ramp.M_Ramp from ...M_Ramp.dsm.
```

Metadata on the generated instance:

```text
DreamShader.SourceFile   DShader/M_Ramp.dsm
DreamShader.SourceHash   9f2c41ab
```

## See also

- [Generation](index.md) — where the hash is computed in the pipeline
- [import](../language/import.md) — how an imported header is read
- [Preprocessor](../language/preprocessor.md) — the defines the key folds in, and why only the ones that were read
- [Regeneration](regeneration.md) — the ownership guard built on `DreamShader.SourceFile`
- [Divergence](divergence.md) — the OTHER fingerprint: what the asset holds, not what the source said
- [Ephemeral materials](ephemeral.md) — which assets are stamped in which mode
- [`UDreamShaderMaterialInstance`](../api/material-instance.md) — `SourceFilePath` and `SourceHash`
- [Generated HLSL](generated-hlsl.md) — the Custom-node code, and the 1.x include it replaced
- [Commandlet](../tools/commandlet.md) — headless compiles and forcing
- [HLSL passes](../runtime/hlsl.md) — the snapshots whose inputs a `.dsp`'s key covers
- [Project settings](../settings/project.md) — auto-compile and debounce
