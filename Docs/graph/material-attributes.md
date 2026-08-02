# MaterialAttributes

> [DreamShader](../index.md) » [Graph](index.md) » **MaterialAttributes**

A struct-like Graph value that carries a whole material's attribute set as one connection, written and
read member by member. *(since 1.2.5)*

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` declaration, or a function `Inputs` / `Outputs` section |
| Kind | value type |
| Generates | `UMaterialExpressionMakeMaterialAttributes` (declaration), `UMaterialExpressionSetMaterialAttributes` (one per member write), `UMaterialExpressionBreakMaterialAttributes` (one per member read) |

## Synopsis

```c
MaterialAttributes <name> ;                       // creates an empty attribute set
MaterialAttributes <name> = <material-attributes-expression> ;

<name> . <member> = <expression> ;                // member write
<name> . <member> = { <expression> , … } ;        // member write, brace initializer

<expression> := <name> . <member>                 // member read
```

`.`, `=`, `{ }` and `;` are literal DreamShaderLang punctuation. `<member>` is one of the names in
[Members](#members).

## The value type

| Property | Value |
| :-- | :-- |
| Component count | `0` — the marker that distinguishes it from a scalar or vector |
| Type token | `MaterialAttributes`, matched case-insensitively |
| Alternate spelling | all internal spaces are stripped before matching, so `Material Attributes` resolves too |
| Arithmetic | rejected — `Arithmetic operators cannot be applied to MaterialAttributes values.` |
| Swizzle | not applicable; `.member` on such a value is an attribute read, never a channel mask |
| Assignment | only from another `MaterialAttributes` value; anything else fails with `Expected a MaterialAttributes value.` |
| Assignment to a numeric target | rejected — `MaterialAttributes values cannot be assigned to numeric outputs.` |

## Creating a value

| Form | Effect |
| :-- | :-- |
| `MaterialAttributes Attrs;` | Creates one `MakeMaterialAttributes` node with every input unconnected. Unlike `Texture2D` and `Substrate`, a `MaterialAttributes` declaration does **not** require an initializer. |
| `MaterialAttributes Attrs = {};` | Identical to the bare declaration — an empty brace initializer resolves to the default value. |
| `MaterialAttributes B = A;` | Binds `B` to the same node and output as `A`. No node is created. |
| `MaterialAttributes Attrs = F_Layer(uv);` | Any expression whose value is a `MaterialAttributes` — a function call, an `if` result, a `StaticSwitchParameter` call. |

> [!WARNING]
> **An `Outputs` declaration does not create the variable.** Only output declarations that carry an
> initializer are turned into synthesized statements ahead of the `Graph` body. Writing a member of a
> name that exists only as `Outputs { MaterialAttributes Attrs; }` fails with
> `Unknown MaterialAttributes variable 'Attrs'.` Declare it in the `Graph` block — the two
> declarations do not collide, because the redeclaration guard only looks at Graph variables:
>
> ```c
> Outputs { MaterialAttributes Attrs; Base.MaterialAttributes = Attrs; }
> Graph   { MaterialAttributes Attrs; Attrs.BaseColor = Tint; }
> ```

> [!NOTE]
> `MaterialAttributes B = A;` is a value copy, not an alias. A later `B.Roughness = r;` chains a new
> `SetMaterialAttributes` onto the node `B` currently points at and rebinds **`B`** only; `A` keeps
> pointing at the earlier node in the chain.

## Members

Member names are matched **case-insensitively**, on both the read and the write side. Aliases are
exact synonyms.

| Member | Aliases | Components | Legacy Break output index |
| :-- | :-- | :-- | :-- |
| `BaseColor` | — | 3 | 0 |
| `Metallic` | — | 1 | 1 |
| `Specular` | — | 1 | 2 |
| `Roughness` | — | 1 | 3 |
| `Anisotropy` | — | 1 | 4 |
| `EmissiveColor` | `Emissive` | 3 | 5 |
| `Opacity` | — | 1 | 6 |
| `OpacityMask` | — | 1 | 7 |
| `Normal` | — | 3 | 8 |
| `Tangent` | — | 3 | 9 |
| `WorldPositionOffset` | `WPO` | 3 | 10 |
| `SubsurfaceColor` | — | 3 | 11 |
| `CustomData0` | `ClearCoat` | 1 | 12 |
| `CustomData1` | `ClearCoatRoughness` | 1 | 13 |
| `AmbientOcclusion` | `AO` | 1 | 14 |
| `Refraction` | — | 3 | 15 |
| `CustomizedUV0` | `CustomizedUVs0` | 2 | 16 |
| `CustomizedUV1` | `CustomizedUVs1` | 2 | 17 |
| `CustomizedUV2` | `CustomizedUVs2` | 2 | 18 |
| `CustomizedUV3` | `CustomizedUVs3` | 2 | 19 |
| `CustomizedUV4` | `CustomizedUVs4` | 2 | 20 |
| `CustomizedUV5` | `CustomizedUVs5` | 2 | 21 |
| `CustomizedUV6` | `CustomizedUVs6` | 2 | 22 |
| `CustomizedUV7` | `CustomizedUVs7` | 2 | 23 |
| `PixelDepthOffset` | `PDO` | 1 | 24 |
| `Displacement` | — | 1 | 26 |
| `DiffuseColor` | — | 3 | *(none)* |
| `SpecularColor` | — | 3 | *(none)* |
| `SurfaceThickness` | — | 1 | *(none)* |
| `FrontMaterial` *(UE 5.4+)* | — | 1 | *(none)* |

`MaterialAttributes` and its alias `Attributes` resolve as material *properties* but are explicitly
rejected as members: `Attrs.MaterialAttributes` fails with
`Unsupported MaterialAttributes member 'MaterialAttributes'.` Any other name fails with the same
message.

> [!NOTE]
> The engine fork macro `MOON_ENGINE` adds five further members, `MooaEncodedAttribute0` through
> `MooaEncodedAttribute4` (4 components, Break output indices 27–31). They do not exist in a stock
> UE 5.3–5.8 build.

### Reading a member

A read creates a **new** `UMaterialExpressionBreakMaterialAttributes` node — reads are not served
from the [node reuse](node-reuse.md) cache, so ten reads of `Attrs.Roughness` produce ten Break nodes.

The Break output is chosen in two steps:

1. **By name.** The attribute's display name for the material is compared case-insensitively against
   every output name on the Break node. This keeps the read path correct across engine builds and
   across Substrate-enabled builds, and it covers first-class attributes that the legacy table below
   does not list.
2. **By index.** If no output name matches, the legacy index in the table above is used.

> [!WARNING]
> The last four members in the table have no legacy index. They are readable **only** when the engine
> build's `BreakMaterialAttributes` node exposes a matching output name; otherwise the read fails with
> `BreakMaterialAttributes does not expose member '{Member}'.` This is engine- and Substrate-build
> dependent — `SurfaceThickness` in particular is not exposed as a Break output on every build. Writes
> to those members are unaffected; only reads go through `BreakMaterialAttributes`.

The read result is a plain numeric value with the member's component count, so it may be swizzled:
`Attrs.BaseColor.r` is a Break output followed by a channel mask.

### Writing a member

Each member write creates a **new** `UMaterialExpressionSetMaterialAttributes` node chained onto the
variable's current value, and rebinds the variable to it. *N* member writes produce *N* Set nodes.

Within one Set node, the first write of a given attribute appends an entry to `AttributeSetTypes` and
a matching input pin named after the attribute's display name; a repeated write of the same attribute
on the same node reuses that pin.

| Rule | Failure |
| :-- | :-- |
| The target splits on its **first** `.`; both halves must be non-empty | `Invalid MaterialAttributes member assignment target '{Target}'.` |
| The base name must already be a Graph variable | `Unknown MaterialAttributes variable '{Base}'.` |
| The base variable must hold a `MaterialAttributes` value | `Graph variable '{Base}' is not a MaterialAttributes value.` |
| The member name must resolve and must not be `MaterialAttributes` | `Unsupported MaterialAttributes member '{Member}'.` |
| The member must have a numeric component count | `MaterialAttributes member '{Member}' cannot be assigned from Graph code.` |
| The value must coerce to that component count | `MaterialAttributes member '{Member}' expects {Count} component(s). {Error}` |
| The statement must have a right-hand side | `MaterialAttributes member assignment '{Target}' requires a value.` |

Coercion follows the normal rules: a scalar splats, a wider value is silently narrowed by a leading
channel mask, and a float2 assigned to a float3 member is an error. See
[Conversions](conversions.md).

> [!NOTE]
> The first-`.` split means `Attrs.BaseColor.r = 1.0;` is parsed as base `Attrs`, member `BaseColor.r`
> and fails with `Unsupported MaterialAttributes member 'BaseColor.r'.` There is no per-channel
> attribute write; build the full value first.
>
> A declaration is classified before a member target is considered, so `MaterialAttributes A.B = x;`
> declares a variable literally named `A.B`. It is not a member write.

### Brace initializers on a member

`Attrs.BaseColor = {1.0, 0.35, 0.1};` is legal. The target type is taken from the member's own type,
and the braces desugar to a constructor call of that type — here `float3(1.0, 0.35, 0.1)`, which
constant-folds into a single `Constant3Vector`. A member whose type cannot be expressed as a scalar or
vector fails with `MaterialAttributes member '{Member}' does not have a numeric scalar/vector type.`

## Passing attribute values around

| Context | Behaviour |
| :-- | :-- |
| `Outputs { MaterialAttributes Attrs; }` | Declares an attribute-typed output variable. It fixes the expected type of whatever the `Graph` binds to it; it does **not** by itself create a Graph variable. |
| `Base.MaterialAttributes = Attrs;` | Binds the value to the material and auto-enables *Use Material Attributes*. |
| `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` / `VirtualFunction` inputs and outputs | Accepted. The argument must already be a `MaterialAttributes` value; nothing is converted. |
| `GraphFunction` result type | Accepted — `MaterialAttributes` is one of the recognized result-type tokens. |
| `if` / `else` branch output | Accepted when **both** branches produce a `MaterialAttributes` value; the merged value keeps the attribute flag. Mixing with a numeric branch fails with `Graph if branches cannot mix MaterialAttributes and numeric values.` |
| `StaticSwitchParameter` call branches | Accepted when both branches are attribute values; mixing fails with `StaticSwitchParameter '{Name}' cannot mix MaterialAttributes and numeric branches.` |
| Constructor argument | Rejected — `Constructor '{Name}' cannot use MaterialAttributes arguments.` |
| `AppendVector` part | Rejected — `AppendVector inputs must be numeric scalar/vector values.` |
| Arithmetic operand | Rejected — `Arithmetic operators cannot be applied to MaterialAttributes values.` |

Binding `Base.MaterialAttributes` is documented in full in
[Output bindings](../language/output-bindings.md).

## Substrate interaction

`Substrate` is a **separate** zero-component value type, not an attribute set, and requires UE 5.4 or
newer. The two never mix.

| Situation | Behaviour |
| :-- | :-- |
| `Substrate` value assigned to a `MaterialAttributes` target | `Expected a MaterialAttributes value.` |
| `MaterialAttributes` value assigned to a `Substrate` target | `Expected a Substrate value.` |
| `Substrate` value assigned to any numeric target, including an attribute member | `Substrate values cannot be assigned to numeric outputs.` |
| `Substrate` value with `.member` or a swizzle | `Substrate values do not support swizzle/member access in Graph.` |
| `Substrate` value as an `if` branch output | `Graph if statement cannot select Substrate value '{Name}'.` |
| `Attrs.FrontMaterial` | The member name resolves on UE 5.4+ and is typed as **one numeric component**, so it accepts a scalar, not a `Substrate` value. The supported route for a Substrate surface is the `Base.FrontMaterial` output binding. |
| `Base.MaterialAttributes` and `Base.FrontMaterial` in one `Shader` | Rejected at generation: `{File}: Base.FrontMaterial and Base.MaterialAttributes cannot be used by the same Shader.` |
| `Substrate` declared on UE 5.3 | `Graph variable '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` |

See [Substrate builtins](../builtins/substrate.md) for the `Substrate.*` call surface.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

| Message | Cause |
| :-- | :-- |
| `Failed to create a MakeMaterialAttributes node.` | The node for a `MaterialAttributes` declaration could not be created. |
| `Unsupported MaterialAttributes member '{Member}'.` | The member name does not resolve, or it resolves to `MaterialAttributes` / `Attributes`. Emitted from the read path, the write path and the brace-initializer type resolver. |
| `MaterialAttributes member '{Member}' cannot be read as a numeric value.` | Defensive guard: a resolved member with no numeric component count reached the read path. |
| `MaterialAttributes member '{Member}' cannot be assigned from Graph code.` | Defensive guard for the same condition on the write path. |
| `MaterialAttributes member '{Member}' does not have a numeric scalar/vector type.` | A brace initializer targets a member whose type is not a 1–4 component numeric type. |
| `Failed to create a BreakMaterialAttributes node.` | The read node could not be created. |
| `BreakMaterialAttributes does not expose member '{Member}'.` | Neither the display-name match nor the legacy index found an output on this engine build. |
| `Invalid MaterialAttributes member assignment target '{Target}'.` | The target has no `.`, or an empty base or member half. |
| `Unknown MaterialAttributes variable '{Base}'.` | The base name is not a Graph variable. Declare it before writing members. |
| `Graph variable '{Base}' is not a MaterialAttributes value.` | The base variable holds a numeric, texture or `Substrate` value. |
| `MaterialAttributes member '{Member}' expects {Count} component(s). {Error}` | The assigned value could not be coerced to the member's width. |
| `MaterialAttributes member assignment '{Target}' requires a value.` | A member target with no right-hand side. |
| `Failed to create a SetMaterialAttributes node.` | The write node could not be created. |
| `Failed to connect '{Base}' as the SetMaterialAttributes base value.` | The chained base connection failed. |
| `Failed to connect MaterialAttributes member '{Member}'.` | The member input pin connection failed. |
| `Expected a MaterialAttributes value.` | A non-attribute value was assigned to a `MaterialAttributes` variable, output or function input. |
| `MaterialAttributes values cannot be assigned to numeric outputs.` | An attribute value was assigned to a scalar/vector target. |
| `Arithmetic operators cannot be applied to MaterialAttributes values.` | An attribute value used as an operand of `+ - * /`. |
| `Constructor '{Name}' cannot use MaterialAttributes arguments.` | An attribute value passed to `float3(…)`, `vec4(…)` and the rest. |
| `Graph if branches cannot mix MaterialAttributes and numeric values.` | One `if` branch produces an attribute value and the other a numeric value. |
| `Brace initializer assignment for '{Name}' requires a declared scalar or vector target type.` | A brace initializer was assigned to a name whose target type could not be resolved. |

## Example

```c
Shader(Name="Docs/M_MatAttrs")
{
    Properties {
        vec3            BaseTint = vec3(0.6, 0.8, 1.0);
        ScalarParameter R        = 0.35 [Group="Surface"];
    }

    Settings {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs {
        MaterialAttributes Attrs;
        Base.MaterialAttributes = Attrs;
    }

    Graph {
        MaterialAttributes Attrs;

        Attrs.BaseColor = BaseTint;
        Attrs.Roughness = R;
        Attrs.Metallic  = 0.0;

        // Read a member back out and feed it to another member.
        float Rough     = Attrs.Roughness;
        Attrs.Specular  = Rough * 0.5;
    }
}
```

Generated nodes:

```text
VectorParameter           BaseTint                      (property node)
ScalarParameter           R
MakeMaterialAttributes                                   -> Attrs
SetMaterialAttributes     base=Make,  BaseColor=BaseTint -> Attrs
SetMaterialAttributes     base=Set#1, Roughness=R        -> Attrs
Constant                  0.0
SetMaterialAttributes     base=Set#2, Metallic=0.0       -> Attrs
BreakMaterialAttributes   in=Set#3, out="Roughness"      -> Rough
Constant                  0.5
Multiply                  Rough * 0.5
SetMaterialAttributes     base=Set#3, Specular=Multiply  -> Attrs
```

The material's *Use Material Attributes* flag is set by the `Base.MaterialAttributes` binding.

## See also

- [Output bindings](../language/output-bindings.md) — `Base.MaterialAttributes` and the full target catalogue
- [Declarations](declarations.md) — declaration forms, default values and the redeclaration rule
- [Statements](statements.md) — member assignment as a statement form
- [Conversions](conversions.md) — the coercion applied to every member write
- [Constructors](constructors.md) — what a brace initializer on a member desugars to
- [Swizzle](swizzle.md) — masking the result of a member read
- [`if` / `else`](if.md) — selecting between two attribute values
- [Calls](calls.md) — passing attribute values into and out of functions
- [Node reuse](node-reuse.md) — which nodes are deduplicated, and why Break nodes are not
- [Substrate builtins](../builtins/substrate.md) — the `Substrate.*` surface and the UE 5.4 gate
- [Types](../language/types.md) — every type token and where each is valid
