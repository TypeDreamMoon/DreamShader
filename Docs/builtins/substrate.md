# Substrate.*

> [DreamShader](../index.md) » [Builtins](index.md) » **`Substrate.*`**

A sibling call namespace to [`UE.*`](ue.md) that wraps Unreal's Substrate BSDF, composition and
utility material nodes with a fixed expression class per name.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration initializer |
| Kind | builtin call namespace |
| Generates | one `UMaterialExpressionSubstrate*` node per call |
| Names | 24, resolving to 22 distinct engine classes (two alias pairs) |
| Requires | **UE 5.4 or newer** |

## Availability

The whole namespace — the descriptor table, the class check, and the `Substrate` type token — is
compiled in only when the engine is **UE 5.4 or newer**. On UE 5.3 every `Substrate.*` call fails
with `Substrate builtin call '{Name}' requires Unreal Engine 5.4 or newer.`

| Surface | Gate | Below the gate |
| :-- | :-- | :-- |
| `Substrate.<Name>(…)` calls | UE 5.4 | `Substrate builtin call '{Name}' requires Unreal Engine 5.4 or newer.` |
| The `Substrate` declared-type token | UE 5.4 | the token does not resolve |
| `OutputType="Substrate"` on a generic `UE.*` call | UE 5.4 | `UE.{Name} OutputType="Substrate" requires Unreal Engine 5.4 or newer.` |
| `Base.FrontMaterial` output binding | UE 5.4 | `Base.FrontMaterial requires Unreal Engine 5.4 or newer.` |
| `Settings = { ShadingModel = "Substrate"; }` (and the `"Strata"` spelling) | UE 5.4 | `ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.` |

> [!NOTE]
> DreamShader's own gate is the **engine version only**. Substrate must additionally be enabled for
> the project (*Project Settings ▸ Engine ▸ Rendering ▸ Substrate*) for the generated graph to
> compile in the engine. DreamShader does not read that project setting, so a source file that
> compiles to a Substrate graph on a non-Substrate project produces the asset and then fails during
> Unreal's own material translation.

> [!NOTE]
> The editor tooling reflects the same gate. On UE 5.3 the Bridge manifest
> `Saved/DreamShader/Bridge/substrate-builtins.json` is written with an empty entry array,
> `supported: false` and
> `unsupportedReason: "Substrate builtins require Unreal Engine 5.4 or newer."` — so completion in
> the editor extensions offers nothing. See [Bridge](../tools/bridge.md).

## Synopsis

```c
Substrate . <Name> ( [ <argument-name> = <expression> ]
                     [ , <argument-name> = <expression> ] …
                     [ , { Output | OutputName } = "<pin-name>" ]
                     [ , OutputIndex = <integer> ] )
```

`Substrate` and `.` are literal; `[ … ]`, `{ a | b }` and `…` are meta-notation.

The namespace prefix and the `<Name>` are matched **case-insensitively**: `SUBSTRATE.UNLIT(…)`
resolves. Argument names are matched case-insensitively and whitespace-trimmed, but are otherwise
exact — no separator stripping.

## Argument model

Every `Substrate.*` call goes through the generic reflected-builtin path, so it inherits that path's
rules.

| Rule | Behaviour |
| :-- | :-- |
| All arguments must be **named** | a positional argument fails with `Generic Substrate.{Name} calls require named arguments.` |
| `Class=` is **rejected** | `Substrate.{Name} uses a fixed MaterialExpression class and does not accept Class.` — the class comes from the descriptor and cannot be overridden |
| `OutputType=` / `ResultType=` are **ignored** | the output type is synthesized from the descriptor; supplying either has no effect and no diagnostic |
| `Output=` / `OutputName=` | selects an output pin by name |
| `OutputIndex=` | selects an output pin by 0-based index |
| `Output`/`OutputName` and `OutputIndex` together | `UE.{Name} cannot use OutputName/Output together with OutputIndex.` |
| Anything else | dispatched by reflection — see below |

### Reflection-driven binding

Argument binding is not table-driven. For each remaining argument the generator tries, in order:

1. **Input pin name** — every input pin the node reports, compared case-insensitively after
   trimming.
2. **`UPROPERTY` name** — any reflected property on the class or a superclass, compared the same
   way. Boolean properties additionally match with a leading `b` stripped, so `FractionalPart`
   reaches `bFractionalPart`.
3. If the property is an `FExpressionInput` or an `FMaterialAttributesInput`, the argument becomes a
   **wired input** and its value is evaluated as an expression. Otherwise it is written as a
   **literal property** on the node.
4. No match → `UE.{Name}: '{Argument}' is not a property on '{Class}'.`

A pin-name match wins over a reflected property of the same name.

> [!WARNING]
> **Engine pin names containing spaces are unreachable.** Argument-name normalization trims and
> lowercases but does **not** strip spaces, so a pin displayed as `Diffuse Albedo` cannot be spelled
> as an argument name. Use the `UPROPERTY` name instead — `DiffuseAlbedo` — which is what the tables
> below list. Where a pin name and property name coincide, either spelling works.

> [!NOTE]
> **Accepted argument names follow the engine, not the plugin.** Because step 1 and step 2 query the
> live `UMaterialExpressionSubstrate*` class, the exact accepted set is whatever the running engine
> version exposes; an input added or removed by an engine release appears or disappears with no
> plugin change. The tables below list the inputs these classes expose; the **Completion** column
> marks the curated subset DreamShader publishes to editor completion and the Bridge manifest, which
> *is* fixed by the plugin. Any non-input reflected property on the same class — enums, floats,
> booleans — is also settable as a literal argument even though it is not listed here.

### Selecting an output

`Output=` / `OutputName=` matches an output pin's name verbatim, case-insensitively, including names
that contain spaces (output pin names are matched as whole names, not parsed as argument names).
`OutputIndex=` takes a 0-based index. Only the utility wrappers below have more than one output.

## Catalogue

*Substrate output* marks the wrappers whose result is a Substrate material value (0 components,
bindable to `Base.FrontMaterial`); the four marked **no** are the utility nodes and return ordinary
numeric values.

| `Substrate.<Name>` | `UMaterialExpression` class | Substrate output | Entry |
| :-- | :-- | :-- | :-- |
| `ShadingModels` | `UMaterialExpressionSubstrateShadingModels` | yes | [↓](#substrateshadingmodels) |
| `Slab` | `UMaterialExpressionSubstrateSlabBSDF` | yes | [↓](#substrateslab) |
| `SimpleClearCoat` | `UMaterialExpressionSubstrateSimpleClearCoatBSDF` | yes | [↓](#substratesimpleclearcoat) |
| `VolumetricFogCloud` | `UMaterialExpressionSubstrateVolumetricFogCloudBSDF` | yes | [↓](#substratevolumetricfogcloud) |
| `Unlit` | `UMaterialExpressionSubstrateUnlitBSDF` | yes | [↓](#substrateunlit) |
| `Hair` | `UMaterialExpressionSubstrateHairBSDF` | yes | [↓](#substratehair) |
| `Eye` | `UMaterialExpressionSubstrateEyeBSDF` | yes | [↓](#substrateeye) |
| `SingleLayerWater` | `UMaterialExpressionSubstrateSingleLayerWaterBSDF` | yes | [↓](#substratesinglelayerwater) |
| `LightFunction` | `UMaterialExpressionSubstrateLightFunction` | yes | [↓](#substratelightfunction) |
| `PostProcess` | `UMaterialExpressionSubstratePostProcess` | yes | [↓](#substratepostprocess) |
| `UI` | `UMaterialExpressionSubstrateUI` | yes | [↓](#substrateui) |
| `ConvertMaterialAttributes` | `UMaterialExpressionSubstrateConvertMaterialAttributes` | yes | [↓](#substrateconvertmaterialattributes) |
| `ConvertToDecal` | `UMaterialExpressionSubstrateConvertToDecal` | yes | [↓](#substrateconverttodecal) |
| `HorizontalMix` | `UMaterialExpressionSubstrateHorizontalMixing` | yes | [↓](#substratehorizontalmix) |
| `HorizontalMixing` **— alias of `HorizontalMix`** | `UMaterialExpressionSubstrateHorizontalMixing` | yes | [↓](#substratehorizontalmix) |
| `VerticalLayer` | `UMaterialExpressionSubstrateVerticalLayering` | yes | [↓](#substrateverticallayer) |
| `VerticalLayering` **— alias of `VerticalLayer`** | `UMaterialExpressionSubstrateVerticalLayering` | yes | [↓](#substrateverticallayer) |
| `Add` | `UMaterialExpressionSubstrateAdd` | yes | [↓](#substrateadd) |
| `Weight` | `UMaterialExpressionSubstrateWeight` | yes | [↓](#substrateweight) |
| `Select` | `UMaterialExpressionSubstrateSelect` | yes | [↓](#substrateselect) |
| `TransmittanceToMFP` | `UMaterialExpressionSubstrateTransmittanceToMFP` | **no** | [↓](#substratetransmittancetomfp) |
| `MetalnessToDiffuseAlbedoF0` | `UMaterialExpressionSubstrateMetalnessToDiffuseAlbedoF0` | **no** | [↓](#substratemetalnesstodiffusealbedof0) |
| `HazinessToSecondaryRoughness` | `UMaterialExpressionSubstrateHazinessToSecondaryRoughness` | **no** | [↓](#substratehazinesstosecondaryroughness) |
| `ThinFilm` | `UMaterialExpressionSubstrateThinFilm` | **no** | [↓](#substratethinfilm) |

The two alias pairs are exact duplicates: same class, same inputs, same output typing. Neither
spelling is deprecated.

## Output typing

| Descriptor | Declared output type | Component count | Flags |
| :-- | :-- | :-- | :-- |
| Substrate output | `Substrate` | 0 | `Substrate` value, authoritative |
| Utility node | `auto` | taken from the selected pin's real value type | numeric |

The declared type is then checked against the selected output pin's actual value type. If a wrapper
declared as a Substrate output resolves to a pin that is not a Substrate value, the call fails with
`Substrate.{Name} output is not a Substrate value.`

A `Substrate` value can only be:

- assigned to a `Substrate`-typed `Graph` variable or `Outputs` declaration;
- passed to another `Substrate.*` wrapper's Substrate-typed input;
- bound to `Base.FrontMaterial`.

It cannot be swizzled, used with `+ - * /`
(`Arithmetic operators cannot be applied to Substrate values.`), passed to a
[math builtin](math.md) (`Math function '{Name}' only accepts numeric scalar/vector arguments.`),
switched by a `StaticSwitchParameter`
(`StaticSwitchParameter '{Name}' cannot switch Substrate values.`), or produced by an HLSL Custom
node.

---

## Node reference

### Substrate.ShadingModels

Legacy-style shading-model surface expressed as a Substrate material.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `BaseColor` | numeric | — |
| `Metallic` | numeric | — |
| `Specular` | numeric | — |
| `Roughness` | numeric | — |
| `Anisotropy` | numeric | — |
| `EmissiveColor` | numeric | — |
| `Normal` | numeric | — |
| `Tangent` | numeric | — |
| `SubSurfaceColor` | numeric | — |
| `ClearCoat` | numeric | — |
| `ClearCoatRoughness` | numeric | — |
| `Opacity` | numeric | — |
| `TransmittanceColor` | numeric | — |
| `WaterScatteringCoefficients` | numeric | — |
| `WaterAbsorptionCoefficients` | numeric | — |
| `WaterPhaseG` | numeric | — |
| `ColorScaleBehindWater` | numeric | — |
| `ClearCoatNormal` | numeric | — |
| `CustomTangent` | numeric | — |
| `ThinTranslucentSurfaceCoverage` | numeric | — |

Output: Substrate value. Editor completion offers no parameters for this wrapper.

### Substrate.Slab

The general-purpose Substrate BSDF slab.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `DiffuseAlbedo` | numeric | yes |
| `F0` | numeric | yes |
| `F90` | numeric | — |
| `Roughness` | numeric | yes |
| `Anisotropy` | numeric | — |
| `Normal` | numeric | yes |
| `Tangent` | numeric | — |
| `SSSMFP` | numeric | — |
| `SSSMFPScale` | numeric | — |
| `SSSPhaseAnisotropy` | numeric | — |
| `EmissiveColor` | numeric | — |
| `SecondRoughness` | numeric | — |
| `SecondRoughnessWeight` | numeric | — |
| `FuzzRoughness` | numeric | — |
| `FuzzAmount` | numeric | — |
| `FuzzColor` | numeric | — |
| `GlintValue` | numeric | — |
| `GlintUV` | numeric | — |

Output: Substrate value.

### Substrate.SimpleClearCoat

A slab with a fixed second clear-coat lobe.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `DiffuseAlbedo` | numeric | yes |
| `F0` | numeric | yes |
| `Roughness` | numeric | yes |
| `ClearCoatCoverage` | numeric | yes |
| `ClearCoatRoughness` | numeric | yes |
| `Normal` | numeric | yes |
| `EmissiveColor` | numeric | — |
| `BottomNormal` | numeric | — |

Output: Substrate value.

### Substrate.VolumetricFogCloud

Participating-media BSDF for volumetric fog and cloud materials.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `Albedo` | numeric | yes |
| `Extinction` | numeric | yes |
| `EmissiveColor` | numeric | yes |
| `AmbientOcclusion` | numeric | yes |

Output: Substrate value.

### Substrate.Unlit

Emissive-only BSDF. The smallest complete Substrate surface.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `EmissiveColor` | numeric | yes |
| `TransmittanceColor` | numeric | — |
| `Normal` | numeric | — |

Output: Substrate value.

### Substrate.Hair

Hair BSDF.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `BaseColor` | numeric | yes |
| `Scatter` | numeric | yes |
| `Specular` | numeric | yes |
| `Roughness` | numeric | yes |
| `Backlit` | numeric | yes |
| `Tangent` | numeric | yes |
| `EmissiveColor` | numeric | yes |

Output: Substrate value.

### Substrate.Eye

Eye BSDF.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `DiffuseColor` | numeric | yes |
| `Roughness` | numeric | yes |
| `CorneaNormal` | numeric | yes |
| `IrisNormal` | numeric | yes |
| `IrisPlaneNormal` | numeric | yes |
| `IrisMask` | numeric | yes |
| `IrisDistance` | numeric | yes |
| `EmissiveColor` | numeric | yes |

Output: Substrate value.

### Substrate.SingleLayerWater

Single-layer water BSDF.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `BaseColor` | numeric | yes |
| `Metallic` | numeric | yes |
| `Specular` | numeric | yes |
| `Roughness` | numeric | yes |
| `Normal` | numeric | yes |
| `EmissiveColor` | numeric | yes |
| `TopMaterialOpacity` | numeric | yes |
| `WaterAlbedo` | numeric | yes |
| `WaterExtinction` | numeric | yes |
| `WaterPhaseG` | numeric | yes |
| `ColorScaleBehindWater` | numeric | yes |

Output: Substrate value.

### Substrate.LightFunction

Substrate output node for light-function materials.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `Color` | numeric | yes |

Output: Substrate value.

### Substrate.PostProcess

Substrate output node for post-process materials.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `Color` | numeric | yes |
| `Opacity` | numeric | yes |

Output: Substrate value.

### Substrate.UI

Substrate output node for UI-domain materials.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `Color` | numeric | yes |
| `Opacity` | numeric | yes |

Output: Substrate value.

### Substrate.ConvertMaterialAttributes

Converts a `MaterialAttributes` value into a Substrate material.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `MaterialAttributes` | **`MaterialAttributes`** | yes |
| `Attributes` — alias of `MaterialAttributes` | **`MaterialAttributes`** | — |
| `WaterScatteringCoefficients` | numeric | yes |
| `WaterAbsorptionCoefficients` | numeric | yes |
| `WaterPhaseG` | numeric | yes |
| `ColorScaleBehindWater` | numeric | yes |

Output: Substrate value.

`MaterialAttributes` is the reflected property name and `Attributes` is the engine's pin name for the
same input; both bind input 0. Passing a numeric value to it fails with
`Substrate.ConvertMaterialAttributes input '{Pin}' expects a MaterialAttributes value.`
See [`MaterialAttributes`](../graph/material-attributes.md).

### Substrate.ConvertToDecal

Converts a Substrate material into a decal material.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `DecalMaterial` | **Substrate** | yes |
| `Coverage` | numeric | yes |

Output: Substrate value.

### Substrate.HorizontalMix

Screen-space horizontal blend of two Substrate materials. Also spelled
**`Substrate.HorizontalMixing`** — the two names are interchangeable.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `Background` | **Substrate** | yes |
| `Foreground` | **Substrate** | yes |
| `Mix` | numeric | yes |

Output: Substrate value.

### Substrate.VerticalLayer

Layers one Substrate material over another. Also spelled **`Substrate.VerticalLayering`** — the two
names are interchangeable.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `Top` | **Substrate** | yes |
| `Base` | **Substrate** | yes |
| `Thickness` | numeric | yes |

Output: Substrate value.

### Substrate.Add

Adds two Substrate materials.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `A` | **Substrate** | yes |
| `B` | **Substrate** | yes |

Output: Substrate value.

### Substrate.Weight

Scales a Substrate material's contribution.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `A` | **Substrate** | yes |
| `Weight` | numeric | yes |

Output: Substrate value.

### Substrate.Select

Static selection between two Substrate materials.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `A` | **Substrate** | yes |
| `B` | **Substrate** | yes |
| `SelectValue` | numeric | yes |

Output: Substrate value.

### Substrate.TransmittanceToMFP

Utility. Converts a transmittance colour and thickness into a mean-free-path parameterisation.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `TransmittanceColor` | numeric | yes |
| `Thickness` | numeric | yes |

Outputs — numeric, selectable with `Output=` or `OutputIndex=`:

| Index | Name |
| :-- | :-- |
| 0 (default) | `MFP` |
| 1 | `Thickness` |

### Substrate.MetalnessToDiffuseAlbedoF0

Utility. Converts a legacy base-colour/metallic/specular triple into the slab parameterisation.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `BaseColor` | numeric | yes |
| `Metallic` | numeric | yes |
| `Specular` | numeric | yes |

Outputs — numeric:

| Index | Name |
| :-- | :-- |
| 0 (default) | `DiffuseAlbedo` |
| 1 | `F0` |

### Substrate.HazinessToSecondaryRoughness

Utility. Converts a haziness control into a second-roughness lobe.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `BaseRoughness` | numeric | yes |
| `Haziness` | numeric | yes |

Outputs — numeric:

| Index | Name |
| :-- | :-- |
| 0 (default) | `Second Roughness` |
| 1 | `Second Roughness Weight` |

### Substrate.ThinFilm

Utility. Computes thin-film interference specular colours.

| Argument | Value | Completion |
| :-- | :-- | :-- |
| `Normal` | numeric | yes |
| `F0` | numeric | yes |
| `F90` | numeric | yes |
| `Thickness` | numeric | yes |
| `IOR` | numeric | yes |

Outputs — numeric:

| Index | Name |
| :-- | :-- |
| 0 (default) | `Specular Color` |
| 1 | `Edge Specular Color` |

---

## Binding a Substrate value to Base.FrontMaterial

A Substrate material reaches the generated `UMaterial` through the `Base.FrontMaterial` output
binding, and through no other route.

```c
Outputs = {
    Substrate Surface;              // declared-type token; case-insensitive, no other spelling
    Base.FrontMaterial = Surface;
}
Graph = {
    Surface = Substrate.Unlit(EmissiveColor = Color);
}
```

| Rule | Behaviour |
| :-- | :-- |
| Declared-type token | the single spelling `Substrate`; whitespace-stripped and case-insensitive. `Strata` is **not** a type token. |
| Shading model | the binding force-sets the material's shading model to Substrate — no `Settings` entry is needed |
| Explicit `ShadingModel` setting | allowed only as `"Substrate"` or `"Strata"`; any other value fails |
| `Base.MaterialAttributes` in the same `Shader` | rejected — the two bindings are mutually exclusive |
| Backend | requires a `Graph` block; an HLSL Custom node cannot produce or drive a Substrate value |
| Engine version | UE 5.4+ |

> [!NOTE]
> `Strata` is the pre-rename spelling. It is accepted as a `Settings = { ShadingModel = … }` value
> (an alias for the same shading model) but never as a type token and never as a call namespace.
> See [Enum values](../settings/material-enums.md).

## Notes

- **There is no wrapper for every Substrate class.** `UMaterialExpressionSubstrateToonBSDF` has no
  entry in the table. Reach it — and any future Substrate class — through the generic path:
  `UE.Expression(Class="SubstrateToonBSDF", OutputType="Substrate", …)`. Class-name resolution
  accepts `SubstrateToonBSDF`, `MaterialExpressionSubstrateToonBSDF` and the full `/Script/Engine.…`
  object path interchangeably; the `U`-prefixed C++ spelling is **not** accepted. See
  [`UE.Expression`](ue-expression.md#class-resolution).
- **Registered `UE.*` sugar does not apply inside this namespace.** `Substrate.TexCoord(…)` is not a
  thing; unknown names fail with `Unsupported Substrate builtin call '{Name}' in Graph.`
- Substrate nodes participate in **node reuse**: two textually identical `Substrate.Slab(…)` calls
  over identical argument values collapse to a single node. See
  [Node reuse](../graph/node-reuse.md).
- The [decompiler](../tools/decompiler.md) exports an existing Substrate graph back to
  DreamShaderLang, deriving channel swizzles from each connection's write mask *(since 1.5.0)*.
- The complete `Substrate.*` surface is exported for editor tooling to
  `Saved/DreamShader/Bridge/substrate-builtins.json` (schema `DreamShader.SubstrateBuiltins`,
  version 1), one entry per name with its `qualifiedName`, `className`, `outputType`,
  `isSubstrateOutput` and curated `parameters`. See [Bridge](../tools/bridge.md).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout these tables; the compiler emits the
substituted text.

> [!NOTE]
> Several messages below begin with the literal text `UE.` even for a `Substrate.*` call. Those come
> from the shared generic-builtin path, which formats its prefix as `UE.` unconditionally; only the
> messages that carry the namespace explicitly render as `Substrate.`. This is cosmetic — the
> `{Name}` in such a message is still the Substrate wrapper name.

### Call site

| Message | Cause |
| :-- | :-- |
| `Substrate builtin call '{Name}' requires Unreal Engine 5.4 or newer.` | any `Substrate.*` call on UE 5.3 |
| `Unsupported Substrate builtin call '{Name}' in Graph.` | the name is not one of the 24 in the catalogue |
| `Generic Substrate.{Name} calls require named arguments.` | any positional argument |
| `Substrate.{Name} uses a fixed MaterialExpression class and does not accept Class.` | `Class=` was supplied |
| `Substrate.{Name} resolved to non-Substrate class '{Class}'.` | the descriptor's class is not a Substrate BSDF or utility class |
| `UE.{Name} could not resolve MaterialExpression class '{Class}'.` | the descriptor's class is not present in the running engine |
| `UE.{Name} failed to create '{Class}'.` | node creation returned nothing |
| `UE.{Name}: '{Argument}' is not a property on '{Class}'.` | the argument matched neither a pin name nor a reflected property |
| `Substrate.{Name} input '{Pin}' expects a Substrate value.` | a Substrate-typed pin was given a numeric value |
| `Substrate.{Name} input '{Pin}' does not accept Substrate values.` | a numeric pin was given a Substrate value |
| `Substrate.{Name} input '{Pin}' expects a MaterialAttributes value.` | a `MaterialAttributes` pin was given something else |
| `Substrate.{Name} input '{Pin}' does not accept MaterialAttributes values.` | a numeric pin was given a `MaterialAttributes` value |
| `UE.{Name} input '{Pin}': {Error}` | evaluating a wired input's expression failed |
| `UE.{Name} failed to bind input '{Pin}'.` | the pin could not be connected |
| `UE.{Name} property '{Property}' must use a literal value.` | a literal property argument was not a literal |
| `UE.{Name} property '{Property}' must use Path(...) or an Unreal object path.` | an object-typed property argument was not an asset reference |
| `UE.{Name} property '{Property}': {Error}` | writing the literal property failed |
| `UE.{Name} cannot use OutputName/Output together with OutputIndex.` | both selectors supplied |
| `UE.{Name} OutputName must be a literal value.` | `Output=` / `OutputName=` was not a literal |
| `UE.{Name} output '{Pin}' was not found on '{Class}'.` | the named output pin does not exist |
| `UE.{Name} OutputIndex is out of range for '{Class}'.` | negative index, or past the last output |
| `UE.{Name} created '{Class}', but it has no material outputs.` | the node exposes no outputs at all |
| `Substrate.{Name} output is not a Substrate value.` | the selected pin's value type is not Substrate although the descriptor declares one |

### Substrate values elsewhere in the pipeline

| Message | Cause |
| :-- | :-- |
| `Substrate requires Unreal Engine 5.4 or newer.` | a Substrate value or type was requested on UE 5.3 |
| `Graph variable '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | a `Substrate`-typed `Graph` declaration on UE 5.3 |
| `UE.{Name} OutputType="Substrate" requires Unreal Engine 5.4 or newer.` | a generic `UE.*` call declared a Substrate output on UE 5.3 |
| `UE.{Name} OutputType="Substrate" is not supported by UMaterialExpressionCustom.` | a Substrate output was requested from a `Custom` node |
| `UE.{Name} Custom input '{Pin}' does not accept Substrate values.` | a Substrate value was fed to a `Custom` node input |
| `Arithmetic operators cannot be applied to Substrate values.` | a Substrate value used with `+ - * /` |
| `Math function '{Name}' only accepts numeric scalar/vector arguments.` | a Substrate value passed to a [math builtin](math.md) |
| `StaticSwitchParameter '{Name}' cannot switch Substrate values.` | a Substrate value on a `True=` / `False=` branch |
| `Base.FrontMaterial requires Unreal Engine 5.4 or newer.` | the binding target used on UE 5.3 |
| `{File}: Base.FrontMaterial requires ShadingModel="Substrate" or no explicit ShadingModel setting.` | a conflicting explicit shading model |
| `{File}: Base.FrontMaterial and Base.MaterialAttributes cannot be used by the same Shader.` | both bindings present |
| `{File}: Base.FrontMaterial expects a Substrate value and cannot be driven by a material Custom node. Use a Graph block and Substrate.* nodes.` | the binding source came from an HLSL Custom node |
| `{File}: Material output '{Output}' expects a Substrate value and cannot be driven by a material Custom node. Use a Graph block and Substrate.* nodes.` | same, for another Substrate-typed material output |
| `{File}: Material output '{Output}' expects a numeric value, but got Substrate.` | a Substrate value bound to a numeric material output |
| `{File}: Output '{Output}' is declared as Substrate and cannot be generated by a material Custom node. Use a Graph block and Substrate.* nodes.` | a `Substrate`-typed `Outputs` declaration under the Custom-node path |
| `{Kind} '{Name}' output '{Output}' uses Substrate, which is not supported by HLSL Custom node functions. Use a Graph block and Substrate.* nodes.` | a `Substrate`-typed output on a block generated as a Custom node |
| `{Kind} '{Name}' output '{Output}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | same, on UE 5.3 |
| `DreamShader Function '{Name}' result '{Result}' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead.` | a `Function` declared a `Substrate` result |
| `DreamShader Function '{Name}' result '{Result}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | same, on UE 5.3 |
| `ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.` | `Settings = { ShadingModel = "Substrate"; }` or `"Strata"` on UE 5.3 |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Docs/M_Substrate")
{
    Properties = {
        vec3  BaseColor = vec3(0.6, 0.1, 0.1);
        float Metallic  = 0.0;
        float Specular  = 0.5;
        float Rough     = 0.3;
        vec3  Glow      = vec3(0.1, 0.6, 1.0);
    }

    Outputs = {
        Substrate Surface;
        Base.FrontMaterial = Surface;
    }

    Graph = {
        // Utility node: two numeric outputs, selected by name.
        vec3 Albedo = Substrate.MetalnessToDiffuseAlbedoF0(
            BaseColor = BaseColor, Metallic = Metallic, Specular = Specular,
            Output = "DiffuseAlbedo");
        vec3 F0 = Substrate.MetalnessToDiffuseAlbedoF0(
            BaseColor = BaseColor, Metallic = Metallic, Specular = Specular,
            Output = "F0");

        Substrate Body = Substrate.Slab(
            DiffuseAlbedo = Albedo,
            F0            = F0,
            Roughness     = Rough);

        Substrate Emissive = Substrate.Unlit(EmissiveColor = Glow);

        Surface = Substrate.Add(A = Body, B = Emissive);
    }
}
```

Generated nodes:

```text
SubstrateMetalnessToDiffuseAlbedoF0   -> output "DiffuseAlbedo"  -> Albedo    (3 components)
                                      -> output "F0"             -> F0        (3 components)
                                         (one node, reused for both reads)
SubstrateSlabBSDF                     -> Body                    (Substrate value)
SubstrateUnlitBSDF                    -> Emissive                (Substrate value)
SubstrateAdd                          -> Surface                 (Substrate value)
Material ShadingModel forced to Substrate by the Base.FrontMaterial binding
```

## See also

- [Builtins](index.md) — the call surfaces available inside `Graph`
- [`UE.*` catalogue](ue.md) — the sibling namespace and its registered builtins
- [`UE.Expression`](ue-expression.md) — reaching Substrate classes that have no wrapper
- [`OutputType` values](output-type.md) — including the `Substrate` token
- [Math builtins](math.md) — the unprefixed numeric call surface
- [Transform builtins](transform.md) — `UE.TransformVector` / `UE.TransformPosition`
- [Output bindings](../language/output-bindings.md) — `Base.FrontMaterial` and the full target list
- [`MaterialAttributes`](../graph/material-attributes.md) — the value type `ConvertMaterialAttributes` consumes
- [Types](../language/types.md) — the `Substrate` declared-type token
- [Enum values](../settings/material-enums.md) — `ShadingModel = "Substrate"` / `"Strata"`
- [Calls](../graph/calls.md) — named-argument syntax
- [Node reuse](../graph/node-reuse.md) — why repeated wrapper calls produce one node
- [Bridge](../tools/bridge.md) — `substrate-builtins.json` and the editor completion manifest
- [Decompiler](../tools/decompiler.md) — exporting an existing Substrate graph back to source
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
