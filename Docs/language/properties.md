# Properties

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Properties**

A section that declares the parameter nodes, constant nodes and `UE.*` builtin nodes a block
generates into its material graph.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` |
| Kind | section |
| Generates | one `UMaterialExpression` per declaration that the graph actually reads |
| Aliased | inside a `VirtualFunction`, `Properties` is a **synonym for `Inputs`** — see [Notes](#notes) |

## Synopsis

```c
Properties [=]
{
    <property-declaration>…
    <group-scope>…
}
```

```c
property-declaration := [ const ] <type-token> <name> [ = <default> ] [ [ <metadata> ] ] ;

group-scope          := Group( "<group-name>" ) { { <property-declaration> | <group-scope> }… } [ ; ]
```

In `property-declaration` the innermost `[ … ]` pair around `<metadata>` is **literal
DreamShaderLang punctuation**; the outer pair is the meta-syntax for "optional". A declaration with a
metadata block therefore looks like `float Roughness = 0.5 [Group="Surface"];`.

The `=` between the section name and its `{ … }` block is optional sugar *(since 1.5.0)*. The section
name is matched case-insensitively. A `;` after the section's closing `}` is optional.

## Declaration members

| Member | Required | Form | Description |
| :-- | :-- | :-- | :-- |
| `const` | no | keyword | Generates a constant node instead of a parameter node. Case-insensitive; must be followed by whitespace or be the entire type token. *(since 1.2.6)* |
| **`<type-token>`** | yes | token | Selects the generated node class and the default-value grammar. See [Type tokens](#type-tokens). |
| **`<name>`** | yes | text | The identifier the `Graph` reads, and — unless `[ParameterName=…]` overrides it — the material parameter name. |
| `= <default>` | no | literal | Initial value. The accepted literal grammar is chosen by the type token, not by the shape of the literal. |
| `[ <metadata> ]` | no | block | Group, sort priority, description, slider bounds, and arbitrary reflected `UMaterialExpression` properties. See [Metadata](../parameters/metadata.md). |

### Parse order

A statement is decomposed in this fixed order, which is what makes type tokens containing spaces and
parentheses work:

| Step | Operation | Consequence |
| :-- | :-- | :-- |
| 1 | A trailing `[ … ]` block is peeled off the end | The statement must **end** with `]` for metadata to be recognized at all |
| 2 | Split at the first `=` that is outside `()`, `[]` and `"…"` | The presence of that `=` is the only thing that sets "has a default value" |
| 3 | Split the left side at the **last** top-level whitespace | Type is everything before, name is everything after |
| 4 | `const` is stripped from the **front of the type token** | `const` is detected after the type/name split, not before |

Because step 3 is parenthesis-aware and accepts any whitespace character, a `UE.*` builtin type with
a spaced argument list splits correctly:

```c
UE.TexCoord(Index = 0) UV;      // type = UE.TexCoord(Index = 0), name = UV
```

> [!WARNING]
> The property name is only checked for being **non-empty**. It is not validated as an identifier.
> `Properties { float 1Bad = 0; }` parses without a diagnostic; the declaration is simply
> unreachable from `Graph`, whose name resolution requires a real identifier. Contrast
> [`Inputs` / `Outputs`](inputs-outputs.md), where the name **must** match
> `[A-Za-z_][A-Za-z0-9_]*`.

## Type tokens

Four token families are recognized, all matched case-insensitively. The complete catalogues live on
their own pages:

| Family | Tokens | Generated node | Reference |
| :-- | :-- | :-- | :-- |
| Compact scalar | `float` `float1` `half` `half1` `int` `uint` `bool` — 7 tokens | `UMaterialExpressionScalarParameter` | [Compact types](../parameters/compact-types.md) |
| Compact vector | `float2..4` `half2..4` `vec2..4` `int2..4` `uint2..4` `bool2..4` `ivec2..4` `uvec2..4` `bvec2..4` — 27 tokens | `UMaterialExpressionVectorParameter` | [Compact types](../parameters/compact-types.md) |
| Compact texture | `Texture2D` `TextureCube` `Texture2DArray` `Texture3D` `VolumeTexture` — 5 tokens | `UMaterialExpressionTextureObjectParameter` | [Compact types](../parameters/compact-types.md) |
| Explicit parameter node | 22 `*Parameter` / sampler tokens | the named `UMaterialExpression` subclass | [Parameter nodes](../parameters/parameter-nodes.md) |
| `UE.<Function>[( … )]` | any builtin name, with an argument list | the builtin's node, or a reflected node when `OutputType=` is given | [UE builtins](../builtins/ue.md), [`UE.Expression`](../builtins/ue-expression.md) |

Anything else fails with `Unsupported property type '{Type}'.`

> [!NOTE]
> The `Properties` token set is **not** the same as the `Inputs` / `Outputs` token set.
> `MaterialAttributes`, `Substrate`, `SamplerState` and `StaticBool` are valid parameter types but
> are **not** valid `Properties` types. `Scalar`, `Color`, `Vector`, `mat2`, `mat3` and `mat4` are
> valid nowhere. The per-context validity matrix is on [Types](types.md).

An inline `= <default>` is rejected on a `UE.*` builtin property — its arguments go inside the
parentheses instead:

```
UE builtin property '{Name}' does not support inline defaults. Put arguments inside UE.{Function}(...).
```

## `const` properties

*(since 1.2.6)*

`const` replaces the parameter node with a constant node, so the value is baked into the material and
does not appear in the parameter list of an instance.

| Declared type | Generated node |
| :-- | :-- |
| scalar | `UMaterialExpressionConstant` |
| vector, `ComponentCount == 2` | `UMaterialExpressionConstant2Vector` |
| vector, `ComponentCount == 3` | `UMaterialExpressionConstant3Vector` (alpha forced to `1.0`) |
| vector, any other count | `UMaterialExpressionConstant4Vector` |
| texture | `UMaterialExpressionTextureObject` |

Rules:

- `const` is legal **only** with the compact scalar, vector and texture tokens. `const` in front of
  an explicit parameter-node token or a `UE.*` builtin is a generation error.
- A `const` texture with no default falls back to the engine default asset for its dimension
  (`Texture2D`, `TextureCube` and `VolumeTexture` have one; `Texture2DArray` does not and therefore
  requires an explicit `= Path(…)`).
- Metadata still applies to a `const` declaration, so `[Desc="…"]` works on a constant node.
- **A `const` vector reads as its whole node output (index 0)** in `Graph`, whereas a non-`const`
  vector parameter reads through the named output matching its component count (`R`, `RG`, `RGB`,
  `RGBA`). See [Reading parameters in Graph](../parameters/graph-usage.md).

```c
Properties = {
    const float DebugScale = 1.0;                // UMaterialExpressionConstant
    float Strength         = 1.0;                // UMaterialExpressionScalarParameter
    vec3  Tint             = vec3(1.0, 1.0, 1.0);// UMaterialExpressionVectorParameter
}
```

## `Group("Name") { … }` scope blocks

*(since 1.5.0)*

A `Group` scope stamps its name onto every declaration it contains, so a shared group does not have
to be repeated in each declaration's metadata.

```c
Properties {
    Group("Surface") {
        ScalarParameter Roughness = 0.5 [Slider(0, 1)];
        VectorParameter BaseColor = float4(1, 1, 1, 1);
    }
}
```

### Head grammar

| Rule | Detail |
| :-- | :-- |
| Keyword | `Group`, matched case-insensitively |
| Argument list | must be a balanced `( … )` immediately after the keyword |
| Argument | the trimmed inner text **must start with a `"`**; the name is then unquoted |
| Name | must be non-empty after unquoting and trimming |
| Body | `{ … }`; the walker is brace-, paren-, bracket- and string-aware |
| Terminator | a single `;` immediately after the closing `}` is consumed silently |

`Group(…)` is the **only** construct that may open a `{` inside `Properties`. Any other `{` is a
diagnostic.

### Nesting and `|` composition

Scopes nest to any depth. A nested scope's effective group name is the enclosing name, a `|`, and the
inner name — which is Unreal's own sub-category syntax for the `Group` property:

```c
Properties {
    Group("Surface") {
        float A = 0;                    // Group = "Surface"
        Group("Detail") {
            float B = 0;                // Group = "Surface|Detail"
            Group("Micro") {
                float C = 0;            // Group = "Surface|Detail|Micro"
            }
        }
    }
}
```

A literal `|` typed inside a group name passes through unchanged, so
`Group("Manual|Literal") { … }` produces `Manual|Literal` exactly.

### Inheritance precedence

The inherited group is applied to a member **only if** that member's metadata block typed neither
`Group` nor `Category`. An explicit key on the declaration always wins:

```c
Group("Surface") {
    float A = 0;                        // Group = "Surface"     (inherited)
    float B = 0 [Group="Override"];     // Group = "Override"    (explicit wins)
    float C = 0 [Category="Other"];     // Group = "Other"       (Category is the alias)
}
```

### Automatic `SortPriority`

Members of a group scope are auto-numbered by declaration order.

| Rule | Value |
| :-- | :-- |
| Counter start | `0` |
| Counter step | `10` |
| Counter scope | **one counter shared by every group in the same `Properties` section**, not one per group |
| Explicit `SortPriority` / `Sort` | wins, and **does not consume a slot** |
| Ungrouped (top-level) declarations | never auto-numbered, and never given a group |

```c
Properties {
    Group("Surface") {
        ScalarParameter A = 0.5;                    // SortPriority = 0
        VectorParameter B = float4(1, 1, 1, 1);     // SortPriority = 10
    }
    Group("Detail") {
        ScalarParameter C = 1.0 [SortPriority=99];  // SortPriority = 99, no slot consumed
        ScalarParameter D = 2.0;                    // SortPriority = 20  (counter continued)
    }
    ScalarParameter Loose = 3.0;                    // no group, no auto sort
}
```

> [!NOTE]
> The counter is seeded once per `Properties` section. A block that declares `Properties` twice gets
> a fresh counter starting at `0` in the second section, so two groups in two sections can end up
> with overlapping sort priorities.

When no group scope and no explicit metadata supply a value, **no `SortPriority` is written at all**:
the parsed metadata carries `32` but it is only applied when the declaration actually set one, so the
generated node keeps its own class default (`32` on Unreal's parameter nodes).

## Metadata

The trailing `[ … ]` block is shared with the typed-parameter sections. Its recognized keys are:

| Key | Aliases | Value |
| :-- | :-- | :-- |
| `Group` | `Category` | string |
| `Description` | `Desc`, `Tooltip` | string |
| `SortPriority` | `Sort` | integer |
| `Slider(min, max)` | — | shorthand with no `=`; expands to `SliderMin` + `SliderMax` |
| `ParameterName` | — | string; overrides the material parameter name |
| *anything else* | — | written to the same-named reflected `UMaterialExpression` property |

Entries are separated by `;` or `,`. Full grammar, the reflection rules, the enum-value spellings and
every diagnostic are on [Metadata](../parameters/metadata.md).

## Ordering and repetition

- Statements are `;`-terminated. The splitter tracks `()`, `[]`, `{}` and `"…"`, so a `Group` body and
  a bracketed metadata block never split a statement in the wrong place.
- Comments are stripped from the section body before statements are split.
- Sections may appear **in any order** inside a block, and a repeated `Properties` section
  **appends** to the previous one. There is no "declared twice" diagnostic at the section level.
- **Declaration order does not affect name resolution in `Graph`.** Property nodes are created
  lazily on first read and cached by name, so a property referenced *N* times creates exactly one
  node and a property never referenced creates none.
- Declaration order *does* determine the auto-`SortPriority` counter and the vertical order of the
  generated nodes.
- A `UE.*` argument that references another property resolves against the same `Properties` list
  regardless of position, but a reference cycle is rejected with
  `Property '{Name}' has a recursive UE builtin dependency.`

Name-uniqueness rules are enforced at generation, not at parse:

| Scope | Rule |
| :-- | :-- |
| `Shader` | property names must be unique **ignoring case** |
| `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` | a property name may not collide with a property **or an input** name |

## Notes

- **`Properties` means something different inside a `VirtualFunction`.** There it is an alias for
  [`Inputs`](inputs-outputs.md) and gets the typed-parameter grammar (`opt`, identifier-validated
  names, literal-space splitting) instead of the declaration grammar on this page. See
  [VirtualFunction](virtual-function.md).
- Property nodes are created at graph X `-800`, stepping Y by `220` per property. Those positions are
  what a `Layout` block or the automatic layout pass then rearranges; see
  [Layout](layout.md) and [Graph layout](../generation/graph-layout.md).
- When a `Shader` compiles to a whole-surface `Custom` node instead of a `Graph`, only properties
  whose identifier textually appears in the prepared HLSL become `Custom` node inputs. Unreferenced
  properties are skipped entirely.
- Every declaration's metadata is *also* stored verbatim under its lower-cased key and applied by
  reflection, including the four "known" keys. A key that is not a reflected property on the
  generated node class is a hard error — except `Group`, `SortPriority` and `Desc`, which are skipped
  with a log warning when the class does not expose them.
- Regeneration destroys hand edits to the generated nodes. See
  [Regeneration](../generation/regeneration.md).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section. In the
`Unexpected '{'` message below, only `{Statement}` is a substitution — the other braces are literal
message text.

### Parse time

| Message | Cause |
| :-- | :-- |
| `Invalid property declaration '{Statement}'.` | no top-level whitespace separating the type token from the name |
| `Missing property name in declaration '{Statement}'.` | the name side of the split is empty |
| `Missing property type after const in declaration '{Statement}'.` | `const` with nothing after it |
| `Unsupported property type '{Type}'.` | the type token matches no compact token, no parameter-node token and no `UE.` prefix |
| `Metadata must follow a declaration.` | the statement is nothing but a `[ … ]` block |
| `Invalid scalar default value '{Value}' for property '{Name}'.` | a scalar token's default is neither numeric nor `true`/`false` |
| `Invalid vector default value '{Value}' for property '{Name}'.` | a vector token's default is not a `name(a, b, c, d)` literal |
| `Invalid boolean default value '{Value}' for property '{Name}'.` | `StaticBoolParameter` / `StaticSwitchParameter` given a non-boolean default |
| `Invalid texture default value '{Value}' for property '{Name}'. {Detail}` | a texture token's default is not a resolvable asset reference |
| `Invalid texture sample default value '{Value}' for property '{Name}'. {Detail}` | same, for the `TextureSampleParameter*` family |
| `UE builtin property '{Name}' does not support inline defaults. Put arguments inside UE.{Function}(...).` | `= …` written after a `UE.*` type token |
| `Unexpected '{' in Properties near '{Statement}'. Only Group("Name") { ... } may open a brace here.` | a `{` that is not a `Group` scope head |
| `Group(...) requires a non-empty name.` | `Group("")` |
| `Unterminated Group("{Name}") { ... } block.` | the scope's `{` is never closed |
| `Metadata entry '{Entry}' must use Key=Value syntax.` | a metadata entry with no top-level `=` that is not `Slider(…)` |
| `Metadata key '{Key}' is declared more than once.` | duplicate metadata key after normalization |
| `Metadata SortPriority value '{Value}' is not an integer.` | non-integer sort priority |

### Generation time

| Message | Cause |
| :-- | :-- |
| `{File}: Property '{Name}' is declared more than once. Property names must be unique.` | two `Shader` properties whose names are equal ignoring case |
| `{Kind} '{Function}' property '{Name}' conflicts with another property or input name.` | a material-function property collides with another property or an input |
| `Const property '{Name}' must use a plain scalar, vector, or texture type instead of a parameter node or UE builtin declaration.` | `const` in front of a parameter-node token or a `UE.*` builtin |
| `Const texture property '{Name}' could not load asset '{Path}'.` | explicit `const` texture default that does not load |
| `Const texture property '{Name}' could not load default {Label} asset '{Path}'.` | the engine fallback texture for the dimension did not load |
| `Const texture property '{Name}' with type Texture2DArray requires an explicit default asset.` | `const Texture2DArray` with no default — no engine fallback exists |
| `Failed to create a texture object node for const property '{Name}'.` | node construction failed |
| `Failed to create a const node for property '{Name}'.` | node construction failed |
| `Failed to create a parameter node for property '{Name}'.` | the parameter path returned no node and no message |
| `Property '{Name}' has a recursive UE builtin dependency.` | `UE.*` argument reference cycle between properties |
| `property '{Name}': {Detail}` | prefix applied to any metadata or default-value failure on that property |

The complete cross-stage list is in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Materials/M_Surface", Root="Game")
{
    Properties {
        Group("Surface") {
            VectorParameter BaseColor = float4(0.8, 0.8, 0.8, 1.0);   // SortPriority 0
            ScalarParameter Roughness = 0.55 [Slider(0, 1)];          // SortPriority 10
            Group("Advanced") {
                ScalarParameter Metallic = 0.0;                       // Group "Surface|Advanced", 20
            }
        }

        Group("Textures") {
            TextureSampleParameter2D AlbedoMap = Path(Game, "Textures/T_White_Linear") [
                SamplerType="LinearColor";
                MipValueMode="None";
                AutomaticViewMipBias=true;
            ];                                                        // SortPriority 30
        }

        const float DebugScale = 1.0;                                 // ungrouped, not auto-sorted
        UE.TexCoord(Index = 0) UV;                                    // ungrouped
    }

    Outputs {
        float3 Color;
        float  Rough;
        float  Metal;

        Base.BaseColor  = Color;
        Base.Roughness  = Rough;
        Base.Metallic   = Metal;
    }

    Graph {
        float4 Albedo = AlbedoMap(Coordinates = UV);
        Color = Albedo.rgb * BaseColor.rgb * DebugScale;
        Rough = Roughness;
        Metal = Metallic;
    }
}
```

Resulting parameter organization:

```text
Surface           SortPriority  0   BaseColor   VectorParameter
Surface           SortPriority 10   Roughness   ScalarParameter, slider 0..1
Surface|Advanced  SortPriority 20   Metallic    ScalarParameter
Textures          SortPriority 30   AlbedoMap   TextureSampleParameter2D

DebugScale  -> UMaterialExpressionConstant           not a parameter; ungrouped, not auto-sorted
UV          -> UMaterialExpressionTextureCoordinate  ungrouped, not auto-sorted
```

## See also

- [Shader](shader.md) — the block whose `Properties` become material parameters
- [ShaderFunction](shader-function.md) — function-local `Properties` *(since 1.2.6)*
- [VirtualFunction](virtual-function.md) — where `Properties` instead aliases `Inputs`
- [Inputs / Outputs / Results](inputs-outputs.md) — the typed-parameter section grammar
- [Types](types.md) — the full type-token catalogue and per-context validity matrix
- [Compact types](../parameters/compact-types.md) — every compact token and the node it generates
- [Parameter nodes](../parameters/parameter-nodes.md) — all 22 explicit `*Parameter` tokens
- [Metadata](../parameters/metadata.md) — the `[ … ]` block, `Slider(…)`, reflected passthrough
- [Sampler type](../parameters/sampler-type.md) — `SamplerType` values and spellings
- [`Path(...)`](../parameters/path.md) — asset-reference grammar for texture defaults
- [Reading parameters in Graph](../parameters/graph-usage.md) — bare reads and the pin call form
- [UE builtins](../builtins/ue.md) — every `UE.*` name accepted as a property type
- [Output bindings](output-bindings.md) — connecting graph values to material properties
- [Layout](layout.md) — pinning the generated node positions
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
