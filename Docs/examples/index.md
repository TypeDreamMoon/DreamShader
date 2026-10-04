# Examples

> [DreamShader](../index.md) » **Examples**

Fifteen complete, self-contained DreamShaderLang 1.x sources, ordered from the smallest possible
material to the constructs that need a `.dsh` header, a `.dsf` function file, or a specific engine
version — each followed by the same thing written as a `.dss`.

| | |
| :-- | :-- |
| Applies to | DreamShaderLang 1.x (`.dsm`, `.dsf`, `.dsh`), built by the 2.0 compiler through its legacy front end *(since 2.0.0)*; the `.dss` forms are [DreamShaderLang 2.0](../language-v2/index.md) |
| Engines | UE `5.3` – `5.8`; anything version-gated is marked inline |
| Assumed source root | `<Project>/DShader` — the `SourceDirectory` [project setting](../settings/project.md) |
| Generated output | a `Graph`-backend material and every material function, layer and blend: saved on each successful build. A `ThinCustom` material — the default backend — stays Ephemeral, in memory, until it is materialized; see [Ephemeral materials](../generation/ephemeral.md) |

Every snippet below is a whole file. Paths in the leading comment are the on-disk location the
example assumes; `import` and `#include` paths resolve against that layout. Assets referenced with
`Path(Engine, …)` or `/Engine/…` ship with the engine, so those examples load as written; the two
that reference a project asset no example builds ([11](#11-virtualfunction-wrapping-an-existing-asset),
[15](#15-runtime-virtual-texture-sampling-and-writing)) are marked.

Diagnostics in the notes are cited by code. A compile reports each one as
`<file>(<line>,<col>): DSHnnnn: <message>`, at the line and column of the construct it is about;
the code's page has the message, its cause and the fix. The code is the contract, the wording is not.
A message of the 1.x generator found in an old log is mapped on
[Diagnostics](../diagnostics/index.md#1x-messages-and-their-codes).

*The same in a `.dss`*, under each example, is that file written by hand in DreamShaderLang 2.0:
`#pragma material(…)` for `Settings`, a `uniform` with `///` tags for each parameter, and one
`export` function whose name is the asset's. A `.dss` builds its export in the file's own folder, so
each one here builds the same asset path as the 1.x file it stands beside — keep one or the other,
not both. [`dsc migrate`](../tools/migrate.md) writes the `.dss` of a 1.x file and proves it builds
the same graph; its output is longer than these, because it keeps every 1.x detail (`/// @sort 32`
on each parameter, the zero initialisers 1.x gave each output).

## Contents

| # | Example | Exercises |
| :-- | :-- | :-- |
| [1](#1-minimal-unlit-material) | Minimal unlit material | [`Shader`](../language/shader.md), [output bindings](../language/output-bindings.md) |
| [2](#2-parameters-scalar-vector-static-switch-texture) | Parameters, metadata, group scopes | [`Properties`](../language/properties.md), [metadata](../parameters/metadata.md) |
| [3](#3-a-shared-dsh-header-namespace--function) | A shared `.dsh` header | [`Namespace`](../language/namespace.md), [`import`](../language/import.md) |
| [4](#4-calling-functions-value-form-and-statement-form) | Calling functions | [Graph calls](../graph/calls.md) |
| [5](#5-ue-nodes-and-a-generic-ueexpression) | `UE.*` and `UE.Expression` | [`UE.*`](../builtins/ue.md), [`UE.Expression`](../builtins/ue-expression.md) |
| [6](#6-if--else-in-graph) | `if` / `else` | [`if`](../graph/if.md) |
| [7](#7-materialattributes-output-binding) | `MaterialAttributes` | [MaterialAttributes](../graph/material-attributes.md) |
| [8](#8-substrate-material-ue-54) | Substrate material *(UE 5.4+)* | [`Substrate.*`](../builtins/substrate.md) |
| [9](#9-shaderfunction-in-a-dsf-called-from-a-material) | `ShaderFunction` in a `.dsf` | [`ShaderFunction`](../language/shader-function.md), [`VirtualFunction`](../language/virtual-function.md) |
| [10](#10-shaderlayer--shaderlayerblend) | `ShaderLayer` / `ShaderLayerBlend` | [Layers](../language/shader-layer.md) |
| [11](#11-virtualfunction-wrapping-an-existing-asset) | `VirtualFunction` | [`VirtualFunction`](../language/virtual-function.md) |
| [12](#12-graphfunction-hoisting-a-ue-call-into-a-custom-node) | `GraphFunction` hoisting | [`GraphFunction`](../language/graph-function.md) |
| [13](#13-settings-domain-shading-model-blend-mode-backend) | `Settings` and `Backend` | [Shader settings](../settings/material.md), [Backend](../settings/backend.md) |
| [14](#14-layout-placement-and-region) | `Layout` and `#Region` | [`Layout`](../language/layout.md) |
| [15](#15-runtime-virtual-texture-sampling-and-writing) | Runtime Virtual Texture | [`UE.Expression`](../builtins/ue-expression.md), [output bindings](../language/output-bindings.md) |

Custom Pass pipelines (`.dsp`, UE 5.8) have a page of their own: [Custom Pass examples](custom-pass.md).

---

## 1. Minimal unlit material

The smallest file that produces an asset: one parameter, one binding, one assignment.

```c
// DShader/Materials/M_Minimal.dsm
Shader(Name="Materials/M_Minimal", Root="Game")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.2, 0.2);
    }

    Settings = {
        Domain       = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = Tint;
    }
}
```

Generated asset:

```text
package     /Game/Materials/M_Minimal
object path /Game/Materials/M_Minimal.M_Minimal
class       UDreamShaderMaterialInstance over a hidden base UMaterial (the default ThinCustom backend)
```

- `Shader` is matched **case-sensitively**; `Properties`, `Settings`, `Outputs`, `Graph` are not.
  See [Lexical elements](../language/lexical.md#case-sensitivity).
- The `=` after a section name is optional sugar *(since 1.5.0)*: `Properties { … }` parses
  identically. The final `;` in a section body is optional too.
- `Root` defaults to `/Game`, so `Root="Game"` above is redundant. Full rules:
  [Asset paths](../generation/asset-paths.md).
- A `Shader` with no `Graph` section and nothing in its `Outputs` is
  [`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255). Without a `Graph`, the `Outputs` compute the values
  themselves — an output declaration with an initializer, or *(since 2.0.0)* a binding whose right
  side is an expression. A `Shader` with no `Outputs` section builds with nothing wired to it, and
  says so with the warning [`DSH2256`](../diagnostics/DSH2xxx.md#dsh2256) *(since 2.0.0)*.

### The same in a `.dss`

```hlsl
// DShader/Materials/M_Minimal.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

uniform float3 Tint = float3(1.0, 0.2, 0.2);

export void M_Minimal(inout material m)
{
    m.EmissiveColor = Tint;
}
```

The signature `export void <Name>(inout material m)` is what makes the file a material; a 1.x
`Outputs` binding is a write to a member of `m`.

*See also:* [`Shader`](../language/shader.md) · [Output bindings](../language/output-bindings.md) ·
[Material enums](../settings/material-enums.md)

---

## 2. Parameters: scalar, vector, static switch, texture

Every parameter family, with a `Group("…")` scope, `[ … ]` metadata and the `Slider(min, max)`
shorthand.

```c
// DShader/Materials/M_Params.dsm
Shader(Name="Materials/M_Params")
{
    Properties = {
        Group("Surface") {
            ScalarParameter Roughness = 0.55 [Slider(0, 1)];
            VectorParameter Albedo    = float4(0.8, 0.8, 0.8, 1.0) [Description="Base albedo"];
        }

        Group("Detail") {
            TextureSampleParameter2D DetailMap = Path(Engine, "EngineResources/WhiteSquareTexture") [
                SamplerType   = "LinearColor";
                SamplerSource = "FromTextureAsset";
                SortPriority  = 99;
            ];
            StaticSwitchParameter UseDetail = true;
        }

        Texture2D   NoiseTex   = Path(Engine, "EngineResources/WhiteSquareTexture");
        const float DebugScale = 1.0;
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        float3 Color;
        float  Rough;

        Base.BaseColor = Color;
        Base.Roughness = Rough;
    }

    Graph = {
        vec2 UV     = UE.TexCoord(Index = 0);
        vec4 Detail = DetailMap(Coordinates = UV);
        vec4 Noise  = SampleTexture2D(NoiseTex, UV);

        Color = UseDetail(True = Detail.rgb * Albedo.rgb, False = Albedo.rgb);
        Rough = Roughness * DebugScale * Noise.r;
    }
}
```

- `float`/`vec3`/`Texture2D` are the **compact** spellings; `ScalarParameter`/`VectorParameter`/
  `TextureSampleParameter2D` name the Unreal node explicitly. Both are documented in
  [Compact types](../parameters/compact-types.md) and [Parameter nodes](../parameters/parameter-nodes.md).
- A `Group("…") { … }` scope stamps its name onto the parameters inside it and numbers them
  `0, 10, 20, …` from **one counter per `Properties` section**; an explicit `SortPriority` wins and
  takes no number. Here `Roughness` gets 0 and `Albedo` 10. Nested groups compose with `|`
  (`Outer|Inner`).
- *(since 2.0.0)* The stamp reaches the scalar, vector, texture and static-bool parameters. A
  pin-bearing or switch parameter — `DetailMap`, `UseDetail` — is written out as a node at each use,
  and that node carries only the metadata in its own `[ … ]`: `DetailMap` gets `SortPriority = 99`
  and no group, `UseDetail` neither.
- `const` declares a value nobody can override — a compile-time constant, not a parameter. It takes a
  scalar, vector or texture type; any other is [`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253).

> [!NOTE]
> The call form `Name(Pin = …)` only works for the ten parameter tokens whose node owns input pins —
> `ChannelMaskParameter`, `StaticComponentMaskParameter` and the eight `*SampleParameter*` tokens —
> and every argument must be named ([`DSH5259`](../diagnostics/DSH5xxx.md#dsh5259)). A **texture
> object** parameter (`Texture2D NoiseTex`, `TextureObjectParameter`) has no pins and is no function:
> `NoiseTex(Coordinates = UV)` is [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208). Sample it with the
> reserved `SampleTexture2D(textureObject, uv)` form instead, as above. `SampleTexture2D` is matched
> **case-sensitively** and takes exactly two positional arguments
> ([`DSH5256`](../diagnostics/DSH5xxx.md#dsh5256) otherwise).

> [!WARNING]
> A bare read of a `StaticSwitchParameter` is not a value: `Color = UseDetail;` is
> [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200). Call it with `True=` and `False=` (or `A=`/`B=`, or
> positionally); a call without both branches is [`DSH5258`](../diagnostics/DSH5xxx.md#dsh5258), and
> any other argument is dropped with the warning [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254).

### The same in a `.dss`

```hlsl
// DShader/Materials/M_Params.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

/// @group Surface   @sort 0    @slider 0 1
uniform float Roughness = 0.55;
/// @group Surface   @sort 10   @desc Base albedo
uniform float4 Albedo = float4(0.8, 0.8, 0.8, 1.0);

/// @group Detail   @sort 99   @sampler LinearColor
/// @default /Engine/EngineResources/WhiteSquareTexture
uniform Texture2D DetailMap;
/// @static
/// @group Detail   @sort 20
uniform bool UseDetail = true;

/// @default /Engine/EngineResources/WhiteSquareTexture
uniform Texture2D NoiseTex;
static const float DebugScale = 1.0;

export void M_Params(inout material m)
{
    float2 UV = UE.TexCoord(Index = 0);
    float4 Detail = DetailMap.Sample(UV);
    float4 Noise = NoiseTex.Sample(UV);

    float3 Color = Albedo.rgb;
    if (UseDetail)
    {
        Color = Detail.rgb * Albedo.rgb;
    }
    m.BaseColor = Color;
    m.Roughness = Roughness * DebugScale * Noise.r;
}
```

- A texture `uniform` takes no initializer: its asset is `/// @default`, its sampler type
  `/// @sampler`. Sampled with `.Sample(UV)` it builds a `TextureObjectParameter` and a
  `TextureSample`, where the 1.x token built one `TextureSampleParameter2D`; the sampler source is the
  node's default, *From Texture Asset*.
- A `/// @static` `uniform bool` read by an `if` is the static switch. `/// @group` and `/// @sort`
  write each parameter's group and priority; `static const` is the 1.x `const`.

*See also:* [`Properties`](../language/properties.md) · [Metadata](../parameters/metadata.md) ·
[`SamplerType`](../parameters/sampler-type.md) · [`Path(...)`](../parameters/path.md) ·
[Reading parameters in `Graph`](../parameters/graph-usage.md)

---

## 3. A shared `.dsh` header: `Namespace` + `Function`

A `.dsh` header holds `Function`, `GraphFunction`, `Namespace` and `VirtualFunction` blocks,
`import` lines, and 2.0 declarations — no asset block ([`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249)).
It generates no asset. It is parsed on its own, and its declarations become names of every file that
imports it *(since 2.0.0; 1.x pasted its text into the importing file)*.

```c
// DShader/Shared/Common.dsh
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }

    Function float Luma(in vec3 color) {
        return dot(color, float3(0.299, 0.587, 0.114));
    }
}

Function SelfContained Remap01(in float value, out float result) {
    result = saturate(value * 0.5 + 0.5);
}

Function SplitChannels(in vec4 src, out vec3 rgb, out float alpha) {
    rgb   = src.rgb;
    alpha = src.a;
}
```

Importing it:

```c
// DShader/Materials/M_Tinted.dsm
import "Shared/Common.dsh";

Shader(Name="Materials/M_Tinted")
{
    Properties = {
        vec3 Albedo = vec3(0.6, 0.8, 1.0);
        vec3 Tint   = vec3(1.0, 0.4, 0.1);
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Common::ApplyTint(Albedo, Tint, Tinted);
        Color = Tinted;
    }
}
```

- `import "Shared/Common"` is equivalent: when the specifier carries **no extension at all**, `.dsh`
  is appended. Only a `.dsh` can be imported *(since 2.0.0)*: a `.dsf` or `.dsm` path is
  [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252).
- A relative specifier is tried against the importing file's own directory, then its source root's
  directory (`DShader`), then that root's `Packages` — never another root's. A candidate that
  resolves outside its root is skipped. See [`import`](../language/import.md#resolution).
- *(since 2.0.0)* `import` is read as a token at file scope: the `;` is optional, `'single quotes'`
  work in a 1.x file, and an `import` inside a `/* … */` comment is commented out (1.x processed it).
  Keep each `import` on a line of its own all the same: the editor's dependency scanner, which
  recompiles the importers when a header is saved, still reads them line by line.
- `Function` bodies are HLSL, not `Graph` statements: `dot`, `saturate` and `float3(…)` here are
  HLSL intrinsics. GLSL spellings are rewritten inside those bodies, case-insensitively and without a
  diagnostic (`vec3`→`float3`, `mix`→`lerp`, `fract`→`frac`, `mod`→`fmod`).
- *(since 2.0.0)* `SelfContained` means the body embeds no other `Function`: a call it makes is left
  to the shader compiler, with the warning [`DSH6264`](../diagnostics/DSH6xxx.md#dsh6264), while a
  plain `Function` embeds every `Function` it calls (1.x embedded them only in a `SelfContained`
  body). `Remap01` calls none, so the modifier changes nothing here. `Inline` is its old spelling,
  with the warning [`DSH6306`](../diagnostics/DSH6xxx.md#dsh6306); on a `GraphFunction` either one is
  [`DSH6307`](../diagnostics/DSH6xxx.md#dsh6307).
- *(since 2.0.0)* A namespace-qualified call inside another `Function` or `GraphFunction` body works:
  body normalisation flattens `Common::ApplyTint` to `Common_ApplyTint`, which is the function's own
  name, and the callee is embedded in the caller's node. 1.x left an undefined symbol for the shader
  compiler. Write the `::` without spaces around it.

### The same in a `.dss`

```hlsl
// DShader/Materials/M_Tinted.dss
#include "Shared/Common.dsh"

#pragma material(Domain = Surface, ShadingModel = Unlit)

uniform float3 Albedo = float3(0.6, 0.8, 1.0);
uniform float3 Tint   = float3(1.0, 0.4, 0.1);

export void M_Tinted(inout material m)
{
    float3 Tinted;
    Common_ApplyTint(Albedo, Tint, Tinted);
    m.EmissiveColor = Tinted;
}
```

A `.dsh` holds both dialects, so the `.dss` includes the header as it is. A namespaced function is
called by the name it is declared under, `Common_ApplyTint`, and an `out` argument is a variable
declared first. The header itself in 2.0 spelling — a `.dsm` imports this one too:

```hlsl
// DShader/Shared/Common.dsh
/// @custom
/// @name Common::ApplyTint
void Common_ApplyTint(float3 color, float3 tint, out float3 result)
{
    result = color * tint;
}

/// @custom
/// @name Common::Luma
float Common_Luma(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

/// @custom selfcontained
void Remap01(float value, out float result)
{
    result = saturate(value * 0.5 + 0.5);
}

/// @custom
void SplitChannels(float4 src, out float3 rgb, out float alpha)
{
    rgb   = src.rgb;
    alpha = src.a;
}
```

`/// @custom` keeps the body verbatim and makes each call one Custom node, as a 1.x `Function` does;
`/// @name` keeps the node's title `Common::ApplyTint`.

*See also:* [`import`](../language/import.md) · [`Namespace`](../language/namespace.md) ·
[`Function`](../language/function.md) · [Source files](../language/source-files.md) ·
[Packages](../tools/packages.md)

---

## 4. Calling functions: value form and statement form

A function's value is its return value, or its first `out`. The statement form passes the inputs,
then one target per output; its trailing arguments name the variables that receive the results.

```c
// DShader/Materials/M_Calls.dsm
import "Shared/Common.dsh";

Shader(Name="Materials/M_Calls")
{
    Properties = {
        vec4 Source = vec4(0.4, 0.6, 0.9, 0.75);
        vec3 Tint   = vec3(1.0, 0.4, 0.1);
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Translucent"; }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        // statement form: two out results, two target names
        SplitChannels(Source, Rgb, A);

        // statement form: one out result
        Common::ApplyTint(Rgb, Tint, Tinted);

        // value form: single-output functions
        float L    = Common::Luma(Tinted);
        float Soft = Remap01(L);

        Color = Tinted * Soft;
        Alpha = A;
    }
}
```

| Callee kind | Value form | Statement form | Named arguments |
| :-- | :-- | :-- | :-- |
| `Function` | yes — the return value, or the first `out` *(since 2.0.0; 1.x: one result only)* | yes | yes *(since 2.0.0)* |
| `GraphFunction` | as `Function` | yes | yes *(since 2.0.0)* |
| `ShaderFunction` | yes — the first output, unless a selector picks another; callable only from its own file | yes | yes |
| `ShaderLayer` / `ShaderLayerBlend` | **not callable**: [`DSH6208`](../diagnostics/DSH6xxx.md#dsh6208) *(since 2.0.0)* | — | — |
| `VirtualFunction` | yes | yes | yes |

- An out target nobody declared is declared by the call, as a local of the output's type (info
  [`DSH5283`](../diagnostics/DSH5xxx.md#dsh5283)). A target has to be assignable — a variable, or a
  member or swizzle of one ([`DSH4239`](../diagnostics/DSH4xxx.md#dsh4239)) — of the output's type
  ([`DSH4218`](../diagnostics/DSH4xxx.md#dsh4218)).
- In an all-positional statement call the last arguments receive the outputs, one each, in
  declaration order; the ones before them are inputs. Too many arguments are
  [`DSH4224`](../diagnostics/DSH4xxx.md#dsh4224), and an input left out that is not `opt` is
  [`DSH4217`](../diagnostics/DSH4xxx.md#dsh4217).
- Function names are **case-sensitive** *(since 2.0.0)*: a call that matches a function only in case
  still resolves, with the warning [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275). There is **no
  overload resolution** — one name declares one thing, and a second declaration of it is
  [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210) where it is written.
- The math builtins (`lerp`, `dot`, `pow`, `min`, `max`, `clamp`, `saturate`, `sin`, `frac`, `fmod`,
  …) are resolved before user functions; a function named like one is
  [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206) at its declaration. `mix`, `fract` and `mod` are
  their GLSL spellings, accepted in a 1.x `Graph` with the warning
  [`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277).

### The same in a `.dss`

```hlsl
// DShader/Materials/M_Calls.dss
#include "Shared/Common.dsh"

#pragma material(Domain = Surface, ShadingModel = Unlit, BlendMode = Translucent)

uniform float4 Source = float4(0.4, 0.6, 0.9, 0.75);
uniform float3 Tint   = float3(1.0, 0.4, 0.1);

export void M_Calls(inout material m)
{
    float3 Rgb;
    float  A;
    SplitChannels(Source, Rgb, A);

    float3 Tinted;
    Common_ApplyTint(Rgb, Tint, Tinted);

    float L = Common_Luma(Tinted);
    float Soft;
    Remap01(L, Soft);

    m.EmissiveColor = Tinted * Soft;
    m.Opacity = A;
}
```

A `.dss` passes every `out` of a call to a variable it declared first. The 1.x value call of a
function without a return type, `Remap01(L)`, is a legacy rule (L3b); `dsc migrate` writes it as the
statement above — see [what the rewrite does](../tools/migrate.md#what-the-rewrite-does).

*See also:* [Graph calls](../graph/calls.md) · [Name resolution](../graph/name-resolution.md) ·
[Math builtins](../builtins/math.md) · [Statements](../graph/statements.md)

---

## 5. `UE.*` nodes and a generic `UE.Expression(...)`

`UE.<Name>(…)` creates one `UMaterialExpression`. `<Name>` is a class of the builtin catalog — any
engine expression class by its short name (`Sine`, `CameraVectorWS`), or one of the 1.x aliases the
catalog carries (`TexCoord`, `TransformVector`, …). `UE.Expression(Class = …)` names the class in an
argument instead; the two are one path *(since 2.0.0)*.

```c
// DShader/Materials/M_UEBuiltins.dsm
Shader(Name="Materials/M_UEBuiltins")
{
    Settings = {
        Domain       = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float2 uv    = UE.TexCoord(Index = 0);
        float  t     = UE.Time();
        float3 view  = UE.CameraVectorWS();

        // Generic form: any MaterialExpression class, by name.
        float pulse  = UE.Expression(Class="Sine", OutputType="float1", Input=t);

        // Class= may be omitted when the function name IS the class name.
        float pulse2 = UE.Sine(OutputType="float1", Input=t * 0.5);

        // Reflected literal properties are written by name.
        float3 local = UE.TransformVector(Input = view, Source = "World", Destination = "Local");

        Color = float3(uv.x, uv.y, pulse + pulse2 + local.z);
    }
}
```

- `Class="Sine"`, `"MaterialExpressionSine"`, `"UMaterialExpressionSine"` *(since 2.0.0)* and
  `"/Script/Engine.MaterialExpressionSine"` all resolve to the same class. Quotes are optional:
  `Class=Sine` and `OutputType=float1` work identically.
- An argument names an input pin of the class first, then one of its properties; the pin wins on a
  tie. A pin takes an expression; a property takes a literal
  ([`DSH4373`](../diagnostics/DSH4xxx.md#dsh4373) for a value computed at run time). What happens to
  a name that is neither is on [`UE.Expression`](../builtins/ue-expression.md#argument-dispatch).
- `UE.Expression(…)` without `Class=` is [`DSH5218`](../diagnostics/DSH5xxx.md#dsh5218).
- `OutputType` (alias `ResultType`) is no longer required *(since 2.0.0)*: the legacy front end takes
  it off the call, and the catalog types the node. It still sets the output type of a `Custom` node;
  elsewhere a numeric token only gives the width of a one-output node's value, and anything else is
  dropped. See [`OutputType`](../builtins/output-type.md).

> [!WARNING]
> On the 27 1.x names of the [`UE.*` catalogue](../builtins/ue.md#catalogue) — `TexCoord`, `Time`,
> `CameraVectorWS`, `TransformVector`, … — the legacy front end keeps the arguments 1.x read and
> drops every other one, an unknown name or a positional argument, with the warning
> [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254) *(since 2.0.0; 1.x dropped it without a word)*.
> `UE.Time(Bogus = 1)` and `UE.TexCoord(0)` still build, and say why. The only positional argument
> kept is the first of `UE.TransformVector` / `UE.TransformPosition` (→ `Input`). On any other node a
> positional argument binds only where the catalog gives the class an argument order, and is
> [`DSH5220`](../diagnostics/DSH5xxx.md#dsh5220) elsewhere.

### The same in a `.dss`

```hlsl
// DShader/Materials/M_UEBuiltins.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

export void M_UEBuiltins(inout material m)
{
    float2 uv   = UE.TexCoord(Index = 0);
    float  t    = UE.Time();
    float3 view = UE.CameraVectorWS();

    float  pulse  = UE.Expression(Class = "Sine", Input = t);
    float  pulse2 = UE.Sine(Input = t * 0.5);
    float3 local  = UE.TransformVector(Input = view, Source = "World", Destination = "Local");

    m.EmissiveColor = float3(uv.x, uv.y, pulse + pulse2 + local.z);
}
```

A `.dss` gets no legacy filter: an argument the node does not have is
[`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213), not a warning.

*See also:* [`UE.*` catalogue](../builtins/ue.md) · [`UE.Expression`](../builtins/ue-expression.md) ·
[`OutputType`](../builtins/output-type.md) · [Transform bases](../builtins/transform.md)

---

## 6. `if` / `else` in `Graph`

The condition must be parenthesised and each body must be braced. Both branches are built into the
graph; one conditional node per changed variable — a `UMaterialExpressionIf`, or a `StaticSwitch`
on a static bool — selects between them at run time.

```c
// DShader/Materials/M_Branch.dsm
Shader(Name="Materials/M_Branch")
{
    Properties = {
        ScalarParameter Threshold = 0.5  [Group="Surface"];
        VectorParameter Lit       = float4(1.0, 0.85, 0.2, 1.0) [Group="Surface"];
        VectorParameter Dark      = float4(0.05, 0.05, 0.1, 1.0) [Group="Surface"];
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "Unlit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float2 uv   = UE.TexCoord(Index = 0);
        float  mask = uv.x;

        if (mask > Threshold) {
            Color = Lit.rgb;
        } else if (mask > Threshold * 0.5) {
            Color = Lit.rgb * 0.5;
        } else {
            Color = Dark.rgb;
        }
    }
}
```

| Condition | Meaning |
| :-- | :-- |
| `a > b`, `a < b`, `a >= b`, `a <= b`, `a == b`, `a != b` | the six comparison operators |
| `if (x)` | truthy — identical to `x != 0` |

- The condition compares two numbers: a comparison of two vectors is
  [`DSH4260`](../diagnostics/DSH4xxx.md#dsh4260), an operand that is not a number
  [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226).
- A variable declared before the `if` and changed in one branch only is merged with the value it had
  before the `if`. A variable that had no value before it and is assigned in one branch only is
  [`DSH4372`](../diagnostics/DSH4xxx.md#dsh4372) where it is read after the `if`.
- A variable *declared* inside a branch is local to that branch *(since 2.0.0)*: it is not merged, and
  its name is unknown after the `if`.
- A value is converted to its variable's declared type at every assignment, so the two branch values
  of one variable always agree.
- Reading a parameter inside a branch is fine — parameters are never branch outputs.
- `if` is matched **case-sensitively**: `If (x) { … }` is a call to a function named `If`, and the `{`
  after it is [`DSH2154`](../diagnostics/DSH2xxx.md#dsh2154).

> [!WARNING]
> `&&` and `||` are refused in a 1.x condition *(since 2.0.0)*: [`DSH2210`](../diagnostics/DSH2xxx.md#dsh2210).
> 1.x dropped everything after the first comparison without a word, so `if (a > 0 && b > 0)` tested
> `a > 0`. Nest two `if` statements instead. The other constructs 1.x truncated — `%`, `?:`, `&`, `|`,
> `^`, `<<`, `v[i]` — are errors anywhere in an expression
> ([`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200)–[`DSH2202`](../diagnostics/DSH2xxx.md#dsh2202)); see
> [Unsupported constructs](../graph/unsupported.md#operators-1x-truncated).

### The same in a `.dss`

```hlsl
// DShader/Materials/M_Branch.dss
#pragma material(Domain = Surface, ShadingModel = Unlit, BlendMode = Opaque)

/// @group Surface
uniform float Threshold = 0.5;
/// @group Surface
uniform float4 Lit = float4(1.0, 0.85, 0.2, 1.0);
/// @group Surface
uniform float4 Dark = float4(0.05, 0.05, 0.1, 1.0);

export void M_Branch(inout material m)
{
    float2 uv = UE.TexCoord(Index = 0);
    float mask = uv.x;

    float3 Color = float3(0, 0, 0);
    if (mask > Threshold)
    {
        Color = Lit.rgb;
    }
    else if (mask > Threshold * 0.5)
    {
        Color = Lit.rgb * 0.5;
    }
    else
    {
        Color = Dark.rgb;
    }
    m.EmissiveColor = Color;
}
```

The variable the branches assign is declared before the `if`, as a 1.x `Outputs` declaration is.

*See also:* [`if` / `else`](../graph/if.md) · [Expressions](../graph/expressions.md) ·
[Conversions](../graph/conversions.md)

---

## 7. `MaterialAttributes` output binding

Binding `Base.MaterialAttributes` turns on *Use Material Attributes* on the generated material.
`MaterialAttributes Attrs;` is an empty attribute set — one `MakeMaterialAttributes` node — and its
members are written by name; where the value is used, the writes become one
`SetMaterialAttributes` node *(since 2.0.0; 1.x chained one per write)*.

```c
// DShader/Materials/M_Attrs.dsm
Shader(Name="Materials/M_Attrs")
{
    Properties = {
        vec3  BaseTint = vec3(0.6, 0.8, 1.0);
        float R        = 0.35;
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        Base.MaterialAttributes = Attrs;
    }

    Graph = {
        MaterialAttributes Attrs;

        Attrs.BaseColor = BaseTint;
        Attrs.Roughness = R;
        Attrs.Metallic  = 0.0;

        // A member already written reads back as the value written; no node is made.
        float Echo = Attrs.Roughness;
        Attrs.Specular = Echo;
    }
}
```

> [!NOTE]
> The `Outputs` bindings are applied after the `Graph`, so the binding takes the `Attrs` declared in
> the `Graph`, as above. *(since 2.0.0)* An `Outputs` declaration `MaterialAttributes Attrs;` is that
> variable too, starting empty — see [MaterialAttributes](../graph/material-attributes.md#creating-a-value).
> `MaterialAttributes` is a type of `Outputs`, `Inputs`, `Graph` variables and function results; as a
> `Properties` type it is [`DSH3252`](../diagnostics/DSH3xxx.md#dsh3252), and as an input of a
> `Function` [`DSH6210`](../diagnostics/DSH6xxx.md#dsh6210).

- Member names are the material property names (`BaseColor`, `Metallic`, `Specular`, `Roughness`,
  `EmissiveColor`, `Opacity`, `Normal`, …) of the running engine; one it does not have is
  [`DSH5200`](../diagnostics/DSH5xxx.md#dsh5200).
- An arithmetic operator on a `MaterialAttributes` value is [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226).
- `Base.MaterialAttributes` and `Base.FrontMaterial` in one `Shader` are no longer refused *(since
  2.0.0)*: both are wired, and *Use Material Attributes* makes the material read the attribute set.

### The same in a `.dss`

```hlsl
// DShader/Materials/M_Attrs.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

uniform float3 BaseTint = float3(0.6, 0.8, 1.0);
uniform float  R        = 0.35;

export void M_Attrs(inout material m)
{
    m.BaseColor = BaseTint;
    m.Roughness = R;
    m.Metallic  = 0.0;
    m.Specular  = m.Roughness;
}
```

The entry writes the members of `m`, which go straight to the material's pins: no
`MakeMaterialAttributes` node is made. A member is read back only after it is written
([`DSH4370`](../diagnostics/DSH4xxx.md#dsh4370) otherwise).

*See also:* [MaterialAttributes](../graph/material-attributes.md) ·
[Output bindings](../language/output-bindings.md) · [Types](../language/types.md)

---

## 8. Substrate material *(UE 5.4+)*

`Substrate.*` builds Substrate nodes; `Base.FrontMaterial` is the binding that consumes them.

```c
// DShader/Materials/M_Substrate.dsm
Shader(Name="Materials/M_Substrate")
{
    Properties = {
        vec3 Color = vec3(0.1, 0.6, 1.0);
    }

    Outputs = {
        Substrate Surface;
        Base.FrontMaterial = Surface;
    }

    Graph = {
        Surface = Substrate.Unlit(EmissiveColor = Color);
    }
}
```

- Substrate needs **UE 5.4 or newer**: the catalog lists the Substrate classes of the running engine,
  and an older engine has none. There a `Substrate.*` call is
  [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210) and `Base.FrontMaterial`
  [`DSH5200`](../diagnostics/DSH5xxx.md#dsh5200); `Substrate.Select` needs UE 5.6
  ([`DSH5294`](../diagnostics/DSH5xxx.md#dsh5294)). The project must also have Substrate on for the
  engine to compile the graph — see
  [One source, two kinds of project](../language-v2/substrate.md#one-source-two-kinds-of-project--substrate-).
- DreamShader sets no shading model for `Base.FrontMaterial` *(since 2.0.0; 1.x forced `Substrate`
  and refused any other `ShadingModel`)*: a `ShadingModel` setting is applied as written.
- A BSDF node takes its arguments by name; the five composition nodes (`Add`, `Weight`, `Mix`,
  `Layer`, `Select`) take them by position too *(since 2.0.0)*. A positional argument elsewhere is
  [`DSH5220`](../diagnostics/DSH5xxx.md#dsh5220), and `Class=` is
  [`DSH5216`](../diagnostics/DSH5xxx.md#dsh5216): the name already is the class.
- A `Substrate.*` call in a `GraphFunction` body is not lifted into a node — no Custom node input
  carries a Substrate value — and says so with the warning
  [`DSH6316`](../diagnostics/DSH6xxx.md#dsh6316) *(since 2.0.0)*.
- *(since 2.0.0)* Substrate values combine: `A + B`, `A * w` and `lerp(A, B, t)` are `Substrate.Add`,
  `Substrate.Weight` and `Substrate.HorizontalMix`, and an `if` between two of them is a
  `StaticSwitch` under a static condition and a `Substrate.Select` otherwise. Any other operator is
  [`DSH5293`](../diagnostics/DSH5xxx.md#dsh5293), and a member read on a Substrate value
  [`DSH4207`](../diagnostics/DSH4xxx.md#dsh4207). See [Substrate sugar](../language-v2/substrate.md).

### The same in a `.dss`

```hlsl
// DShader/Materials/M_Substrate.dss
uniform float3 Color = float3(0.1, 0.6, 1.0);

export void M_Substrate(inout material m)
{
    m.FrontMaterial = Substrate.Unlit(EmissiveColor = Color);
}
```

*See also:* [`Substrate.*`](../builtins/substrate.md) ·
[Output bindings](../language/output-bindings.md) · [Types](../language/types.md)

---

<a id="9-shaderfunction-in-a-dsf-imported-and-called"></a>

## 9. `ShaderFunction` in a `.dsf`, called from a material

A `.dsf` declares function assets — `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` — beside
helper blocks (`Function`, `GraphFunction`, `Namespace`, `VirtualFunction`). A file makes a material
or function assets, never both ([`DSH6201`](../diagnostics/DSH6xxx.md#dsh6201)), and only a `.dsh`
can be imported, so a material reaches the generated function as it reaches any existing asset:
through a `VirtualFunction` *(since 2.0.0)*.

```c
// DShader/Functions/F_Tint.dsf
ShaderFunction(Name="Functions/F_Tint")
{
    Inputs = {
        vec3 InColor;
        opt float Strength = 1.0 [
            Description  = "Preview strength";
            SortPriority = 10;
        ];
    }

    Outputs = {
        vec3 OutColor [Description="Tinted colour"];
    }

    Settings = {
        Description       = "Tint helper";
        ExposeToLibrary   = true;
        LibraryCategories = "Examples";
    }

    Graph = {
        OutColor = InColor * Strength;
    }
}
```

Calling it from a material:

```c
// DShader/Materials/M_UsesTint.dsm
VirtualFunction(Name="F_Tint")
{
    Options = { Asset = Path(Game, "Functions/F_Tint"); }

    Inputs = {
        vec3 InColor;
        opt float Strength = 1.0;
    }

    Outputs = {
        vec3 OutColor;
    }
}

Shader(Name="Materials/M_UsesTint")
{
    Properties = {
        vec3 Albedo = vec3(0.6, 0.8, 1.0);
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        // named form; the opt input may be omitted or passed `default`
        Color = F_Tint(InColor = Albedo, Strength = 0.5);
    }
}
```

- *(since 2.0.0)* Through 1.9.x the material imported the function file —
  `import "Functions/F_Tint.dsf";` — and one compile built both assets. That import is now
  [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252); the `VirtualFunction` declares the asset that
  `F_Tint.dsf` builds, and each file builds its own.
- Build the function first. The call loads the asset when the material is built, and a missing one
  is [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219). A material function is saved on every successful
  build, and the commandlet's `-All` compiles every `.dsf` before anything else.
- Inside its own `.dsf`, a `ShaderFunction` is called by the last segment of its `Name` (`F_Tint`);
  anywhere else, by the `VirtualFunction`'s `Name`. Names are case-sensitive *(since 2.0.0)*; a call
  that matches only in case resolves with the warning [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275).
- Arguments are positional, named, or positional then named *(since 2.0.0; 1.x refused a mix)*; a
  positional argument after a named one is [`DSH2158`](../diagnostics/DSH2xxx.md#dsh2158). An input
  left out of a call must be `opt` or have a default, or the call is
  [`DSH4217`](../diagnostics/DSH4xxx.md#dsh4217).
- `opt` — or *(since 2.0.0)* a default value alone — is what makes an input optional on the generated
  `UMaterialFunction`; its default drives the input's preview value.
- A material function honours `Description`, `ExposeToLibrary` and `LibraryCategories`.
  *(since 2.0.0)* `ExposeToLibrary` takes effect only with a non-empty `LibraryCategories`,
  `UserExposedCaption` is not applied (warning [`DSH3264`](../diagnostics/DSH3xxx.md#dsh3264)), and any
  other key is ignored with the warning [`DSH3263`](../diagnostics/DSH3xxx.md#dsh3263). In a `Shader`
  a key that names no material property fails the build — see
  [example 13](#13-settings-domain-shading-model-blend-mode-backend).

### The same in a `.dss`

```hlsl
// DShader/Functions/F_Tint.dss
/// @desc Tint helper
/// @library Examples
/// @param Strength  Preview strength
export void F_Tint(float3 InColor, float Strength = 1.0, out float3 OutColor)
{
    OutColor = InColor * Strength;
}
```

```hlsl
// DShader/Materials/M_UsesTint.dss
/// @asset /Game/Functions/F_Tint
extern void F_Tint(float3 InColor, float Strength = 1.0, out float3 OutColor);

#pragma material(Domain = Surface, ShadingModel = Unlit)

uniform float3 Albedo = float3(0.6, 0.8, 1.0);

export void M_UsesTint(inout material m)
{
    float3 Tinted;
    F_Tint(Albedo, Strength = 0.5, OutColor = Tinted);
    m.EmissiveColor = Tinted;
}
```

An `export` function that is not a material entry is a material function; a default argument makes
an input optional, an `out` parameter is an extra output, and `/// @library` is the library category.
The material calls the asset through an `extern` prototype bound by `/// @asset` — the 2.0
`VirtualFunction`.

*See also:* [`ShaderFunction`](../language/shader-function.md) ·
[`VirtualFunction`](../language/virtual-function.md) ·
[`Inputs` / `Outputs`](../language/inputs-outputs.md) ·
[Function settings](../settings/function.md) · [Graph calls](../graph/calls.md)

---

## 10. `ShaderLayer` / `ShaderLayerBlend`

These generate native `UMaterialFunctionMaterialLayer` and `UMaterialFunctionMaterialLayerBlend`
assets *(since 1.3.0)*, and their interfaces are fixed by signature rules.

```c
// DShader/Layers/L_SimpleSurface.dsf
ShaderLayer(Name="Layers/L_SimpleSurface")
{
    Properties = {
        VectorParameter LayerColor = float4(0.8, 0.2, 0.1, 1.0) [Group="Layer"];
        ScalarParameter LayerRough = 0.5                        [Group="Layer"; Slider(0, 1)];
    }

    Outputs = {
        MaterialAttributes Attrs;
    }

    Graph = {
        Attrs.BaseColor = LayerColor.rgb;
        Attrs.Roughness = LayerRough;
    }
}

ShaderLayerBlend(Name="Layers/LB_Overlay")
{
    Properties = {
        ScalarParameter Alpha = 0.5 [Group="Blend"; Slider(0, 1)];
    }

    Inputs = {
        MaterialAttributes Bottom;
        MaterialAttributes Top;
    }

    Outputs = {
        MaterialAttributes Attrs;
    }

    Graph = {
        Attrs.BaseColor = lerp(Bottom.BaseColor, Top.BaseColor, Alpha);
        Attrs.Roughness = lerp(Bottom.Roughness, Top.Roughness, Alpha);
    }
}
```

| Block | Inputs | Output |
| :-- | :-- | :-- |
| `ShaderLayer` | none, or one `MaterialAttributes`. *(since 2.0.0)* The layer always gets exactly one `MaterialAttributes` input, named after its output — the material it is applied to | exactly one `MaterialAttributes` |
| `ShaderLayerBlend` | `MaterialAttributes` only, at least one *(since 2.0.0; 1.x: exactly two)*; the engine's layer stack blends two | exactly one `MaterialAttributes` |

- A layer with any other input or output is [`DSH6204`](../diagnostics/DSH6xxx.md#dsh6204); a blend
  without a `MaterialAttributes` input, or with another output,
  [`DSH6205`](../diagnostics/DSH6xxx.md#dsh6205); either one without a `MaterialAttributes` output
  [`DSH3278`](../diagnostics/DSH3xxx.md#dsh3278). Layer and blend controls belong in `Properties`,
  where they become parameters of the generated function.
- A layer's output starts as the material that came in, so `L_SimpleSurface` changes two attributes of
  the material it is applied to; a blend's output starts empty.
- On **UE 5.7+** each blend input gets a `BlendInputRelevance` from its name: `Top` / `TopLayer` is
  `Top`, `Bottom` / `BottomLayer` / `Base` / `BaseLayer` is `Bottom`, and any other name is `Bottom`
  on the first input and `Top` on a later one. On earlier engines the property does not exist.
- `MaterialLayer(...)` / `MaterialLayerBlend(...)` still parse as the old spellings, with the warning
  [`DSH2251`](../diagnostics/DSH2xxx.md#dsh2251) *(deprecated in 1.3.0)*.
- Neither block can be called from a `Graph` *(since 2.0.0)*: [`DSH6208`](../diagnostics/DSH6xxx.md#dsh6208).
  Their consumer is the layer stack of a material or material instance.

### The same in a `.dss`

```hlsl
// DShader/Layers/L_SimpleSurface.dss
/// @group Layer
uniform float4 LayerColor = float4(0.8, 0.2, 0.1, 1.0);
/// @group Layer   @slider 0 1
uniform float LayerRough = 0.5;
/// @group Blend   @slider 0 1
uniform float Alpha = 0.5;

/// @layer
export void L_SimpleSurface(inout material m)
{
    m.BaseColor = LayerColor.rgb;
    m.Roughness = LayerRough;
}

/// @layerblend
export void LB_Overlay(material Bottom, material Top, inout material Attrs)
{
    Attrs.BaseColor = lerp(Bottom.BaseColor, Top.BaseColor, Alpha);
    Attrs.Roughness = lerp(Bottom.Roughness, Top.Roughness, Alpha);
}
```

One file, two exports, two assets: `/// @layer` and `/// @layerblend` say which kind each one is.

*See also:* [`ShaderLayer` / `ShaderLayerBlend`](../language/shader-layer.md) ·
[MaterialAttributes](../graph/material-attributes.md) · [Source files](../language/source-files.md)

---

## 11. `VirtualFunction`: wrapping an existing asset

A `VirtualFunction` generates nothing. It declares the interface of a `UMaterialFunction` that
already exists so that `Graph` blocks can call it with type checking.

```c
// DShader/VirtualFunctions/BufferWriter.dsh
VirtualFunction(Name="BufferWriter")
{
    Options = {
        Asset       = Path(Game, "MaterialFunctions/F_BufferWriter");
        Description = "Existing material function declared for Graph calls.";
    }

    Inputs = {
        float3 Color;
        opt float Alpha = 1.0;
    }

    Outputs = {
        float3 Result;
    }
}
```

```c
// DShader/Materials/M_Buffered.dsm
import "VirtualFunctions/BufferWriter.dsh";

Shader(Name="Materials/M_Buffered")
{
    Properties = { vec3 Tint = vec3(1.0, 0.4, 0.1); }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = BufferWriter(Color = Tint, Alpha = 0.5);
    }
}
```

> [!NOTE]
> This example references `/Game/MaterialFunctions/F_BufferWriter`, a project asset. Substitute a
> path that exists in your project, or let the editor write the declaration for you: the
> **CreateVirtualFunction** entry of a `UMaterialFunction`'s context menu writes a matching `.dsh`
> under `DShader/VirtualFunctions`, and **Create extern Prototype (2.0)** writes the `extern` form a
> `.dss` includes. See [VirtualFunction tools](../tools/virtual-function-tools.md).

- The asset may come from `Options = { Asset = … }` or from the header attribute
  `VirtualFunction(Name="…", Asset="…")`. Without one the block is
  [`DSH6312`](../diagnostics/DSH6xxx.md#dsh6312).
- `Settings` is an accepted alias for `Options`, and `Properties` is an accepted alias for `Inputs`
  inside this block only. A `Graph` or `Code` section is [`DSH2247`](../diagnostics/DSH2xxx.md#dsh2247).
- At least one output is required ([`DSH6313`](../diagnostics/DSH6xxx.md#dsh6313)), and `Name` must be
  an identifier ([`DSH6311`](../diagnostics/DSH6xxx.md#dsh6311)).
- Nothing is checked against the asset until a `Graph` calls it. A declared input or output is then
  matched to the asset's pins by name — exactly, then ignoring case — and failing both by its
  position; one that matches nothing is [`DSH8220`](../diagnostics/DSH8xxx.md#dsh8220) /
  [`DSH8221`](../diagnostics/DSH8xxx.md#dsh8221), and an asset that does not load
  [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219).
- `Path` roots: `Game`, `Engine`, `Plugin.<Name>` / `Plugins.<Name>`, a full object path, or a bare
  quoted `"/Game/…"`. See [`VirtualFunction`](../language/virtual-function.md#the-asset-reference).

### The same in a `.dss`

```hlsl
// DShader/VirtualFunctions/BufferWriter.dsh
/// @asset /Game/MaterialFunctions/F_BufferWriter
/// @desc Existing material function declared for Graph calls.
extern float3 BufferWriter(float3 Color, float Alpha = 1.0);
```

```hlsl
// DShader/Materials/M_Buffered.dss
#include "VirtualFunctions/BufferWriter.dsh"

#pragma material(Domain = Surface, ShadingModel = Unlit)

uniform float3 Tint = float3(1.0, 0.4, 0.1);

export void M_Buffered(inout material m)
{
    m.EmissiveColor = BufferWriter(Tint, Alpha = 0.5);
}
```

The header is the same declaration in 2.0 spelling — an `extern` prototype; a first output named
`Result` is its return value, and a default argument an input the call may leave out. A `.dsm`
imports this header as it imports the 1.x one, and the `.dss` includes either.

*See also:* [`VirtualFunction`](../language/virtual-function.md) ·
[`Options`](../language/options.md) · [`Path(...)`](../parameters/path.md) ·
[VirtualFunction tools](../tools/virtual-function-tools.md)

---

## 12. `GraphFunction`: hoisting a `UE.*` call into a Custom node

A `GraphFunction` body is HLSL like a `Function`, but every `UE.*` call inside it is evaluated as a
real material node and wired into the generated Custom node as an extra input pin.

```c
// DShader/Shared/Wind.dsh
GraphFunction WindPulse(in float2 uv, out float pulse) {
    float t = UE.Time();
    pulse = sin(uv.x * 8.0 + t);
}
```

```c
// DShader/Materials/M_Wind.dsm
import "Shared/Wind.dsh";

Shader(Name="Materials/M_Wind")
{
    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        vec2  UV    = UE.TexCoord(Index = 0);
        float Pulse = WindPulse(UV);          // value form: one out result
        Color = vec3(Pulse, Pulse, Pulse);
    }
}
```

Generated Custom-node code, in outline:

```hlsl
float pulse = (float)0;
// Begin DreamShader source: <file>
// DreamShader custom: WindPulse line <N>

    float t = _ds_WindPulse_UE0;
    pulse = sin(uv.x * 8.0 + t);
// End DreamShader source: <file>
return pulse;
```

- The generated pin is named `_ds_<Function>_UE<N>`: the base name `__ds_<Function>_UE<N>` is
  identifier-sanitized, which collapses its leading `__` to one `_`, and a name that collides with a
  declared `in` parameter gets `_1`, `_2`, … See
  [`GraphFunction`](../language/graph-function.md#generated-input-pins).
- *(since 2.0.0)* A lifted call is bound in the function's own scope: its parameters, replaced at each
  call by that call's arguments, and the file's declarations. A name only the caller has is
  [`DSH6325`](../diagnostics/DSH6xxx.md#dsh6325) — 1.x read the caller's `Graph` scope.
- Only the `UE.` prefix is lifted. A `Substrate.*` call stays HLSL text, with the warning
  [`DSH6316`](../diagnostics/DSH6xxx.md#dsh6316); a call to another `Function` is embedded in the
  node, as in a `Function` body.
- A lifted value may be a number, a bool or *(since 2.0.0)* a texture object; a `MaterialAttributes`
  or Substrate value is [`DSH6329`](../diagnostics/DSH6xxx.md#dsh6329).
- `GraphFunction` accepts no `SelfContained`/`Inline` modifier ([`DSH6307`](../diagnostics/DSH6xxx.md#dsh6307))
  and no recursion ([`DSH6260`](../diagnostics/DSH6xxx.md#dsh6260), and
  [`DSH6330`](../diagnostics/DSH6xxx.md#dsh6330) through a lifted call). It takes named arguments
  *(since 2.0.0)*. An empty body is accepted, and its outputs come out as 0.
- The statement form works too: `WindPulse(UV, Pulse);` declares `Pulse` (info
  [`DSH5283`](../diagnostics/DSH5xxx.md#dsh5283)) when nothing else does.

### The same in a `.dss`

```hlsl
// DShader/Materials/M_Wind.dss
#pragma material(Domain = Surface, ShadingModel = Unlit)

/// @custom
void WindPulse(float2 uv, out float pulse)
{
    float t = UE.Time();
    pulse = sin(uv.x * 8.0 + t);
}

export void M_Wind(inout material m)
{
    float2 UV = UE.TexCoord(Index = 0);
    float Pulse;
    WindPulse(UV, Pulse);
    m.EmissiveColor = float3(Pulse, Pulse, Pulse);
}
```

A `/// @custom` function is a `GraphFunction` and a `Function` at once: its body is verbatim HLSL,
and a `UE.` call in it is lifted into an input of the Custom node the same way.

*See also:* [`GraphFunction`](../language/graph-function.md) · [`Function`](../language/function.md) ·
[Generated HLSL](../generation/generated-hlsl.md) · [`UE.*` catalogue](../builtins/ue.md)

---

## 13. Settings: domain, shading model, blend mode, backend

```c
// DShader/Materials/M_Glass.dsm
Shader(Name="Materials/M_Glass")
{
    Properties = {
        vec3  Tint    = vec3(0.7, 0.9, 1.0);
        float Opacity = 0.35;
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Translucent";
        TwoSided     = true;
        Wireframe    = false;
        Backend      = "Graph";
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.BaseColor = Color;
        Base.Opacity   = Alpha;
    }

    Graph = {
        Color = Tint;
        Alpha = Opacity;
    }
}
```

| Key | Aliases | Values |
| :-- | :-- | :-- |
| `MaterialDomain` | `Domain` | `Surface`, `DeferredDecal` / `Decal`, `LightFunction`, `Volume`, `PostProcess`, `UI` / `UserInterface`, `RuntimeVirtualTexture` / `VirtualTexture` |
| `ShadingModel` | — | `Unlit`, `DefaultLit` / `Lit`, `Subsurface`, `PreintegratedSkin`, `ClearCoat`, `SubsurfaceProfile`, `TwoSidedFoliage`, `Hair`, `Cloth`, `Eye`, `SingleLayerWater`, `ThinTranslucent`, plus `Substrate` / `Strata` on UE 5.4+ |
| `BlendMode` | `RenderType` | `Opaque`, `Masked` / `Cutout`, `Translucent` / `Transparent`, `Additive`, `Modulate`, `AlphaComposite` / `PremultipliedAlpha` / `Premultiplied`, `AlphaHoldout`, `TranslucentColoredTransmittance` |
| `Backend` | — | `Graph`, `ThinCustom`; `Instance` is the old spelling of `ThinCustom`, with the warning [`DSH7204`](../diagnostics/DSH7xxx.md#dsh7204) |

- Enum values are matched with spaces, `_` and `-` stripped, case-insensitively: `"Default Lit"`,
  `"DefaultLit"`, `"default_lit"` and `"DEFAULT-LIT"` are one alias. Quotes are optional on every
  setting value.
- `MaterialDomain`, `ShadingModel` and `BlendMode` are resolved through the tables above; `Backend`
  and *(since 2.0.0)* `Substrate` are DreamShader's own keys. **Every other key is reflected straight
  onto `UMaterial`** — `TwoSided` and `Wireframe` above are real `UMaterial` properties. A key that
  names no property fails the build: [`DSH8215`](../diagnostics/DSH8xxx.md#dsh8215), carrying
  [`DSH7118`](../diagnostics/DSH7xxx.md#dsh7118).
- A key written twice is the warning [`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262) *(since 2.0.0;
  silent before)*, and the later value wins. Repeated `Settings` sections merge.
- Omitting `Backend` falls back to the project's **Default Compiler Backend** (`ThinCustom` as
  shipped). With `Backend = "Graph"` the material is an ordinary `UMaterial`, saved on every
  successful build.

> [!WARNING]
> `Backend = "";` is the error [`DSH7201`](../diagnostics/DSH7xxx.md#dsh7201) *(since 2.0.0; 1.x read
> it as `Graph`)*, and any value that is no backend is [`DSH7202`](../diagnostics/DSH7xxx.md#dsh7202).
> Only *omitting* the key falls back to the project setting.

- A modified engine adds its own shading models automatically — the reflected table is read from
  `EMaterialShadingModel` at runtime, and the project's mapping tables can add or shadow spellings.

### The same in a `.dss`

```hlsl
// DShader/Materials/M_Glass.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Translucent)
#pragma material(TwoSided = true, Wireframe = false, Backend = Graph)

uniform float3 Tint = float3(0.7, 0.9, 1.0);
uniform float Opacity = 0.35;

export void M_Glass(inout material m)
{
    m.BaseColor = Tint;
    m.Opacity = Opacity;
}
```

`#pragma material` takes the same keys and values as `Settings`, and may be split over several
lines; a key set twice is an error there, [`DSH7200`](../diagnostics/DSH7xxx.md#dsh7200).

*See also:* [`Settings`](../settings/index.md) · [Shader settings](../settings/material.md) ·
[Material enums](../settings/material-enums.md) · [Backend](../settings/backend.md) ·
[Project settings](../settings/project.md)

---

## 14. Layout placement and `#Region`

`Layout` pins generated nodes to fixed positions and draws comment boxes. `#Region` / `#EndRegion`
group statements inside a `Graph` block; each region becomes a comment box in the generated graph.

```c
// DShader/Materials/M_Laid.dsm
Shader(Name="Materials/M_Laid")
{
    Properties = {
        VectorParameter BaseColor = float4(0.8, 0.8, 0.8, 1.0) [Group="Surface"; SortPriority=10];
        ScalarParameter Roughness = 0.55                       [Group="Surface"; SortPriority=20];
        Texture2D       NoiseTex  = Path(Engine, "EngineResources/WhiteSquareTexture");
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        float3 Color;
        float  Rough;

        Base.BaseColor = Color;
        Base.Roughness = Rough;
    }

    Graph = {
        #Region "Sampling"
        vec2 UV    = UE.TexCoord(Index = 0);
        vec4 Noise = SampleTexture2D(NoiseTex, UV);
        #EndRegion

        #Region "Surface"
        Color = BaseColor.rgb * Noise.rgb;
        Rough = Roughness;
        #EndRegion
    }

    Layout = {
        Comment(Name="Sampling", X=-1200, Y=-200, W=900, H=400, Color=float4(0.10, 0.16, 0.22, 0.35));
        Comment(Name="Surface",  X=-1200, Y=260,  W=900, H=400);
        Node(Var="UV",    X=-1100, Y=-120);
        Node(Var="Noise", X=-760,  Y=-120);
    }
}
```

| Call | Required arguments | Optional |
| :-- | :-- | :-- |
| `Node` | `Var` (text), `X`, `Y` (integers) | — |
| `Comment` | `Name` (text), `X`, `Y`, `W`, `H` (integers) | `Color` (a `float4` literal) |

- `Var` names a `Graph` variable. Anything but `Node(…)` and `Comment(…)` in a `Layout` section is
  [`DSH3274`](../diagnostics/DSH3xxx.md#dsh3274); a missing or non-integer argument
  [`DSH3275`](../diagnostics/DSH3xxx.md#dsh3275), a malformed `Color`
  [`DSH3276`](../diagnostics/DSH3xxx.md#dsh3276).
- A second `Layout` section **replaces** the first rather than appending, with the warning
  [`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258) — unlike `Properties`, `Inputs` and `Outputs`, which
  append.
- `#Region` names may be quoted or bare, the directives are matched case-insensitively, and regions
  nest. A `#Region` without a name is [`DSH2216`](../diagnostics/DSH2xxx.md#dsh2216), an `#EndRegion`
  with none open [`DSH2217`](../diagnostics/DSH2xxx.md#dsh2217), a region never closed
  [`DSH2218`](../diagnostics/DSH2xxx.md#dsh2218). A directive is a token, so every diagnostic keeps its
  real line and column.
- Region directives are read only in a `Graph` body; a `Function` or `GraphFunction` body is HLSL,
  passed on as written.
- `#Region` names and `Layout` `Comment` names are independent: the `Sampling` region and the
  `Sampling` comment above are two boxes.
- The decompiler writes node positions and free comment boxes back when the *Export Decompiled
  Layout* project setting is on (the default) — as `#pragma layout` lines, or as a `Layout` section
  with the legacy format.

> [!WARNING]
> Regeneration clears the target graph. Node positions not pinned by `Layout`, hand-added nodes,
> node property tweaks and comment boxes whose text begins with `DreamShader: ` are destroyed. Only
> comment boxes without that prefix survive. An asset edited by hand since it was generated is not
> rebuilt at all ([`DSH8207`](../diagnostics/DSH8xxx.md#dsh8207)); see
> [Divergence](../generation/divergence.md).

### The same in a `.dss`

```hlsl
// DShader/Materials/M_Laid.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

/// @group Surface   @sort 10
uniform float4 BaseColor = float4(0.8, 0.8, 0.8, 1.0);
/// @group Surface   @sort 20
uniform float Roughness = 0.55;
/// @default /Engine/EngineResources/WhiteSquareTexture
uniform Texture2D NoiseTex;

#pragma layout(Comment, Name = "Sampling", X = -1200, Y = -200, W = 900, H = 400, Color = "0.10 0.16 0.22 0.35")
#pragma layout(Comment, Name = "Surface", X = -1200, Y = 260, W = 900, H = 400)
#pragma layout(Node, Var = "UV", X = -1100, Y = -120)
#pragma layout(Node, Var = "Noise", X = -760, Y = -120)

export void M_Laid(inout material m)
{
    #pragma region Sampling
    float2 UV = UE.TexCoord(Index = 0);
    float4 Noise = NoiseTex.Sample(UV);
    #pragma endregion

    #pragma region Surface
    m.BaseColor = BaseColor.rgb * Noise.rgb;
    m.Roughness = Roughness;
    #pragma endregion
}
```

`#pragma layout(…)` is the 2.0 `Layout` statement, and `#pragma region` / `#pragma endregion` the
2.0 `#Region` / `#EndRegion`.

*See also:* [`Layout`](../language/layout.md) · [Graph layout](../generation/graph-layout.md) ·
[Regeneration](../generation/regeneration.md) · [Decompiler](../tools/decompiler.md)

---

## 15. Runtime Virtual Texture: sampling and writing

The RVT nodes have no dedicated builtin family and need none. The sample, replace and feature-switch
nodes are ordinary expressions reached through [`UE.<ClassName>`](../builtins/ue-expression.md);
`RuntimeVirtualTextureOutput` is a *custom output* with no output pin at all, so it is never the
right-hand side of a `Graph` assignment — it is bound in `Outputs` through
[`Expression( … ).Pin[i]`](../language/output-bindings.md#the-expression--pini-target).

```c
// DShader/Materials/M_RVTSurface.dsm
Shader(Name="Materials/M_RVTSurface", Root="Game")
{
    Properties = {
        vec3 Fallback = vec3(0.5, 0.5, 0.5);
    }

    Settings = {
        Backend      = "Graph";
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        float3 SurfaceColor;
        float3 RVTBaseColor;
        float  RVTRoughness;

        Base.BaseColor = SurfaceColor;

        // Both statements reuse one RuntimeVirtualTextureOutput node.
        Expression(Class="RuntimeVirtualTextureOutput").Pin[0] = RVTBaseColor;
        Expression(Class="RuntimeVirtualTextureOutput").Pin[2] = RVTRoughness;
    }

    Graph = {
        float3 Sampled = UE.RuntimeVirtualTextureSample(
            OutputType     = "float3",
            Output         = "BaseColor",
            MaterialType   = "BaseColor_Normal_Roughness",
            VirtualTexture = Path(Game, "RVT/RVT_Terrain"));

        // Where the virtual texture has no page, fall back to the parameter ...
        float3 Blended = UE.RuntimeVirtualTextureReplace(
            OutputType = "float3", Default = Fallback, VirtualTextureOutput = Sampled);

        // ... and where virtual texturing is off entirely, fall back to it again.
        SurfaceColor = UE.VirtualTextureFeatureSwitch(
            OutputType = "float3", Yes = Blended, No = Fallback);

        RVTBaseColor = SurfaceColor;
        RVTRoughness = 0.5;
    }
}
```

> [!NOTE]
> This example references `/Game/RVT/RVT_Terrain`, a project `URuntimeVirtualTexture` asset.
> Substitute one that exists in your project.

| Node | `Class` specifier | Written as |
| :-- | :-- | :-- |
| Runtime Virtual Texture Sample | `RuntimeVirtualTextureSample` | `UE.…` in `Graph` |
| Runtime Virtual Texture Sample Parameter | `RuntimeVirtualTextureSampleParameter` | a [parameter declaration](../parameters/parameter-nodes.md) in `Properties` |
| Runtime Virtual Texture Replace | `RuntimeVirtualTextureReplace` | `UE.…` in `Graph` |
| Virtual Texture Feature Switch | `VirtualTextureFeatureSwitch` | `UE.…` in `Graph` |
| Runtime Virtual Texture Output | `RuntimeVirtualTextureOutput` | `Expression( … ).Pin[i]` in `Outputs` |

- `RuntimeVirtualTextureSample` carries eight outputs: `0` BaseColor, `1` Specular, `2` Roughness,
  `3` Normal, `4` WorldHeight, `5` Mask, `6` Displacement, `7` Mask4. Select one by name with
  `Output="BaseColor"` or by position with `OutputIndex=`; without a selector the call is read as a
  node with several outputs — see [Output widths](../builtins/ue.md#output-widths).
- `OutputType` is taken off each call *(since 2.0.0)*; the catalog types the nodes.
- `RuntimeVirtualTextureOutput` input pins, in `Pin[i]` order: `0` BaseColor, `1` Specular,
  `2` Roughness, `3` Normal, `4` WorldHeight, `5` Opacity, `6` Mask, `7` Displacement, `8` Mask4.
- `VirtualTexture` is a reflected asset property, so it takes a [`Path(...)`](../parameters/path.md)
  reference — `Path(Game, …)`, `Path(Plugin.<Name>, …)` — or a bare quoted object path. `MaterialType`
  and the node's other enum and bool properties are written by the same reflection pass, with the
  usual [enum spelling](../settings/material-enums.md) rules.
- A material that *writes* to RVT keeps `Domain = "Surface"` and adds the output node, exactly as
  above. The separate `Domain = "RuntimeVirtualTexture"` spelling maps to `MD_RuntimeVirtualTexture`,
  which the engine marks **deprecated and hidden**; DreamShader still accepts it so existing assets
  keep decompiling, but new materials should not use it.
- `Backend = "Graph"` is pinned here so the result is a plain `UMaterial`, saved on every build, whose
  nodes you can open and compare against the source. The nodes themselves do not depend on the
  backend.

### The same in a `.dss`

```hlsl
// DShader/Materials/M_RVTSurface.dss
#pragma material(Backend = Graph, Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

uniform float3 Fallback = float3(0.5, 0.5, 0.5);

export void M_RVTSurface(inout material m)
{
    float3 Sampled = UE.RuntimeVirtualTextureSample(
        MaterialType = "BaseColor_Normal_Roughness",
        VirtualTexture = "/Game/RVT/RVT_Terrain").BaseColor;
    float3 Blended = UE.RuntimeVirtualTextureReplace(Default = Fallback, VirtualTextureOutput = Sampled);
    float3 SurfaceColor = UE.VirtualTextureFeatureSwitch(Yes = Blended, No = Fallback);

    m.BaseColor = SurfaceColor;
    UE.Expression(Class = "RuntimeVirtualTextureOutput", Pin[0] = SurfaceColor, Pin[2] = 0.5);
}
```

An output is selected by member access (`.BaseColor`), and an asset property takes a quoted object
path. A custom-output node is a statement of the entry; `Pin[i] = …` connects input `i` by its
engine index.

*See also:* [`UE.Expression`](../builtins/ue-expression.md) ·
[Output bindings](../language/output-bindings.md) · [`Path(...)`](../parameters/path.md) ·
[Material enums](../settings/material-enums.md)

---

## Running the examples

1. Save the files under `<Project>/DShader` (or wherever `SourceDirectory` points).
2. With *Auto Compile On Save* enabled — the default — the editor compiles a source 0.25 s after it is
   saved (*Save Debounce Seconds*). A `Graph`-backend material and every material function, layer and
   blend are saved to disk by each successful build. A `ThinCustom` material — the default backend —
   is built Ephemeral, in memory, until a cook, an explicit *Materialize*, a child material instance
   or the commandlet gives it a package.
3. Headless compile of one file:

```powershell
& "<Engine>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" `
  "<Project>/MyProject.uproject" `
  -run=DreamShader compile -Source="<Project>/DShader/Materials/M_Minimal.dsm" -Force `
  -unattended -nopause -nosplash -stdout -log
```

Substitute `-All` for `-Source=` to compile every source of the project: every `.dsf` first, then the
other sources in path order; a `.dsh` is compiled through the files that include it. The
commandlet materializes every `ThinCustom` product, so it writes a package for each material.

*See also:* [Getting started](../getting-started.md) ·
[Ephemeral materials](../generation/ephemeral.md) · [Commandlet](../tools/commandlet.md) ·
[Editor integration](../tools/editor-integration.md)

## See also

- [Getting started](../getting-started.md) — installation and the first compile
- [DreamShaderLang](../language/index.md) — the 1.x declaration grammar, block by block
- [DreamShaderLang 2.0](../language-v2/index.md) — the `.dss` language of the second form of each example
- [`dsc migrate`](../tools/migrate.md) — rewriting a 1.x source as `.dss`, proved before it is written
- [Graph language](../graph/index.md) — the statement and expression language inside `Graph`
- [Builtins](../builtins/index.md) — `UE.*`, `Substrate.*`, math, and the HLSL library
- [Custom Pass examples](custom-pass.md) — `.dsp` pipelines with their materials and shaders
- [Parameters](../parameters/index.md) — every parameter token, metadata key and `Path` root
- [Settings](../settings/index.md) — per-file and project-wide configuration
- [Generation](../generation/index.md) — how a source file becomes an asset
- [Tools](../tools/index.md) — editor integration, decompiler, commandlet, bridge
- [Diagnostics](../diagnostics/index.md) — where diagnostics appear, how to read a code, the 1.x messages
- [Testing](../contributing/testing.md) — the `Tests/Corpus` fixtures the `.dss` forms are modelled on
