# SamplerType

> [DreamShader](../index.md) » [Parameters](index.md) » **SamplerType**

The metadata key that sets how a texture node interprets its texture: `EMaterialSamplerType` on the
generated `UMaterialExpressionTextureBase` subclass.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — a `[ … ]` metadata block on a texture-valued `Properties` declaration |
| Kind | reflected metadata key |
| Generates | a write to `SamplerType` on the generated node |
| Since | `1.2.4` |

## Synopsis

```c
<texture-token> <name> [ = <default> ] [ SamplerType = "<value>" [ ; <entry> … ] ] ;
```

There is no dedicated `SamplerType` code path. It is an ordinary
[reflected metadata key](metadata.md#reflected-property-passthrough) that happens to land on a
`TEnumAsByte<EMaterialSamplerType>` UPROPERTY, so it accepts the standard enum-literal spellings and
produces the standard enum diagnostics.

Applicable to every declaration whose generated node derives from `UMaterialExpressionTextureBase`:

| Declaration | Node | `SamplerType` applies |
| :-- | :-- | :-- |
| `Texture2D` / `TextureCube` / `Texture2DArray` / `Texture3D` / `VolumeTexture` | `TextureObjectParameter` | yes |
| `const Texture2D` and the other four compact texture tokens | `TextureObject` | yes |
| `TextureObjectParameter` | `TextureObjectParameter` | yes |
| `TextureSampleParameter2D` … `TextureSampleParameterSubUV` | the matching sample parameter | yes |
| `SpriteTextureSampler` | `SpriteTextureSampler` | yes |
| `TextureCollectionParameter`, `SparseVolumeTextureObjectParameter`, `RuntimeVirtualTextureSampleParameter`, `SparseVolumeTextureSampleParameter` | — | no — these classes are not `UMaterialExpressionTextureBase` subclasses and reject the key |

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
> The enumerator list ends with the sentinel `SAMPLERTYPE_MAX`. It carries no `Hidden` metadata, so
> `[SamplerType="SAMPLERTYPE_MAX"]` and `[SamplerType="MAX"]` resolve rather than erroring, and write a
> value that is not a sampler type. It is not a usable value.

An unmatched string fails with

```text
Metadata property 'SamplerType' on '{Class}': '{Value}' is not a valid enum value for 'SamplerType'.
```

## Inference and override order

| Step | What happens |
| :-- | :-- |
| 1 | The node is created and its texture asset is assigned — from `= Path(…)`, from the engine fallback asset for the declared dimension, or by `SetDefaultTexture()` for a sampler parameter with no asset |
| 2 | `AutoSetSampleType()` runs, deriving `SamplerType` from the assigned texture's compression settings and sRGB flag |
| 3 | The `[ … ]` metadata block is applied |

Because metadata is applied last, an explicit `SamplerType` **always wins** over the inferred value.
Omitting the key means the value follows the asset — which changes if the asset's compression settings
change.

> [!NOTE]
> The [decompiler](../tools/decompiler.md) always emits `SamplerType` explicitly, even when it equals
> the inferred value, so a `UMaterial` → `.dsm` → `UMaterial` round trip does not drift when an asset
> is later recompressed.

## SamplerSource

The companion key on `UMaterialExpressionTextureSample` subclasses — which sampler state the sample
uses. Same enum-literal rules.

| Value | Enum constant | `DisplayName` spelling | Meaning |
| :-- | :-- | :-- | :-- |
| `FromTextureAsset` | `SSM_FromTextureAsset` | `From texture asset` | Take the sampler from the texture; consumes one of the shader's limited sampler slots |
| `Wrap_WorldGroupSettings` | `SSM_Wrap_WorldGroupSettings` | `Shared: Wrap` | Shared sampler, wrap addressing, filter from the world texture group; consumes no slot |
| `Clamp_WorldGroupSettings` | `SSM_Clamp_WorldGroupSettings` | `Shared: Clamp` | Shared sampler, clamp addressing, filter from the world texture group; consumes no slot |
| `TerrainWeightmapGroupSettings` | `SSM_TerrainWeightmapGroupSettings` | — | **Not selectable** — tagged `UMETA(Hidden)`, and hidden enumerators are skipped by the literal matcher |

The default is `FromTextureAsset`. Because `Shared: Wrap` normalizes to `sharedwrap`, both
`[SamplerSource="Shared: Wrap"]` and `[SamplerSource="SharedWrap"]` select it.

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

`MipValueMode` is the one key that changes the node's *pin* set, and therefore what
[the call form](graph-usage.md#pin-names-by-parameter-type) can wire.

## Dimension validation and inference

`SamplerType` describes interpretation; the texture's **dimension** is a separate check.

| Declaration | Declares an explicit dimension | Asset checked against it |
| :-- | :-- | :-- |
| `Texture2D`, `TextureCube`, `Texture2DArray`, `Texture3D`, `VolumeTexture` | yes | **yes** |
| `const` forms of the same five tokens | yes | **yes** |
| `TextureObjectParameter` | no | the expected type is inferred from the loaded asset, so the check can never fail |
| `TextureCollectionParameter`, `SparseVolumeTextureObjectParameter` | no | no |
| `TextureSampleParameter2D` … `SparseVolumeTextureSampleParameter` | yes, inferred from the token spelling | **no** |

The effective texture type is resolved like this:

| Condition | Effective type |
| :-- | :-- |
| The declaration set an explicit dimension | the declared type, as-is |
| No default asset was assigned | the declared type, as-is |
| Otherwise | inferred from the loaded asset: `UTextureCube` → `TextureCube`, `UTexture2DArray` → `Texture2DArray`, `UVolumeTexture` → `VolumeTexture`, anything else → `Texture2D` |

`Texture2D` as an *expected* type means "not a cube, not a 2D array, not a volume". A mismatch is
reported as

```text
{Context} texture property '{Name}' expects {Expected} but '{Path}' is a '{ActualClass}'.
```

where `{Context}` is `Texture` for a parameter and `Const` for a `const` declaration, `{Expected}` is
one of `Texture2D`, `TextureCube`, `Texture2DArray`, `VolumeTexture`, `{Path}` is the declared asset
path or the literal `<default>`, and `{ActualClass}` is the loaded object's class name, or `None`.

> [!WARNING]
> The eight texture-sample tokens set an explicit dimension and are still never validated, because
> they are built through the generic reflected path. A `TextureSampleParameterCube` pointed at a 2D
> texture generates cleanly and fails later inside Unreal's shader compiler. See
> [Parameter node tokens](parameter-nodes.md#dimension-validation-asymmetry).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

| Message | Cause |
| :-- | :-- |
| `Metadata property 'SamplerType' on '{Class}': '{Value}' is not a valid enum value for 'SamplerType'.` | no enumerator matched the value under any of the four spellings |
| `Metadata property 'SamplerType' is not a reflected property on '{Class}'.` | the generated class is not a `UMaterialExpressionTextureBase` subclass |
| `Metadata property 'SamplerSource' on '{Class}': '{Value}' is not a valid enum value for 'SamplerSource'.` | as above, for `SamplerSource` — including an attempt to select the hidden terrain-weightmap value |
| `Texture texture property '{Name}' expects {Expected} but '{Path}' is a '{Class}'.` | dimension mismatch on a compact texture token |
| `Const texture property '{Name}' expects {Expected} but '{Path}' is a '{Class}'.` | dimension mismatch on a `const` texture token |
| `property '{Name}': {Inner}` | wrapper applied to every metadata failure, naming the declaration |

## Example

```c
Shader(Name="Docs/M_SamplerType")
{
    Properties = {
        // Explicit: linear data, shared sampler, no slot consumed.
        TextureSampleParameter2D MaskMap = Path(Game, "Textures/T_Mask") [
            SamplerType   = "LinearColor";
            SamplerSource = "Shared: Wrap";
            MipValueMode  = "None";
        ];

        // Inferred: SamplerType follows the asset's compression settings.
        TextureSampleParameter2D BaseMap = Path(Game, "Textures/T_Base");

        // Any of the four spellings works; this one normalizes to linearcolor.
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
Env      SamplerType = SAMPLERTYPE_LinearColor  (spelling "linear color" normalizes to linearcolor)
Lut      SamplerType = SAMPLERTYPE_Data         on a TextureObject (const) node
```

## See also

- [Parameters](index.md) — the hub and the decision table
- [Metadata block](metadata.md) — the enum-literal rules this key follows
- [Parameter node tokens](parameter-nodes.md) — which tokens expose `SamplerType` at all
- [Compact type tokens](compact-types.md) — the five compact texture tokens and their fallback assets
- [Path(…)](path.md) — the asset-reference grammar
- [Using parameters in Graph](graph-usage.md) — how `MipValueMode` changes the callable pin set
- [Decompiler](../tools/decompiler.md) — the round-trip guarantee for these keys
