# Unsupported constructs

> [DreamShader](../index.md) » [Graph](index.md) » **Unsupported constructs**

Every HLSL and GLSL construct that a `Graph` block does **not** implement, the code the compiler
reports for it, and what to write instead.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — applies to every `Graph { … }` body, `Outputs` binding expression and `Outputs` declaration default |
| Kind | reference of refused syntax |

A `Graph` block is a **node-graph builder**, not a shader compiler. It has a fixed set of statement
forms, four arithmetic operators and no control flow other than `if`. Anything else is refused with
a diagnostic code. Nothing is compiled into something smaller than what was written *(since 2.0.0)*:
the 1.x generator silently dropped the rest of an expression at the first operator it did not know
(`a % b` built `a`), and every such construct is now an error whose message says what 1.x did.

To write loops, `?:`, `&&` or bitwise operators, move the code into a [`Function`](../language/function.md)
or migrate the file to `.dss` with [`dsc migrate`](../tools/migrate.md), where they exist.

## How a construct is refused

The legacy front end reads a `Graph` body with the 2.0 statement and expression parser, whose grammar
is a superset of the 1.x one, and then refuses what 1.x did not have *(since 2.0.0)*:

| Stage | What it refuses | Codes |
| :-- | :-- | :-- |
| Lexer | a character that is no token at all (`@`, `$`, a backtick), a malformed number (`0.5.5`, `1.0fx`) | [`DSH2101`](../diagnostics/DSH2xxx.md#dsh2101), [`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105) |
| Parser | text that is not a statement or expression of the 2.0 grammar: a missing `;`, `)` or name, a `switch` | `DSH2150`–`DSH2165` |
| Legacy restriction pass | a construct the 2.0 grammar has and 1.x did not: operators, `[ ]`, casts, loops, `return`, compound assignment | [`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200)–[`DSH2222`](../diagnostics/DSH2xxx.md#dsh2222) |
| Binder | a construct that parses but has no meaning: a string value, an integer division, mismatched widths | `DSH42xx` |

Every diagnostic carries the line and column of the construct. A stage reports all of its errors,
and a stage with errors stops the build before the next one.

## Operators 1.x truncated

Each of these was accepted by 1.x and built only the part before the operator, without a message.
*(since 2.0.0)* each is an error.

| Written | Code | Write instead |
| :-- | :-- | :-- |
| `a % b` | [`DSH2201`](../diagnostics/DSH2xxx.md#dsh2201) | `fmod(a, b)` |
| `a && b`, `a \|\| b`, `!a` | [`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200) | nested `if` statements, or `max` / `min` on 0/1 masks |
| `a & b`, `a \| b`, `a ^ b`, `~a` | `DSH2201` | a `Function` with an HLSL body |
| `a << 2`, `a >> 2` | `DSH2201` | `a * 4.0`, `a / 4.0`, or a `Function` |
| `a < b`, `a > b`, `a <= b`, `a >= b`, `a == b`, `a != b` outside an `if` condition | `DSH2200` | an [`if` condition](if.md#operators) |
| `a ? b : c` | `DSH2200` | `if` / `else`, `lerp(c, b, mask)`, or a `StaticSwitchParameter` call |
| `v[0]` | [`DSH2202`](../diagnostics/DSH2xxx.md#dsh2202) | a [swizzle](swizzle.md): `v.x` |
| a second comparison, `&&` or `\|\|` in an `if` condition | [`DSH2210`](../diagnostics/DSH2xxx.md#dsh2210) | nested `if` statements |

```c
// Refused (DSH2201); 1.x built this as UE.Time().
float Wave = UE.Time() % 1.0;

// Correct:
float Wave = fmod(UE.Time(), 1.0);
```

```c
// Refused (DSH2210); 1.x built this as if (Color.r > 0.5).
if (Color.r > 0.5 && Color.g > 0.5) { Out = One; } else { Out = Zero; }

// Correct:
if (Color.r > 0.5) {
    if (Color.g > 0.5) { Out = One; } else { Out = Zero; }
} else {
    Out = Zero;
}
```

The restriction pass reports a refused construct once and does not look inside it, so `a % b % c`
is one diagnostic; `a % b + c % d` is two.

## Statement-level constructs

| Written | Code | Write instead |
| :-- | :-- | :-- |
| `return Color;`, `return;` | [`DSH2208`](../diagnostics/DSH2xxx.md#dsh2208) | assign the declared output variable: `Color = …;` |
| `for (int i = 0; i < 3; i = i + 1) {}` | `DSH2208` | unroll by hand, or move the loop into a [`Function`](../language/function.md) whose body is real HLSL |
| `while (t) {}`, `do {} while (t);` | `DSH2208` | as above |
| `break;`, `continue;`, `discard;` | `DSH2208` | — |
| `switch (Mode) {}` | [`DSH2160`](../diagnostics/DSH2xxx.md#dsh2160) | nested [`if`](if.md) statements, or a [`StaticSwitchParameter` call](calls.md#staticswitchparameter) |
| `float Foo(float x) { return x; }` | [`DSH2154`](../diagnostics/DSH2xxx.md#dsh2154), at the `(` where the declaration's `;` belongs | declare it at top level as a [`Function`](../language/function.md) or [`GraphFunction`](../language/graph-function.md) and call it |
| `struct S { float a; }` | [`DSH2151`](../diagnostics/DSH2xxx.md#dsh2151): `struct` cannot start an expression | use [`MaterialAttributes`](material-attributes.md), or a struct inside a `Function` body |
| `{ … }` with no `if` / `else` in front | [`DSH2220`](../diagnostics/DSH2xxx.md#dsh2220) | drop the braces |
| `static` / `const` on a variable | [`DSH2212`](../diagnostics/DSH2xxx.md#dsh2212) | a `const` property |
| `float w[4];` | [`DSH2213`](../diagnostics/DSH2xxx.md#dsh2213) | separate variables |
| `a + b;` — a value nothing receives | [`DSH2211`](../diagnostics/DSH2xxx.md#dsh2211) | assign it |
| `#include "X.ush"`, `#pragma …` | [`DSH1035`](../diagnostics/DSH1xxx.md#dsh1035): the preprocessor of a `.dsm` / `.dsf` knows no such line | [`import`](../language/import.md) at file level, or a `Function` that uses the include |
| `#define K 2` | none in the Graph: the line belongs to the [preprocessor](../language/preprocessor.md), which uses a define in `#if` conditions | a `const` property |

`Graph` bodies are read as statements, not as text split at `;`, so a braced construct followed by
more code no longer swallows that code into one statement *(since 2.0.0)*.

## Expression-level constructs

| Written | Code |
| :-- | :-- |
| `x = a == b;` | `DSH2200` |
| `a == b;` *(as a statement)* | `DSH2211` — the comparison's value goes nowhere |
| `x = a++;`, `a++;`, `--a;` | [`DSH2207`](../diagnostics/DSH2xxx.md#dsh2207) |
| `x = (float)a;` | [`DSH2203`](../diagnostics/DSH2xxx.md#dsh2203) — there are no C-style casts; write `float(a)` |
| `x = (y = z);` | [`DSH2204`](../diagnostics/DSH2xxx.md#dsh2204) |
| `x = 0x10;` | [`DSH2222`](../diagnostics/DSH2xxx.md#dsh2222) — there are no hex literals; 1.x read `0x10` as `0` |
| `x = 1.0fx;`, `x = 0.5.5;` | `DSH2105` |
| `x = "text";` | [`DSH4202`](../diagnostics/DSH4xxx.md#dsh4202) |
| `x = {1, 2, 3};`, `Attrs.BaseColor = {1, 0, 0};` | [`DSH2162`](../diagnostics/DSH2xxx.md#dsh2162) — a brace list is read only as a declaration's initializer |
| `float4 v = {{1,2},{3,4}};` | [`DSH2214`](../diagnostics/DSH2xxx.md#dsh2214) — brace initializers do not nest |
| `f(a b)` | [`DSH2152`](../diagnostics/DSH2xxx.md#dsh2152) — a missing `,` |
| `if (a) Color = X;` | [`DSH2209`](../diagnostics/DSH2xxx.md#dsh2209) — braces are mandatory |
| `@`, `$`, a backtick | `DSH2101` |

## Compound assignment and increment

`+= -= *= /= %=`, the bitwise compound forms, `++` and `--` are not 1.x operators.

| Written | Code |
| :-- | :-- |
| `a += b;`, `a -= b;`, `a *= b;`, `a /= b;`, `a %= b;` | [`DSH2205`](../diagnostics/DSH2xxx.md#dsh2205) |
| `a++;`, `a--;`, `++a;`, `--a;` | `DSH2207` |

*(since 2.0.0)* the spacing no longer matters. 1.x read `a += b;` as a declaration of a variable
named `+` of type `a`, and `a+=b;` as an assignment to a brand-new variable named `a+`, leaving `a`
unchanged without a message; both are `DSH2205` now. Write the expansion: `a = a + b;`.

```c
// Refused (DSH2205).
Sum+=Tint;

// Correct:
Sum = Sum + Tint;
```

## Wrong-case keywords

`if` and `else` are keywords and are matched **case-sensitively**, as are `for`, `while`, `return`,
`struct` and the other HLSL keywords. A mis-cased spelling is a name.

| Written | Result |
| :-- | :-- |
| `If (x) {}`, `IF (x) {}` | `If (x)` is a call to a function named `If`, and the `{` where that statement's `;` belongs is `DSH2154` |
| `if (x) {} Else {}`, `ELSE {}` | the `if` parses; `Else` is a name, and its `{` is `DSH2154` |

Type tokens, constructor names and `true` / `false` are matched in any case. Other names are looked up
exactly first and then ignoring case, with a warning ([`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275),
[`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276) for engine names); see
[Name resolution](name-resolution.md).

## Features that simply do not exist

| Feature | Status | Alternative |
| :-- | :-- | :-- |
| Matrix types (`float3x3`, `float4x4`, `mat2`, `mat3`, `mat4`) | the graph has no matrix values: [`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361) | [`UE.TransformVector` / `UE.TransformPosition`](../builtins/transform.md), or matrix locals **inside** a `Function` body |
| Arrays and indexing | `DSH2213`, `DSH2202` | [swizzles](swizzle.md) for channels; `Texture2DArray` sampling for layers |
| `inout` parameters | a 1.x function has `in` and `out` only: [`DSH6302`](../diagnostics/DSH6xxx.md#dsh6302) | pass an `in` and an `out` |
| Function declarations inside `Graph` | `DSH2154` | top-level `Function` / `GraphFunction` |
| Integer arithmetic | absent — `int`, `uint`, `bool` and `half` are floats in the graph; a `/` between two integer constructors is [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243) | see [Expressions](expressions.md#integer-division) |
| Hex, octal and binary literals | `DSH2222` for hex | decimal literals; see [Literals](literals.md) |
| String values | `DSH4202` outside named `UE.*` arguments | — |
| Ternary conditional | `DSH2200` | `if` / `else`, `lerp`, `StaticSwitchParameter` |
| Comma operator | absent | separate statements |
| Assignment inside an expression | `DSH2204` | separate statements |
| Nested brace initializers | `DSH2214` | a constructor call: `float4(float2(1,2), float2(3,4))` |
| Preprocessor directives | only the [conditional set](../language/preprocessor.md) and `#Region` / `#EndRegion` | `import`, `const` properties |
| `Code = { … }` inside a `Shader` | **removed** — [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) | `Graph = { … }` |

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH1035` | a `#` line other than the conditional set and `#Region` / `#EndRegion` — `#include`, `#pragma` |
| `DSH2101` | a character that is not part of any token |
| `DSH2105` | a malformed number: `0.5.5`, `1.0fx`, `1e` |
| `DSH2151` | an expression was expected and something else was found: `struct`, `= 5;`, `if ()` |
| `DSH2152` | a `)` is missing — often a missing `,` between two arguments |
| `DSH2154` | a statement does not end with `;`: a mis-cased `If`, a function definition, a final statement without `;` |
| `DSH2160` | `switch`, `case` or `default` at the start of a statement |
| `DSH2162` | a brace list used as a value, outside a declaration |
| `DSH2200` | a comparison, a logical operator or `?:` outside an `if` condition |
| `DSH2201` | `%`, a shift or a bitwise operator |
| `DSH2202` | `[ ]` |
| `DSH2203` | a C-style cast |
| `DSH2204` | an assignment inside an expression |
| `DSH2205` | a compound assignment |
| `DSH2207` | `++` or `--` |
| `DSH2208` | `for`, `while`, `do`, `return`, `break`, `continue`, `discard` |
| `DSH2209` | an `if` or `else` body without braces |
| `DSH2210` | more than one comparison, or `&&` / `\|\|`, in an `if` condition |
| `DSH2211` | an expression statement whose value nothing receives |
| `DSH2212` | a storage keyword on a Graph variable |
| `DSH2213` | an array declarator |
| `DSH2214` | a brace list inside a brace initializer |
| `DSH2220` | a bare `{ … }` block |
| `DSH2222` | a hexadecimal literal |
| `DSH2246` | a `Code` section in a `Shader` or function block |
| `DSH4202` | a string used as a value |
| `DSH4243` | both operands of `/` are integer constructor calls |
| `DSH4361` | a matrix value |
| `DSH6302` | an `inout` parameter on a 1.x function |

The complete list, with every message, lives in the [diagnostics index](../diagnostics/index.md).

## Example

A GLSL-shaped attempt and its DreamShaderLang equivalent.

```c
// Refused.
Graph {
    vec2  uv    = UE.TexCoord(Index = 0);
    float t     = UE.Time() % 4.0;                 // DSH2201
    float mask  = uv.x > 0.5 && uv.y > 0.5;        // DSH2200
    vec3  col   = mask ? Hot : Cold;               // DSH2200
    col        *= Gain;                            // DSH2205
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
- [Math builtins](../builtins/math.md) — `fmod`, `min`, `max`, `clamp`, `lerp`
- [Name resolution](name-resolution.md) — the case-sensitivity matrix
- [`dsc migrate`](../tools/migrate.md) — rewrite a 1.x file as `.dss`, where loops and `?:` exist
- [Diagnostics index](../diagnostics/index.md) — every code, by stage
