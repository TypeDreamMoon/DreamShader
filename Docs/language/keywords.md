# Keyword index

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Keyword index**

Every word the declaration grammar treats specially: top-level block keywords, section names and
their aliases, declaration qualifiers, contextual keywords, reserved names, and the identifiers that
are rewritten before HLSL is emitted.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` |
| Kind | index |
| Case rule | top-level block keywords and the [reserved words](#reserved-words) are case-**sensitive**; everything else on this page is case-**insensitive** unless its row says otherwise |

## Synopsis

```c
<top-level-keyword> ( <attribute-key> = <value> [, …] )
{
    <section-name> [=] { <qualifier>… <type> <name> [= <default>] [[ <metadata> ]] ; … } [;]
}
```

> [!NOTE]
> **Block words, section names and type names are not reserved against identifiers.** A property,
> output variable, parameter or function may be named `Shader`, `Graph` or `Layout`; they are
> recognized only where the grammar expects them. The [reserved words](#reserved-words) are the
> exception *(since 2.0.0)*.

## Top-level block keywords

Matched **case-sensitively**, as a whole word: `ShaderFunction` is its own word, never `Shader`
followed by something. A mis-cased one (`shader(…)`) is not a block:
[`DSH2240`](../diagnostics/DSH2xxx.md#dsh2240).

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

`Shader` is limited to **one per file** ([`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250)), and a `.dsh`
header holds no asset block at all ([`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249)), so an `import`
cannot add a second one. Every other block may be repeated. `Function` and `GraphFunction` may also
appear nested inside `Namespace`, where their names become `<Namespace>::<Name>`.

> [!WARNING]
> `MaterialLayer` and `MaterialLayerBlend` are deprecated since 1.3.0 in favour of `ShaderLayer` and
> `ShaderLayerBlend`. They still read the same, with the warning
> [`DSH2251`](../diagnostics/DSH2xxx.md#dsh2251). Later diagnostics and generated metadata always
> report the modern spelling.

## Section names

Matched case-insensitively. The `=` before the block is optional *(since 1.5.0)*, as is the `;` after
it. Sections may appear in any order and may repeat: `Properties`, `Inputs` and `Outputs` append,
`Settings` merges key by key, and a second `Graph` or `Layout` **replaces** the first, with the warning
[`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258) *(since 2.0.0)*. A name a block does not take is
[`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245).

| Name | Accepted in | Meaning | Reference |
| :-- | :-- | :-- | :-- |
| `Properties` | `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` | parameter, `const` and `UE.*` node declarations | [Properties](properties.md) |
| `Properties` | `VirtualFunction` | **alias for `Inputs`** — typed parameters, not parameter nodes | [VirtualFunction](virtual-function.md) |
| `Inputs` | `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction` | typed input parameters | [Inputs / Outputs](inputs-outputs.md) |
| `Outputs` | `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction` | typed output parameters | [Inputs / Outputs](inputs-outputs.md) |
| `Outputs` | `Shader` | output declarations and bindings — **a different grammar** | [Output bindings](output-bindings.md) |
| `Results` | `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction` | alias for `Outputs` | [Inputs / Outputs](inputs-outputs.md) |
| `Settings` | `Shader` | material settings — special keys plus reflected `UMaterial` properties | [Material settings](../settings/material.md) |
| `Settings` | `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` | the material-function keys | [Function settings](../settings/function.md) |
| `Settings` | `VirtualFunction` | alias for `Options` | [Options](options.md) |
| `Options` | `VirtualFunction` | the declared asset and other stored keys | [Options](options.md) |
| `Graph` | `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` | the node-graph body | [Graph](../graph/index.md) |
| `Code` | — | **rejected everywhere**; use `Graph` | [Graph](../graph/index.md) |
| `Layout` | `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` | `Node` / `Comment` placement | [Layout](layout.md) |

`Function` and `GraphFunction` have no sections at all — their `{ … }` is raw HLSL.

> [!WARNING]
> `Code = { … }` is an error in every block that accepts sections:
> [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246), and in a `VirtualFunction`, together with `Graph`,
> [`DSH2247`](../diagnostics/DSH2xxx.md#dsh2247). Use `Graph`.

## Declaration qualifiers

| Qualifier | Position | Effect | Reference |
| :-- | :-- | :-- | :-- |
| `const` *(since 1.2.6)* | before the type in a `Properties` declaration | emits a constant node instead of a parameter; refused on most parameter-node tokens ([`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253)) | [Properties](properties.md) |
| `opt` *(since 1.2.3)* | before the type in an `Inputs` declaration | marks the function input optional in Unreal; recognized when a type and a name follow it | [Inputs / Outputs](inputs-outputs.md) |
| `in` | before a `Function` / `GraphFunction` parameter type | input parameter; the default when a parameter has only two words | [Function](function.md) |
| `out` | before a `Function` / `GraphFunction` parameter type | output parameter; at least one is required unless a return type is declared, and none may stand beside one ([`DSH6305`](../diagnostics/DSH6xxx.md#dsh6305), [`DSH6304`](../diagnostics/DSH6xxx.md#dsh6304)) | [Function](function.md) |
| `SelfContained` | after `Function` | emits the body as a self-contained function; **not accepted on `GraphFunction`** ([`DSH6307`](../diagnostics/DSH6xxx.md#dsh6307)) | [Function](function.md) |
| `Inline` | after `Function` | exact alias of `SelfContained`, with the warning [`DSH6306`](../diagnostics/DSH6xxx.md#dsh6306) *(since 2.0.0)* | [Function](function.md) |

`in` and `out` are the only accepted qualifiers on a function parameter, in any case; anything else is
[`DSH6302`](../diagnostics/DSH6xxx.md#dsh6302).

## Contextual keywords

Recognized only in the position listed, and case-insensitively except where the row says otherwise.

| Keyword | Position | Meaning | Reference |
| :-- | :-- | :-- | :-- |
| `import` | top level, between blocks | declares the names of a `.dsh` header into this file *(since 2.0.0; 1.x pasted the file in)*; lower case — another case reads the same, with the warning [`DSH2253`](../diagnostics/DSH2xxx.md#dsh2253) | [`import`](import.md) |
| `Group("…") { … }` *(since 1.5.0)* | statement position inside `Properties` | scopes a parameter group onto every declaration it contains; nests, composing with `\|` | [Properties](properties.md) |
| `Slider(min, max)` *(since 1.5.0)* | entry inside a `[ … ]` metadata block | sets a scalar parameter's UI range | [Metadata block](../parameters/metadata.md) |
| `Path( … )` | default value of a texture or asset-valued declaration | asset reference with a root spelling | [`Path(...)`](../parameters/path.md) |
| `Base.` | start of an `Outputs` binding target | binds to a material property | [Output bindings](output-bindings.md) |
| `Expression( … )` | start of an `Outputs` binding target | binds to a pin on a reflected node; `Class="…"` is mandatory | [Output bindings](output-bindings.md) |
| `.Pin[<index>]` | after an `Expression( … )` target | selects the pin to bind | [Output bindings](output-bindings.md) |
| `Node( … )` | statement inside `Layout` | pins a variable's node position | [Layout](layout.md) |
| `Comment( … )` | statement inside `Layout` | places a comment box | [Layout](layout.md) |
| `#Region` / `#EndRegion` | own line inside a `Graph` body | names a region of the graph; nests | [Layout](layout.md) |
| `#if` / `#elif` / `#else` / `#endif` *(since 1.9.0)* | own line, anywhere except a `Function` / `GraphFunction` HLSL body | cuts the untaken branch out of the text before parsing; **lowercase only** | [Preprocessor](preprocessor.md) |
| `#ifdef` / `#ifndef` *(since 1.9.0)* | own line, same positions | sugar for `#if defined(NAME)` and `#if !defined(NAME)` | [Preprocessor](preprocessor.md) |
| `#define` / `#undef` *(since 1.9.0)* | own line, same positions | defines or removes a name, **for that file only**; a `#define` value runs to end of line and is plain text | [Preprocessor](preprocessor.md#define-is-file-local) |
| `defined` *(since 1.9.0)* | inside a `#if` / `#elif` expression | `1` when the name is defined, `0` otherwise; `defined(X)` and `defined X` both work | [Preprocessor](preprocessor.md#values) |
| `UE.` | type position in `Properties`, call position in `Graph` | builtin material-node namespace; in a `Graph` call written exactly `UE` *(since 2.0.0)* | [`UE.*` catalogue](../builtins/ue.md) |
| `true` / `false` | default values, `Graph` expressions | boolean literal; converts to `1.0` / `0.0` where a scalar is expected | [Types](types.md) |
| `default` *(since 1.2.3)* | call argument in `Graph` | leaves that input unconnected, so the function's own default applies; lower case only *(since 2.0.0)* | [Calls](../graph/calls.md) |

## Reserved words

*(since 2.0.0)* One lexer reads every file, and its keywords are reserved in a 1.x file too, in lower
case: `uniform` `static` `const` `extern` `export` `in` `out` `inout` `struct` `if` `else` `for` `while`
`do` `return` `break` `continue` `discard` `true` `false` `import`. None of them can name a property,
an output, an input or a `Graph` variable. Another case is an ordinary word: `Const` and `IN` are
read as `const` and `in` where the 1.x grammar expects those, and `True` as `true`.

## Reserved names

| Name | Where | Rule |
| :-- | :-- | :-- |
| `__return` | `Function` / `GraphFunction` parameter names | reserved, in any case; a parameter of that name is [`DSH6303`](../diagnostics/DSH6xxx.md#dsh6303) |
| `return` | everywhere | a [reserved word](#reserved-words): it cannot name an output variable, and is no binding source *(since 2.0.0)* |
| `DS_…` *(since 1.9.0)* | preprocessor define names | the whole `DS_` **prefix** is DreamShader's. `#define` or `#undef` of such a name fails with [`DSH1039`](../diagnostics/DSH1xxx.md#dsh1039); the other define tiers drop it with a warning. See [Preprocessor](preprocessor.md#the-builtin-ds_-constants) |

`return` inside a `Function` body is HLSL. A bare `return;` in a function that declares a return type
is [`DSH6308`](../diagnostics/DSH6xxx.md#dsh6308).

## Identifier rewrites

Identifiers rewritten inside `Function` and `GraphFunction` declarations before HLSL is emitted.
Matching is case-insensitive and whole-identifier only; text inside strings and comments is left
alone, and a `::`-qualified name bypasses the table — it is flattened to `<Namespace>_<Name>`.

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

A token that matches nothing in this table is emitted unchanged. Type tokens in every section are
read through the one type table on [Types](types.md), which holds the same GLSL spellings.

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH2240` | text at top level that is not a block keyword or `import` — including a correctly spelled keyword in the wrong case |
| [`DSH2248`](../diagnostics/DSH2xxx.md#dsh2248) | 2.0 syntax at the top level of a `.dsm` / `.dsf` |
| [`DSH2242`](../diagnostics/DSH2xxx.md#dsh2242) | an asset block header with no `Name` attribute |
| [`DSH6309`](../diagnostics/DSH6xxx.md#dsh6309), [`DSH6311`](../diagnostics/DSH6xxx.md#dsh6311) | a `Namespace` / `VirtualFunction` with no name, or one that is not an identifier |
| `DSH2250` | a second `Shader` in the file |
| `DSH2245` | a section name the block does not accept |
| `DSH2246`, `DSH2247` | a `Code` section; a `Graph` or `Code` section in a `VirtualFunction` |
| [`DSH6310`](../diagnostics/DSH6xxx.md#dsh6310) | anything but `Function` or `GraphFunction` inside a `Namespace`, including a nested `Namespace` |
| `DSH2251` | warning: `MaterialLayer` / `MaterialLayerBlend` |
| `DSH2253` | warning: `import` in another case |
| `DSH2258` | warning: a second `Graph` or `Layout` section |
| `DSH6302`, `DSH6303`, `DSH6304`, `DSH6305`, `DSH6306`, `DSH6307`, `DSH6308` | the `Function` / `GraphFunction` signature rules above |

The complete list is in the [diagnostics index](../diagnostics/index.md).

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
- [`import`](import.md) · [Preprocessor](preprocessor.md) — the directives that are not part of the grammar
- [Diagnostics index](../diagnostics/index.md) — every code
