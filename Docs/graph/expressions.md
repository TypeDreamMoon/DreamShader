# Expressions

> [DreamShader](../index.md) » [Graph](index.md) » **Expressions**

The value-producing grammar of a `Graph` block: literals, identifiers, calls, member access and the
four arithmetic operators, evaluated into `UMaterialExpression` nodes.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration default |
| Kind | expression grammar |
| Generates | `UMaterialExpressionAdd`, `UMaterialExpressionSubtract`, `UMaterialExpressionMultiply`, `UMaterialExpressionDivide` — one node per binary operator; identical ones are merged (see [Notes](#notes)) |

## Synopsis

```c
<expression>     := <additive>
<additive>       := <multiplicative> [ { + | - } <multiplicative> ] …
<multiplicative> := <unary> [ { * | / } <unary> ] …
<unary>          := { + | - } <unary> | <postfix>
<postfix>        := <primary> [ <postfix-op> ] …
<postfix-op>     := . <identifier> | :: <identifier> | ( [ <argument> [ , <argument> ] … ] )
<primary>        := <identifier> | <number-literal> | <string-literal> | ( <additive> )
<argument>       := [ <identifier> = ] <additive>
```

`( ) , . :: = + - * /` are literal DreamShaderLang punctuation. `[ … ]`, `{ a | b }` and `…` are
meta-notation and are never typed.

This is the 1.x grammar. The legacy front end parses an expression with the 2.0 grammar, which is a
superset, and refuses everything outside the grammar above with a code *(since 2.0.0)* — see
[Operators that do not exist](#operators-that-do-not-exist).

## Precedence and associativity

Highest binding first.

| Level | Operators | Arity | Associativity | Notes |
| :-- | :-- | :-- | :-- | :-- |
| 1 | `f(…)` call, `.member`, `::name` | postfix | left | Chains freely: `A::F(x).rgb.b` |
| 2 | `+` `-` | unary, prefix | right | Recursive: `- -x` parses and is legal. `--x` without the space is the decrement operator, [`DSH2207`](../diagnostics/DSH2xxx.md#dsh2207) |
| 3 | `*` `/` | binary | left | `a / b / c` is `(a / b) / c` |
| 4 | `+` `-` | binary | left | `a - b - c` is `(a - b) - c` |
| — | `( … )` grouping | — | — | Overrides the levels above |

## Operators that do not exist

The 1.x expression grammar has no other operators. Each of the following parses, and is then refused:

| Category | Spellings | Code |
| :-- | :-- | :-- |
| Modulo | `%` — use the `fmod` builtin | [`DSH2201`](../diagnostics/DSH2xxx.md#dsh2201) |
| Comparison | `==` `!=` `<` `>` `<=` `>=` — legal **only** in an `if` condition | [`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200) |
| Logical | `&&` `\|\|` `!` | `DSH2200` |
| Bitwise / shift | `&` `\|` `^` `~` `<<` `>>` | `DSH2201` |
| Conditional | `? :` | `DSH2200` |
| Increment / decrement | `++` `--` | `DSH2207` |
| Compound assignment | `+=` `-=` `*=` `/=` and the rest | [`DSH2205`](../diagnostics/DSH2xxx.md#dsh2205) |
| Assignment inside an expression | `=` (outside a named call argument) | [`DSH2204`](../diagnostics/DSH2xxx.md#dsh2204) |
| Indexing | `[ ]` — use a [swizzle](swizzle.md) | [`DSH2202`](../diagnostics/DSH2xxx.md#dsh2202) |
| Cast | `(float3)x` — use a constructor | [`DSH2203`](../diagnostics/DSH2xxx.md#dsh2203) |
| Comma operator | `,` (outside a call argument list) | none of its own: the statement ends there, so what follows is usually [`DSH2154`](../diagnostics/DSH2xxx.md#dsh2154) |
| Matrix types / operators | none exist; see [Constructors](constructors.md#notes) | [`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361) for a matrix value |

The full catalogue, with what to write instead, is in [Unsupported constructs](unsupported.md).

> [!WARNING]
> **1.x truncated these silently.** Its tokenizer mapped every character it did not know (`%`, `!`,
> `<`, `>`, `&`, `|`, `^`, `~`, `?`, `[`, …) to the end of the input, so the parser kept the expression
> before it and dropped the rest without a message. *(since 2.0.0)* the whole expression is read and
> the operator is refused:
>
> | Written | 1.x built | Code now |
> | :-- | :-- | :-- |
> | `a % b` | `a` | `DSH2201` |
> | `a && b` | `a` | `DSH2200` |
> | `a ? b : c` | `a` | `DSH2200` |
> | `v[0]` | `v` | `DSH2202` |
> | `a << 2` | `a` | `DSH2201` |
>
> A character that is no token at all (`@`, `$`, a backtick) is
> [`DSH2101`](../diagnostics/DSH2xxx.md#dsh2101). Parenthesising a suspect expression is no longer
> needed to make a truncation visible.

## Operand rules

The binder applies these tests to both operands of `+ - * /`:

| # | Test | Code when it fails |
| :-- | :-- | :-- |
| 1 | Both operands are numbers or bools — not a texture, a sampler or a `MaterialAttributes` value | [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) |
| 2 | A `Substrate` operand: `+` between two `Substrate` values and `*` by a scalar build `Substrate.Add` / `Substrate.Weight` *(since 2.0.0; 1.x refused both)*; any other operator | [`DSH5293`](../diagnostics/DSH5xxx.md#dsh5293) |
| 3 | The widths agree: the result is as wide as the widest operand, a scalar operand spreads to that width, and a narrower vector does not | `DSH4226` |
| 4 | For `/`: not both operands integer constructors | [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243) — see [Integer division](#integer-division) |

An operator never narrows an operand: `float3 * float4` is `DSH4226`, so that channels are never
dropped silently. *(since 2.0.0)* nor does it widen one: 1.x widened an operand to the width of an
"authoritative" value in some cases; operands are typed now, and the scalar spread is the only way
two widths meet. See [Conversions](conversions.md).

There is no separate scalar-promotion step: a scalar operand is passed to the material node as-is and
Unreal replicates it, so `A * K` with `A` a `vec3` and `K` a `float` produces a single `Multiply`
node, not an `AppendVector` splat.

## Result of a binary operator

| Property | Value |
| :-- | :-- |
| Node | `Add` / `Subtract` / `Multiply` / `Divide` (per operator) |
| Width | the widest operand's |
| Kind | the operands' kinds promoted as in HLSL — an `int` with a `float` is a `float`; in the graph every value is a float |
| All-constant operands | folded into one `Constant` node by the constant-folding pass *(since 2.0.0)* |

## Unary operators

| Form | Lowering | Node cost | Result |
| :-- | :-- | :-- | :-- |
| `+x` | identity | no node | `x` unchanged |
| `-x` | `Multiply(x, Constant(-1))` | one `Multiply` and one `Constant` holding `-1` | the type of `x` |

`!x` is `DSH2200`, `~x` is `DSH2201`, `++x` and `--x` are `DSH2207`.

A negated constant is folded: `-2.0` becomes one `Constant` holding `-2` *(since 2.0.0)*, as does any
arithmetic over constants. See [Constructors](constructors.md#constant-folding).

## Integer division

`/` is the only operator with an extra type rule:
[`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243) when **both** operands are integer constructor calls
(`int`, `int2..4`, `ivec2..4`, `uint`, `uint2..4`, `uvec2..4`). A material graph has no integer
division, so the compiler refuses to pick between HLSL's truncating answer and the graph's fractional
one.

Every other `/` is the graph's float division, as in 1.x, which typed every number literal float —
even where both sides are integers to the compiler: an integer literal (`7`), an `int` / `uint`
variable or property (legacy rule L27). [`dsc migrate`](../tools/migrate.md) writes such a division
as `float(7) / 2`, because a `.dss` refuses every `/` between integers.

| Expression | Result |
| :-- | :-- |
| `int(7) / int(2)` | `DSH4243` |
| `int(7) / 2` | `3.5` *(2.0.0 – 2.1.0 refused it with `DSH4243`)* |
| `7 / 2` | `3.5` *(2.0.0 – 2.1.0 refused it with `DSH4243`)* |
| `7.0 / 2` | `3.5` |
| `float(int(7)) / int(2)` | `3.5` — one operand is a float |
| `int a = 7; int b = 2; a / b` | `3.5` *(2.0.0 – 2.1.0 refused it with `DSH4243`)* |

> [!NOTE]
> `int`, `uint`, `bool` and `half` are kinds of their own to the compiler, and this rule is what the
> difference is for. In the generated graph every one of them is a float — there is no integer
> arithmetic and no truncation. See [Conversions](conversions.md).

## Compound assignment

`+= -= *= /=` are not 1.x operators. *(since 2.0.0)* each is
[`DSH2205`](../diagnostics/DSH2xxx.md#dsh2205), whatever the spacing.

| Written | 1.x read it as | Code now |
| :-- | :-- | :-- |
| `a += b;` | a declaration of a variable `+` of type `a` | `DSH2205` |
| `a+=b;` | an assignment to a new variable named `a+`; `a` unchanged, no message | `DSH2205` |

Write the expansion instead: `a = a + b;`.

## Increment and decrement

`++` and `--` are not 1.x operators. *(since 2.0.0)* every form is
[`DSH2207`](../diagnostics/DSH2xxx.md#dsh2207): `a++;`, `a--;`, `++a;`, `--a;`, and `x = --a;`
inside a larger expression, which 1.x read as `-(-a)`. Write `a = a + 1;` instead.

## Notes

- Identical subexpressions are merged after lowering: the deduplication pass keeps one node for every
  set of nodes with the same operation and the same operands in the same order, so `A + B` written
  twice yields one `Add`, while `B + A` is a second one. Constants are merged the same way. See
  [Node reuse](node-reuse.md).
- Grouping parentheses generate nothing; they only affect parse order.
- A call may be swizzled directly: `UE.TexCoord().x` is a postfix chain of a call followed by a member
  access. See [Swizzle](swizzle.md).
- `::` is the namespace separator in a callee path: `N::F` names the function `F` of
  `Namespace(Name="N")`. It must be followed by a name ([`DSH5260`](../diagnostics/DSH5xxx.md#dsh5260)).
  See [Name resolution](name-resolution.md).
- Named call arguments (`Coordinates = uv`) are part of the argument grammar, not an assignment
  operator. See [Calls](calls.md).

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| [`DSH2151`](../diagnostics/DSH2xxx.md#dsh2151) | an operand was expected and something else was found, e.g. `x = ;` or `a * )` |
| [`DSH2152`](../diagnostics/DSH2xxx.md#dsh2152) | a `)` is missing from a call or a parenthesized expression; also a missing `,` between arguments |
| [`DSH2161`](../diagnostics/DSH2xxx.md#dsh2161) | `.` is not followed by a name |
| `DSH5260` | `::` is not followed by a name |
| `DSH2101` | a character that is no token |
| `DSH2200` | a comparison, a logical operator or `?:` outside an `if` condition |
| `DSH2201` | `%`, a shift or a bitwise operator |
| `DSH2202` | `[ ]` |
| `DSH2203` | a C-style cast |
| `DSH2204` | an assignment inside an expression |
| `DSH2205` | a compound assignment |
| `DSH2207` | `++` or `--` |
| `DSH4226` | an operand is not a number, or the widths do not agree |
| `DSH4243` | both operands of `/` are integer constructor calls |
| `DSH5293` | an operator a `Substrate` value does not have |
| [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200) | an operand name is not declared |
| [`DSH4202`](../diagnostics/DSH4xxx.md#dsh4202) | a string used as a value |
| [`DSH2211`](../diagnostics/DSH2xxx.md#dsh2211) | an expression statement that is not a call or an assignment, e.g. `a + b;` |

The complete list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Docs/M_Expressions")
{
    Properties {
        vec3 A = vec3(1.0, 0.5, 0.2);
        vec3 B = vec3(0.1, 0.2, 0.3);
        float K = 2.0;
    }
    Settings { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs { vec3 Color; Base.EmissiveColor = Color; }
    Graph {
        vec3 Sum    = A + B;
        vec3 Diff   = A - B;
        vec3 Scaled = A * K;
        vec3 Ratio  = A / K;
        vec3 Neg    = -Scaled;
        Color = (Sum + Diff - Scaled + Ratio) * 0.25 + Neg;
    }
}
```

Generated nodes:

```text
VectorParameter A, VectorParameter B             (property nodes)
ScalarParameter K                                (property node)
Add(A, B)                                        -> Sum
Subtract(A, B)                                   -> Diff
Multiply(A, K)                                   -> Scaled
Divide(A, K)                                     -> Ratio
Multiply(Scaled, Constant(-1))                   -> Neg
Add / Subtract / Add chain, Multiply(..., 0.25), Add(..., Neg)
```

## See also

- [Statements](statements.md) — the statement forms an expression can appear in
- [Unsupported constructs](unsupported.md) — every refused construct and its code
- [Literals](literals.md) — numeric, string and boolean literal forms
- [Constructors](constructors.md) — `float3(…)`, `vec4(…)` and the integer constructors
- [Swizzle](swizzle.md) — `.rgb`, `.bgr`, channel masks
- [Conversions](conversions.md) — widening, narrowing and kinds
- [`if` / `else`](if.md) — the only place comparison operators are accepted
- [Calls](calls.md) — call syntax, named arguments, out arguments
- [Name resolution](name-resolution.md) — how an identifier or callee is looked up
- [Node reuse](node-reuse.md) — why repeated subexpressions produce one node
- [Math builtins](../builtins/math.md) — `fmod`, `pow`, `min`, `max` and the rest
