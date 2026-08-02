# Layout

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Layout**

A section that pins generated node positions and declares comment boxes in the generated material
graph, replacing the automatic layout pass.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` |
| Kind | section |
| Generates | node positions, and one `UMaterialExpressionComment` per `Comment` statement |
| Not accepted in | `VirtualFunction` |

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

Statements are `;`-separated calls. Both statement names and all argument keys are matched
case-insensitively. The `=` before the `{ … }` block is optional *(since 1.5.0)*.

## `Node`

Pins one already-generated expression to an exact position.

| Argument | Required | Type | Default |
| :-- | :-- | :-- | :-- |
| **`Var`** | yes | text | — |
| **`X`** | yes | integer | — |
| **`Y`** | yes | integer | — |

`Var` names a value recorded during graph construction. Three kinds of name are recorded:

| Name kind | Recorded when |
| :-- | :-- |
| a `Graph` statement's target variable | the statement produced a node — declared locals and assigned output variables alike |
| a [`Properties`](properties.md) declaration name | the graph actually read that property (property nodes are created lazily) |
| an `Inputs` / `Outputs` declaration name | only in `ShaderFunction`, `ShaderLayer` and `ShaderLayerBlend`, where each one becomes a `FunctionInput` / `FunctionOutput` node |

Both `MaterialExpressionEditorX/Y` and the editor graph node's `NodePosX/Y` are written, so the
position survives opening the material.

> [!NOTE]
> A `Var` that matches nothing is **silently ignored** — there is no diagnostic for a typo or for
> pinning a property the graph never reads.

## `Comment`

Creates a comment box at an exact rectangle.

| Argument | Required | Type | Struct default | Effect |
| :-- | :-- | :-- | :-- | :-- |
| **`Name`** | yes | text | — | Box title. Emitted as `DreamShader: <Name>` — see [Notes](#notes). |
| **`X`** | yes | integer | `0` | Left edge. |
| **`Y`** | yes | integer | `0` | Top edge. |
| **`W`** | yes | integer | `420` | Width, clamped to a minimum of `120`. |
| **`H`** | yes | integer | `240` | Height, clamped to a minimum of `80`. |
| `Color` | no | `float4` literal | `float4(0.10, 0.16, 0.22, 0.35)` | Box colour. |

> [!WARNING]
> `W` and `H` carry struct defaults but are nonetheless **required arguments**. `Comment(Name="X",
> X=0, Y=0)` fails with `Layout argument 'W' must be an integer.` — the integer arguments report a
> missing value and a malformed one with the same message. The struct defaults are observable only for
> comment boxes the automatic layout pass constructs, never for source-declared ones.

The `Color` literal follows the general vector-literal grammar: the token before `(` is ignored, so
`float4(…)`, `vec4(…)` and `(…)` all parse. One component splats to `(x, x, x, 1)`, two give
`(x, y, 0, 0)`, three give `(x, y, z, 1)`, and components past the fourth are parsed and discarded.

Generated comment boxes always use `FontSize = 24` and group mode, so dragging the box moves the
nodes it encloses.

## Argument parsing

| Rule | Detail |
| :-- | :-- |
| Statement shape | `<Name>( <key> = <value> [, <key> = <value> ]… )` — text after the closing `)` is an error |
| Statement name | must be a valid identifier, matched case-insensitively against `Node` and `Comment` |
| Key normalization | trimmed, then lower-cased |
| Value handling | one surrounding `"…"` pair is stripped and unescaped, then the value is trimmed |
| Duplicate key | rejected with a diagnostic — unlike `Settings`, where the later key wins |
| Empty key or value | rejected |
| Comments | stripped from the section body before statements are split |

## Coordinate space

Positions are Unreal material-graph editor coordinates: **X increases to the right, Y increases
downward**, and the units are the same ones the editor's node positions use.

The constants the automatic layout pass uses are a useful frame of reference when hand-placing nodes:

| Landmark | X |
| :-- | :-- |
| generated property / parameter nodes | `-800` |
| `FunctionInput` nodes | `-800` |
| inline literal constants | `-1120` |
| one automatic layout column | `420` wide, laid out leftwards from the output column |
| output-binding reroute usages | `720` |
| `FunctionOutput` nodes | `900` |
| the automatic layout's output column | `900` |
| `Expression( … ).Pin[i]` output-target nodes | `1200` |

Vertical stride is `220` for property nodes and automatic layout rows, `180` for function inputs and
outputs. Negative X is "upstream"; the material root node sits to the right of everything else.

## Explicit layout versus automatic layout

A `Layout` block does not merely add to the automatic pass — it **replaces** the ranking algorithm.

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

> [!WARNING]
> **Layout is skipped entirely in transient (in-memory) mode.** Auto-compile-on-save, the Gen page
> buttons and the live preview all generate in memory, so a `Layout` block has no visible effect
> there — the nodes keep whatever positions the construction pass produced. Positions appear only in
> a persisted asset: at cook, through the commandlet, or after an explicit *Materialize*. See
> [In-memory materials](../generation/in-memory.md).

> [!NOTE]
> A second `Layout` section **resets** the first rather than appending. Only the last `Layout` block
> in a block body has any effect. Every other section in DreamShaderLang either appends or merges.

## `#Region` / `#EndRegion`

Region directives live in **`Graph` body text**, not in `Layout`. They name a span of graph
statements; the layout pass turns each distinct region name into a comment-box block.

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
| Recognition | the trimmed line must start with `#Region` / `#EndRegion`, matched case-insensitively, followed by end of line, whitespace, or `"` |
| Name | the rest of the line, unquoted and trimmed; required on `#Region` |
| Nesting | regions nest — the parser keeps a stack |
| Span | `StartLine` is the line **after** `#Region`; `EndLine` is the line **before** `#EndRegion`, floored at `StartLine` |
| Line numbering | directive lines are replaced by an equal-length run of spaces, so diagnostics keep their real line and column |

A statement inside a region tags the variable it produces with the region name. On the automatic
layout path each region becomes one comment box; on the explicit path region names contribute the
block boundaries used to decide where cross-block reroutes are inserted.

> [!NOTE]
> `#Region` names and `Layout` `Comment` names are independent. Declaring a `Comment` whose rectangle
> happens to contain a region's nodes does not merge the two — containment is what assigns a node to a
> comment block on the explicit path.

## Notes

- **Comment text is always prefixed with `DreamShader: `.** A `Comment(Name="Sampling", …)` produces
  a box reading `DreamShader: Sampling`. That prefix is also the marker used at teardown: on
  regeneration, comment boxes whose text starts with `DreamShader: ` are deleted and rebuilt, and
  comment boxes that do not carry the prefix **survive**. A hand-authored comment box is the only
  hand edit that survives regeneration. See [Regeneration](../generation/regeneration.md).
- A `Comment` whose `Name` is empty or whitespace after unquoting is rejected at parse time with
  `Layout argument 'Name' is required.`, so no box is ever created for one.
- The [decompiler](../tools/decompiler.md) emits `Layout` blocks in exactly this format, so a
  material can be exported, edited and regenerated with its positions intact. Emission is controlled
  by the **Export Decompiled Layout** project setting, default on. See
  [Project settings](../settings/project.md).
- The automatic layout pass gives up on very large graphs — at or above 1200 expressions it logs
  `Skipping automatic layout for large DreamShader graph ({Count} nodes). Existing generated positions will be used.`
  and leaves construction-time positions in place. A `Layout` block is the way to control those
  graphs. See [Graph layout](../generation/graph-layout.md).
- `Layout` is not accepted inside a `VirtualFunction`; that block generates no graph.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section. All of these are parse-time
errors.

### `Layout` statements

| Message | Cause |
| :-- | :-- |
| `Invalid Layout statement '{Statement}'.` | no balanced `( … )` |
| `Unexpected text after Layout statement '{Statement}'.` | trailing text after the closing `)` |
| `Invalid Layout statement name in '{Statement}'.` | the text before `(` is not an identifier |
| `Layout argument '{Argument}' must use Key=Value syntax.` | a positional argument |
| `Invalid Layout argument '{Argument}'.` | empty key or empty value |
| `Layout argument '{Key}' is declared more than once.` | duplicate argument key |
| `Layout argument '{Name}' is required.` | a required text argument is missing or blank |
| `Layout argument '{Name}' must be an integer.` | a required integer argument is missing or not an integer |
| `Invalid Layout Node statement '{Statement}'. {Detail}` | wrapper around the two messages above, for `Node` |
| `Invalid Layout Comment statement '{Statement}'. {Detail}` | wrapper around the two messages above, for `Comment` |
| `Layout Comment Color must be a float4 literal in '{Statement}'.` | `Color` is not a vector literal |
| `Unknown Layout statement '{Name}'.` | a call name other than `Node` or `Comment` |

### `#Region` directives

| Message | Cause |
| :-- | :-- |
| `Graph #Region on line {Line} must include a name.` | `#Region` with nothing after it |
| `Graph #EndRegion on line {Line} has no matching #Region.` | unbalanced `#EndRegion` |
| `Graph #Region '{Name}' is missing #EndRegion.` | a region still open at the end of the `Graph` body |

The complete cross-stage list is in the [diagnostics index](../diagnostics/index.md).

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

Generated graph:

```text
Comment      "DreamShader: Emissive"   at (-1300, -260)  size 1100 x 520
Tint         VectorParameter           at (-1200, -160)
Intensity    ScalarParameter           at (-1200,   60)
Boosted      Multiply                  at ( -800, -160)
Color        Add                       at ( -400,  -60)
DS_Color_<n> NamedReroute              positioned by propagation
```

Nothing is written to disk unless the material is persisted — see the transient-mode warning above.

## See also

- [Shader](shader.md) — the block whose graph `Layout` positions
- [ShaderFunction](shader-function.md) — material-function blocks also accept `Layout`
- [Graph](../graph/index.md) — where `#Region` directives are written
- [Declarations](../graph/declarations.md) — which statements register a nameable variable
- [Properties](properties.md) — property names are also valid `Node` `Var` targets
- [Output bindings](output-bindings.md) — the reroute pairs created for each binding
- [Graph layout](../generation/graph-layout.md) — the automatic pass, its blocks, constants and limits
- [Regeneration](../generation/regeneration.md) — the `DreamShader: ` comment prefix rule
- [In-memory materials](../generation/in-memory.md) — why layout is skipped for memory-only materials
- [Decompiler](../tools/decompiler.md) — round-tripping `Layout` out of an existing material
- [Project settings](../settings/project.md) — **Export Decompiled Layout**
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
