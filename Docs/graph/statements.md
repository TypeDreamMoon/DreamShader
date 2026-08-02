# Statements

> [DreamShader](../index.md) » [Graph](index.md) » **Statements**

The complete set of statement forms a `Graph` body may contain, the rules that terminate them, and
the order in which the parser decides which form a given piece of text is.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph` block |
| Kind | statement grammar |
| Executes | once each, top to bottom, at generation time |

## Synopsis

```c
graph-statement :=
      [ <type> ] <name> [ = <expression> ] ;                      // declaration / assignment
    | <type> <name> [ = <init> ] , <name> [ = <init> ] … ;        // comma declarators
    | <name> = { <expression> , … } ;                             // brace initializer
    | <name> . <member> = { <expression> | { <expression> , … } } ;
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
| 5 | Assignment + brace initializer | `Color = {r, g, b};` | `;` | [below](#assignment) |
| 6 | `MaterialAttributes` member write | `Attrs.BaseColor = Tint;` | `;` | [MaterialAttributes](material-attributes.md) |
| 7 | Member write + brace initializer | `Attrs.BaseColor = {1, 0, 0};` | `;` | [MaterialAttributes](material-attributes.md) |
| 8 | Comma-separated declarators | `float a = 1, b, c = 3;` | `;` | [Declarations](declarations.md) |
| 9 | Statement-form call | `F_Split(Src, OutA, OutB);` | `;` | [Calls](calls.md) |
| 10 | `if` / `else` / `else if` | `if (m > .5) { … } else { … }` | **none** — brace matched | [if / else](if.md) |

Form 8 expands to one statement per declarator; all of them share the type token of the **first**
declarator and all of them report the **same** source line and column.

## Termination and splitting

| Rule | Behaviour |
| :-- | :-- |
| Separator | `;` at top level only. Parenthesis, brace, bracket depth and string state are all tracked, so a `;` inside `( )`, `{ }`, `[ ]` or `"…"` does not split. |
| Repeated `;` | runs of `;` collapse; empty statements are dropped without a diagnostic |
| Trailing `;` | the **final** statement of a body may omit its `;` |
| `if` statements | detected *before* the `;` scan and delimited by brace matching, so an `if` needs no `;` and a `;` inside its body does not split it |
| Leading whitespace | skipped; every statement records a 1-based line and column relative to the block |
| Nested bodies | `if` / `else` bodies are parsed by the same splitter, so **every** form above is legal inside a branch, including nested `if` |

```c
Graph = {
    float a = 1.0;;;             // the empty statements between ; are dropped
    if (a > 0.5) { a = 0.0; }    // no ; needed; the inner ; does not split the if
    float b = a * 2.0            // final statement, ; omitted
}
```

## Classification order

Given one statement's text, the parser decides its form in this order. The first rule that matches
wins.

| Order | Test | Result |
| --: | :-- | :-- |
| 1 | starts with the keyword `if` (**case-sensitive**, identifier-bounded) | form 10 |
| 2 | splitting on top-level `,` yields more than one segment **and** the first segment, minus its `= …`, splits into a type and a name | form 8 |
| 3 | no top-level `=`, and the text splits into a type and a name | form 1 |
| 4 | no top-level `=`, and it does not | form 9 (expression statement) |
| 5 | has a top-level `=`, and the left side splits into a type and a name | form 2, or form 3 when the right side is `{ … }` |
| 6 | has a top-level `=`, and the left side does not | form 4/5, or form 6/7 when the target contains a `.` |

"Splits into a type and a name" means: split at the **last** top-level whitespace character, tracking
`( )` depth and string state; both halves must be non-empty. This is why `UE.Panner(Speed = 0.1) P;`
is a declaration — the space inside the argument list is not at top level.

> [!WARNING]
> The keyword probe in rule 1 is case-sensitive and is the **only** case-sensitive construct in the
> statement grammar. `If (x) { … }` is not an `if` statement: it falls through to rule 3, splits into
> type `If (x)` / name `{ … }`, and fails with `Unsupported Graph variable type 'If (x)'.`

> [!NOTE]
> The name half of rules 3 and 5 is only required to be **non-empty** — it is not validated as an
> identifier. `float3 A.B = x;` is therefore a declaration of a variable literally named `A.B`, not a
> member write; member writes are only reachable through rule 6, which requires the left side to
> *not* split into a type and a name. Declarators after the first in form 8 **are** identifier-checked.

## Assignment

Forms 4 and 5. The target is the left side of the top-level `=`, taken verbatim after trimming.

```c
<target> = <expression> ;
<target> = { <expression> , … } ;
```

The target is resolved in this order:

| Order | Target names | Behaviour |
| --: | :-- | :-- |
| 1 | a name containing a `.` that splits into two non-empty halves | `MaterialAttributes` member write — see [MaterialAttributes](material-attributes.md) |
| 2 | an existing `Graph` value | the value is coerced to the **existing** binding's shape: component count, texture flag, texture type, Substrate flag |
| 3 | an `Outputs` declaration of the enclosing block | the value is coerced to the **declared** type |
| 4 | nothing yet | a new variable is created carrying the value's own shape — no type token needed |

Lookup in rules 2 and 3 is **case-insensitive**: an exact match is tried first, then a
case-insensitive scan. See [Name resolution](name-resolution.md).

Rule 4 means an undeclared name on the left of `=` is not an error:

```c
Graph = {
    vec3 Base = Tint * 2.0;
    Scratch   = Base.rgb;    // legal: creates 'Scratch' as a 3-component value
    Color     = Scratch;     // 'Color' is an Outputs declaration -> coerced to its type
}
```

Coercion at rules 2 and 3 **silently narrows** a wider value to the target width by prefixing an
`r` / `rg` / `rgb` mask, and splats a scalar up to the target width. It never widens a 2-component
value to 3. See [Conversions](conversions.md).

### Brace-initializer assignment

A right-hand side whose trimmed text is at least two characters long and starts with `{` and ends
with `}` is a brace initializer. It is re-serialised as `<TargetType>(<inner>)` and evaluated as a
constructor call, so it obeys every constructor rule — including "no named arguments" and the
single-scalar splat. `{}` produces the target type's default value.

The target type comes from, in order: the declared type token; the attribute's type for a member
write; the existing variable's component count; the matching `Outputs` declaration. Texture and
`Substrate` targets are rejected outright. Details and the full resolution table are on
[Declarations](declarations.md); the constructor rules are on [Constructors](constructors.md).

## Expression statements

Form 9. A statement that is neither a declaration nor an assignment is evaluated as an expression,
and is accepted only when it is a **call**:

| Requirement | Diagnostic when unmet |
| :-- | :-- |
| the expression is a call | `Graph expression statements currently support only Function calls with explicit out arguments.` |
| the callee flattens to a name | `Graph expression statements must call a named Function.` |
| the name resolves to exactly one `Function`, `GraphFunction`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` or `VirtualFunction` | `Graph expression statement '{Text}' is unsupported. Only DreamShader Function, GraphFunction, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction calls may use statement syntax.` |
| exactly one, not several | `Graph expression statement '{Text}' is ambiguous because multiple callable definitions exist.` |

In statement form the arguments are the declared inputs followed by **one plain variable name per
output**, in declaration order. Statement calls are positional-only for every callee kind. Full
argument rules, out-target constraints and the value-call form are on [Calls](calls.md).

Statement-form multi-output `ShaderFunction` / `VirtualFunction` calls are available
*(since 1.3.5)*; single-output `Function` / `GraphFunction` value calls *(since 1.3.1)*.

## Synthesized statements

Every `Outputs` declaration of the enclosing block that carries a default value is turned into a
declaration statement and **prepended** before the first statement of the `Graph` body. These
statements have no source location; their diagnostics therefore carry no usable line number.

| Message | Cause |
| :-- | :-- |
| `Output declaration initializer requires a type and name.` | the synthesized declaration had a blank type or name |
| `Output declaration '{Name}' has an empty initializer.` | the `Outputs` default value text was empty |

This is also what makes `Graph = { }` legal for a `Shader`: the initialized output declaration is the
body. See [Shader](../language/shader.md).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

### Statement wrapping

| Message | Cause |
| :-- | :-- |
| `In Graph statement '{Text}': {Detail}` | any parse failure inside a statement; `{Detail}` is the inner message |
| `In Graph statement '{Text}': '{Declarator}' is not a valid declarator in a comma-separated declaration.` | a declarator after the first is not a bare identifier |
| `In Graph if body: {Detail}` / `In Graph else body: {Detail}` | a failure inside a branch body |

### Parse time

| Message | Cause |
| :-- | :-- |
| `Unexpected token '{Token}' in Graph expression.` | tokens remain after a complete expression, or a non-primary token appears where a value was expected |
| `Expected token type {Code} in Graph expression near '{Text}'.` | a `,` or `)` is missing from a call argument list |
| `Expected member name after '.'.` | `.` is not followed by an identifier |
| `Expected function name after '::'.` | `::` is not followed by an identifier |
| `Empty Graph expression.` | the expression text was empty |
| `Invalid numeric literal '{Text}'.` | a number token that converts to zero while containing a character no spelling of zero can contain — in practice an underflowing exponent such as `1e-9999`. A token with a trailing remainder (`0.5.5`) converts to its valid prefix instead — see [Literals](literals.md) |

### Build time

| Message | Cause |
| :-- | :-- |
| `Encountered an invalid empty Graph statement.` | a statement with no expression, no declaration and no brace initializer |
| `Encountered a Graph assignment without a target variable.` | the left side of `=` was empty, e.g. `= 5;` |
| `Failed to evaluate Graph assignment for '{Name}'. {Detail}` | the right-hand side failed to evaluate |
| `Failed to assign Graph member '{Name}'. {Detail}` | a member write failed |
| `MaterialAttributes member assignment '{Target}' requires a value.` | form 6 with no right-hand side |
| `Graph variable '{Name}' is declared more than once.` | redeclaration; the check is case-insensitive |
| `Graph variable '{Name}' was previously assigned an incompatible value. {Detail}` | assignment rule 2 could not coerce |
| `Graph output variable '{Name}' was assigned an incompatible value. {Detail}` | assignment rule 3 could not coerce |
| `Graph builder is not initialized.` | internal guard; the builder was run without a target material |

Progress text emitted while the statements run:
`Building DreamShader graph nodes ({N} statements)...`,
`Evaluating DreamShader graph statement {I} of {N}...`,
`Evaluating DreamShader graph statement {I} of {N}: '{Name}'...`. For bodies with more than 512
statements only every 64th statement updates the text.

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

Every statement form in one body:

```c
ShaderFunction(Name="Functions/F_AllForms")
{
    Properties = { vec3 Tint = vec3(1.0, 0.4, 0.1); float K = 2.0; }
    Inputs     = { vec2 UV; }
    Outputs    = { MaterialAttributes Attrs; vec3 Debug; }

    Graph = {
        MaterialAttributes Attrs;              //    the Outputs declaration does not create it
        float3 c;                              // 1  declaration, no initializer
        float3 scaled = Tint * K;              // 2  declaration + expression
        vec4   packed = {scaled, 1.0};         // 3  declaration + brace initializer
        c = packed.rgb;                        // 4  assignment
        Debug = {0.0, 0.0, 0.0};               // 5  assignment + brace initializer
        Attrs.BaseColor = c;                   // 6  member write
        Attrs.Roughness = {0.35};              // 7  member write + brace initializer
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
- [if / else](if.md) — condition splitting, the truth table, branch merging
- [Calls](calls.md) — value-form and statement-form calls, out targets, named arguments
- [MaterialAttributes](material-attributes.md) — member reads and writes
- [Conversions](conversions.md) — what "coerced to the existing shape" does
- [Name resolution](name-resolution.md) — how a bare identifier is looked up
- [Unsupported constructs](unsupported.md) — `for`, `while`, `return`, `+=`, ternary, and their real messages
- [Node reuse](node-reuse.md) — why two identical statements can produce one node
- [Output bindings](../language/output-bindings.md) — `Outputs` declarations and their initializers
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
