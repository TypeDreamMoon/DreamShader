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

Tokens are matched **case-insensitively**, but the spelling the author typed is what is used to
resolve the expression class, so any casing works. The set is closed: exactly the 22 tokens below.
`const` is never legal with any of them.

## The 22 tokens

`ScalarParameter`, `VectorParameter` and `TextureObjectParameter` take dedicated construction paths;
every other token resolves its class by name, trying `<Token>`, `U<Token>`, `MaterialExpression<Token>`
and `UMaterialExpression<Token>` in that order against every non-abstract `UMaterialExpression`
subclass, case-insensitively.

| # | Token | Type | Components | Generated class | `= default` accepts | `Graph` call form |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| 1 | `ScalarParameter` | Scalar | 1 | `UMaterialExpressionScalarParameter` | scalar literal | — |
| 2 | `StaticBoolParameter` | Scalar | 1 | `UMaterialExpressionStaticBoolParameter` | `true` / `false` only | — |
| 3 | `StaticSwitchParameter` | Scalar | 1 | `UMaterialExpressionStaticSwitchParameter` | `true` / `false` only | **required** — `N(True = …, False = …)` |
| 4 | `VectorParameter` | Vector | 4 | `UMaterialExpressionVectorParameter` | vector literal | — |
| 5 | `DoubleVectorParameter` | Vector | 4 | `UMaterialExpressionDoubleVectorParameter` | vector literal | — |
| 6 | `ChannelMaskParameter` | Vector | **1** | `UMaterialExpressionChannelMaskParameter` | vector literal | `N(Input = …)` |
| 7 | `StaticComponentMaskParameter` | Vector | 4 | `UMaterialExpressionStaticComponentMaskParameter` | vector literal | `N(Input = …)` |
| 8 | `CurveAtlasRowParameter` | Vector | **3** | `UMaterialExpressionCurveAtlasRowParameter` | vector literal — only `.R` is written | — (see the warning below) |
| 9 | `DynamicParameter` | Vector | 4 | `UMaterialExpressionDynamicParameter` | vector literal | — |
| 10 | `FontSampleParameter` | Vector | 4 | `UMaterialExpressionFontSampleParameter` | vector literal — **discarded** | — |
| 11 | `SpriteTextureSampler` | Vector | 4 | `UMaterialExpressionSpriteTextureSampler` (Paper2D) | vector literal **only** | — |
| 12 | `TextureObjectParameter` | Texture | 0 | `UMaterialExpressionTextureObjectParameter` | `Path(…)` / bare quoted path | — |
| 13 | `TextureCollectionParameter` | Texture | 0 | `UMaterialExpressionTextureCollectionParameter` | `Path(…)` / bare quoted path | — |
| 14 | `SparseVolumeTextureObjectParameter` | Texture | 0 | `UMaterialExpressionSparseVolumeTextureObjectParameter` | `Path(…)` / bare quoted path | — |
| 15 | `TextureSampleParameter2D` | Vector | 4 | `UMaterialExpressionTextureSampleParameter2D` | `Path(…)` / bare quoted path | `N(Coordinates = …)` |
| 16 | `TextureSampleParameter2DArray` | Vector | 4 | `UMaterialExpressionTextureSampleParameter2DArray` | `Path(…)` / bare quoted path | `N(Coordinates = …)` |
| 17 | `TextureSampleParameterCube` | Vector | 4 | `UMaterialExpressionTextureSampleParameterCube` | `Path(…)` / bare quoted path | `N(Coordinates = …)` |
| 18 | `TextureSampleParameterCubeArray` | Vector | 4 | `UMaterialExpressionTextureSampleParameterCubeArray` | `Path(…)` / bare quoted path | `N(Coordinates = …)` |
| 19 | `TextureSampleParameterVolume` | Vector | 4 | `UMaterialExpressionTextureSampleParameterVolume` | `Path(…)` / bare quoted path | `N(Coordinates = …)` |
| 20 | `TextureSampleParameterSubUV` | Vector | 4 | `UMaterialExpressionTextureSampleParameterSubUV` | `Path(…)` / bare quoted path | `N(Coordinates = …)` |
| 21 | `RuntimeVirtualTextureSampleParameter` | Vector | 4 | `UMaterialExpressionRuntimeVirtualTextureSampleParameter` | `Path(…)` / bare quoted path | `N(Coordinates = …)` |
| 22 | `SparseVolumeTextureSampleParameter` | Vector | 4 | `UMaterialExpressionSparseVolumeTextureSampleParameter` | `Path(…)` / bare quoted path | `N(Coordinates = …, TextureObject = …)` |

`Type` is the DreamShader property type, not the Unreal pin type. `Texture` means "asset-valued": the
node carries an asset and produces no numeric output. The `Graph` call form is summarised here; the
exhaustive pin table, including the pins that only appear under certain metadata, is in
[Using parameters in Graph](graph-usage.md#pin-names-by-parameter-type).

> [!NOTE]
> `ChannelMaskParameter` reads as a **1-component** value in `Graph` and `CurveAtlasRowParameter` as a
> **3-component** value, even though both generate 4-channel-looking nodes. Declare the receiving
> variable accordingly.

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
| `DoubleVectorParameter` | — |
| `ChannelMaskParameter` | `MaskChannel` (enum `EChannelMaskParameterColor`) |
| `StaticComponentMaskParameter` | `DefaultR`, `DefaultG`, `DefaultB`, `DefaultA` (booleans) |
| `CurveAtlasRowParameter` | `Curve`, `Atlas` |
| `DynamicParameter` | `ParameterIndex` (0–3) |
| `FontSampleParameter` | `Font`, `FontTexturePage` |
| `SpriteTextureSampler` | `Texture`, `SamplerType`, `bSampleAdditionalTextures`, `AdditionalSlotIndex`, `SlotDisplayName` |
| `TextureObjectParameter` | `Texture`, `SamplerType`, `IsDefaultMeshpaintTexture` |
| `TextureCollectionParameter` | `TextureCollection` |
| `SparseVolumeTextureObjectParameter` | `SparseVolumeTexture` |
| `TextureSampleParameter2D` … `TextureSampleParameterSubUV` (tokens 15–20) | `Texture`, `SamplerType`, `SamplerSource`, `MipValueMode`, `GatherMode` *(since UE 5.6)*, `AutomaticViewMipBias`, `ConstCoordinate`, `ConstMipValue`, `IsDefaultMeshpaintTexture`; `TextureSampleParameterSubUV` adds `bBlend` |
| `RuntimeVirtualTextureSampleParameter` | `VirtualTexture`, `MaterialType`, `bSinglePhysicalSpace`, `bAdaptive`, `MipValueMode`, `TextureAddressMode` |
| `SparseVolumeTextureSampleParameter` | `SparseVolumeTexture`, `MipValueMode`, `SamplerSource`, `ConstMipValue` |

Boolean UPROPERTYs are matched with the leading `b` optional, so `[bBlend=true]` and `[Blend=true]`
both bind `bBlend`. The value grammar for each property type is in
[Metadata](metadata.md#reflected-property-passthrough).

## Which default parser a token uses

The `= <default>` branch is chosen by the **token family**, not by what the generated node can
actually store.

| Branch | Tokens | Accepts |
| :-- | :-- | :-- |
| Scalar | `ScalarParameter`, `StaticBoolParameter`, `StaticSwitchParameter` | a scalar literal; the two static tokens accept **only** `true` / `false` |
| Vector | `VectorParameter`, `DoubleVectorParameter`, `ChannelMaskParameter`, `StaticComponentMaskParameter`, `DynamicParameter`, `FontSampleParameter`, `CurveAtlasRowParameter`, `SpriteTextureSampler` | a vector literal, per [Compact type tokens](compact-types.md#vector-default-grammar) |
| Texture object | `TextureObjectParameter`, `TextureCollectionParameter`, `SparseVolumeTextureObjectParameter` | a [`Path(…)`](path.md) asset reference |
| Texture sample | the eight `TextureSampleParameter*` / `RuntimeVirtualTextureSampleParameter` / `SparseVolumeTextureSampleParameter` tokens (15–22) | a [`Path(…)`](path.md) asset reference |

> [!WARNING]
> **`FontSampleParameter`, `CurveAtlasRowParameter` and `SpriteTextureSampler` cannot take `= Path(…)`.**
> They sit in the vector branch, so `SpriteTextureSampler S = Path(Game, "T_X");` fails with
> `Invalid vector default value 'Path(Game,"T_X")' for property 'S'.` Bind their assets through
> metadata instead — `[Texture=Path(…)]`, `[Font=Path(…)]`, `[Curve=Path(…); Atlas=Path(…)]`.

### What the default is written to

| Situation | Result |
| :-- | :-- |
| No `= <default>` | nothing is written; the node keeps its engine default |
| Class has no `DefaultValue` UPROPERTY | the parsed default is **silently discarded** — this is `FontSampleParameter` |
| Class has a scalar (`float` / `double`) `DefaultValue` but the token is vector-classified | only `VectorDefaultValue.R` is written — this is `CurveAtlasRowParameter`, whose `DefaultValue` is a curve row position |
| Token is `StaticBoolParameter` / `StaticSwitchParameter` | the literal string `true` or `false` is written |
| Any other Scalar-typed token | the value is written as a sanitized float string |
| Vector-typed token | written as `(R=…,G=…,B=…,A=…)`, retried as `(X=…,Y=…,Z=…,W=…)` if the first form is rejected |
| Texture-classified token, or any token with an asset path | the first present slot of `Texture` → `TextureObject` → `SparseVolumeTexture` → `VirtualTexture` → `TextureCollection` → `Font` is written |

If none of those six asset slots exists on the class, generation fails with
`'{Class}' does not expose a texture/asset property for property '{Name}'.`

## Texture-dimension inference

The eight texture-sample tokens declare an explicit dimension, inferred from the token spelling by
substring test, in this order:

| Order | Token contains (case-insensitive) | Texture type |
| :-- | :-- | :-- |
| 1 | `Cube` | `TextureCube` |
| 2 | `Array` | `Texture2DArray` |
| 3 | `Volume` | `VolumeTexture` |
| 4 | none of the above | `Texture2D` |

| Token | Inferred texture type |
| :-- | :-- |
| `TextureSampleParameter2D` | `Texture2D` |
| `TextureSampleParameter2DArray` | `Texture2DArray` |
| `TextureSampleParameterCube` | `TextureCube` |
| `TextureSampleParameterCubeArray` | **`TextureCube`** |
| `TextureSampleParameterVolume` | `VolumeTexture` |
| `TextureSampleParameterSubUV` | `Texture2D` |
| `RuntimeVirtualTextureSampleParameter` | `Texture2D` |
| `SparseVolumeTextureSampleParameter` | `VolumeTexture` |

> [!NOTE]
> `TextureSampleParameterCubeArray` contains both `Cube` and `Array`; `Cube` is tested first, so the
> token is classified as `TextureCube` rather than as a cube array. This has no observable effect,
> because the inferred dimension of a sample token is never used for validation (below) and the
> generated class is the correct `UMaterialExpressionTextureSampleParameterCubeArray`.

The three texture-object tokens (12–14) deliberately declare **no** dimension: the effective dimension
is taken from the assigned default asset at generation time. That is what lets
`TextureObjectParameter` accept a cube, an array or a volume texture *(unreleased)*.

### Dimension validation asymmetry

| Token family | Asset dimension checked against the declaration? |
| :-- | :-- |
| Compact `Texture2D` / `TextureCube` / `Texture2DArray` / `Texture3D` / `VolumeTexture` | **yes** |
| `TextureObjectParameter` | yes, but the expected type is inferred from the asset, so it can never fail |
| `TextureCollectionParameter`, `SparseVolumeTextureObjectParameter` | **no** |
| All eight texture-sample tokens (15–22) | **no** |

> [!WARNING]
> A `TextureSampleParameterCube` assigned a plain 2D texture generates without a DreamShader
> diagnostic. Every token in the "no" rows above is constructed through the generic reflected path,
> which has no dimension check, so the mismatch surfaces later as an Unreal shader-compile error on
> the material. Assign the right dimension, or use the compact token if you want the check.

## Notes

- **A default value is optional for every one of the 22 tokens.** The parse+generate test matrix
  covers each token, and `ScalarParameter`, `VectorParameter` and `TextureObjectParameter` are covered
  twice — once with and once without an inline default.
- **A sampler parameter with no texture still compiles.** When the generated node derives from
  `UMaterialExpressionTextureBase` and its `Texture` slot is null, `SetDefaultTexture()` is called for
  `UMaterialExpressionTextureSampleParameter` subclasses, then `AutoSetSampleType()` runs
  unconditionally. Without that, a default-less sampler parameter would fail with *Missing input
  Texture*. The rescue does **not** apply to the runtime-virtual-texture, sparse-volume, font or curve
  atlas nodes — those still need their asset bound.
- **Metadata is applied last**, after `AutoSetSampleType()`, so an explicit `[SamplerType=…]` always
  wins over the inferred value. See [SamplerType](sampler-type.md).
- `SpriteTextureSampler` lives in the **Paper2D** plugin. With Paper2D disabled the class cannot be
  resolved and generation fails with
  `Could not resolve MaterialExpression class for parameter type 'SpriteTextureSampler'.`
- `DynamicParameter` is not a `UMaterialExpressionParameter`. It has no `Group`, `SortPriority`, `Desc`
  or `ParameterName` UPROPERTY: the first three are skipped with a warning, and `[ParameterName="…"]`
  is a hard error. Its parameter name is written to `ParamNames[0]` instead. See
  [Metadata](metadata.md#organization-fields-that-are-not-reflected).
- `StaticSwitchParameter` cannot be read as a value. A bare reference fails with
  `Unknown Graph identifier '{Name}'.`; it must be called. See
  [Using parameters in Graph](graph-usage.md#staticswitchparameter).
- `CurveAtlasRowParameter`'s `InputTime` pin is **not** reachable from `Graph` — see the diagnostics
  note in [Using parameters in Graph](graph-usage.md#types-that-look-callable-but-are-not).
- The [decompiler](../tools/decompiler.md) emits `SamplerType` and the texture-sample metadata keys
  explicitly on every export, even at their defaults, so a decompile → recompile round trip is stable.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

| Message | Cause |
| :-- | :-- |
| `Invalid boolean default value '{Text}' for property '{Name}'.` | a non-`true`/`false` default on `StaticBoolParameter` or `StaticSwitchParameter` |
| `Invalid scalar default value '{Text}' for property '{Name}'.` | an unparsable scalar default on `ScalarParameter` |
| `Invalid vector default value '{Text}' for property '{Name}'.` | an unparsable vector default on any vector-branch token — including a `Path(…)` written on `FontSampleParameter`, `CurveAtlasRowParameter` or `SpriteTextureSampler` |
| `Invalid texture default value '{Text}' for property '{Name}'. {Inner}` | a texture-object token's `Path(…)` failed to resolve |
| `Invalid texture sample default value '{Text}' for property '{Name}'. {Inner}` | a texture-sample token's `Path(…)` failed to resolve |
| `Could not resolve MaterialExpression class for parameter type '{Token}'.` | no `UMaterialExpression` subclass matched the token under any of the four name spellings |
| `Failed to create a '{Token}' node for property '{Name}'.` | the resolved class could not be instantiated; the message quotes the declared token, not the class it resolved to |
| `'{Class}' does not expose a ParameterName property.` | the class has no `ParameterName` UPROPERTY and is not `DynamicParameter` |
| `'{Class}' does not expose a texture/asset property for property '{Name}'.` | an asset default was given but none of the six asset slots exists on the class |
| `property '{Name}': {Inner}` | wrapper applied to any default-value or metadata failure on this property |
| `Const property '{Name}' must use a plain scalar, vector, or texture type instead of a parameter node or UE builtin declaration.` | `const` applied to any of the 22 tokens |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

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
TextureSampleParameter2D      BaseTex    Group="Surface" SortPriority=0  SamplerType=LinearColor
ScalarParameter               Roughness  Group="Surface" SortPriority=10 SliderMin=0 SliderMax=1
VectorParameter               Tint       Group="Surface" SortPriority=20
ChannelMaskParameter          Pick       Group="Masks"   SortPriority=30 MaskChannel=Red
StaticComponentMaskParameter  Keep       Group="Masks"   SortPriority=40
StaticSwitchParameter         UseDetail  Group="Switches"
TextureCoordinate             UV         CoordinateIndex=0
```

## See also

- [Parameters](index.md) — the hub and the decision table
- [Compact type tokens](compact-types.md) — the 39 built-in tokens and the dimension check they do get
- [Metadata block](metadata.md) — how `[Curve=…]`, `[MaskChannel=…]` and every other key is written
- [SamplerType](sampler-type.md) — every sampler-type value, and `SamplerSource`
- [Path(…)](path.md) — the asset-reference grammar these defaults use
- [Using parameters in Graph](graph-usage.md) — reads, the pin call form, and `StaticSwitchParameter`
- [Properties (section)](../language/properties.md) — the enclosing section grammar
- [UE.Expression](../builtins/ue-expression.md) — the generic reflected form for classes with no token
- [Decompiler](../tools/decompiler.md) — which of these tokens a `UMaterial` export produces
- [Testing](../contributing/testing.md) — the parameter matrix fixture that pins this table
