# Shader settings

> [DreamShader](../index.md) » [Settings](index.md) » **Shader settings**

The `Settings` section of a [`Shader`](../language/shader.md) block: a few hand-handled keys plus a
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
        [Backend       = { Graph | ThinCustom };]
        [Substrate     = { Legacy | Bridge | Native };]
        [<property-name> = <literal>;] …
    }
}
```

The block grammar, comment handling, statement splitting and quoting rules are common to every
`Settings` section and are specified in [Settings](index.md#how-a-statement-is-parsed). Since 2.0.0 a
key is one name: a nested path or an index is [`DSH3261`](../diagnostics/DSH3xxx.md#dsh3261).

The keys and values are those of the 2.0 `#pragma material(…)`; a `Shader`'s `Settings` becomes one.

## Special keys

These keys never reach the reflection resolver. The emitter skips the six whose name — after
trimming, lowercasing **and** deleting spaces, `_` and `-` — is one of:

```text
blendmode   rendertype   shadingmodel   materialdomain   domain   backend
```

| Canonical key | Synonym | Value grammar | Value when the key is absent | Effect |
| :-- | :-- | :-- | :-- | :-- |
| **`BlendMode`** | `RenderType` | one of the [blend-mode spellings](material-enums.md#blendmode) | `Opaque` | sets `UMaterial::BlendMode` |
| **`ShadingModel`** | — | one of the [shading-model spellings](material-enums.md#shadingmodel) | `DefaultLit` | calls `SetShadingModel` |
| **`MaterialDomain`** | `Domain` | one of the [domain spellings](material-enums.md#domain) | `Surface` | sets `UMaterial::MaterialDomain` |
| **`Backend`** | — | `Graph` or `ThinCustom`; `Instance` is the old spelling of `ThinCustom` | the project's *Default Compiler Backend* | selects the materialization strategy — see [Backend](backend.md) |

One more key is read by the compiler and never reaches the material *(since 2.0.0)*:

| Key | Value grammar | Value when the key is absent | Effect |
| :-- | :-- | :-- | :-- |
| **`Substrate`** | `Legacy`, `Bridge`, `Native` | `Legacy` | how a material written against the legacy attributes is read in a project that has Substrate on — `Bridge` folds the shading attributes of a Surface material into one `Substrate.ShadingModels` node on `FrontMaterial`. See [Substrate sugar](../language-v2/substrate.md#one-source-two-kinds-of-project--substrate-). An unknown mode is [`DSH7232`](../diagnostics/DSH7xxx.md#dsh7232). |

`Backend` and `Substrate` are taken out of the settings when the source is bound, matched ignoring
case; the rest is applied when the material is built.

When both a canonical key and its synonym are present, the **canonical** key wins: `BlendMode` beats
`RenderType`, `MaterialDomain` beats `Domain`. There is no diagnostic for the conflict.

Key spelling for these is looked up by trim-and-lowercase only, so `blendmode`, `BlendMode` and
`BLENDMODE` all work.

> [!WARNING]
> A spelling that differs from a special key only by underscores is **silently dropped**.
> `Blend_Mode = "Translucent";` does not set the blend mode: the direct probe for `BlendMode` misses
> the stored key `blend_mode`, and the reflection pass skips it because its separator-stripped form is
> on the special-key list. No error, no warning, no effect. The same applies to `Render_Type`,
> `Shading_Model`, `Material_Domain` and `Back_end`. Write the names without separators. (A space or a
> hyphen cannot stand in a key since 2.0.0: [`DSH3261`](../diagnostics/DSH3xxx.md#dsh3261).)

### Application order

1. The material is reset to the [defaults](#defaults--what-an-omitted-setting-resets-to).
2. The **whole** block is validated — every special value is resolved and every generic value is
   written to a throw-away transient `UMaterial`. A single bad value stops the build before the real
   material is touched, and the rebuild is rolled back.
3. `BlendMode`, then `ShadingModel`, then `MaterialDomain`; then `bUsedWithVolumetricCloud` follows
   the domain.
4. The generic keys, in the settings' iteration order.

> [!NOTE]
> **No ordering is guaranteed between two generic keys**, so do not rely on one key being written
> before another. The special keys are always written before any generic key, which is why a generic
> `BlendMode`-adjacent property such as `TranslucencyLightingMode` sees the final blend mode.

`Backend` is consumed before any material exists — an unrecognized `Backend` value is a binding error,
which stops the compile before the rest of `Settings` is applied, so no settings diagnostic of the
build is reported for that file.

### Interaction with `Base.FrontMaterial`

*(since 2.0.0)* The two 1.x checks on a `Base.FrontMaterial` binding — an explicit `ShadingModel`
that is not `Substrate` or `Strata`, and `Base.FrontMaterial` together with `Base.MaterialAttributes`
on one `Shader` — have no 2.0 diagnostic, and the compiler does not force the shading model to
Substrate: an explicit `ShadingModel` is applied as written. See
[Output bindings](../language/output-bindings.md), [Substrate builtins](../builtins/substrate.md) and,
for `Substrate = Bridge`, [Substrate sugar](../language-v2/substrate.md).

## The reflection resolver

Every key that is not special is resolved against the generated material by Unreal reflection.

| # | Step | Detail |
| --: | :-- | :-- |
| 1 | Split the key into segments on `.` at bracket depth 0 | a 1.x key is one segment since 2.0.0 (see [Nested and indexed paths](#nested-and-indexed-paths)) |
| 2 | Parse each segment's optional trailing `[<integer>]` | as above |
| 3 | Map the segment name through the [alias table](#alias-table) | applied **per segment**, before the field scan |
| 4 | Scan `TFieldIterator<FProperty>` over the current struct, **including super-classes** | so `UMaterial`, `UMaterialInterface` and `UObject` properties are all reachable |
| 5 | Descend | a non-terminal segment must be an `FStructProperty`; the walk continues inside the struct |
| 6 | Write the value | parsed according to the resolved property's C++ type — see [Value grammar](#value-grammar) |

No property of that name is [`DSH7118`](../diagnostics/DSH7xxx.md#dsh7118).

### Property-name matching

A segment matches a property when any of these three, after deleting spaces, `_` and `-` and
lowercasing, is equal to the segment:

| Rule | Example |
| :-- | :-- |
| The raw `FProperty` name | `TwoSided` ← `TwoSided`, `two_sided`, `TWOSIDED` |
| The property name **with a leading `b` stripped**, when the name is `b` followed by an uppercase letter | `bFullyRough` ← `FullyRough`; `bIsSky` ← `IsSky`; `bIsThinSurface` ← `IsThinSurface` |
| The property's `DisplayName` metadata | whatever the engine declares for that property |

The `b`-stripping rule is one-way and permissive: the full name still matches, so both
`bFullyRough = true;` and `FullyRough = true;` resolve to the same property. A property whose name
begins with a lowercase `b` followed by a non-uppercase character (for example `bias`) is not
b-stripped.

### Nested and indexed paths

The resolver itself still walks `A.B` (a member of a struct property), `A[N]` (element `N` of a
fixed-size C array) and any composition of the two. *(since 2.0.0)* A 1.x `Settings` key cannot reach
them: the legacy front end reads one name and reports `DSH3261` at the `.` or the `[`. Two of the
resolver's checks remain reachable:

| Written | Code |
| :-- | :-- |
| a fixed-size array (`ArrayDim` greater than 1) named without an index, e.g. `PhysicalMaterialMap = …` | [`DSH7121`](../diagnostics/DSH7xxx.md#dsh7121) |
| a name no property has | `DSH7118` |

`TArray`, `TMap` and `TSet` properties are not indexable at all — they fall through to the
`ImportText` catch-all in [Value grammar](#value-grammar).

### Alias table

Ten fixed key aliases are applied per path segment before the field scan. Alias keys are compared in
their separator-stripped, lowercased form, so `Lighting_Mode` and `LIGHTINGMODE` both hit the first
row.

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

## Value grammar

The resolved property's C++ type decides how the value text is parsed. The value has already been
unquoted by the parser, so `true` and `"true"` are the same input.

| Property type | Accepted literal | Code on failure |
| :-- | :-- | :-- |
| `bool` | `true` / `false`, case-insensitive, trimmed | [`DSH7132`](../diagnostics/DSH7xxx.md#dsh7132) |
| `int32` | signed integer literal | [`DSH7133`](../diagnostics/DSH7xxx.md#dsh7133) |
| `uint32` | integer in `[0, 4294967295]` | [`DSH7134`](../diagnostics/DSH7xxx.md#dsh7134) |
| `float` | a numeric literal — `true` / `false` are not numbers here | [`DSH7135`](../diagnostics/DSH7xxx.md#dsh7135) |
| `double` | as `float` | [`DSH7136`](../diagnostics/DSH7xxx.md#dsh7136) |
| `FString` | any text, trimmed — **never fails** | — |
| `FName` | any text, trimmed — **never fails** | — |
| object reference | `Path( … )`, an absolute object path such as `/Game/Textures/T_Noise`, or a `Class'…'` reference | [`DSH7137`](../diagnostics/DSH7xxx.md#dsh7137) (not a reference), [`DSH7138`](../diagnostics/DSH7xxx.md#dsh7138) (does not load), [`DSH7139`](../diagnostics/DSH7xxx.md#dsh7139) (wrong class), or the resolver's own code — see [Path](../parameters/path.md#diagnostics) |
| `enum class` (`FEnumProperty`) | an [enum literal](#enum-literals) | [`DSH7140`](../diagnostics/DSH7xxx.md#dsh7140) |
| `uint8` enum (`FByteProperty` with an enum) | an [enum literal](#enum-literals) | [`DSH7141`](../diagnostics/DSH7xxx.md#dsh7141) |
| plain `uint8` | integer in `[0, 255]` | [`DSH7142`](../diagnostics/DSH7xxx.md#dsh7142) |
| anything else | Unreal struct-literal text, e.g. `(R=1.0,G=0.0,B=0.0,A=1.0)` | [`DSH7143`](../diagnostics/DSH7xxx.md#dsh7143) |

A number is read the way Unreal's `LexTryParseString` reads it, which stops at the first character it
cannot use: `OpacityMaskClipValue = "0.5abc";` is `0.5`.

The type's message is wrapped as [`DSH7125`](../diagnostics/DSH7xxx.md#dsh7125) while the block is
validated ([`DSH7126`](../diagnostics/DSH7xxx.md#dsh7126) if the real material then refuses it), and
that reaches you as [`DSH8215`](../diagnostics/DSH8xxx.md#dsh8215), which names the material and quotes
the code.

> [!NOTE]
> An object-typed property whose class derives from `UTexture` **and** whose property name is exactly
> `Texture` or `TextureObject` writes `nullptr` instead of erroring when the asset fails to load.
> This is the material-expression convention; it also applies here.

When an object-typed value is a reference — it starts with `Path(` or `/`, or ends with `'` — and does
not resolve, the sentence after `DSH7125`'s period is the resolver's message.

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
its codes are in [Path](../parameters/path.md).

## Validation

Before anything is written to the real material, each generic key/value pair is applied to a
transient probe `UMaterial` created in the transient package. The probe write and the real write use
the same code, so a value that validates always applies. The only failure specific to this stage is
[`DSH7124`](../diagnostics/DSH7xxx.md#dsh7124), a probe that could not be created.

## Defaults — what an omitted setting resets to

A generated material is reset immediately before `Settings` is applied. An omitted key is therefore
**not** "left as it was on the previous generation" — it takes the value below.

| Property | Reset value |
| :-- | :-- |
| `BlendMode` | `Opaque` |
| `MaterialDomain` | `Surface` |
| shading model | `DefaultLit` |
| `TwoSided` | `false` |
| `bUseMaterialAttributes` | `false` |
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
| `bUsedWithVolumetricCloud` | `true` when `Domain="Volume"`, otherwise `false` |

Every other `UMaterial` property keeps whatever a freshly constructed material has.

`bUsedWithVolumetricCloud` is the one entry above that is not a constant: the domain decides it. A
`Volume`-domain material is a volumetric material, and the volumetric cloud renderer refuses one
that is not flagged for it — so you never have to write the flag by hand. Write it explicitly to
override the domain's choice; a `Volume` material that only ever feeds volumetric fog can turn the
flag back off with `bUsedWithVolumetricCloud = "false";` and save the cloud shader permutations.

## The round-trip set

The [decompiler](../tools/decompiler.md) emits `Domain`, `ShadingModel` and `BlendMode`
unconditionally, plus each of the following when it differs from the `UMaterial` class default. This
is the set guaranteed to survive a `UMaterial` → `.dsm` → `UMaterial` round trip; it is a subset of
what `Settings` accepts, not a limit on it.

**Booleans (40, in emit order)**

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
bAlwaysEvaluateWorldPositionOffset                        bUsedWithVolumetricCloud
```

`bHasPixelAnimation` is emitted only on UE 5.4 and newer.

`bUsedWithVolumetricCloud` is the one boolean not compared against the `UMaterial` class default: it
is compared against the value the domain would give it (see the table above). On a `Volume`-domain
material the value that needs no line is `true`, so it is the `false` that gets written out — which
is what keeps a fog-only `Volume` material from coming back with the flag on.

**Enums (1)** — `MaterialDecalResponse`.

Reachable but never emitted by the decompiler, and therefore lost on a round trip:
`OpacityMaskClipValue`, `NumCustomizedUVs`, `TranslucencyLightingMode`, `RefractionMethod`,
`RefractionDepthBias`, `TranslucencyPass`, `ShadingRate`, `FloatPrecisionMode`, `BlendableLocation`,
`BlendablePriority`, `bIsBlendable`, `UserSceneTexture`, `StencilCompare`, `StencilRefValue`,
`bEnableStencilTest`, `MaxWorldPositionOffsetDisplacement`, `PhysMaterial`, `PhysMaterialMask`, and
every other engine property the resolver can reach by name. Exact availability is engine-version
dependent — the resolver is pure reflection over the engine's own property set.

## Notes

- The reflection surface is the **engine's**, not the plugin's. A custom or modified engine build
  exposes its own properties and its own enum values through the same resolver, so this manual's
  tables describe the stock UE 5.3 – 5.8 surface only.
- Under the [ThinCustom backend](backend.md) every setting lands on the **hidden base `UMaterial`**,
  not on the emitted `UDreamShaderMaterialInstance`. Reading the blend mode off the instance shows
  the inherited value.
- A `Settings` block on a [`ShaderFunction`](function.md) does **not** share this behaviour. Only
  four keys are read there, and every key on this page is a
  [`DSH3263`](../diagnostics/DSH3xxx.md#dsh3263) warning there.
- Duplicate keys across multiple `Settings` sections merge, last one wins, with a
  [`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262) warning. See
  [Settings](index.md#how-a-statement-is-parsed).
- A settings failure is reported once for the material, as `DSH8215`; the message names the material
  and quotes the reflected writer's code and message.

## Diagnostics

Because keys are lowercased when they reach the emitter, the key quoted in these messages is the
**lowercased** spelling, not what the source wrote.

| Code | Raised when |
| :-- | :-- |
| [`DSH7127`](../diagnostics/DSH7xxx.md#dsh7127) | the `BlendMode` / `RenderType` value matched no alias |
| [`DSH7129`](../diagnostics/DSH7xxx.md#dsh7129) | the `ShadingModel` value matched no alias |
| [`DSH7128`](../diagnostics/DSH7xxx.md#dsh7128) | the value trims and case-folds to `Substrate` or `Strata` on UE 5.3 |
| [`DSH7130`](../diagnostics/DSH7xxx.md#dsh7130) | the `MaterialDomain` / `Domain` value matched no alias |
| `DSH7118` | no property matched — the usual "unknown key" error |
| `DSH7121` | a fixed-size array property named without an index |
| `DSH7124` | the probe material could not be allocated |
| `DSH7125` / `DSH7126` | the literal write failed on the probe / on the real material; the sentence after it is the type-specific code's message from [Value grammar](#value-grammar) |
| `DSH8215` | the build-time diagnostic every code above reaches you inside |
| [`DSH7200`](../diagnostics/DSH7xxx.md#dsh7200) | a key is set twice across `#pragma material` lines (a 1.x `Settings` block reports `DSH3262` instead) |
| `DSH7232` | `Substrate` is not `Legacy`, `Bridge` or `Native` |
| `DSH3261` | a key that is not one name — including a nested path or an index |

The `Backend` value is checked when the source is bound — see [Backend](backend.md#diagnostics).
`DSH7112`–`DSH7117`, `DSH7119`, `DSH7120`, `DSH7122` and `DSH7123` concern nested and indexed paths and
the resolver's own invariants; a 1.x key does not reach them.

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
PhysMaterial             = /Engine/EngineMaterials/DefaultPhysicalMaterial
```

## See also

- [Settings](index.md) — the block grammar and key normalization shared by every block kind
- [Material enums](material-enums.md) — every accepted `ShadingModel`, `BlendMode` and `Domain` value
- [Backend](backend.md) — the materialization key
- [Function settings](function.md) — why none of this applies inside a `ShaderFunction`
- [Project settings](project.md) — the mapping maps that extend the enum spellings
- [Shader](../language/shader.md) — the enclosing block
- [Output bindings](../language/output-bindings.md) — `Base.FrontMaterial` and `Base.MaterialAttributes`
- [Path](../parameters/path.md) — the `Path( … )` grammar used by object-typed settings
- [Metadata](../parameters/metadata.md) — the analogous reflected-property block on parameters
- [Decompiler](../tools/decompiler.md) — which settings survive a round trip
- [Regeneration](../generation/regeneration.md) — what a rebuild resets
- [Diagnostics index](../diagnostics/index.md) — every code
