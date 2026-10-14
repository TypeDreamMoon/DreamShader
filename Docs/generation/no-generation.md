# NoGeneration

> [DreamShader](../index.md) » [Generation](index.md) » **NoGeneration**

*(after 2.1.2)* A source, or one export of it, that only a compile **naming** it builds. Every other
compile leaves its asset as it is on disk, and whatever uses that asset uses it as it is.

| | |
| :-- | :-- |
| Spellings | `#pragma nogeneration` (the whole file) · `/// @nogeneration` on an `export` (that asset) · `NoGeneration = true` in a 1.x block's header (that block's asset) |
| Applies to | every asset a source builds: materials, instances (`.dsi`), material functions, layers and blends, Custom Pass pipelines (`.dsp`) |
| Built by | *Compile* in the [Material Content Browser](../tools/material-browser.md), *Revert to Source*, creating an instance of the material, a bridge request for the file, [`compile -Source`](../tools/commandlet.md), `check -Source -Shaders` |
| Left alone by | the editor's startup sweep, a save, the rebuild of a source it depends on (a header, a `.dsi`'s parent, a `.dsp`'s material), *Recompile All*, `compile -All`, `check -All -Shaders`, a cook |

Use it for an asset that is expensive to rebuild, or that someone tunes by hand in the editor and
should not be rebuilt under them every time a shared header changes.

## Writing it

```hlsl
// M_Ocean.dss -- none of its assets is rebuilt unless the file is compiled by name.
#pragma nogeneration

export void M_Ocean(inout material m) { … }
```

```hlsl
// Only MF_Heavy is left alone; MF_Light is built as usual.
/// @nogeneration
export float3 MF_Heavy(float3 p) { … }

export float3 MF_Light(float3 p) { return MF_Heavy(p) * 0.5; }
```

```c
// 1.x: on any block that builds an asset -- Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend.
ShaderFunction(Name = "Noise/MF_Heavy", NoGeneration = true)
{
    …
}
```

A 1.x file may also write `#pragma nogeneration` at its top level, for every block of the file.
[`dsc migrate`](../tools/migrate.md) writes `NoGeneration = true` as `/// @nogeneration`.

| Mistake | Code |
| :-- | :-- |
| `NoGeneration =` something other than `true`, `false`, `1` or `0` | [`DSH2260`](../diagnostics/DSH2xxx.md#dsh2260) — the block is built as if it were `false` |
| `#pragma nogeneration(…)` | [`DSH3219`](../diagnostics/DSH3xxx.md#dsh3219) — the arguments are ignored and the file **stays** marked |
| `#pragma nogeneration` in a header | [`DSH7235`](../diagnostics/DSH7xxx.md#dsh7235) — ignored; the files that include it are not marked |
| `/// @nogeneration` on a helper, a `@custom` function or an `extern` | [`DSH7224`](../diagnostics/DSH7xxx.md#dsh7224) — it marks an asset, and only an `export` has one |

## What a compile that does not name it does

The source is still read, bound and checked, so a save still reports its errors. Then, for each
NoGeneration asset:

| The asset | What happens | Reported as |
| :-- | :-- | :-- |
| exists | left as it is, even when the source has changed since it was built | info [`DSH8277`](../diagnostics/DSH8xxx.md#dsh8277); the result line `Skipped <path> from <source>; the source marks it NoGeneration, …` |
| has never been built | nothing is written | warning [`DSH8278`](../diagnostics/DSH8xxx.md#dsh8278); `Skipped <path> from <source>; … it has never been built.` |

The other assets of the file are built as usual. One that calls a NoGeneration export of the same file
calls the asset on disk; when there is none, it fails with
[`DSH8279`](../diagnostics/DSH8xxx.md#dsh8279).

## What uses it

Nothing compiles a NoGeneration source first on its behalf:

| Reference | With the asset on disk | Without |
| :-- | :-- | :-- |
| a `.dsi` whose `Parent` it builds | the instance is built against that asset — its parameters are read off it, not off the source | [`DSH8279`](../diagnostics/DSH8xxx.md#dsh8279) |
| a `.dsp` pass whose `Material` it builds | checked against that asset | `DSH8279` |
| a `.dss` that reads the buffers of a `.dsp` it builds | checked against that pipeline | `DSH8279` |
| an `extern` function, or a 1.x `MaterialFunctionCall`, naming the asset | the asset, as with any asset | the emitter's [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219) |

`DSH8279` asks you to compile the source by name once; from then on its users build against what
that compile wrote.

## Hand edits

A NoGeneration asset is never rebuilt behind your back, so a hand edit to it survives every compile
that does not name the source. A compile that does name it is an ordinary rebuild, and the
[divergence](divergence.md) check still stops it from overwriting a hand edit without asking.

## See also

- [Caching](caching.md) — the other reason a compile leaves an asset alone: its build key is current
- [Regeneration](regeneration.md) — what a rebuild does to an asset
- [Disabling diagnostics](../language-v2/disable-diagnostics.md) — `DSH8278` can be disabled for a file that is never meant to be built automatically
