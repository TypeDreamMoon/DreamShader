# Node reuse

> [DreamShader](../index.md) » [Graph](index.md) » **Node reuse**

Common-subexpression elimination in the graph builder: an expression that has already produced a
material node is served from a cache instead of producing a second one.

| | |
| :-- | :-- |
| Declared in | not written by the author — applies to every `Graph { … }` body, `Outputs` binding expression and `Outputs` declaration default |
| Kind | build-time behaviour |
| Generates | nothing; it *prevents* duplicate `UMaterialExpression` nodes |

## The cache

One string-keyed map of already-built values lives for the lifetime of **one graph builder**. A builder
covers exactly one generated asset: one `Shader`, or one `ShaderFunction` / `ShaderLayer` /
`ShaderLayerBlend`. A `.dsm` that declares a material and two function assets therefore has three
independent caches, and nothing is shared between them.

The cache is **not** scoped to a block. It is not reset when an `if` body is entered or left, so a
subexpression first built inside the then-branch is reused verbatim inside the else-branch and after
the merge.

An entry is only stored when its key is non-empty and it holds a real node; a lookup with an empty key,
or on an entry whose node is null, always misses.

## What is keyed

Complete list of the sites that consult the cache.

| Construct | Key |
| :-- | :-- |
| Numeric literal | `literal-node\|<value as %.17g>` |
| `true` / `false` | `static-bool-node\|0` or `static-bool-node\|1` |
| Constant-folded vector constructor | `constvec<N>\|<component>\|<component>…` |
| `+` `-` `*` `/` | `binary-node\|<operator>\|<left value token>\|<right value token>` |
| `AppendVector` chain — from a constructor, a reordered or repeated swizzle, or a widening coercion | `append\|<value token>\|<value token>…` |
| `saturate` `sin` `cos` `abs` `floor` `ceil` `frac` `fract` `sqrt` `normalize` | `math-unary\|<name>\|<node class>\|<value token>\|<output count>` |
| `lerp` / `mix` | `math-lerp\|<A>\|<B>\|<Alpha>` |
| `dot` | `math-dot\|<A>\|<B>` |
| `pow` | `math-pow\|<Base>\|<Exponent>` |
| `min` / `max` | `math-min\|<A>\|<B>` / `math-max\|<A>\|<B>` |
| `clamp` | `math-clamp\|<Input>\|<Min>\|<Max>` |
| `fmod` / `mod` | `math-fmod\|<A>\|<B>` |
| Generic reflected `UE.*` calls — `UE.Expression`, and any `UE.<Name>` that is **not** a registered sugar builtin — and every `Substrate.*` call | the call key plus `\|Class=<node class>\|OutputType=<token>`, and a second key per selected output |
| `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` / `VirtualFunction` calls | the call key plus `\|Asset=<object path>`, and a second key per selected output |

The two-level keying on the last two rows means the **node** is shared across calls that differ only in
which output they read, while each distinct output gets its own cached value.

## The value identity token

Wherever a key embeds an already-resolved value, it embeds this token rather than the source text:

```text
Expr=<node path>|Out=<index>|Comp=<count>|Mask=<5 flags>|Tex=<0|1>|TexType=<id>|MA=<0|1>|Sub=<0|1>|Auth=<0|1>|Int=<0|1>
```

All ten fields participate. Two values that point at the same node and output but carry different
channel masks are different keys, and `int(2)` and `float(2)` never collide because the integer marker
is part of the token.

## The call key

```text
<kind>|<function name>|<arg name>=<token>|<arg name>=<token>…
```

| Rule | Detail |
| :-- | :-- |
| Kind and function name | normalized — trimmed and lower-cased. `UE.Expression` and `ue.expression` share a node. |
| Named argument | keyed under its normalized name, so only the argument's *case* may change freely |
| Positional argument | keyed under `#<index>`, so **reordering positional arguments changes the key** |
| Excluded arguments | `UE.*` calls drop `Class`, `OutputType`, `ResultType`, `Output`, `OutputName` and `OutputIndex` from the argument list before keying, then re-append the resolved class and output type explicitly |
| Nested tokens | `name:<lowered>` for an unresolved identifier, the value identity token for a resolved one, `literal:<normalized text>`, `unary:<op>(<t>)`, `binary:<op>(<l>,<r>)`, `member:<t>.<lowered>`, and a nested call key for a nested call |
| Literal text normalization | trim, CRLF → LF, and runs of spaces collapsed to one |
| Unkeyable argument | If any argument produces no token, the whole call is not cached and the node is always created. |

Because an identifier that already resolves to a value contributes its *value* token, re-assigning a
variable between two textually identical calls correctly produces two different nodes.

## What is never reused

| Construct | Reason |
| :-- | :-- |
| `UE.Expression` whose resolved class derives from `UMaterialExpressionCustom` | Explicitly exempt — Custom nodes carry per-call code and must not be shared |
| `Function` and `GraphFunction` calls | The generated `Custom` node is built fresh for every call site |
| `MakeMaterialAttributes`, `SetMaterialAttributes`, `BreakMaterialAttributes` | Not keyed; every declaration, member write and member read builds a node |
| `UMaterialExpressionIf` | Not keyed; one node per merged variable per `if` statement |
| Registered `UE.*` sugar builtins — `UE.TexCoord`, `UE.Time`, `UE.Panner`, `UE.TransformVector`, `UE.TransformPosition` and every no-argument state read | Matched by name and built by a dedicated handler that runs **before** any key is computed. The cache is never consulted, so each call site gets its own node. See [UE builtins](../builtins/ue.md) |
| `UE.CollectionParam` / `UE.CollectionParameter` | Same — handled ahead of the generic path and never keyed |
| `StaticSwitchParameter` calls | Not keyed |
| Parameter and property nodes | Not in this cache at all — see below |
| Ordered swizzles such as `.rgb`, `.ga`, `.a` | No node exists to reuse; an ordered swizzle is an input channel mask on the connection |

### Parameters and properties

A declared property is materialized once per scope by a different mechanism: the node is created on
first read and the resulting value is inserted into the **current value map**, so every later read of
the name resolves as an ordinary Graph variable.

> [!WARNING]
> That map is copied when an `if` statement runs, and property names are deliberately excluded from the
> merge. A parameter first read **inside** a branch therefore does not survive the `if`, and reading it
> again — in the other branch, or after the `if` — builds another parameter node. The nodes address the
> same material parameter, so the material behaves correctly, but the generated graph contains
> duplicates. Read the parameter once before the `if` if the duplicate nodes matter:
>
> ```c
> // Two ScalarParameter nodes named 'Threshold'.
> if (u > 0.5) { Color = Lit.rgb * Threshold; } else { Color = Dark.rgb * Threshold; }
>
> // One.
> float T = Threshold;
> if (u > 0.5) { Color = Lit.rgb * T; } else { Color = Dark.rgb * T; }
> ```

## Observable consequences

- Every occurrence of the same numeric literal in one asset produces **one** `Constant` node. The key
  is the parsed `double` printed to 17 significant digits, so `1` and `1.0` collide, `0.5` and `.5`
  collide, and `0.1` and `0.10000000000000001` collide as well — both spellings parse to the same
  `double`. Only spellings that parse to *different* doubles get different nodes.
- Unary minus lowers to `Multiply(x, Constant(-1))`, and that `Constant(-1)` is shared by every unary
  minus in the asset.
- `A + B` written twice yields one `Add` node — provided both operands resolve to identical values,
  meaning the same node, output index and channel mask.
- `UE.TexCoord(Index = 0)` written five times yields **five** `TextureCoordinate` nodes: the registered
  sugar builtins never reach the cache. Read it once into a variable if the duplicates matter.
  `UE.Expression(Class = "TextureCoordinate", OutputType = "float2")` written five times yields one
  node, because that spelling takes the generic reflected path.
- `Src.rgb` used ten times costs nothing at all: ordered swizzles never create a node.
- `F_Tint(a, b)` and `F_Tint(b, a)` are different keys; `F_Tint(Color = a)` and `F_Tint(color = a)` are
  the same key.
- Because the cache spans `if` branches, a shared subexpression written in both branches is built once
  and both `If` inputs point at it.
- Node **layout** mostly follows creation: for operators, `AppendVector` chains, math builtins, folded
  vectors and generic `UE.*` / `Substrate.*` calls the Y cursor is taken *after* the cache lookup, so
  a reused node costs no vertical slot. Scalar and `StaticBool` literals are the exception — their Y
  is consumed before the lookup, so a reused `Constant` still leaves a gap. See
  [Graph layout](../generation/graph-layout.md).

## Example

```c
Shader(Name="Docs/M_Reuse")
{
    Properties { vec3 Tint = vec3(1.0, 0.5, 0.2); }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }

    Graph {
        vec2 UV1 = UE.TexCoord(Index = 0);
        vec2 UV2 = UE.TexCoord(Index = 0);   // sugar builtin: a SECOND TextureCoordinate node
        vec2 UV3 = UV1;                      // a plain alias: no node at all

        float A = UV1.x * 2.0;
        float B = UV3.x * 2.0;               // same key -> same Multiply as A

        float C = 2.0 * UV1.x;               // different key: operands swapped

        Color = Tint * (A + B + C);
    }
}
```

Generated nodes:

```text
TextureCoordinate  Index=0        -> UV1, UV3
TextureCoordinate  Index=0        -> UV2   (sugar builtins are never cached)
Constant           2.0            -> shared by both multiplications
Multiply           UV1.x * 2.0    -> A, B
Multiply           2.0 * UV1.x    -> C
Add                A + B          (A and B are the same value, so this is Add(m, m))
Add                (A+B) + C
VectorParameter    Tint
Multiply           Tint * (...)
```

Nine nodes for seven statements, and the three `.x` swizzles contribute none of them. Two of the nine
are the identical `TextureCoordinate` pair the cache does not collapse.

## See also

- [Expressions](expressions.md) — the operators whose results are keyed
- [Swizzle](swizzle.md) — the ordered-mask fast path that emits no node
- [Constructors](constructors.md) — constant folding, the other node-count reducer
- [Literals](literals.md) — how a numeric literal becomes the key's value
- [Calls](calls.md) — which call kinds produce a reusable node
- [`if` / `else`](if.md) — why the cache is deliberately not branch-scoped
- [MaterialAttributes](material-attributes.md) — the Make/Set/Break nodes that are never reused
- [Parameters in Graph](../parameters/graph-usage.md) — how a parameter node is materialized and cached
- [Graph layout](../generation/graph-layout.md) — where the surviving nodes are placed
- [UE.Expression](../builtins/ue-expression.md) — the `Class=`/`OutputType=` arguments folded into the key
