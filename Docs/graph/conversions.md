# Conversions

> [DreamShader](../index.md) » [Graph](index.md) » **Conversions**

The rules that adapt a `Graph` value to the type of the place it goes, and what they cost in nodes.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — applies to every typed site inside a `Graph { … }` body and to `Outputs` bindings |
| Kind | implicit conversion rules |
| Generates | **nothing** for a change of number kind or a scalar broadcast; a `UMaterialExpressionComponentMask` when a 1.x source cuts a wider value down; nothing when the types already match |

*(since 2.0.0)* A 1.x `Graph` body is bound by the 2.0 binder, and every value has a **type**: a
number of 1 to 4 components of one kind (`float`, `half`, `double`, `int`, `uint`, `bool`), a texture
of one texture type, `SamplerState`, `MaterialAttributes` or `Substrate`. A conversion is the binder
comparing a value's type with the type of the place it goes. What a 1.x source may do and a `.dss`
may not — cut a wider value down — is legacy rule L22, and it says so.

## Synopsis

Conversion is not written by the author; it is applied automatically wherever a value meets a typed
place.

```text
convert( <value>, <place type> ) -> <value> | <error>

<type> := MaterialAttributes
        | Substrate
        | SamplerState
        | Texture( { Texture2D | TextureCube | Texture2DArray | Texture3D | VolumeTexture } )
        | Number( { float | half | double | int | uint | bool }, { 1 | 2 | 3 | 4 } )
```

## Where conversion applies

| # | Site | Type taken from | Code when nothing fits |
| :-- | :-- | :-- | :-- |
| 1 | Declaration with an initializer — `float3 c = A;` | the declared type | [`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228) |
| 2 | Assignment to a Graph variable — `c = A;` | the variable's declared type | `DSH4228` |
| 3 | Assignment to a name that matches an `Outputs` declaration — `Color = A;` | the output's declared type | `DSH4228` |
| 4 | `MaterialAttributes` member write — `Attrs.BaseColor = A;` | the attribute's own type | `DSH4228` |
| 5 | Function / `GraphFunction` / `ShaderFunction` / `VirtualFunction` input argument | the input's declared type | [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) |
| 6 | A `UE.*` / `Substrate.*` pin, the coordinates of a texture sample | the pin's type in the engine catalog | [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214) |
| 7 | An `if` condition | one true-or-false value | [`DSH4260`](../diagnostics/DSH4xxx.md#dsh4260) |
| 8 | An `Outputs` binding of a `Shader` — `Base.EmissiveColor = Color;` | the material attribute's type | `DSH4228`, and never a cut (see [Narrowing](#narrowing)) |
| 9 | Operands of `+ - * /` | the widest operand: a scalar spreads to it, nothing else changes width | `DSH4226` |

An assignment to a name that matches nothing declared, in any case, declares it with the value's own
type (legacy rule L26, info [`DSH5292`](../diagnostics/DSH5xxx.md#dsh5292)); there is nothing to
convert to.

An `if` that assigns a variable in both branches merges two values of the variable's declared type, so
the merge itself converts nothing *(since 2.0.0)*; see [`if` / `else`](if.md).

## Rule order

The rules are tested in this exact order; the first that matches decides the outcome.

| # | Value → place | Behaviour | Nodes |
| :-- | :-- | :-- | :-- |
| 1 | The same type | passes through **unchanged** | none |
| 2 | A number of the same width and another kind (`int` → `float`, `float` → `bool`, …) | the kind changes; the value in the graph does not | none |
| 3 | **Widening from a scalar** — a scalar into a wider number | broadcast: the scalar is wired where the value is read, and the material node replicates it *(since 2.0.0)* | none; one `ConstantNVector` when the scalar is constant |
| 4 | A narrower vector into a node's pin (2 into a `float3` pin, …) | taken as it is: what a node does with fewer components is the node's business | none |
| 5 | **Narrowing**, in a 1.x source — a wider vector into a narrower place | its leading components (`r`, `rg`, `rgb`), said by the info [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) *(since 2.0.0)* | a `ComponentMask`, or the output pin the source node has for exactly those channels; none at a node's pin |
| 6 | A node with several outputs into a number | its default output, when that output's type fits; in a 1.x source, a node whose first output the catalog gives no width is read as that output, and at a node's pin any node is (info [`DSH5287`](../diagnostics/DSH5xxx.md#dsh5287); *2.0.0 – 2.1.0 refused the second*); otherwise [`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201) | none |
| 7 | Anything else | rejected with the site's code | — |

Rule 7 covers widening a value that is **not** a scalar — input 2 into a 3 or a 4, input 3 into a 4,
anywhere but a node's pin — and every change of type family.

### Numeric conversion matrix

Input width down the side, expected width across the top, in a 1.x source. At a node's pin the two
errors in the matrix are accepted (rule 4), and a cut makes no node.

| input \ expected | 1 | 2 | 3 | 4 |
| :-- | :-- | :-- | :-- | :-- |
| **1** | unchanged | broadcast, no node | broadcast, no node | broadcast, no node |
| **2** | `.r`, `DSH5289` | unchanged | **error** | **error** |
| **3** | `.r`, `DSH5289` | `.rg`, `DSH5289` | unchanged | **error** |
| **4** | `.r`, `DSH5289` | `.rg`, `DSH5289` | `.rgb`, `DSH5289` | unchanged |

### Kind conversion matrix

"error" is the code of the site (see the first table).

| expected \ input | number | MaterialAttributes | Substrate | texture |
| :-- | :-- | :-- | :-- | :-- |
| **MaterialAttributes** | error | pass | error | error |
| **Substrate** | error | error | pass | error |
| **Texture(T)** | error | error | error | pass if the texture type is `T`; a pin that takes any texture takes any |
| **Number(N)** | numeric matrix above | error | error | error |

A texture converts only to its own texture type: `Texture2D`, `TextureCube`, `Texture2DArray`,
`Texture3D` and `VolumeTexture` are five types, and `SamplerState` is a type of its own, not a
texture *(since 2.0.0)*. A `TextureCube` never converts to a `Texture2D`.

## Narrowing

In a 1.x source a value wider than its place keeps its leading components (legacy rule L22). The cut
is no longer silent *(since 2.0.0)*: each one is the info `DSH5289`, so `dsc check` on the source
lists them, and `dsc migrate` writes the swizzle. It costs what the swizzle costs: a
`ComponentMask`, unless the source node has an output pin for exactly those leading channels — see
[Swizzle](swizzle.md).

```c
vec4  Src   = vec4(0.1, 0.2, 0.3, 0.4);
float3 Rgb  = Src;      // Src.rgb — DSH5289; channel A is dropped
float  R    = Src;      // Src.r   — DSH5289
```

Writing the swizzle explicitly (`float3 Rgb = Src.rgb;`) documents the intent, builds the same node
and makes the info go away.

Narrowing applies **only** at sites 1–7 above. It is deliberately **not** applied:

| Context | Behaviour instead |
| :-- | :-- |
| Binary operator operands | refused — `DSH4226` |
| Constructor arguments | refused — [`DSH4222`](../diagnostics/DSH4xxx.md#dsh4222) |
| Swizzle bounds | refused — [`DSH4230`](../diagnostics/DSH4xxx.md#dsh4230): a swizzle can never exceed the base width |
| An `Outputs` binding of a `Shader` (`Base.Opacity = Color4;`) | refused — `DSH4228`, the one place 1.x refused to cut too |
| A `.dss` source | refused with the site's code |

## Widening

The **only** widening rule is scalar broadcast. A 1-component value meets a wider place without a node:
the scalar itself is connected wherever the value is read, and the material node it reaches
replicates it *(since 2.0.0; 1.x built an `AppendVector` chain)*. A constant scalar is folded into a
`ConstantNVector`.

```c
float  K   = 0.5;
vec3   All = K;         // no node: K is connected wherever All is read
```

There is **no** zero-fill and **no** partial widening. `float3 v = SomeFloat2;` is `DSH4228`. Use a
constructor to say what the extra channels contain: `float3 v = float3(SomeFloat2, 0.0);`. The one
place a narrower vector is accepted is a node's pin (rule 4).

## Float, int and bool

The compiler knows the kind of every number *(since 2.0.0)*; the generated graph does not, because
every value in a material graph is a float. A change of kind therefore changes nothing in the graph.

| Token family | What it means for conversion |
| :-- | :-- |
| `float`, `float1..4`, `half`, `half1..4`, `vec2..4` | 1 / 2 / 3 / 4 components of kind `float` or `half` |
| `int`, `int2..4`, `ivec2..4` | the same 1 / 2 / 3 / 4 components of kind `int` — no truncation, no rounding |
| `uint`, `uint2..4`, `uvec2..4` | kind `uint` — no range clamping, no sign handling |
| `bool`, `bool2..4`, `bvec2..4` | kind `bool` — no normalisation to 0/1 |
| `StaticBool`, `StaticBoolParameter` | read as `bool`, 1 component |

Consequences:

- `int x = 7.9;` stores 7.9. Use `floor(…)` if truncation is wanted.
- `bool b = 0.5;` stores 0.5. There is no conversion to 0 or 1.
- Assigning a `float4` to an `int3` narrows exactly like `float4` → `float3` (`DSH5289`).
- The kind matters in one place: `/` between two integer
  [constructor](constructors.md#integer-constructors) calls is
  [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243). With an integer literal (`7`) or an `int` variable
  on either side it is a float division, as in 1.x. See
  [Integer division](expressions.md#integer-division).

## Authoritative component counts

*(since 2.0.0)* There are none. 1.x let some values — a constant-folded constructor, `dot`, the
`UE.*` builtins of a known-width table — carry an "authoritative" width that overrode a declaration
and steered operators. Every value now has the width of its type:

| Value | Width |
| :-- | :-- |
| A variable | the width it is declared with — always honoured |
| A literal | 1 |
| A constructor, folded or not | its type's |
| A builtin | its operands' widest, or what the builtin returns (`dot`, `length`, `distance`: 1) |
| A `UE.*` / `Substrate.*` node output | what the engine catalog gives that output; an output the engine gives no width follows the node's widest input, or, when no input is wider than a scalar, the place the value goes |
| A swizzle | its number of channels |

So `float2 dir = UE.CameraVectorWS();` is a `float2` — the leading two components, `DSH5289` — where
1.x stored three. Two operands of different widths meet only through the scalar broadcast; anything
else is `DSH4226` (site 9), and an operator never widens or narrows an operand on its own.

## Notes

- Conversion never turns a number into a `MaterialAttributes`, texture or `Substrate` value, or one
  texture type into another.
- Declaring a texture, `SamplerState` or `Substrate` variable without an initializer is
  [`DSH2215`](../diagnostics/DSH2xxx.md#dsh2215), because there is no default value to start from.
  Scalars and vectors start at zero. See [Declarations](declarations.md).
- An `Outputs` declaration is an ordinary variable of its declared type: each assignment to it is
  checked where it is written, not once after the graph is built.
- A `MaterialAttributes` value that reaches a pin or a function input that does not carry one is
  [`DSH4371`](../diagnostics/DSH4xxx.md#dsh4371), when the graph is built.

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH4228` | A value does not fit the variable, output or attribute it is stored in (sites 1–4 and 8). |
| `DSH4226` | An argument or an operand does not fit (sites 5 and 9). |
| `DSH5214` | A value does not fit a node's pin (site 6). |
| `DSH4260` | A condition is not one true-or-false value (site 7). |
| `DSH5289` | *(info)* A wider value is cut to its leading components (rule 5). |
| `DSH5287` | *(info)* A node with several outputs is read as its first (rule 6). |
| `DSH5201` | A node with several outputs is used as a value without naming one, and its default output does not fit. |
| `DSH5292` | *(info)* An assignment declares a name declared nowhere. |
| `DSH2215` | A texture, sampler or `Substrate` variable has no initializer. |
| `DSH4371` | A `MaterialAttributes` value reaches a pin or input that does not carry one. |

## Example

```c
Shader(Name="Docs/M_Conversions", Root="Game")
{
    Properties {
        VectorParameter Tint = float4(1.0, 2.0, 3.0, 999.0);
        ScalarParameter K    = 0.5;
    }

    Settings { Domain = "Surface"; ShadingModel = "DefaultLit"; BlendMode = "Opaque"; }

    Outputs {
        float3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        float3 dir    = UE.CameraVectorWS();  // a float3 output
        float3 tinted = Tint;                 // narrowing: Tint.rgb, info DSH5289
        float3 lit    = K;                    // scalar broadcast: no node
        Color = dir * tinted + lit;
    }
}
```

Replacing the last statement with `Color = dir * Tint;` fails with `DSH4226`: the `float4` parameter
makes the product four components wide, and an operator does not widen the `float3` operand to
match.

## See also

- [Expressions](expressions.md) — operator operand rules and the integer-division check
- [Constructors](constructors.md) — the explicit way to change a value's width
- [Swizzle](swizzle.md) — the explicit way to narrow, and the node a narrowing costs
- [Declarations](declarations.md) — declared type tokens, default values, redeclaration
- [`if` / `else`](if.md) — branch merging and the types it demands
- [MaterialAttributes](material-attributes.md) — per-attribute types
- [Calls](calls.md) — argument conversion against declared input types
- [Types](../language/types.md) — the complete type-token catalogue
- [`UE.*` builtins](../builtins/ue.md) — the output types of the builtins
- [`dsc migrate`](../tools/migrate.md) — rule L22 and the swizzle it writes
- [Diagnostics index](../diagnostics/index.md) — every code by pipeline stage
