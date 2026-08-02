# Shader settings

> [DreamShader](../index.md) » [Settings](index.md) » **Shader settings**

The `Settings` section of a [`Shader`](../language/shader.md) block: six hand-handled keys plus a
reflection resolver that writes any other key onto the generated `UMaterial`.

| | |
| :-- | :-- |
| Declared in | `.dsm` — inside a `Shader( … ) { … }` block |
| Kind | section |
| Generates | property writes on the generated `UMaterial` (under the ThinCustom backend, on the hidden base material) |

## Synopsis

```c
Shader(Name = "<asset-path>")
{
    Settings [=] {
        [BlendMode     = <blend-mode>;]     // or RenderType
        [ShadingModel  = <shading-model>;]
        [MaterialDomain = <domain>;]        // or Domain
        [Backend       = { Graph | ThinCustom | Instance };]
        [<property-path> = <literal>;] …
    }
}

<property-path> := <segment> [ . <segment> ] …
<segment>       := <property-name> [ [<integer>] ]
```

`[<integer>]` is literal DreamShaderLang punctuation; the enclosing `[ … ]` is the optional-marker
meta-bracket. The block grammar, comment handling, statement splitting and quoting rules are common
to every `Settings` section and are specified in [Settings](index.md#how-a-statement-is-parsed).

## Special keys

These six keys — matched after trimming, lowercasing **and** deleting spaces, `_` and `-` — never
reach the reflection resolver.

```text
blendmode   rendertype   shadingmodel   materialdomain   domain   backend
```

| Canonical key | Synonym | Value grammar | Value when the key is absent | Effect |
| :-- | :-- | :-- | :-- | :-- |
| **`BlendMode`** | `RenderType` | one of the [blend-mode spellings](material-enums.md#blendmode) | `Opaque` | sets `UMaterial::BlendMode` |
| **`ShadingModel`** | — | one of the [shading-model spellings](material-enums.md#shadingmodel) | `DefaultLit` | calls `SetShadingModel` |
| **`MaterialDomain`** | `Domain` | one of the [domain spellings](material-enums.md#domain) | `Surface` | sets `UMaterial::MaterialDomain` |
| **`Backend`** | — | `Graph`, `ThinCustom`, `Instance`, or the empty string | the project's *Default Compiler Backend* | selects the materialization strategy — see [Backend](backend.md) |

When both a canonical key and its synonym are present, the **canonical** key wins: `BlendMode` beats
`RenderType`, `MaterialDomain` beats `Domain`. There is no diagnostic for the conflict.

Key spelling for these four is looked up by trim-and-lowercase only, so `blendmode`, `BlendMode` and
`BLENDMODE` all work.

> [!WARNING]
> A spelling that differs from a special key only by spaces, underscores or hyphens is **silently
> dropped**. `Blend_Mode = "Translucent";` does not set the blend mode: the direct probe for
> `BlendMode` misses the stored key `blend_mode`, and the reflection loop skips it because its
> separator-stripped form is on the special-key list. No error, no warning, no effect. The same
> applies to `Render_Type`, `Shading Model`, `Material-Domain`, `Domain ` with an internal space, and
> `Back_end`. Write the six names without separators.

### Application order

1. The **whole** block is validated first — every special value is resolved and every generic value
   is written to a throw-away transient `UMaterial`. A single bad value aborts before the real
   material is touched.
2. `BlendMode`, then `ShadingModel`, then `MaterialDomain`.
3. The generic keys, in the parsed map's iteration order.

> [!NOTE]
> The generic pass iterates a hash map. **No ordering is guaranteed between two generic keys**, so do
> not rely on one key being written before another. The three special keys are always written before
> any generic key, which is why a generic `BlendMode`-adjacent property such as
> `TranslucencyLightingMode` sees the final blend mode.

`Backend` is consumed before any material exists — an unrecognized `Backend` value fails the compile
*before* the rest of `Settings` is validated.

### Interaction with `Base.FrontMaterial`

When any output binds `Base.FrontMaterial`, an explicit `ShadingModel` that is not `Substrate` or
`Strata` (compared case-insensitively after trimming) is a hard error; otherwise the shading model is
force-set to Substrate after the block is applied. Binding `Base.FrontMaterial` and
`Base.MaterialAttributes` on the same `Shader` is also a hard error. See
[Output bindings](../language/output-bindings.md) and [Substrate builtins](../builtins/substrate.md).

## The reflection resolver

Every key that is not special is resolved against the generated material by Unreal reflection.

| # | Step | Detail |
| --: | :-- | :-- |
| 1 | Split the key into segments on `.` at bracket depth 0 | `Lightmass.DiffuseBoost` → `Lightmass`, `DiffuseBoost`. A `.` inside `[ … ]` does not split. |
| 2 | Parse each segment's optional trailing `[<integer>]` | the index must be a non-negative integer and `]` must be the segment's last character |
| 3 | Map the segment name through the [alias table](#alias-table) | applied **per segment**, before the field scan |
| 4 | Scan `TFieldIterator<FProperty>` over the current struct, **including super-classes** | so `UMaterial`, `UMaterialInterface` and `UObject` properties are all reachable |
| 5 | Descend | a non-terminal segment must be an `FStructProperty`; the walk continues inside the struct |
| 6 | Write the value | parsed according to the resolved property's C++ type — see [Value grammar](#value-grammar) |

### Property-name matching

A segment matches a property when any of these three, after deleting spaces, `_` and `-` and
lowercasing, is equal to the segment:

| Rule | Example |
| :-- | :-- |
| The raw `FProperty` name | `TwoSided` ← `TwoSided`, `two_sided`, `TWO SIDED`, `two-sided` |
| The property name **with a leading `b` stripped**, when the name is `b` followed by an uppercase letter | `bFullyRough` ← `FullyRough`; `bIsSky` ← `IsSky`; `bIsThinSurface` ← `IsThinSurface` |
| The property's `DisplayName` metadata | whatever the engine declares for that property |

The `b`-stripping rule is one-way and permissive: the full name still matches, so both
`bFullyRough = true;` and `FullyRough = true;` resolve to the same property. A property whose name
begins with a lowercase `b` followed by a non-uppercase character (for example `bias`) is not
b-stripped.

### Nested and indexed paths

| Form | Meaning | Example |
| :-- | :-- | :-- |
| `A.B` | `B` inside the struct property `A` | `Lightmass.DiffuseBoost = 1.5;` |
| `A.B.C` | arbitrary depth, each non-terminal an `FStructProperty` | `NaniteOverrideMaterial.bEnableOverride = true;` |
| `A[N]` | element `N` of a fixed-size C array (`ArrayDim > 1`) | `PhysicalMaterialMap[2] = Path(Game, "Physics/PM_Metal");` |
| `A[N].B` | a member of an indexed struct element | index and path segments compose freely |

`[N]` on a property whose `ArrayDim` is 1 is an error; omitting `[N]` on a property whose `ArrayDim`
is greater than 1 is also an error. `TArray`, `TMap` and `TSet` properties are **not** indexable this
way — they fall through to the `ImportText` catch-all in [Value grammar](#value-grammar).

### Alias table

Ten fixed key aliases are applied per path segment before the field scan. Alias keys are compared in
their separator-stripped, lowercased form, so `Lighting_Mode`, `Lighting Mode` and `LIGHTINGMODE` all
hit the first row.

| Alias | Resolves to |
| :-- | :-- |
| `LightingMode` | `TranslucencyLightingMode` |
| `TranslucentLightingMode` | `TranslucencyLightingMode` |
| `RefractionMode` | `RefractionMethod` |
| `PhysicalMaterial` | `PhysMaterial` |
| `PhysicalMaterialMask` | `PhysMaterialMask` |
| `Lightmass` | `LightmassSettings` |
| `MobileSeparateTranslucency` | `bEnableMobileSeparateTranslucency` |
| `AlwaysEvaluateWorldPositionOffset` | `bAlwaysEvaluateWorldPositionOffset` |
| `ResponsiveAA` | `bEnableResponsiveAA` |
| `ThinSurface` | `bIsThinSurface` |

Because the alias applies per segment, `Lightmass.DiffuseBoost` resolves as
`LightmassSettings` → `DiffuseBoost`.

## Value grammar

The resolved property's C++ type decides how the value text is parsed. The value has already been
unquoted by the parser, so `true` and `"true"` are the same input.

| Property type | Accepted literal | Failure message |
| :-- | :-- | :-- |
| `bool` | `true` / `false`, case-insensitive, trimmed | `'{Value}' is not a valid boolean value for '{Property}'.` |
| `int32` | signed integer literal | `'{Value}' is not a valid integer value for '{Property}'.` |
| `uint32` | integer in `[0, 4294967295]` | `'{Value}' is not a valid unsigned integer value for '{Property}'.` |
| `float` | any numeric literal; also `true` → `1.0` and `false` → `0.0` | `'{Value}' is not a valid numeric value for '{Property}'.` |
| `double` | as `float` | `'{Value}' is not a valid numeric value for '{Property}'.` |
| `FString` | any text, trimmed — **never fails** | — |
| `FName` | any text, trimmed — **never fails** | — |
| object reference | `Path( … )` or an absolute object path such as `/Game/Textures/T_Noise` | `Object property '{Property}' expects Path(...) or an absolute Unreal object path.`; `Failed to load asset '{Path}' for '{Property}'.`; `Asset '{Path}' is not compatible with '{Property}'. Expected '{Class}'.` |
| `enum class` (`FEnumProperty`) | an [enum literal](#enum-literals) | `'{Value}' is not a valid enum value for '{Property}'.` |
| `uint8` enum (`FByteProperty` with an enum) | an [enum literal](#enum-literals) | `'{Value}' is not a valid enum value for '{Property}'.` |
| plain `uint8` | integer in `[0, 255]` | `'{Value}' is not a valid byte value for '{Property}'.` |
| anything else | Unreal struct-literal text, e.g. `(R=1.0,G=0.0,B=0.0,A=1.0)` | `Property '{Property}' on '{Object}' is not a supported literal type yet.` |

Whichever message applies is wrapped by the caller:
`Invalid value '{Value}' for setting '{Key}'. {TypeMessage}`.

> [!NOTE]
> An object-typed property whose class derives from `UTexture` **and** whose property name is exactly
> `Texture` or `TextureObject` writes `nullptr` instead of erroring when the asset fails to load.
> This is the material-expression convention; it also applies here.

> [!WARNING]
> When an object-typed value *looks* like a path — it starts with `Path(` or `/` — but fails to
> resolve, the type-specific explanation is empty and the diagnostic degenerates to
> `Invalid value '/Game/Nope' for setting 'physmaterial'. ` with nothing after the period. Check that
> the asset exists and that its class matches the property.

### Enum literals

An enum-typed value is matched after trimming, lowercasing and deleting every space, `_`, `-`, `:`,
`.` and `/`. Values carrying `Hidden` metadata are skipped. A candidate matches any of:

| Form | Example for `ETranslucencyLightingMode::TLM_Surface` |
| :-- | :-- |
| the short enum name | `TLM_Surface` |
| the fully-qualified name | `ETranslucencyLightingMode::TLM_Surface` |
| the display name | `Surface` |
| the short name with everything up to the first `_` removed | `Surface` |

This matching is separate from the `ShadingModel` / `BlendMode` / `Domain` alias maps described in
[Material enums](material-enums.md); those three keys never reach this code.

### `Path( … )` values

| Form | Meaning |
| :-- | :-- |
| `Path("/Game/Foo/Bar")` | absolute object path, one argument |
| `Path(Game, "Foo/Bar")` | `/Game/Foo/Bar` |
| `Path(Engine, "Foo/Bar")` | `/Engine/Foo/Bar` |
| `Path(Plugin.PluginName, "Foo/Bar")` | the named plugin's content root |
| `/Game/Foo/Bar` | bare absolute path, no `Path( … )` wrapper |

Backslashes are normalized to `/` and surrounding quotes are trimmed. The complete root catalogue and
its errors are in [Path](../parameters/path.md).

## Validation

Before anything is written to the real material, each generic key/value pair is applied to a
transient probe `UMaterial` created in the transient package. The probe write and the real write use
the same code, so a value that validates always applies. The only failure specific to this stage is
`Failed to create a transient material for Settings validation.`

## Defaults — what an omitted setting resets to

A generated material is reset immediately before `Settings` is applied. An omitted key is therefore
**not** "left as it was on the previous generation" — it takes the value below.

| Property | Reset value |
| :-- | :-- |
| `BlendMode` | `Opaque` |
| `MaterialDomain` | `Surface` |
| shading model | `DefaultLit` |
| `TwoSided` | `false` |
| `OpacityMaskClipValue` | `0.3333` |
| `Wireframe` | `false` |
| `DitheredLODTransition` | `false` |
| `DitherOpacityMask` | `false` |
| `bAllowNegativeEmissiveColor` | `false` |
| `bCastDynamicShadowAsMasked` | `false` |
| `bCastRayTracedShadows` | `true` |
| `bEnableResponsiveAA` | `false` |
| `bScreenSpaceReflections` | `false` |
| `bContactShadows` | `false` |
| `bDisableDepthTest` | `false` |
| `bOutputTranslucentVelocity` | `false` |
| `bWriteOnlyAlpha` | `false` |
| `BlendableOutputAlpha` | `false` |
| `TranslucencyLightingMode` | `TLM_VolumetricNonDirectional` |
| `bTangentSpaceNormal` | `true` |
| `bAlwaysEvaluateWorldPositionOffset` | `false` |
| `bFullyRough` | `false` |
| `bIsSky` | `false` |
| `bIsThinSurface` | `false` |
| `MaterialDecalResponse` | `MDR_ColorNormalRoughness` |
| `bHasPixelAnimation` | `false` *(since UE 5.4)* |
| `NumCustomizedUVs` | `0` |

Every other `UMaterial` property keeps whatever a freshly constructed material has.

## The round-trip set

The [decompiler](../tools/decompiler.md) emits `Domain`, `ShadingModel` and `BlendMode`
unconditionally, plus each of the following when it differs from the `UMaterial` class default. This
is the set guaranteed to survive a `UMaterial` → `.dsm` → `UMaterial` round trip; it is a subset of
what `Settings` accepts, not a limit on it.

**Booleans (39, in emit order)**

```text
TwoSided                    Wireframe                     DitheredLODTransition
DitherOpacityMask           bAllowNegativeEmissiveColor   bCastDynamicShadowAsMasked
bEnableResponsiveAA         bScreenSpaceReflections       bContactShadows
bDisableDepthTest           bOutputTranslucentVelocity    bTangentSpaceNormal
bFullyRough                 bIsSky                        bIsThinSurface
bHasPixelAnimation          bUsedWithSkeletalMesh         bUsedWithMorphTargets
bUsedWithClothing           bUsedWithNanite               bUsedWithEditorCompositing
bUsedWithParticleSprites    bUsedWithBeamTrails           bUsedWithMeshParticles
bUsedWithNiagaraSprites     bUsedWithNiagaraRibbons       bUsedWithNiagaraMeshParticles
bUsedWithGeometryCache      bUsedWithStaticLighting       bUsedWithSplineMeshes
bUsedWithInstancedStaticMeshes                            bUsedWithGeometryCollections
bUsedWithHairStrands        bUsedWithWater                bUsedWithVirtualHeightfieldMesh
bCastRayTracedShadows       bWriteOnlyAlpha               BlendableOutputAlpha
bAlwaysEvaluateWorldPositionOffset
```

`bHasPixelAnimation` is emitted only on UE 5.4 and newer.

**Enums (1)** — `MaterialDecalResponse`.

Reachable but never emitted by the decompiler, and therefore lost on a round trip:
`OpacityMaskClipValue`, `NumCustomizedUVs`, `TranslucencyLightingMode`, `RefractionMethod`,
`RefractionDepthBias`, `TranslucencyPass`, `ShadingRate`, `FloatPrecisionMode`, `BlendableLocation`,
`BlendablePriority`, `bIsBlendable`, `UserSceneTexture`, `StencilCompare`, `StencilRefValue`,
`bEnableStencilTest`, `MaxWorldPositionOffsetDisplacement`, `PhysMaterial`, `PhysMaterialMask`,
`PhysicalMaterialMap[N]`, `Lightmass.*`, `DisplacementScaling.*`, `NaniteOverrideMaterial.*`, and
every other engine property the resolver can reach. Exact availability is engine-version dependent —
the resolver is pure reflection over the engine's own property set.

## Notes

- The reflection surface is the **engine's**, not the plugin's. A custom or modified engine build
  exposes its own properties and its own enum values through the same resolver, so this manual's
  tables describe the stock UE 5.3 – 5.8 surface only.
- Under the [ThinCustom backend](backend.md) every setting lands on the **hidden base `UMaterial`**,
  not on the emitted `UDreamShaderMaterialInstance`. Reading the blend mode off the instance shows
  the inherited value.
- A `Settings` block on a [`ShaderFunction`](function.md) does **not** share this behaviour. Only
  four keys are read there, and unknown keys — including every key on this page — are ignored without
  a diagnostic.
- Duplicate keys across multiple `Settings` sections merge, last one wins. See
  [Settings](index.md#how-a-statement-is-parsed).
- The compiler prefixes every message below with the source file path.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this page. Because keys are lowercased
when they are stored, the `{Key}` in these messages is the **lowercased** spelling, not what the
source wrote.

| Message | Cause |
| :-- | :-- |
| `Unsupported BlendMode/RenderType '{Value}'.` | the `BlendMode` / `RenderType` value matched no alias |
| `Unsupported ShadingModel '{Value}'.` | the `ShadingModel` value matched no alias |
| `ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.` | the value trims and case-folds to `Substrate` or `Strata` on UE 5.3 |
| `Unsupported MaterialDomain '{Value}'.` | the `MaterialDomain` / `Domain` value matched no alias |
| `Unsupported material setting '{Key}'.` | no property matched a path segment — the usual "unknown key" error |
| `Setting path segment cannot be empty.` | an empty `.`-delimited segment, as in `Foo..Bar` |
| `Invalid array setting segment '{Segment}'.` | malformed brackets — no `]`, `]` before `[`, `]` not last, or nothing before `[` |
| `Invalid array index '{Index}' in setting segment '{Segment}'.` | the index is not an integer, or is negative |
| `Setting '{Segment}' is not an indexed array property.` | `[N]` used on a property whose `ArrayDim` is 1 |
| `Array index {Index} is out of range for setting '{Segment}' (max {Max}).` | the index is at or beyond `ArrayDim` |
| `Setting '{Segment}' requires an explicit [index].` | a fixed-array property addressed without `[N]` |
| `Setting path '{Key}' cannot continue through '{Segment}'.` | a non-terminal segment is not a struct property |
| `Invalid material setting target.` | the resolver was handed no object |
| `Invalid material setting path '{Key}'.` | the key produced no path segments |
| `Failed to create a transient material for Settings validation.` | the probe material could not be allocated |
| `Invalid value '{Value}' for setting '{Key}'. {TypeMessage}` | the literal write failed; `{TypeMessage}` is the type-specific text from [Value grammar](#value-grammar) and is empty for a path-shaped object value that failed to resolve |
| `{File}: Base.FrontMaterial requires ShadingModel="Substrate" or no explicit ShadingModel setting.` | an explicit non-Substrate shading model with a `Base.FrontMaterial` binding |
| `{File}: Base.FrontMaterial and Base.MaterialAttributes cannot be used by the same Shader.` | both bindings on one `Shader` |

`Unsupported Backend '{Value}'. Supported values: Graph, Instance, ThinCustom.` is raised earlier, by
the backend resolver — see [Backend](backend.md#diagnostics).

## Example

```c
Shader(Name="Docs/M_ShaderSettings", Root="Game")
{
    Properties {
        VectorParameter BaseColor = float4(0.8, 0.8, 0.8, 1.0) [Group="Surface"];
        ScalarParameter Roughness = 0.55                       [Group="Surface"; Slider(0, 1)];
    }

    Settings {
        // Special keys.
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Masked";

        // Reflected booleans; the b-prefix is optional.
        TwoSided   = true;
        FullyRough = true;
        bIsSky     = false;

        // Reflected scalar and enum.
        OpacityMaskClipValue = 0.25;
        MaterialDecalResponse = "ColorNormalRoughness";

        // Alias -> TranslucencyLightingMode, matched by display name.
        LightingMode = "Surface";

        // Nested struct path.
        Lightmass.DiffuseBoost = 1.5;

        // Object reference.
        PhysicalMaterial = Path(Engine, "EngineMaterials/DefaultPhysicalMaterial");
    }

    Outputs {
        vec3  Color;
        float Rough;
        float Mask;
        Base.BaseColor      = Color;
        Base.Roughness      = Rough;
        Base.OpacityMask    = Mask;
    }

    Graph {
        Color = BaseColor.rgb;
        Rough = Roughness;
        Mask  = BaseColor.a;
    }
}
```

Resulting material state:

```text
BlendMode                = BLEND_Masked
MaterialDomain           = MD_Surface
ShadingModel             = MSM_DefaultLit
TwoSided                 = true
bFullyRough              = true
bIsSky                   = false
OpacityMaskClipValue     = 0.25
MaterialDecalResponse    = MDR_ColorNormalRoughness
TranslucencyLightingMode = TLM_Surface
LightmassSettings.DiffuseBoost = 1.5
PhysMaterial             = /Engine/EngineMaterials/DefaultPhysicalMaterial
```

## See also

- [Settings](index.md) — the block grammar and key normalization shared by every block kind
- [Material enums](material-enums.md) — every accepted `ShadingModel`, `BlendMode` and `Domain` value
- [Backend](backend.md) — the fourth special key
- [Function settings](function.md) — why none of this applies inside a `ShaderFunction`
- [Project settings](project.md) — the mapping maps that extend the enum spellings
- [Shader](../language/shader.md) — the enclosing block
- [Output bindings](../language/output-bindings.md) — `Base.FrontMaterial` and `Base.MaterialAttributes`
- [Path](../parameters/path.md) — the `Path( … )` grammar used by object-typed settings
- [Metadata](../parameters/metadata.md) — the analogous reflected-property block on parameters
- [Decompiler](../tools/decompiler.md) — which settings survive a round trip
- [Regeneration](../generation/regeneration.md) — what a rebuild resets
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
