# if / else

> [DreamShader](../index.md) » [Graph](index.md) » **if**

The only control-flow statement in a `Graph` block: a build-time construct that evaluates **both**
branches into material nodes and selects between the two results with a conditional node per
variable.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, including inside another `if` or `else` body |
| Kind | statement |
| Generates | **one conditional node per variable** the branches change: `UMaterialExpressionIf`, or `UMaterialExpressionStaticSwitch` when the condition is a static bool parameter |

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
| Condition parentheses | **Required.** `if x > 0 { }` is [`DSH2157`](../diagnostics/DSH2xxx.md#dsh2157). |
| Body braces | **Required** on both branches. A single-statement body without braces is [`DSH2209`](../diagnostics/DSH2xxx.md#dsh2209). |
| Statement terminator | **Not required.** The statement ends with the `}` of its last body, so no `;` follows it. A stray `;` after `}` is an empty statement. |
| Keyword case | **`if` and `else` are matched case-sensitively**, like every keyword. Type tokens, constructors and `true` / `false` are matched in any case; another name in the wrong case is found with the warning [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275). |
| Body contents | Every statement form is legal inside a branch: declarations, assignments, `MaterialAttributes` member writes, standalone calls, and nested `if` statements. |
| `else if` | An `if` statement in the `else` slot. An `else if` chain is therefore a tree of nested statements, not a flat list. |

> [!WARNING]
> `If (x) {}` and `ELSE {}` are **not** control flow. Because the keyword match is case-sensitive,
> `If (x)` is a call to a function named `If`, and the `{` where that statement's `;` belongs is
> [`DSH2154`](../diagnostics/DSH2xxx.md#dsh2154) — a code that does not mention `if` at all. See
> [Unsupported constructs](unsupported.md#wrong-case-keywords).

## Condition

The condition is one expression, read by the expression parser *(since 2.0.0; 1.x split the text
at the first comparison operator before parsing either side)*.

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

### How the condition is read

1. The condition is parsed as one expression. A missing operand — `if ()`, `if (Mask >)` — is
   [`DSH2151`](../diagnostics/DSH2xxx.md#dsh2151) *(since 2.0.0; 1.x read `if (Mask >)` as
   `if (Mask != 0)`)*.
2. When the expression is a comparison, its two sides are the operands. Anything else is the
   **truthy** form.
3. Each operand is a 1.x value expression: a second comparison, `&&` or `||` is
   [`DSH2210`](../diagnostics/DSH2xxx.md#dsh2210), and the other operators 1.x did not have are
   refused as anywhere else (`DSH2200`–`DSH2222`, see [Unsupported constructs](unsupported.md)).
4. The binder types the operands: both must be numbers ([`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226)
   for a texture, a `MaterialAttributes` or a `Substrate` value), and the condition must be a single
   value — a comparison of two vectors is [`DSH4260`](../diagnostics/DSH4xxx.md#dsh4260).

> [!WARNING]
> **`&&` and `||` are refused** *(since 2.0.0)*. 1.x silently dropped everything after the first
> comparison, so `if (a > 0 && b > 0)` compiled as `if (a > 0)` with no diagnostic. It is
> `DSH2210` now.
>
> ```c
> // Refused (DSH2210); 1.x built if (Mask > 0.5).
> if (Mask > 0.5 && Alpha > 0.5) { Color = Hot; } else { Color = Cold; }
>
> // Write nested ifs instead:
> if (Mask > 0.5) {
>     if (Alpha > 0.5) { Color = Hot; } else { Color = Cold; }
> } else {
>     Color = Cold;
> }
> ```
>
> The same holds for `%`, `?:`, `[ ]`, `~`, `!` and the shift and bitwise operators anywhere in a
> condition: each is an error with its code. See
> [Unsupported constructs](unsupported.md#operators-1x-truncated).

## Truthy semantics

`if (x)` is wired as **`x != 0`**, not as `x > 0`.

| Form | Right operand | Then-branch taken when |
| :-- | :-- | :-- |
| `if (x)` | a `Constant` node holding `0` | `x != 0`, i.e. `x > 0` **or** `x < 0` |

> [!WARNING]
> A **negative** value is truthy. `if (Height)` selects the then-branch for `Height = -1.0` exactly as
> it does for `Height = 1.0`. This matches HLSL/C semantics and the decompiler's `!= 0` convention,
> but it is not what `if (Height > 0)` means. Write the comparison explicitly when the sign matters.

## Node wiring

One conditional node is created per merged variable.

| Condition | Node |
| :-- | :-- |
| a comparison of two scalars | `If`: `A` receives the left operand, `B` the right one |
| truthy, or any other single value | `If`: `A` receives the value, `B` a `Constant` holding `0` |
| a `StaticBoolParameter` property, or a `StaticBool` function input | `StaticSwitch`: `Value` receives the parameter, `A` (true) the then-value, `B` (false) the else-value |

The three result pins of an `If` are wired from the branch values:

| Operator | `AGreaterThanB` | `AEqualsB` | `ALessThanB` |
| :-- | :-- | :-- | :-- |
| `>` | then | else | else |
| `<` | else | else | then |
| `>=` | then | then | else |
| `<=` | else | then | then |
| `==` | else | then | else |
| `!=` | then | else | then |
| truthy | then | else | then |

A `bool` property that is not a `StaticBoolParameter` — a plain `StaticBool` or `bool` entry — is a
dynamic parameter *(since 2.0.0)*, and `if` on it is the truthy `If`. See
[Properties](../language/properties.md).

The result is as wide as the wider of the two branch values. It is never a texture object.

## Both branches are always built

There is no dead-code elimination and no build-time decision on the condition, even a constant one.
Lowering an `if` statement does all of the following, in order:

1. Lower the condition once.
2. Lower every then-statement, starting from the values as they stood before the `if`.
3. Lower every else-statement, starting from the same values.
4. Merge (below).

Consequences:

- **Every node in both branches that feeds a merged value exists in the generated material.** A
  branch that is never taken at runtime still costs shader instructions. `if` is a select, not a jump.
- An error inside either branch fails the build, even in a branch that a constant condition could
  never reach. Each error is reported at its own line and column.
- A variable declared inside a branch is local to it *(since 2.0.0)*: it is not merged, and the name
  is unknown after the `if`. See [Declarations](declarations.md#scope).
- Identical nodes in the two branches and after the merge are merged by the deduplication pass. See
  [Node reuse](node-reuse.md).
- The condition's operands are shared, but **N** merged variables make **N** conditional nodes.

## The merge

A variable is merged when it was declared **before** the `if` and either branch changed it.
Properties are never merged: they are inputs, and reading one inside a single branch changes
nothing.

| Variable | Result |
| :-- | :-- |
| changed in both branches | one conditional node over the two values |
| changed in one branch only | one conditional node over the changed value and the value from before the `if` |
| declared inside a branch | dropped at the end of the branch; nothing is merged |
| had no value before the `if` and is assigned in one branch only (a function output not yet written, for instance) | marked; a later read is [`DSH4372`](../diagnostics/DSH4xxx.md#dsh4372) |
| a `MaterialAttributes` value | merged attribute by attribute, one conditional node per attribute the branches disagree on |

> [!NOTE]
> The value of a variable is converted to its declared type at every assignment, so the two branch
> values of one variable always have the same type.

> [!WARNING]
> **Branch-local declarations stay in their branch** *(since 2.0.0)*. 1.x merged a name declared in a
> branch, demanded it in the other branch too, and kept it after the `if`. Now a `float3 Temp = …;`
> declared in one branch is gone after that branch, and the same name declared in both branches is
> two unrelated variables. Declare the variable **before** the `if` when the code after it needs it:
>
> ```c
> // 'Blend' is local to each branch: Color = Blend; after the if is DSH4200.
> if (Mask > 0.5) { float3 Blend = float3(1.0, 0.0, 0.0); Color = Blend; }
> else            { float  Blend = 0.25;                  Color = float3(Blend, Blend, Blend); }
>
> // One declaration, one type, assigned in both branches.
> float3 Blend = float3(0.0, 0.0, 0.0);
> if (Mask > 0.5) { Blend = float3(1.0, 0.0, 0.0); }
> else            { Blend = float3(0.25, 0.25, 0.25); }
> Color = Blend;
> ```

## What cannot be selected

| Value kind | Behaviour |
| :-- | :-- |
| scalar / vector (1–4 components) | selected normally |
| `MaterialAttributes` | selected attribute by attribute; a different **whole** set in each branch is [`DSH4375`](../diagnostics/DSH4xxx.md#dsh4375) |
| texture object, sampler | rejected — [`DSH4379`](../diagnostics/DSH4xxx.md#dsh4379) |
| `Substrate` | a `StaticSwitch` under a static condition; otherwise a `Substrate.Select` node (UE 5.6+), or [`DSH4378`](../diagnostics/DSH4xxx.md#dsh4378) on an older engine. [`DSH4380`](../diagnostics/DSH4xxx.md#dsh4380) warns when the two values are different kinds of BSDF |

## Diagnostics

### Statement shape

| Condition | Code |
| :-- | :-- |
| No `(` after `if` | `DSH2157` |
| The condition's `(` is never closed | [`DSH2152`](../diagnostics/DSH2xxx.md#dsh2152) |
| `if ()`, or a condition with a missing operand | `DSH2151` |
| A body without braces, after `if` or `else` | `DSH2209` |
| A body's `{` is never closed | [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150), at the end of the file |
| `If`, `Else` and other mis-cased keywords | `DSH2154` |

### Condition

| Condition | Code |
| :-- | :-- |
| A second comparison, `&&` or `\|\|` | `DSH2210` |
| `!`, `?:` | [`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200) |
| `%`, `~`, a shift or a bitwise operator | [`DSH2201`](../diagnostics/DSH2xxx.md#dsh2201) |
| An assignment inside the condition | [`DSH2204`](../diagnostics/DSH2xxx.md#dsh2204) |
| An operand that is not a number | `DSH4226` |
| A comparison of two vectors | `DSH4260` |

### Merge

| Condition | Code |
| :-- | :-- |
| A variable with no value before the `if`, assigned in one branch only, is read after it | `DSH4372` |
| The branches leave a different whole `MaterialAttributes` set in one variable | `DSH4375` |
| A texture or sampler differs between the branches | `DSH4379` |
| A `Substrate` value against a number, or a run-time `Substrate` branch on an engine without `Substrate.Select` | `DSH4378` |
| *warning:* the `Substrate.Select` blends two different kinds of BSDF | `DSH4380` |

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
- [Unsupported constructs](unsupported.md) — loops, `switch`, `?:`, and the codes that refuse them
- [Declarations](declarations.md) — declaration scope and the redeclaration rule
- [Conversions](conversions.md) — the conversion applied at every assignment
- [Node reuse](node-reuse.md) — why branch subexpressions collapse into shared nodes
- [MaterialAttributes](material-attributes.md) — selecting attribute values with `if`
- [Name resolution](name-resolution.md) — why a parameter read inside a branch is not a branch output
- [Calls](calls.md) — calling functions from inside a branch
- [Diagnostics index](../diagnostics/index.md) — every code, by stage
