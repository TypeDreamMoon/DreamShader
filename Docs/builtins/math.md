# Math builtins

> [DreamShader](../index.md) » [Builtins](index.md) » **Math builtins**

Unprefixed, HLSL-spelled call names that lower directly to arithmetic `UMaterialExpression` nodes.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration initializer; a `.dss` takes the same names anywhere an expression stands |
| Kind | builtin call surface — the compiler's core operations |
| Generates | one `UMaterialExpression` per call, chosen per name — see [the catalogue](#catalogue). Five names — `fwidth`, `rcp`, `rsqrt`, `reflect`, `refract` — have no node behind them and generate a small subgraph |
| Spellings | 44 HLSL names, plus four GLSL spellings a 1.x source may still use *(since 2.0.0; 29 spellings through 1.9.x)* |
| Namespace | none — these are called bare, `saturate(x)`, not `UE.saturate(x)` |

## Synopsis

```c
{ abs | acos | asin | atan | ceil | cos | ddx | ddy | exp | exp2 | floor | frac | fwidth | length
| log | log2 | log10 | normalize | rcp | round | rsqrt | saturate | sign | sin | sqrt | tan | trunc } ( <x> )
{ atan2 | cross | distance | dot | fmod | max | min | pow | reflect | step } ( <x> , <y> )
{ clamp | lerp | refract | smoothstep } ( <x> , <y> , <z> )
```

`( )` and `,` are literal DreamShaderLang punctuation; `{ a | b }` is meta-notation and is never
typed. Each `<x>` / `<y>` / `<z>` is any [Graph expression](../graph/expressions.md). `sinh`, `cosh`
and `tanh` are names too, and refused — see [Notes](#notes).

Names are matched **exactly** *(since 2.0.0)*. A 1.x source still gets the 1.x leniencies, each
with a diagnostic that says so — see [Name resolution](#name-resolution). An argument is positional,
or named after the node's pin — see [Named arguments](#named-arguments).

## Catalogue

One row per HLSL name. *Result* is the type the compiler gives the call; *widest operand* means the
widest of all the arguments, scalars broadcasting to it.

| Name | Arity | Lowers to | Input pins, in argument order | Result |
| :-- | :-- | :-- | :-- | :-- |
| `abs` | 1 | `UMaterialExpressionAbs` | `Input` | the argument's |
| `acos` *(since 1.6.0)* | 1 | `UMaterialExpressionArccosine` | `Input` | the argument's |
| `asin` *(since 1.6.0)* | 1 | `UMaterialExpressionArcsine` | `Input` | the argument's |
| `atan` *(since 1.6.0)* | 1 | `UMaterialExpressionArctangent` | `Input` | the argument's |
| `atan2` *(since 1.6.0)* | 2 | `UMaterialExpressionArctangent2` | `Y`, `X` | widest operand |
| `ceil` | 1 | `UMaterialExpressionCeil` | `Input` | the argument's |
| `clamp` | 3 | `UMaterialExpressionClamp` | `Input`, `Min`, `Max` | widest operand |
| `cos` | 1 | `UMaterialExpressionCosine` | `Input` | the argument's |
| `cross` *(since 1.6.0)* | 2 | `UMaterialExpressionCrossProduct` | `A`, `B` | **always `float3`** |
| `ddx` *(since 2.0.0)* | 1 | `UMaterialExpressionDDX` | `Value` | the argument's |
| `ddy` *(since 2.0.0)* | 1 | `UMaterialExpressionDDY` | `Value` | the argument's |
| `distance` *(since 2.0.0)* | 2 | `UMaterialExpressionDistance` | `A`, `B` | **always `float`** |
| `dot` | 2 | `UMaterialExpressionDotProduct` | `A`, `B` | **always `float`** |
| `exp` *(since 2.0.0)* | 1 | `UMaterialExpressionExponential` | `Input` | the argument's |
| `exp2` *(since 2.0.0)* | 1 | `UMaterialExpressionExponential2` | `Input` | the argument's |
| `floor` | 1 | `UMaterialExpressionFloor` | `Input` | the argument's |
| `fmod` *(since 1.5.0)* | 2 | `UMaterialExpressionFmod` | `A` ← dividend, `B` ← divisor | widest operand |
| `frac` | 1 | `UMaterialExpressionFrac` | `Input` | the argument's |
| `fwidth` *(since 2.0.0)* | 1 | **a subgraph**: `abs(ddx(x)) + abs(ddy(x))` | — | the argument's |
| `length` *(since 1.6.0)* | 1 | `UMaterialExpressionLength` | `Input` | **always `float`** |
| `lerp` | 3 | `UMaterialExpressionLinearInterpolate` | `A`, `B`, `Alpha` | widest operand |
| `log` *(since 2.0.0)* | 1 | `UMaterialExpressionLogarithm` | `Input` | the argument's |
| `log2` *(since 2.0.0)* | 1 | `UMaterialExpressionLogarithm2` | `X` | the argument's |
| `log10` *(since 2.0.0)* | 1 | `UMaterialExpressionLogarithm10` | `X` | the argument's |
| `max` | 2 | `UMaterialExpressionMax` | `A`, `B` | widest operand |
| `min` | 2 | `UMaterialExpressionMin` | `A`, `B` | widest operand |
| `normalize` | 1 | `UMaterialExpressionNormalize` | **`VectorInput`** | the argument's |
| `pow` | 2 | `UMaterialExpressionPower` | `Base`, `Exponent` | widest operand |
| `rcp` *(since 2.0.0)* | 1 | **a subgraph**: `Divide` with `ConstA = 1` | — | the argument's |
| `reflect` *(since 1.6.0)* | 2 | **a 4-node subgraph** — see [below](#reflect-refract) | — | **always `float3`** |
| `refract` *(since 1.6.0)* | 3 | **a 14-node subgraph** — see [below](#reflect-refract) | — | **always `float3`** |
| `round` *(since 2.0.0)* | 1 | `UMaterialExpressionRound` | `Input` | the argument's |
| `rsqrt` *(since 2.0.0)* | 1 | **a subgraph**: `SquareRoot`, then `Divide` with `ConstA = 1` | — | the argument's |
| `saturate` | 1 | `UMaterialExpressionSaturate` | `Input` | the argument's |
| `sign` *(since 2.0.0)* | 1 | `UMaterialExpressionSign` | `Input` | the argument's |
| `sin` | 1 | `UMaterialExpressionSine` | `Input` | the argument's |
| `smoothstep` *(since 1.6.0)* | 3 | `UMaterialExpressionSmoothStep` | `Min`, `Max`, `Value` | widest operand |
| `sqrt` | 1 | `UMaterialExpressionSquareRoot` | `Input` | the argument's |
| `step` *(since 1.6.0)* | 2 | `UMaterialExpressionStep` | `Y` ← argument 1 (edge), `X` ← argument 2 (value) | widest operand |
| `tan` *(since 2.0.0)* | 1 | `UMaterialExpressionTangent` | `Input` | the argument's |
| `trunc` *(since 2.0.0)* | 1 | `UMaterialExpressionTruncate` | `Input` | the argument's |

GLSL spellings, read in a 1.x source as the HLSL name, with the warning
[`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277): `mix` → `lerp`, `fract` → `frac`, `mod` → `fmod`,
`inversesqrt` → `rsqrt`. In a `.dss` they are [`DSH4250`](../diagnostics/DSH4xxx.md#dsh4250).
[`dsc migrate`](../tools/migrate.md) respells them.

## Argument rules

These apply identically to every builtin above, in a 1.x source and a `.dss` alike.

| # | Rule | Consequence when violated |
| :-- | :-- | :-- |
| 1 | Arity is exact — no defaults, no optional arguments, no varargs | [`DSH4224`](../diagnostics/DSH4xxx.md#dsh4224) |
| 2 | An argument is positional, or named after one of the node's input pins; a positional argument fills the first operand not filled yet. In a `.dss` a positional argument after a named one is [`DSH2158`](../diagnostics/DSH2xxx.md#dsh2158) | an unknown name is [`DSH4216`](../diagnostics/DSH4xxx.md#dsh4216), a pin given twice [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215), a pin left out between given ones [`DSH4217`](../diagnostics/DSH4xxx.md#dsh4217) |
| 3 | Each argument is evaluated as a full Graph expression, including nested builtin calls | the inner expression reports its own error, at its own position |
| 4 | Every argument is a number (or a bool, read as 0 / 1) — a texture object, a `MaterialAttributes` value or a `Substrate` value is not | [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) |
| 5 | Component counts **are** checked *(since 2.0.0)*: the widest operand sets the width, a scalar broadcasts to it, a narrower vector does not widen | `DSH4226` |
| 6 | `cross`, `reflect` and `refract` take `float3` operands | a narrower vector is `DSH4226`; a wider one is cut to three in a 1.x source with the note [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) — what 1.x did — and is `DSH4226` in a `.dss` |
| 7 | Two integer operands of `/` are refused — the graph has no integer division | [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243) |

Through 1.9.x none of rules 4 – 6 was a compile check: `dot(vec3Value, vec2Value)` was accepted
without a word and failed later, in Unreal's own material translation.

## Name resolution

A call to a bare name is resolved in this order. The full lookup is on
[Name resolution](../graph/name-resolution.md).

| # | Candidate | Reference |
| :-- | :-- | :-- |
| 1 | in a 1.x `Graph`, the spellings the legacy front end rewrites while it reads the call: `SampleTexture2D`, a declared property's pin-call form, `UE.SceneTexture` and the other 1.x call shapes | [`UE.*` catalogue](ue.md) · [Using parameters in `Graph`](../parameters/graph-usage.md) |
| 2 | type names — constructors such as `float3(…)`, `vec4(…)` | [Constructors](../graph/constructors.md) |
| 3 | `UE.`- and `Substrate.`-prefixed callees | [`UE.*` catalogue](ue.md) · [`Substrate.*`](substrate.md) |
| 4 | `Texture2DSample`, `Texture2DSampleLevel` | |
| 5 | **math builtins — this page**, by the exact HLSL name; then a GLSL spelling (`DSH5277` in 1.x, `DSH4250` in a `.dss`) | — |
| 6 | functions this file declares or imports — `Function`, `GraphFunction`, `ShaderFunction`, `VirtualFunction`, a `.dss` function | [Calls](../graph/calls.md) |
| 7 | in a 1.x source only: a builtin or a function whose name matches **ignoring case**, with the warning [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275) | — |

> [!IMPORTANT]
> **A function cannot be named like a builtin** *(since 2.0.0)*. A `Function`, `GraphFunction`,
> `ShaderFunction`, `VirtualFunction` or `.dss` function named `lerp`, `clamp`, `dot`, `min`, `max`,
> `pow`, `abs` — any HLSL name or GLSL spelling on this page — is
> [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206) at its declaration. Through 1.9.x such a function
> compiled and was silently unreachable from a `Graph` block, because the builtin won.

A misspelled builtin — `saturte(x)` — is [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208), with a
*did you mean* when a declared name differs only in case.

<a id="named-arguments"></a>

## Named arguments

*(since 2.0.0)* A named argument names one of the node's input pins — the *Input pins* column of the
catalogue — and fills that operand, wherever it stands in the list:

```c
float t = lerp(A, B, Alpha = Mask);     // A, B by position; Alpha by name
float c = clamp(X, Max = 1.0, Min = 0.0);
```

The pin names are the engine's, so they are not always HLSL's: `pow` is `Base` / `Exponent`, `step`
is `Y` (the edge) / `X` (the value), `normalize` is `VectorInput`. A name the node does not have is
[`DSH4216`](../diagnostics/DSH4xxx.md#dsh4216), which lists the pins. The builtins with no node
(`fwidth`, `rcp`, `rsqrt`, `reflect`, `refract`) take their arguments in order only.

> [!NOTE]
> Through 1.9.x a named argument on a math builtin was reported as an **arity** error —
> `saturate(Input = X)` failed with an "expects exactly 1 argument" message. It selects the pin now.

## Per-builtin notes

### clamp

`clamp(Input, Min, Max)` wires all three arguments and leaves the node's `ClampMode` at its default,
`CMODE_Clamp`. For `CMODE_ClampMin` or `CMODE_ClampMax`, call the node itself:
`UE.Clamp(Input = x, Min = a, ClampMode = CMODE_ClampMin)`. See [`UE.Expression`](ue-expression.md).

### dot, length, distance, cross

The builtins with a fixed result: `dot`, `length` and `distance` are always one component, `cross`
always three, whatever the arguments were — so `float d = dot(A, B);` needs no swizzle.

### fmod

Argument 1 is the dividend and argument 2 the divisor; they are wired to the node's `A` and `B` pins
respectively.

`mod` is the GLSL spelling. In a `Graph` block a 1.x source may still write it, with the warning
`DSH5277`. Inside a [`Function`](../language/function.md) HLSL body the identifier `mod` is rewritten
to `fmod` — silently, ignoring case — together with the other GLSL names.

### lerp

The result is as wide as the widest of the three arguments, `Alpha` included: a scalar `Alpha`
blending two `float3` values yields a `float3`, and is broadcast to it. Over two Substrate values,
`lerp(A, B, t)` is a `Substrate.HorizontalMixing` instead — see
[Substrate sugar](../language-v2/substrate.md).

### min, max

The two names share one typing rule and differ only in the node class selected.

### normalize

The only builtin whose single input pin is not named `Input`: `UMaterialExpressionNormalize` names it
`VectorInput`. The difference is invisible at a positional call site (`normalize(N)`), and it is the
name a named argument has to use.

### sin, cos

Both leave the node's `Period` property at its default. For a non-default period call the node
itself: `UE.Sine(Input = x, Period = 2.0)`.

### step

`step(edge, x)` returns `x >= edge ? 1 : 0`, as in HLSL. `UMaterialExpressionStep` names its pins the
other way round — `Y` is the edge and `X` is the value — so argument 1 wires to `Y` and argument 2 to
`X`.

### smoothstep

`smoothstep(min, max, x)`, argument order as in HLSL, wired straight to the node's `Min`, `Max` and
`Value` pins.

### asin, acos, atan, atan2

The four inverse-trigonometric nodes. `atan2(y, x)` takes its arguments in HLSL order and the node's
pins are already named `Y` and `X`, so the mapping is direct.

> [!NOTE]
> The engine also ships `ArcsineFast`, `ArccosineFast`, `ArctangentFast` and `Arctangent2Fast` —
> cheaper approximations valid over a limited input range. They have no builtin spelling; call the node
> itself, `UE.ArcsineFast(Input = x)`.

### fwidth, rcp, rsqrt

No engine node does any of the three. `fwidth(x)` is built as `abs(ddx(x)) + abs(ddy(x))`, which is
what the HLSL intrinsic is defined as; `rcp(x)` as a `Divide` with `ConstA = 1`; `rsqrt(x)` as a
`SquareRoot` feeding such a `Divide`.

<a id="reflect-refract"></a>

### reflect, refract

Unreal has no `Reflect` or `Refract` `UMaterialExpression`, so both are lowered to the arithmetic
HLSL defines them as, and the value returned to the caller is the final node of that subgraph.

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
as `i * 0` rather than a constant so that its width always equals `i`'s, which `If` requires of its
two branches.

> [!NOTE]
> Both are node-count-expensive by construction. Where the surrounding code is already HLSL, write
> them in a [`Function`](../language/function.md) body instead and let the intrinsic do it in one node.

## Notes

- Two identical calls over identical operands — `sin(X)` written twice — become one node: the IR's
  structural de-duplication merges them before anything is emitted. See
  [Node reuse](../graph/node-reuse.md).
- Node positions come from the [graph layout](../generation/graph-layout.md), like every other node.
- **`sinh`, `cosh` and `tanh` are refused** with [`DSH4246`](../diagnostics/DSH4xxx.md#dsh4246): the
  material graph has no hyperbolic node. Write them in a [`Function`](../language/function.md) body
  (or a `/// @custom` function in a `.dss`), where the shader compiler has them.
- **There is no matrix on this surface.** The Unreal material graph has no matrix value type — no
  `float3x3` value, no `mul(M, v)` — and a matrix that survives constant folding is
  [`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361). The space-conversion nodes are the
  [Transform builtins](transform.md). For genuine matrix math, write a
  [`Function`](../language/function.md) HLSL body, where `float4x4` and `mul` are just HLSL.
- Inside a `Function` HLSL body these names are *not* handled by the compiler at all; the body is
  emitted verbatim and HLSL's own intrinsics apply.
- The [decompiler](../tools/decompiler.md) writes these spellings back. Its 2.0 writer (`.dss`, the
  default) prints the builtin for any node of a class in the catalogue whose input pins are all
  connected and whose other properties are at their defaults — a `Sine` with a non-default `Period`
  stays `UE.Sine(...)` — and recognises the subgraphs `fwidth`, `rcp`, `rsqrt`, `reflect` and
  `refract` lower to when nothing else reads into them. The legacy writer (`-Format Legacy`, the
  *Export Legacy* entries) keeps its 1.x table.

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| [`DSH4224`](../diagnostics/DSH4xxx.md#dsh4224) | the wrong number of arguments |
| [`DSH4216`](../diagnostics/DSH4xxx.md#dsh4216) | a named argument names no input pin of the node |
| [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215) | one pin is given twice |
| [`DSH4217`](../diagnostics/DSH4xxx.md#dsh4217) | an argument is missing between the ones given |
| [`DSH2158`](../diagnostics/DSH2xxx.md#dsh2158) | *(`.dss`)* a positional argument after a named one |
| [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) | an argument is not a number, or its width does not fit the call's |
| [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) | *(info, 1.x only)* a wider vector cut down to the width the call takes |
| [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243) | an integer division |
| [`DSH4246`](../diagnostics/DSH4xxx.md#dsh4246) | `sinh`, `cosh` or `tanh` |
| [`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277) | *(warning, 1.x only)* a GLSL spelling |
| [`DSH4250`](../diagnostics/DSH4xxx.md#dsh4250) | a GLSL spelling in a `.dss` |
| [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275) | *(warning, 1.x only)* a name that matches a builtin only ignoring case |
| [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208) | the name is no builtin, type, node or function |
| [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206) | a function declared with a builtin's name |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | the emitter could not create a node of the subgraph a builtin lowers to |

Every code: [diagnostics](../diagnostics/README.md).

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
        vec3  mixed = lerp(A, B, Alpha = sa);
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
LinearInterpolate(A, B, sa)   -> mixed     (float3: the widest operand)
Normalize(mixed)              -> unit      (float3)
DotProduct(unit, A)           -> d         (float, always)
Add / Multiply chain          -> Color
```

## See also

- [Builtins](index.md) — the call surfaces available inside `Graph`
- [`UE.*` catalogue](ue.md) — every named material-node builtin
- [`UE.Expression`](ue-expression.md) — any `UMaterialExpression`, called by its class
- [Transform builtins](transform.md) — `UE.TransformVector` / `UE.TransformPosition`
- [`Substrate.*`](substrate.md) — Substrate node wrappers (UE 5.4+)
- [`DreamShaderBuiltins.ush`](hlsl-library.md) — the shipped HLSL helper header
- [Expressions and operators](../graph/expressions.md) — `+ - * /`, precedence, operand rules
- [Constructors](../graph/constructors.md) — the type names resolved before builtins
- [Conversions](../graph/conversions.md) — widening and narrowing
- [Calls](../graph/calls.md) — call syntax, named arguments, out arguments
- [Name resolution](../graph/name-resolution.md) — the full lookup order
- [Node reuse](../graph/node-reuse.md) — why repeated calls produce one node
- [Unsupported constructs](../graph/unsupported.md) — `%` and the other absent operators
- [`Function`](../language/function.md) — HLSL bodies, where the full intrinsic set applies
- [Diagnostics](../diagnostics/README.md) — every code
