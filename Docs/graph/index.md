# Graph

> [DreamShader](../index.md) » **Graph**

The section whose body is a sequence of statements that the compiler executes **once, in order, at
generation time**, materializing one or more `UMaterialExpression` nodes per expression.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` |
| Kind | section |
| Generates | `UMaterialExpression` nodes on the target `UMaterial` / `UMaterialFunction` |
| Section name | matched **case-insensitively**; the `=` before `{` is optional *(since 1.5.0)* |

## Synopsis

```c
Graph [=] {
    <graph-statement> …
}
```

A `Graph` body is not an expression language embedded in a shader — it is a *node-construction*
program. Nothing in it survives to runtime as code. What survives is the graph it built.

## Where a `Graph` block may appear

| Block | `Graph` | `Code` |
| :-- | :-- | :-- |
| [`Shader`](../language/shader.md) | ✔ required unless an `Outputs` declaration carries an initializer | ✘ hard error |
| [`ShaderFunction`](../language/shader-function.md) | ✔ always required | ✘ hard error |
| [`ShaderLayer` / `ShaderLayerBlend`](../language/shader-layer.md) | ✔ always required | ✘ hard error |
| [`VirtualFunction`](../language/virtual-function.md) | ✘ hard error | ✘ hard error |
| [`Function` / `GraphFunction`](../language/function.md) | body is HLSL, not a `Graph` block | — |

> [!WARNING]
> `Code = { … }` is rejected in every block that reaches the parser. The rejection message itself
> still reads `… Function Code = { ... } is still supported.`, but the grammar that would accept a
> `Code` section has no entry point. Use `Graph`.

The same statement language is also used, in reduced form, in two more places:

| Place | Shape |
| :-- | :-- |
| An `Outputs` binding's right-hand side | one full expression, no statements |
| An `Outputs` declaration's `= <default>` | one expression, lowered into a synthesized declaration statement prepended before the `Graph` body |

## The evaluation model

1. **Initialized `Outputs` declarations run first.** Every `Outputs` declaration that carries a
   default value becomes a synthesized declaration statement placed **before** the first statement of
   the `Graph` body. Those synthesized statements have no source location, so their diagnostics carry
   no line number.
2. **The body is split into statements** at top-level `;`. Parentheses, braces, brackets and string
   literals are tracked, so a `;` inside them does not split. An `if` statement is recognized before
   the `;` scan and is delimited by brace matching instead.
3. **Statements execute once each, top to bottom.** There is no loop construct, no `return`, no
   recursion, no re-entry. Statement *n* can only see values produced by statements `1 … n-1`.
4. **Every expression evaluates to a value descriptor**, not to a number: the `UMaterialExpression`
   that produced it, an output index, an optional channel mask, a component count, and flags marking
   texture object / `MaterialAttributes` / `Substrate` / authoritative-width / integer-constructor.
   A variable name is a binding to one such descriptor.
5. **Nodes are created eagerly** as expressions are evaluated, and wired immediately.
6. **`Outputs` binding sources are evaluated last**, each as a full expression against the value map
   the body left behind.

### Consequences worth internalising

- **There is no runtime control flow.** An `if` executes **both** branches at build time; both branch
  node sets exist in the finished material, and a `UMaterialExpressionIf` selects between them per
  pixel. See [if / else](if.md).
- **A declared variable is not storage.** Assigning to a name rebinds it to a different node/pin; the
  previously bound node stays in the graph if something else still references it, and is dropped
  otherwise.
- **Identical subexpressions collapse.** The builder caches values by a structural key, so the same
  literal, the same `A + B`, the same `UE.TexCoord(Index=0)` produce **one** node no matter how many
  times they are written. See [Node reuse](node-reuse.md).
- **Some constructs create no nodes at all.** An ordered, non-repeating swizzle (`.rgb`, `.ga`) is
  lowered to a channel mask on the *connection*, not to a `ComponentMask` node. See
  [Swizzles](swizzle.md).
- **Type errors are shape errors.** `int`, `bool`, `half` and `float` are the same thing to the
  builder; what it checks is component count and the opaque-value flags. See
  [Conversions](conversions.md).
- **Unknown operator characters are silently discarded.** `%`, `&&`, `||`, `?:`, `[ ]`, `<<` and
  friends terminate the expression instead of failing it, so `a % b` evaluates to `a`. This is the
  single highest-value pitfall in the language — see [Unsupported constructs](unsupported.md).

## Statements at a glance

| Form | Example | Reference |
| :-- | :-- | :-- |
| Declaration | `float3 c = A * K;` | [Declarations](declarations.md) |
| Comma declarators | `float a = 1, b, c = 3;` | [Declarations](declarations.md) |
| Brace initializer | `vec4 v = {rgb, 1.0};` | [Declarations](declarations.md) |
| Assignment | `Color = Tint;` | [Statements](statements.md) |
| Attribute member write | `Attrs.BaseColor = Tint;` | [MaterialAttributes](material-attributes.md) |
| Standalone call | `F_Split(In, OutA, OutB);` | [Calls](calls.md) |
| `if` / `else` / `else if` | `if (m > .5) { … } else { … }` | [if / else](if.md) |

The full synopsis table, with flags and termination rules, is on [Statements](statements.md).

## Comments and layout

Comments are **not** stripped by the section parser — they reach the graph parser intact — and are
then removed with layout preserved: `//` line comments and `/* */` block comments are replaced by
spaces and newlines are kept, so every diagnostic line and column still points at the original
source. Block comments do not nest; the first `*/` closes.

String literals are honoured during comment removal, so a `//` inside `"…"` is not a comment.

## `#Region` / `#EndRegion`

`#Region "Name"` and `#EndRegion` directive lines inside a `Graph` body group the nodes produced by
the statements between them into a comment box in the generated graph. The directive lines are
replaced by an equal-length run of spaces before parsing, so they cost no line or column offsets.
Regions nest. Full syntax, nesting semantics and diagnostics are on [Layout](../language/layout.md).

## Diagnostic locations

Graph diagnostics are reported against the **source file**, not against the block:

```text
<file>(<line>,<column>): <message>
```

The line is the `Graph` block's first content line plus the in-block line, minus one; the column is
offset by the block's start column only on the block's first line. Errors raised while executing an
`if` body are prefixed `In Graph if body: ` / `In Graph else body: ` *after* the location, so the
file and line always come first.

## Example

```c
Shader(Name="DreamShaderTests/Corpus/M_Arithmetic")
{
    Properties = {
        vec3  A = vec3(1.0, 0.5, 0.2);
        vec3  B = vec3(0.1, 0.2, 0.3);
        float K = 2.0;
    }
    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs  = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = {
        vec3 Sum    = A + B;
        vec3 Diff   = A - B;
        vec3 Scaled = A * K;
        vec3 Ratio  = A / K;
        Color = Sum + Diff - Scaled + Ratio;
    }
}
```

Generated nodes:

```text
VectorParameter A, VectorParameter B, ScalarParameter K   (property nodes)
Add       (A, B)          -> Sum
Subtract  (A, B)          -> Diff
Multiply  (A, K)          -> Scaled
Divide    (A, K)          -> Ratio
Add       (Sum, Diff)
Subtract  (.., Scaled)
Add       (.., Ratio)     -> Base.EmissiveColor
```

## Pages in this section

| Page | Covers |
| :-- | :-- |
| [Statements](statements.md) | every statement form, termination rules, assignment, expression statements |
| [Declarations](declarations.md) | `T name = expr;`, comma declarators, brace initializers, defaults, scope |
| [Expressions](expressions.md) | operator table, precedence and associativity, unary operators |
| [Literals](literals.md) | numeric forms and suffixes, `true` / `false`, string literals |
| [Constructors](constructors.md) | the 34 constructor names, splatting, packing, constant folding |
| [Swizzles](swizzle.md) | channel sets, reorder and repeat, mask fast path vs `AppendVector` |
| [Conversions](conversions.md) | coercion order, widening, silent narrowing, authoritative widths |
| [if / else](if.md) | condition splitting, the comparison truth table, branch merging |
| [MaterialAttributes](material-attributes.md) | attribute values, member reads and writes, Substrate interop |
| [Calls](calls.md) | calling `Function`, `GraphFunction`, `ShaderFunction`, `VirtualFunction`, parameter pins |
| [Name resolution](name-resolution.md) | identifier and call lookup order, shadowing |
| [Node reuse](node-reuse.md) | the common-subexpression cache and what a user observes |
| [Unsupported constructs](unsupported.md) | loops, `return`, ternary, `%`, comparisons, indexing — and the silent truncations |

## See also

- [Shader](../language/shader.md) — the block that owns a `Graph` and its `Outputs`
- [ShaderFunction](../language/shader-function.md) — a `Graph` that becomes a `UMaterialFunction`
- [GraphFunction](../language/graph-function.md) — HLSL bodies with `UE.*` calls hoisted into pins
- [Type tokens](../language/types.md) — which tokens a `Graph` declaration accepts
- [Layout](../language/layout.md) — `#Region` / `#EndRegion` and node placement directives
- [UE builtins](../builtins/ue.md) — the complete `UE.*` catalogue callable from a `Graph`
- [Math builtins](../builtins/math.md) — `lerp`, `dot`, `saturate`, `fract`, `mod`, …
- [Reading parameters in Graph](../parameters/graph-usage.md) — bare reads and the pin call form
- [Graph layout](../generation/graph-layout.md) — how generated nodes are positioned
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
