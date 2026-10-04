# SamplerType

> [DreamShader](../index.md) » [Parameters](index.md) » **SamplerType**

The metadata key that sets how a texture node interprets its texture: `EMaterialSamplerType` on the
generated `UMaterialExpressionTextureBase` subclass.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — a `[ … ]` metadata block on a texture-valued `Properties` declaration |
| Kind | metadata key |
| Generates | a write to `SamplerType` on the generated node |
| Since | `1.2.4` |

## Synopsis

```c
<texture-token> <name> [ = <default> ] [ SamplerType = "<value>" [ ; <entry> … ] ] ;
```

*(since 2.0.0)* `SamplerType` has a path of its own on a declaration: on a compact texture token or
`TextureObjectParameter` it becomes the 2.0 `/// @sampler` directive, a `SAMPLERTYPE_` prefix removed,
and is resolved by the emitter's [enum-literal matcher](metadata.md#enum-literals)
([`DSH8235`](../diagnostics/DSH8xxx.md#dsh8235) when nothing matches; an empty value is
[`DSH7222`](../diagnostics/DSH7xxx.md#dsh7222)). On a texture-sample token, which is expanded at its
uses, it is an argument of the reflected node, checked against the engine catalog when the source is
bound.

Applicable to every declaration whose generated node derives from `UMaterialExpressionTextureBase`:

| Declaration | Node | `SamplerType` applies |
| :-- | :-- | :-- |
| `Texture2D` / `TextureCube` / `Texture2DArray` / `Texture3D` / `VolumeTexture` | `TextureObjectParameter` | yes |
| `const Texture2D` and the other four compact texture tokens | `TextureObject` | yes |
| `TextureObjectParameter` | `TextureObjectParameter` | yes |
| `TextureSampleParameter2D` … `TextureSampleParameterSubUV` | the matching sample parameter | yes |
| `RuntimeVirtualTextureSampleParameter`, `SparseVolumeTextureSampleParameter` | — | no — these classes have no `SamplerType` ([`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213)) |
| `SpriteTextureSampler`, `TextureCollectionParameter`, `SparseVolumeTextureObjectParameter` | — | the token has no 2.0 form ([`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253)) |

## Values

17 values. Each row is one enumerator; all four spellings in the row select it, and the comparison
strips spaces, `_`, `-`, `:`, `.` and `/` and ignores case — so `"linear color"`, `"Linear-Color"` and
`"linearcolor"` are the same value as `LinearColor`.

| Value | Enum constant | Fully qualified | `DisplayName` spelling |
| :-- | :-- | :-- | :-- |
| `Color` | `SAMPLERTYPE_Color` | `EMaterialSamplerType::SAMPLERTYPE_Color` | `Color` |
| `Grayscale` | `SAMPLERTYPE_Grayscale` | `EMaterialSamplerType::SAMPLERTYPE_Grayscale` | `Grayscale` |
| `Alpha` | `SAMPLERTYPE_Alpha` | `EMaterialSamplerType::SAMPLERTYPE_Alpha` | `Alpha` |
| `Normal` | `SAMPLERTYPE_Normal` | `EMaterialSamplerType::SAMPLERTYPE_Normal` | `Normal` |
| `Masks` | `SAMPLERTYPE_Masks` | `EMaterialSamplerType::SAMPLERTYPE_Masks` | `Masks` |
| `DistanceFieldFont` | `SAMPLERTYPE_DistanceFieldFont` | `EMaterialSamplerType::SAMPLERTYPE_DistanceFieldFont` | `Distance Field Font` |
| `LinearColor` | `SAMPLERTYPE_LinearColor` | `EMaterialSamplerType::SAMPLERTYPE_LinearColor` | `Linear Color` |
| `LinearGrayscale` | `SAMPLERTYPE_LinearGrayscale` | `EMaterialSamplerType::SAMPLERTYPE_LinearGrayscale` | `Linear Grayscale` |
| `Data` | `SAMPLERTYPE_Data` | `EMaterialSamplerType::SAMPLERTYPE_Data` | `Data` |
| `External` | `SAMPLERTYPE_External` | `EMaterialSamplerType::SAMPLERTYPE_External` | `External` |
| `VirtualColor` | `SAMPLERTYPE_VirtualColor` | `EMaterialSamplerType::SAMPLERTYPE_VirtualColor` | `Virtual Color` |
| `VirtualGrayscale` | `SAMPLERTYPE_VirtualGrayscale` | `EMaterialSamplerType::SAMPLERTYPE_VirtualGrayscale` | `Virtual Grayscale` |
| `VirtualAlpha` | `SAMPLERTYPE_VirtualAlpha` | `EMaterialSamplerType::SAMPLERTYPE_VirtualAlpha` | `Virtual Alpha` |
| `VirtualNormal` | `SAMPLERTYPE_VirtualNormal` | `EMaterialSamplerType::SAMPLERTYPE_VirtualNormal` | `Virtual Normal` |
| `VirtualMasks` | `SAMPLERTYPE_VirtualMasks` | `EMaterialSamplerType::SAMPLERTYPE_VirtualMasks` | **`Virtual Mask`** |
| `VirtualLinearColor` | `SAMPLERTYPE_VirtualLinearColor` | `EMaterialSamplerType::SAMPLERTYPE_VirtualLinearColor` | `Virtual Linear Color` |
| `VirtualLinearGrayscale` | `SAMPLERTYPE_VirtualLinearGrayscale` | `EMaterialSamplerType::SAMPLERTYPE_VirtualLinearGrayscale` | `Virtual Linear Grayscale` |

There is no virtual counterpart for `DistanceFieldFont` or `External`.

> [!NOTE]
> On a **texture-sample token** the value is checked against the 17 enumerator names the engine
> catalog lists *(since 2.0.0)*. A spelling that normalizes to one of them (`SAMPLERTYPE_Normal`,
> `linear color`) is accepted with a [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278) warning; a
> `DisplayName` that is not also an enumerator name — `Virtual Mask` — is
> [`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215). On a declaration all four spellings work.

> [!NOTE]
> The enumerator list ends with the sentinel `SAMPLERTYPE_MAX`. It carries no `Hidden` metadata, so
> on a declaration `[SamplerType="SAMPLERTYPE_MAX"]` and `[SamplerType="MAX"]` resolve rather than
> erroring, and write a value that is not a sampler type. It is not a usable value. The catalog a
> texture-sample token is checked against does not list it (`DSH5215`).

## Inference and override order

| Step | What happens |
| :-- | :-- |
| 1 | The node is created and its texture asset is assigned — from `= Path(…)`, from the engine fallback asset for the declared dimension, or by `SetDefaultTexture()` for a sampler parameter with no asset |
| 2 | If the source gives no `SamplerType`, `AutoSetSampleType()` derives one from the assigned texture's compression settings and sRGB flag |
| 3 | If it does, that value is written |

An explicit `SamplerType` **always wins**: the inference does not run at all on a node that has one.
Omitting the key means the value follows the asset — which changes if the asset's compression settings
change.

> [!NOTE]
> The [decompiler](../tools/decompiler.md) always emits `SamplerType` explicitly, even when it equals
> the inferred value, so a `UMaterial` → `.dsm` → `UMaterial` round trip does not drift when an asset
> is later recompressed.

## SamplerSource

The companion key on `UMaterialExpressionTextureSample` subclasses — which sampler state the sample
uses. It applies to the texture-sample tokens only, so its value is checked against the catalog as
above.

| Value | Enum constant | `DisplayName` spelling | Meaning |
| :-- | :-- | :-- | :-- |
| `FromTextureAsset` | `SSM_FromTextureAsset` | `From texture asset` | Take the sampler from the texture; consumes one of the shader's limited sampler slots |
| `Wrap_WorldGroupSettings` | `SSM_Wrap_WorldGroupSettings` | `Shared: Wrap` | Shared sampler, wrap addressing, filter from the world texture group; consumes no slot |
| `Clamp_WorldGroupSettings` | `SSM_Clamp_WorldGroupSettings` | `Shared: Clamp` | Shared sampler, clamp addressing, filter from the world texture group; consumes no slot |
| `TerrainWeightmapGroupSettings` | `SSM_TerrainWeightmapGroupSettings` | — | **Not selectable** — tagged `UMETA(Hidden)`, and not in the catalog |

The default is `FromTextureAsset`. Write the enumerator name — `[SamplerSource="Wrap_WorldGroupSettings"]`;
`SSM_Wrap_WorldGroupSettings` is accepted with `DSH5278`. The display names `Shared: Wrap` and
`Shared: Clamp` normalize to nothing in the catalog and are `DSH5215` *(since 2.0.0; 1.x accepted
them)*.

## Related texture-sample keys

These are the keys the decompiler emits on every texture-sample parameter, with the value it treats as
the default. All are ordinary reflected metadata.

| Key | Type | Default |
| :-- | :-- | :-- |
| `SamplerType` | `EMaterialSamplerType` | derived from the asset |
| `SamplerSource` | `ESamplerSourceMode` | `FromTextureAsset` |
| `MipValueMode` | `ETextureMipValueMode` | `None` |
| `GatherMode` *(since UE 5.6)* | enum | `None` |
| `AutomaticViewMipBias` | bool | `true` |
| `ConstCoordinate` | `uint8` | `0` |
| `ConstMipValue` | `int32` | `-1` |
| `IsDefaultMeshpaintTexture` | bool | `false` |
| `SortPriority` | `int32` | `32` |

`MipValueMode` is the one key that changes the node's *pin* names, and therefore which names
[the call form](graph-usage.md#pin-names-by-parameter-type) can match on the live node.

## Dimension validation and inference

`SamplerType` describes interpretation; the texture's **dimension** is a separate check.

| Declaration | Declares a dimension | What is checked |
| :-- | :-- | :-- |
| `Texture2D`, `TextureCube`, `Texture2DArray`, `Texture3D`, `VolumeTexture` | yes | a `Class'…'` shell of another dimension ([`DSH1044`](../diagnostics/DSH1xxx.md#dsh1044)); the loaded asset is not checked *(since 2.0.0)* |
| `const` forms of the same five tokens | yes | as above |
| `TextureObjectParameter` | no | the dimension is taken from the loaded asset |
| `TextureSampleParameter2D` / `2DArray` / `Cube` / `Volume` / `SubUV` | yes, from the token's suffix | the shell, as for a compact token |
| `TextureSampleParameterCubeArray` | no | only that a shell names a texture ([`DSH1043`](../diagnostics/DSH1xxx.md#dsh1043)) |

`TextureObjectParameter`'s dimension is read off the asset its default loads: `UTextureCube` →
`TextureCube`, `UTexture2DArray` → `Texture2DArray`, `UVolumeTexture` → `VolumeTexture`, anything
else — or a default that does not load — `Texture2D`.

> [!WARNING]
> A `TextureSampleParameterCube` pointed at a 2D texture by its plain object path generates cleanly
> and fails later inside Unreal's shader compiler. See
> [Parameter node tokens](parameter-nodes.md#dimension-validation-asymmetry).

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH8235` | a declaration's `SamplerType` matches no enumerator under any of the four spellings |
| `DSH7222` | a declaration's `SamplerType` is empty |
| `DSH5215` | a texture-sample token's `SamplerType` or `SamplerSource` is no enumerator the catalog lists |
| `DSH5278` | a texture-sample token's value is a 1.x spelling of an enumerator (warning) |
| `DSH5213` | the node class has no `SamplerType` (or no `SamplerSource`) |
| `DSH1043` / `DSH1044` | a texture default's `Class'…'` names a non-texture / a texture of another dimension |

## Example

```c
Shader(Name="Docs/M_SamplerType")
{
    Properties = {
        // Explicit: linear data, shared sampler, no slot consumed.
        TextureSampleParameter2D MaskMap = Path(Game, "Textures/T_Mask") [
            SamplerType   = "LinearColor";
            SamplerSource = "Wrap_WorldGroupSettings";
            MipValueMode  = "None";
        ];

        // Inferred: SamplerType follows the asset's compression settings.
        TextureSampleParameter2D BaseMap = Path(Game, "Textures/T_Base");

        // A 1.x spelling: accepted, with DSH5278.
        TextureSampleParameterCube Env = Path(Engine, "EngineResources/DefaultTextureCube") [
            SamplerType = "linear color"
        ];

        const Texture2D Lut = Path(Game, "Textures/T_Lut") [SamplerType = "Data"];
    }

    Settings = { Domain = "Surface"; ShadingModel = "DefaultLit"; BlendMode = "Opaque"; }
    Outputs  = { vec3 Color; Base.BaseColor = Color; }

    Graph = {
        vec2 UV = UE.TexCoord(Index = 0);
        vec4 B  = BaseMap(Coordinates = UV);
        vec4 M  = MaskMap(Coordinates = UV);
        Color = B.rgb * M.r;
    }
}
```

Applied values:

```text
MaskMap  SamplerType = SAMPLERTYPE_LinearColor  SamplerSource = SSM_Wrap_WorldGroupSettings
BaseMap  SamplerType = <AutoSetSampleType from /Game/Textures/T_Base>
Env      SamplerType = SAMPLERTYPE_LinearColor  ("linear color" normalizes to LinearColor; DSH5278)
Lut      SamplerType = SAMPLERTYPE_Data         on a TextureObject (const) node
```

`Env` and `Lut` are applied when a body reads them; this `Graph` does not, so they build nothing.

## See also

- [Parameters](index.md) — the hub and the decision table
- [Metadata block](metadata.md) — the enum-literal rules this key follows
- [Parameter node tokens](parameter-nodes.md) — which tokens expose `SamplerType` at all
- [Compact type tokens](compact-types.md) — the five compact texture tokens and their fallback assets
- [Path(…)](path.md) — the asset-reference grammar
- [Using parameters in Graph](graph-usage.md) — how `MipValueMode` changes the callable pin names
- [Decompiler](../tools/decompiler.md) — the round-trip guarantee for these keys
