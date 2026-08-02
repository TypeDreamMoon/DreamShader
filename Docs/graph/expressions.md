# Expressions

> [DreamShader](../index.md) » [Graph](index.md) » **Expressions**

The value-producing grammar of a `Graph` block: literals, identifiers, calls, member access and the
four arithmetic operators, evaluated into `UMaterialExpression` nodes.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration default |
| Kind | expression grammar |
| Generates | `UMaterialExpressionAdd`, `UMaterialExpressionSubtract`, `UMaterialExpressionMultiply`, `UMaterialExpressionDivide` — one node per binary operator that is not served from the reuse cache |

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

## Precedence and associativity

Highest binding first. The parser implements exactly these four levels.

| Level | Operators | Arity | Associativity | Notes |
| :-- | :-- | :-- | :-- | :-- |
| 1 | `f(…)` call, `.member`, `::name` | postfix | left | Chains freely: `A::F(x).rgb.b` |
| 2 | `+` `-` | unary, prefix | right | Recursive: `--x` parses and is legal |
| 3 | `*` `/` | binary | left | `a / b / c` is `(a / b) / c` |
| 4 | `+` `-` | binary | left | `a - b - c` is `(a - b) - c` |
| — | `( … )` grouping | — | — | Overrides the levels above |

## Operators that do not exist

The expression grammar has no other operators. None of the following are implemented at expression
level:

| Category | Spellings absent from the grammar |
| :-- | :-- |
| Modulo | `%` — use the `fmod` / `mod` builtin |
| Comparison | `==` `!=` `<` `>` `<=` `>=` — legal **only** in an `if` condition |
| Logical | `&&` `\|\|` `!` |
| Bitwise / shift | `&` `\|` `^` `~` `<<` `>>` |
| Conditional | `? :` |
| Increment / decrement | `++` `--` |
| Compound assignment | `+=` `-=` `*=` `/=` |
| Assignment inside an expression | `=` (outside a named call argument) |
| Indexing | `[ ]` — use a [swizzle](swizzle.md) |
| Comma operator | `,` (outside a call argument list) |
| Matrix types / operators | none exist; see [Constructors](constructors.md#notes) |

Their failure modes differ, and several of them fail *silently*. The complete catalogue with the exact
observable behaviour of each is in [Unsupported constructs](unsupported.md).

> [!WARNING]
> **Unknown characters terminate an expression silently.** The tokenizer maps every character it does
> not recognise to the end-of-input token — that is `%`, `!`, `<`, `>`, `&`, `|`, `^`, `~`, `?`, `[`,
> `]`, `{`, `}`, `;`, `#`, `@`, `$`, `'`, the backtick, and a lone `:` — and the parser accepts an
> expression that is followed by end-of-input. Anything after the first unknown character is therefore
> **discarded without a diagnostic**:
>
> | Written | Actually compiled |
> | :-- | :-- |
> | `a % b` | `a` |
> | `a && b` | `a` |
> | `a ? b : c` | `a` |
> | `v[0]` | `v` |
> | `a << 2` | `a` |
>
> Two contexts do report it. In *leading* position the unknown character is rejected with
> `Unexpected token '{Text}' in Graph expression.` (so `!x` errors), and inside parentheses the
> missing `)` is rejected with `Expected token type {TypeId} in Graph expression near '{Text}'.` (so
> `(a % b)` errors while `a % b` does not). Wrapping a suspect expression in parentheses is the
> reliable way to make truncation visible.

## Operand rules

`CreateBinaryOperatorNode` applies these tests in order to both operands of `+ - * /`.

| # | Test | Outcome when it fails |
| :-- | :-- | :-- |
| 1 | Neither operand is a texture object | `Arithmetic operators cannot be applied to texture values.` |
| 2 | Neither operand is a `MaterialAttributes` value | `Arithmetic operators cannot be applied to MaterialAttributes values.` |
| 3 | Neither operand is a `Substrate` value | `Arithmetic operators cannot be applied to Substrate values.` |
| 4 | Component counts are equal, **or** either operand is a scalar (1 component) | one rescue attempt (rule 5), then rule 6 |
| 5 | Rescue: if exactly one operand carries an authoritative component count, the other may be widened to it | rescue skipped |
| 6 | Compatibility re-tested | `Operator '{Op}' requires matching vector sizes or a scalar/vector pair, got {Left} and {Right} component(s).` |

Rule 5 in full: the widened operand must be the one **without** an authoritative count, the
authoritative count must be greater than zero, and the other operand's count must be **less than or
equal to** the authoritative count. Narrowing is never attempted here — dropping channels at an
operator is deliberately refused so the size error surfaces instead. See
[Conversions](conversions.md#authoritative-component-counts).

There is no separate scalar-promotion step: a scalar operand is passed to the material node as-is and
Unreal replicates it, so `A * K` with `A` a `vec3` and `K` a `float` produces a single `Multiply`
node, not an `AppendVector` splat.

## Result of a binary operator

| Property | Value |
| :-- | :-- |
| Node | `Add` / `Subtract` / `Multiply` / `Divide` (per operator) |
| Component count | `max(left, right)` after rule 5 |
| Authoritative component count | set when **either** operand had one |
| Input channel mask | cleared — the result is a full-width value |
| Integer marker | **not** propagated; the result of any operator is non-integer |

## Unary operators

| Form | Lowering | Node cost | Result |
| :-- | :-- | :-- | :-- |
| `+x` | identity | no node | `x` unchanged, all flags preserved |
| `-x` | `Multiply(x, Constant(-1))` | one `Multiply`, plus one `Constant` (shared with every other `-1` in the graph) | component count of `x` |

Any other unary operator token cannot reach evaluation through the grammar; the defensive message is
`Unsupported unary operator '{Operator}'.`

`-2.0` is *not* folded into a negative literal by the expression evaluator: it becomes
`Constant(2.0) * Constant(-1)`. Inside a **constructor argument** it is folded, because constant
folding accepts a leading `+`/`-` on a literal — `vec3(-1.0, 0.0, 1.0)` emits a single
`Constant3Vector`. See [Constructors](constructors.md#constant-folding).

## Integer division

`/` is the only operator with an extra type rule.

```text
Integer division is not supported by the material graph; use float() or floor(a/b).
```

It fires when **both** operands carry the integer marker. That marker is set by exactly one thing: a
direct call to an integer constructor (`int`, `int2..4`, `ivec2..4`, `uint`, `uint2..4`, `uvec2..4`).
It is not set by literals, by variables, by suffixed literals such as `3u`, or by the result of any
operator.

| Expression | Result |
| :-- | :-- |
| `int(7) / int(2)` | error |
| `int(7) / 2` | allowed — `2` is a literal and carries no integer marker |
| `7 / 2` | allowed — a float `Divide` producing `3.5` |
| `float(int(7)) / int(2)` | allowed — every constructor call assigns the marker from its own name, so `float(…)` clears it |
| `int a = int(7); int b = int(2); a / b` | error — the marker travels with the variable |
| `int a = 7; int b = 2; a / b` | allowed — the `int` *declaration type* does not set the marker; only an `int(…)` *call* does |

> [!NOTE]
> The marker exists solely to reject this case. `int`, `uint`, `bool` and `half` type tokens are
> otherwise indistinguishable from `float` in the generated graph — there is no integer arithmetic and
> no truncation. See [Conversions](conversions.md#float-int-and-bool).

## Compound assignment

`+= -= *= /=` are not operators. A statement containing one is classified by the ordinary
declaration/assignment splitter, and the result depends on **whitespace**.

| Written | How it is classified | Observable behaviour |
| :-- | :-- | :-- |
| `a += b;` | first top-level `=` splits the statement; the left side `a +` splits on its last whitespace into type `a`, name `+` | error: `Unsupported Graph variable type 'a' for '+'.` |
| `a -= b;` | same, name `-` | error: `Unsupported Graph variable type 'a' for '-'.` |
| `a *= b;` | same, name `*` | error: `Unsupported Graph variable type 'a' for '*'.` |
| `a /= b;` | same, name `/` | error: `Unsupported Graph variable type 'a' for '/'.` |
| `a+=b;` | the left side `a+` contains no whitespace, so it is not a declaration | **no error** — a new Graph variable literally named `a+` is created and assigned `b`; `a` is unchanged |
| `a += b ;` | as the spaced form | error |

> [!WARNING]
> The spaceless forms `a+=b`, `a-=b`, `a*=b`, `a/=b` compile without any diagnostic and have no effect
> on `a`. Write the expansion instead: `a = a + b;`.

## Increment and decrement

`++` and `--` are not operators either. There is no postfix operator, so the tokens are read as two
consecutive `+` or `-` signs and the statement fails while the expression is still being parsed —
before it can be classified as a statement.

| Written | How it parses | Observable behaviour |
| :-- | :-- | :-- |
| `a++;` | `a` `+` `+` `<end>` — the second `+` is taken as a unary sign whose operand is the end of input | error: `Unexpected token '' in Graph expression.` |
| `a--;` | `a` `-` `-` `<end>`, same shape | error: `Unexpected token '' in Graph expression.` |
| `++a;` | parses as `+(+a)`, a unary expression, which is not a call | error: `Graph expression statements currently support only Function calls with explicit out arguments.` |
| `--a;` | parses as `-(-a)`, likewise not a call | error: `Graph expression statements currently support only Function calls with explicit out arguments.` |

> [!NOTE]
> The empty quotes in `Unexpected token '' in Graph expression.` are not a formatting fault: the
> reported token is the end-of-input token, whose text is empty.

Used inside a larger expression the prefix forms do parse — `x = --a;` is `-(-a)`, which generates
two `Multiply` nodes and yields `a` unchanged. Write `a = a + 1;` instead.

## Notes

- Every operand is evaluated exactly once per distinct value; textually identical subexpressions over
  identical operand values collapse to a single node. `A + B` written twice yields one `Add`. See
  [Node reuse](node-reuse.md).
- Grouping parentheses generate nothing; they only affect parse order.
- A call may be swizzled directly: `UE.TexCoord().x` is a postfix chain of a call followed by a member
  access. See [Swizzle](swizzle.md#swizzling-a-call-result).
- `::` is the namespace separator in a callee path and is only valid before an identifier; it is not a
  general operator. See [Name resolution](name-resolution.md).
- Named call arguments (`Coordinates = uv`) are part of the argument grammar, not an assignment
  operator. Argument names are matched case-insensitively and ignore surrounding whitespace. See
  [Calls](calls.md).

## Diagnostics

Format specifiers are rendered as `{Placeholder}` throughout this page; the compiler emits the
substituted text.

| Message | Cause |
| :-- | :-- |
| `Unexpected token '{Text}' in Graph expression.` | A non-primary token in primary position (`!x`, `= b`), or a leftover real token after a complete expression (`a == b`). Not emitted for unknown characters in trailing position. |
| `Expected token type {TypeId} in Graph expression near '{Text}'.` | A `)` or `,` was required and not found — most often an unknown character inside a parenthesised expression or an argument list. |
| `Expected member name after '.'.` | `.` not followed by an identifier. |
| `Expected function name after '::'.` | `::` not followed by an identifier. |
| `Empty Graph expression.` | The expression text is empty after trimming. |
| `Unsupported Graph expression kind.` | Defensive guard for an expression node the evaluator does not handle. |
| `Unsupported unary operator '{Operator}'.` | Defensive guard; unreachable through the grammar. |
| `Arithmetic operators cannot be applied to texture values.` | A texture object used as an operand of `+ - * /`. |
| `Arithmetic operators cannot be applied to MaterialAttributes values.` | A `MaterialAttributes` value used as an operand. |
| `Arithmetic operators cannot be applied to Substrate values.` | A `Substrate` value used as an operand. |
| `Operator '{Op}' requires matching vector sizes or a scalar/vector pair, got {Left} and {Right} component(s).` | Operand widths differ, neither is a scalar, and the authoritative-count rescue did not apply. |
| `Integer division is not supported by the material graph; use float() or floor(a/b).` | Both operands of `/` came from integer constructors. |
| `Unsupported or failed binary operator '{Operator}'.` | The operator is not one of `+ - * /`, or the material node could not be created. |
| `Unknown Graph identifier '{Name}'.` | An operand name is neither a Graph variable, a declared property, nor `true`/`false`. |
| `Unsupported Graph variable type '{Type}' for '{Name}'.` | Compound-assignment form with whitespace, or any other statement whose left side splits into an unrecognised type token and a name. |
| `Graph expression statements currently support only Function calls with explicit out arguments.` | `++a;`, `break;`, or any bare expression statement that parses but is not a call. |
| `Unexpected token '' in Graph expression.` | The expression ended while an operand was still expected — `a++;`, `a--;`, or a trailing operator. The quoted token is the empty end-of-input token. |

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

Generated nodes (grouped by the reuse cache):

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
- [Unsupported constructs](unsupported.md) — every rejected and every silently-truncated construct
- [Literals](literals.md) — numeric, string and boolean literal forms
- [Constructors](constructors.md) — `float3(…)`, `vec4(…)` and the integer constructors
- [Swizzle](swizzle.md) — `.rgb`, `.bgr`, channel masks
- [Conversions](conversions.md) — widening, narrowing and authoritative component counts
- [`if` / `else`](if.md) — the only place comparison operators are accepted
- [Calls](calls.md) — call syntax, named arguments, out arguments
- [Name resolution](name-resolution.md) — how an identifier or callee is looked up
- [Node reuse](node-reuse.md) — why repeated subexpressions produce one node
- [Math builtins](../builtins/math.md) — `fmod`, `pow`, `min`, `max` and the rest
