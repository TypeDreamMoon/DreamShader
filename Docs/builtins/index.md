# Builtins

> [DreamShader](../index.md) » **Builtins**

The names DreamShaderLang resolves itself instead of looking them up among the author's declarations:
the `UE.*` namespace, the `Substrate.*` namespace, the reserved `SampleTexture2D` form, and the math
builtin set.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` |
| Kind | section hub |
| Generates | one or more `UMaterialExpression` nodes per call |

## The call surfaces

Since 2.0.0 the surfaces share one implementation. A `UE.` or `Substrate.` call is bound against the
**builtin catalog** — the material expression classes of the running engine, read by reflection
(`dsc export-catalog` writes it out) — and a math builtin is one of the compiler's core operations.
What still differs between the rows is what the **legacy front end** does to a 1.x spelling before the
binder sees it.

| # | Surface | Written as | Where | What the legacy front end does first | Reference |
| :-- | :-- | :-- | :-- | :-- | :-- |
| 1 | The 1.x `UE.*` names | `UE.TexCoord(Index = 0)` | `Graph` | keeps the 1.x argument list of each of the 27 names, dropping every other argument with [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254); rewrites `UE.SceneTexture`, `UE.StaticSwitchParameter`, `UE.TranslatedWorldPosition` | [`UE.*` catalogue](ue.md) |
| 2 | Any node, by class | `UE.Expression(Class = "Sine", …)` or `UE.Sine(…)` | `Graph` | takes `OutputType` / `ResultType` off the call (a width hint; kept on a Custom node) | [`UE.Expression`](ue-expression.md) |
| 3 | Property declaration | `UE.TexCoord(Index = 0) UV;` | `Properties` | declares a local initialised with the call at the head of each `Graph` body that reads it | [`UE.*` catalogue](ue.md#properties-declaration-form) |
| 4 | Math builtins | `pow(a, b)` | `Graph` | nothing | [Math builtins](math.md) |
| 5 | `Substrate.*` | `Substrate.Slab(BaseColor = c)` | `Graph`, `Outputs` | drops `OutputType` / `ResultType` | [Substrate](substrate.md) |

Surface 3 is a *declaration*, not an expression; from the local it declares, the call is an ordinary
`UE.` call. Surfaces 1, 2, 4 and 5 are expressions evaluated inside a `Graph` body.

> [!IMPORTANT]
> Surfaces 1 and 3 share builtin names but not argument lists. `UE.TexCoord` in a `Graph` keeps
> `Index` and drops `CoordinateIndex` with `DSH5254`; as a property declaration it takes both, since no
> 1.x filter applies there. Every difference is tabulated in
> [`UE.*` catalogue § Properties declaration form](ue.md#properties-declaration-form).

### The reserved `SampleTexture2D` form

`SampleTexture2D(<texture-object>, <uv>)` is not in a namespace but belongs to the same cluster. It is
a reserved name, rewritten by the legacy front end before user properties and functions are looked
up, to

```c
UE.Expression(Class = "TextureSample", TextureObject = <arg0>, Coordinates = <arg1>)
```

— the whole sample, `RGBA` where a `float4` is wanted. It is the only builtin call form matched
**case-sensitively**: `sampletexture2d(t, uv)` is not it. It takes exactly two positional arguments
([`DSH5256`](../diagnostics/DSH5xxx.md#dsh5256) otherwise). See
[`UE.Expression`](ue-expression.md#sampletexture2d).

## Dispatch order inside a `Graph`

A call is resolved in this order; the first match wins. The full lookup is on
[Name resolution](../graph/name-resolution.md).

| Order | Test on the callee | Result |
| :-- | :-- | :-- |
| 1 | in a 1.x source, while the call is read: `Path(…)`, a call of a declared parameter-node property, `SampleTexture2D` (case-**sensitive**), `UE.SceneTexture`, `UE.StaticSwitchParameter`, `UE.TranslatedWorldPosition`, and the 1.x argument lists of the other `UE.` names | rewritten to their 2.0 shape |
| 2 | is a type name | [constructor](../graph/constructors.md) |
| 3 | `UE.<Name>` or `Substrate.<Name>` | a node of the catalog — [`UE.Expression`](ue-expression.md) |
| 4 | a texture's `.Sample(…)` / `.SampleLevel(…)`, `Texture2DSample`, `Texture2DSampleLevel` | a texture sample |
| 5 | a math builtin name, then a GLSL spelling (`DSH5277` in a 1.x source) | [math builtin](math.md) |
| 6 | a function this file declares or imports | [Calls](../graph/calls.md) |
| 7 | in a 1.x source only: a math builtin or function matching ignoring case | as 5 or 6, with [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275) |

Inside step 3 the name after the prefix is looked up in the catalog, in the prefix's namespace:

| Order | Match | Result |
| :-- | :-- | :-- |
| 1 | `Class = "…"`, if the call has one | the class it names — [Class resolution](ue-expression.md#class-resolution) |
| 2 | the catalog's short name of a class — `Sine`, `TextureCoordinate`, `SubstrateSlabBSDF` | that class |
| 3 | an alias the catalog carries — `TexCoord`, `ViewportUV`, `Slab`, `HorizontalMix`, … | that class |
| 4 | in a 1.x source only: the one name or alias that matches ignoring case | that class, with [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276) |
| 5 | none | [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210), or `DSH5294` / `DSH5300` for a node a later engine has |

`UE.Expression` names no class, so without `Class` it is
[`DSH5218`](../diagnostics/DSH5xxx.md#dsh5218).

## Name and argument matching

| Element | Rule |
| :-- | :-- |
| `UE.` prefix | exact *(since 2.0.0)* — `ue.X` is an unknown variable `ue` ([`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)); a variable named `UE` hides the namespace |
| `Substrate.` prefix | in a 1.x source any case — it is a 1.x type spelling |
| Builtin name | exact; in a 1.x source a unique case-insensitive match with `DSH5276` |
| `SampleTexture2D` | case-**sensitive** |
| Math builtin name | exact; in a 1.x source case-insensitive with `DSH5275` — see [Math builtins](math.md#name-resolution) |
| Argument names | exact, or an alias the catalog carries; in a 1.x source a unique case-insensitive match with `DSH5276`. No other normalization — `_`, `-` and spaces are **not** removed |
| `Class` | exactly this spelling |
| `Output`, `OutputName`, `OutputIndex`, `OutputType`, `ResultType` | any case — the legacy front end takes them off the call |
| Enum values passed to properties | the value's name without its prefix, exactly; in a 1.x source case, spaces, `_`, `-`, `:`, `.`, `/`, the prefix and an `Enum::` scope are ignored, with [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278) |
| Unquoted text arguments | a bare word or dotted name is accepted wherever a quoted string is — `Class = Sine` equals `Class = "Sine"` |

Positional arguments are accepted by the classes the catalog gives an argument order —
`TextureCoordinate`, `Constant` … `Constant4Vector`, `Transform`, `TransformPosition`, `Panner`,
`ComponentMask`, `Time` and the five Substrate composition nodes — and refused elsewhere with
[`DSH5220`](../diagnostics/DSH5xxx.md#dsh5220) *(since 2.0.0)*. On the 27 1.x names the legacy front end
drops them first, except position 0 (`Input`) of `UE.TransformVector` and `UE.TransformPosition`;
`UE.StaticSwitchParameter` reads `True` at position 0 and `False` at position 1, and `SampleTexture2D`
takes both of its arguments positionally. The [math builtins](math.md) take arguments by position, or
named after the node's pins *(since 2.0.0)*.

> [!WARNING]
> **On the 27 1.x names, an argument the name did not read is dropped — with a warning now.** The
> legacy front end keeps the 1.x argument list and drops anything else with `DSH5254`
> *(since 2.0.0; 1.x dropped it without a word)*. `UE.Time(Bogus = 1)` compiles and says so;
> `UE.TexCoord(0)` produces UV channel 0 because the positional argument is dropped and
> `CoordinateIndex` keeps its node default. Every other call is bound argument by argument against the
> catalog, and a name that is no pin or property is [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) —
> or, in a 1.x source, kept for the built node with [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291).

## Pages

| Page | Contents |
| :-- | :-- |
| [`UE.*` catalogue](ue.md) | The 27 1.x names, their node classes, argument lists and outputs; the special-cased builtins; the property declaration form |
| [`UE.Expression`](ue-expression.md) | Class resolution, argument binding, property values, output selection, Custom nodes |
| [`OutputType`](output-type.md) | What `OutputType` / `ResultType` still do in a 1.x source, and the output names |
| [Math builtins](math.md) | `pow`, `min`, `max`, `fmod`, `frac` and the rest |
| [Transform bases](transform.md) | The basis values for `UE.TransformVector` and `UE.TransformPosition` |
| [Substrate](substrate.md) | The `Substrate.*` node catalogue |
| [HLSL library](hlsl-library.md) | `Shaders/DreamShaderBuiltins.ush` |
| [Custom Pass nodes](dream-pass.md) | `UE.DreamPassOutput` and `UE.DreamPassBuffer`, reflected expressions of the DreamShaderPass module (UE 5.8) |

## Notes

- **Identical calls are one node, on every surface.** The IR merges nodes with the same class,
  properties and inputs *(since 2.0.0; 1.x reused nodes for the generic and `Substrate.*` calls and
  the math builtins only, and never for a Custom node)*. See [Node reuse](../graph/node-reuse.md).
- Where a node lands on the canvas is decided by the layout pass. See
  [Graph layout](../generation/graph-layout.md).
- A `UE.*` call written inside a [`GraphFunction`](../language/graph-function.md) body is hoisted out
  of the HLSL and becomes a generated input pin on the emitted Custom node. The argument rules on this
  page still apply to the call itself.
- The `Substrate.*` namespace holds the Substrate classes of the running engine, which has none before
  UE 5.4; a call there is `DSH5210`. See [Substrate](substrate.md#availability).
- The builtin catalog is the engine's reflected classes, those of loaded plugins included. A name that
  is not a builtin, a declared property, a `Graph` variable or a declared function is an
  [unknown identifier](../graph/name-resolution.md).
- A `.dss` writes the same nodes as `UE.<ClassShortName>(Pin = …, Property = …)`; `dsc migrate` writes
  each 1.x call that way. See [`dsc migrate`](../tools/migrate.md) and
  [DreamShaderLang 2.0](../language-v2/index.md).

## Example

```c
Shader(Name="Docs/M_Surfaces")
{
    Properties {
        Texture2D BaseTex = Path(Game, "Textures/T_Noise");
        float     Speed   = 0.2;
    }

    Settings { ShadingModel = "Unlit"; }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        // 1 — 1.x UE.* names
        float2 uv = UE.Panner(Coordinate = UE.TexCoord(Index = 0), Speed = Speed);

        // 2 — any node by its class name; OutputType is optional since 2.0.0
        float pulse = UE.Sine(OutputType = "float1", Input = UE.Time());

        // the reserved two-argument sample form
        float4 tex = SampleTexture2D(BaseTex, uv);

        // 4 — math builtin
        float k = pow(pulse, 2.0);

        Color = tex.rgb * k;
    }
}
```

## See also

- [`UE.*` catalogue](ue.md) — every 1.x name, one entry each
- [`UE.Expression`](ue-expression.md) — any node by its class
- [`OutputType`](output-type.md) — the argument 1.x needed and 2.0 reads as a hint
- [Substrate](substrate.md) — the `Substrate.*` sibling namespace
- [Custom Pass nodes](dream-pass.md) — the two expressions a `.dsp` pipeline's materials use
- [Math builtins](math.md) — the scalar/vector math set
- [Calls](../graph/calls.md) — call syntax, named arguments and out arguments
- [Name resolution](../graph/name-resolution.md) — the lookup order a callee goes through
- [Node reuse](../graph/node-reuse.md) — identical calls and nodes
- [Properties](../language/properties.md) — the section the declaration form lives in
- [`Path(…)`](../parameters/path.md) — asset-reference syntax
- [Diagnostics index](../diagnostics/index.md) — every code
