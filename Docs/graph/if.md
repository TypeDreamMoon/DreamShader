# if / else

> [DreamShader](../index.md) » [Graph](index.md) » **if**

The only control-flow statement in a `Graph` block: a build-time construct that evaluates **both**
branches into material nodes and selects between the two results with a `UMaterialExpressionIf`.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, including inside another `if` or `else` body |
| Kind | statement |
| Generates | `UMaterialExpressionIf` — **one per variable** the branches change; plus one `UMaterialExpressionConstant` holding `0` for the truthy form |

## Synopsis

```c
if ( <condition> )
{
    <graph-statement> …
}
[ else
{
    <graph-statement> …
} ]
```

```c
if ( <condition> ) { … }
else if ( <condition> ) { … }
…
[ else { … } ]
```

```c
<condition> := <expression> [ { >= | <= | == | != | > | < } <expression> ]
```

`if`, `else`, `( )`, `{ }` are literal DreamShaderLang punctuation. `[ … ]`, `{ a | b }` and `…` are
meta-notation and are never typed.

## Requirements

| Rule | Detail |
| :-- | :-- |
| Condition parentheses | **Required.** `if x > 0 { }` fails with `Graph if statement is missing a condition block.` |
| Body braces | **Required** on both branches. A single-statement body without braces fails with `Graph if statement is missing a '{ ... }' body.` |
| Statement terminator | **Not required.** The statement extent is computed by brace matching, so no `;` follows the closing `}`, and a `;` inside a body never splits the statement. A stray `;` after `}` is skipped as an empty statement. |
| Keyword case | **`if` and `else` are matched case-sensitively.** These are the only two keywords in the Graph language; every other name (types, constructors, builtins, `true`/`false`, argument names) is case-insensitive. |
| Body contents | Every statement form is legal inside a branch: declarations, assignments, `MaterialAttributes` member writes, standalone calls, and nested `if` statements. |
| `else if` | Captured as raw text and re-parsed as a nested `if` statement inside `ElseStatements`. An `else if` chain is therefore a tree of nested statements, not a flat list. |

> [!WARNING]
> `If (x) {}` and `ELSE {}` are **not** control flow. Because the keyword match is case-sensitive, the
> text falls into the declaration classifier: `If (x) {}` alone reports
> `Failed to declare Graph variable '{}'. Unsupported Graph variable type 'If (x)'.` — a message that
> does not mention `if` at all. See [Unsupported constructs](unsupported.md#wrong-case-keywords).

## Condition

The condition text is split **textually**, before the expression parser runs. It is not part of the
expression grammar — see [Expressions](expressions.md#operators-that-do-not-exist).

### Operators

Exactly six comparison operators exist, and only here.

| Operator | Meaning |
| :-- | :-- |
| `>` | left greater than right |
| `<` | left less than right |
| `>=` | left greater than or equal to right |
| `<=` | left less than or equal to right |
| `==` | left equal to right |
| `!=` | left not equal to right |
| *(none)* | the **truthy** form — see below |

### How the split is performed

1. The condition text is scanned left to right at nesting depth 0. `( )`, `{ }`, `[ ]` depth and
   string literals are tracked and skipped.
2. At each character position the operators are probed in the fixed order
   `>=`, `<=`, `==`, `!=`, `>`, `<`. The **first** position that matches wins, and a two-character
   operator is always probed before the one-character operator that prefixes it, so `a >= b` splits on
   `>=` and never on `>`.
3. If a match is found but either side is empty after trimming, the split is **discarded** and the
   whole text becomes a truthy condition instead.
4. With no operator match, the whole text is the left operand and the operator is the internal
   pseudo-operator `truthy`.
5. Each side is then handed to the ordinary expression parser. A failure is reported as
   `In Graph if condition '{Condition}': {Error}`.

Both operands must evaluate to a **scalar** (one component) and must not be a texture object, a
`MaterialAttributes` value or a `Substrate` value.

> [!WARNING]
> **`&&` and `||` are silently dropped.** `&` and `|` are not tokens; the expression parser treats
> them as end-of-input and accepts everything before them. In `if (a > 0 && b > 0)` the first `>`
> splits the text into left `a` and right `0 && b > 0`; the right-hand side parses successfully as
> `0`, and the compiled condition is exactly `a > 0`. No diagnostic is produced.
>
> ```c
> // Written — compiles, but the second test is discarded.
> if (Mask > 0.5 && Alpha > 0.5) { Color = Hot; } else { Color = Cold; }
>
> // Equivalent, and what the compiler actually built:
> if (Mask > 0.5) { Color = Hot; } else { Color = Cold; }
>
> // Write nested ifs instead:
> if (Mask > 0.5) {
>     if (Alpha > 0.5) { Color = Hot; } else { Color = Cold; }
> } else {
>     Color = Cold;
> }
> ```
>
> The same fate awaits `%`, `?:`, `[ ]`, `~` and the shift and bitwise operators anywhere in a
> condition. See [Unsupported constructs](unsupported.md#silently-truncated-expressions).

> [!NOTE]
> A condition whose right side is empty, such as `if (Mask >)`, does **not** error. The split is
> rejected because the right operand is blank, the whole text `Mask >` becomes a truthy condition,
> and the trailing `>` is discarded as end-of-input. The result is `if (Mask != 0)`.

## Truthy semantics

`if (x)` is wired as **`x != 0`**, not as `x > 0`.

| Form | Right operand | Then-branch taken when |
| :-- | :-- | :-- |
| `if (x)` | a synthesized `Constant` node holding `0` | `x != 0`, i.e. `x > 0` **or** `x < 0` |

> [!WARNING]
> A **negative** value is truthy. `if (Height)` selects the then-branch for `Height = -1.0` exactly as
> it does for `Height = 1.0`. This matches HLSL/C semantics and the decompiler's `!= 0` convention,
> but it is not what `if (Height > 0)` means. Write the comparison explicitly when the sign matters.

## Node wiring

One `UMaterialExpressionIf` node is created per merged variable. `A` receives the condition's left
operand, `B` the right operand (or the synthesized zero), and the three result pins are wired from the
branch values.

| Operator | `AGreaterThanB` | `AEqualsB` | `ALessThanB` |
| :-- | :-- | :-- | :-- |
| `>` | then | else | else |
| `<` | else | else | then |
| `>=` | then | then | else |
| `<=` | else | then | then |
| `==` | else | then | else |
| `!=` | then | else | then |
| truthy | then | else | then |

The result value takes its component count and its `MaterialAttributes` flag from the **then**
branch. It is never a texture object and never a `Substrate` value.

## Both branches are always built

There is no dead-code elimination and no build-time constant folding of the condition. Executing an
`if` statement does all of the following, in order:

1. Copy the current variable map; run every then-statement against the copy.
2. Copy the current variable map again; run every else-statement against that copy.
3. Restore the enclosing map and merge (below).

Consequences:

- **Every node in both branches exists in the generated material.** A branch that is never taken at
  runtime still costs shader instructions. `if` is a select, not a jump.
- An error inside either branch fails the build, even a branch that a constant condition could never
  reach. Body errors are wrapped as `In Graph if body: {Error}` / `In Graph else body: {Error}`.
- A variable declared inside a branch does **not** stay local: it participates in the merge (see
  below), so declaring the same name in only one branch is an error.
- Node reuse is *not* scoped to a branch. A subexpression first built in the then-branch is reused
  verbatim in the else-branch and after the merge. See [Node reuse](node-reuse.md).
- The condition is evaluated once per merged variable. Because the operands go through the reuse
  cache, this creates one set of condition nodes, but **N** `If` nodes for **N** merged variables.

## The merge

A name is a **branch output** when it is present in a branch's map and either absent from the
enclosing map or bound to a different value there. Two values are considered the same only when the
node, output index, component count, all five channel-mask fields, the texture flag, the texture
type, the `MaterialAttributes` flag and the `Substrate` flag all match.

| Step | Rule | Failure |
| :-- | :-- | :-- |
| 1 | Names that match a declared property or parameter are **skipped** — materialising a parameter node is a read side effect, not an assignment | — |
| 2 | The name must be present in **both** branch maps | `Graph if statement could not resolve both branch values for '{Name}'.` |
| 3 | Expected shape = the enclosing value's shape, if the name existed before the `if` | — |
| 4 | Otherwise, expected shape = the matching `Outputs` declaration's shape, if there is one | — |
| 5 | Otherwise, the two branch values must agree exactly on component count, texture flag, texture type, `MaterialAttributes` flag and `Substrate` flag | `Graph if branches assign variable '{Name}' with inconsistent types` *(no trailing period)* |
| 6 | The expected shape must not be a texture object | `Graph if statement cannot select texture value '{Name}'.` |
| 7 | The expected shape must not be a `Substrate` value | `Graph if statement cannot select Substrate value '{Name}'.` |
| 8 | Both branch values are coerced to the expected shape | `Graph if branches assign incompatible values to '{Name}'. {Error}` |
| 9 | The `If` node is created and wired | `Graph if statement failed to merge '{Name}'. {Error}` |

> [!NOTE]
> Step 1 is why reading a parameter inside a single branch is legal. Without it,
> `if (m > T) { Color = Lit.rgb; } else { Color = Dark.rgb; }` would fail, because `Lit` is
> materialised into the then-map only and `Dark` into the else-map only.

> [!WARNING]
> **Branch-local declarations leak into the merge.** A `float3 Temp = …;` declared only in the
> then-branch is a branch output, so the merge demands a `Temp` in the else-branch too and otherwise
> fails with `Graph if statement could not resolve both branch values for 'Temp'.` Declaring the name
> in both branches with different widths fails with
> `Graph if branches assign variable 'Temp' with inconsistent types`. Declare the variable **before**
> the `if` when both branches need it:
>
> ```c
> // Fails: 'Blend' is float3 in one branch and float in the other.
> if (Mask > 0.5) { float3 Blend = float3(1.0, 0.0, 0.0); Color = Blend; }
> else            { float  Blend = 0.25;                  Color = float3(Blend, Blend, Blend); }
>
> // Works: one declaration, one shape, assigned in both branches.
> float3 Blend = float3(0.0, 0.0, 0.0);
> if (Mask > 0.5) { Blend = float3(1.0, 0.0, 0.0); }
> else            { Blend = float3(0.25, 0.25, 0.25); }
> Color = Blend;
> ```

## What cannot be selected

| Value kind | Behaviour |
| :-- | :-- |
| scalar / vector (1–4 components) | selected normally |
| `MaterialAttributes` | selected, provided **both** branches produce a `MaterialAttributes` value |
| texture object | rejected — `Graph if statement cannot select texture value '{Name}'.` |
| `Substrate` | rejected — `Graph if statement cannot select Substrate value '{Name}'.` |
| mixed `MaterialAttributes` and numeric | rejected — `Graph if branches cannot mix MaterialAttributes and numeric values.` |

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

### Statement splitting

These fire first, while the statement's extent is being determined.

| Message | Cause |
| :-- | :-- |
| `Graph if statement is missing a condition block.` | No `(` after `if`. |
| `Graph if statement has an unterminated condition block.` | The condition's `(` is never closed. |
| `Graph if statement is missing a '{ ... }' body.` | No `{` after the condition — including a braceless single-statement body. |
| `Graph if statement has an unterminated body block.` | The then-body's `{` is never closed. |
| `Graph else statement is missing a '{ ... }' body.` | `else` is followed by neither `{` nor `if`. |
| `Graph else statement has an unterminated body block.` | The else-body's `{` is never closed. |

### Parsing

| Message | Cause |
| :-- | :-- |
| `Graph if condition is empty.` | `if ()`, or a condition that is only whitespace. |
| `In Graph if condition '{Condition}': {Error}` | Either side of the condition failed to parse — commonly `!x`, `a = b`, or a leading `>`/`<`. |
| `In Graph if body: {Error}` | A statement inside the then-body failed to parse. |
| `In Graph else body: {Error}` | A statement inside the else-body failed to parse. |
| `Invalid Graph if statement '{Statement}'.` | Defensive re-validation of the condition parentheses; the statement-splitting message above reports the same shape first. |
| `Invalid Graph if body in '{Statement}'.` | Defensive re-validation of the then-body braces. |
| `Invalid Graph else body in '{Statement}'.` | Defensive re-validation of the else-body braces. |
| `Unexpected text after Graph if statement: '{Text}'.` | Defensive guard for text following the last body brace. |

### Building

| Message | Cause |
| :-- | :-- |
| `Graph builder is not initialized.` | Internal: an `if` statement was executed without an active build context. |
| `Failed to evaluate Graph if condition. {Error}` | The left or right operand expression could not be evaluated. |
| `Graph if condition left side must evaluate to a scalar value.` | The left operand is a vector, a texture object, a `MaterialAttributes` value or a `Substrate` value. |
| `Graph if condition right side must evaluate to a scalar value.` | As above, for the right operand. |
| `Failed to create a zero literal for Graph if condition.` | The truthy form's `Constant(0)` node could not be created. |
| `Unsupported Graph if comparison operator '{Operator}'.` | Defensive guard; the splitter can only produce the six operators and `truthy`. |
| `Failed to create a Material If node.` | The `UMaterialExpressionIf` node could not be created. |
| `Graph if statement could not resolve both branch values for '{Name}'.` | The name is a branch output in one branch only — usually a declaration or assignment that exists in only one body. |
| `Graph if branches assign variable '{Name}' with inconsistent types` | New in both branches with different shapes, and no enclosing variable or `Outputs` declaration fixes the expected shape. Emitted without a trailing period. |
| `Graph if statement cannot select texture value '{Name}'.` | A texture object is a branch output. |
| `Graph if statement cannot select Substrate value '{Name}'.` | A `Substrate` value is a branch output. |
| `Graph if branches assign incompatible values to '{Name}'. {Error}` | A branch value could not be coerced to the expected shape. |
| `Graph if statement failed to merge '{Name}'. {Error}` | The `If` node could not be built for this name. |
| `Texture values cannot be selected by Graph if statements.` | Either branch value is a texture object. |
| `Substrate values cannot be selected by Graph if statements.` | Either branch value is a `Substrate` value. |
| `Graph if branches cannot mix MaterialAttributes and numeric values.` | One branch produces a `MaterialAttributes` value and the other a numeric value. |

## Example

```c
Shader(Name="Docs/M_IfElse")
{
    Properties {
        float Mask = 0.6;
        vec3  Tint = vec3(1.0, 0.2, 0.2);
    }

    Settings {
        Domain       = "UI";
        ShadingModel = "Unlit";
    }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        if (Mask > 0.5) {
            Color = Tint;
        } else if (Mask > 0.25) {
            Color = Tint * 0.5;
        } else {
            Color = vec3(0.0, 0.0, 0.0);
        }
    }
}
```

Generated nodes:

```text
VectorParameter  Tint
ScalarParameter  Mask
Constant         0.5                 (shared)
Constant         0.25
Multiply         Tint * 0.5
Constant3Vector  (0,0,0)             (folded literal)
If               A=Mask B=0.25  ->   inner else-if merge of 'Color'
If               A=Mask B=0.5   ->   outer merge of 'Color'
```

Both `If` nodes exist because the `else if` is a nested `if` statement, and every branch's nodes are
present in the material regardless of the runtime value of `Mask`.

## See also

- [Statements](statements.md) — every statement form, including the ones legal inside a branch
- [Expressions](expressions.md) — the operator grammar the condition operands are parsed with
- [Unsupported constructs](unsupported.md) — loops, `switch`, `?:`, and the silent-truncation catalogue
- [Declarations](declarations.md) — declaration scope and the redeclaration rule
- [Conversions](conversions.md) — the coercion applied to both branch values at the merge
- [Node reuse](node-reuse.md) — why branch subexpressions collapse into shared nodes
- [MaterialAttributes](material-attributes.md) — selecting attribute values with `if`
- [Name resolution](name-resolution.md) — why a parameter read inside a branch is not a branch output
- [Calls](calls.md) — calling functions from inside a branch
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
