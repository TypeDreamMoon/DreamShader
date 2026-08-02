# Math builtins

> [DreamShader](../index.md) » [Builtins](index.md) » **Math builtins**

Unprefixed, HLSL-spelled call names that a `Graph` block lowers directly to arithmetic
`UMaterialExpression` nodes.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration initializer |
| Kind | builtin call surface |
| Generates | one `UMaterialExpression` per call, chosen per name — see [the catalogue](#catalogue) |
| Spellings | 19, covering 16 operations (three alias pairs) |
| Namespace | none — these are called bare, `saturate(x)`, not `UE.saturate(x)` |

## Synopsis

```c
{ abs | ceil | cos | floor | frac | fract | normalize | saturate | sin | sqrt } ( <x> )
{ dot | fmod | max | min | mod | pow } ( <x> , <y> )
{ clamp | lerp | mix } ( <x> , <y> , <z> )
```

`( )` and `,` are literal DreamShaderLang punctuation; `{ a | b }` is meta-notation and is never
typed. Each `<x>` / `<y>` / `<z>` is any [Graph expression](../graph/expressions.md).

Every name is matched **case-insensitively**: `SATURATE(x)`, `Lerp(a, b, t)` and `Sin(x)` all
resolve. Every argument must be **positional** — see [the named-argument warning](#named-arguments).

## Catalogue

One row per accepted spelling. *Return width* is the component count the generator assigns to the
call's result; *Authoritative* is whether that width is marked authoritative for the widening rules
in [Conversions](../graph/conversions.md#authoritative-component-counts).

| Spelling | Arity | Lowers to | Input pins wired, in order | Return width | Authoritative |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `abs` | 1 | `UMaterialExpressionAbs` | `Input` | width of the argument | inherited from the argument |
| `ceil` | 1 | `UMaterialExpressionCeil` | `Input` | width of the argument | inherited from the argument |
| `clamp` | 3 | `UMaterialExpressionClamp` | `Input`, `Min`, `Max` | width of argument 1 | from argument 1 |
| `cos` | 1 | `UMaterialExpressionCosine` | `Input` | width of the argument | inherited from the argument |
| `dot` | 2 | `UMaterialExpressionDotProduct` | `A`, `B` | **always 1** | always set |
| `floor` | 1 | `UMaterialExpressionFloor` | `Input` | width of the argument | inherited from the argument |
| `fmod` *(since 1.5.0)* | 2 | `UMaterialExpressionFmod` | `A` ← dividend, `B` ← divisor | width of argument 1 | from argument 1 |
| `frac` | 1 | `UMaterialExpressionFrac` | `Input` | width of the argument | inherited from the argument |
| `fract` *(since 1.5.0)* | 1 | `UMaterialExpressionFrac` | `Input` | width of the argument | inherited from the argument |
| `lerp` | 3 | `UMaterialExpressionLinearInterpolate` | `A`, `B`, `Alpha` | `max` of arguments 1 and 2 | set when either of arguments 1, 2 had it |
| `max` | 2 | `UMaterialExpressionMax` | `A`, `B` | `max` of both arguments | set when either argument had it |
| `min` | 2 | `UMaterialExpressionMin` | `A`, `B` | `max` of both arguments | set when either argument had it |
| `mix` | 3 | `UMaterialExpressionLinearInterpolate` | `A`, `B`, `Alpha` | `max` of arguments 1 and 2 | set when either of arguments 1, 2 had it |
| `mod` *(since 1.5.0)* | 2 | `UMaterialExpressionFmod` | `A` ← dividend, `B` ← divisor | width of argument 1 | from argument 1 |
| `normalize` | 1 | `UMaterialExpressionNormalize` | **`VectorInput`** | width of the argument | inherited from the argument |
| `pow` | 2 | `UMaterialExpressionPower` | `Base`, `Exponent` | width of argument 1 | from argument 1 |
| `saturate` | 1 | `UMaterialExpressionSaturate` | `Input` | width of the argument | inherited from the argument |
| `sin` | 1 | `UMaterialExpressionSine` | `Input` | width of the argument | inherited from the argument |
| `sqrt` | 1 | `UMaterialExpressionSquareRoot` | `Input` | width of the argument | inherited from the argument |

Alias pairs — the two spellings in each pair are interchangeable and produce identical nodes:
`lerp` / `mix`, `frac` / `fract`, `fmod` / `mod`.

## Argument rules

These apply identically to every builtin above.

| # | Rule | Consequence when violated |
| :-- | :-- | :-- |
| 1 | Arity is exact — no defaults, no optional arguments, no varargs | `Math function '{Name}' expects exactly {N} argument(s).` |
| 2 | Every argument is positional; a named argument is not accepted | reported as an arity error, see [below](#named-arguments) |
| 3 | Each argument is evaluated as a full Graph expression, including nested builtin calls | the inner error is wrapped as `Math function '{Name}' argument {Index}: {Error}` |
| 4 | Texture-object values are rejected | `Math function '{Name}' only accepts numeric scalar/vector arguments.` |
| 5 | `MaterialAttributes` values are rejected | same message |
| 6 | `Substrate` values are rejected | same message |
| 7 | Component counts of the arguments are **not** checked, widened or broadcast | nothing here; the mismatch surfaces later as an Unreal material-translation error on the generated node |

Rule 7 is the one to watch: `dot(vec3Value, floatValue)` is accepted by DreamShader without a
diagnostic and fails during Unreal's own shader compile. Unlike the arithmetic operators, this path
has no scalar/vector compatibility test — compare
[Expressions ▸ Operand rules](../graph/expressions.md#operand-rules).

## Name resolution

Math-builtin names are resolved **before** any user-declared name. The `Graph` call dispatcher tests,
in order:

| # | Candidate | Reference |
| :-- | :-- | :-- |
| 1 | vector/scalar constructor names (`float3`, `vec4`, `int2`, …) | [Constructors](../graph/constructors.md) |
| 2 | `UE.SceneTexture` | [`UE.*` catalogue](ue.md) |
| 3 | any `UE.`-prefixed callee | [`UE.*` catalogue](ue.md) |
| 4 | any `Substrate.`-prefixed callee | [`Substrate.*`](substrate.md) |
| 5 | **math builtins — this page** | — |
| 6 | `SampleTexture2D` | [`UE.*` catalogue](ue.md) |
| 7 | declared properties (parameter pin-call form) | [Using parameters in `Graph`](../parameters/graph-usage.md) |
| 8 | `Function`, `GraphFunction`, `ShaderFunction`, `VirtualFunction` | [Calls](../graph/calls.md) |

> [!WARNING]
> **The 19 names on this page are reserved and shadow user code silently.** A `Function`,
> `GraphFunction`, `ShaderFunction`, `VirtualFunction` or property named `lerp`, `clamp`, `dot`,
> `min`, `max`, `pow`, `abs` — or any other spelling in the catalogue — is unreachable from a `Graph`
> block: the builtin wins at step 5 and no diagnostic is emitted. The declaration still compiles and
> still generates its asset; only the `Graph` call site is redirected. Rename the user symbol, or
> call it from a `Function` body instead of a `Graph` block.
>
> Constructor names (step 1) are reserved the same way. Full lookup order:
> [Name resolution](../graph/name-resolution.md).

> [!NOTE]
> A misspelled builtin is not reported as a math error. `saturte(x)` falls through all eight steps
> and is reported by the call path as `Unknown Graph function 'saturte'.`

<a id="named-arguments"></a>

## Named arguments

Every arity guard is evaluated as "argument count is wrong **or** an argument is named", and both
outcomes emit the arity message.

> [!WARNING]
> Passing a named argument to a math builtin reports an **arity** error, not a namedness error.
> `saturate(Input = X)` — one argument, correctly named after the node's pin — fails with
> `Math function 'saturate' expects exactly 1 argument.` The fix is to drop the name:
> `saturate(X)`. Named arguments are a `UE.*` / `Substrate.*` feature, not a math-builtin feature.

## Per-builtin notes

### clamp

`clamp(Input, Min, Max)` wires all three arguments and leaves the node's `ClampMode` at its default,
`CMODE_Clamp`. To generate a `Clamp` node in `CMODE_ClampMin` or `CMODE_ClampMax`, use the generic
form instead: `UE.Expression(Class="Clamp", OutputType="float1", Input=x, Min=a, ClampMode="CMODE_ClampMin")`.
See [`UE.Expression`](ue-expression.md).

### dot

The only builtin with a fixed return width. `dot` always produces a 1-component, authoritative
result regardless of the argument widths, so `float d = dot(A, B);` needs no swizzle.

### fmod, mod

Argument 1 is the dividend and argument 2 the divisor; they are wired to the node's `A` and `B` pins
respectively. The result takes the dividend's width.

`mod` is the GLSL spelling. Inside a [`Function`](../language/function.md) HLSL body the identifier
`mod` is rewritten to `fmod` by the GLSL-alias pass; in a `Graph` block both spellings are accepted
directly by this dispatcher, with no rewrite.

> [!NOTE]
> The [decompiler](../tools/decompiler.md) has no case for `UMaterialExpressionFmod`. An existing
> `Fmod` node exports as a generic `UE.Expression(Class="Fmod", …)` call rather than as `fmod(…)`.
> The exported source is equivalent; it simply does not round-trip to the builtin spelling.

### lerp, mix

The result width is `max` of arguments 1 and 2 — the `Alpha` argument does not participate. A scalar
`Alpha` blending two `vec3` values yields a `vec3`.

### min, max

The two names share one implementation and differ only in the node class selected. Both take the
`max` of the two argument widths.

### normalize

The only builtin whose input pin is not named `Input`. Inputs on this path are bound by reflected
property name, and `UMaterialExpressionNormalize` names its pin `VectorInput`; the difference is
invisible at the call site (`normalize(N)`) but appears in the two `could not bind input` /
`failed to access input` diagnostics.

### sin, cos

Both leave the node's `Period` property at its default. For a non-default period use
`UE.Expression(Class="Sine", OutputType="float1", Input=x, Period=2.0)`.

## Notes

- Every math node is created at editor X coordinate `360`, with Y taken from the generator's running
  layout counter. See [Graph layout](../generation/graph-layout.md).
- Results are **common-subexpression cached**. Two textually identical calls over identical operand
  values — `sin(X)` written twice — produce one `Sine` node, not two. The cache key covers the
  builtin name, the node class and every argument value. See
  [Node reuse](../graph/node-reuse.md).
- There is no matrix, trigonometric-inverse, exponential, logarithmic, `step`, `smoothstep`,
  `reflect`, `refract`, `cross` or `length` builtin on this surface. Reach any other
  `UMaterialExpression` through [`UE.Expression`](ue-expression.md) — for example
  `UE.Expression(Class="CrossProduct", OutputType="float3", A=u, B=v)` — or write the operation in a
  [`Function`](../language/function.md) HLSL body, where the full HLSL intrinsic set is available.
- Inside a `Function` HLSL body these names are *not* handled by this dispatcher at all; the body is
  emitted verbatim and HLSL's own intrinsics apply.
- The [decompiler](../tools/decompiler.md) emits these spellings when exporting an existing
  material: `LinearInterpolate` → `lerp`, `Clamp` (when `ClampMode == CMODE_Clamp`) → `clamp`,
  `Power` → `pow`, `DotProduct` → `dot`, `Normalize` → `normalize`, `Min`/`Max` → `min`/`max`,
  `Abs` → `abs`, `Saturate` → `saturate`, `Floor`/`Ceil`/`Frac`/`SquareRoot` →
  `floor`/`ceil`/`frac`/`sqrt`, and `Sine`/`Cosine` (when `Period` is 1.0) → `sin`/`cos`.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table; the compiler emits the
substituted text. `{Name}` is the spelling as the author wrote it, so its casing is preserved.
`{Index}` is 1-based.

| Message | Cause |
| :-- | :-- |
| `Math function '{Name}' expects exactly 1 argument.` | wrong argument count for a 1-argument builtin, **or** any argument was named |
| `Math function '{Name}' expects exactly 2 arguments.` | same, for `dot`, `pow`, `min`, `max`, `fmod`, `mod` |
| `Math function '{Name}' expects exactly 3 arguments.` | same, for `lerp`, `mix`, `clamp` |
| `Math function '{Name}' is missing argument {Index}.` | an argument slot the builtin asked for does not exist |
| `Math function '{Name}' argument {Index}: {Error}` | evaluating the argument expression failed; `{Error}` is the inner diagnostic |
| `Math function '{Name}' only accepts numeric scalar/vector arguments.` | an argument is a texture object, a `MaterialAttributes` value or a `Substrate` value |
| `Failed to create math function '{Name}'.` | the material node could not be created |
| `Math function '{Name}' could not bind input '{Input}'.` | the node class does not expose the expected input property (unary builtins only) |
| `Math function '{Name}' failed to access input '{Input}'.` | the input property exists but its storage could not be reached (unary builtins only) |
| `Unknown Graph function '{Name}'.` | the name is not a builtin, constructor, property or user function — emitted by the call path, not by this one |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Docs/M_MathBuiltins")
{
    Properties = {
        float X = 0.5;
        vec3  A = vec3(1.0, 0.0, 0.0);
        vec3  B = vec3(0.0, 1.0, 0.0);
    }
    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs  = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = {
        float s     = sin(X);
        float c     = cos(X);
        float cl    = clamp(X, 0.0, 1.0);
        float sa    = saturate(X);
        vec3  mixed = lerp(A, B, sa);
        vec3  unit  = normalize(mixed);
        float d     = dot(unit, A);
        Color = mixed * (s + c + cl) + unit * d;
    }
}
```

Generated nodes:

```text
Sine(X)                       -> s
Cosine(X)                     -> c
Clamp(X, 0.0, 1.0)            -> cl
Saturate(X)                   -> sa
LinearInterpolate(A, B, sa)   -> mixed     (3 components: max(3, 3))
Normalize(mixed)              -> unit      (3 components)
DotProduct(unit, A)           -> d         (1 component, always)
Add / Multiply chain          -> Color
```

## See also

- [Builtins](index.md) — the call surfaces available inside `Graph`
- [`UE.*` catalogue](ue.md) — every named material-node builtin
- [`UE.Expression`](ue-expression.md) — the generic escape hatch for any `UMaterialExpression`
- [`OutputType` values](output-type.md) — the token set `UE.Expression` accepts
- [Transform builtins](transform.md) — `UE.TransformVector` / `UE.TransformPosition`
- [`Substrate.*`](substrate.md) — Substrate node wrappers (UE 5.4+)
- [`DreamShaderBuiltins.ush`](hlsl-library.md) — the shipped HLSL helper header
- [Expressions and operators](../graph/expressions.md) — `+ - * /`, precedence, operand rules
- [Constructors](../graph/constructors.md) — the constructor names that shadow builtins first
- [Conversions](../graph/conversions.md) — widening rules and authoritative component counts
- [Calls](../graph/calls.md) — call syntax, named arguments, out arguments
- [Name resolution](../graph/name-resolution.md) — the full lookup order and shadowing rules
- [Node reuse](../graph/node-reuse.md) — why repeated calls produce one node
- [Unsupported constructs](../graph/unsupported.md) — `%` and the other absent operators
- [`Function`](../language/function.md) — HLSL bodies, where the full intrinsic set applies
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
