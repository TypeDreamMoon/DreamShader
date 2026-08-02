# Constructors

> [DreamShader](../index.md) » [Graph](index.md) » **Constructors**

Call-syntax expressions named after a scalar or vector type that build a value of that width from
their arguments.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration default |
| Kind | expression, call form |
| Generates | `UMaterialExpressionAppendVector` (N−1 nodes), or one `UMaterialExpressionConstant2Vector` / `Constant3Vector` / `Constant4Vector` when the call is constant-folded; nothing at all in the pass-through cases |

## Synopsis

```c
<constructor-call> := <constructor-name> ( <expression> [ , <expression> ] … )
```

Named arguments are rejected. A [brace initializer](declarations.md#brace-initializers) is exactly a
constructor call: `vec3 rgb = {r, g, b};` is re-serialised as `vec3(r, g, b)` and obeys every rule on
this page.

## Constructor names

**34 names**, all matched **case-insensitively** (`FLOAT3(…)` works). Component count is decided
purely by the **last character** of the name: `2` → 2, `3` → 3, `4` → 4, anything else → 1.

| Name | Components | Integer-marked | Family |
| :-- | :-- | :-- | :-- |
| `float` | 1 | no | HLSL float |
| `float1` | 1 | no | HLSL float |
| `float2` | 2 | no | HLSL float |
| `float3` | 3 | no | HLSL float |
| `float4` | 4 | no | HLSL float |
| `half` | 1 | no | HLSL half — identical behaviour to `float` |
| `half1` | 1 | no | HLSL half |
| `half2` | 2 | no | HLSL half |
| `half3` | 3 | no | HLSL half |
| `half4` | 4 | no | HLSL half |
| `vec2` | 2 | no | GLSL float vector |
| `vec3` | 3 | no | GLSL float vector |
| `vec4` | 4 | no | GLSL float vector |
| `int` | 1 | **yes** | HLSL signed integer |
| `int2` | 2 | **yes** | HLSL signed integer |
| `int3` | 3 | **yes** | HLSL signed integer |
| `int4` | 4 | **yes** | HLSL signed integer |
| `ivec2` | 2 | **yes** | GLSL signed integer vector |
| `ivec3` | 3 | **yes** | GLSL signed integer vector |
| `ivec4` | 4 | **yes** | GLSL signed integer vector |
| `uint` | 1 | **yes** | HLSL unsigned integer |
| `uint2` | 2 | **yes** | HLSL unsigned integer |
| `uint3` | 3 | **yes** | HLSL unsigned integer |
| `uint4` | 4 | **yes** | HLSL unsigned integer |
| `uvec2` | 2 | **yes** | GLSL unsigned integer vector |
| `uvec3` | 3 | **yes** | GLSL unsigned integer vector |
| `uvec4` | 4 | **yes** | GLSL unsigned integer vector |
| `bool` | 1 | no | HLSL boolean |
| `bool2` | 2 | no | HLSL boolean |
| `bool3` | 3 | no | HLSL boolean |
| `bool4` | 4 | no | HLSL boolean |
| `bvec2` | 2 | no | GLSL boolean vector |
| `bvec3` | 3 | no | GLSL boolean vector |
| `bvec4` | 4 | no | GLSL boolean vector |

### Names that do not exist

| Absent | Reason |
| :-- | :-- |
| `vec1`, `ivec1`, `uvec1`, `bvec1` | The GLSL families start at 2; only the HLSL families have a `1` spelling, and only `float1` / `half1` |
| `int1`, `uint1`, `bool1` | Not in the name table — use `int`, `uint`, `bool`. Only `float1` and `half1` have a `1` spelling |
| `hvec2`, `hvec3`, `hvec4` | No GLSL half family |
| `double`, `double2..4`, `dvec2..4` | No double-precision family |
| `float2x2`, `float3x3`, `float4x4`, `mat2`, `mat3`, `mat4` | **There are no matrix types anywhere in the graph language.** Matrix-like operations are reachable only through [`UE.TransformVector` / `UE.TransformPosition`](../builtins/transform.md) |
| `Scalar`, `Vector`, `Color` | Removed type aliases; see [Types](../language/types.md) |

Every name above lexes as an ordinary identifier and, because it is not a constructor, falls through
the call-resolution order to the user-function lookup — the message is
`Unknown Graph function '{Name}'.`

> [!WARNING]
> **Constructor names shadow everything.** The constructor test is the **first** step of call
> resolution, before `UE.*` builtins, `Substrate.*` builtins, math builtins, parameters, and all user
> definitions. A `Function`, `GraphFunction`, `ShaderFunction` or `VirtualFunction` named `float3`,
> `int`, `bool4`, `vec2` (or any other name in the table) can be declared without a diagnostic but can
> never be called — every call site builds a constructor instead. See
> [Name resolution](name-resolution.md).

### Integer constructors

The 14 integer-marked names are `int`, `int2`, `int3`, `int4`, `ivec2`, `ivec3`, `ivec4`, `uint`,
`uint2`, `uint3`, `uint4`, `uvec2`, `uvec3`, `uvec4`.

The marker they set does exactly one thing: `/` refuses to build a `Divide` node when **both**
operands carry it. It performs no truncation, no rounding, and no range clamping — an integer
constructor produces the same float-valued graph as its `float` counterpart. See
[Integer division](expressions.md#integer-division).

The marker is assigned from the **callee name on every constructor call**, so wrapping in a
non-integer constructor clears it: `float(int(7))` is not integer-marked.

## Argument rules

Argument expressions are evaluated left to right and screened before any arity check.

| # | Condition | Behaviour |
| :-- | :-- | :-- |
| 1 | Any argument is named (`float3(x = 1.0)`) | error: `Constructor '{Name}' does not accept named arguments.` |
| 2 | Any argument is a texture object | error: `Constructor '{Name}' cannot use Texture2D arguments.` |
| 3 | Any argument is a `MaterialAttributes` value | error: `Constructor '{Name}' cannot use MaterialAttributes arguments.` |
| 4 | Any argument is a `Substrate` value | error: `Constructor '{Name}' cannot use Substrate arguments.` |

Then, with `N` the constructor's component count:

| # | Case | Behaviour | Nodes created |
| :-- | :-- | :-- | :-- |
| 5 | `N == 1`, exactly one argument, and that argument has exactly 1 component | the argument is returned **unchanged**, then the integer marker is set from the name | none |
| 6 | `N == 1`, anything else | error: `Constructor '{Name}' expects a single scalar input.` | — |
| 7 | `N > 1`, exactly one argument, and that argument is a scalar | **splat**: the value is replicated `N` times | N−1 `AppendVector` |
| 8 | `N > 1`, exactly one argument that already has `N` components | returned unchanged | none |
| 9 | `N > 1`, otherwise | the arguments' component counts must sum to **exactly** `N` | N−1 `AppendVector` |
| 10 | `N > 1`, sum ≠ `N` | error: `Constructor '{Name}' expects {N} total components but got {Total}.` | — |

### Component packing

Rule 9 is a straight left-to-right concatenation of channels — arguments of mixed widths are legal as
long as the total is exact.

| Call | Total | Valid | Result |
| :-- | :-- | :-- | :-- |
| `float4(rgb, 1.0)` | 3 + 1 | yes | `float4` |
| `float4(uv, uv)` | 2 + 2 | yes | `float4` |
| `float3(x, yz)` | 1 + 2 | yes | `float3` |
| `float4(x, y, z, w)` | 1×4 | yes | `float4` |
| `vec3(0.5)` | scalar splat (rule 7) | yes | `(0.5, 0.5, 0.5)` |
| `float3(rgba)` | 4 | **no** | `Constructor 'float3' expects 3 total components but got 4.` |
| `float3(uv)` | 2 | **no** | `Constructor 'float3' expects 3 total components but got 2.` |
| `float(rgb)` | 3 | **no** | `Constructor 'float' expects a single scalar input.` |
| `float3()` | 0 | **no** | `Constructor 'float3' expects 3 total components but got 0.` |

> [!NOTE]
> **A constructor never narrows.** `float3(rgba)` is an error, not a truncation; write `rgba.rgb`.
> Narrowing happens only at coercion sites — see [Conversions](conversions.md#narrowing).
> Widening happens only from a scalar (rule 7); `float3(uv)` will not zero-fill.

## Constant folding

When all of the following hold, the whole call collapses to a single constant-vector node instead of
`N` `Constant` nodes plus `N−1` `AppendVector` nodes:

- `N >= 2`;
- the constructor name is **not** an integer constructor;
- every argument is unnamed **and** is a numeric literal, optionally with a leading unary `+` or `-`.

| Written | Folded to |
| :-- | :-- |
| `vec2(0.5, 1.0)` | `Constant2Vector(0.5, 1.0)` |
| `vec3(0.5)` | `Constant3Vector(0.5, 0.5, 0.5)` — a single literal is replicated across all channels first |
| `float4(1.0, 2.0, 3.0, -1.0)` | `Constant4Vector(1, 2, 3, -1)` |
| `int3(1, 2, 3)` | **not folded** — integer constructors are excluded so their marker is preserved |
| `vec3(K, 0.0, 0.0)` | not folded — `K` is not a literal |

The folded node is the **only** constructor result that carries an authoritative component count,
which changes how it behaves as an operand of a size-mismatched operator and as a declaration
initializer. See [Conversions](conversions.md#authoritative-component-counts).

Folded vectors are deduplicated by value: `vec3(0.5)` written five times yields one
`Constant3Vector`. See [Node reuse](node-reuse.md).

## Notes

- Rules 5 and 8 create **no node**. `float3(SomeFloat3)` is a no-op wrapper; its only observable
  effect is clearing the integer marker.
- A constructor call is an ordinary postfix expression and can be swizzled: `vec4(uv, 0, 1).xy`.
- A brace initializer with an empty body (`float3 v = {};`) does not reach a constructor at all — it
  produces the type's default value. See [Declarations](declarations.md).
- Nested brace initializers are not supported: `float4 m = {{1,2},{3,4}};` re-serialises to
  `float4({1,2},{3,4})` and the `{` is not a valid token, producing
  `Invalid brace initializer for type 'float4'. Unexpected token '{' in Graph expression.`
- Type **tokens** accepted in a declaration are a different, larger set than constructor **names** —
  `MaterialAttributes`, `Substrate`, `StaticBool`, `Texture2D`, `TextureCube`, `Texture2DArray`,
  `Texture3D`, `VolumeTexture` and `SamplerState` are declarable but have no constructor. See
  [Declarations](declarations.md) and [Types](../language/types.md).

## Diagnostics

Format specifiers are rendered as `{Placeholder}` throughout this page; the compiler emits the
substituted text.

| Message | Cause |
| :-- | :-- |
| `Constructor '{Name}' does not accept named arguments.` | Any `name = value` argument. |
| `Constructor '{Name}' cannot use Texture2D arguments.` | A texture object passed as an argument, of any texture dimension. |
| `Constructor '{Name}' cannot use MaterialAttributes arguments.` | A `MaterialAttributes` value passed as an argument. |
| `Constructor '{Name}' cannot use Substrate arguments.` | A `Substrate` value passed as an argument. |
| `Constructor '{Name}' expects a single scalar input.` | A 1-component constructor called with zero arguments, more than one argument, or one argument wider than 1 component. |
| `Constructor '{Name}' expects {N} total components but got {Total}.` | The argument widths do not sum to the constructor's width. |
| `Failed to create a constant float{N} node for constructor '{Name}'.` | Constant folding succeeded but the `ConstantNVector` node could not be created. |
| `Failed to create an AppendVector node.` | An `AppendVector` node in the concatenation chain could not be created. |
| `Invalid brace initializer for type '{Type}'. {Detail}` | The brace form failed; `{Detail}` is the constructor error above. |
| `Unknown Graph function '{Name}'.` | A misspelled constructor (`vec1`, `mat3`, `float5`) falls through to the user-function lookup. |

## Example

```c
Shader(Name="Docs/M_Constructors", Root="Game")
{
    Properties { ScalarParameter K = 0.5; }
    Settings   { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs    { vec4 Color; Base.EmissiveColor = Color; }

    Graph {
        float  a   = fract(K * 3.0);
        vec3   rgb = vec3(a);              // rule 7: scalar splat
        vec3   tint = vec3(0.2, 0.4, 0.8); // constant-folded to Constant3Vector
        vec4   rgba = float4(rgb, 1.0);    // rule 9: 3 + 1 packing
        Color = rgba * vec4(tint, 1.0);
    }
}
```

Generated nodes:

```text
Constant(3.0), Multiply, Frac                      -> a
AppendVector, AppendVector                         -> vec3(a)            (splat, 2 nodes)
Constant3Vector(0.2, 0.4, 0.8)                     -> tint               (folded, 1 node)
AppendVector                                       -> float4(rgb, 1.0)   (1 node)
Constant(1.0), AppendVector                        -> vec4(tint, 1.0)
Multiply                                           -> Base.EmissiveColor
```

## See also

- [Expressions](expressions.md) — operators, precedence, and the integer-division rule
- [Literals](literals.md) — what counts as a numeric literal for constant folding
- [Conversions](conversions.md) — narrowing, widening and authoritative component counts
- [Swizzle](swizzle.md) — the correct way to narrow a value
- [Declarations](declarations.md) — brace initializers and declared type tokens
- [Name resolution](name-resolution.md) — the call-resolution order constructors sit at the top of
- [Types](../language/types.md) — the complete type-token catalogue and removed aliases
- [`UE.TransformVector` / `UE.TransformPosition`](../builtins/transform.md) — the only matrix-like operations
- [Node reuse](node-reuse.md) — folded-vector deduplication
