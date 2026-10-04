# Constructors

> [DreamShader](../index.md) » [Graph](index.md) » **Constructors**

Call-syntax expressions named after a scalar or vector type that build a value of that width from
their arguments.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration default |
| Kind | expression, call form |
| Generates | `UMaterialExpressionAppendVector` (one per argument after the first), or one `UMaterialExpressionConstant` / `Constant2Vector` / `Constant3Vector` / `Constant4Vector` when every argument is constant; nothing at all in the pass-through and broadcast cases |

## Synopsis

```c
<constructor-call> := <constructor-name> ( <expression> [ , <expression> ] … )
```

Named arguments are rejected. A [brace initializer](declarations.md#brace-initializers) is exactly a
constructor call: `vec3 rgb = {r, g, b};` is rewritten as `vec3(r, g, b)` and obeys every rule on
this page.

The parser reads a constructor name as a **type** *(since 2.0.0)*: the call never reaches the
function lookup, and the binder checks it as it checks a `.dss` constructor.

## Constructor names

**34 names**, all matched **case-insensitively** (`FLOAT3(…)` works). The component count is the
number at the end of the name; a name without one, or with `1`, is a scalar.

| Name | Components | Kind | Family |
| :-- | :-- | :-- | :-- |
| `float` | 1 | float | HLSL float |
| `float1` | 1 | float | HLSL float |
| `float2` | 2 | float | HLSL float |
| `float3` | 3 | float | HLSL float |
| `float4` | 4 | float | HLSL float |
| `half` | 1 | half | HLSL half — identical behaviour to `float` |
| `half1` | 1 | half | HLSL half |
| `half2` | 2 | half | HLSL half |
| `half3` | 3 | half | HLSL half |
| `half4` | 4 | half | HLSL half |
| `vec2` | 2 | float | GLSL float vector |
| `vec3` | 3 | float | GLSL float vector |
| `vec4` | 4 | float | GLSL float vector |
| `int` | 1 | **int** | HLSL signed integer |
| `int2` | 2 | **int** | HLSL signed integer |
| `int3` | 3 | **int** | HLSL signed integer |
| `int4` | 4 | **int** | HLSL signed integer |
| `ivec2` | 2 | **int** | GLSL signed integer vector |
| `ivec3` | 3 | **int** | GLSL signed integer vector |
| `ivec4` | 4 | **int** | GLSL signed integer vector |
| `uint` | 1 | **uint** | HLSL unsigned integer |
| `uint2` | 2 | **uint** | HLSL unsigned integer |
| `uint3` | 3 | **uint** | HLSL unsigned integer |
| `uint4` | 4 | **uint** | HLSL unsigned integer |
| `uvec2` | 2 | **uint** | GLSL unsigned integer vector |
| `uvec3` | 3 | **uint** | GLSL unsigned integer vector |
| `uvec4` | 4 | **uint** | GLSL unsigned integer vector |
| `bool` | 1 | bool | HLSL boolean |
| `bool2` | 2 | bool | HLSL boolean |
| `bool3` | 3 | bool | HLSL boolean |
| `bool4` | 4 | bool | HLSL boolean |
| `bvec2` | 2 | bool | GLSL boolean vector |
| `bvec3` | 3 | bool | GLSL boolean vector |
| `bvec4` | 4 | bool | GLSL boolean vector |

*(since 2.0.0)* the parser takes every spelling the HLSL type grammar has, in any case, so a few more
names are constructors:

| Name | Components | Note |
| :-- | :-- | :-- |
| `int1`, `uint1`, `bool1` | 1 | HLSL's spelling of a scalar, as `float1` is |
| `double`, `double1` … `double4` | 1–4 | kind double; a float in the graph like every number |
| `StaticBool` | 1 | read as `bool`, as it is in a declaration |
| `float2x2` … `float4x4`, `mat2`, `mat3`, `mat4` (and the `NxM` of the other families) | — | a matrix, which the graph has no value for: [`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361) |

### Names that do not exist

| Absent | Reason |
| :-- | :-- |
| `vec1`, `ivec1`, `uvec1`, `bvec1` | The GLSL families start at 2 |
| `hvec2`, `hvec3`, `hvec4` | No GLSL half family |
| `dvec2`, `dvec3`, `dvec4` | No GLSL double family |
| `Scalar`, `Vector`, `Color` | Removed type aliases; see [Types](../language/types.md) |

Every name above lexes as an ordinary identifier and, because it is not a type, falls through to the
function lookup — [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208) unless a function of that name is
declared.

`MaterialAttributes`, `Substrate`, the texture types and `SamplerState` are type names too, but no
value of them can be constructed: `Texture2D(…)` is [`DSH4223`](../diagnostics/DSH4xxx.md#dsh4223).

> [!WARNING]
> **Constructor names shadow everything.** A constructor name is read as a type while the call is
> parsed — before `UE.*` builtins, `Substrate.*` builtins, math builtins, parameters, and all user
> definitions are looked at. A `Function`, `GraphFunction`, `ShaderFunction` or `VirtualFunction`
> named `float3`, `int`, `bool4`, `vec2` (or any other name in the tables) can be declared without a
> diagnostic but can never be called — every call site builds a constructor instead. See
> [Name resolution](name-resolution.md).

### Integer constructors

The integer names are `int`, `int1..4`, `ivec2..4`, `uint`, `uint1..4` and `uvec2..4`. Their result is
an `int` or `uint` value to the compiler.

The kind does exactly one thing: `/` is refused when **both** operands are integer constructor calls
([`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243)), as in 1.x. It performs no truncation, no rounding,
and no range clamping — an integer constructor produces the same float-valued graph as its `float`
counterpart, and `int(7.9)` is 7.9. See [Integer division](expressions.md#integer-division).

The kind comes from the constructor's own name, so wrapping in a non-integer constructor clears it:
`float(int(7))` is a float.

## Argument rules

The call and its arguments are checked first; every check that fails is reported.

| # | Condition | Behaviour |
| :-- | :-- | :-- |
| 1 | Any argument is named (`float3(x = 1.0)`) | [`DSH4225`](../diagnostics/DSH4xxx.md#dsh4225) |
| 2 | No argument at all (`float3()`) | [`DSH4221`](../diagnostics/DSH4xxx.md#dsh4221) |
| 3 | An argument is not a number or a bool — a texture object, a `MaterialAttributes` or a `Substrate` value — or is a matrix | [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) |

Then, with `N` the constructor's component count:

| # | Case | Behaviour | Nodes created |
| :-- | :-- | :-- | :-- |
| 4 | exactly one argument, with `N` components (for `N == 1`, a scalar) | the argument is returned **unchanged**; only its kind changes | none |
| 5 | `N > 1`, exactly one argument, and that argument is a scalar | **broadcast**: the scalar is wired where the value is read, and the material node replicates it *(since 2.0.0; 1.x built an `AppendVector` chain)* | none; one `ConstantNVector` when the scalar is constant |
| 6 | otherwise | the arguments' component counts must sum to **exactly** `N` | one `AppendVector` per argument after the first |
| 7 | the sum is not `N` | [`DSH4222`](../diagnostics/DSH4xxx.md#dsh4222) | — |

### Component packing

Rule 6 is a straight left-to-right concatenation of channels — arguments of mixed widths are legal as
long as the total is exact.

| Call | Total | Valid | Result |
| :-- | :-- | :-- | :-- |
| `float4(rgb, 1.0)` | 3 + 1 | yes | `float4` |
| `float4(uv, uv)` | 2 + 2 | yes | `float4` |
| `float3(x, yz)` | 1 + 2 | yes | `float3` |
| `float4(x, y, z, w)` | 1×4 | yes | `float4` |
| `vec3(0.5)` | scalar broadcast (rule 5) | yes | `(0.5, 0.5, 0.5)` |
| `float3(rgba)` | 4 | **no** | `DSH4222` |
| `float3(uv)` | 2 | **no** | `DSH4222` |
| `float(rgb)` | 3 | **no** | `DSH4222` |
| `float3()` | 0 | **no** | `DSH4221` |

> [!NOTE]
> **A constructor never narrows**, in a 1.x source either. `float3(rgba)` is an error, not a
> truncation; write `rgba.rgb`. Narrowing happens only at conversion sites — see
> [Conversions](conversions.md#narrowing). Widening happens only from a scalar (rule 5);
> `float3(uv)` will not zero-fill.

## Constant folding

When every argument is a constant, the whole call is one constant node instead of `N` `Constant`
nodes plus `AppendVector` nodes: a `Constant` / `Constant2Vector` / `Constant3Vector` /
`Constant4Vector` holding the values. A constant is a numeric literal, a negated one, a `const`
property, a variable that holds one, or any arithmetic on constants: the folding pass folds
operators, constructors and swizzles whose operands are all constants *(since 2.0.0)*.

| Written | Folded to |
| :-- | :-- |
| `vec2(0.5, 1.0)` | `Constant2Vector(0.5, 1.0)` |
| `vec3(0.5)` | `Constant3Vector(0.5, 0.5, 0.5)` — the broadcast folds too |
| `float4(1.0, 2.0, 3.0, -1.0)` | `Constant4Vector(1, 2, 3, -1)` |
| `int3(1, 2, 3)` | `Constant3Vector(1, 2, 3)` *(since 2.0.0)*; the value stays an `int3` to the compiler |
| `vec3(K, 0.0, 0.0)` | not folded when `K` is a parameter — two `AppendVector` nodes |

Every value has the width of its type; a folded constructor has no width of its own that would
override a declaration *(since 2.0.0)*. See
[Conversions](conversions.md#authoritative-component-counts).

Equal constants are one node: `vec3(0.5)` written five times yields one `Constant3Vector`. See
[Node reuse](node-reuse.md).

## Notes

- Rule 4 creates **no node**. `float3(SomeFloat3)` is a no-op wrapper; `float3(SomeInt3)` only
  changes the kind to float.
- A constructor call is an ordinary postfix expression and can be swizzled: `vec4(uv, 0, 1).xy`.
- A brace initializer with an empty body (`float3 v = {};`) does not reach a constructor at all — it
  produces the type's zero value. See [Declarations](declarations.md).
- Nested brace initializers are not supported: `float4 m = {{1,2},{3,4}};` is rewritten to
  `float4({1,2},{3,4})`, and a brace list as an argument is
  [`DSH2214`](../diagnostics/DSH2xxx.md#dsh2214).
- An output selector (`Output=`, `OutputIndex=`) on a constructor is
  [`DSH5253`](../diagnostics/DSH5xxx.md#dsh5253).

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH4225` | Any `name = value` argument. |
| `DSH4221` | A constructor with no argument. |
| `DSH4226` | An argument that is a texture object, a `MaterialAttributes` or `Substrate` value, or a matrix. |
| `DSH4222` | The argument widths do not sum to the constructor's width, and the call is not one scalar. |
| `DSH4223` | A type with no constructor: `MaterialAttributes`, `Substrate`, a texture type, `SamplerState`. |
| `DSH4361` | A matrix constructor: the graph has no matrix values. |
| `DSH2214` | A brace list nested in a brace initializer. |
| `DSH5253` | An output selector on a constructor. |
| `DSH4208` | A misspelled constructor (`vec1`, `float5`) falls through to the function lookup. |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | The emitter could not create an `AppendVector` or constant node. |

## Example

```c
Shader(Name="Docs/M_Constructors", Root="Game")
{
    Properties { ScalarParameter K = 0.5; }
    Settings   { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }

    Graph {
        float  a    = frac(K * 3.0);
        vec3   grey = vec3(a);              // rule 5: scalar broadcast, no node
        vec3   tint = vec3(0.2, 0.4, 0.8);  // constant-folded to Constant3Vector
        vec2   uv   = float2(a, 1.0);       // rule 6: 1 + 1 packing
        Color = tint * grey + float3(uv, 0.0);   // rule 6: 2 + 1 packing
    }
}
```

Generated nodes:

```text
ScalarParameter K
Constant(3.0), Multiply, Frac                      -> a
(no node)                                          -> vec3(a): `a` is wired where grey is read
Constant3Vector(0.2, 0.4, 0.8)                     -> tint               (folded, 1 node)
Multiply                                           -> tint * grey
Constant(1.0), AppendVector                        -> float2(a, 1.0)     (1 node)
Constant(0.0), AppendVector                        -> float3(uv, 0.0)
Add                                                -> Base.EmissiveColor
```

## See also

- [Expressions](expressions.md) — operators, precedence, and the integer-division rule
- [Literals](literals.md) — what counts as a constant for folding
- [Conversions](conversions.md) — narrowing, widening and the width of a value
- [Swizzle](swizzle.md) — the correct way to narrow a value
- [Declarations](declarations.md) — brace initializers and declared type tokens
- [Name resolution](name-resolution.md) — the call-resolution order constructors sit at the top of
- [Types](../language/types.md) — the complete type-token catalogue and removed aliases
- [`UE.TransformVector` / `UE.TransformPosition`](../builtins/transform.md) — the only matrix-like operations
- [Node reuse](node-reuse.md) — constant deduplication
