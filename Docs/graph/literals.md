# Literals

> [DreamShader](../index.md) » [Graph](index.md) » **Literals**

The three literal forms a `Graph` expression can contain: numeric literals, string literals, and the
boolean identifiers `true` / `false`.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration default |
| Kind | expression primary |
| Generates | `UMaterialExpressionConstant` (numeric), `UMaterialExpressionStaticBool` (boolean); a string literal generates nothing on its own |

## Synopsis

```c
<number-literal> := { <digit> | . } … [ { e | E } [ { + | - } ] <digit> … ] [ <suffix> ]
<suffix>         := { f | F | h | H | u | U | l | L }
<string-literal> := " <character> … "
<bool-literal>   := { true | false }
```

A numeric literal starts at a digit, or at a `.` that is immediately followed by a digit. `[ … ]`,
`{ a | b }` and `…` are meta-notation; `.`, `e`, `+`, `-` and `"` are literal characters.

## Numeric literals

Every numeric literal is a **1-component floating-point value**. There is no integer literal type.

| Written | Token after lexing | Value | Result |
| :-- | :-- | :-- | :-- |
| `1` | `1` | 1.0 | `Constant`, 1 component |
| `1.0` | `1.0` | 1.0 | the **same** `Constant` node as `1` |
| `0.5` | `0.5` | 0.5 | `Constant` |
| `.5` | `.5` | 0.5 | `Constant` — a leading `.` is legal |
| `1e-3` | `1e-3` | 0.001 | `Constant` |
| `2.5E+2` | `2.5E+2` | 250.0 | `Constant` |
| `0.55f` | `0.55` | 0.55 | `Constant` — the suffix is stripped |
| `3u` | `3` | 3.0 | `Constant` — **not** an integer value |
| `0.5.5` | `0.5.5` | 0.5 | `Constant` — the conversion keeps the longest valid prefix and **silently discards** the rest |
| `0x10` | `0` then identifier `x10` | — | error: `Unexpected token 'x10' in Graph expression.` |
| `1.0fx` | `1.0` then identifier `fx` | — | error: `Unexpected token 'fx' in Graph expression.` |
| `3ul` | `3` then identifier `ul` | — | error: `Unexpected token 'ul' in Graph expression.` |

Lexing details:

- Digits and `.` are consumed greedily into a single token. **Multiple `.` are accepted by the
  lexer**, and the numeric conversion is permissive: it takes the longest valid prefix and throws the
  remainder away without a diagnostic, so `0.5.5` is `0.5` and `0..5` is `0`.
- At most **one** exponent is consumed. A second `e`/`E` terminates the token and begins an
  identifier.
- The sign of an exponent may be `+` or `-` and is consumed only immediately after `e`/`E`.
- There is **no hexadecimal, binary or octal notation**, and no digit separators.
- A leading `-` is not part of the literal; it is the [unary minus operator](expressions.md#unary-operators).

## Numeric suffixes

A single trailing type suffix is consumed and **excluded from the token text**. It has no semantic
effect whatsoever — it does not change the type, the width, or the integer marker.

| Suffix | HLSL/GLSL meaning | Effect in DreamShaderLang |
| :-- | :-- | :-- |
| `f` | float | consumed, discarded |
| `F` | float | consumed, discarded |
| `h` | half | consumed, discarded |
| `H` | half | consumed, discarded |
| `u` | unsigned int | consumed, discarded |
| `U` | unsigned int | consumed, discarded |
| `l` | long | consumed, discarded |
| `L` | long | consumed, discarded |

Rules:

- **Exactly one** suffix character. `3ul`, `1.0LL` and `2.0fh` are not literals — the first suffix
  character is left in place and the remainder lexes as an identifier.
- The suffix is consumed only when the character after it is **not** alphanumeric and not `_`. This is
  what distinguishes `0.55f;` (suffix) from `1.0fx` (identifier `fx`).
- `u`, `U`, `l`, `L` do **not** produce an integer-typed value. Only an integer
  [constructor](constructors.md#integer-constructors) does.

## Integer and float

| Question | Answer |
| :-- | :-- |
| Is `1` different from `1.0`? | No. Both parse to the same `double` and share one `Constant` node. |
| Does `3u` produce an integer? | No. |
| Does `int x = 7;` produce an integer? | No — the declared type token only fixes the component count. |
| What produces an integer-marked value? | Only a call to `int`, `int2..4`, `ivec2..4`, `uint`, `uint2..4`, `uvec2..4`. |
| What does the integer marker do? | It is read by exactly one rule: `/` rejects the operation when **both** operands carry it. See [Integer division](expressions.md#integer-division). |
| Is there integer arithmetic or truncation? | No. `7 / 2` evaluates to `3.5` in the material graph. |

## String literals

A string literal is tokenized with escapes decoded, and **never evaluates to a value**. It is legal
only where an argument handler reads the literal *text* rather than evaluating it — a named `UE.*` /
`Substrate.*` argument, an `Output=` / `OutputName=` output selector, or a `Path(…)` asset reference.
Anywhere the expression evaluator reaches it, it is a build error.

| Escape | Produces |
| :-- | :-- |
| `\n` | line feed |
| `\r` | carriage return |
| `\t` | tab |
| `\"` | `"` |
| `\\` | `\` |
| `\<any other character>` | that character, with the backslash dropped — `\q` yields `q` |

> [!NOTE]
> An **unterminated** string literal is not diagnosed. The lexer consumes to end of input and emits
> whatever it collected as the string token; the failure then surfaces as a missing `)` or a
> truncated statement further along.

Valid placement:

```c
vec4 Scene = UE.SceneTexture(Id = "PostProcessInput0");
```

Invalid placement (anywhere the expression evaluator sees it):

```c
float x = "0.5";   // String literals can only be used in named UE builtin arguments.
```

## Boolean literals

`true` and `false` *(unreleased)* are lexed as ordinary identifiers and resolved late, after Graph
variables and after declared properties.

| | |
| :-- | :-- |
| Spelling | matched **case-insensitively** — `true`, `True`, `TRUE`, `false`, `False`, `FALSE` all resolve |
| Node | `UMaterialExpressionStaticBool` |
| Component count | 1 |
| Reuse | keyed on the boolean value, so a graph contains at most one `StaticBool` node per value |

They are the only way to feed a `StaticSwitch` input or a `StaticBool` function input from a `Graph`
block.

```c
Color = UseDetail(True = Detailed, False = Flat);
StaticBool Enabled = true;
```

> [!WARNING]
> Because `true` / `false` are resolved **after** variables and properties, a Graph variable or a
> declared property named `True` or `False` shadows the literal, and the shadowing is silent. Avoid
> those names. See [Name resolution](name-resolution.md).

## Notes

- Numeric literals are shared across the whole graph: the reuse key is the literal's exact `double`
  value, so every occurrence of `0.5` in one shader body maps to a single `Constant` node. Values that
  differ beyond 17 significant digits are distinct nodes. See [Node reuse](node-reuse.md).
- When **every** argument of a non-integer constructor is a numeric literal (optionally with a leading
  unary `+`/`-`), the constructor folds into one `Constant2Vector` / `Constant3Vector` /
  `Constant4Vector` node instead of N `Constant` nodes plus `AppendVector` nodes. See
  [Constructors](constructors.md#constant-folding).
- A literal's component count is authoritative only through that folding path; a bare scalar literal
  is not authoritative. See [Conversions](conversions.md#authoritative-component-counts).
- The literal grammar described here is the **Graph expression** lexer. The declaration-level lexer
  used by `Properties`, `Settings` and metadata blocks is documented in
  [Lexical elements](../language/lexical.md).

## Diagnostics

Format specifiers are rendered as `{Placeholder}` throughout this page; the compiler emits the
substituted text.

| Message | Cause |
| :-- | :-- |
| `Invalid numeric literal '{Text}'.` | A number token that does not convert. The conversion only rejects a token that evaluates to **zero** while containing a character that no spelling of zero can contain — in practice an exponent that underflows, such as `1e-9999`. Shapes that merely have a trailing remainder (`0.5.5`, `0..5`, `1e`) convert to their valid prefix instead and never reach this message. |
| `Unexpected token '{Text}' in Graph expression.` | An identifier produced by an unconsumed suffix (`fx`, `ul`) or by hex notation (`x10`). |
| `String literals can only be used in named UE builtin arguments.` | A string literal evaluated as a value. |
| `Failed to create a StaticBool node for literal '{Text}'.` | The `StaticBool` material node could not be created for `true` / `false`. |
| `Unknown Graph identifier '{Name}'.` | An identifier that is not a variable, not a property and not `true` / `false`. |

## Example

```c
Shader(Name="Docs/M_Literals", Root="Game")
{
    Settings {
        Domain = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode = "Opaque";
    }

    Outputs {
        float Rough;
        vec3 Color;
        Base.Roughness = Rough;
        Base.EmissiveColor = Color;
    }

    Graph {
        Rough = 0.55f;                  // suffix stripped -> 0.55
        float Half  = .5;               // leading dot
        float Small = 1e-3;             // exponent
        float Same  = 1;                // shares the Constant node with 1.0 below
        Color = vec3(Half, Small, 1.0) * Same;
    }
}
```

Generated nodes:

```text
Constant(0.55)                       -> Base.Roughness
Constant(0.5)   Constant(0.001)      -> AppendVector chain for vec3(Half, Small, 1.0)
Constant(1.0)                        -> reused by both `Same` and the third vec3 component
Multiply                             -> Base.EmissiveColor
```

## See also

- [Expressions](expressions.md) — operators, precedence and where a literal may appear
- [Constructors](constructors.md) — constant folding of all-literal constructor calls
- [Conversions](conversions.md) — how a 1-component literal widens to a vector target
- [Lexical elements](../language/lexical.md) — comments, identifiers and the declaration-level literal rules
- [Types](../language/types.md) — the full type-token catalogue
- [Name resolution](name-resolution.md) — the lookup order that lets a variable shadow `true`
- [Node reuse](node-reuse.md) — literal deduplication
- [Generic `UE.Expression`](../builtins/ue-expression.md) — the named string arguments a literal may fill
