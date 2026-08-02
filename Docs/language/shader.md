# Shader

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Shader**

A top-level block that declares one Unreal material asset: its parameters, its render-state settings,
its output bindings, and the node graph that feeds them.

| | |
| :-- | :-- |
| Declared in | `.dsm` only — a `.dsh` or `.dsf` containing the text `Shader(` is rejected before parsing |
| Kind | top-level block |
| Generates | `UMaterial` (Graph backend) or `UDreamShaderMaterialInstance` + a hidden `UMaterial` base (ThinCustom backend) |
| Multiplicity | at most one per parse unit (see [Notes](#notes)) |

## Synopsis

```c
Shader(Name = "<asset-path>" [, Root = "<root>"] [,])
{
    [Properties [=] { <property-declaration> ; … }]
    [Settings   [=] { <key> = <value> ; … }]
    [Outputs    [=] { { <output-declaration> | <output-binding> } ; … }]
    [Graph      [=] { <graph-statement> … }]
    [Layout     [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

The `=` between a section name and its `{ … }` block is optional sugar *(since 1.5.0)*; a `;` after a
section's closing `}` is optional. Sections may appear in any order and may be repeated.

The keyword `Shader` is matched **case-sensitively**; section names are matched
case-insensitively. See [Lexical elements](lexical.md#case-sensitivity).

## Header attributes

| Attribute | Required | Value | Effect |
| :-- | :-- | :-- | :-- |
| **`Name`** | yes | string | The asset's logical path. The last `/`-separated segment is the asset name; preceding segments become folders. |
| `Root` | no | string | The package root the folders hang off. Defaults to `/Game` when absent or empty. |

Attribute keys are matched case-insensitively (`name=` works). Values may be quoted or bare; a bare
value ends at the first `,` or `)`. A trailing comma before `)` is accepted. A duplicate key silently
overwrites the earlier one — there is no diagnostic.

Full `Name` / `Root` grammar, the accepted root spellings, and the resulting on-disk path are
specified in [Asset paths](../generation/asset-paths.md).

## Sections

| Section | Accepted | Repeat behaviour | Reference |
| :-- | :-- | :-- | :-- |
| `Properties` | yes | appends | [Properties](properties.md) |
| `Settings` | yes | merges; last key wins | [Shader settings](../settings/material.md) |
| `Outputs` | yes | appends | [Output bindings](output-bindings.md) |
| `Graph` | yes | overwrites the previous body | [Graph](../graph/index.md) |
| `Layout` | yes | **resets** — a second `Layout` discards the first | [Layout](layout.md) |
| `Code` | **no** — hard error | — | — |
| `Inputs` | no — unknown section | — | — |
| `Results` | no — unknown section | — | — |
| `Options` | no — unknown section | — | — |

`Properties` in a `Shader` declares parameter, `const` and `UE.*` builtin nodes. This is *not* the
same grammar `Properties` gets inside a [`VirtualFunction`](virtual-function.md), where it is a
synonym for `Inputs`.

## The `Outputs` / `Graph` relationship

`Outputs` holds two kinds of statement, disambiguated per statement:

| Statement shape | Meaning |
| :-- | :-- |
| `<type> <name> ;` | output-variable declaration; the `Graph` must assign it |
| `<type> <name> = <expression> ;` | initialized output declaration *(since 1.3.4)* |
| `<target> = <variable> ;` | output binding — `Base.<Property>` or `Expression( … ).Pin[<i>]` |

The rule that governs whether `Graph` is required is evaluated after the whole parse unit is read:

- A `Graph` block is required **unless** at least one output declaration carries an initializer.
  Otherwise the parse fails with `Shader must provide a Graph block.`
- An empty `Graph = { }` is therefore legal exactly when some output is initialized in `Outputs`.
- Bindings are what actually connect the material. A `Shader` with no bindings parses with a warning
  and then fails at generation with `<file>: Outputs block is required.`

```c
// Legal: no Graph body needed, the output declaration carries its own initializer.
Shader(Name="M_Flat")
{
    Properties = { vec3 Tint = vec3(1.0, 0.2, 0.2); }
    Outputs = {
        vec3 Color = Tint;
        Base.EmissiveColor = Color;
    }
    Graph = { }
}
```

## Generated asset

`Name` + `Root` resolve to a package path exactly as described in
[Asset paths](../generation/asset-paths.md). Which UClass lands there depends on the resolved
backend:

| Backend | Asset written | Notes |
| :-- | :-- | :-- |
| `Graph` | `UMaterial` | the node graph is built directly on the material |
| `ThinCustom` *(since 1.5.0)* | `UDreamShaderMaterialInstance` whose parent is a hidden `UMaterial` subobject named `MB_DreamThinBase_<leaf>` | one asset, one package |
| `Instance` | alias for `ThinCustom` *(deprecated in 1.5.0)* | |

The backend comes from `Settings = { Backend = "…"; }` if present, otherwise from the project's
**Default Compiler Backend** setting. See [Backend](../settings/backend.md) and
[Project settings](../settings/project.md).

> [!NOTE]
> The interactive editor never writes a per-material `.uasset`. Auto-compile-on-save, the Gen page
> buttons, and the live preview all generate **in memory**. Assets reach disk only at cook, through
> the [commandlet](../tools/commandlet.md), or through an explicit *Materialize* action. See
> [In-memory materials](../generation/in-memory.md).

> [!WARNING]
> Regeneration clears the target graph. Node positions not pinned by [`Layout`](layout.md), added
> nodes, node property tweaks, comment boxes whose text begins with `DreamShader: `, and — under the
> ThinCustom backend — every parameter override on the generated instance are destroyed. Only
> comment boxes that do **not** carry the `DreamShader: ` prefix survive. See
> [Regeneration](../generation/regeneration.md).

## Notes

- **At most one `Shader` per parse unit.** Because `import` directives are inlined into a single text
  before parsing, "one `Shader`" is enforced across the whole transitive import closure, not per
  file. A second `Shader` keyword fails with
  `Only one top-level Shader block is currently supported.`
- **The file-kind restriction is a substring scan, not a parse.** A `.dsh` is rejected if its text
  contains `Shader(`, `ShaderFunction(`, `ShaderLayer(`, `ShaderLayerBlend(`, `MaterialLayer(` or
  `MaterialLayerBlend(` anywhere — including inside a comment or a string literal. A `.dsf` is
  rejected if it contains `Shader(`. Note that `ShaderFunction(` does not contain the substring
  `Shader(`, which is what lets a `.dsf` hold function blocks. See
  [Source files](source-files.md).
- **`Shader()` with no attributes parses, then fails.** The empty attribute list is syntactically
  valid; the missing `Name` is what produces the error.
- `Shader` may share a `.dsm` with any number of `ShaderFunction`, `ShaderLayer`,
  `ShaderLayerBlend`, `VirtualFunction`, `Function`, `GraphFunction` and `Namespace` blocks. All of
  them are generated by one compile of that file.
- A `.dsm` that declares no `Shader` block is still compilable — it simply produces whatever function
  assets it does declare.
- Binding `Base.MaterialAttributes` auto-enables *Use Material Attributes* on the material.
  Binding `Base.FrontMaterial` force-sets the shading model to Substrate and requires UE 5.4+.
  The two cannot be used by the same `Shader`.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

### Parse time

| Message | Cause |
| :-- | :-- |
| `Shader(Name="...") is required.` | the header has no `Name` attribute |
| `Only one top-level Shader block is currently supported.` | a second `Shader` block in the parse unit (import closure included) |
| `Shader must provide a Graph block.` | no `Graph` section and no initialized output declaration |
| `Unknown shader section '{Section}'.` | a section name other than `Properties`, `Settings`, `Outputs`, `Graph`, `Layout`, `Code` |
| `Shader graph sections now use Graph = { ... }. Function Code = { ... } is still supported.` | a `Code` section was used |
| `Expected '{' near index {Index}.` | the body block is missing |
| `Unterminated block.` | the body `{` is never closed |
| `Expected ',' or ')' near index {Index}.` | malformed attribute list |

### Parse-time warnings

Warnings do not fail the parse; they are appended to the compile message.

| Message | Cause |
| :-- | :-- |
| `No Outputs block was provided. Generation requires explicit material property bindings.` | the `Shader` declared no output bindings |

### Generation time

| Message | Cause |
| :-- | :-- |
| `{File}: This file does not define a top-level Shader block.` | material generation was asked for a file with no `Shader` |
| `{File}: Outputs block is required.` | the `Shader` declared no output bindings |
| `{File}: .dsf files cannot define top-level Shader blocks.` | a `Shader` block reached the compiler from a `.dsf` |
| `DreamShader source '{File}' cannot generate a material asset directly.` | material generation was requested for a `.dsh` or `.dsf` |
| `{File}: Property '{Name}' is declared more than once. Property names must be unique.` | two `Properties` entries with names equal ignoring case |
| `{File}: Base.FrontMaterial and Base.MaterialAttributes cannot be used by the same Shader.` | both bindings present |
| `{File}: Base.FrontMaterial requires ShadingModel="Substrate" or no explicit ShadingModel setting.` | conflicting explicit shading model |
| `{File}: Graph blocks do not support binding Outputs to the reserved name 'return'.` | `return` bound in a `Shader` that has a `Graph` block |
| `Unsupported Backend '{Value}'. Supported values: Graph, Instance, ThinCustom.` | `Settings = { Backend = … }` value not recognized |
| `Asset '{ObjectPath}' already exists and is not a Material.` | the target path holds a different UClass |
| `Asset '{ObjectPath}' already exists and is not a DreamShader instance material. Delete it (or remove Backend="Instance") before switching backends.` | ThinCustom backend, target path holds a non-DreamShader object |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating.` | ownership guard: a saved asset at the target path lacks DreamShader provenance metadata |
| `DreamShader asset name must resolve to a non-empty asset path.` | `Name` is empty after trimming and slash-stripping |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Materials/M_Emissive", Root="Game")
{
    Properties = {
        Group("Look") {
            vec3  Tint      = vec3(1.0, 0.4, 0.1) [Description="Emissive tint"];
            float Intensity = 2.0                 [Slider(0, 10)];
        }
        TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_Noise");
    }

    Settings = {
        ShadingModel = "Unlit";
        BlendMode    = "Translucent";
        TwoSided     = true;
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        vec2 UV  = UE.TexCoord(Index = 0);
        vec4 Tex = BaseTex(Coordinates = UV);
        Color = Tex.rgb * Tint * Intensity;
        Alpha = Tex.a;
    }

    Layout = {
        Comment(Name="Sampling", X=-1200, Y=-200, W=900, H=400);
        Node(Var="UV", X=-1100, Y=-120);
    }
}
```

Generated asset:

```text
package     /Game/Materials/M_Emissive
object path /Game/Materials/M_Emissive.M_Emissive
on disk     <Project>/Content/Materials/M_Emissive.uasset      (persist mode only)
```

> [!NOTE]
> `BaseTex` is declared as `TextureSampleParameter2D`, not as the compact `Texture2D`, because only
> the sample-parameter family owns the input pins the `BaseTex(Coordinates = UV)` call form wires. A
> compact `Texture2D` generates a texture *object* parameter, which has no such pins; calling it
> falls through to the function dispatcher and fails. See
> [Using parameters in Graph](../parameters/graph-usage.md#the-pin-call-form).

## See also

- [Source files](source-files.md) — which block kinds each of `.dsm` / `.dsh` / `.dsf` may contain
- [Properties](properties.md) — the `Properties` section grammar
- [Inputs / Outputs / Results](inputs-outputs.md) — typed-parameter sections (functions only)
- [Output bindings](output-bindings.md) — the full `Base.*` target catalogue and `Expression(…).Pin[i]`
- [Layout](layout.md) — `Node` / `Comment` placement directives and `#Region`
- [Graph](../graph/index.md) — the statement/expression language inside `Graph`
- [Shader settings](../settings/material.md) — every key a `Shader`'s `Settings` accepts
- [Backend](../settings/backend.md) — `Graph` vs `ThinCustom`, and the deprecated `Instance` alias
- [Asset paths](../generation/asset-paths.md) — `Name=` + `Root=` → package path
- [In-memory materials](../generation/in-memory.md) — memory-only generation and materializing to disk
- [Regeneration](../generation/regeneration.md) — what survives a rebuild and what does not
- [ShaderFunction](shader-function.md) — the reusable `UMaterialFunction` block
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
