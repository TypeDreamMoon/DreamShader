# Layout

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Layout**

A section that pins generated node positions and declares comment boxes in the generated material
graph.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` |
| Kind | section |
| Generates | node positions, and one `UMaterialExpressionComment` per `Comment` statement |
| Not accepted in | `VirtualFunction` |

*(since 2.0.0)* Which pass reads the section is the project's
[Graph Layout Style](../generation/graph-layout.md#layout-styles). `Blocks`, the default, and the other
styles computed on the compiler's IR obey every `Node` and `Comment` statement on top of their own
placement. `Classic`, the 1.x layout, uses the section the 1.x way, which is what
[Explicit layout versus automatic layout](#explicit-layout-versus-automatic-layout) describes. A
`.dss` writes the same statements as `#pragma layout(Node, …)` / `#pragma layout(Comment, …)` — see
[DreamShaderLang 2.0](../language-v2/index.md#declarations).

## Synopsis

```c
Layout [=]
{
    Node( Var = "<variable>", X = <int>, Y = <int> ) ;
    Comment( Name = "<title>", X = <int>, Y = <int>, W = <int>, H = <int>
             [, Color = float4( <r>, <g>, <b>, <a> )] ) ;
    …
}
```

Statements are calls, each optionally followed by `;`. Both statement names and all argument keys are
matched case-insensitively. The `=` before the `{ … }` block is optional *(since 1.5.0)*.

## `Node`

Pins one already-generated expression to an exact position.

| Argument | Required | Type | Default |
| :-- | :-- | :-- | :-- |
| **`Var`** | yes | text | — |
| **`X`** | yes | integer | — |
| **`Y`** | yes | integer | — |

`Var` names a value of the source. Three kinds of name are recorded:

| Name kind | Recorded when |
| :-- | :-- |
| a `Graph` statement's target variable | the statement produced a node — declared locals and assigned output variables alike |
| a [`Properties`](properties.md) declaration name | the graph actually read that property |
| an `Inputs` / `Outputs` declaration name | only in `ShaderFunction`, `ShaderLayer` and `ShaderLayerBlend`, where each one becomes a `FunctionInput` / `FunctionOutput` node |

Both `MaterialExpressionEditorX/Y` and the editor graph node's `NodePosX/Y` are written, so the
position survives opening the material.

> [!NOTE]
> A `Var` that matches nothing is **silently ignored** — there is no diagnostic for a typo or for
> pinning a property the graph never reads.

## `Comment`

Creates a comment box at an exact rectangle.

| Argument | Required | Type | Effect |
| :-- | :-- | :-- | :-- |
| **`Name`** | yes | text | Box title — see [Notes](#notes). |
| **`X`** | yes | integer | Left edge. |
| **`Y`** | yes | integer | Top edge. |
| **`W`** | yes | integer | Width; at least `120` under `Classic`, `64` under the IR styles. |
| **`H`** | yes | integer | Height; at least `80` under `Classic`, `64` under the IR styles. |
| `Color` | no | `float4` literal | Box colour; without it the layout's own default. |

> [!WARNING]
> `W` and `H` are **required arguments**. `Comment(Name="X", X=0, Y=0)` is
> [`DSH3275`](../diagnostics/DSH3xxx.md#dsh3275), which reports a missing value and a malformed one
> alike.

The `Color` literal follows the general vector-literal grammar: the token before `(` is ignored, so
`float4(…)`, `vec4(…)` and `(…)` all parse. One component splats to `(x, x, x, 1)`, two give
`(x, y, 0, 0)`, three give `(x, y, z, 1)`, and components past the fourth are ignored. Anything else
is [`DSH3276`](../diagnostics/DSH3xxx.md#dsh3276).

Generated comment boxes always use `FontSize = 24` and group mode, so dragging the box moves the
nodes it encloses.

## Argument parsing

| Rule | Detail |
| :-- | :-- |
| Statement shape | `<Name>( <key> = <value> [, <key> = <value> ]… )` — anything else is [`DSH3274`](../diagnostics/DSH3xxx.md#dsh3274) |
| Statement name | an identifier, matched case-insensitively against `Node` and `Comment`; any other name is `DSH3274` |
| Key matching | case-insensitive |
| Value handling | a single quoted string is its text; anything else is the text of its tokens, which a whole-number argument must read as an optional sign and digits |
| Duplicate key | `DSH3274` — unlike `Settings`, where the later key wins |
| Empty value | `DSH3274` |
| Missing or empty required argument, non-integer coordinate | `DSH3275` |
| Comments | allowed between any two tokens |

An argument that is neither required nor `Color` is passed on to the layout, which ignores it with
the warning [`DSH7230`](../diagnostics/DSH7xxx.md#dsh7230); so is a `Color` on a `Node`.

## Coordinate space

Positions are Unreal material-graph editor coordinates: **X increases to the right, Y increases
downward**, and the units are the same ones the editor's node positions use.

A few `Classic` constants are a useful frame of reference when hand-placing nodes:

| Landmark | X |
| :-- | :-- |
| the output column: the widest node next to the material, and `FunctionOutput` nodes | `900` |
| output-binding reroute usages, as constructed | `720` |
| the material root | `520` right of the graph's bounds |

Columns grow leftwards from the output column, so negative X is "upstream" and the material root node
sits to the right of everything else. The full table, and the IR styles' own spacing, are on
[Graph layout](../generation/graph-layout.md#layout-constants). *(since 2.0.0)* The 1.x per-kind
construction positions (`-800` for property nodes, `1200` for `Expression( … )` targets, …) are gone:
every node is created in one column and the layout places it.

## Explicit layout versus automatic layout

Under `Classic`, a `Layout` block does not merely add to the automatic pass — it **replaces** the
ranking algorithm.

| Condition | Result |
| :-- | :-- |
| at least one `Node` matched a recorded variable, **or** at least one `Comment` is declared | explicit layout runs |
| a `Layout` block exists but no `Node` matched and no `Comment` is declared | the automatic layout pass runs as if the block were absent |
| no `Layout` block | the automatic layout pass runs |

On the explicit path, expressions the block did not name are still placed: positions propagate
iteratively from already-positioned neighbours — midway between a known consumer and a known
dependency, `360` left of a known consumer, or `360` right of a known dependency — with a collision
fan-out for coincident slots. Anything still unplaced goes into a fallback column to the left of
everything positioned.

Under the IR styles a pinned node is moved to its position, and the nodes its style placed around it
move with it.

> [!NOTE]
> **Layout always runs** *(since 2.0.0)*, for an Ephemeral material as much as for a saved one; the
> one opt-out is the `Classic` style's large-graph guard. See
> [Graph layout](../generation/graph-layout.md#when-layout-runs).

> [!NOTE]
> A second `Layout` section **replaces** the first rather than appending, with the warning
> [`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258) *(since 2.0.0)*. Only the last `Layout` block in a
> block body has any effect.

## `#Region` / `#EndRegion`

Region directives live in **`Graph` body text**, not in `Layout`. They name a span of graph
statements; the layout turns each region into a comment box.

```c
Graph = {
    #Region "Surface"
    Color = BaseColor.rgb;
    Rough = Roughness;
    #EndRegion

    #Region "Emissive"
    Glow = Tint * Intensity;
    #EndRegion
}
```

| Rule | Detail |
| :-- | :-- |
| Recognition | a `#` line whose word right after the `#` is `Region` or `EndRegion`, matched case-insensitively. Another `#` line in a `.dsm` / `.dsf` is refused by the [preprocessor](preprocessor.md#what-is-not-a-directive) first (`DSH1035`); one that reaches a 1.x `Graph` body, as `#pragma` can in a `.dsh`, is [`DSH2219`](../diagnostics/DSH2xxx.md#dsh2219) |
| Name | the rest of the line, unquoted and trimmed; required on `#Region` — [`DSH2216`](../diagnostics/DSH2xxx.md#dsh2216) |
| Nesting | regions nest; an `#EndRegion` with none open is [`DSH2217`](../diagnostics/DSH2xxx.md#dsh2217), a region still open at the end of the body [`DSH2218`](../diagnostics/DSH2xxx.md#dsh2218) |
| Positions | a directive is a token like any other, so every diagnostic keeps its real line and column |

Under `Blocks` a region is a box of its own, and a region that holds regions is a box around their
boxes. Under `Classic`, a statement inside a region tags the variable it produces with the region
name; on the automatic path each region becomes one comment box, and on the explicit path region
names contribute the block boundaries used to decide where cross-block reroutes are inserted.

> [!NOTE]
> The [preprocessor](preprocessor.md) sees every `#` line before the parser does *(since 1.9.0)*, and
> it names `region` / `endregion` explicitly as lines to pass through — in any case, exactly as
> matched here. So `#Region`, `#region` and `#REGION` all reach the parser unchanged, and region
> directives are unaffected by conditional compilation.

> [!NOTE]
> `#Region` names and `Layout` `Comment` names are independent. Declaring a `Comment` whose rectangle
> happens to contain a region's nodes does not merge the two.

## Notes

- **Comment titles.** Under `Classic` every box is titled `DreamShader: <Name>`, so
  `Comment(Name="Sampling", …)` reads `DreamShader: Sampling`. Under the IR styles a `Comment` keeps
  its title as written, and the boxes the style makes for regions and blocks carry the prefix. The
  prefix marks a box as the compiler's own; see [Regeneration](../generation/regeneration.md).
- A `Comment` whose `Name` is empty or whitespace is `DSH3275`, so no box is ever created for one.
- The [decompiler](../tools/decompiler.md) writes positions and free comment boxes back as
  `#pragma layout` lines, or as a `Layout` block with `-Format Legacy`, so a material can be exported,
  edited and regenerated with its positions intact. Emission is controlled by the **Export Decompiled
  Layout** project setting, default on. See [Project settings](../settings/project.md).
- The `Classic` automatic layout gives up on very large graphs — at or above 1200 expressions it logs
  `Skipping automatic layout for large DreamShader graph ({Count} nodes). Existing generated positions will be used.`
  and leaves construction-time positions in place. A `Layout` block is the way to control those
  graphs. See [Graph layout](../generation/graph-layout.md).
- `Layout` is not accepted inside a `VirtualFunction`
  ([`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245)); that block generates no graph.

## Diagnostics

Every diagnostic carries the line and column of the statement or directive it is about.

### `Layout` statements

| Code | Raised when |
| :-- | :-- |
| `DSH3274` | the section is not opened with `{`; a statement that is not `<Name>( … )`; an argument that is not `Key = Value`; a key written twice; an unclosed `(`; a name other than `Node` or `Comment` |
| `DSH3275` | a required argument is missing or empty, or a coordinate is not a whole number |
| `DSH3276` | `Color` is not a vector literal |
| `DSH2258` | warning: a second `Layout` section |
| `DSH7230` | warning: an argument the layout has no use for, ignored |

### `#Region` directives

| Code | Raised when |
| :-- | :-- |
| `DSH2216` | `#Region` with no name |
| `DSH2217` | `#EndRegion` with no `#Region` open |
| `DSH2218` | a region still open at the end of the `Graph` body |
| `DSH2219` | another `#` line that reaches a `Graph` body |

The complete list is in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Materials/M_Layout", Root="Game")
{
    Properties {
        VectorParameter Tint      = float4(0.4, 0.8, 1.0, 1.0);
        ScalarParameter Intensity = 2.0 [Slider(0, 10)];
    }

    Outputs {
        float3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        #Region "Emissive"
        float3 Boosted = Tint.rgb * Intensity;
        Color = Boosted + vec3(0.05, 0.05, 0.05);
        #EndRegion
    }

    Layout {
        Comment(Name="Emissive", X=-1300, Y=-260, W=1100, H=520,
                Color=float4(0.10, 0.22, 0.16, 0.35));
        Node(Var="Tint",      X=-1200, Y=-160);
        Node(Var="Intensity", X=-1200, Y=  60);
        Node(Var="Boosted",   X= -800, Y=-160);
        Node(Var="Color",     X= -400, Y= -60);
    }
}
```

Generated graph, under `Classic`:

```text
Comment      "DreamShader: Emissive"   at (-1300, -260)  size 1100 x 520
Tint         VectorParameter           at (-1200, -160)
Intensity    ScalarParameter           at (-1200,   60)
Boosted      Multiply                  at ( -800, -160)
Color        Add                       at ( -400,  -60)
DS_EmissiveColor_<n>  NamedReroute     positioned by propagation
```

## See also

- [Shader](shader.md) — the block whose graph `Layout` positions
- [ShaderFunction](shader-function.md) — material-function blocks also accept `Layout`
- [Graph](../graph/index.md) — where `#Region` directives are written
- [Preprocessor](preprocessor.md) — the other `#` syntax, and how the two are told apart
- [Declarations](../graph/declarations.md) — which statements register a nameable variable
- [Properties](properties.md) — property names are also valid `Node` `Var` targets
- [Output bindings](output-bindings.md) — the reroute pairs created for each binding
- [Graph layout](../generation/graph-layout.md) — the layout styles, their blocks, constants and limits
- [Regeneration](../generation/regeneration.md) — the `DreamShader: ` comment prefix rule
- [Ephemeral materials](../generation/ephemeral.md) — materials that live only in memory
- [Decompiler](../tools/decompiler.md) — round-tripping positions out of an existing material
- [Project settings](../settings/project.md) — **Export Decompiled Layout**
- [Diagnostics index](../diagnostics/index.md) — every code
