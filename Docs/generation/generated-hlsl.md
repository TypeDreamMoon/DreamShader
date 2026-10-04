# Generated HLSL

> [DreamShader](../index.md) » [Generation](index.md) » **Generated HLSL**

The HLSL a compile writes for the `Function` blocks of a source. *(since 2.0.0)* It is written into the
Custom node each call makes; no `.ush` include is generated.

| | |
| :-- | :-- |
| Written when | a `Graph` calls a [`Function`](../language/function.md) — one Custom node per call |
| Written to | the Custom node's `Code`; its `IncludeFilePaths` carry only the `#include` lines hoisted out of the bodies |
| Contains | the called function's body, verbatim, and every `Function` that body calls, embedded in a wrapper struct |
| Through 1.x | one `/DreamShaderGenerated/<basename>_<hash>.ush` per source, holding every `Function`, which each node included |

A 1.x `Function` is read by the legacy front end as a `/// @custom` function, the 2.0 form of a Custom
node with a verbatim body — see [`/// @custom` bodies](../language-v2/index.md#-custom-bodies). A
[`GraphFunction`](../language/graph-function.md) is one too, with its `UE.*` calls lifted out into
graph nodes wired to extra inputs.

## Synopsis

```text
<node code>     := [ <wrapper> ] <body block>
<wrapper>       := struct <wrapper type> { <helper definition> … }; <wrapper type> <wrapper var>;
<wrapper type>  := generated_wrapper_ <SanitizeIdentifier(function)> _ <CRC32(function) as %08X>
<wrapper var>   := __ds_wrapper_ <CRC32(function) as %08X>
<helper symbol> := DreamShaderFn_ <SanitizeIdentifier(helper name)>
<body block>    := [ <first out> declared ] <begin marker> <custom marker> <body> <end marker> [ return … ]
```

`function` is the name of the `Function` the node is built for. The wrapper is written only when its
body calls another `Function`.

## Location

*(since 2.0.0)* There is no file to locate: the code lives in the node, inside the asset, and is
derived from the source the [build key](caching.md) hashes, so an unchanged source skips it like
everything else.

The virtual directory `/DreamShaderGenerated` is still mapped to the **Generated Shader Directory**
project setting (category `Paths`, default `Intermediate/DreamShader/GeneratedShaders`) when the module
starts, and *Tools ▸ DreamShader ▸ Clean Generated Shaders* still deletes every `*.ush` under it —
recursively, one file at a time, never the directory itself — and queues a full forced recompile. No
compile writes there any more; a file found there was written by a 1.x build. A material saved by a
1.x build may still have Custom nodes that include one; rebuilding the material replaces them with
nodes that carry their own code.

## Node code layout

```hlsl
struct generated_wrapper_<Name>_<HASH>             // only when the body calls other Functions
{
	<helper definition>

	<helper definition>

};
generated_wrapper_<Name>_<HASH> __ds_wrapper_<HASH>;

<T> <first out> = (<T>)0;                           // only when the first result is an `out` parameter
// Begin DreamShader source: <project-relative path of the file that declares the function>
// DreamShader custom: <Name> line <N>
<body, verbatim>
// End DreamShader source: <same path>
return <first out>;                                 // only when the body needs one, see below
```

| Element | Rule |
| :-- | :-- |
| body | the text between the braces, after the 1.x [body normalisation](../language/function.md#body-normalisation). It is never re-indented, and no rewrite adds or removes a line, so a shader-compile error maps back to its source line; `<N>` is the line of the body's opening brace |
| path | project-relative, so the code — and with it the shader key — is the same on every machine. A function declared in an imported `.dsh` names the header |
| first result | the declared return type, or the first `out` parameter of a function that has none (legacy rule L9): that one is the node's primary output and the other `out` parameters are additional outputs |
| final `return` | added when the body has no top-level `return` of its own or the function returns nothing: `return <first out>;`, else `return 0.0;`. A return type whose body never returns is [`DSH6257`](../diagnostics/DSH6xxx.md#dsh6257) |
| inputs | each `in` parameter is a node input; a texture input gets the engine's `<name>Sampler` beside it |

## Function name mangling

Every embedded helper is prefixed. Without it, a `Function Luminance(float3)` would redefine the
engine's own `Luminance` from `/Engine/Private/Common.ush` and fail shader compilation with
`redefinition of 'Luminance'`.

```text
DreamShaderFn_ + SanitizeIdentifier(<Function name>)
```

`SanitizeIdentifier` applies these rules, in order:

| # | Rule |
| :-- | :-- |
| 1 | every character outside `[A-Za-z0-9_]` becomes `_` |
| 2 | an empty result becomes the literal `DreamShaderSymbol` |
| 3 | an all-underscore result becomes the literal `DreamShaderSymbol` |
| 4 | a first character that is not `[A-Za-z_]` gets a `_` prepended |
| 5 | runs of consecutive underscores collapse to one |

| DSL name | Function name | Emitted symbol |
| :-- | :-- | :-- |
| `Luma` | `Luma` | `DreamShaderFn_Luma` |
| `Common::ApplyTint` | `Common_ApplyTint` | `DreamShaderFn_Common_ApplyTint` |
| `Common::Remap01` | `Common_Remap01` | `DreamShaderFn_Common_Remap01` |

A `Function` inside `Namespace(Name="Common")` is the function `Common_ApplyTint`, so a top-level
`Function Common_ApplyTint` is the same name declared twice:
[`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210).

## Per-function definition shape

An embedded helper, as a member of the wrapper:

```hlsl
	<RetType> DreamShaderFn_<Name>(<parameters>)
	{
		<out parameter> = (<T>)0;                   // every `out` parameter but the first result
		<T> <first out> = (<T>)0;                   // when the first result is an `out` parameter
// Begin DreamShader source: <path>
// DreamShader custom: <Name> line <N>
<body, verbatim>
// End DreamShader source: <path>
		return <first out>;                         // when the first result is an `out` parameter
	}
```

| Element | Rule |
| :-- | :-- |
| `RetType` | the declared return type, or the type of the first `out` parameter when there is none |
| parameters | every parameter in declaration order but that first `out`; an `out` one as `out <type> <name>` *(since 2.0.0; 1.x put the inputs first)* |
| sampler pairing | immediately after each texture-typed parameter, an extra `SamplerState <name>Sampler` |
| other `out` parameters | zero-initialized first: an HLSL `out` arrives uninitialized |
| body | as in the node's own code: verbatim, between the markers |

Texture-typed parameters, for sampler pairing, are these five tokens, compared case-insensitively:

| `Texture2D` | `TextureCube` | `Texture2DArray` | `Texture3D` | `VolumeTexture` |
| :-- | :-- | :-- | :-- | :-- |

`SamplerState` is **not** one of them; a parameter declared `SamplerState` gets no companion
argument. A texture parameter `T` beside a parameter named `TSampler` is
[`DSH6255`](../diagnostics/DSH6xxx.md#dsh6255).

A parameter is written with the 2.0 spelling of its declared type:

| Declared token | Emitted HLSL type |
| :-- | :-- |
| `vec2`…`vec4`, `ivec*`, `uvec*`, `bvec*`, `mat2`…`mat4` | `float2`…`float4`, `int*`, `uint*`, `bool*`, `float2x2`…`float4x4` |
| a scalar, vector or matrix type in another case (`Float3`) | lower case |
| `VolumeTexture` | `Texture3D` |
| everything else | as declared |

## Includes on the node

The `#include "…"` lines at the start of a body are blanked in place — overwritten with spaces, so
every position after them still matches the source — and added to the node's `IncludeFilePaths`
instead: the embedded helpers' first, in the order they are embedded, then the function's own, each
path once. An `#include` with an empty path is [`DSH6258`](../diagnostics/DSH6xxx.md#dsh6258). See
[Function ▸ Includes](../language/function.md#includes).

## Self-contained functions

A node embeds the **transitive closure** of the `Function`s its body calls: each callee once, a
dependency before its dependents. Recursion has no HLSL form; a cycle among them is
[`DSH6260`](../diagnostics/DSH6xxx.md#dsh6260).

| Element | Form |
| :-- | :-- |
| struct type | `generated_wrapper_<sanitized name>_<hash>` |
| instance variable | `__ds_wrapper_<hash>` |
| name | the `Function` the node is built for |
| hash | CRC32 of the **raw, unsanitized** name, uppercase hex |
| call sites in the node's own code | `__ds_wrapper_<hash>.DreamShaderFn_<Name>(…)` |
| call sites between embedded members | plain `DreamShaderFn_<Name>(…)`, no qualifier |
| member separation | one tab of indentation, one blank line between members |

A call site is rewritten by **name**: the argument list stays as written, except that a
`<texture>Sampler` argument is spliced in after each texture argument. That needs the call to pass
exactly the callee's arguments ([`DSH6262`](../diagnostics/DSH6xxx.md#dsh6262)) and each texture
argument to be a plain name ([`DSH6263`](../diagnostics/DSH6xxx.md#dsh6263)). A helper returns its
first result, so a body calls it as a value — `float r = Remap01(x);`. *(since 2.0.0)* The 1.x
reshaping of a call that passed that result as an argument (`Remap01(x, r);`) is gone.

A namespace-qualified call inside a body works *(since 2.0.0)*: the body normalisation flattens
`Common::Remap01(` to `Common_Remap01(`, which is the namespaced function's own name, so it is found
and embedded like any other call. Through 1.x it called an undefined `Common_Remap01` and the shader
failed to compile.

`SelfContained` (and its old spelling `Inline`, [`DSH6306`](../diagnostics/DSH6xxx.md#dsh6306)) asked
1.x for the closure to be embedded; every node embeds it now without being asked, so the modifier
changes nothing and the function is a plain `/// @custom`. *(2.0.0 – 2.1.0 read it as
`/// @custom selfcontained`, which embeds nothing: a `Function` it called was left for the shader
compiler, [`DSH6264`](../diagnostics/DSH6xxx.md#dsh6264).)*

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH4210` | two `Function`s with the same name in the file and the headers it imports — including a namespaced one and a top-level one that flatten to the same name |
| `DSH6260` | the `Function`s a node embeds call each other in a cycle |
| [`DSH6259`](../diagnostics/DSH6xxx.md#dsh6259) | *(warning)* a body calls a name that differs from a `Function` only in case; the call is left as written |
| [`DSH6261`](../diagnostics/DSH6xxx.md#dsh6261) | a body calls a function that is not a `Function` or `GraphFunction`, such as a `ShaderFunction` |
| [`DSH6327`](../diagnostics/DSH6xxx.md#dsh6327) | a body calls a `GraphFunction` whose `UE.*` calls were lifted into inputs of its own node |
| `DSH6262`, `DSH6263` | a call that passes a texture cannot be matched up with the callee's parameters |
| `DSH6258` | an `#include` line with an empty path |

A call in a body to a name that is no function of the file is left for the shader compiler.

## Example

```c
// DShader/M_Ramp.dsm
Namespace(Name="Common")
{
    Function float3 ApplyTint(in float3 color, in float3 tint) { return color * tint; }
}

Function SelfContained Remap01(in float value, out float result) {
    result = saturate(value * 0.5 + 0.5);
}

Shader(Name="Materials/M_Ramp")
{
    Properties { ScalarParameter Input = 0.25; vec3 Tint = vec3(1.0, 0.4, 0.1); }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }
    Graph {
        float R;
        Remap01(Input, R);
        Color = Common::ApplyTint(vec3(R, R, R), Tint);
    }
}
```

The `Remap01` call becomes a Custom node with input `value` and primary output `result`:

```hlsl
float result = (float)0;
// Begin DreamShader source: DShader/M_Ramp.dsm
// DreamShader custom: Remap01 line 7

    result = saturate(value * 0.5 + 0.5);
// End DreamShader source: DShader/M_Ramp.dsm
return result;
```

The `Common::ApplyTint` call becomes a Custom node with inputs `color` and `tint`; its body returns:

```hlsl
// Begin DreamShader source: DShader/M_Ramp.dsm
// DreamShader custom: Common_ApplyTint line 4
 return color * tint; 
// End DreamShader source: DShader/M_Ramp.dsm
```

Neither body calls another `Function`, so neither node has a wrapper and neither includes anything.
A function that does call one carries it (hash values are illustrative):

```c
Function float Luma(in float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }
Function float3 Desaturate(in float3 c, in float amount) { float l = Luma(c); return lerp(c, float3(l, l, l), amount); }
```

```hlsl
struct generated_wrapper_Desaturate_1A2B3C4D
{
	float DreamShaderFn_Luma(float3 c)
	{
// Begin DreamShader source: DShader/M_Gray.dsm
// DreamShader custom: Luma line 1
 return dot(c, float3(0.299, 0.587, 0.114)); 
// End DreamShader source: DShader/M_Gray.dsm
	}

};
generated_wrapper_Desaturate_1A2B3C4D __ds_wrapper_1A2B3C4D;

// Begin DreamShader source: DShader/M_Gray.dsm
// DreamShader custom: Desaturate line 2
 float l = __ds_wrapper_1A2B3C4D.DreamShaderFn_Luma(c); return lerp(c, float3(l, l, l), amount); 
// End DreamShader source: DShader/M_Gray.dsm
```

## See also

- [Function](../language/function.md) — the block that produces this code
- [GraphFunction](../language/graph-function.md) — a Custom node whose `UE.*` calls are lifted into the graph
- [Namespace](../language/namespace.md) — how a qualified name is flattened
- [`/// @custom` bodies](../language-v2/index.md#-custom-bodies) — the 2.0 form every `Function` becomes
- [HLSL library](../builtins/hlsl-library.md) — `Shaders/DreamShaderBuiltins.ush`, the hand-written companion
- [Caching](caching.md) — the build key that covers this code
- [Project settings](../settings/project.md) — **Generated Shader Directory**
- [Ephemeral materials](ephemeral.md) — *Clean Generated Shaders* and the other maintenance actions
- [Generation](index.md) — where the code is built in the pipeline
- [Diagnostics index](../diagnostics/index.md) — every code
