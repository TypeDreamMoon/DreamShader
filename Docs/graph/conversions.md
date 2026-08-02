# Conversions

> [DreamShader](../index.md) » [Graph](index.md) » **Conversions**

The single coercion routine that adapts a `Graph` value to an expected shape, and the
authoritative-component-count rule that decides when a declared width is honoured.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — applies to every typed site inside a `Graph { … }` body and to `Outputs` bindings |
| Kind | implicit conversion rules |
| Generates | `UMaterialExpressionAppendVector` × (N−1) when widening a scalar; **nothing** when narrowing (a channel mask is used) or when the widths already match |

## Synopsis

Conversion is not written by the author; it is applied automatically wherever a value meets an
expected shape.

```text
coerce( <value>, <expected-shape> ) -> <value> | <error>

<expected-shape> := MaterialAttributes
                  | Substrate
                  | Texture( { Texture2D | TextureCube | Texture2DArray | VolumeTexture } )
                  | Numeric( { 1 | 2 | 3 | 4 } )
```

A value's own shape is one of the same four kinds, plus two markers that ride along with it: the
**authoritative component count** flag and the **integer** flag.

## Where conversion applies

| # | Site | Expected shape taken from |
| :-- | :-- | :-- |
| 1 | Declaration with an initializer — `float3 c = A;` | the declared type token, **unless the authoritative escape hatch fires** |
| 2 | Assignment to an existing Graph variable — `c = A;` | the existing value's shape (component count, texture flag, texture dimension, Substrate flag) |
| 3 | Assignment to a name that matches an `Outputs` declaration — `Color = A;` | the output's declared type |
| 4 | `MaterialAttributes` member write — `Attrs.BaseColor = A;` | the attribute's own component count |
| 5 | Function / `GraphFunction` / `ShaderFunction` / `VirtualFunction` input argument | the input's declared type |
| 6 | `if` branch merge — both branch values are coerced to the merged shape | the pre-branch value, else the `Outputs` declaration, else the two branches must already agree |
| 7 | Binary-operator rescue | the authoritative operand's count — **widening only** |
| 8 | `Outputs` binding expression | the bound output's declared type |

Assignment to a name that is neither an existing variable nor an `Outputs` declaration performs **no**
conversion at all: the new variable is created with the value's own shape.

## Rule order

The rules are tested in this exact order; the first that matches decides the outcome.

| # | Condition | Behaviour | Message on failure |
| :-- | :-- | :-- | :-- |
| 1 | Expected is `MaterialAttributes` | input must be a `MaterialAttributes` value; passes through unchanged | `Expected a MaterialAttributes value.` |
| 2 | Expected is `Substrate` | input must be a `Substrate` value; passes through unchanged | `Expected a Substrate value.` |
| 3 | Expected is a texture | input must be a texture object | `Expected a texture object value.` |
| 3a | Expected is a texture and input is a texture | the texture **dimension** must be identical; passes through unchanged | `Expected a texture object value with a matching texture type.` |
| 4 | Input is `MaterialAttributes`, expected is numeric | rejected | `MaterialAttributes values cannot be assigned to numeric outputs.` |
| 5 | Input is `Substrate`, expected is numeric | rejected | `Substrate values cannot be assigned to numeric outputs.` |
| 6 | Input is a texture object, expected is numeric | rejected | `Texture objects cannot be assigned to numeric outputs.` |
| 7 | Component counts are equal | passes through **unchanged**, every flag preserved | — |
| 8 | **Narrowing** — expected ≥ 1 and input count > expected | a leading sequential swizzle (`r`, `rg`, `rgb`) is applied; **silent**, no node created | — |
| 9 | **Widening from a scalar** — expected > 1 and input count = 1 | the value is replicated `expected` times through `AppendVector` | — |
| 10 | Anything else | rejected | `Expected {Expected} component(s) but got {Actual}.` |

Rule 10 is reachable only for widening a value that is **not** a scalar: input 2 → expected 3 or 4,
and input 3 → expected 4.

### Numeric conversion matrix

Input width down the side, expected width across the top.

| input \ expected | 1 | 2 | 3 | 4 |
| :-- | :-- | :-- | :-- | :-- |
| **1** | unchanged | splat, 1 `AppendVector` | splat, 2 `AppendVector` | splat, 3 `AppendVector` |
| **2** | `.r`, no node | unchanged | **error** | **error** |
| **3** | `.r`, no node | `.rg`, no node | unchanged | **error** |
| **4** | `.r`, no node | `.rg`, no node | `.rgb`, no node | unchanged |

### Kind conversion matrix

| expected \ input | numeric | MaterialAttributes | Substrate | texture object |
| :-- | :-- | :-- | :-- | :-- |
| **MaterialAttributes** | `Expected a MaterialAttributes value.` | pass | `Expected a MaterialAttributes value.` | `Expected a MaterialAttributes value.` |
| **Substrate** | `Expected a Substrate value.` | `Expected a Substrate value.` | pass | `Expected a Substrate value.` |
| **Texture(T)** | `Expected a texture object value.` | `Expected a texture object value.` | `Expected a texture object value.` | pass if the dimension is `T`, else `Expected a texture object value with a matching texture type.` |
| **Numeric(N)** | numeric matrix above | `MaterialAttributes values cannot be assigned to numeric outputs.` | `Substrate values cannot be assigned to numeric outputs.` | `Texture objects cannot be assigned to numeric outputs.` |

The four texture dimensions are distinct expected shapes. `Texture2D` and `SamplerState` both mean
`Texture2D`; `Texture3D` and `VolumeTexture` both mean `VolumeTexture`; `TextureCube` and
`Texture2DArray` stand alone. A `TextureCube` never converts to a `Texture2D`.

## Narrowing

Narrowing is **silent** — there is no warning and no node cost, because it is implemented as a leading
sequential [swizzle](swizzle.md), which lives on the connection rather than in the graph.

```c
vec4  Src   = vec4(0.1, 0.2, 0.3, 0.4);
float3 Rgb  = Src;      // silently becomes Src.rgb — channel A is dropped
float  R    = Src;      // silently becomes Src.r
```

> [!WARNING]
> An over-wide initializer or an over-wide value assigned to a narrower output is not diagnosed. If a
> channel disappears from a generated material, check every assignment whose right-hand side is wider
> than its target. Writing the swizzle explicitly (`float3 Rgb = Src.rgb;`) documents the intent and
> behaves identically.

Narrowing applies **only** at the conversion sites listed above. It is deliberately **not** applied:

| Context | Behaviour instead |
| :-- | :-- |
| Binary operator operands | refused — the size-mismatch error fires (see below) |
| Constructor arguments | refused — `Constructor '{Name}' expects {N} total components but got {Total}.` |
| Swizzle bounds | refused — a swizzle can never exceed the base width |

## Widening

The **only** widening rule is scalar splat. A 1-component value is replicated to fill the expected
width through `AppendVector` nodes.

```c
float  K   = 0.5;
vec3   All = K;         // becomes AppendVector(AppendVector(K, K), K)
```

There is **no** zero-fill and **no** partial widening. `float3 v = SomeFloat2;` is
`Expected 3 component(s) but got 2.` Use a constructor to say what the extra channels contain:
`float3 v = float3(SomeFloat2, 0.0);`

## Float, int and bool

There are no numeric-representation conversions in the graph language at all.

| Token family | What it means for conversion |
| :-- | :-- |
| `float`, `float1..4`, `half`, `half1..4`, `vec2..4` | 1 / 2 / 3 / 4 components |
| `int`, `int2..4`, `ivec2..4` | the same 1 / 2 / 3 / 4 components — no truncation, no rounding |
| `uint`, `uint2..4`, `uvec2..4` | the same — no range clamping, no sign handling |
| `bool`, `bool2..4`, `bvec2..4` | the same — no normalisation to 0/1 |
| `StaticBool`, `StaticBoolParameter` | 1 component; carries no marker of its own, so every rule on this page treats it as a scalar |

Consequences:

- `int x = 7.9;` stores 7.9. Use `floor(…)` if truncation is wanted.
- `bool b = 0.5;` stores 0.5. There is no conversion to 0 or 1.
- Assigning a `float4` to an `int3` narrows exactly like `float4` → `float3`.
- The **only** observable difference between an integer and a float value is the integer marker set by
  an integer [constructor](constructors.md#integer-constructors) call, and its only effect is to
  reject `/` when both operands carry it. See
  [Integer division](expressions.md#integer-division).

## Authoritative component counts

A value carries an **authoritative component count** when its width is known from the engine rather
than inferred from a declaration. The flag changes two things: whether an operator will rescue a size
mismatch, and whether a declared width is honoured at all.

### Which values are authoritative

| Source | Authoritative |
| :-- | :-- |
| A constant-folded constructor — `vec3(0.5)`, `float4(1,0,0,1)` | **yes** — the only path where a constructor originates the flag |
| A `UE.*` builtin whose node class is in the known-width table below | **yes** |
| The `dot` math builtin | **yes**, 1 component |
| Any other math builtin — `lerp`/`mix`, `min`, `max`, `pow`, `clamp`, `fmod`/`mod`, and the unary set | inherits: `lerp`/`min`/`max` take the logical OR of their two value operands, the rest inherit from their first operand |
| A bare numeric literal, e.g. `0.5` | no |
| A declared `Properties` parameter of any width | no |
| A non-folded constructor — `vec3(K)`, `float4(rgb, A)` | inherits: the logical OR of its arguments' flags |
| A swizzle | inherits from the base value |
| The result of `+ - * /` | inherits: the logical OR of the two operands' flags |
| A narrowed or widened value produced by rule 8 or rule 9 | inherits from the input value |
| A variable | whatever the value assigned to it carried |

### Known-width builtin nodes

Matched by expression class; these are the node classes the generator can assign a trustworthy output
width to. Names are the `UMaterialExpression` class names behind the corresponding
[`UE.*` builtins](../builtins/ue.md).

| Components | Node classes |
| :-- | :-- |
| 1 | `MaterialExpressionPixelDepth`, `MaterialExpressionTwoSidedSign`, `MaterialExpressionArctangent2Fast`, `MaterialExpressionLength`, `MaterialExpressionMaterialXLuminance` |
| 2 | `TextureCoordinate`, `Panner`, `ScreenPosition`, `Rotator`, `MaterialExpressionSceneTexelSize` |
| 3 | `WorldPosition`, `ObjectPositionWS`, `CameraVectorWS`, `VertexNormalWS`, `VertexTangentWS`, `Transform`, `TransformPosition`, `MaterialExpressionSkyAtmosphereLightDirection`, `MaterialExpressionPixelNormalWS`, `MaterialExpressionCrossProduct` |

Every other node's output width is unknown to the generator, so values produced from it are
non-authoritative.

### Effect 1 — the operator rescue

When `+ - * /` receives two operands whose widths are incompatible (unequal, and neither is a scalar),
one rescue is attempted before the error:

| Requirement | Detail |
| :-- | :-- |
| Exactly one operand is authoritative | if both or neither are, no rescue |
| The authoritative width is greater than zero | — |
| The other operand's width is **≤** the authoritative width | strictly enforced |
| The other operand is then widened to the authoritative width | rule 9 only, i.e. only from a scalar |

The rescue **never narrows**. Narrowing at an operator would silently drop channels, so the
size-mismatch error is raised instead:

```text
Operator '{Op}' requires matching vector sizes or a scalar/vector pair, got {Left} and {Right} component(s).
```

This is why `UE.CameraVectorWS() * Tint` fails when `Tint` is a `VectorParameter`: the builtin is an
authoritative 3, the parameter is a non-authoritative 4, and 4 > 3 blocks the rescue.

### Effect 2 — a declared width can be ignored

> [!WARNING]
> When a declaration's initializer carries an authoritative component count that **differs** from the
> declared type's width, the value is stored **as-is, uncoerced, with no diagnostic**. The declared
> width is effectively ignored.

The escape hatch fires when **all** of these hold:

| # | Condition |
| :-- | :-- |
| 1 | The statement is a declaration with an initializer |
| 2 | The initializer value has an authoritative component count |
| 3 | The initializer value is plain numeric — not a texture, not `MaterialAttributes`, not `Substrate` |
| 4 | The declared type is plain numeric — not a texture, not `Substrate`, and its component count is greater than 0 |
| 5 | The two component counts differ |

```c
float2 dir = UE.CameraVectorWS();   // stored as a 3-component value; float2 is ignored, no error
float3 ok  = UE.CameraVectorWS();   // widths agree, normal path
```

The mismatch is not lost — it resurfaces at the first place the value is used with a width that
matters (an operator, an output binding, a function argument), where the message names the real widths
rather than the declared one. The rationale is that silently truncating an engine-known width is worse
than reporting the problem one step later.

The escape hatch applies to **declarations only**. Assignment to an existing variable (site 2) and
assignment to an output name (site 3) always coerce, so both will narrow silently even when the value
is authoritative.

### Effect 3 — branch merging

The authoritative flag and the integer flag are **not** compared when deciding whether an `if` branch
changed a value. Two values that differ only in those flags are treated as identical and the name is
not merged. See [`if` / `else`](if.md).

## Notes

- Conversion never changes the *kind* of a value. There is no path from numeric to
  `MaterialAttributes`, texture or `Substrate`, or between texture dimensions.
- A conversion that passes through (rules 1, 2, 3a, 7) preserves every flag on the value, including
  the integer marker and any pending channel mask.
- `Substrate` shapes require UE 5.4 or newer; on an older engine the type token does not resolve at
  all and the declaration fails before conversion with
  `Graph variable '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.`
- Declaring a texture or `Substrate` variable without an initializer is rejected —
  `Graph variable type '{Type}' requires an explicit initializer.` — because there is no default value
  to convert. Scalars and vectors default to zero. See [Declarations](declarations.md).
- The `Outputs` declaration is re-checked after the whole graph is built; a final mismatch reports
  `{Shader}: Graph output '{Name}' does not match its declared type.`

## Diagnostics

Format specifiers are rendered as `{Placeholder}` throughout this page; the compiler emits the
substituted text. The coercion routine produces the short messages in the first group; the calling
site prefixes them with the second group's wrapper.

| Message | Cause |
| :-- | :-- |
| `Expected {Expected} component(s) but got {Actual}.` | Widening a 2- or 3-component value to a wider target. |
| `Expected a MaterialAttributes value.` | A numeric, texture or `Substrate` value where `MaterialAttributes` was expected. |
| `Expected a Substrate value.` | Anything other than a `Substrate` value where `Substrate` was expected. |
| `Expected a texture object value.` | A non-texture value where a texture was expected. |
| `Expected a texture object value with a matching texture type.` | A texture of the wrong dimension — for example a `TextureCube` where `Texture2D` was expected. |
| `MaterialAttributes values cannot be assigned to numeric outputs.` | A `MaterialAttributes` value assigned to a scalar or vector target. |
| `Substrate values cannot be assigned to numeric outputs.` | A `Substrate` value assigned to a scalar or vector target. |
| `Texture objects cannot be assigned to numeric outputs.` | A texture object assigned to a scalar or vector target. |

| Wrapper | Site |
| :-- | :-- |
| `Graph variable '{Name}' is declared as '{Type}' but assigned an incompatible value. {Detail}` | Declaration with an initializer (site 1). |
| `Graph variable '{Name}' was previously assigned an incompatible value. {Detail}` | Assignment to an existing variable (site 2). |
| `Graph output variable '{Name}' was assigned an incompatible value. {Detail}` | Assignment to an `Outputs` name (site 3). |
| `MaterialAttributes member '{Member}' expects {Count} component(s). {Detail}` | Attribute member write (site 4). |
| `Graph if branches assign incompatible values to '{Name}'. {Detail}` | Branch merge (site 6). |
| `In output expression '{Text}': {Detail}` | `Outputs` binding expression (site 8). |
| `Operator '{Op}' requires matching vector sizes or a scalar/vector pair, got {Left} and {Right} component(s).` | Operator size mismatch after the rescue attempt (site 7). |

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
        float3 dir   = UE.CameraVectorWS();  // authoritative 3
        float3 tinted = Tint;                // silent narrowing: Tint.rgb
        float3 lit    = K;                   // scalar splat: 2 AppendVector nodes
        Color = dir * tinted + lit;
    }
}
```

Replacing the third statement with `Color = dir * Tint;` fails, because the non-authoritative
4-component parameter cannot be narrowed to meet the authoritative 3-component builtin:

```text
Operator '*' requires matching vector sizes or a scalar/vector pair, got 3 and 4 component(s).
```

## See also

- [Expressions](expressions.md) — operator operand rules and the integer-division check
- [Constructors](constructors.md) — the explicit way to change a value's width
- [Swizzle](swizzle.md) — the explicit way to narrow, and the mechanism narrowing uses
- [Declarations](declarations.md) — declared type tokens, default values, redeclaration
- [`if` / `else`](if.md) — branch merging and the shapes it demands
- [MaterialAttributes](material-attributes.md) — per-attribute component counts
- [Calls](calls.md) — argument coercion against declared input types
- [Types](../language/types.md) — the complete type-token catalogue
- [`UE.*` builtins](../builtins/ue.md) — which builtins have a known output width
- [Diagnostics index](../diagnostics/index.md) — every message by pipeline stage
