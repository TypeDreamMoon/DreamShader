# Swizzle

> [DreamShader](../index.md) » [Graph](index.md) » **Swizzle**

Postfix member access on a numeric value that selects, reorders or repeats its channels.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration default |
| Kind | expression, postfix |
| Generates | **nothing** for a sequential mask (the selection becomes a channel mask on the connection); `UMaterialExpressionAppendVector` × (N−1) for a reordered or repeated mask |

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
  nor `v` resolves, so `.uv` fails with
  `Swizzle 'uv' is invalid for a value with {Count} components.`
- Channel characters are matched **case-insensitively**: `.RGB`, `.Rgb`, `.XyZ` are all valid.
- **Mixing the two sets in one swizzle is accepted**, not diagnosed. `.xg` resolves to channels 0 and
  1, exactly like `.xy` or `.rg`.

## Length and bounds

| Rule | Violation |
| :-- | :-- |
| One to four channel characters | 5 or more: `Unsupported swizzle '{Channels}'.` |
| Every character must resolve to a channel index | unknown letter: `Swizzle '{Channels}' is invalid for a value with {Count} components.` |
| Every channel index must be **less than** the base value's component count | out of range: `Swizzle '{Channels}' is invalid for a value with {Count} components.` |

A swizzle can only ever narrow or rearrange; it can never read past the base width.

| Base | Swizzle | Result |
| :-- | :-- | :-- |
| `float4` | `.rgb` | `float3` |
| `float3` | `.a` | error — channel 3 ≥ 3 components |
| `float2` | `.xyz` | error — channel 2 ≥ 2 components |
| `float` | `.x`, `.r` | the base value, unchanged |
| `float` | `.xx`, `.rrr` | splat to `float2` / `float3` |
| `float` | `.y`, `.z`, `.w`, `.g`, `.b`, `.a` | **error** |

> [!WARNING]
> **A scalar does not splat through an out-of-range channel.** `Roughness.yz` on a scalar parameter is
> a hard error, not a two-component broadcast:
> `Swizzle 'yz' is invalid for a value with 1 components.` To broadcast a scalar, repeat channel 0
> (`Roughness.xx`), use a constructor (`float2(Roughness)`), or rely on
> [scalar widening](conversions.md#widening) at an assignment.

## Lowering

Three strategies, tried in this order.

| # | Applies when | Nodes created | Result |
| :-- | :-- | :-- | :-- |
| 1 | The base has an expression node **and** the channel indices are **strictly increasing with no repeats** (measured against any mask already on the base) | **none** | The selection is recorded as an `FExpressionInput` channel mask and applied when the value is connected to a pin |
| 2 | Base has 2–4 components and the mask is reordered or repeated | N−1 `AppendVector` | Each channel becomes its own single-channel masked value (still node-free), then the pieces are concatenated |
| 3 | Base has 1 component and every channel is in range | N−1 `AppendVector` (none for a 1-character swizzle) | The scalar is replicated N times |

### Sequential versus non-sequential masks

| Swizzle | Channels | Strategy | Node cost |
| :-- | :-- | :-- | :-- |
| `.r` / `.x` | 0 | 1 | 0 |
| `.a` | 3 | 1 | 0 |
| `.rg` / `.xy` | 0,1 | 1 | 0 |
| `.rgb` / `.xyz` | 0,1,2 | 1 | 0 |
| `.ga` | 1,3 | 1 — increasing, gaps are allowed | 0 |
| `.xg` | 0,1 | 1 — mixed sets are still increasing | 0 |
| `.rgba` | 0,1,2,3 | 1 | 0 |
| `.gr` | 1,0 | 2 | 1 `AppendVector` |
| `.bgr` | 2,1,0 | 2 *(since 1.3.3)* | 2 `AppendVector` |
| `.xxx` | 0,0,0 | 2 or 3 | 2 `AppendVector` |
| `.rrgg` | 0,0,1,1 | 2 | 3 `AppendVector` |
| `.rr` on a `float` | 0,0 | 3 | 1 `AppendVector` |

"Strictly increasing" is judged on the **source** channel of the underlying node, not on the letters
written. See composed swizzles below.

### Composed swizzles

A swizzle of an already-masked value re-maps through the existing mask, so the channel numbering is
always relative to the value being swizzled, not to the original node.

| Written | Meaning |
| :-- | :-- |
| `v.rgb.b` | channel 2 of `v` — `.rgb` selects `{0,1,2}`, then `.b` picks entry 2 of that list |
| `v.ga.r` | channel 1 of `v` — `.ga` selects `{1,3}`, then `.r` picks entry 0 |
| `v.ga.g` | channel 3 of `v` |
| `v.bgr.r` | channel 2 of `v` |

Composition is folded into a single mask wherever the result is still sequential, so `v.rgb.rg` costs
no nodes at all. Selecting an entry that does not exist in the outer list is an ordinary bounds
failure against the *masked* width — `v.ga.b` reports
`Swizzle 'b' is invalid for a value with 2 components.`, not the width of `v`.

### Swizzling a call result

`.` is an ordinary postfix operator, so any call result can be swizzled directly — no temporary
variable is required.

```c
float u  = UE.TexCoord().x;
vec3  c  = SampleTexture2D(Albedo, uv).rgb;
vec2  yx = vec4(1.0, 2.0, 3.0, 4.0).yx;
float m  = MyFunction(A, B).r;
```

The swizzle applies to the call's **selected output value**. For a multi-output function call that is
used as a value, that is the single output the call form resolves to — see [Calls](calls.md).

## Non-swizzlable bases

Member access on a non-numeric value is not a swizzle.

| Base value | Behaviour |
| :-- | :-- |
| `MaterialAttributes` | Not a swizzle. The member name is resolved as a material attribute and a `BreakMaterialAttributes` read is generated. See [MaterialAttributes](material-attributes.md) |
| Texture object | error: `Texture values do not support swizzle/member access in Code.` |
| `Substrate` | error: `Substrate values do not support swizzle/member access in Graph.` |

> [!NOTE]
> The texture message says **"in Code"** while the Substrate message says **"in Graph"**. Both come
> from the same graph builder; the wording predates the `Code` → `Graph` section rename.

## Notes

- A sequential swizzle is free. `Src.rgb` used ten times costs zero nodes, because the mask lives on
  each consuming connection rather than on a node.
- Narrowing coercion is implemented with a sequential swizzle (`r`, `rg`, `rgb`), so an implicit
  truncation at an assignment also costs nothing. See [Conversions](conversions.md#narrowing).
- The authoritative-component-count flag is **inherited** through a swizzle. `UE.CameraVectorWS().rg`
  is an authoritative 2-component value and will therefore refuse to pair with a mismatched
  non-authoritative operand. See [Conversions](conversions.md#authoritative-component-counts).
- A swizzle result carries the base value's texture / attribute / Substrate flags, which is why those
  bases are rejected up front rather than producing a malformed value.
- Repeated and reordered swizzles are deduplicated like any other expression, so `Src.bgr` written
  twice yields one `AppendVector` chain. See [Node reuse](node-reuse.md).
- `.` followed by anything that is not an identifier is a parse error before any swizzle logic runs:
  `Expected member name after '.'.`

## Diagnostics

Format specifiers are rendered as `{Placeholder}` throughout this page; the compiler emits the
substituted text.

| Message | Cause |
| :-- | :-- |
| `Unsupported swizzle '{Channels}'.` | More than four channel characters (an empty swizzle is unreachable — `.` requires an identifier). |
| `Swizzle '{Channels}' is invalid for a value with {Count} components.` | A channel character does not resolve, or its index is greater than or equal to the base width. Covers both the scalar and the vector base. |
| `Channel {Index} is invalid for a value with {Count} components.` | Internal guard in the single-channel path: a masked value whose component count disagrees with its own mask. Unreachable through the swizzle grammar, which bounds-checks first. |
| `Failed to compose swizzle channel mask.` | The channel mask could not be applied to the value. |
| `Texture values do not support swizzle/member access in Code.` | `.` applied to a texture object. |
| `Substrate values do not support swizzle/member access in Graph.` | `.` applied to a `Substrate` value. |
| `Expected member name after '.'.` | `.` not followed by an identifier. |
| `Cannot build an empty vector.` | Internal guard from the append helper the reorder path uses. |
| `AppendVector cannot build {Count} components; Unreal material vectors support at most 4.` | Internal guard from the append helper; unreachable from a swizzle, which caps at four channels. |
| `Failed to create an AppendVector node.` | A node in the reorder/repeat chain could not be created. |

## Example

```c
Shader(Name="Docs/M_Swizzle")
{
    Properties { vec4 Src = vec4(0.1, 0.2, 0.3, 0.4); }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }

    Graph {
        vec3  Forward   = Src.rgb;   // sequential  -> channel mask, no node
        vec3  Reordered = Src.bgr;   // reordered   -> 2 AppendVector nodes
        float One       = Src.a;     // sequential  -> channel mask, no node
        vec2  Repeated  = One.rr;    // scalar base -> 1 AppendVector node
        Color = Forward + Reordered * One + vec3(Repeated, 0.0);
    }
}
```

Generated nodes:

```text
VectorParameter                   -> Src (property node)
(no node)                         -> Src.rgb        [mask RGB on each consumer pin]
AppendVector, AppendVector        -> Src.bgr
(no node)                         -> Src.a          [mask A on each consumer pin]
AppendVector                      -> One.rr
Multiply, Add, AppendVector, Add  -> Base.EmissiveColor
```

## See also

- [Expressions](expressions.md) — postfix precedence and where `.` binds
- [Conversions](conversions.md) — implicit narrowing, widening and authoritative widths
- [Constructors](constructors.md) — the explicit way to widen or repack
- [MaterialAttributes](material-attributes.md) — what `.` means on an attributes value
- [Calls](calls.md) — swizzling the result of a function or builtin call
- [Node reuse](node-reuse.md) — why sequential swizzles are free
- [Unsupported constructs](unsupported.md) — why `v[0]` silently becomes `v`
- [Graph layout](../generation/graph-layout.md) — where the generated `AppendVector` nodes are placed
