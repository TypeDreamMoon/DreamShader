# Shader

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Shader**

A top-level block that declares one Unreal material asset: its parameters, its render-state settings,
its output bindings, and the node graph that feeds them.

| | |
| :-- | :-- |
| Declared in | `.dsm` — a `.dsh` holding one is [`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249) (see [Source files](source-files.md#how-the-restriction-is-enforced)) |
| Kind | top-level block |
| Generates | `UMaterial` (Graph backend) or `UDreamShaderMaterialInstance` + a hidden `UMaterial` base (ThinCustom backend) |
| Multiplicity | at most one per file (see [Notes](#notes)) |

## Synopsis

```c
Shader(Name = "<asset-path>" [, Root = "<root>"])
{
    [Properties [=] { <property-declaration> ; … }]
    [Settings   [=] { <key> = <value> ; … }]
    [Outputs    [=] { { <output-declaration> ; | <output-binding> ; | Expression( … ) { Pin[i] = <source> ; … } } … }]
    [Graph      [=] { <graph-statement> … }]
    [Layout     [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

The `=` between a section name and its `{ … }` block is optional sugar *(since 1.5.0)*; a `;` after a
section's closing `}` is optional. Sections may appear in any order and may be repeated.

The keyword `Shader` is matched **case-sensitively**; section names are matched
case-insensitively. See [Lexical elements](lexical.md#case-sensitivity).

A `.dss` writes the same material as `#pragma material(...)`, `uniform`s and one
`export void <Name>(inout material m)` — see [DreamShaderLang 2.0](../language-v2/index.md);
[`dsc migrate`](../tools/migrate.md) rewrites a `.dsm` that way.

## Header attributes

| Attribute | Required | Value | Effect |
| :-- | :-- | :-- | :-- |
| **`Name`** | yes | string | The asset's logical path. The last `/`-separated segment is the asset name; preceding segments become folders. |
| `Root` | no | string | The package root the folders hang off. Defaults to `/Game` when absent or empty. |

Attribute keys are matched case-insensitively (`name=` works). Values may be quoted or bare; a bare
value ends at the first `,` or `)`. A duplicate key keeps the later value, with the warning
[`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244) *(since 2.0.0; silent before)*. A trailing comma before
`)` is accepted, as in 1.x *(2.0.0 – 2.1.0 refused it with
[`DSH2243`](../diagnostics/DSH2xxx.md#dsh2243))*.

Full `Name` / `Root` grammar, the accepted root spellings, and the resulting on-disk path are
specified in [Asset paths](../generation/asset-paths.md).

## Sections

| Section | Accepted | Repeat behaviour | Reference |
| :-- | :-- | :-- | :-- |
| `Properties` | yes | appends | [Properties](properties.md) |
| `Settings` | yes | merges; last key wins, a key written twice is the warning [`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262) | [Shader settings](../settings/material.md) |
| `Outputs` | yes | appends | [Output bindings](output-bindings.md) |
| `Graph` | yes | the later body wins, with the warning [`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258) | [Graph](../graph/index.md) |
| `Layout` | yes | **resets** — a second `Layout` discards the first, with the warning `DSH2258` | [Layout](layout.md) |
| `Code` | **no** — [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) | — | — |
| `Inputs` | no — unknown section, [`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) | — | — |
| `Results` | no — unknown section, `DSH2245` | — | — |
| `Options` | no — unknown section, `DSH2245` | — | — |

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
| `Expression( … ) { Pin[<i>] = <variable> ; … }` | [block form](output-bindings.md#block-form) — several pins on one terminal node *(since 1.9.0)* |

The rule that governs whether `Graph` is required is evaluated after the whole block is read:

- A `Shader` with no `Graph` section and an empty `Outputs` is
  [`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255). Without a `Graph`, the `Outputs` statements have to
  compute the values themselves — an output declaration with an initializer, or *(since 2.0.0)* a
  binding whose right side is an expression, such as `Base.FrontMaterial = Substrate.Layer(Coat, Body);`.
- An empty `Graph = { }` is legal.
- Bindings are what actually connect the material. A `Shader` with no `Outputs` section is the warning
  [`DSH2256`](../diagnostics/DSH2xxx.md#dsh2256): the material builds, with nothing wired to it
  *(since 2.0.0; through 1.9.x generation then failed)*.

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
| `ThinCustom` *(since 1.5.0)* | `UDreamShaderMaterialInstance` whose parent is a hidden `UMaterial` subobject named `MB_DreamThinBase_<leaf>` | one asset, one package; parameter overrides you set on the instance are yours to keep — a rebuild captures them and puts back every name the new base still declares *(since 1.9.0)*, see [Divergence](../generation/divergence.md#parameter-overrides-on-a-generated-thincustom-instance) |
| `Instance` | alias for `ThinCustom` *(deprecated in 1.5.0)* | |

The backend comes from `Settings = { Backend = "…"; }` if present, otherwise from the project's
**Default Compiler Backend** setting. See [Backend](../settings/backend.md) and
[Project settings](../settings/project.md).

> [!NOTE]
> A `Graph`-backend `UMaterial` is an ordinary asset and is saved on every successful build *(since
> 2.0.0)*. A `ThinCustom` product stays **Ephemeral** — in memory, no package on disk — until a cook, an
> explicit *Materialize*, or a child instance gives it one, and once it has a package it stays on
> disk. See [Ephemeral materials](../generation/ephemeral.md).

> [!WARNING]
> Regeneration clears the target graph. Node positions not pinned by [`Layout`](layout.md), added
> nodes, node property tweaks, and comment boxes whose text begins with `DreamShader: ` are
> destroyed. Only comment boxes that do **not** carry the `DreamShader: ` prefix survive. Under the
> ThinCustom backend the instance's parameter overrides are cleared too, but they are captured first
> and restored afterwards *(since 1.9.0)* — a parameter the source no longer declares is the one case
> that loses its value. See [Regeneration](../generation/regeneration.md).

## Notes

- **At most one `Shader` per file** ([`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250)). Through 1.9.x
  imports were inlined into one text before parsing, so the rule spanned the import closure; a header
  is read on its own now and cannot hold a `Shader` ([`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249)),
  so an [`import`](import.md) never adds one. The `.dsh` check reads tokens, not text: a comment that
  mentions `Shader(` is fine. See [Source files](source-files.md#how-the-restriction-is-enforced).
- **`Shader()` with no attributes is [`DSH2242`](../diagnostics/DSH2xxx.md#dsh2242)**, the missing
  `Name`.
- `Shader` may share a `.dsm` with `VirtualFunction`, `Function`, `GraphFunction` and `Namespace`
  blocks, which are helpers of the material, and with `ShaderFunction`, `ShaderLayer` and
  `ShaderLayerBlend` blocks: one compile of the file builds them all, each into its own asset, and the
  material may call such a function by name *(2.0.0 – 2.1.0 refused it with [`DSH6201`](../diagnostics/DSH6xxx.md#dsh6201))*. A `Shader`
  in a `.dsf` is [`DSH2259`](../diagnostics/DSH2xxx.md#dsh2259).
- A `.dsm` that declares no `Shader` block is still compilable — it simply produces whatever function
  assets it does declare.
- Binding `Base.MaterialAttributes` turns on *Use Material Attributes* on the material. See
  [Output bindings](output-bindings.md) for `Base.FrontMaterial` and the other targets.

## Diagnostics

Every stage reports all of its errors, each at its own line and column *(since 2.0.0)*.

### Parse time

| Code | Raised when |
| :-- | :-- |
| [`DSH2241`](../diagnostics/DSH2xxx.md#dsh2241) | `Shader` is not followed by its attribute list |
| [`DSH2242`](../diagnostics/DSH2xxx.md#dsh2242) | the attribute list has no `Name`, or an empty one |
| [`DSH2243`](../diagnostics/DSH2xxx.md#dsh2243) | a malformed attribute list — a missing key, `=` or value, a missing `,` or `)` |
| [`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244) | *(warning)* an attribute written twice |
| [`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250) | a second `Shader` block in the file |
| [`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249) | a `Shader` block in a `.dsh` |
| [`DSH2257`](../diagnostics/DSH2xxx.md#dsh2257) | the body `{`, a section name, or a section's `{` is missing |
| [`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) | a section other than `Properties`, `Settings`, `Outputs`, `Graph` and `Layout` |
| [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) | a `Code` section |
| [`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258) | *(warning)* a `Graph` or `Layout` section written twice |
| [`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255) | no `Graph` section, and an empty `Outputs` |
| [`DSH2256`](../diagnostics/DSH2xxx.md#dsh2256) | *(warning)* no `Outputs` section |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | the file ends inside the block |

### Bind and generation time

| Code | Raised when |
| :-- | :-- |
| [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210) | a property, a function or another name declared twice |
| [`DSH7202`](../diagnostics/DSH7xxx.md#dsh7202) | `Settings = { Backend = … }` names no backend; `Instance` is read as `ThinCustom` with the warning [`DSH7204`](../diagnostics/DSH7xxx.md#dsh7204) |
| [`DSH8200`](../diagnostics/DSH8xxx.md#dsh8200) | `Name` / `Root` do not resolve to a valid asset path |
| [`DSH8201`](../diagnostics/DSH8xxx.md#dsh8201) | the material could not be created or reused — the target path holds another class, or an asset DreamShader did not generate; the reason is appended |
| [`DSH8203`](../diagnostics/DSH8xxx.md#dsh8203) | the same, for a ThinCustom instance |
| [`DSH8206`](../diagnostics/DSH8xxx.md#dsh8206) | the asset is open in an asset editor, so it was not rebuilt |
| [`DSH8207`](../diagnostics/DSH8xxx.md#dsh8207) | the asset was edited by hand since it was generated, so it was not rebuilt; the reason it carries is the `DSH8115` text — see [Divergence](../generation/divergence.md) |

The messages of `Properties`, `Settings`, `Outputs` and `Graph` are on their own pages. Every code:
[diagnostics](../diagnostics/README.md).

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
on disk     <Project>/Content/Materials/M_Emissive.uasset      (once Materialized, or under the Graph backend)
```

> [!NOTE]
> `BaseTex` is declared as `TextureSampleParameter2D`, not as the compact `Texture2D`, because only
> the sample-parameter family owns the input pins the `BaseTex(Coordinates = UV)` call form wires. A
> compact `Texture2D` generates a texture *object* parameter, which has no such pins; calling it
> falls through to the function dispatcher and fails. See
> [Using parameters in Graph](../parameters/graph-usage.md#the-pin-call-form).

## See also

- [Source files](source-files.md) — which block kinds each of `.dsm` / `.dsh` / `.dsf` may contain
- [DreamShaderLang 2.0](../language-v2/index.md) — the `.dss` form of a material
- [Properties](properties.md) — the `Properties` section grammar
- [Inputs / Outputs / Results](inputs-outputs.md) — typed-parameter sections (functions only)
- [Output bindings](output-bindings.md) — the full `Base.*` target catalogue and `Expression(…).Pin[i]`
- [Layout](layout.md) — `Node` / `Comment` placement directives and `#Region`
- [Graph](../graph/index.md) — the statement/expression language inside `Graph`
- [Shader settings](../settings/material.md) — every key a `Shader`'s `Settings` accepts
- [Backend](../settings/backend.md) — `Graph` vs `ThinCustom`, and the deprecated `Instance` alias
- [Asset paths](../generation/asset-paths.md) — `Name=` + `Root=` → package path
- [Ephemeral materials](../generation/ephemeral.md) — memory-only generation and materializing to disk
- [Regeneration](../generation/regeneration.md) — what survives a rebuild and what does not
- [ShaderFunction](shader-function.md) — the reusable `UMaterialFunction` block
- [Diagnostics](../diagnostics/README.md) — every code
