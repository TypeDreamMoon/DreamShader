# Statements

> [DreamShader](../index.md) » [Graph](index.md) » **Statements**

The complete set of statement forms a `Graph` body may contain, the rules that terminate them, and
how the parser decides which form a given statement is.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph` block |
| Kind | statement grammar |
| Executes | once each, top to bottom, at generation time |

## Synopsis

```c
graph-statement :=
      <type> <name> [ = <expression> ] ;                          // declaration
    | <type> <name> [ = <init> ] , <name> [ = <init> ] … ;        // comma declarators
    | <type> <name> = { <expression> , … } ;                      // brace initializer
    | <name> = <expression> ;                                     // assignment
    | <name> . <member> = <expression> ;                          // member write
    | <call-expression> ;                                         // statement-form call
    | if ( <condition> ) { <graph-statement> … }
      [ else { <graph-statement> … } | else if ( … ) { … } … ]
```

## Form table

| # | Form | Example | Terminator | Reference |
| --: | :-- | :-- | :-- | :-- |
| 1 | Declaration, no initializer | `float3 c;` | `;` | [Declarations](declarations.md) |
| 2 | Declaration + expression initializer | `float3 c = A * K;` | `;` | [Declarations](declarations.md) |
| 3 | Declaration + brace initializer | `vec4 v = {rgb, 1.0};` | `;` | [Declarations](declarations.md) |
| 4 | Assignment to an existing or output variable | `Color = Tint;` | `;` | [below](#assignment) |
| 5 | Assignment + brace initializer | `Color = {r, g, b};` | — | **refused** — [`DSH2162`](../diagnostics/DSH2xxx.md#dsh2162) *(since 2.0.0)*; write `Color = float3(r, g, b);` |
| 6 | `MaterialAttributes` member write | `Attrs.BaseColor = Tint;` | `;` | [MaterialAttributes](material-attributes.md) |
| 7 | Member write + brace initializer | `Attrs.BaseColor = {1, 0, 0};` | — | **refused** — `DSH2162` *(since 2.0.0)*; write a constructor |
| 8 | Comma-separated declarators | `float a = 1, b, c = 3;` | `;` | [Declarations](declarations.md) |
| 9 | Statement-form call | `F_Split(Src, OutA, OutB);` | `;` | [Calls](calls.md) |
| 10 | `if` / `else` / `else if` | `if (m > .5) { … } else { … }` | **none** — ends at its last `}` | [if / else](if.md) |

Form 8 expands to one variable per declarator; all of them share the type token of the **first**
declarator, and each reports its own source line and column.

## Termination and splitting

A `Graph` body is read by the 2.0 statement parser, token by token *(since 2.0.0)*; it is no longer
split as text at `;`.

| Rule | Behaviour |
| :-- | :-- |
| Separator | every statement except `if` ends with `;`; a missing one is [`DSH2154`](../diagnostics/DSH2xxx.md#dsh2154), reported at the first token after the statement |
| Repeated `;` | each extra `;` is an empty statement and is dropped without a diagnostic |
| Final `;` | **required** on the last statement of a body too: `DSH2154` *(since 2.0.0)* |
| `if` statements | end with the `}` of their last body, so an `if` needs no `;` |
| Positions | every statement and expression carries its own line and column |
| Nested bodies | `if` / `else` bodies are blocks of the same grammar, so **every** form above is legal inside a branch, including nested `if` |

```c
Graph = {
    float a = 1.0;;;             // the empty statements are dropped
    if (a > 0.5) { a = 0.0; }    // no ; after the }
    float b = a * 2.0;           // the final statement needs its ; (DSH2154 without it)
}
```

A statement that fails to parse is skipped up to the next `;` at its own nesting level, and the
parser carries on with the next statement; every error of the body is reported in one build.

## Classification order

The parser decides a statement's form from its first tokens. The first rule that matches wins.

| Order | Statement starts with | Result |
| --: | :-- | :-- |
| 1 | the keyword `if` (**case-sensitive**) | form 10 |
| 2 | `for`, `while`, `do`, `return`, `break`, `continue`, `discard` | parsed, then refused: [`DSH2208`](../diagnostics/DSH2xxx.md#dsh2208) |
| 3 | `switch`, `case`, `default` | [`DSH2160`](../diagnostics/DSH2xxx.md#dsh2160) |
| 4 | `{` | a bare block: [`DSH2220`](../diagnostics/DSH2xxx.md#dsh2220) |
| 5 | `;` | an empty statement |
| 6 | a `#Region` / `#EndRegion` line | a region; see [Layout](../language/layout.md) |
| 7 | two names in a row (`float3 c`), optionally after `static` / `const` | a declaration: forms 1–3 and 8. A storage keyword is [`DSH2212`](../diagnostics/DSH2xxx.md#dsh2212) |
| 8 | anything else | an expression: an assignment (forms 4–7), a call (form 9), or [`DSH2211`](../diagnostics/DSH2xxx.md#dsh2211) for a value nothing receives |

> [!WARNING]
> Keywords are case-sensitive. `If (x) { … }` is not an `if` statement: `If` is a name, `If (x)` a
> call, and the `{` where the call statement's `;` belongs is `DSH2154`. See
> [Unsupported constructs](unsupported.md#wrong-case-keywords).

> [!NOTE]
> A declared name is one identifier. `float3 A.B = x;` is `DSH2154` at the `.` *(since 2.0.0)*; 1.x
> declared a variable literally named `A.B`. Member writes have no type in front: `Attrs.BaseColor = x;`.

## Assignment

Forms 4 and 6. The target is a name or `name.member`; anything else — a swizzle of a member, an
index, a call — is [`DSH2206`](../diagnostics/DSH2xxx.md#dsh2206).

```c
<target> = <expression> ;
```

The target is resolved in this order:

| Order | Target names | Behaviour |
| --: | :-- | :-- |
| 1 | `name.member` | `MaterialAttributes` member write — see [MaterialAttributes](material-attributes.md) |
| 2 | a Graph variable, an `Outputs` declaration of a `Shader`, or an output of a function block | the value is converted to the variable's declared type |
| 3 | a property | [`DSH4229`](../diagnostics/DSH4xxx.md#dsh4229): a property is an input; copy it into a variable first |
| 4 | nothing | declared here, with the value's type — legacy rule L26, said by the info [`DSH5292`](../diagnostics/DSH5xxx.md#dsh5292) |

A name is looked up exactly first, then ignoring case; a unique match in another case is taken with
the warning [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275). See [Name resolution](name-resolution.md).

Rule 4 means an undeclared name on the left of `=` is not an error:

```c
Graph = {
    vec3 Boost = Tint * 2.0;
    Scratch    = Boost;      // DSH5292: declares 'Scratch' as a float3
    Color      = Scratch;    // 'Color' is an Outputs declaration -> converted to its type
}
```

The conversion at rule 2 takes the **leading components** of a wider value, with the info
[`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) (legacy rule L22), and spreads a scalar across a
wider target. It never widens a 2-component value to 3: that, and any other value that does not fit,
is [`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228). See [Conversions](conversions.md).

### Brace-initializer assignment

A brace list is read only as the initializer of a declaration. On the right of an assignment or a
member write it is [`DSH2162`](../diagnostics/DSH2xxx.md#dsh2162) *(since 2.0.0)*; write the
constructor instead: `Color = float3(r, g, b);`. The declaration form and its rules are on
[Declarations](declarations.md#brace-initializers); the constructor rules are on
[Constructors](constructors.md).

## Expression statements

Form 9. A statement that is neither a declaration nor an assignment must be a **call**; any other
expression is `DSH2211`.

In statement form a function's outputs are received by the trailing arguments, one plain variable
name per output, in declaration order. Full argument rules, out-target constraints and the
value-call form are on [Calls](calls.md).

Statement-form multi-output `ShaderFunction` / `VirtualFunction` calls are available
*(since 1.3.5)*; single-output `Function` / `GraphFunction` value calls *(since 1.3.1)*.

## Synthesized statements

Every `Outputs` declaration of a `Shader` becomes a variable declared at the top of the material's
body, before the first statement of the `Graph` *(since 2.0.0)*. It is initialized with its own
initializer when it has one, and otherwise with a zero of its type (an empty attribute set for
`MaterialAttributes`); a texture or `Substrate` output without an initializer starts without a
value. These declarations carry the position of the `Outputs` line they came from.

A top-level `Graph` declaration of an output's name (same case) does not declare a second variable:
with an initializer it becomes an assignment to the output, and without one it is dropped.

| Code | Raised when |
| :-- | :-- |
| [`DSH3270`](../diagnostics/DSH3xxx.md#dsh3270) | an `Outputs` declaration has `=` and nothing after it |
| [`DSH3266`](../diagnostics/DSH3xxx.md#dsh3266) | an `Outputs` declaration is followed by neither `;` nor the section's closing `}` |

A `Shader` whose `Outputs` section declares or binds anything needs no `Graph` section; one with
neither is [`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255). See [Shader](../language/shader.md).

## Diagnostics

Every diagnostic is reported as `<file>(<line>,<column>): DSHnnnn: <message>`, at the construct
itself, including inside an `if` body.

### Parse time

| Code | Raised when |
| :-- | :-- |
| `DSH2154` | a statement does not end with `;` — including the last one of a body |
| [`DSH2151`](../diagnostics/DSH2xxx.md#dsh2151) | an expression was expected and something else was found, e.g. `= 5;` |
| [`DSH2152`](../diagnostics/DSH2xxx.md#dsh2152) | a `)` is missing from a call or a parenthesized expression |
| [`DSH2161`](../diagnostics/DSH2xxx.md#dsh2161) | `.` is not followed by a name |
| [`DSH5260`](../diagnostics/DSH5xxx.md#dsh5260) | `::` is not followed by a name |
| [`DSH2163`](../diagnostics/DSH2xxx.md#dsh2163) | a declarator after `,` is not a name |
| [`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105) | a malformed number, such as `0.5.5` — see [Literals](literals.md) |
| `DSH2160` | `switch`, `case`, `default` |
| `DSH2162` | a brace list used as a value (forms 5 and 7) |
| `DSH2200`–`DSH2222` | a construct 1.x did not have — see [Unsupported constructs](unsupported.md) |

### Bind and build time

| Code | Raised when |
| :-- | :-- |
| `DSH2206` | the left of `=` is not a name or `name.member` |
| [`DSH4220`](../diagnostics/DSH4xxx.md#dsh4220) | a name is declared twice in one block, or a branch declares a name of the enclosing body |
| `DSH4228` | the value of an assignment or initializer does not fit the target's type |
| `DSH4229` | the target is a property |
| `DSH5289` | *info:* a wider value was cut to the target's leading components |
| `DSH5292` | *info:* an assignment declared its undeclared target |
| `DSH5275` | *warning:* a name matched a declaration only ignoring case |
| [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200) | a name matches no declaration at all |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

Every accepted statement form in one body:

```c
ShaderFunction(Name="Functions/F_AllForms")
{
    Properties = { vec3 Tint = vec3(1.0, 0.4, 0.1); float K = 2.0; }
    Inputs     = { vec2 UV; }
    Outputs    = { MaterialAttributes Attrs; vec3 Debug; }

    Graph = {
        MaterialAttributes Attrs;              //    names the output: no second variable
        float3 c;                              // 1  declaration, no initializer
        float3 scaled = Tint * K;              // 2  declaration + expression
        vec4   packed = {scaled, 1.0};         // 3  declaration + brace initializer
        c = packed.rgb;                        // 4  assignment
        Debug = float3(0.0, 0.0, 0.0);         //    (form 5 is refused: write the constructor)
        Attrs.BaseColor = c;                   // 6  member write
        Attrs.Roughness = 0.35;                //    (form 7 is refused: write the value)
        float a = 1, b, d = 3;                 // 8  comma declarators

        if (K > 1.0) {                         // 10 if / else
            Debug = c * a;
        } else {
            Debug = c * b;
        }
    }
}
```

## See also

- [Graph](index.md) — the evaluation model and the section grammar
- [Declarations](declarations.md) — type tokens, defaults, comma declarators, brace initializers, scope
- [Expressions](expressions.md) — operators, precedence, associativity
- [if / else](if.md) — the condition, the truth table, branch merging
- [Calls](calls.md) — value-form and statement-form calls, out targets, named arguments
- [MaterialAttributes](material-attributes.md) — member reads and writes
- [Conversions](conversions.md) — what "converted to the declared type" does
- [Name resolution](name-resolution.md) — how a bare identifier is looked up
- [Unsupported constructs](unsupported.md) — `for`, `while`, `return`, `+=`, ternary, and their codes
- [Node reuse](node-reuse.md) — why two identical statements can produce one node
- [Output bindings](../language/output-bindings.md) — `Outputs` declarations and their initializers
- [Diagnostics index](../diagnostics/index.md) — every code, by stage
