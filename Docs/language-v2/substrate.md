# Substrate sugar

> [DreamShader](../index.md) » [DreamShaderLang 2.0](index.md) » **Substrate sugar**

Shorter spellings for Substrate graphs. Each one is a spelling and nothing else: it binds to a fixed
pattern of [`Substrate.*`](../builtins/substrate.md) nodes, and the graph is the one the long form makes.
The decompiler writes the long form, and the short form when it is asked for the readable one.

| | |
| :-- | :-- |
| Applies to | `.dss`, and `.dsm` / `.dsf` / `.dsh` — both front ends arrive at one binder |
| Requires | the engine's Substrate nodes (UE 5.4+); `Substrate.Select` needs UE 5.6 |
| Changes the graph | never — `A + B` **is** `Substrate.Add(A = A, B = B)` |
| Since | `2.0.0` |

```hlsl
#pragma material(Substrate = Native)

/// @group Surface
uniform Texture2D Albedo;
uniform float Rust = 0.3;
uniform float Coat = 1.0;

export void M_CoatedMetal(inout material m)
{
    float4 Tex = Albedo.Sample(UE.TexCoord(Index = 0));

    Substrate Metal = Substrate.Slab(BaseColor = Tex.rgb, Metallic = 1.0, Roughness = 0.35);   // S3
    Substrate RustS = Substrate.Slab(BaseColor = Tex.rgb * float3(0.6, 0.3, 0.1), Roughness = 0.9);
    Substrate Body  = lerp(Metal, RustS, saturate(Tex.a * Rust));                              // S1
    Substrate Clear = Substrate.Slab(IOR = 1.5, Roughness = 0.1);                              // S3

    m.FrontMaterial = Substrate.Layer(Clear * Coat, Body, 0.01);                               // S1
}
```

## Operators and `lerp`

A `Substrate` value is not a number. It has two operators and one intrinsic, and each is a node:

| You write | It is | Pins |
| :-- | :-- | :-- |
| `A + B` | `Substrate.Add` | `A`, `B` |
| `A * w`, `w * A` | `Substrate.Weight` | `A`, `Weight` |
| `lerp(A, B, t)` | `Substrate.HorizontalMix` | `Background`, `Foreground`, `Mix` |

Precedence is the language's own: `A + B * w` weights `B`, then adds. Every other operator over a
Substrate value — `-`, `/`, `A * B`, unary minus, a compound assignment such as `A += B` — is
[`DSH5293`](../diagnostics/DSH5xxx.md), and so is a `lerp` that mixes a Substrate value with a number.

The five composition nodes also take their operands **by position**, and two of them have short names:

| Call | Positional order |
| :-- | :-- |
| `Substrate.Add(A, B)` | `A`, `B` |
| `Substrate.Weight(A, w)` | `A`, `Weight` |
| `Substrate.Mix(Bg, Fg, t)` — also `HorizontalMix`, `HorizontalMixing` | `Background`, `Foreground`, `Mix` |
| `Substrate.Layer(Top, Base, Thickness)` — also `VerticalLayer`, `VerticalLayering` | `Top`, `Base`, `Thickness` |
| `Substrate.Select(A, B, t)` | `A`, `B`, `SelectValue` |

A BSDF has eighteen pins, so a BSDF takes its arguments by name only.

> [!NOTE]
> The operators take the engine's default for parameter blending — off. A source that wants it writes
> the named call, `Substrate.Mix(A, B, t, UseParameterBlending = true)`, and the decompiler keeps a
> node that has any property set as the call it is.

## `if` and `?:` over Substrate values

A branch that leaves a different Substrate value on each side is a node too:

| The condition | The branch becomes |
| :-- | :-- |
| a `/// @static` uniform, or a `/// @static` function parameter | a `StaticSwitch` typed `Substrate` — one side is compiled out |
| anything decided at run time | `Substrate.Select(A = <else side>, B = <then side>, SelectValue = <condition>)` |

```hlsl
Substrate Front = Dry;
if (Wetness > 0.5)          // run time: a Substrate.Select
{
    Front = Wet;
}
m.FrontMaterial = Front;
```

`Substrate.Select` exists from UE 5.6 on; on an older engine the run-time form is
[`DSH4378`](../diagnostics/DSH4xxx.md), which says so and offers the two ways out — make the condition
static, or `lerp` the two values. Select parameter-blends its inputs, and the engine refuses to blend
some pairs of unlike BSDFs; when the two sides are different node classes the compiler says
[`DSH4380`](../diagnostics/DSH4xxx.md) as a warning and makes the node, because which pairs is the
engine's business and it reports them itself.

## Legacy parameters on a slab

`Substrate.Slab` is parameterised by `DiffuseAlbedo` and `F0`, which nobody has textures for. The
arguments below are **not pins** of the node; each family is the input side of a conversion node the
compiler puts in front of the pins it stands for:

| Arguments | Conversion node | Feeds |
| :-- | :-- | :-- |
| `BaseColor`, `Metallic`, `Specular` | `Substrate.MetalnessToDiffuseAlbedoF0` | `DiffuseAlbedo`, `F0` |
| `Haziness` — measured against the call's own `Roughness` | `Substrate.HazinessToSecondaryRoughness` | `SecondRoughness`, `SecondRoughnessWeight` |
| `Transmittance`, `Thickness` | `Substrate.TransmittanceToMFP` | `SSSMFP` |
| `IOR` | none: `F0 = ((n − 1) / (n + 1))²` | `F0` |

A node takes a family when it has every pin the family feeds: `Substrate.Slab` takes all four,
`Substrate.SimpleClearCoat` the two that end in `F0`. A name that **is** a pin of the node is that pin —
`Substrate.ShadingModels(BaseColor = …)` converts nothing.

- An argument that is left out keeps the engine's default on the conversion node (`BaseColor` 0.18,
  `Specular` 0.5, `Metallic` 0).
- An `IOR` that is a number folds into a constant `F0` (`1.5` → `0.04`); a computed one is four
  arithmetic nodes.
- The conversion nodes are ordinary nodes, so two slabs that convert the same `BaseColor` and `Metallic`
  share one — the same merge any two equal calls get.

| Mistake | Code |
| :-- | :-- |
| two names for the same pins — `BaseColor` with `DiffuseAlbedo` or `F0`, `IOR` with `Metallic` / `Specular` / `BaseColor` / `F0`, `Haziness` with `SecondRoughness`, `Transmittance` with `SSSMFP` | [`DSH5295`](../diagnostics/DSH5xxx.md) |
| `Haziness` without `Roughness`, `Thickness` without `Transmittance` | [`DSH5296`](../diagnostics/DSH5xxx.md) |
| the same argument twice | `DSH4215` |
| the conversion node is missing from this engine | [`DSH4381`](../diagnostics/DSH4xxx.md) |

## Building a value member by member

A `Substrate` local declared as a node call **without arguments** is a value still being built:

```hlsl
Substrate S = Substrate.Slab();      // no arguments: a builder. With arguments it is a finished node.
S.BaseColor = Albedo;                // a pin, or an argument of the table above -- in any order
S.Metallic  = 1.0;
#if DS_HAS_FUZZ
S.FuzzAmount = Fuzz;                 // the preprocessor cuts lines, which is what this form is for
#endif
m.FrontMaterial = S;                 // the first use of S makes the node
```

| Rule | |
| :-- | :-- |
| A member write connects a pin | `S.Pin = x`; `S.Pin *= x` and `++S.Pin` start from what the member holds |
| The last write wins | a second `S.Roughness = …` replaces the first |
| A member read makes no node | `S.Roughness` is whatever was written to it; reading a member nothing wrote is [`DSH5299`](../diagnostics/DSH5xxx.md) |
| The first use seals the value | assigning `S`, passing it, using it as an operand. A member write after that is [`DSH5297`](../diagnostics/DSH5xxx.md) |
| No member write under an `if` the declaration is outside of | it would be one node in two versions — [`DSH5298`](../diagnostics/DSH5xxx.md). Build two values and choose between them. A builder declared **inside** the arm is that arm's own. `#if` is not an `if`. |
| A builder that is assigned whole has no members afterwards | `S = T;` then `S.Roughness` is [`DSH5297`](../diagnostics/DSH5xxx.md) |
| A pin is connected whole | `S.DiffuseAlbedo.r = 1` is `DSH4229` |
| What a call is asked at once, a builder is asked at its first use | `Haziness` without `Roughness` is [`DSH5296`](../diagnostics/DSH5xxx.md) there |

The node is made where the value is first used, not where it is declared, and through the same path the
call takes — so `Substrate.Slab(Roughness = r, BaseColor = c)` and the four lines that build it cannot
come out as different graphs, and the decompiler always writes the call.

> [!NOTE]
> The binder reads a body once, top to bottom. What it cannot see is an unrolled loop coming round: a
> write that stands *above* the use in the text and runs *after* it on the second trip is refused by the
> IR builder instead, as [`DSH4383`](../diagnostics/DSH4xxx.md). Declare the value inside the loop.

## `FrontMaterial` of a material that arrived through a pin

`m.FrontMaterial` is a Substrate value wherever a `material` is: written like any attribute, and read —
off a layer's input, a blend's `Bottom` and `Top`, the result of `UE.BlendMaterialAttributes` — through
a `GetMaterialAttributes` node, because the engine's Break node has no such output. That is what lets a
layer blend carry Substrate:

```hlsl
/// @layerblend
export void MLB_Wet(material Bottom, material Top, float Alpha, inout material R)
{
    R.FrontMaterial = lerp(Bottom.FrontMaterial, Top.FrontMaterial, Alpha);
}
```

## One source, two kinds of project — `Substrate =`

`#pragma material(Substrate = …)` says how a material written against the legacy attributes is to be
read in a project that has Substrate on:

| Mode | In a project with Substrate **off** | In a project with Substrate **on** |
| :-- | :-- | :-- |
| `Legacy` *(default)* | as written | as written — the engine converts the legacy attributes itself when it loads the asset |
| `Bridge` | as written | the shading attributes of a **Surface** material go into one `Substrate.ShadingModels` node on `FrontMaterial`, pin for pin as the engine's own conversion does it — see the table below |
| `Native` | driving `FrontMaterial` is [`DSH4382`](../diagnostics/DSH4xxx.md): the engine could not compile the asset | as written |

What `Bridge` folds is exactly what the engine's conversion of a legacy Surface material folds, so the
asset reads the same as the one the engine would have made at load — except that the node is in the
graph you build, diff and decompile:

| Attribute | Pin of `Substrate.ShadingModels` | On the material afterwards |
| :-- | :-- | :-- |
| `BaseColor`, `Metallic`, `Specular`, `Roughness`, `Anisotropy`, `EmissiveColor`, `Tangent` | the pin of the same name | no |
| `SubsurfaceColor` | `SubSurfaceColor` | no |
| `ClearCoat`, `ClearCoatRoughness` (`CustomData0` / `CustomData1`) | `ClearCoat`, `ClearCoatRoughness` | no |
| `Normal`, `Opacity` | the pin of the same name | **yes** — the engine copies these two, and so does `Bridge` |
| `ShadingModel` (a material whose setting is `FromMaterialExpression`) | `ShadingModel` | **yes** |
| everything else — `OpacityMask`, `WorldPositionOffset`, `PixelDepthOffset`, `Displacement`, `Refraction`, `AmbientOcclusion`, `SurfaceThickness`, the customized UVs | — | stays as written |

The node's `ShadingModelOverride` is the material's `ShadingModel` setting; a material that computes
its shading model leaves the override alone and drives the pin instead. A material that writes none of
the attributes that move has nothing to bridge and is left as written.

"Substrate on" is the built-in define [`DS_SUBSTRATE`](../language/preprocessor.md), which is part of the
[build key](../generation/caching.md) of every material that says `Bridge` or `Native` — switching the
project setting rebuilds them. A `Bridge` material that drives `FrontMaterial` itself is left as
written; so is one of another domain, which the engine converts with a node of its own. An unknown mode
is [`DSH7232`](../diagnostics/DSH7xxx.md).

## Names, and engines that lack a node

Every Substrate node class of the engine — and of the plugins that are loaded — is callable, whether or
not DreamShader has heard of it: a class goes by its class name without `MaterialExpressionSubstrate` in
front and `BSDF` behind, so UE 5.8's `UMaterialExpressionSubstrateToonBSDF` is `Substrate.Toon` with no
change to the plugin. The names in [`Substrate.*`](../builtins/substrate.md) are those names plus a
handful of aliases (`HorizontalMix` / `Mix`, `VerticalLayer` / `Layer`).

When a sugar needs a node this engine does not have, the message names the engine that has it —
`Substrate.Select` from 5.6, `Substrate.Toon` from 5.8 — instead of reporting an unknown name
([`DSH5294`](../diagnostics/DSH5xxx.md)).

## Decompiling

The decompiler writes every Substrate node as the call it is, under the name sources use —
`Substrate.Slab`, not the reflected `SubstrateSlabBSDF`. A Coverage Weight is `Substrate.Weight(A = …, Weight = …)`
and a Transmittance-To-MFP in front of a slab is `SSSMFP = Substrate.TransmittanceToMFP(…).MFP`, so a search
of the text for a node the graph shows finds it.

The sugar is read back only for the readable form (the commandlet's `-Readable`, the bridge's
`readable`), and there only where it is graph-exact:

| The graph has | The readable text says |
| :-- | :-- |
| `SubstrateAdd` / `SubstrateWeight` / `SubstrateHorizontalMixing` with every pin wired and no property set | `A + B`, `A * w`, `lerp(A, B, t)`, in a local named after the node (`SubstrateWeight`) |
| a conversion node whose outputs feed one BSDF and nothing else, with nothing set on it | `BaseColor = …`, `Metallic = …`, `Haziness = …`, `Transmittance = …` on that BSDF |

Either form is written for:

| The graph has | The text says |
| :-- | :-- |
| a material whose product says `Bridge` or `Native` | `#pragma material(Substrate = …)` |

`IOR` is the one that is not read back: a constant `F0` does not say it was an index of refraction. A
`Bridge` material that was folded decompiles to the `Substrate.ShadingModels` call it became.

## 1.x sources

The sugar is the binder's, so a `.dsm` has all of it in its own spelling — `vec3` for `float3`, and the
`Outputs` section where 2.0 writes `m.X =`:

```c
Shader(Name = "Materials/M_CoatedMetal")
{
    Outputs {
        Base.FrontMaterial = Substrate.Layer(Clear * Coat, Body, 0.01);   // an expression, not a variable name
    }
    Graph {
        Substrate Metal = Substrate.Slab(BaseColor = Tint.rgb, Metallic = 1.0, Roughness = 0.35);
        Substrate Body  = lerp(Metal, RustS, saturate(Rust));
        Substrate Clear = Substrate.Slab(IOR = 1.5, Roughness = 0.1);
    }
}
```

The right side of an `Outputs` binding is an expression of its own; it is read after the `Graph` has
run. A `Shader` whose `Outputs` compute everything needs no `Graph` section at all.

## Diagnostics

| Code | Severity | Says |
| :-- | :-- | :-- |
| `DSH5293` | error | an operator a Substrate value does not have; `lerp` over one Substrate value and one number |
| `DSH5294` | error | the sugar needs a node this engine does not have, and which engine has it |
| `DSH5295` | error | two names for the same pins |
| `DSH5296` | error | `Haziness` without `Roughness`; `Thickness` without `Transmittance` |
| `DSH5297` | error | a member write after the value was used; a member of a builder that was assigned whole |
| `DSH5298` | error | a member write under an `if` the declaration is outside of |
| `DSH5299` | error | no such pin; a read of a member nothing wrote |
| `DSH4378` | error | a run-time branch over Substrate values on an engine without `Substrate.Select` |
| `DSH4380` | warning | a `Substrate.Select` between unlike BSDFs |
| `DSH4381` | error | a conversion node this engine does not have |
| `DSH4382` | error | `Substrate = Native` driving `FrontMaterial` with Substrate off |
| `DSH4383` | error | an unrolled loop came round to a builder member after the value was used or assigned |
| `DSH7232` | error | an unknown `Substrate =` mode |

## See also

- [`Substrate.*`](../builtins/substrate.md) — the nodes themselves, pin by pin
- [Material settings](../settings/material.md) — `#pragma material(...)`
- [Preprocessor](../language/preprocessor.md) — `DS_SUBSTRATE`, and `#if` around builder lines
- [Decompiler](../tools/decompiler.md)
