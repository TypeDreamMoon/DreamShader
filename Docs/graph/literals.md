# Literals

> [DreamShader](../index.md) » [Graph](index.md) » **Literals**

The three literal forms a `Graph` expression can contain: numeric literals, string literals, and the
boolean literals `true` / `false`.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration default |
| Kind | expression primary |
| Generates | `UMaterialExpressionConstant` (numeric and boolean); a string literal generates nothing on its own |

A 1.x `Graph` body is read by the DreamShaderLang 2.0 lexer *(since 2.0.0)*: the token rules below
are the 2.0 ones, and the few 1.x readings that differ are refused with a code that says what 1.x did.

## Synopsis

```c
<number-literal> := { <digit> | . } … [ { e | E } [ { + | - } ] <digit> … ] [ <suffix> … ]
<suffix>         := { f | F | h | H | u | U | l | L }
<string-literal> := " <character> … "
<bool-literal>   := { true | false }
```

A numeric literal starts at a digit, or at a `.` that is immediately followed by a digit. `[ … ]`,
`{ a | b }` and `…` are meta-notation; `.`, `e`, `+`, `-` and `"` are literal characters.

## Numeric literals

A literal without a fraction, an exponent or a float suffix is an **integer** to the compiler —
`int`, or `uint` with a `u` *(since 2.0.0)*; every other one is a `float` (`half` with an `h`). In the
generated graph every literal is the same thing: a 1-component float `Constant`.

| Written | Kind | Value | Result |
| :-- | :-- | :-- | :-- |
| `1` | `int` | 1.0 | `Constant`, 1 component |
| `1.0` | `float` | 1.0 | the **same** `Constant` node as `1` |
| `0.5` | `float` | 0.5 | `Constant` |
| `.5` | `float` | 0.5 | `Constant` — a leading `.` is legal |
| `1.` | `float` | 1.0 | `Constant` — a trailing `.` is legal |
| `1e-3` | `float` | 0.001 | `Constant` |
| `2.5E+2` | `float` | 250.0 | `Constant` |
| `0.55f` | `float` | 0.55 | `Constant` |
| `0.5h` | `half` | 0.5 | `Constant` |
| `3u` | `uint` | 3.0 | `Constant` — an unsigned integer *(since 2.0.0)* |
| `0.5.5` | — | — | [`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105) *(since 2.0.0; 1.x silently read 0.5)* |
| `0x10` | — | — | [`DSH2222`](../diagnostics/DSH2xxx.md#dsh2222) — a hexadecimal literal, which 1.x never read as one |
| `1.0fx` | — | — | `DSH2105` — the letters belong to the number |
| `3ul` | `uint` | 3.0 | `Constant` — `u` and `l` are both integer suffixes *(since 2.0.0)* |

Lexing details:

- A number is one token: digits, at most one `.`, at most one exponent, then suffix letters. Anything
  still glued to it — another `.`, letters, digits after a suffix — belongs to the same token and makes
  it [`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105) *(since 2.0.0; 1.x kept the longest valid prefix and
  dropped the rest)*. So `0.5.5`, `0..5`, `1abc` and `1.0fx` are errors, not numbers.
- An exponent needs digits: `1e` and `1e+` are `DSH2105`. The sign of an exponent may be `+` or `-`
  and is read only immediately after `e`/`E`.
- A hexadecimal literal (`0x1F`) is an integer to the lexer and refused in a 1.x `Graph`
  ([`DSH2222`](../diagnostics/DSH2xxx.md#dsh2222)). There is no binary or octal notation, and no digit
  separators.
- A leading `-` is not part of the literal; it is the [unary minus operator](expressions.md#unary-operators).
  A negated literal folds into one `Constant`.

## Numeric suffixes

Suffix letters are kept in the literal's text and decide its kind; the value is the number in front
of them.

| Suffix | HLSL/GLSL meaning | Effect in DreamShaderLang |
| :-- | :-- | :-- |
| `f`, `F` | float | a `float` literal |
| `h`, `H` | half | a `half` literal |
| `u`, `U` | unsigned int | a `uint` literal |
| `l`, `L` | long | an integer literal |

Rules:

- Several suffix letters may follow one number. A float letter together with an integer letter
  (`1fu`) is `DSH2105`, and so is an integer letter on a literal with a fraction or an exponent
  (`1.0u`, `1e3l`) or a float letter on a hexadecimal one.
- The kind of a literal matters only to the compiler — see [Integer and float](#integer-and-float).
  The graph gets a float `Constant` either way.

## Integer and float

| Question | Answer |
| :-- | :-- |
| Is `1` different from `1.0`? | To the compiler, yes: `1` is an `int` *(since 2.0.0)*. In the graph, no: both are one `Constant` node holding 1. |
| Does `3u` produce an integer? | Yes, a `uint` *(since 2.0.0)*. |
| Does `int x = 7;` produce an integer? | Yes: `x` is an `int` variable *(since 2.0.0)*. Its value in the graph is the float 7. |
| What makes a value an integer? | An integer literal, an `int` / `uint` declaration, or an integer [constructor](constructors.md#integer-constructors). |
| What does the integer kind do? | One thing: `/` is refused when both operands are integers ([`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243)). So `7 / 2` is an error *(since 2.0.0; 1.x evaluated it to 3.5)*; write `7.0 / 2`. See [Integer division](expressions.md#integer-division). |
| Is there integer arithmetic or truncation? | No. `int x = 7.9;` holds 7.9, and `7.0 / 2` is 3.5 in the material graph. |

## String literals

A string literal is tokenized with escapes decoded, and **never evaluates to a value**. It is legal
only where an argument handler reads the literal *text* rather than evaluating it — a named `UE.*` /
`Substrate.*` argument that sets a property of the node, an `Output=` / `OutputName=` output selector,
or a `Path(…)` asset reference. Anywhere a value is wanted it is
[`DSH4202`](../diagnostics/DSH4xxx.md#dsh4202).

| Escape | Produces |
| :-- | :-- |
| `\n` | line feed |
| `\r` | carriage return |
| `\t` | tab |
| `\"` | `"` |
| `\\` | `\` |
| `\0` | nothing — the character is dropped |
| `\<any other character>` | [`DSH2104`](../diagnostics/DSH2xxx.md#dsh2104) *(since 2.0.0; 1.x dropped the backslash)*; the backslash is kept in the value |

> [!NOTE]
> A string never spans lines. One with no closing `"` before the end of its line is
> [`DSH2103`](../diagnostics/DSH2xxx.md#dsh2103) *(since 2.0.0; 1.x read on to the end of the input)*,
> and the rest of the file is still read and reports its own errors.

Valid placement:

```c
vec4 Scene = UE.SceneTexture(Id = "PostProcessInput0");
```

Invalid placement (anywhere a value is wanted):

```c
float x = "0.5";   // DSH4202
```

## Boolean literals

`true` and `false` *(since 1.6.0)* are read as literals while the line is parsed, in any case, before
any name is looked up *(since 2.0.0; 1.x resolved them after variables and properties)*.

| | |
| :-- | :-- |
| Spelling | matched **case-insensitively** — `true`, `True`, `TRUE`, `false`, `False`, `FALSE` |
| Type | `bool`, 1 component |
| Node | a `Constant` holding 1 or 0 *(since 2.0.0; 1.x built a `StaticBool`)* |
| Reuse | equal constants are one node, so a graph has at most one per value |

```c
StaticBool Enabled = true;     // a bool variable
float      On      = TRUE;     // any case; a bool used as a number is 1
```

> [!WARNING]
> Because `true` / `false` are read **before** any name, a Graph variable or a declared property named
> `True` or `False`, in any case, cannot be read: every use of the name is the literal. Avoid those
> names. See [Name resolution](name-resolution.md#shadowing).

## Notes

- Equal numeric literals are one `Constant` node across the whole graph: the [dedupe pass](node-reuse.md)
  compares values to 9 significant digits, so `0.5` written five times, or `1` and `1.0`, is one node.
- When **every** argument of a constructor is a constant, the constructor folds into one
  `Constant2Vector` / `Constant3Vector` / `Constant4Vector` node instead of `Constant` nodes plus
  `AppendVector` nodes — integer constructors included *(since 2.0.0)*. See
  [Constructors](constructors.md#constant-folding).
- A literal is one component wide; no value carries a width other than its type's *(since 2.0.0)*. See
  [Conversions](conversions.md#authoritative-component-counts).
- The literal rules here are those of `Graph` expressions. The declaration-level rules for
  `Properties`, `Settings` and metadata blocks are in [Lexical elements](../language/lexical.md).

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH2105` | A malformed number: a second `.`, an exponent without digits, a float and an integer suffix together, an integer suffix on a fraction, letters glued to the end. |
| `DSH2222` | A hexadecimal literal in a 1.x `Graph`. |
| `DSH4243` | `/` between two integers — two integer literals included. |
| `DSH2103` | A string literal with no closing `"` on its line. |
| `DSH2104` | An unknown escape in a string literal. |
| `DSH4202` | A string literal where a value is wanted. |
| [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200) | An identifier that is no variable and no declaration. |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | The emitter could not create the `Constant` node. |

## Example

```c
Shader(Name="Docs/M_Literals", Root="Game")
{
    Properties { ScalarParameter K = 0.5; }

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
        Rough = 0.55f * K;              // a float literal with its suffix
        float H     = .5;               // leading dot
        float Small = 1e-3;             // exponent
        float One   = 1;                // an int literal: the same Constant node as the 1.0 below
        Color = vec3(H * K, Small, 1.0) * One;
    }
}
```

Generated nodes:

```text
ScalarParameter K
Constant(0.55), Multiply             -> Base.Roughness
Constant(0.5), Multiply              -> H * K
Constant(0.001)                      -> Small
Constant(1.0)                        -> One, and the third vec3 component: one node
AppendVector, AppendVector           -> vec3(H * K, Small, 1.0)
Multiply                             -> Base.EmissiveColor
```

## See also

- [Expressions](expressions.md) — operators, precedence and where a literal may appear
- [Constructors](constructors.md) — constant folding of all-constant constructor calls
- [Conversions](conversions.md) — how a 1-component literal widens to a vector target
- [Lexical elements](../language/lexical.md) — comments, identifiers and the declaration-level literal rules
- [Types](../language/types.md) — the full type-token catalogue
- [Name resolution](name-resolution.md) — the lookup order, and why `true` cannot be shadowed
- [Node reuse](node-reuse.md) — constant deduplication
- [Generic `UE.Expression`](../builtins/ue-expression.md) — the named string arguments a literal may fill
