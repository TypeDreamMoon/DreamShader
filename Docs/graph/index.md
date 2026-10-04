# Graph

> [DreamShader](../index.md) » **Graph**

The section whose body is a sequence of statements that the compiler lowers **once, in order, at
generation time** into the `UMaterialExpression` nodes of the generated material.

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

To write the same thing as DreamShaderLang 2.0 — with loops, `?:` and `&&` — see
[the 2.0 language](../language-v2/index.md); [`dsc migrate`](../tools/migrate.md) rewrites a 1.x
file for you.

## Where a `Graph` block may appear

| Block | `Graph` | `Code` |
| :-- | :-- | :-- |
| [`Shader`](../language/shader.md) | ✔ required unless the `Outputs` section declares or binds something ([`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255)) | ✘ [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) |
| [`ShaderFunction`](../language/shader-function.md) | ✔ — without one the body is empty | ✘ `DSH2246` |
| [`ShaderLayer` / `ShaderLayerBlend`](../language/shader-layer.md) | ✔ — without one the body is empty | ✘ `DSH2246` |
| [`VirtualFunction`](../language/virtual-function.md) | ✘ [`DSH2247`](../diagnostics/DSH2xxx.md#dsh2247) | ✘ `DSH2247` |
| [`Function` / `GraphFunction`](../language/function.md) | body is HLSL, not a `Graph` block | — |

> [!WARNING]
> `Code = { … }` is `DSH2246` in every block that has a `Graph`. Use `Graph`.

The same statement language is also used, in reduced form, in two more places:

| Place | Shape |
| :-- | :-- |
| An `Outputs` binding's right-hand side | one full expression, no statements |
| An `Outputs` declaration's `= <default>` | one expression: the initializer of a variable declared before the first `Graph` statement |

## The evaluation model

1. **The legacy front end reads the body into the 2.0 tree** *(since 2.0.0)*. A `Shader` becomes one
   material function whose body is, in order: every `Outputs` declaration (with its initializer, or a
   zero of its type), the `Graph` statements, and the `Outputs` bindings as assignments to the
   material. A function block's `Graph` is the body of the function its `Inputs` and `Outputs` declare.
2. **The binder types every name and expression** before any node exists. A construct 1.x did not
   have, or that does not type, stops the build with a code — the body is never half-built.
3. **Statements are lowered once each, top to bottom.** There is no loop construct, no `return`, no
   recursion, no re-entry. Statement *n* can only see values produced by statements `1 … n-1`.
4. **Every expression is a typed value**: a number of 1–4 components (of kind `float`, `half`, `int`,
   `uint` or `bool`), a texture, a sampler, a `MaterialAttributes` value or a `Substrate` value. A
   variable name is bound to one such value.
5. **Passes run over the lowered graph**: constant folding, deduplication of identical nodes, and
   pruning of every node nothing reads. The emitter then creates the `UMaterialExpression` nodes.
6. **`Outputs` binding sources are evaluated last**, each as a full expression against the values
   the body left behind.

### Consequences worth internalising

- **There is no runtime control flow.** An `if` lowers **both** branches at build time; both branch
  node sets exist in the finished material, and an `If` node (a `StaticSwitch` for a static condition)
  selects between them per variable. See [if / else](if.md).
- **A declared variable is not storage.** Assigning to a name rebinds it to a different node; the
  previously bound node stays in the graph if something else still reads it, and is pruned otherwise.
- **Identical subexpressions collapse.** The deduplication pass merges nodes with the same operation
  and the same operands, so the same literal, the same `A + B`, the same `UE.TexCoord(Index=0)`
  produce **one** node no matter how many times they are written. See [Node reuse](node-reuse.md).
- **Some constructs create no nodes of their own.** See [Swizzles](swizzle.md) for the cost of a
  channel selection.
- **Types are checked, widths included.** Two operands of different widths are
  [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) unless one is a scalar; a wider value stored into a
  narrower variable keeps its leading components with the info
  [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289); `int`, `uint`, `bool` and `half` are kinds of their
  own to the compiler and floats in the graph. See [Conversions](conversions.md).
- **Unknown operators are refused, not dropped** *(since 2.0.0)*. 1.x silently ended an expression at
  `%`, `&&`, `||`, `?:`, `[ ]`, `<<` and friends, so `a % b` built `a`; each is now an error with a
  code (`DSH2200`–`DSH2222`). See [Unsupported constructs](unsupported.md).

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

The full synopsis table, with termination rules, is on [Statements](statements.md).

## Comments and layout

Comments are read by the lexer *(since 2.0.0)*: `//` line comments and `/* */` block comments.
Block comments do not nest; the first `*/` closes, and one that never closes is
[`DSH2102`](../diagnostics/DSH2xxx.md#dsh2102). A `//` inside a string literal is not a comment.
Every diagnostic line and column is the position in the source file.

## `#Region` / `#EndRegion`

`#Region "Name"` and `#EndRegion` lines inside a `Graph` body group the nodes produced by the
statements between them into a comment box in the generated graph. Each is a line of its own and a
statement of the body *(since 2.0.0)*; regions nest. A `#Region` without a name, an `#EndRegion` with
none open, a region still open at the end of the body and any other `#` line are
[`DSH2216`](../diagnostics/DSH2xxx.md#dsh2216)–[`DSH2219`](../diagnostics/DSH2xxx.md#dsh2219), or
[`DSH1035`](../diagnostics/DSH1xxx.md#dsh1035) when the preprocessor does not know the line. Full
syntax and nesting semantics are on [Layout](../language/layout.md).

## Diagnostic locations

Every diagnostic is reported against the **source file**, at the construct it is about:

```text
<file>(<line>,<column>): DSHnnnn: <message>
```

The code is the contract; the wording may change. The line and column are those of the construct
itself, inside a section or an `if` body alike *(since 2.0.0)*; there is no `In Graph if body:`
prefix any more. A stage reports all of its errors, and a stage with errors stops the build before
the next one. The codes are listed in the [diagnostics index](../diagnostics/index.md).

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
| [Constructors](constructors.md) | the constructor names, splatting, packing, constant folding |
| [Swizzles](swizzle.md) | channel sets, reorder and repeat |
| [Conversions](conversions.md) | how a value is fitted to the place it goes |
| [if / else](if.md) | the condition, the comparison truth table, branch merging |
| [MaterialAttributes](material-attributes.md) | attribute values, member reads and writes, Substrate interop |
| [Calls](calls.md) | calling `Function`, `GraphFunction`, `ShaderFunction`, `VirtualFunction`, parameter pins |
| [Name resolution](name-resolution.md) | identifier and call lookup order, shadowing |
| [Node reuse](node-reuse.md) | the deduplication of identical nodes and what a user observes |
| [Unsupported constructs](unsupported.md) | loops, `return`, ternary, `%`, comparisons, indexing — and the codes that refuse them |

## See also

- [Shader](../language/shader.md) — the block that owns a `Graph` and its `Outputs`
- [ShaderFunction](../language/shader-function.md) — a `Graph` that becomes a `UMaterialFunction`
- [GraphFunction](../language/graph-function.md) — HLSL bodies with `UE.*` calls hoisted into pins
- [Type tokens](../language/types.md) — which tokens a `Graph` declaration accepts
- [Layout](../language/layout.md) — `#Region` / `#EndRegion` and node placement directives
- [UE builtins](../builtins/ue.md) — the complete `UE.*` catalogue callable from a `Graph`
- [Math builtins](../builtins/math.md) — `lerp`, `dot`, `saturate`, `frac`, `fmod`, …
- [Reading parameters in Graph](../parameters/graph-usage.md) — bare reads and the pin call form
- [Graph layout](../generation/graph-layout.md) — how generated nodes are positioned
- [The 2.0 language](../language-v2/index.md) — the same compiler, with HLSL control flow
- [Diagnostics index](../diagnostics/index.md) — every code, by stage
