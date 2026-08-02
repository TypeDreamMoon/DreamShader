# Declarations

> [DreamShader](../index.md) » [Graph](index.md) » **Declarations**

A statement that introduces a new name into the `Graph` value map and binds it to a node, an output
index and a channel mask.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph` block |
| Kind | statement |
| Generates | nodes for the initializer; for an uninitialized numeric declaration, a zero `Constant` (plus `AppendVector` for widths above 1) |

## Synopsis

```c
<type> <name> [ = { <expression> | <brace-initializer> } ]
     [ , <name> [ = { <expression> | <brace-initializer> } ] ] … ;

brace-initializer := { [ <expression> [ , <expression> ] … ] }
```

A declaration is recognized by splitting the text before the top-level `=` at its **last** top-level
whitespace character. Parenthesis depth and string state are tracked; brace and bracket depth are
not. Both halves must be non-empty.

> [!NOTE]
> The name half is checked only for being non-empty — it is **not** validated as an identifier.
> `float3 A.B = x;` declares a variable literally named `A.B`; it is not a `MaterialAttributes`
> member write. Declarators after the first in a comma list **are** identifier-checked.

## Accepted type tokens

| Token(s) | Components | Notes |
| :-- | --: | :-- |
| `float` `float1` `half` `half1` `int` `uint` `bool` | 1 | |
| `float2` `half2` `vec2` `int2` `uint2` `bool2` `ivec2` `uvec2` `bvec2` | 2 | |
| `float3` `half3` `vec3` `int3` `uint3` `bool3` `ivec3` `uvec3` `bvec3` | 3 | |
| `float4` `half4` `vec4` `int4` `uint4` `bool4` `ivec4` `uvec4` `bvec4` | 4 | |
| `MaterialAttributes` | 0 | see [MaterialAttributes](material-attributes.md) |
| `Substrate` | 0 | **UE 5.4+**; requires an initializer |
| `StaticBool` `StaticBoolParameter` | 1 | *(unreleased)* |
| `Texture2D` `SamplerState` | 0 | texture object of dimension `Texture2D`; requires an initializer |
| `TextureCube` | 0 | requires an initializer |
| `Texture2DArray` | 0 | requires an initializer |
| `Texture3D` `VolumeTexture` | 0 | dimension `VolumeTexture`; requires an initializer |

Comparison is case-insensitive. For the numeric rows, `MaterialAttributes` and `Substrate` the token
also has **all internal spaces removed** before matching, so `float 3` resolves as `float3` and
`Material Attributes` as `MaterialAttributes`; the texture rows and `StaticBool` are matched on the
token as written, so `Texture 2D` does **not** resolve.
`int`, `uint`, `bool` and `half` are spellings of the same float widths — see
[Type tokens](../language/types.md).

## Uninitialized declarations

`<type> <name>;` binds the name to a default value:

| Declared type | Result |
| :-- | :-- |
| scalar (1 component) | one `Constant` node with `R = 0` |
| vector (2–4 components) | the **same** zero `Constant` appended N times through `AppendVector` — 1 constant node and N−1 append nodes |
| `MaterialAttributes` | a `MakeMaterialAttributes` node, component count 0 |
| `Substrate` | error — `Graph variable type '{Type}' requires an explicit initializer.` |
| any texture type, `SamplerState` | error — `Graph variable type '{Type}' requires an explicit initializer.` |
| unresolvable token | error — `Unsupported Graph variable type '{Type}'.` |

```c
float3 c;              // Constant(0) -> AppendVector -> AppendVector
MaterialAttributes A;  // MakeMaterialAttributes, ready for member writes
Texture2D T;           // error: requires an explicit initializer
```

## Declarations with an initializer

The initializer is evaluated first, then the declaration is typed:

| Order | Step |
| --: | :-- |
| 1 | the type token must resolve, else `Unsupported Graph variable type '{Type}' for '{Name}'.` |
| 2 | if the value carries an **authoritative** component count, both the value and the declared type are plain numeric, and the counts differ — the value is stored **as-is**, unchanged, with no diagnostic (see [below](#authoritative-widths-override-the-declared-width)) |
| 3 | otherwise the value is coerced to the declared type; failure gives `Graph variable '{Name}' is declared as '{Type}' but assigned an incompatible value. {Detail}` |

Coercion at step 3 silently narrows a wider value by prefixing an `r` / `rg` / `rgb` mask and splats
a scalar up to the declared width. It never widens 2 components to 3. See
[Conversions](conversions.md).

## Authoritative widths override the declared width

Some values know their own width for certain. When such a value is assigned to a declaration of a
different plain-numeric width, the declared type is **ignored**: the value is stored unchanged, no
coercion happens, and no diagnostic is produced. The mismatch surfaces later, at the first operator
or output binding that cannot reconcile it.

A value is authoritative when it comes from:

| Source | Width |
| :-- | --: |
| `UE.TexCoord` / `TextureCoordinate`, `UE.Panner` / `Panner`, `ScreenPosition`, `Rotator`, `SceneTexelSize` | 2 |
| `UE.WorldPosition`, `UE.ObjectPositionWS`, `UE.CameraVectorWS`, `UE.VertexNormalWS`, `UE.VertexTangentWS`, `Transform`, `TransformPosition`, `SkyAtmosphereLightDirection`, `PixelNormalWS`, `CrossProduct` | 3 |
| `PixelDepth`, `TwoSidedSign`, `Arctangent2Fast`, `Length`, `MaterialXLuminance` | 1 |
| a constant-folded vector constructor — `vec3(0.5)`, `float4(1, 0, 0, 1)` | the constructor's width |
| `dot(a, b)` | 1 |
| a swizzle of an authoritative value | the swizzle's width |
| a binary operator, or an `AppendVector`, with at least one authoritative operand | `max` of the operand widths |

```c
Graph = {
    float2 dir = UE.CameraVectorWS();   // no error; 'dir' is 3 components, not 2
    Color = dir * Tint;                 // fails here if Tint is 4 components:
                                        //   Operator '*' requires matching vector sizes or a
                                        //   scalar/vector pair, got 3 and 4 component(s).
}
```

> [!WARNING]
> The declared width of such a variable is documentation only. Reading `float2 dir` and expecting
> two channels is wrong; `dir` carries three. Add an explicit swizzle (`UE.CameraVectorWS().xy`) or a
> constructor when a narrower value is actually wanted.

Binary operators deliberately do **not** narrow an operand to match an authoritative one — they
report a size mismatch instead, so that channels are never dropped silently. Assignment,
declaration coercion, function inputs and attribute writes do narrow silently. See
[Conversions](conversions.md).

## Comma declarators

```c
float a = 1, b, c = 3;
```

One statement per declarator is produced. Rules:

| Rule | Behaviour |
| :-- | :-- |
| Recognition | applies only when splitting on top-level `,` yields more than one segment **and** the first segment, minus its `= …`, splits into a type and a name |
| Shared type | every declarator uses the type token of the **first** declarator; a type token on a later declarator is not accepted |
| Declarator names | must be bare identifiers — `[A-Za-z_][A-Za-z0-9_]*` |
| Initializers | each declarator may carry its own `= <expression>` or `= { … }`, or none |
| Source location | all resulting statements report the **same** line and column as the whole list |

`float a = 1, b, c = 3;` declares three 1-component values: `a` initialized to `1`, `b` defaulted to
`0`, `c` initialized to `3`.

| Message | Cause |
| :-- | :-- |
| `In Graph statement '{Text}': '{Declarator}' is not a valid declarator in a comma-separated declaration.` | a declarator after the first is not a bare identifier |

## Brace initializers

A right-hand side whose trimmed text is at least two characters long, starts with `{` and ends with
`}` is a brace initializer. It is re-serialised as `<TargetType>( <inner> )` and evaluated as an
ordinary constructor call. Brace initializers are therefore **exactly** constructor calls and inherit
every rule of [Constructors](constructors.md): positional arguments only, a single scalar splats to
all channels, multiple arguments must sum to exactly the target width.

`{}` is special-cased: it produces the target type's default value, as if the declaration had no
initializer.

Target-type resolution, in order:

| Order | Situation | Target type |
| --: | :-- | :-- |
| 1 | the statement is a declaration | the declared type token |
| 2 | the target is a `MaterialAttributes` member | the attribute's own type |
| 3 | the target is an existing variable | derived from its component count: 0 → `MaterialAttributes`, 1 → `float`, 2 → `float2`, 3 → `float3`, 4 → `float4` |
| 4 | the target matches an `Outputs` declaration | the same component-count mapping |
| 5 | none of the above | error — `Brace initializer assignment for '{Name}' requires a declared scalar or vector target type.` |

Texture and `Substrate` targets are rejected at steps 3 and 4.

> [!WARNING]
> Nested braces do not work. `float4 m = {{1,2},{3,4}};` re-serialises to `float4({1,2},{3,4})`, and
> `{` is not a token the expression lexer knows, so it fails with
> `Invalid brace initializer for type 'float4'. Unexpected token '{' in Graph expression.`

```c
Graph = {
    float r = 1.0;
    float g = 0.5;
    float b = 0.2;
    vec3 rgb  = {r, g, b};      // -> vec3(r, g, b)
    vec4 rgba = {rgb, 1.0};     // -> vec4(rgb, 1.0), mixed-width packing
    Color = rgba.rgb;
}
```

## Redeclaration

A declaration whose name is already bound in the current value map fails:

```text
Graph variable '{Name}' is declared more than once.
```

The lookup is **case-insensitive**: an exact match is tried first, then a case-insensitive scan. So
`float a = 1; float A = 2;` is a redeclaration error, while `A` and `a` refer to the same value
everywhere else.

Assignment to an existing name is not a redeclaration — omit the type token to reassign. See
[Statements](statements.md#assignment).

## Scope

**There is no block scope.** One value map exists per builder — that is, one per `Shader` and one per
material function — and every declaration writes into it.

| Situation | Visibility after the statement |
| :-- | :-- |
| Declaration at body level | visible for the rest of the body |
| Declaration inside an `if` or `else` branch | see below |
| A declared `Properties` parameter read inside a branch | not treated as a branch output; the property node is shared, not merged |

Branches are executed against **copies** of the enclosing map, and the copies are then merged:

| Case | Outcome |
| :-- | :-- |
| the name is declared in **both** branches, and the two values have the same shape | merged through a `UMaterialExpressionIf` and written into the enclosing map — **it is visible after the `if`** |
| the name is declared in **both** branches with different shapes | error — `Graph if branches assign variable '{Name}' with inconsistent types` |
| the name is declared in **only one** branch | error — `Graph if statement could not resolve both branch values for '{Name}'.` |
| the name is a texture or `Substrate` value | error — `Graph if statement cannot select texture value '{Name}'.` / `… cannot select Substrate value '{Name}'.` |

> [!WARNING]
> A branch-local temporary is not local. Declaring a helper in one branch only is an error, not a
> discarded name. Either declare it in both branches with the same width, or hoist the declaration
> above the `if`.

Because a merged name becomes an ordinary entry of the enclosing map, declaring that same name again
after the `if` is a redeclaration error. Merge semantics in full are on [if / else](if.md).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

| Message | Cause |
| :-- | :-- |
| `Unsupported Graph variable type '{Type}'.` | the token of an uninitialized declaration does not resolve |
| `Unsupported Graph variable type '{Type}' for '{Name}'.` | the token of an initialized declaration does not resolve |
| `Graph variable '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` declared on UE 5.3 |
| `Graph variable type '{Type}' requires an explicit initializer.` | a texture, `SamplerState` or `Substrate` declaration with no `=` |
| `Substrate requires Unreal Engine 5.4 or newer.` | the type resolver rejected `Substrate` on an older engine |
| `Graph variable '{Name}' is declared more than once.` | redeclaration; matched case-insensitively |
| `Graph variable '{Name}' is declared as '{Type}' but assigned an incompatible value. {Detail}` | the initializer could not be coerced to the declared type |
| `Failed to declare Graph variable '{Name}'. {Detail}` | wrapper around a default-value or typing failure |
| `Failed to evaluate Graph assignment for '{Name}'. {Detail}` | the initializer expression failed |
| `Failed to create a default literal node.` | the zero `Constant` for a default value could not be created |
| `Failed to create a MakeMaterialAttributes node.` | a `MaterialAttributes` default value could not be created |
| `Initializer '{Text}' is not a valid brace initializer.` | the text did not start with `{` and end with `}` |
| `Invalid brace initializer for type '{Type}'. {Detail}` | the re-serialised constructor call failed |
| `Brace initializer assignment for '{Name}' requires a declared scalar or vector target type.` | no target type could be resolved |
| `Brace initializer assignment is not supported for texture variable '{Name}'.` | brace initializer on a texture variable |
| `Brace initializer assignment is not supported for Substrate variable '{Name}'.` | brace initializer on a `Substrate` variable |
| `Brace initializer assignment is not supported for texture output '{Name}'.` | brace initializer on a texture `Outputs` declaration |
| `Brace initializer assignment is not supported for Substrate output '{Name}'.` | brace initializer on a `Substrate` `Outputs` declaration |
| `In Graph statement '{Text}': '{Declarator}' is not a valid declarator in a comma-separated declaration.` | non-identifier declarator |
| `Graph if statement could not resolve both branch values for '{Name}'.` | a name declared in only one branch |
| `Graph if branches assign variable '{Name}' with inconsistent types` | branch declarations of different shapes *(no trailing period)* |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="DreamShaderTests/Corpus/M_Declarations")
{
    Properties = {
        vec4  Src = vec4(0.1, 0.2, 0.3, 0.4);
        float K   = 2.0;
    }
    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs  = { vec3 Color; Base.EmissiveColor = Color; }

    Graph = {
        float a = 1.0, b = 0.5, c;      // comma declarators; 'c' defaults to 0
        vec3  rgb  = Src.rgb;           // ordered swizzle: no node, just a mask
        vec4  full = {rgb, a};          // brace initializer -> vec4(rgb, a)
        vec3  lit;                      // Constant(0) + 2x AppendVector

        if (K > 1.0) {
            lit = full.rgb * b;         // assignment, not declaration: 'lit' already exists,
        } else {                        // so both branches change the same name and merge
            lit = full.rgb * c;
        }

        Color = lit;
    }
}
```

## See also

- [Statements](statements.md) — every statement form and the classification order
- [Type tokens](../language/types.md) — the full per-context validity matrix
- [Constructors](constructors.md) — the rules a brace initializer inherits
- [Conversions](conversions.md) — narrowing, splatting, and where each applies
- [Expressions](expressions.md) — what may appear on the right of `=`
- [if / else](if.md) — branch execution and the merge algorithm
- [Name resolution](name-resolution.md) — case-insensitive lookup and shadowing
- [MaterialAttributes](material-attributes.md) — declaring and writing attribute values
- [Swizzles](swizzle.md) — how a swizzle affects a declared value's width
- [Node reuse](node-reuse.md) — why two identical initializers share one node
- [Unsupported constructs](unsupported.md) — `return`, `for`, `+=` and the messages they produce
- [Output bindings](../language/output-bindings.md) — `Outputs` declarations and their initializers
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
