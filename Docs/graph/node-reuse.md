# Node reuse

> [DreamShader](../index.md) » [Graph](index.md) » **Node reuse**

Common-subexpression elimination: two nodes that compute the same thing from the same inputs are one
node in the generated asset.

| | |
| :-- | :-- |
| Declared in | not written by the author — applies to every `Graph { … }` body, `Outputs` binding expression and `Outputs` declaration default |
| Kind | build-time behaviour |
| Generates | nothing; it *prevents* duplicate `UMaterialExpression` nodes |

*(since 2.0.0)* Reuse is a pass over the compiler's intermediate graph (IR), run once the whole asset
has been built, rather than a cache the 1.x generator consulted while it built nodes. The rule is the
same one — two nodes with the same class and the same arguments are one node — but it applies to
every node, so several constructs 1.x never shared are shared now (see
[What is never merged](#what-is-never-merged)).

## The dedupe pass

The passes run over the graph of **one** generated asset: one `Shader`, or one `ShaderFunction` /
`ShaderLayer` / `ShaderLayerBlend`. A `.dsm` that declares a material and two function assets has three
independent graphs, and nothing is shared between them.

| Order | Pass | What it does |
| --: | :-- | :-- |
| 1 | prune | drops nodes nothing reads, before anything is merged |
| 2 | fold | an operator, builtin, constructor or swizzle whose operands are all constants becomes one constant |
| 3 | swizzles | a mask of a mask becomes one mask while it stays ascending; an identity mask is no node (except in a 1.x source, see [Swizzle](swizzle.md#lowering)) |
| 4 | dedupe | merges equal nodes |
| 5 | prune | drops what no output reaches any more; a parameter nothing reads is the info [`DSH4390`](../diagnostics/DSH4xxx.md#dsh4390) |

Passes 2–4 repeat, at most four times, until a round changes nothing: a fold can make two nodes equal,
and a merge can put two equal constants in front of the folder.

The pass is **not** scoped to a block. It sees the whole graph at once, so a subexpression written in
the then-branch and again in the else-branch, or after the `if`, is one node.

## What is merged

Two nodes are one node when every part of them is equal:

| Part | Compared |
| :-- | :-- |
| Operation | what the node is: an operator, a builtin, a constant, a parameter, a reflected `UE.*` / `Substrate.*` node, a Custom node, a function call |
| Class | the expression class of a reflected node, the function of a Custom node or a call |
| Outputs | how many, their names and their types — a bool constant and a float constant differ |
| Properties | every property, as a set of name and value; a number to 9 significant digits |
| Operands | in order, so `a * b` and `b * a` are two nodes |
| Input pins | as a set of pin and value, so the order arguments were written in does not matter |

Operands and pins compare by the node they read. The pass works front to back and merges a node's
inputs before the node itself, so equality is structural: two nodes are equal when they read equal
nodes in the same way.

## Values, not text

The comparison is on nodes, never on source text. A variable stands for the node it holds, so:

- re-assigning a variable between two textually identical expressions produces two different nodes;
- `UV1.x * 2.0` and `UV3.x * 2.0` are one node when `UV3` holds the same node as `UV1`;
- two spellings that build the same node are one node: `F_Tint(a, b)` and
  `F_Tint(Color = a, Tint = b)`, `1` and `1.0`.

A name in another case is the same declaration (with the warning
[`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275)), so `F_Tint(Color = a)` and `F_Tint(color = a)` are one
node as well.

## What is never merged

| Construct | Reason |
| :-- | :-- |
| A statement: the material's output, a function's outputs, a custom-output node such as `UE.DreamPassOutput` | it is a place in the asset, not a value |
| A function input (`FunctionInput`) | it is part of the asset's signature |
| Two nodes that differ in one property, pin or operand | `UE.TexCoord(Index = 0)` and `UE.TexCoord(Index = 1)`; a parameter's pin call and a bare read of it |
| An `Outputs` expression target whose argument list differs from another's by even one argument | the argument list is the node's identity — write the pins in one [block](../language/output-bindings.md#block-form) when they are meant to be one node |

Everything else is merged when equal *(since 2.0.0)*. That includes what 1.x never shared:

| Construct | 1.x | Now |
| :-- | :-- | :-- |
| `Function` and `GraphFunction` calls | a fresh `Custom` node per call site | one node per distinct argument list |
| `UE.Expression` of a Custom class | never shared | shared when the code and the inputs are equal |
| `UE.TexCoord`, `UE.Time`, `UE.Panner`, `UE.TransformVector`, `UE.TransformPosition` and the no-argument state reads | a node per call site | one node per distinct argument list |
| `UE.CollectionParam` / `UE.CollectionParameter` | a node per call site | one node per collection and parameter |
| `MakeMaterialAttributes`, `SetMaterialAttributes` and attribute reads | a node per declaration, write and read | shared when equal |
| `If` | one node per merged variable per `if` | shared when the condition and both values are equal |
| `StaticSwitchParameter` calls | never shared | one node per distinct pair of branches |
| Parameter and property nodes | once per scope, again after an `if` | one node per parameter and argument list — see below |
| Swizzles | no node: a mask on each connection | one `ComponentMask` per source and mask, or none — see [Swizzle](swizzle.md) |

### Parameters and properties

A scalar, vector, texture or static-bool property is a material parameter, and every read of it is
that parameter's node: equal nodes, so one node in the asset. *(since 2.0.0)* the 1.x duplicate is
gone — a parameter first read inside an `if` branch and read again after it is one node.

A property whose type is a parameter node (`TextureSampleParameter2D`, `ChannelMaskParameter`, …) is
written out at every use with its declaration's default and metadata; uses with the same arguments are
one node, and a [pin call](calls.md#input-pin-wiring) with different pins is another node that
addresses the same material parameter.

> [!NOTE]
> `Outputs` expression targets — `Expression( … ).Pin[i]` and the
> [block form](../language/output-bindings.md#block-form) — are grouped while the `Outputs` section is
> read, not by this pass: every binding whose class and argument list agree (argument names ignoring
> case, in any order, the pin index excluded) fills a different pin of one node, and a pin bound twice
> is [`DSH3268`](../diagnostics/DSH3xxx.md#dsh3268). That is what makes the exact spelling of the
> argument list load-bearing: one extra or missing argument is a second node, silently. The block
> form exists to remove the repetition that made that mistake easy — it writes the specification once
> and lowers to N bindings that cannot disagree.

## Observable consequences

- Every occurrence of the same numeric value in one asset is **one** `Constant` node: `1` and `1.0`,
  `0.5` and `.5`. Constants are compared to 9 significant digits.
- A negation `-x` is `Multiply(x, Constant(-1))`. The same `-x` written twice is one `Multiply`; two
  different negations each have a `Constant(-1)` of their own *(since 2.0.0; 1.x shared one)*. A
  negated constant (`-0.5`) is folded into one `Constant`.
- `A + B` written twice yields one `Add` node — provided both operands are the same nodes, read through
  the same outputs.
- `UE.TexCoord(Index = 0)` written five times yields **one** `TextureCoordinate` node *(since 2.0.0;
  1.x made five)*.
- `Src.rgb` used ten times is one `ComponentMask`, or no node at all when `Src` has an `RGB` output
  pin.
- `F_Tint(a, b)` and `F_Tint(b, a)` are different nodes; `F_Tint(Color = a)` and `F_Tint(color = a)`
  are the same node.
- Because the pass spans `if` branches, a shared subexpression written in both branches is built once
  and both inputs of the `If` read it.
- Node **layout** is computed on the merged graph, so a merged node has one place and leaves no gap.
  See [Graph layout](../generation/graph-layout.md).

## Example

```c
Shader(Name="Docs/M_Reuse")
{
    Properties { vec3 Tint = vec3(1.0, 0.5, 0.2); }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }

    Graph {
        vec2 UV1 = UE.TexCoord(Index = 0);
        vec2 UV2 = UE.TexCoord(Index = 0);   // the same node as UV1
        vec2 UV3 = UV1;                      // a plain alias: no node at all

        float A = UV1.x * 2.0;
        float B = UV3.x * 2.0;               // same node as A
        float C = 2.0 * UV1.x;               // a different node: operands swapped

        Color = Tint * (A + B + C);
    }
}
```

Generated nodes:

```text
TextureCoordinate  Index=0        -> UV1, UV2, UV3
ComponentMask      R              -> UV1.x, UV3.x
Constant           2.0            -> shared by all three multiplications
Multiply           UV1.x * 2.0    -> A, B
Multiply           2.0 * UV1.x    -> C
Add                A + B          (A and B are the same value, so this is Add(m, m))
Add                (A+B) + C
VectorParameter    Tint
Multiply           Tint * (...)
```

Nine nodes for seven statements *(since 2.0.0)*: the two `TexCoord` calls are one node, and the `.x`
swizzle is one `ComponentMask` however often it is read.

## See also

- [Expressions](expressions.md) — the operators whose results are merged
- [Swizzle](swizzle.md) — what a swizzle costs, and the identity masks a 1.x source keeps
- [Constructors](constructors.md) — constant folding, the other node-count reducer
- [Literals](literals.md) — how a numeric literal becomes a constant
- [Calls](calls.md) — call nodes and their argument lists
- [`if` / `else`](if.md) — why reuse is not branch-scoped
- [MaterialAttributes](material-attributes.md) — the Make/Set/read nodes
- [Parameters in Graph](../parameters/graph-usage.md) — how a parameter node is made
- [Graph layout](../generation/graph-layout.md) — where the surviving nodes are placed
- [UE.Expression](../builtins/ue-expression.md) — the `Class=` argument and the properties of a node
