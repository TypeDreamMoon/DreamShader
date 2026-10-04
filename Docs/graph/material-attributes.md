# MaterialAttributes

> [DreamShader](../index.md) » [Graph](index.md) » **MaterialAttributes**

A struct-like Graph value that carries a whole material's attribute set as one connection, written and
read member by member. *(since 1.2.5)*

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` declaration, or a function `Inputs` / `Outputs` section |
| Kind | value type |
| Generates | `UMaterialExpressionMakeMaterialAttributes` (an empty set), `UMaterialExpressionSetMaterialAttributes` (written attributes, where the value is used), `UMaterialExpressionBreakMaterialAttributes` (member reads) |

## Synopsis

```c
MaterialAttributes <name> ;                       // creates an empty attribute set
MaterialAttributes <name> = <material-attributes-expression> ;

<name> . <member> = <expression> ;                // member write

<expression> := <name> . <member>                 // member read
```

`.`, `=` and `;` are literal DreamShaderLang punctuation. `<member>` is one of the names in
[Members](#members).

## The value type

| Property | Value |
| :-- | :-- |
| Component count | none — an attribute set is a type of its own, never a scalar or a vector |
| Type token | `MaterialAttributes`, matched case-insensitively |
| Alternate spelling | none: a type token is one token, so `Material Attributes` no longer resolves *(since 2.0.0)* |
| Arithmetic | rejected — [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) |
| Swizzle | not applicable; `.member` on such a value is an attribute read, never a channel mask |
| Assignment | only from another `MaterialAttributes` value; anything else is [`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228) |
| Assignment to a numeric target | rejected — `DSH4228` |

## Creating a value

| Form | Effect |
| :-- | :-- |
| `MaterialAttributes Attrs;` | An empty set: one `MakeMaterialAttributes` node with every input unconnected. Unlike `Texture2D` and `Substrate`, a `MaterialAttributes` declaration does **not** require an initializer. |
| `MaterialAttributes Attrs = {};` | Identical to the bare declaration — an empty brace initializer is no initializer. |
| `MaterialAttributes B = A;` | `B` takes `A`'s value. No node is created. |
| `MaterialAttributes Attrs = F_Layer(uv);` | Any expression whose value is a `MaterialAttributes` — a function call, a `StaticSwitchParameter` call. |

> [!WARNING]
> **In a `Shader`, an `Outputs` declaration creates the variable** *(since 2.0.0)*. Every `Outputs`
> declaration is declared at the top of the material's body, and a `MaterialAttributes` one starts as
> an empty set, so `Attrs.BaseColor = Tint;` works with no `Graph` declaration at all. A
> `MaterialAttributes Attrs;` in the `Graph` names that same variable and adds nothing:
>
> ```c
> Outputs { MaterialAttributes Attrs; Base.MaterialAttributes = Attrs; }
> Graph   { MaterialAttributes Attrs; Attrs.BaseColor = Tint; }
> ```
>
> In a function block a `MaterialAttributes` output is the function's output: the attributes written
> to it make its set, and reading one never written is
> [`DSH4370`](../diagnostics/DSH4xxx.md#dsh4370).

> [!NOTE]
> `MaterialAttributes B = A;` is a value copy, not an alias. A later `B.Roughness = r;` changes **`B`**
> only; `A` keeps its attributes.

## Members

The attribute table is the engine's *(since 2.0.0)*: an attribute this engine does not have is
[`DSH5200`](../diagnostics/DSH5xxx.md#dsh5200). A member name is matched exactly first (name or
alias), then ignoring case, with the warning [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276). Aliases
are exact synonyms.

| Member | Aliases | Components |
| :-- | :-- | :-- |
| `BaseColor` | — | 3 |
| `Metallic` | — | 1 |
| `Specular` | — | 1 |
| `Roughness` | — | 1 |
| `Anisotropy` | — | 1 |
| `EmissiveColor` | `Emissive` | 3 |
| `Opacity` | — | 1 |
| `OpacityMask` | — | 1 |
| `Normal` | — | 3 |
| `Tangent` | — | 3 |
| `WorldPositionOffset` | `WPO` | 3 |
| `SubsurfaceColor` | — | 3 |
| `CustomData0` | `ClearCoat` | 1 |
| `CustomData1` | `ClearCoatRoughness` | 1 |
| `AmbientOcclusion` | `AO` | 1 |
| `Refraction` | — | 3 |
| `CustomizedUV0` … `CustomizedUV7` | — | 2 |
| `PixelDepthOffset` | `PDO` | 1 |
| `Displacement` | — | 1 |
| `DiffuseColor` | — | 3 |
| `SpecularColor` | — | 3 |
| `SurfaceThickness` | — | 1 |
| `FrontMaterial` *(UE 5.4+)* | — | a `Substrate` value |
| `MaterialAttributes` | `Attributes` | the whole set |

The 1.x aliases `CustomizedUVs0` … `CustomizedUVs7` are not in the alias table *(since 2.0.0)*;
write `CustomizedUV0` … `CustomizedUV7`.

`MaterialAttributes` (alias `Attributes`) resolves as a member *(since 2.0.0; 1.x rejected it)*: it
is the whole set, and it is what the binding `Base.MaterialAttributes = Attrs;` writes. To replace a
`Graph` variable's whole set, assign the variable itself: `Attrs = Other;`.

> [!NOTE]
> The engine fork macro `MOON_ENGINE` adds the members `MoonEncodedAttribute0` through
> `MoonEncodedAttribute4`. The pre-rename spelling `MooaEncodedAttribute0` …
> `MooaEncodedAttribute4` is accepted as an alias, so decompiled MooaToon functions still parse. They
> do not exist in a stock UE 5.3–5.8 build.

### Reading a member

A read of an attribute the body has already written in this value is that written value; no node is
made. Any other read comes from one `UMaterialExpressionBreakMaterialAttributes` per value, shared by
all of that value's reads *(since 2.0.0; 1.x made one Break node per read)*.

> [!WARNING]
> A read that has to go through the Break node needs the engine's Break node to publish the
> attribute; one it does not publish is [`DSH8216`](../diagnostics/DSH8xxx.md#dsh8216). This depends
> on the engine build. A value with nothing to read from — a function's `MaterialAttributes` output
> before anything was written to it — is `DSH4370`.

The read result is a plain numeric value with the member's component count, so it may be swizzled:
`Attrs.BaseColor.r` is an attribute read followed by a channel selection.

### Writing a member

A member write records the value for that attribute; a later write of the same attribute replaces
it. Nodes are made where the value is **used** — passed to a function or a pin, assigned to an
output, bound to the material: one `UMaterialExpressionSetMaterialAttributes` on top of the value's
set, with one pin per attribute written so far *(since 2.0.0; 1.x chained one Set node per write)*.
A value with no set underneath — a function's output — becomes one `MakeMaterialAttributes` with the
written pins instead.

| Rule | Code when it fails |
| :-- | :-- |
| The target is `name.member`; a member of a member is not a target | [`DSH2206`](../diagnostics/DSH2xxx.md#dsh2206) |
| The base name must be declared | [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200) |
| The base must hold a `MaterialAttributes` value | on a number the member is read as a swizzle, [`DSH4230`](../diagnostics/DSH4xxx.md#dsh4230); on a texture [`DSH4206`](../diagnostics/DSH4xxx.md#dsh4206); on a `Substrate` value [`DSH4207`](../diagnostics/DSH4xxx.md#dsh4207) |
| The member name must resolve | `DSH5200` |
| The value must fit the member's type | `DSH4228`; a wider vector gives its leading components with the info [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) |

Coercion follows the normal rules: a scalar spreads, a wider value is cut to its leading components
(`DSH5289`), and a float2 assigned to a float3 member is an error. See
[Conversions](conversions.md).

> [!NOTE]
> `Attrs.BaseColor.r = 1.0;` is `DSH2206`. There is no per-channel attribute write in a 1.x Graph;
> build the full value first.
>
> A declared name is one identifier, so `MaterialAttributes A.B = x;` is
> [`DSH2154`](../diagnostics/DSH2xxx.md#dsh2154) *(since 2.0.0)*.

### Brace initializers on a member

`Attrs.BaseColor = {1.0, 0.35, 0.1};` is [`DSH2162`](../diagnostics/DSH2xxx.md#dsh2162)
*(since 2.0.0)*: a brace list is read only as a declaration's initializer. Write the constructor,
`Attrs.BaseColor = float3(1.0, 0.35, 0.1);`.

## Passing attribute values around

| Context | Behaviour |
| :-- | :-- |
| `Outputs { MaterialAttributes Attrs; }` | In a `Shader`, declares the variable `Attrs`, starting as an empty set (see above). |
| `Base.MaterialAttributes = Attrs;` | Binds the value to the material and turns *Use Material Attributes* on. |
| `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` / `VirtualFunction` inputs and outputs | Accepted. The argument must already be a `MaterialAttributes` value; nothing is converted (`DSH4226` for an input, [`DSH4218`](../diagnostics/DSH4xxx.md#dsh4218) for an output variable of another type). |
| `GraphFunction` result type | Accepted — `MaterialAttributes` is one of the recognized result-type tokens. |
| `if` / `else` | The arms' attributes are merged one by one: one conditional node per attribute the arms disagree on. Assigning a different whole set in the two arms is [`DSH4375`](../diagnostics/DSH4xxx.md#dsh4375) — see [if / else](if.md#what-cannot-be-selected). |
| `StaticSwitchParameter` call branches | Accepted when both branches are attribute values. |
| Constructor argument | Rejected — `DSH4226`. |
| Arithmetic operand | Rejected — `DSH4226`. |

Binding `Base.MaterialAttributes` is documented in full in
[Output bindings](../language/output-bindings.md).

## Substrate interaction

`Substrate` is a **separate** value type, not an attribute set, and requires UE 5.4 or newer. The
two never convert into each other.

| Situation | Behaviour |
| :-- | :-- |
| `Substrate` value assigned to a `MaterialAttributes` target, or the reverse | `DSH4228` |
| `Substrate` value assigned to any numeric target, including an attribute member | `DSH4228` |
| `Substrate` value with `.member` | `DSH4207`, except on a `Substrate` value still being built — see [Substrate builtins](../builtins/substrate.md) |
| `Substrate` value as an `if` branch value | a `StaticSwitch` under a static condition, otherwise `Substrate.Select` (UE 5.6+) or [`DSH4378`](../diagnostics/DSH4xxx.md#dsh4378) — see [if / else](if.md#what-cannot-be-selected) |
| `Attrs.FrontMaterial` | The member resolves on UE 5.4+ and is typed **`Substrate`** *(since 2.0.0; 1.x typed it as one number)*. The usual route for a Substrate surface is still the `Base.FrontMaterial` output binding. |
| `Base.MaterialAttributes` and `Base.FrontMaterial` in one `Shader` | Not refused *(since 2.0.0)*. Both are wired, and *Use Material Attributes*, which the first one turns on, makes the material read the attribute set and not its individual pins. |
| `Substrate` node on an engine without it | [`DSH5294`](../diagnostics/DSH5xxx.md#dsh5294) |

See [Substrate builtins](../builtins/substrate.md) for the `Substrate.*` call surface.

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH5200` | the member name is not an attribute of this engine |
| `DSH5276` | *warning:* the member name matched an attribute only ignoring case |
| `DSH2206` | the target of a member write is not `name.member` — e.g. `Attrs.BaseColor.r = …` |
| `DSH2162` | a brace list on the right of a member write |
| `DSH4200` | the base name is not declared |
| `DSH4230`, `DSH4206`, `DSH4207` | the base of a member holds a number, a texture or a `Substrate` value |
| `DSH4226` | an attribute set used in arithmetic, as a constructor argument or as a function input of another type |
| `DSH4228` | an attribute set assigned where something else is expected, or the reverse; a member value that does not fit |
| `DSH5289` | *info:* a wider value was cut to the member's leading components |
| `DSH4370` | a member read from a value that holds nothing to read |
| `DSH4372` | a member set in one `if` arm only, on a value with nothing underneath, is read after the `if` |
| `DSH4375` | the two arms of an `if` leave a different whole set in the same variable |
| `DSH8216` | the engine's Break node does not publish the attribute being read |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214), [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212) | the emitter could not create a Make / Set / Break node, or could not connect an attribute pin |

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
VectorParameter           BaseTint                       (property node)
ScalarParameter           R
MakeMaterialAttributes                                    -> Attrs (empty set)
Constant                  0.0
Constant                  0.5
Multiply                  R * 0.5                         (the read of Roughness is R itself)
SetMaterialAttributes     base=Make, BaseColor=BaseTint, Roughness=R,
                          Metallic=0.0, Specular=Multiply -> Base.MaterialAttributes
```

The material's *Use Material Attributes* flag is set by the `Base.MaterialAttributes` binding.

## See also

- [Output bindings](../language/output-bindings.md) — `Base.MaterialAttributes` and the full target catalogue
- [Declarations](declarations.md) — declaration forms, default values and the redeclaration rule
- [Statements](statements.md) — member assignment as a statement form
- [Conversions](conversions.md) — the conversion applied to every member write
- [Constructors](constructors.md) — the constructor to write instead of a brace list
- [Swizzle](swizzle.md) — selecting channels of a member read
- [`if` / `else`](if.md) — choosing between two attribute values
- [Calls](calls.md) — passing attribute values into and out of functions
- [Node reuse](node-reuse.md) — which nodes are deduplicated
- [Substrate builtins](../builtins/substrate.md) — the `Substrate.*` surface and the UE 5.4 gate
- [Types](../language/types.md) — every type token and where each is valid
