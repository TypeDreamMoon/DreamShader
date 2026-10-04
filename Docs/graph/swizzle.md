# Swizzle

> [DreamShader](../index.md) » [Graph](index.md) » **Swizzle**

Postfix member access on a numeric value that selects, reorders or repeats its channels.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration default |
| Kind | expression, postfix |
| Generates | one `UMaterialExpressionComponentMask` for an ascending mask, or **nothing** when the source node has an output pin for exactly those channels; one mask per channel plus `UMaterialExpressionAppendVector` × (N−1) for a reordered or repeated mask |

## Synopsis

```c
<swizzle> := <expression> . <channels>
<channels> := <channel> [ <channel> ] [ <channel> ] [ <channel> ]
<channel>  := { x | y | z | w | r | g | b | a }
```

Between one and four channel characters. `.` and the channel letters are literal; `[ … ]` and
`{ a | b }` are meta-notation.

## Channel sets

| Characters | Channel index |
| :-- | :-- |
| `x`, `X`, `r`, `R` | 0 |
| `y`, `Y`, `g`, `G` | 1 |
| `z`, `Z`, `b`, `B` | 2 |
| `w`, `W`, `a`, `A` | 3 |

- Exactly **two** sets exist: `xyzw` and `rgba`. There is no `stpq` set and no `uv` set — neither `u`
  nor `v` resolves, so `.uv` fails with [`DSH4230`](../diagnostics/DSH4xxx.md#dsh4230).
- Channel characters are matched **case-insensitively**: `.RGB`, `.Rgb`, `.XyZ` are all valid.
- **Mixing the two sets in one swizzle is `DSH4230`** *(since 2.0.0; 1.x accepted `.xg` as `.xy`)*.

## Length and bounds

| Rule | Violation |
| :-- | :-- |
| One to four channel characters | 5 or more: `DSH4230` |
| Every character must be a channel letter | unknown letter: `DSH4230` |
| Every channel index must be **less than** the base value's component count | out of range: `DSH4230` |
| One set per swizzle | `.xg`: `DSH4230` |

A swizzle can only ever narrow or rearrange; it can never read past the base width.

| Base | Swizzle | Result |
| :-- | :-- | :-- |
| `float4` | `.rgb` | `float3` |
| `float3` | `.a` | error — channel 3 ≥ 3 components |
| `float2` | `.xyz` | error — channel 2 ≥ 2 components |
| `float` | `.x`, `.r` | the base value |
| `float` | `.xx`, `.rrr` | splat to `float2` / `float3` |
| `float` | `.y`, `.z`, `.w`, `.g`, `.b`, `.a` | **error** |

> [!WARNING]
> **A scalar does not splat through an out-of-range channel.** `Roughness.yz` on a scalar parameter is
> a hard error, not a two-component broadcast: `DSH4230`. To broadcast a scalar, repeat channel 0
> (`Roughness.xx`), use a constructor (`float2(Roughness)`), or rely on
> [scalar widening](conversions.md#widening) at an assignment.

## Lowering

*(since 2.0.0)* A swizzle is a node: the generator never puts a channel mask on a connection. Four
cases, tried in this order.

| # | Applies when | Nodes created | Result |
| :-- | :-- | :-- | :-- |
| 1 | The source node publishes exactly the selected channels as an output pin — `.rgb` or `.a` of a texture sample, `.rgb` or `.r` of a vector parameter | **none** | that pin is read |
| 2 | The channel indices are **strictly increasing with no repeats** | one `ComponentMask` | the selected channels |
| 3 | Base has 2–4 components and the mask is reordered or repeated | one single-channel mask per distinct channel (or its pin, by case 1), then N−1 `AppendVector` | the pieces concatenated |
| 4 | Base has 1 component and every channel is in range | N−1 `AppendVector` (none for a 1-character swizzle) | the scalar replicated N times |

An identity mask — every channel of the value, in order: `.x` on a `float`, `.rgb` on a `float3` — is
no node in a `.dss`. In a 1.x source it is kept as a `ComponentMask`, because 1.x masked the
connection whatever width the value was declared with.

### Sequential versus non-sequential masks

| Swizzle | Channels | Case | Node cost |
| :-- | :-- | :-- | :-- |
| `.r` / `.x` | 0 | 2 | 1 `ComponentMask` |
| `.a` | 3 | 2 | 1 `ComponentMask` |
| `.rg` / `.xy` | 0,1 | 2 | 1 `ComponentMask` |
| `.rgb` / `.xyz` | 0,1,2 | 2 | 1 `ComponentMask` |
| `.ga` | 1,3 | 2 — increasing, gaps are allowed | 1 `ComponentMask` |
| `.rgba` on a `float4` | 0,1,2,3 | identity | none in a `.dss`; 1 `ComponentMask` in a 1.x source |
| `.gr` | 1,0 | 3 | 2 `ComponentMask`, 1 `AppendVector` |
| `.bgr` | 2,1,0 | 3 *(since 1.3.3)* | 3 `ComponentMask`, 2 `AppendVector` |
| `.xxx` | 0,0,0 | 3 | 1 `ComponentMask`, 2 `AppendVector` |
| `.rrgg` | 0,0,1,1 | 3 | 2 `ComponentMask`, 3 `AppendVector` |
| `.rr` on a `float` | 0,0 | 4 | 1 `AppendVector` |

Case 1 replaces a `ComponentMask` in this table by a pin wherever the source node has one. Every
node is counted once however often the swizzle is written: equal nodes are merged (see
[Node reuse](node-reuse.md)).

### Composed swizzles

A swizzle of an already-swizzled value re-maps through the existing mask, so the channel numbering is
always relative to the value being swizzled, not to the original node.

| Written | Meaning |
| :-- | :-- |
| `v.rgb.b` | channel 2 of `v` — `.rgb` selects `{0,1,2}`, then `.b` picks entry 2 of that list |
| `v.ga.r` | channel 1 of `v` — `.ga` selects `{1,3}`, then `.r` picks entry 0 |
| `v.ga.g` | channel 3 of `v` |
| `v.bgr.r` | channel 2 of `v` |

A mask of a mask is folded into one mask while the result stays ascending, so `v.rgb.rg` is one
`ComponentMask` (or one pin) on `v`. Selecting an entry that does not exist in the outer list is an
ordinary bounds failure against the *swizzled* width — `v.ga.b` is `DSH4230` because `.ga` has 2
components, whatever the width of `v`.

### Swizzling a call result

`.` is an ordinary postfix operator, so any call result can be swizzled directly — no temporary
variable is required.

```c
float u  = UE.TexCoord().x;
vec3  c  = SampleTexture2D(Albedo, uv).rgb;
vec2  yx = vec4(1.0, 2.0, 3.0, 4.0).yx;
float m  = MyFunction(A, B).r;
```

The swizzle applies to the call's value. On a call of a `Function`, `GraphFunction`, `ShaderFunction`
or `VirtualFunction`, a member that names one of the function's outputs selects that output instead,
and so does **any** member when the function returns nothing (legacy rule L3b): `.r` on a function
that has only `out` parameters is read as an output name, and is
[`DSH5280`](../diagnostics/DSH5xxx.md#dsh5280) unless an output is called `r`. See
[Calls](calls.md#output-selection). On a `UE.*` node with several outputs, a member names an output;
a swizzle is allowed where the outputs are channel views of one value, as on a texture sample.

## Non-swizzlable bases

Member access on a non-numeric value is not a swizzle.

| Base value | Behaviour |
| :-- | :-- |
| `MaterialAttributes` | Not a swizzle. The member name is resolved as a material attribute and read. See [MaterialAttributes](material-attributes.md) |
| Texture object | [`DSH4206`](../diagnostics/DSH4xxx.md#dsh4206): a member of a texture is a sampling method, which has to be called |
| `Substrate` | [`DSH4207`](../diagnostics/DSH4xxx.md#dsh4207): the value has no such member |

## Notes

- A swizzle costs one node however often it is written: `Src.rgb` used ten times is one
  `ComponentMask` — or no node, when `Src` has an `RGB` pin.
- Narrowing in a 1.x source takes the leading channels (`r`, `rg`, `rgb`) with the same swizzle, so
  an implicit cut costs what the swizzle costs. See [Conversions](conversions.md#narrowing).
- A swizzle of constants is folded: `vec4(1.0, 2.0, 3.0, 4.0).yx` is one `Constant2Vector(2, 1)`.
- Repeated and reordered swizzles are merged like any other expression, so `Src.bgr` written twice
  yields one `AppendVector` chain. See [Node reuse](node-reuse.md).
- `.` followed by anything that is not an identifier is a parse error before any swizzle logic runs:
  [`DSH2161`](../diagnostics/DSH2xxx.md#dsh2161).

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH4230` | A character that is no channel letter, more than four characters, a channel at or past the base width, or the two sets mixed. |
| `DSH4206` | A member of a texture object. |
| `DSH4207` | A member of a value that has no members, such as a `Substrate` value. |
| [`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201) | A member of a multi-output `UE.*` node that names no output, or a swizzle across channels that sit on different outputs. |
| `DSH5280` | A member of a call of a function with only `out` parameters that names no output. |
| `DSH2161` | `.` not followed by an identifier. |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214), [`DSH8226`](../diagnostics/DSH8xxx.md#dsh8226) | The emitter could not create a `ComponentMask` or `AppendVector` node, or was handed a mask that is not one to four ascending channels (an internal guard). |

## Example

```c
Shader(Name="Docs/M_Swizzle")
{
    Properties { vec4 Src = vec4(0.1, 0.2, 0.3, 0.4); }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }

    Graph {
        vec3  Forward   = Src.rgb;   // leading channels -> the parameter's RGB pin
        vec3  Reordered = Src.bgr;   // reordered        -> a mask per channel, 2 AppendVector
        float One       = Src.a;     // ascending        -> 1 ComponentMask
        vec2  Repeated  = One.rr;    // scalar base      -> 1 AppendVector
        Color = Forward + Reordered * One + vec3(Repeated, 0.0);
    }
}
```

Generated nodes:

```text
VectorParameter                       -> Src (property node)
(no node)                             -> Src.rgb        [the RGB pin of Src]
ComponentMask B, ComponentMask G,
  AppendVector, AppendVector          -> Src.bgr        [r is the R pin of Src]
ComponentMask A                       -> Src.a
AppendVector                          -> One.rr
Multiply, Add, Constant(0.0),
  AppendVector, Add                   -> Base.EmissiveColor
```

## See also

- [Expressions](expressions.md) — postfix precedence and where `.` binds
- [Conversions](conversions.md) — implicit narrowing and widening
- [Constructors](constructors.md) — the explicit way to widen or repack
- [MaterialAttributes](material-attributes.md) — what `.` means on an attributes value
- [Calls](calls.md) — swizzling the result of a function or builtin call
- [Node reuse](node-reuse.md) — why a swizzle written ten times is one node
- [Unsupported constructs](unsupported.md) — why `v[0]` is refused
- [Graph layout](../generation/graph-layout.md) — where the generated nodes are placed
