# Parameter node tokens

> [DreamShader](../index.md) » [Parameters](index.md) » **Parameter node tokens**

The 22 type tokens that name an Unreal parameter expression class directly, so a `Properties`
declaration generates that exact node instead of the compact scalar / vector / texture-object node.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — the `Properties` section of a `Shader`, `ShaderFunction`, `ShaderLayer` or `ShaderLayerBlend` |
| Kind | parameter type tokens |
| Generates | the named `UMaterialExpression` subclass |
| Since | `1.2.3`; `DynamicParameter` / `CurveAtlasRowParameter` inline defaults and the texture-sample family fixed in `1.4.1` |

## Synopsis

```c
<parameter-node-token> <name> [ = <default> ] [ [ <metadata> ] ] ;
```

Tokens are matched **case-insensitively**. The set is closed: exactly the 22 tokens below.
`const` is legal only with `ScalarParameter`, `VectorParameter` and `TextureObjectParameter`
*(since 2.0.0)*; on any other token it is [`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253).

## The 22 tokens

*(since 2.0.0)* Each token takes one of three 2.0 forms:

- **a declaration** — `ScalarParameter`, `StaticBoolParameter`, `VectorParameter` and
  `TextureObjectParameter` are the `uniform` a compact token would make (`float`, a `/// @static`
  `bool`, `float4`, `Texture2D`), with one parameter node however often it is read;
- **expanded at each use** — the other eleven that build are no declaration: every read or call of the
  property is a reflected `UE.Expression(Class = "<Token>", ParameterName = …, …)` call carrying the
  default and the metadata, and identical uses share one node;
- **no 2.0 spelling** — seven tokens are `DSH3253`. Move such a material to a `.dss` and build the node
  with [`UE.Expression`](../builtins/ue-expression.md).

| # | Token | 2.0 form | Generated class | `= default` accepts | `Graph` use |
| :-- | :-- | :-- | :-- | :-- | :-- |
| 1 | `ScalarParameter` | declaration | `UMaterialExpressionScalarParameter` | scalar literal | value |
| 2 | `StaticBoolParameter` | declaration | `UMaterialExpressionStaticBoolParameter` | `true` / `false` only | value |
| 3 | `StaticSwitchParameter` | expanded | `UMaterialExpressionStaticSwitchParameter` | `true` / `false` only | **call only** — `N(True = …, False = …)` |
| 4 | `VectorParameter` | declaration | `UMaterialExpressionVectorParameter` | vector literal | value, 4 components |
| 5 | `DoubleVectorParameter` | `DSH3253` | — | — | — |
| 6 | `ChannelMaskParameter` | expanded | `UMaterialExpressionChannelMaskParameter` | vector literal | `N(Input = …)`, **1** component |
| 7 | `StaticComponentMaskParameter` | expanded | `UMaterialExpressionStaticComponentMaskParameter` | none — see below | `N(Input = …)` |
| 8 | `CurveAtlasRowParameter` | `DSH3253` | — | — | — |
| 9 | `DynamicParameter` | `DSH3253` | — | — | — |
| 10 | `FontSampleParameter` | `DSH3253` | — | — | — |
| 11 | `SpriteTextureSampler` | `DSH3253` | — | — | — |
| 12 | `TextureObjectParameter` | declaration | `UMaterialExpressionTextureObjectParameter` | asset reference | value (texture) |
| 13 | `TextureCollectionParameter` | `DSH3253` | — | — | — |
| 14 | `SparseVolumeTextureObjectParameter` | `DSH3253` | — | — | — |
| 15 | `TextureSampleParameter2D` | expanded | `UMaterialExpressionTextureSampleParameter2D` | asset reference | `N(Coordinates = …)`, read as `RGBA` |
| 16 | `TextureSampleParameter2DArray` | expanded | `UMaterialExpressionTextureSampleParameter2DArray` | asset reference | as 15 |
| 17 | `TextureSampleParameterCube` | expanded | `UMaterialExpressionTextureSampleParameterCube` | asset reference | as 15 |
| 18 | `TextureSampleParameterCubeArray` | expanded | `UMaterialExpressionTextureSampleParameterCubeArray` | asset reference | as 15 |
| 19 | `TextureSampleParameterVolume` | expanded | `UMaterialExpressionTextureSampleParameterVolume` | asset reference | as 15 |
| 20 | `TextureSampleParameterSubUV` | expanded | `UMaterialExpressionTextureSampleParameterSubUV` | asset reference | as 15 |
| 21 | `RuntimeVirtualTextureSampleParameter` | expanded | `UMaterialExpressionRuntimeVirtualTextureSampleParameter` | none — see below | `N(Coordinates = …)`, several outputs |
| 22 | `SparseVolumeTextureSampleParameter` | expanded | `UMaterialExpressionSparseVolumeTextureSampleParameter` | none — see below | `N(Coordinates = …)`, several outputs |

The class of an expanded token is looked up in the engine's node catalog; a class this engine does not
have is [`DSH5212`](../diagnostics/DSH5xxx.md#dsh5212). The `Graph` use is summarised here; the pin
table is in [Using parameters in Graph](graph-usage.md#pin-names-by-parameter-type).

## Type-specific metadata slots

Every token also accepts the generic metadata keys (`Group`, `Description`, `SortPriority`,
`Slider(min, max)`, `ParameterName`) plus any reflected UPROPERTY of its class. The class-specific
slots that matter in practice:

| Token | Metadata keys that bind class-specific UPROPERTYs |
| :-- | :-- |
| `ScalarParameter` | `SliderMin`, `SliderMax`, `PrimitiveDataIndex` |
| `StaticBoolParameter` | — |
| `StaticSwitchParameter` | — |
| `VectorParameter` | `UseCustomPrimitiveData` (binds `bUseCustomPrimitiveData`), `PrimitiveDataIndex`, `ChannelNames` |
| `ChannelMaskParameter` | `MaskChannel` (enum `EChannelMaskParameterColor`) |
| `StaticComponentMaskParameter` | `DefaultR`, `DefaultG`, `DefaultB`, `DefaultA` (booleans) |
| `TextureObjectParameter` | `Texture`, `SamplerType`, `IsDefaultMeshpaintTexture` |
| `TextureSampleParameter2D` … `TextureSampleParameterSubUV` (tokens 15–20) | `Texture`, `SamplerType`, `SamplerSource`, `MipValueMode`, `GatherMode` *(since UE 5.6)*, `AutomaticViewMipBias`, `ConstCoordinate`, `ConstMipValue`, `IsDefaultMeshpaintTexture`; `TextureSampleParameterSubUV` adds `bBlend` |
| `RuntimeVirtualTextureSampleParameter` | `VirtualTexture`, `MaterialType`, `bSinglePhysicalSpace`, `bAdaptive`, `MipValueMode`, `TextureAddressMode` |
| `SparseVolumeTextureSampleParameter` | `SparseVolumeTexture`, `MipValueMode`, `SamplerSource`, `ConstMipValue` |

Boolean UPROPERTYs are matched with the leading `b` optional, so `[bBlend=true]` and `[Blend=true]`
both bind `bBlend`. How a key the class does not have is reported depends on the form: a warning on a
declaration, an error on an expanded token — see [Metadata](metadata.md#reflected-property-passthrough).

## Which default parser a token uses

| Tokens | Accepts | Otherwise |
| :-- | :-- | :-- |
| `ScalarParameter` | a scalar literal, per [Compact type tokens](compact-types.md#scalar-default-grammar) | [`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254) |
| `StaticBoolParameter`, `StaticSwitchParameter` | `true` / `false` only | `DSH3254` |
| `VectorParameter`, `ChannelMaskParameter` | a vector literal, per [Compact type tokens](compact-types.md#vector-default-grammar) | `DSH3254` on `VectorParameter`; on `ChannelMaskParameter` an unreadable literal is dropped with no diagnostic |
| `TextureObjectParameter` | a [`Path(…)`](path.md) asset reference | resolved when the material is built: [`DSH8271`](../diagnostics/DSH8xxx.md#dsh8271) / [`DSH8218`](../diagnostics/DSH8xxx.md#dsh8218) |
| Texture-sample tokens 15–20 | a [`Path(…)`](path.md) asset reference | resolved when the material is built: [`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213), quoting the resolver's code |
| `StaticComponentMaskParameter` | nothing — the class has no `DefaultValue` | the default is dropped with [`DSH5288`](../diagnostics/DSH5xxx.md#dsh5288); write `[DefaultR = true; …]` |
| `RuntimeVirtualTextureSampleParameter`, `SparseVolumeTextureSampleParameter` | nothing | the default is dropped with **no diagnostic** *(since 2.0.0)* |

> [!WARNING]
> **A `RuntimeVirtualTextureSampleParameter` or `SparseVolumeTextureSampleParameter` default is
> ignored.** `= Path(…)` on either token reaches no property of the node, and nothing says so. Bind
> the asset through metadata instead — `[VirtualTexture = Path(…)]`, `[SparseVolumeTexture = Path(…)]`.

### What the default is written to

| Situation | Result |
| :-- | :-- |
| No `= <default>` | nothing is written; an expanded node keeps its engine default, a declaration the default of its compact twin (see [Compact type tokens](compact-types.md)) |
| A declaration token | the default of the `uniform`, exactly as for its compact twin |
| `StaticSwitchParameter` | `DefaultValue = true` or `false` |
| `ChannelMaskParameter` | `DefaultValue` = the four values of the literal |
| Texture-sample tokens 15–20 | the node's `Texture` property. An asset that resolves and does not load is written as nothing, with no diagnostic, and the node then takes the engine's default texture |

## Texture-dimension inference

A texture-sample token fixes a dimension by the suffix after `TextureSampleParameter`:

| Token | Dimension |
| :-- | :-- |
| `TextureSampleParameter2D` | `Texture2D` |
| `TextureSampleParameter2DArray` | `Texture2DArray` |
| `TextureSampleParameterCube` | `TextureCube` |
| `TextureSampleParameterCubeArray` | none — any texture |
| `TextureSampleParameterVolume` | `VolumeTexture` |
| `TextureSampleParameterSubUV` | `Texture2D` |
| `RuntimeVirtualTextureSampleParameter` | not judged |
| `SparseVolumeTextureSampleParameter` | not judged |

*(since 2.0.0)* The suffix is compared whole, so `TextureSampleParameterCubeArray` is no longer taken
for a cube. The dimension is used for one thing only: judging the class a `Class'…'` default names.

`TextureObjectParameter` declares **no** dimension: it takes the dimension of the asset its default
loads — a cube, a 2D array or a volume, else `Texture2D` — which is what lets it accept any of them
*(since 1.6.0)*.

### Dimension validation asymmetry

| Token family | What is checked |
| :-- | :-- |
| Compact `Texture2D` / `TextureCube` / `Texture2DArray` / `Texture3D` / `VolumeTexture` | the `Class'…'` shell: another dimension is [`DSH1044`](../diagnostics/DSH1xxx.md#dsh1044), a non-texture class [`DSH1043`](../diagnostics/DSH1xxx.md#dsh1043). The loaded asset is not checked *(since 2.0.0)* |
| `TextureObjectParameter` | the shell's "not a texture" half only (`DSH1043`) |
| Texture-sample tokens 15–17, 19, 20 | the shell, as for a compact token |
| `TextureSampleParameterCubeArray` | the shell's "not a texture" half only |
| `RuntimeVirtualTextureSampleParameter`, `SparseVolumeTextureSampleParameter` | nothing; their default is not read |

> [!WARNING]
> A `TextureSampleParameterCube` assigned a plain 2D texture by its plain object path generates
> without a DreamShader diagnostic; only a `Texture2D'…'` shell gives the mismatch away
> (`DSH1044`). The mismatch otherwise surfaces later as an Unreal shader-compile error on the
> material. Assign the right dimension.

## Notes

- **A default value is optional for every token that has a 2.0 form.** The test
  `DreamShader.Lang.ParameterExpressions.ParseAll` covers each of the 15, covers `ScalarParameter`,
  `VectorParameter` and `TextureObjectParameter` with and without an inline default, and checks that
  each of the seven others is `DSH3253`.
- **A sampler parameter with no texture still compiles.** When an expanded node derives from
  `UMaterialExpressionTextureBase` and its `Texture` is null, `SetDefaultTexture()` is called for a
  `UMaterialExpressionTextureSampleParameter` subclass. Without that, a default-less sampler parameter
  would fail with *Missing input Texture*. The runtime-virtual-texture and sparse-volume nodes are not
  such subclasses and still need their asset bound.
- **An explicit `SamplerType` always wins.** `AutoSetSampleType()` runs only on a node whose source
  gives no `SamplerType`. See [SamplerType](sampler-type.md).
- `StaticSwitchParameter` cannot be read as a value. A bare reference is not a declared name
  ([`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)); it must be called. See
  [Using parameters in Graph](graph-usage.md#staticswitchparameter).
- **A `Group(…)` scope does not stamp an expanded token** *(since 2.0.0)*: such a member gets no
  group and no automatic `SortPriority` from the scope, and does not use up a counter slot. Write
  `Group` / `SortPriority` in its own metadata block. See
  [Metadata](metadata.md#group-scopes-and-the-sortpriority-counter).
- The [decompiler](../tools/decompiler.md) emits `SamplerType` and the texture-sample metadata keys
  explicitly on every export, even at their defaults, so a decompile → recompile round trip is stable.

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH3253` | one of the seven tokens with no 2.0 form, or `const` on a token other than `ScalarParameter`, `VectorParameter`, `TextureObjectParameter` |
| `DSH3254` | a non-`true`/`false` default on `StaticBoolParameter` or `StaticSwitchParameter`, an unparsable default on `ScalarParameter` or `VectorParameter` |
| `DSH5288` | a default on `StaticComponentMaskParameter`, which has no `DefaultValue` (warning; the default is dropped) |
| `DSH5212` | the engine has no class for an expanded token |
| `DSH1043` / `DSH1044` | a `Class'…'` default names a non-texture / a texture of another dimension |
| `DSH8271` / `DSH8218` | a `TextureObjectParameter` default does not resolve / does not load |
| `DSH8213` | an expanded node cannot take a value: a texture-sample default that does not resolve, a metadata value of the wrong type |
| [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) | an expanded token's metadata key is neither a pin nor a property of its class |
| [`DSH8210`](../diagnostics/DSH8xxx.md#dsh8210) | a declaration token's metadata key is no property of its class (warning; the value is not written) |

The complete list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Docs/M_ParameterNodes")
{
    Properties = {
        Group("Surface") {
            TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_White") [
                SamplerType  = "LinearColor";
                SamplerSource = "FromTextureAsset";
                MipValueMode = "None";
                AutomaticViewMipBias = true;
            ];
            ScalarParameter Roughness = 0.55 [Slider(0, 1)];
            VectorParameter Tint      = float4(1.0, 0.8, 0.6, 1.0);
        }

        Group("Masks") {
            ChannelMaskParameter         Pick = float4(1, 0, 0, 0) [MaskChannel = "Red"];
            StaticComponentMaskParameter Keep = float4(1, 1, 0, 0);
        }

        StaticSwitchParameter UseDetail = true [Group="Switches"];
    }

    Settings = { Domain = "Surface"; ShadingModel = "DefaultLit"; BlendMode = "Opaque"; }

    Outputs = {
        vec3  Color;
        float Rough;

        Base.BaseColor = Color;
        Base.Roughness = Rough;
    }

    Graph = {
        vec2  UV     = UE.TexCoord(Index = 0);
        vec4  Sample = BaseTex(Coordinates = UV);
        vec4  Masked = Keep(Input = Sample);
        float Chan   = Pick(Input = Sample);

        Color = UseDetail(True = Masked.rgb, False = Tint.rgb);
        Rough = Roughness * Chan;
    }
}
```

Generated nodes:

```text
TextureSampleParameter2D      BaseTex    SamplerType=LinearColor
ScalarParameter               Roughness  Group="Surface" SortPriority=0  SliderMin=0 SliderMax=1
VectorParameter               Tint       Group="Surface" SortPriority=10
ChannelMaskParameter          Pick       MaskChannel=Red
StaticComponentMaskParameter  Keep
StaticSwitchParameter         UseDetail  Group="Switches"
TextureCoordinate             UV         CoordinateIndex=0
```

`BaseTex`, `Pick` and `Keep` are expanded tokens, so their `Group(…)` scopes do not stamp them and
`Roughness` takes counter slot 0. `Keep`'s `float4` default is dropped with `DSH5288`: write
`[DefaultR = true; DefaultG = true]` for that mask.

## See also

- [Parameters](index.md) — the hub and the decision table
- [Compact type tokens](compact-types.md) — the 39 built-in tokens and the checks they get
- [Metadata block](metadata.md) — how `[MaskChannel=…]` and every other key is written
- [SamplerType](sampler-type.md) — every sampler-type value, and `SamplerSource`
- [Path(…)](path.md) — the asset-reference grammar these defaults use
- [Using parameters in Graph](graph-usage.md) — reads, the pin call form, and `StaticSwitchParameter`
- [Properties (section)](../language/properties.md) — the enclosing section grammar
- [UE.Expression](../builtins/ue-expression.md) — the generic reflected form for classes with no token
- [Decompiler](../tools/decompiler.md) — which of these tokens a `UMaterial` export produces
- [Testing](../contributing/testing.md) — the parameter matrix fixture that pins this table
