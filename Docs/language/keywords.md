# Keyword index

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Keyword index**

Every word the declaration grammar treats specially: top-level block keywords, section names and
their aliases, declaration qualifiers, contextual keywords, reserved names, and the identifiers that
are rewritten before HLSL is emitted.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` |
| Kind | index |
| Case rule | top-level block keywords are case-**sensitive**; everything else on this page is case-**insensitive** |

## Synopsis

```c
<top-level-keyword> ( <attribute-key> = <value> [, …] )
{
    <section-name> [=] { <qualifier>… <type> <name> [= <default>] [[ <metadata> ]] ; … } [;]
}
```

> [!NOTE]
> **No keyword is reserved against identifiers.** A property, output variable, parameter or function
> may be named `Shader`, `Graph`, `Layout` or `float`; the parser never rejects an identifier because
> it collides with a keyword. Keywords are recognized only where the grammar expects them.

## Top-level block keywords

Matched **case-sensitively**, and only when the following character is not a letter, a digit or `_`.
That boundary rule is what keeps `ShaderFunction` from matching as `Shader`.

| Keyword | Header | Required attributes | Generates | Reference |
| :-- | :-- | :-- | :-- | :-- |
| `Shader` | `Shader(Name = "…"[, Root = "…"])` | `Name` | `UMaterial` | [Shader](shader.md) |
| `ShaderFunction` | `ShaderFunction(Name = "…"[, Root = "…"])` | `Name` | `UMaterialFunction` | [ShaderFunction](shader-function.md) |
| `ShaderLayer` *(since 1.3.0)* | `ShaderLayer(Name = "…"[, Root = "…"])` | `Name` | `UMaterialFunctionMaterialLayer` | [ShaderLayer](shader-layer.md) |
| `ShaderLayerBlend` *(since 1.3.0)* | `ShaderLayerBlend(Name = "…"[, Root = "…"])` | `Name` | `UMaterialFunctionMaterialLayerBlend` | [ShaderLayer](shader-layer.md) |
| `MaterialLayer` *(deprecated in 1.3.0)* | `MaterialLayer(Name = "…"[, Root = "…"])` | `Name` | as `ShaderLayer` | [ShaderLayer](shader-layer.md) |
| `MaterialLayerBlend` *(deprecated in 1.3.0)* | `MaterialLayerBlend(Name = "…"[, Root = "…"])` | `Name` | as `ShaderLayerBlend` | [ShaderLayer](shader-layer.md) |
| `VirtualFunction` *(since 1.2.0)* | `VirtualFunction(Name = "…"[, Asset = "…"])` | `Name`, and an asset from `Asset =` or `Options.Asset` | nothing — declares an existing asset | [VirtualFunction](virtual-function.md) |
| `Namespace` | `Namespace(Name = "…")` | `Name` | nothing — a scope for helpers | [Namespace](namespace.md) |
| `Function` | `Function [SelfContained \| Inline] [<ret>] <Name>( … ) { <HLSL> }` | a name, at least one output | HLSL helper | [Function](function.md) |
| `GraphFunction` *(since 1.3.1)* | `GraphFunction [<ret>] <Name>( … ) { <HLSL> }` | a name, at least one output | HLSL helper with node inputs | [GraphFunction](graph-function.md) |

`Shader` is limited to **one per translation unit**, across the whole import closure. Every other
block may be repeated. `Function` and `GraphFunction` may also appear nested inside `Namespace`,
where their names become `<Namespace>::<Name>`.

> [!WARNING]
> `MaterialLayer` and `MaterialLayerBlend` are deprecated since 1.3.0 in favour of `ShaderLayer` and
> `ShaderLayerBlend`. They still parse; each emits a warning and the parse succeeds:
> `MaterialLayer is deprecated; use ShaderLayer instead.` and
> `MaterialLayerBlend is deprecated; use ShaderLayerBlend instead.` Later diagnostics and generated
> metadata always report the modern spelling.

## Section names

Matched case-insensitively. The `=` before the block is optional *(since 1.5.0)*, as is the `;` after
it. Sections may appear in any order and may repeat.

| Name | Accepted in | Meaning | Reference |
| :-- | :-- | :-- | :-- |
| `Properties` | `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` | parameter, `const` and `UE.*` node declarations | [Properties](properties.md) |
| `Properties` | `VirtualFunction` | **alias for `Inputs`** — typed parameters, not parameter nodes | [VirtualFunction](virtual-function.md) |
| `Inputs` | `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction` | typed input parameters | [Inputs / Outputs](inputs-outputs.md) |
| `Outputs` | `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction` | typed output parameters | [Inputs / Outputs](inputs-outputs.md) |
| `Outputs` | `Shader` | output declarations and bindings — **a different grammar** | [Output bindings](output-bindings.md) |
| `Results` | `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction` | alias for `Outputs` | [Inputs / Outputs](inputs-outputs.md) |
| `Settings` | `Shader` | material settings — special keys plus reflected `UMaterial` properties | [Material settings](../settings/material.md) |
| `Settings` | `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` | the four material-function keys | [Function settings](../settings/function.md) |
| `Settings` | `VirtualFunction` | alias for `Options` | [Options](options.md) |
| `Options` | `VirtualFunction` | the declared asset and other stored keys | [Options](options.md) |
| `Graph` | `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` | the node-graph body, stored verbatim | [Graph](../graph/index.md) |
| `Code` | — | **rejected everywhere**; use `Graph` | [Graph](../graph/index.md) |
| `Layout` | `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` | `Node` / `Comment` placement; a second `Layout` **replaces** the first | [Layout](layout.md) |

`Function` and `GraphFunction` have no sections at all — their `{ … }` is raw HLSL.

> [!WARNING]
> `Code = { … }` is a hard parse error in every block that accepts sections. The messages read
> `Shader graph sections now use Graph = { ... }. Function Code = { ... } is still supported.` and
> `ShaderFunction, ShaderLayer, and ShaderLayerBlend graph sections now use Graph = { ... }. Function Code = { ... } is still supported.`
> Despite what they say, there is no reachable grammar in which `Code` is accepted. Use `Graph`.

## Declaration qualifiers

| Qualifier | Position | Effect | Reference |
| :-- | :-- | :-- | :-- |
| `const` *(since 1.2.6)* | before the type in a `Properties` declaration | emits a constant node instead of a parameter; rejected on parameter-node and `UE.*` declarations | [Properties](properties.md) |
| `opt` *(since 1.2.3)* | before the type in an `Inputs` declaration | marks the function input optional in Unreal; must be followed by a literal space | [Inputs / Outputs](inputs-outputs.md) |
| `in` | before a `Function` / `GraphFunction` parameter type | input parameter; the default when a parameter has only two tokens | [Function](function.md) |
| `out` | before a `Function` / `GraphFunction` parameter type | output parameter; at least one is required unless a return type is declared | [Function](function.md) |
| `SelfContained` | after `Function` | emits the body as a self-contained function; **not accepted on `GraphFunction`** | [Function](function.md) |
| `Inline` | after `Function` | exact alias of `SelfContained` | [Function](function.md) |

`in` and `out` are the only accepted qualifiers on a function parameter; anything else fails with
`Function '{Name}' parameter '{Param}' uses unsupported qualifier '{Qualifier}'. Supported qualifiers are in and out.`

## Contextual keywords

Recognized only in the position listed, always case-insensitively.

| Keyword | Position | Meaning | Reference |
| :-- | :-- | :-- | :-- |
| `import` | first token of its own line, anywhere in a source file | inlines another source file | [`import`](import.md) |
| `Group("…") { … }` *(since 1.5.0)* | statement position inside `Properties` | scopes a parameter group onto every declaration it contains; nests, composing with `\|` | [Properties](properties.md) |
| `Slider(min, max)` *(since 1.5.0)* | entry inside a `[ … ]` metadata block | sets a scalar parameter's UI range | [Metadata block](../parameters/metadata.md) |
| `Path( … )` | default value of a texture or asset-valued declaration | asset reference with a root spelling | [`Path(...)`](../parameters/path.md) |
| `Base.` | start of an `Outputs` binding target | binds to a material property | [Output bindings](output-bindings.md) |
| `Expression( … )` | start of an `Outputs` binding target | binds to a pin on a reflected node; `Class="…"` is mandatory | [Output bindings](output-bindings.md) |
| `.Pin[<index>]` | after an `Expression( … )` target | selects the pin to bind | [Output bindings](output-bindings.md) |
| `Node( … )` | statement inside `Layout` | pins a variable's node position | [Layout](layout.md) |
| `Comment( … )` | statement inside `Layout` | places a comment box | [Layout](layout.md) |
| `#Region` / `#EndRegion` | own line inside a `Graph` body | names a region of the graph; nests | [Layout](layout.md) |
| `UE.` | type position in `Properties`, call position in `Graph` | builtin material-node namespace | [`UE.*` catalogue](../builtins/ue.md) |
| `true` / `false` | default values | boolean literal; converts to `1.0` / `0.0` where a scalar is expected | [Types](types.md) |
| `default` *(since 1.2.3)* | call argument in `Graph` | use the parameter's declared default | [Calls](../graph/calls.md) |

## Reserved names

| Name | Where | Rule |
| :-- | :-- | :-- |
| `__return` | `Function` / `GraphFunction` parameter names | reserved for return-type lowering; a declared return type becomes an `out` parameter with this name. Using it fails with `Function '{Name}' parameter name '__return' is reserved for return-type lowering.` |
| `return` | `Shader` `Outputs` declarations | may not be used as an output-variable name, and as a binding source it may only feed `Base.*` targets |

`return` inside a `Function` body is a statement, not a reserved name: a top-level `return <expr>;`
is rewritten to an assignment to `__return`. A bare `return;` in a function that declares a return
type fails with
`A function with a return type cannot use a bare 'return;'. Return a value, e.g. 'return expr;'.`

## Identifier rewrites

Identifiers rewritten inside `Function` and `GraphFunction` declarations before HLSL is emitted.
Matching is case-insensitive and whole-identifier only; text inside strings and comments is left
alone, and a `::`-qualified name bypasses the table entirely.

| Written | Rewritten to | Applies to |
| :-- | :-- | :-- |
| `vec2` | `float2` | parameter and return types, body text |
| `vec3` | `float3` | parameter and return types, body text |
| `vec4` | `float4` | parameter and return types, body text |
| `ivec2` | `int2` | parameter and return types, body text |
| `ivec3` | `int3` | parameter and return types, body text |
| `ivec4` | `int4` | parameter and return types, body text |
| `uvec2` | `uint2` | parameter and return types, body text |
| `uvec3` | `uint3` | parameter and return types, body text |
| `uvec4` | `uint4` | parameter and return types, body text |
| `bvec2` | `bool2` | parameter and return types, body text |
| `bvec3` | `bool3` | parameter and return types, body text |
| `bvec4` | `bool4` | parameter and return types, body text |
| `mat2` | `float2x2` | parameter and return types, body text |
| `mat3` | `float3x3` | parameter and return types, body text |
| `mat4` | `float4x4` | parameter and return types, body text |
| `mix` | `lerp` | body text only |
| `fract` | `frac` | body text only |
| `mod` | `fmod` | body text only |

A token that matches nothing in this table is emitted unchanged. The type spellings accepted in
`Properties`, `Inputs`, `Outputs` and `Results` are matched by their own lists rather than through
this table — see [Types](types.md).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

| Message | Cause |
| :-- | :-- |
| `Unexpected token near index {Index}.` | text at top level that is not one of the ten block keywords — including a correctly spelled keyword in the wrong case |
| `{Block}(Name="...") is required.` | a block header with no `Name` attribute; `{Block}` is the spelling actually typed |
| `Only one top-level Shader block is currently supported.` | a second `Shader` in the translation unit |
| `Unknown shader section '{Section}'.` | a section name a `Shader` does not accept |
| `Unknown material function section '{Section}'.` | a section name a `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` does not accept |
| `Unknown VirtualFunction section '{Section}'.` | a section name a `VirtualFunction` does not accept |
| `Namespace '{Name}' may only contain Function or GraphFunction blocks.` | any other block inside a `Namespace`, including a nested `Namespace` |
| `VirtualFunction declares an existing MaterialFunction asset and does not support Graph or Code sections.` | `Graph` or `Code` inside a `VirtualFunction` |
| `MaterialLayer is deprecated; use ShaderLayer instead.` | warning; the parse succeeds |
| `MaterialLayerBlend is deprecated; use ShaderLayerBlend instead.` | warning; the parse succeeds |

## Example

Every keyword class in one file.

```c
Namespace(Name="Common")
{
    Function float Luma(in vec3 color) {
        return dot(color, vec3(0.2126, 0.7152, 0.0722));   // vec3 -> float3 on emission
    }
}

Shader(Name="Materials/M_Keywords")
{
    Properties {
        Group("Look") {
            const vec3      Ambient = vec3(0.02, 0.02, 0.03);
            ScalarParameter Rough   = 0.5 [Group="Surface"; Slider(0, 1)];
        }
        TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_Noise");
    }

    Settings {
        Domain       = "Surface";
        ShadingModel = "Unlit";
        BlendMode    = "Opaque";
    }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        #Region "Sampling"
        vec2 UV  = UE.TexCoord(Index = 0);
        vec4 Tex = BaseTex(Coordinates = UV);
        #EndRegion

        Color = Tex.rgb * Common::Luma(Tex.rgb) + Ambient * Rough;
    }

    Layout {
        Comment(Name="Sampling", X=-1200, Y=-200, W=900, H=400);
        Node(Var="UV", X=-1100, Y=-120);
    }
}
```

## See also

- [Lexical elements](lexical.md) — the case-sensitivity matrix and the token rules behind this index
- [Source files](source-files.md) — which blocks each file kind may declare
- [Types](types.md) — every type token and its per-context validity
- [Shader](shader.md) · [ShaderFunction](shader-function.md) · [ShaderLayer](shader-layer.md) ·
  [VirtualFunction](virtual-function.md) · [Function](function.md) ·
  [GraphFunction](graph-function.md) · [Namespace](namespace.md) — the block pages
- [Properties](properties.md) · [Inputs / Outputs](inputs-outputs.md) ·
  [Output bindings](output-bindings.md) · [Options](options.md) · [Layout](layout.md) — the section pages
- [`import`](import.md) — the one directive that is not part of the grammar
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
