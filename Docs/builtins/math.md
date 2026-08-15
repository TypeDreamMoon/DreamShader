# Math builtins

> [DreamShader](../index.md) » [Builtins](index.md) » **Math builtins**

Unprefixed, HLSL-spelled call names that a `Graph` block lowers directly to arithmetic
`UMaterialExpression` nodes.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration initializer |
| Kind | builtin call surface |
| Generates | one `UMaterialExpression` per call, chosen per name — see [the catalogue](#catalogue). `reflect` and `refract` are the two exceptions and generate a small subgraph |
| Spellings | 29, covering 26 operations (three alias pairs) |
| Namespace | none — these are called bare, `saturate(x)`, not `UE.saturate(x)` |

## Synopsis

```c
{ abs | acos | asin | atan | ceil | cos | floor | frac | fract | length
| normalize | saturate | sin | sqrt } ( <x> )
{ atan2 | cross | dot | fmod | max | min | mod | pow | reflect | step } ( <x> , <y> )
{ clamp | lerp | mix | refract | smoothstep } ( <x> , <y> , <z> )
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
| `acos` *(unreleased)* | 1 | `UMaterialExpressionArccosine` | `Input` | width of the argument | inherited from the argument |
| `asin` *(unreleased)* | 1 | `UMaterialExpressionArcsine` | `Input` | width of the argument | inherited from the argument |
| `atan` *(unreleased)* | 1 | `UMaterialExpressionArctangent` | `Input` | width of the argument | inherited from the argument |
| `atan2` *(unreleased)* | 2 | `UMaterialExpressionArctangent2` | `Y` ← argument 1, `X` ← argument 2 | `max` of both arguments | set when either argument had it |
| `ceil` | 1 | `UMaterialExpressionCeil` | `Input` | width of the argument | inherited from the argument |
| `clamp` | 3 | `UMaterialExpressionClamp` | `Input`, `Min`, `Max` | width of argument 1 | from argument 1 |
| `cos` | 1 | `UMaterialExpressionCosine` | `Input` | width of the argument | inherited from the argument |
| `cross` *(unreleased)* | 2 | `UMaterialExpressionCrossProduct` | `A`, `B` | **always 3** | always set |
| `dot` | 2 | `UMaterialExpressionDotProduct` | `A`, `B` | **always 1** | always set |
| `floor` | 1 | `UMaterialExpressionFloor` | `Input` | width of the argument | inherited from the argument |
| `fmod` *(since 1.5.0)* | 2 | `UMaterialExpressionFmod` | `A` ← dividend, `B` ← divisor | width of argument 1 | from argument 1 |
| `frac` | 1 | `UMaterialExpressionFrac` | `Input` | width of the argument | inherited from the argument |
| `fract` *(since 1.5.0)* | 1 | `UMaterialExpressionFrac` | `Input` | width of the argument | inherited from the argument |
| `length` *(unreleased)* | 1 | `UMaterialExpressionLength` | `Input` | **always 1** | always set |
| `lerp` | 3 | `UMaterialExpressionLinearInterpolate` | `A`, `B`, `Alpha` | `max` of arguments 1 and 2 | set when either of arguments 1, 2 had it |
| `max` | 2 | `UMaterialExpressionMax` | `A`, `B` | `max` of both arguments | set when either argument had it |
| `min` | 2 | `UMaterialExpressionMin` | `A`, `B` | `max` of both arguments | set when either argument had it |
| `mix` | 3 | `UMaterialExpressionLinearInterpolate` | `A`, `B`, `Alpha` | `max` of arguments 1 and 2 | set when either of arguments 1, 2 had it |
| `mod` *(since 1.5.0)* | 2 | `UMaterialExpressionFmod` | `A` ← dividend, `B` ← divisor | width of argument 1 | from argument 1 |
| `normalize` | 1 | `UMaterialExpressionNormalize` | **`VectorInput`** | width of the argument | inherited from the argument |
| `pow` | 2 | `UMaterialExpressionPower` | `Base`, `Exponent` | width of argument 1 | from argument 1 |
| `reflect` *(unreleased)* | 2 | **a 4-node subgraph** — see [below](#reflect-refract) | — | `max` of both arguments | set when either argument had it |
| `refract` *(unreleased)* | 3 | **a 14-node subgraph** — see [below](#reflect-refract) | — | `max` of arguments 1 and 2 | set when either of arguments 1, 2 had it |
| `saturate` | 1 | `UMaterialExpressionSaturate` | `Input` | width of the argument | inherited from the argument |
| `sin` | 1 | `UMaterialExpressionSine` | `Input` | width of the argument | inherited from the argument |
| `smoothstep` *(unreleased)* | 3 | `UMaterialExpressionSmoothStep` | `Min`, `Max`, `Value` | `max` of all three arguments | set when any argument had it |
| `sqrt` | 1 | `UMaterialExpressionSquareRoot` | `Input` | width of the argument | inherited from the argument |
| `step` *(unreleased)* | 2 | `UMaterialExpressionStep` | `Y` ← argument 1 (edge), `X` ← argument 2 (value) | `max` of both arguments | set when either argument had it |

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
> **The 29 names on this page are reserved and shadow user code silently.** A `Function`,
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

### step

`step(edge, x)` returns `x >= edge ? 1 : 0`, as in HLSL. `UMaterialExpressionStep` names its pins the
other way round — `Y` is the edge and `X` is the value — so argument 1 wires to `Y` and argument 2 to
`X`. Writing the node form by hand, the equivalent call is
`UE.Expression(Class="Step", OutputType="float1", Y=edge, X=x)`.

### smoothstep

`smoothstep(min, max, x)`, argument order as in HLSL, wired straight to the node's `Min`, `Max` and
`Value` pins.

### length, cross

The two builtins besides `dot` with a fixed return width: `length` is always 1 component and `cross`
always 3, whatever the arguments were. Both widths are authoritative, and they match what the same
classes already report when reached through [`UE.Expression`](ue-expression.md).

### asin, acos, atan, atan2

The four inverse-trigonometric nodes. `atan2(y, x)` takes its arguments in HLSL order and the node's
pins are already named `Y` and `X`, so the mapping is direct.

> [!NOTE]
> The engine also ships `ArcsineFast`, `ArccosineFast`, `ArctangentFast` and `Arctangent2Fast` —
> cheaper approximations valid over a limited input range. They have no builtin spelling; reach them
> with `UE.Expression(Class="ArcsineFast", OutputType="float1", Input=x)`.

<a id="reflect-refract"></a>

### reflect, refract

The only two builtins with no node behind them. Unreal has no `Reflect` or `Refract`
`UMaterialExpression`, so both are lowered to the arithmetic HLSL defines them as, and the value
returned to the caller is the final node of that subgraph.

`reflect(i, n)` becomes `i - 2 * dot(i, n) * n` — four nodes (`DotProduct`, two `Multiply`,
`Subtract`). The literal `2` rides `Multiply`'s `ConstB` rather than costing a `Constant` node.

`refract(i, n, eta)` becomes the full HLSL definition — fourteen nodes:

```text
k = 1 - eta*eta * (1 - dot(n, i)^2)
k < 0 ? 0 : eta*i - (eta*dot(n, i) + sqrt(k)) * n
```

The total-internal-reflection test is an `If` node, so both sides are translated and one is
selected; `sqrt` of a negative `k` lands only on the discarded side, exactly as in HLSL. `k == 0`
still satisfies the formula (`sqrt(0) == 0`) and takes the refracted side. The zero branch is built
as `i * 0` rather than a constant so that its type always equals `i`'s — `If` requires its two
branches to agree, and the tracked component count cannot always guarantee a hand-picked constant
would.

> [!NOTE]
> Both are node-count-expensive by construction, and neither is common enough in a
> [`Graph`](../graph/index.md) block to be worth a graph that large. Where the surrounding code is
> already HLSL, write them in a [`Function`](../language/function.md) body instead and let the
> intrinsic do it in one node.

## Notes

- Every math node is created at editor X coordinate `360`, with Y taken from the generator's running
  layout counter. See [Graph layout](../generation/graph-layout.md).
- Results are **common-subexpression cached**. Two textually identical calls over identical operand
  values — `sin(X)` written twice — produce one `Sine` node, not two. The cache key covers the
  builtin name, the node class and every argument value. See
  [Node reuse](../graph/node-reuse.md).
- **There is no matrix on this surface, and there cannot be one.** The Unreal material graph has no
  matrix value type at all — no `float3x3`/`float4x4` value, no `mul(M, v)`, no matrix constructor —
  so matrix arithmetic has no spelling here regardless of what the DSL does. The two things that
  exist near it are the space-conversion nodes, reached as
  `UE.Expression(Class="Transform", OutputType="float3", Input=v)` and
  `UE.Expression(Class="TransformPosition", OutputType="float3", Input=p)` with `TransformSourceType`
  / `TransformType` naming the spaces. For genuine matrix math, write a
  [`Function`](../language/function.md) HLSL body, where `float4x4` and `mul` are just HLSL.
- There is still no exponential, logarithmic, `tan`, `sign`, `round`, `trunc` or `distance` builtin.
  Every one of them does have a node — reach it through [`UE.Expression`](ue-expression.md), for
  example `UE.Expression(Class="Logarithm2", OutputType="float1", X=x)` or
  `UE.Expression(Class="Distance", OutputType="float1", A=u, B=v)` — or write the operation in a
  `Function` HLSL body, where the full HLSL intrinsic set is available.
- Inside a `Function` HLSL body these names are *not* handled by this dispatcher at all; the body is
  emitted verbatim and HLSL's own intrinsics apply.
- The [decompiler](../tools/decompiler.md) emits these spellings when exporting an existing
  material: `LinearInterpolate` → `lerp`, `Clamp` (when `ClampMode == CMODE_Clamp`) → `clamp`,
  `Power` → `pow`, `DotProduct` → `dot`, `Normalize` → `normalize`, `Min`/`Max` → `min`/`max`,
  `Abs` → `abs`, `Saturate` → `saturate`, `Floor`/`Ceil`/`Frac`/`SquareRoot` →
  `floor`/`ceil`/`frac`/`sqrt`, and `Sine`/`Cosine` (when `Period` is 1.0) → `sin`/`cos`.
- The decompiler has no case for the newer builtins either — `Step`, `SmoothStep`, `Length`,
  `CrossProduct`, `Arcsine`, `Arccosine`, `Arctangent` and `Arctangent2` export as generic
  `UE.Expression(Class="…", …)` calls, the same way `Fmod` does. The exported source is equivalent
  and recompiles; it simply does not round-trip to the builtin spelling. `reflect` and `refract`
  cannot round-trip at all — they leave a subgraph of ordinary arithmetic nodes behind, with nothing
  marking it as having come from one call.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table; the compiler emits the
substituted text. `{Name}` is the spelling as the author wrote it, so its casing is preserved.
`{Index}` is 1-based.

| Message | Cause |
| :-- | :-- |
| `Math function '{Name}' expects exactly 1 argument.` | wrong argument count for a 1-argument builtin, **or** any argument was named |
| `Math function '{Name}' expects exactly 2 arguments.` | same, for `dot`, `pow`, `min`, `max`, `fmod`, `mod`, `step`, `cross`, `atan2`, `reflect` |
| `Math function '{Name}' expects exactly 3 arguments.` | same, for `lerp`, `mix`, `clamp`, `smoothstep`, `refract` |
| `Math function '{Name}' is missing argument {Index}.` | an argument slot the builtin asked for does not exist |
| `Math function '{Name}' argument {Index}: {Error}` | evaluating the argument expression failed; `{Error}` is the inner diagnostic |
| `Math function '{Name}' only accepts numeric scalar/vector arguments.` | an argument is a texture object, a `MaterialAttributes` value or a `Substrate` value |
| `Failed to create math function '{Name}'.` | the material node could not be created |
| `Math function '{Name}' could not bind input '{Input}'.` | the node class does not expose the expected input property (the pin-name-driven builtins: every 1-argument one plus `step`, `cross`, `atan2`, `smoothstep`) |
| `Math function '{Name}' failed to access input '{Input}'.` | the input property exists but its storage could not be reached (same builtins) |
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
