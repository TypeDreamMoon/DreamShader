# Unsupported constructs

> [DreamShader](../index.md) » [Graph](index.md) » **Unsupported constructs**

Every HLSL and GLSL construct that a `Graph` block does **not** implement, what actually happens when
one is written, and what to write instead.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — applies to every `Graph { … }` body, `Outputs` binding expression and `Outputs` declaration default |
| Kind | reference of rejected and silently-accepted syntax |

A `Graph` block is a **node-graph builder**, not a shader compiler. It has a fixed set of statement
forms, four arithmetic operators and no control flow other than `if`. Anything else either fails with a message
that names a *variable type* rather than the construct, or — for a large and important set of cases —
compiles silently into something smaller than what was written.

## Two failure mechanisms

Understanding these two rules explains every row in the tables below.

**1. The statement classifier turns unknown syntax into a declaration.** There is no keyword table and
no "unsupported keyword" diagnostic. A statement with no top-level `=` is split at its **last
top-level whitespace** into a type token and a name; that split succeeds for almost any text, so
`return Color`, `while (t) {}` and `struct S { float a; }` all become declarations of an unresolvable
type. Parenthesis depth and string literals are tracked during that split — brace depth is **not**.

**2. The tokenizer maps every unknown character to end-of-input.** The expression parser accepts an
expression that is followed by end-of-input, so anything after the first unknown character is
discarded without a diagnostic.

## Silently truncated expressions

> [!WARNING]
> The tokenizer knows only letters, digits, `_`, `.`, `"`, whitespace, the punctuation
> `( ) , + - * / =` and the two-character `::`. **Every other character becomes the end-of-input
> token** — `%`, `!`, `<`, `>`, `&`, `|`, `^`, `~`, `?`, `[`, `]`, `{`, `}`, `;`, `#`, `@`, `$`, `'`,
> the backtick, a lone `:`, and anything else. The parser then accepts the expression that precedes it
> and throws the rest away **without any message**.
>
> | Written | Actually compiled | Write instead |
> | :-- | :-- | :-- |
> | `a % b` | `a` | `fmod(a, b)` or `mod(a, b)` |
> | `a && b` | `a` | nested `if` statements |
> | `a \|\| b` | `a` | nested `if` statements, or `max` on 0/1 masks |
> | `a & b`, `a \| b`, `a ^ b` | `a` | a `Function` with an HLSL body |
> | `a << 2`, `a >> 2` | `a` | `a * 4.0`, `a / 4.0`, or a `Function` |
> | `a < b`, `a > b`, `a <= b`, `a >= b`, `a != b` | `a` | an [`if` condition](if.md#operators) |
> | `a ? b : c` | `a` | `if` / `else`, `lerp(c, b, mask)`, or a `StaticSwitchParameter` call |
> | `v[0]` | `v` | a [swizzle](swizzle.md): `v.x` |
> | `~a` *(trailing)* | `a` | a `Function` with an HLSL body |
>
> ```c
> // Written — the modulo is discarded, Wave is just Time.
> float Wave = UE.Time() % 1.0;
>
> // Actually compiled:
> float Wave = UE.Time();
>
> // Correct:
> float Wave = fmod(UE.Time(), 1.0);
> ```
>
> ```c
> // Written — the green test is discarded.
> if (Color.r > 0.5 && Color.g > 0.5) { Out = One; } else { Out = Zero; }
>
> // Actually compiled:
> if (Color.r > 0.5) { Out = One; } else { Out = Zero; }
>
> // Correct:
> if (Color.r > 0.5) {
>     if (Color.g > 0.5) { Out = One; } else { Out = Zero; }
> } else {
>     Out = Zero;
> }
> ```

Two positions **do** report the problem, and both are worth exploiting:

| Position | Message |
| :-- | :-- |
| Leading — the unknown character starts the expression, as in `!x` or `~x` | `Unexpected token '{Char}' in Graph expression.` |
| Inside parentheses or an argument list — the expected `)` or `,` is never found, as in `(a % b)` or `f(a % b)` | `Expected token type {TypeId} in Graph expression near '{Text}'.` |

> [!NOTE]
> Wrapping a suspect expression in parentheses is the reliable way to make truncation visible:
> `float x = a % b;` compiles silently, `float x = (a % b);` errors.

## Statement-level constructs

None of these are keywords. Each is classified as a declaration, and the reported "variable type" is
whatever text preceded the last top-level whitespace.

| Written | Message | Write instead |
| :-- | :-- | :-- |
| `return Color;` | `Failed to declare Graph variable 'Color'. Unsupported Graph variable type 'return'.` | assign the declared output variable: `Color = …;` |
| `return;` | `Graph expression statements currently support only Function calls with explicit out arguments.` | as above |
| `for (int i = 0; i < 3; i = i + 1) {}` | `Failed to declare Graph variable '{}'. Unsupported Graph variable type 'for (int i = 0; i < 3; i = i + 1)'.` | unroll by hand, or move the loop into a [`Function`](../language/function.md) whose body is real HLSL |
| `while (t) {}` | `Failed to declare Graph variable '{}'. Unsupported Graph variable type 'while (t)'.` | as above |
| `do {} while (t);` | `Failed to declare Graph variable '(t)'. Unsupported Graph variable type 'do {} while'.` | as above |
| `switch (Mode) {}` | `Failed to declare Graph variable '{}'. Unsupported Graph variable type 'switch (Mode)'.` | nested [`if`](if.md) statements, or a [`StaticSwitchParameter` call](calls.md#staticswitchparameter) |
| `break;` | `Graph expression statements currently support only Function calls with explicit out arguments.` | — |
| `continue;` | `Graph expression statements currently support only Function calls with explicit out arguments.` | — |
| `float Foo(float x) { return x; }` | `Failed to declare Graph variable '}'. Unsupported Graph variable type 'float Foo(float x) { return x;'.` | declare it at top level as a [`Function`](../language/function.md) or [`GraphFunction`](../language/graph-function.md) and call it |
| `struct S { float a; }` | `Failed to declare Graph variable '}'. Unsupported Graph variable type 'struct S { float a;'.` | use [`MaterialAttributes`](material-attributes.md), or a struct inside a `Function` body |
| `#define K 2` | `Failed to declare Graph variable '2'. Unsupported Graph variable type '#define K'.` | a `const` property, or a preprocessor directive inside a `Function` body |
| `#include "X.ush"` | `Failed to declare Graph variable '"X.ush"'. Unsupported Graph variable type '#include'.` | [`import`](../language/import.md) at file level, or a `Function` that uses the generated include |

> [!WARNING]
> A braced construct carries no top-level `;`, so the statement splitter does not stop at its closing
> `}`; it keeps scanning to the next top-level `;`. A loop followed by more code therefore swallows
> that code into the same statement, and the quoted type and name in the message contain unrelated
> text. Given
>
> ```c
> for (int i = 0; i < 3; i = i + 1) { Sum = Sum + 1.0; }
> Color = Sum;
> ```
>
> the whole thing is one statement, split at the `=` of `Color`, and the message is
> `Unsupported Graph variable type 'for (int i = 0; i < 3; i = i + 1) { Sum = Sum + 1.0; }' for 'Color'.`
> The messages in the table above assume the construct stands alone.

## Expression-level constructs that do error

| Written | Message |
| :-- | :-- |
| `!x` | `Unexpected token '!' in Graph expression.` |
| `~x` | `Unexpected token '~' in Graph expression.` |
| `x = a == b;` | `Unexpected token '=' in Graph expression.` — `=` is a real token, so `==` is *not* truncated |
| `a == b;` *(as a statement)* | `In Graph statement 'a == b': Unexpected token '=' in Graph expression.` |
| `x = a++;` | `Unexpected token '' in Graph expression.` — the postfix `+` demands an operand and finds end-of-input |
| `x = (float)a;` | `Unexpected token 'a' in Graph expression.` — there are no C-style casts; write `float(a)` |
| `x = 0x10;` | `Unexpected token 'x10' in Graph expression.` — there are no hex, octal or binary literals |
| `x = 1.0fx;` | `Unexpected token 'fx' in Graph expression.` — a numeric suffix is only consumed at an identifier boundary |
| `x = "text";` | `String literals can only be used in named UE builtin arguments.` |
| `float4 v = {{1,2},{3,4}};` | `Invalid brace initializer for type 'float4'. Unexpected token '{' in Graph expression.` — brace initializers do not nest |
| `f(a b)` | `Expected token type {TypeId} in Graph expression near '{Text}'.` — a missing `,` |
| `if (a) Color = X;` | `Graph if statement is missing a '{ ... }' body.` — braces are mandatory |

## Compound assignment and increment

`+= -= *= /= ++ --` are not operators. What happens depends on **whitespace**, because the statement
is split at its first top-level `=` and the left side is then tested for a type/name split.

| Written | Result |
| :-- | :-- |
| `a += b;` | `Unsupported Graph variable type 'a' for '+'.` |
| `a -= b;` | `Unsupported Graph variable type 'a' for '-'.` |
| `a *= b;` | `Unsupported Graph variable type 'a' for '*'.` |
| `a /= b;` | `Unsupported Graph variable type 'a' for '/'.` |
| `a++;` | `Graph expression statements currently support only Function calls with explicit out arguments.` |
| `a--;` | the same message |

> [!WARNING]
> **The spaceless forms compile with no diagnostic and no effect.** `a+=b;` has no whitespace on the
> left of the `=`, so it is not a declaration — it is a plain assignment to a brand-new Graph variable
> literally named `a+`. `a` is unchanged and nothing warns.
>
> ```c
> // Written — silently creates a variable named 'a+'; 'a' never changes.
> Sum+=Tint;
>
> // Correct:
> Sum = Sum + Tint;
> ```
>
> The same applies to `a-=b`, `a*=b` and `a/=b`.

> [!WARNING]
> **Prefix `++` and `--` parse as repeated unary operators and do nothing.** `++a` is `+(+a)`, which is
> the identity and emits no node. `--a` is `-(-a)`, which emits two `Multiply` nodes by `-1` and is
> also numerically `a`. Neither increments anything.
>
> ```c
> // Written — Counter is unchanged, and two Multiply nodes are generated.
> float Next = --Counter;
>
> // Correct:
> float Next = Counter - 1.0;
> ```

## Wrong-case keywords

`if` and `else` are the only keywords in the Graph language and both are matched **case-sensitively**.
A mis-cased spelling is not a keyword and falls into the declaration classifier.

| Written | Result |
| :-- | :-- |
| `If (x) {}` | `Failed to declare Graph variable '{}'. Unsupported Graph variable type 'If (x)'.` |
| `IF (x) {}` | the same shape |
| `if (x) {} Else {}` | the `if` parses; `Else {}` becomes a separate statement and reports `Failed to declare Graph variable '{}'. Unsupported Graph variable type 'Else'.` |
| `if (x) {} ELSE {}` | the same shape |

Everything else in the language is case-insensitive — type tokens, constructor names, builtins,
`true`/`false`, swizzle channels, argument names. The one other case-sensitive name is
`SampleTexture2D`; see [Name resolution](name-resolution.md#identifier-case).

## Features that simply do not exist

| Feature | Status | Alternative |
| :-- | :-- | :-- |
| Matrix types (`float3x3`, `float4x4`, `mat2`, `mat3`, `mat4`) | absent from the Graph type, constructor and function-signature sets — a matrix-typed `Function` parameter or result is rejected at the call site with `uses unsupported type '{Type}'` | [`UE.TransformVector` / `UE.TransformPosition`](../builtins/transform.md), or matrix locals **inside** a `Function` body |
| Arrays and indexing | absent | [swizzles](swizzle.md) for channels; `Texture2DArray` sampling for layers |
| `inout` parameters | absent — only `in` and `out` | pass an `in` and an `out` |
| Function declarations inside `Graph` | absent | top-level `Function` / `GraphFunction` |
| Integer arithmetic | absent — `int`, `uint`, `bool` and `half` collapse to float widths | the integer marker exists only to reject `int(a) / int(b)`; see [Expressions](expressions.md#integer-division) |
| Hex, octal and binary literals | absent | decimal literals; see [Literals](literals.md) |
| String values | absent outside named `UE.*` arguments | — |
| Ternary conditional | absent | `if` / `else`, `lerp`, `StaticSwitchParameter` |
| Comma operator | absent | separate statements |
| Assignment inside an expression | absent | separate statements |
| Nested brace initializers | absent | a constructor call: `float4(float2(1,2), float2(3,4))` |
| Preprocessor directives | absent | `import`, `const` properties, `#Region` for layout only |
| `Code = { … }` inside a `Shader` | **removed** — hard error `Shader graph sections now use Graph = { ... }. Function Code = { ... } is still supported.` | `Graph = { … }` |

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

| Message | Cause |
| :-- | :-- |
| `Failed to declare Graph variable '{Name}'. {Error}` | A declaration with no initializer whose type token could not be resolved — the wrapper around most unsupported-construct failures. |
| `Unsupported Graph variable type '{Type}'.` | The inner message of the above. |
| `Unsupported Graph variable type '{Type}' for '{Name}'.` | A declaration **with** an initializer whose type token could not be resolved — the compound-assignment case and any construct that happens to contain a top-level `=`. |
| `Graph expression statements currently support only Function calls with explicit out arguments.` | A statement that is a bare expression rather than a call: `break;`, `continue;`, `return;`, `a++;`, `x;`. |
| `Graph expression statements must call a named Function.` | A statement call whose callee is not a name. |
| `Unexpected token '{Text}' in Graph expression.` | A non-primary token in primary position, or a real token left over after a complete expression. Never emitted for an unknown character in trailing position. |
| `Expected token type {TypeId} in Graph expression near '{Text}'.` | A `)` or `,` was required and not found — the usual symptom of an unknown character inside parentheses. |
| `Expected member name after '.'.` | `.` not followed by an identifier. |
| `Expected function name after '::'.` | `::` not followed by an identifier. |
| `Invalid numeric literal '{Text}'.` | A number token that converts to zero while containing a character no spelling of zero can contain — in practice an underflowing exponent such as `1e-9999`. `0.5.5` compiles as `0.5` instead of reporting this — see [Literals](literals.md). |
| `String literals can only be used in named UE builtin arguments.` | A string used as a value. |
| `Invalid brace initializer for type '{Type}'. {Error}` | A brace initializer whose contents are not a valid constructor argument list. |
| `Initializer '{Text}' is not a valid brace initializer.` | Text that reached the brace-initializer path without `{ … }` delimiters. |
| `Encountered an invalid empty Graph statement.` | Defensive guard for a statement with no expression, no declaration and no brace initializer. |
| `Encountered a Graph assignment without a target variable.` | A statement such as `= 5;`. |
| `Graph if statement is missing a '{ ... }' body.` | A braceless `if` body. |
| `Shader graph sections now use Graph = { ... }. Function Code = { ... } is still supported.` | A `Code` section inside a `Shader` block. |
| `Unknown Graph identifier '{Name}'.` | A name that resolved on no surface — including `default` outside a material-function call. |

## Example

A GLSL-shaped attempt and its DreamShaderLang equivalent.

```c
// Does not do what it looks like.
Graph {
    vec2  uv    = UE.TexCoord(Index = 0);
    float t     = UE.Time() % 4.0;                 // silently just UE.Time()
    float mask  = uv.x > 0.5 && uv.y > 0.5;        // silently just uv.x
    vec3  col   = mask ? Hot : Cold;               // silently just mask
    col        *= Gain;                            // error: type 'col' for '*'
    Color = col;
}
```

```c
// Correct.
Graph {
    vec2  uv = UE.TexCoord(Index = 0);
    float t  = fmod(UE.Time(), 4.0);

    vec3 col;
    if (uv.x > 0.5) {
        if (uv.y > 0.5) { col = Hot; } else { col = Cold; }
    } else {
        col = Cold;
    }

    Color = col * Gain;
}
```

Anything genuinely imperative — a loop, a bitwise mask, a `switch` — belongs in a `Function`, whose
body is real HLSL and is compiled into a `Custom` node:

```c
Function SelfContained float Ring(in float2 uv, in float count)
{
    float acc = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        acc += sin(uv.x * count * (i + 1));
    }
    return acc * 0.25;
}
```

```c
Graph {
    float r = Ring(uv, 8.0);
}
```

## See also

- [Statements](statements.md) — the statement forms that *are* supported
- [Expressions](expressions.md) — the four operators that exist, and their precedence
- [`if` / `else`](if.md) — the only control flow, and the condition operators
- [Literals](literals.md) — accepted numeric forms, suffixes, and what is not a literal
- [Constructors](constructors.md) — the replacement for casts and matrix construction
- [Swizzle](swizzle.md) — the replacement for indexing
- [Calls](calls.md) — moving imperative code into a callable function
- [`Function`](../language/function.md) — HLSL bodies, where loops and bitwise operators are legal
- [`GraphFunction`](../language/graph-function.md) — HLSL bodies with `UE.*` nodes hoisted into pins
- [Math builtins](../builtins/math.md) — `fmod`, `mod`, `min`, `max`, `clamp`, `lerp`
- [Name resolution](name-resolution.md) — the case-sensitivity matrix
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
