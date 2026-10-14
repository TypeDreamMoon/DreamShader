---
title: Functions
description: Function, GraphFunction and Namespace — parameters, return types, embedding of called functions, UE.* hoisting, and how calls resolve.
---

`Function` and `GraphFunction` move reusable logic out of raw `Graph` statements. Both take a C-style
signature and an HLSL body, and every call site becomes one `Custom` node whose code is that body; the
difference is what happens to the `UE.*` calls in it. The legacy front end reads both as 2.0
`/// @custom` functions (since 2.0.0), and `dsc migrate` writes them that way.

| | `Function` | `GraphFunction` |
| :-- | :-- | :-- |
| Body lands in | the calling node's code; when another body calls it, embedded in that node as a wrapper member | the calling node's code |
| `UE.*` calls in the body | left as literal HLSL text, which the shader compiler does not know | built as material expressions and passed in as `Custom`-node inputs |
| `Inline` / `SelfContained` | accepted, and change nothing | `DSH6307` |
| Callable from another `Function` body | yes — it is embedded | only when its body lifts no `UE.*` call; otherwise `DSH6327` |
| Recursion | `DSH6260` | `DSH6260`; reaching itself through a lifted call is `DSH6330` |

No shared `.ush` include is written any more: each node's `Code` carries all the HLSL it needs
(since 2.0.0). `Namespace` groups either kind under an `Ns::` prefix.

## Synopsis

```c
Function      [ ( <attributes> ) ] [ { Inline | SelfContained } ] [ <return-type> ] <name> ( [ <parameter-list> ] ) { <hlsl> }
GraphFunction [ ( <attributes> ) ]                                [ <return-type> ] <name> ( [ <parameter-list> ] ) { <hlsl> }

<attributes>     := Generate = <string> [ , Library = <string> ] [ , Description = <string> ]

<parameter-list> := <parameter> [ , <parameter> ] …
<parameter>      := [ { in | out } ] <type-token> <parameter-name>
```

| Notation | Meaning | Example |
| --- | --- | --- |
| `<x>` | Placeholder — substitute a real value; the angle brackets are not typed. | `Name = <string>` |
| `[ x ]` | Optional — the whole group may be left out. | `[, Root = <string>]` |
| `{ a \| b }` | Choice — take exactly one of the alternatives separated by \|. | `{ Node( … ) \| Comment( … ) }` |
| `…` | Repetition — the preceding item may appear any number of times. | `<property-declaration> …` |

The only `(Key = Value)` header is the optional one after the keyword (after 2.1.2): `Generate`, `Library` and
`Description`, which mean something in a package's header only — the material function the package
generates from the function; `Generate = ""` opts the function out of its namespace's template. Another key
is `DSH6331`, a `{...}` other than `{Name}` in a template `DSH6332`. See
[Packages ▸ Asset headers](../tools/packages.md#asset-headers). There are no `Settings` and no sections. The `{ … }` after the parameter list
is raw HLSL, kept verbatim: only its braces are read, and a brace inside a string or a comment does
not count.

The keywords `Function` and `GraphFunction` are **case-sensitive** (`function` is `DSH2240`); the
modifier, the `in` / `out` qualifiers and every type token are not.

## Declaration order

The parser reads identifiers left to right and disambiguates by lookahead: after one identifier, a
`(` makes it the name; another identifier makes it the *return type* and the next identifier the name.

| Form | Read as |
| :-- | :-- |
| `Function Name(…)` | name |
| `Function Type Name(…)` | return type, then name |
| `Function SelfContained Name(…)` | modifier, then name |
| `Function SelfContained Type Name(…)` | modifier, return type, name |
| `Function Type SelfContained Name(…)` | return type `Type`, name `SelfContained`, then a stray `Name` where `(` belongs — `DSH6319` |

The modifier is therefore fixed in **first** position, right **after** the keyword.

**Warning:**

A function may not be **named** `Inline` or `SelfContained`. `Function SelfContained(in float a,
out float b) { … }` takes the modifier branch and finds `(` where the name should be: `DSH6300`. The
old section-style body `Function Name { Inputs = { … } Code = { … } }` is `DSH6300` too.

## Parameters

A parameter is the words up to the next `,` or `)`; any whitespace — tabs and line breaks included —
only separates them. Two or three words are accepted:

| Words | Form | Qualifier |
| :-- | :-- | :-- |
| 2 | `<type> <name>` | defaults to `in` |
| 3 | `<qualifier> <type> <name>` | as written |
| fewer than 2, more than 3, or anything that is not a word (`[2]`, `= 1.0`) | — | `DSH6301` |

| Rule | Behaviour |
| :-- | :-- |
| Qualifiers | Only `in` and `out`, compared in lower case, so `In`, `OUT`, `iN` all work. |
| `inout` | **Does not exist** in a 1.x function: `DSH6302`. Split it into an `in` and an `out`; a `.dss` function may use `inout`. |
| Empty parameters | A lone `,` is skipped, so a trailing comma before `)` is tolerated. |
| Order | Parameters keep their declaration order. `in` parameters are the node's input pins, `out` parameters its outputs. The two may interleave. |
| Reserved name | A parameter named `__return` (ignoring case) is `DSH6303`. |
| Duplicate name | `DSH4214`. |
| Not available | `opt`, default values and `[ … ]` metadata are **not** part of this grammar (`DSH6301`); they belong to the [`Inputs` / `Outputs` sections](sections.md). |

### At least one output

A function must produce something: **either** at least one `out` parameter **or** a return type.
Neither is `DSH6305`.

## Return type

| Form | Legal | Result |
| :-- | :-- | :-- |
| `Function Name(in T a, out U r)` | yes | one output, `r` |
| `Function Name(in T a, out U r1, out V r2)` | yes | `r1` is the node's first output, `r2` an additional output |
| `Function U Name(in T a) { return expr; }` | yes | the returned value is the node's output |
| `Function U Name(in T a, out V r)` | **no** | `DSH6304` |
| `Function Name(in T a)` | **no** | `DSH6305` |
| `Function U Name(…) { return; }` | **no** | a top-level bare `return;` under a return type: `DSH6308` |
| `Function Name(…, out U r) { … return; … }` | **no** | a `return;` at any depth in a function without a return type: `DSH6256` (since 2.0.0) |

**A return type implies exactly one output and forbids every explicit `out`.** Multiple outputs require
`out` parameters and no return type.

### `return` is no longer rewritten

`return` stays as written (since 2.0.0). 1.x replaced every top-level `return` with an
assignment to a synthetic `__return` result; now the body *is* the body of the HLSL function that
returns the node's first output — the `Custom` node's own, or the helper's when another body embeds
it. A `return expr;` therefore returns the value at any brace depth, and the nested case that bypassed
`__return` in 1.x behaves like the top-level one.

What is still checked: a top-level bare `return;` under a return type (`DSH6308`), a `return;` anywhere
in a function without one (`DSH6256`), and a return type whose body has no `return` at all — the
warning `DSH6257`, the output will be 0. A body that is one bare expression is given its `return`. The
name `__return` stays reserved.

## Accepted parameter types

The 15 GLSL aliases are normalised to their HLSL spellings at parse time, and a scalar or vector in
another case (`Float3`) is read in lower case. A token that names no type is `DSH4201` at the
declaration (since 2.0.0) (1.x failed only when the function was called).

| Token | Valid as `in` | Valid as `out` / return |
| :-- | :-- | :-- |
| `float`, `half`, `int`, `uint`, `bool`, their `…2`–`…4` vectors and the GLSL aliases | yes | yes — a node output is `Float1`…`Float4` |
| `MaterialAttributes` | **no** — `DSH6210` | yes |
| `StaticBool`, `StaticBoolParameter` | yes — read as `bool` | yes — read as `bool` |
| `Texture2D`, `TextureCube`, `Texture2DArray`, `Texture3D`, `VolumeTexture` | yes | **no** — `DSH6254` |
| `SamplerState` | a sampler (since 2.0.0) (1.x read it as `Texture2D`) | **no** — `DSH6254` |
| `Substrate` | **no** — `DSH6253` | **no** — `DSH6253` |
| matrices (`float3x3`, `mat3`, …) | not across a node pin: `DSH4361` at a `Graph` call | same |

The generated HLSL spells every type the 2.0 way: `vec3` is `float3`, `MaterialAttributes` is
`FMaterialAttributes`, `VolumeTexture` is `Texture3D`.

**Warning:**

`Substrate` is never usable on a `Function` or a `GraphFunction`: a `Custom` node has no Substrate pins
in any engine version. Build the Substrate part in a `ShaderFunction` or in the `Shader`'s `Graph`.

### Texture parameters and samplers

`Texture2D`, `TextureCube`, `Texture2DArray`, `Texture3D` and `VolumeTexture` are *texture
parameters*. On the node one is a pin carrying the texture object, and the engine declares
`SamplerState <Name>Sampler` beside it — use that name in the body. Embedded as a helper, its HLSL
signature gains `SamplerState <Name>Sampler` after each texture parameter, and every call to it from
another body passes `<argument>Sampler` after the texture argument; such a call has to pass every
argument (`DSH6262`), and its texture argument has to be a plain name (`DSH6263`). A parameter of your
own called `<Texture>Sampler` collides with the pair (`DSH6255`). `SamplerState` is **not** a texture
parameter and gets no companion.

`<Texture>.Sample(UV)` on a texture parameter — and `SampleLevel`, `SampleBias`, `SampleGrad` with one
argument fewer than HLSL's own form — is rewritten to the engine's
`Texture2DSample(<Texture>, <Texture>Sampler, UV)` family (since 2.0.0); the native HLSL
spellings stay as written.

```c
Function SampleTinted(in Texture2D tex, in vec2 uv, in vec3 tint, out vec3 rgb, out float alpha)
{
    float4 texel = Texture2DSample(tex, texSampler, uv);
    rgb   = texel.rgb * tint;
    alpha = texel.a;
}
```

## Body normalisation

Every `Function` and `GraphFunction` body — and nothing else in the language — goes through an
identifier-level rewrite before it is stored. The scan skips comments and strings and does two
things: it substitutes 18 aliases (the 15 GLSL type spellings plus `mix` → `lerp`, `fract` → `frac`,
`mod` → `fmod`), and it flattens `A::B` (no spaces around `::`) to the sanitised identifier `A_B`.

**Warning:**

Because the alias match ignores case and applies to whole identifiers anywhere in the body, a helper
of your own called `Mix`, `Mod`, `Fract`, `Vec3` or `Mat4` — or a *variable* by one of those names — is
silently renamed in the emitted HLSL. There is no diagnostic — the `DSH5277` warning for a GLSL
spelling covers `Graph` text only. Rename it, or spell it so it does not collide (`MixColor`,
`ModValue`).

The flattened `Common_Remap01` is exactly a namespaced function's name, so a `Ns::Fn(…)` call inside a
body now reaches that function (since 2.0.0).

## Leading `#include` hoisting

(since 1.8.0)

`#include` directives at the **top** of a body — only whitespace and comments may precede them — are
not part of the function. They are blanked out of the body (overwritten with spaces, so every later
line and column still matches the source) and *hoisted* onto the include list (`IncludeFilePaths`) of
every `Custom` node whose code holds the body: the function's own nodes, and the node of any other
`Function` that embeds it, the embedded functions' includes first (since 2.0.0).

```c
Function float3 SampleWind(in float3 WorldPos)
{
    #include "/Plugin/DreamWind/Shared/DreamWindShared.h"   // hoisted
    FDreamWindSample S = DW_SampleAnalytic(DW_UnpackParams(P0, P1, P2), WorldPos);
    return S.Velocity;
}
```

| Case | Handling |
| :-- | :-- |
| `#include "…"` or `#include <…>` | the path is stored as written, delimiters removed, and must be a virtual shader path the engine resolves (`/Engine/…`, `/Plugin/<Name>/…`) |
| duplicates on one node | collapsed to one entry |
| an empty path | `DSH6258` |
| an `#include` **after** the first statement | not hoisted; it stays inside the generated function, which only works for macro-only headers |

The engine puts a node's includes at file scope, so a header that defines functions works. This is
what lets a header shared between C++ and HLSL be consumed directly, without copying its functions
into DreamShaderLang.

## Generated HLSL

A call site's `Custom` node carries all of its HLSL in its own `Code` (since 2.0.0):

```hlsl
struct generated_wrapper_<Name>_<CRC32>          // only when the body calls other Functions
{
	<Type0> DreamShaderFn_<Callee>(<params>)     // one member per embedded Function,
	{                                            // a callee before its caller
		// Begin DreamShader source: <file>
		// DreamShader custom: <Callee> line <N>
		<callee body, verbatim>
		// End DreamShader source: <file>
		return <Out0>;
	}
};
generated_wrapper_<Name>_<CRC32> __ds_wrapper_<CRC32>;

<Type0> <Out0> = (<Type0>)0;                     // the first out, for a function without a return type
// Begin DreamShader source: <file>
// DreamShader custom: <Name> line <N>
<body, verbatim; calls to embedded functions go through __ds_wrapper_<CRC32>>
// End DreamShader source: <file>
return <Out0>;                                   // appended only when the body has no top-level return
```

| Element | Rule |
| :-- | :-- |
| Symbol name | `DreamShaderFn_` + the function's name with every non-`[A-Za-z0-9_]` character turned to `_` and runs of underscores collapsed. A namespaced `Common::ApplyTint` is the function `Common_ApplyTint`, so `DreamShaderFn_Common_ApplyTint`. |
| Why the prefix | An unprefixed `Luminance(float3)` would shadow the engine intrinsic from `Common.ush` when a sibling member calls it. |
| A helper's return value | its return type; for a function without one, its first `out` parameter — which is why a body may call a multi-output `Function` as a value. |
| The node's own body | not wrapped in a function of DreamShader's: it is the `Custom` node's code. Its first output is the returned value, or a local for the first `out`; every other `out` is an additional output the engine declares and zero-initialises. |
| Markers | the `Begin` / `custom` / `End` comment lines around each body let a shader-compile error be mapped back to its source line; no rewrite adds or removes a line inside a body. |

The node's title (its `Description`) is the function's name — `Ns::Fn` for a namespace member.

## `Inline` / `SelfContained`

Every call site's node holds the function's own body and every other `Function` that body calls,
transitively, as members of the `generated_wrapper_*` struct — HLSL has no nested functions, and a
`Custom` node's code is a function body. 1.x did this only for a `SelfContained` function and otherwise
referenced its shared include; 2.0 has no shared include, so every function gets what `SelfContained`
asked for, and the modifier is read and changes nothing (since 2.1.1). (2.0.0 – 2.1.0 read it as
`/// @custom selfcontained`, which embeds nothing, so each call such a body made was left for the
shader compiler and failed.)

| Aspect | Every `Function`, with `Inline` / `SelfContained` or without |
| :-- | :-- |
| Where the body lives | in the code of every node that calls it, and as a wrapper member in every node whose body calls it |
| Other `Function`s its body calls | embedded, a callee before its caller |
| `IncludeFilePaths` | the leading includes of the body and of every embedded function |
| A call from the node's body to an embedded function | `__ds_wrapper_<CRC>.DreamShaderFn_<Name>(…)` |
| Recursion | `DSH6260` |

`Inline` is the old spelling of `SelfContained`; it reads the same, with the warning `DSH6306`. A call
from a body to something that is not a `Function` — a `ShaderFunction` or a `VirtualFunction` — is
`DSH6261`; to a `GraphFunction` with lifted `UE.*` calls, `DSH6327`. A call that matches a function only
in case is the warning `DSH6259` and is left for the shader compiler.

## `GraphFunction` and `UE.*` hoisting

(since 1.3.1)

When the body is read it is scanned, and every `UE.*` call is recorded as a lifted call. When the node's
code is built, the call's text is replaced by the name of a generated input pin, which carries the
material expression the call evaluates to.

A call is lifted only when all of these hold, in order:

| Step | Condition |
| :-- | :-- |
| 1 | the character before the `U` is an identifier boundary |
| 2 | the next three characters are `U`, `E`, `.` — matched **case-insensitively** in a 1.x body, so `ue.`, `Ue.` and `uE.` all trigger |
| 3 | the character after the `.` starts an identifier |
| 4 | after optional whitespace, the next character is `(` |
| 5 | a matching `)` exists — the search skips strings |

If steps 1–4 fail, the text is copied through silently. If step 5 fails, the body is `DSH6314`. The scan
skips comments, strings and character literals, so a `UE.Time()` inside a comment is not lifted.

The extracted text is parsed as a Graph expression, so any [`UE.*` builtin](../builtins/ue.md) —
[`UE.Expression`](../builtins/ue-expression.md) included — is available. It is bound in the
**function's own scope** (since 2.0.0): its parameters, replaced at each call by that call's
arguments, and the file's file-scope declarations. A name only the caller has is `DSH6325` — 1.x
evaluated lifted calls in the caller's `Graph` scope; pass the value as a parameter. A name nobody has,
a variable the body itself declares included, is `DSH6326`.

### Generated pin names

| Step | Rule |
| :-- | :-- |
| 1 | the base name is `__ds_<FunctionName>_UE<N>`, `N` counting the body's lifted calls from 0 in source order — the same names at every call site |
| 2 | the whole name is identifier-sanitised and runs of underscores collapse, so the leading `__` becomes a single `_` |
| 3 | while it collides with a declared `in` parameter or an earlier lifted call (ignoring case), `_1`, `_2`, … is appended |

So a `GraphFunction WindPulse` whose body contains one `UE.Time()` call produces a pin literally named:

```text
_ds_WindPulse_UE0
```

and a namespaced `Common::Pulse` produces `_ds_Common_Pulse_UE0`. The declared `in` parameters take the
first pins, in declaration order; lifted pins follow. These names are user-visible on the generated
node and in any shader-compiler error that mentions them.

**Note:**

The lift consumes exactly `UE.Name( … )` up to the matching `)` — nothing more. A trailing swizzle or
member access stays behind as HLSL applied to the pin, so `UE.CameraVector().xy` becomes
`_ds_<Fn>_UE0 .xy` and is evaluated by the shader compiler; the pin carries the node's default output
at its full width. The pin name is padded with spaces to the call's length, so every later column of
the body still matches the source.

**Warning:**

**Only `UE.` is lifted.** A `Substrate.*` call is not, and reaches the shader compiler as text, where it
is not valid HLSL; the front end says so with the warning `DSH6316` (since 2.0.0). Substrate
values cannot cross a `Custom` node boundary at all — build Substrate graphs in a `ShaderFunction` or a
`Shader`'s `Graph`.

A lifted call has to produce something a `Custom` input carries: a number, a bool, or a texture object
(since 2.0.0). A `MaterialAttributes` value or a Substrate material is `DSH6329`. A node with
several outputs feeds the pin its default output.

```c
GraphFunction WindPulse(in float2 uv, out float pulse)
{
    float t = UE.Time();
    pulse = sin(uv.x * 8.0 + t);
}
```

One call generates:

```text
UMaterialExpressionTextureCoordinate   →  Custom pin "uv"
UMaterialExpressionTime                →  Custom pin "_ds_WindPulse_UE0"
UMaterialExpressionCustom  Description="WindPulse"  OutputType=CMOT_Float1
```

```hlsl
float pulse = (float)0;
// Begin DreamShader source: DShader/Lib/Motion.dsh
// DreamShader custom: WindPulse line 4

    float t = _ds_WindPulse_UE0;
    pulse = sin(uv.x * 8.0 + t);
// End DreamShader source: DShader/Lib/Motion.dsh
return pulse;
```

Additional `out` parameters are the node's additional outputs, named after the **declared** parameters
(since 2.0.0) (1.x named them after the caller's variables), and the body assigns them directly.
Plain `Function` calls in the body are embedded as in a `Function` node.

## `Namespace`

`Namespace` prefixes the names of the `Function` and `GraphFunction` declarations it contains. It is
not an entity: no object is stored for it and it creates **no scope**. A member's name is the flattened
identifier (since 2.0.0):

```text
Namespace(Name = "Common") { Function ApplyTint(…) }   →   Common::ApplyTint   →   function Common_ApplyTint
```

Name lookup, binder diagnostics and the helper symbol `DreamShaderFn_Common_ApplyTint` all see
`Common_ApplyTint`; the qualified spelling survives in one place — the title (`Description`) of the
member's `Custom` node is `Common::ApplyTint`, as in 1.x.

| Rule | Behaviour |
| :-- | :-- |
| `Name` | Must be an identifier (`DSH6309`). `Namespace(Name="A::B")` is an error — there is **no multi-segment form**. |
| Body | `Function` and `GraphFunction` only. A nested namespace, a section, an asset block or an `import` is `DSH6310`. An empty body is accepted. |
| Re-opening | Allowed and unchecked. Two `Namespace(Name="Common")` blocks, in one file or across imported headers, both prefix `Common::`. |
| Lookup | The qualified name, or its flattened spelling `Common_ApplyTint`. There is no `using` and no unqualified fallback: a bare `ApplyTint(…)` is `DSH4208`. |
| Case | Names are case-sensitive (since 2.0.0). `common::applytint(…)` still resolves when it matches one function, with the warning `DSH5275`. |
| Missing name | `Common::` with nothing after it is `DSH5260`. |
| `Generate`, `Library`, `Description` | (after 2.1.2) In a package's header: every member generates a material function named by the `Generate` template (`MF_BL_{Name}`), with `Library` and `Description` as defaults. See [Packages ▸ Asset headers](../tools/packages.md#asset-headers). |
| From a `.dss` | (after 2.1.2) A `.dss` that includes the header calls a member as `Common::ApplyTint(…)` too. |

```c
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }

    Function float Remap01(in float value) {
        return saturate(value * 0.5 + 0.5);
    }

    GraphFunction Pulse(in float speed, out float value) {
        value = sin(UE.Time() * speed);
    }
}
```

```c
Graph = {
    vec3 Tinted;
    Common::ApplyTint(BaseColor, Tint, Tinted);   // statement call
    float K = Common::Remap01(Tinted.r);          // value call
}
```

**Warning:**

Because `::` and `_` flatten to the same thing, the member `ApplyTint` of `Namespace(Name="Common")`
and a top-level `Function Common_ApplyTint` declare the same name: `DSH4210`.

**Inside another `Function` or `GraphFunction` body**, `Ns::Fn(…)` now reaches the function
(since 2.0.0): body normalisation rewrites `Common::ApplyTint` to `Common_ApplyTint`, which is now
the function's own name, so the call is embedded like any other `Function` call. In 1.x it reached the
shader compiler as an undefined `Common_ApplyTint` and failed there. Write `::` without spaces around
it.

## Calls

Both call forms live in a `Graph` block.

| Form | Rule |
| :-- | :-- |
| `x = Fn(a, b);` — **value call** (since 1.3.1) | passes the inputs; its value is the function's first output — the return value, or the first `out`. A function with several outputs may be called this way too (since 2.0.0), its other outputs left unconnected; `Fn(a, b).Out` or `Fn(a, b, Output = "Out")` selects another. |
| `Fn(a, b, OutX, OutY);` — statement call | the inputs, then one receiver per output, in declaration order. A receiver must be a variable (`DSH4239`) of the output's type (`DSH4218`); one nobody declared is declared there, with the info `DSH5283`. |

Too many arguments are `DSH4224`; an input left out is `DSH4217`. Named arguments are accepted now
(since 2.0.0): they name a parameter (`DSH4216` for a name it does not have), and a call that
uses them is matched by name and declaration order rather than as inputs-then-receivers. The mangled
spelling `DreamShaderFn_Luma(c)` no longer names the function (since 2.0.0): `DSH4208`.

Full argument rules, `default` and output selectors: [Calls](../graph/calls.md).

## Notes

- **One name declares one thing.** Two functions of one name — in a file, or in a file and a header it
  imports — are `DSH4210`; `A::B` and `A_B` are the same name. A `Function` and a `GraphFunction` of one
  name are `DSH4210` too (since 2.0.0) (1.x reported the call as ambiguous). A function named like
  a builtin (`lerp`, `saturate`, …) is `DSH6206`. There is no overloading.
- **Names are case-sensitive** (since 2.0.0). `Luma` and `luma` are two functions; a `Graph` call
  that matches one only in case still resolves when the match is unique, with the warning `DSH5275`.
- **Headers are parsed on their own.** An imported `.dsh` is not pasted into the file; its functions
  are declared into it. See [Imports and Namespaces](import.md).
- A `.dsm` or `.dsf` holding only `Function` / `GraphFunction` blocks compiles and produces nothing.
- On UE 5.3 the generated `Custom` nodes show their code in the material graph; from UE 5.4 on
  `ShowCode` is set to `false`.

## Example

```c
// DShader/Lib/Color.dsh

Function float Luma(in vec3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

Function Remap01(in float value, out float result)
{
    result = saturate(value * 0.5 + 0.5);
}
```

```c
import "Lib/Color.dsh";

Shader(Name="Materials/M_Tinted")
{
    Properties = {
        TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_Base");
        vec3                     Tint    = vec3(1.0, 0.6, 0.2);
    }
    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }
    Graph = {
        vec2 UV  = UE.TexCoord(Index = 0);
        vec4 Tex = BaseTex(Coordinates = UV);

        float Key;
        Remap01(Luma(Tex.rgb), Key);      // statement call with a nested value call

        Color = Tex.rgb * Key;
    }
}
```

Each call is a `Custom` node. The `Luma` node's code — note the body's own `return`:

```hlsl
// Begin DreamShader source: DShader/Lib/Color.dsh
// DreamShader custom: Luma line 4

    return dot(color, float3(0.299, 0.587, 0.114));
// End DreamShader source: DShader/Lib/Color.dsh
```

## Diagnostics

| Message | Cause | Fix |
| --- | --- | --- |
| `DSH6300: Expected a function name after '{0}', found '('.` | Function or GraphFunction (with its optional modifier and return type) followed by ( where the name should be — Function SelfContained( and the section-style Function Name { land here too. | Write Function float3 MyFunction(in float3 value) { ... }. |
| `DSH6319: Expected '(' to open the parameter list of '{0}', found {1}.` | No ( after the name, or no { after the parameter list. | Write the parameter list, empty if need be, and the body: Function float Pi() { return 3.14159; }. |
| `DSH6301: Expected '[in\|out] Type Name' in the parameter list of '{0}', found '{1}'.` | A parameter that is not [in\|out] Type Name. 1.x parameters have no defaults and no metadata. | Write each parameter as in float3 color or out float result. |
| `DSH6302: Expected 'in' or 'out' before the parameter '{0}' of '{1}', found '{2}'; a 1.x function has no 'inout'.` | A parameter carries inout, or another word where in / out goes. | Split it into an in and an out parameter. |
| `DSH6303: Expected a parameter name other than '__return', which 1.x reserved, in '{0}'.` | A parameter called __return. | Rename the parameter. |
| `DSH6304: Expected either a return type or 'out' parameters on '{0}', found both.` | A return type and out parameters. | Return the first result and keep no out, or drop the return type and make every result an out. |
| `DSH6305: Expected '{0}' to return a value or to have at least one 'out' parameter, found neither.` | Neither a return type nor an out parameter, so the node has no value for the graph to read. | Give it a return type or an out parameter. |
| `DSH6306: 'Inline' is the old spelling of 'SelfContained', and both read the same: the function's node embeds the functions it calls.` | Function Inline is used (warning). Both spellings read the same as no modifier at all. | Drop the modifier, or write SelfContained; nothing changes either way. |
| `DSH6307: Expected a return type or a name after 'GraphFunction', found the modifier '{0}', which only a Function takes.` | GraphFunction followed by SelfContained or Inline. | Remove the modifier. |
| `DSH6308: Expected a value after 'return' in '{0}', which returns '{1}', found a bare 'return;'.` | A top-level bare return; in a function with a return type. | Return a value on every path. |
| `DSH6256: '{0}' returns void but its body uses 'return;'; a custom node always returns its first output, so give the function a return type or restructure the body.` | A bare return; at any depth in a function without a return type. | Give the function a return type and return a value, or turn the early exit into an if. |
| `DSH6257: '{0}' declares a return type but its body never returns a value; the node's first output will be 0.` | A return type and no return anywhere in the body (warning). | Return the value the signature promises. |
| `DSH6258: '{0}' has an '#include' with an empty path; write the virtual shader path the header lives at, for example "/Engine/Private/Common.ush".` | A leading #include with an empty path. | Write the virtual shader path of the header. |
| `DSH6260: The '@custom' functions {0} call each other in a cycle; HLSL has no recursion, so their bodies cannot be embedded in a custom node.` | Bodies that call each other in a cycle. | Pull the shared part into a third function that calls nobody. |
| `DSH6261: '{0}' is not a '@custom' function and cannot be called from the HLSL body of '{1}'; a custom node sees no graph values, so mark '{0}' '@custom' as well or move the call out of the body.` | A body calls something that is not a Function — a ShaderFunction or a VirtualFunction. | Call it from the Graph and pass the result in as a parameter. |
| `DSH6262: '{0}' takes {1} argument(s) but this call passes {2}; a call that carries a texture cannot be matched up by position otherwise.` | A call to a texture-taking function with the wrong number of arguments. | Pass exactly one argument per parameter. |
| `DSH6263: The texture argument for '{0}' of '{1}' has to be a plain texture name, because the sampler that goes with it is named after it.` | An argument in a texture position that is not a plain name. | Assign the texture to a name first. |
| `DSH6255: '{0}' declares '{1}' twice in the HLSL it generates; a texture parameter also claims '{1}Sampler', which the engine declares alongside it.` | Two parameters end up with one HLSL name — usually a parameter called &lt;Texture&gt;Sampler. | Rename one of them. |
| `DSH6327: '{0}' takes the 'UE.' calls lifted out of its body as inputs of its own custom node, so the HLSL body of '{1}' cannot call it; call it from a graph body instead.` | A body calls a GraphFunction that lifts UE.* calls. | Call it from a Graph and pass the result in, or turn the UE. calls into parameters. |
| `DSH6259: '{0}' differs from the function '{1}' only in case; HLSL is case-sensitive, so this call is left for the shader compiler. Did you mean '{1}'?` | A call in a body matches a function only in case (warning). | Spell the name as declared. |
| `DSH6314: Expected a ')' to close the 'UE.' call in the body of '{0}', found the end of the body.` | A UE.*( in a body with no matching ). | Close the call. |
| `DSH6316: A 'Substrate.' call in the body of '{0}' is not lifted into a node, because no custom node input carries a Substrate value; it reaches the shader compiler as text.` | A Substrate.* call in a body (warning). | Build the Substrate node in a Graph and keep only numbers in the HLSL. [Details](../builtins/substrate.md) |
| `DSH6325: '{0}' lifts a 'UE.' call out of its body that reads '{1}', which 1.x took from the caller's scope and 2.0 does not; pass '{1}' to '{0}' as a parameter.` | A lifted call reads a variable of the caller. | Add a parameter for the value and pass it at the call. |
| `DSH6326: '{0}' lifts a 'UE.' call out of its body that reads '{1}', which is neither a parameter of '{0}' nor declared at file scope.` | A lifted call reads a name that is neither a parameter nor file-scope — a variable the body declares included. | Pass the value in as a parameter. |
| `DSH6329: '{0}' carries 'UE.' calls lifted out of its body, and only a '@custom' function with a verbatim body lifts calls into its node's inputs.` | A lifted call makes a MaterialAttributes or Substrate value, which no Custom input carries. | Build that node in a Graph instead. |
| `DSH6330: '{0}' reaches itself through the 'UE.' calls lifted out of its body, and each call makes a new custom node, so the graph would never end.` | A lifted call's arguments call the function again. | Compute the inner value in the Graph and pass it in. |
| `DSH6210: '{0}' is '@custom', so '{1}' becomes an input pin of a Custom node, and a Custom node cannot take a material; pass the fields it needs instead.` | MaterialAttributes as an input. | Pass the fields the body needs; MaterialAttributes is fine as a result. [Details](types.md) |
| `DSH6253: '{0}' uses Substrate on '{1}'; a custom node has no Substrate pins, so build that part of the material out of reflected Substrate nodes.` | Substrate in the signature. | Build the Substrate part in a ShaderFunction or a Shader's Graph. |
| `DSH6254: '{0}' returns a texture through '{1}'; a custom node output carries float1..4 or material attributes, never a texture object.` | A texture or sampler returned, or handed back through an out. | Sample inside the body and return the sampled value. |
| `DSH6206: '{0}' is a builtin operation and cannot be redeclared; rename the function.` | A function named like a builtin operation (dot, lerp, saturate, …). | Rename the function. |
| `DSH4210: '{0}' is already declared in this file; one name declares one thing.` | Two declarations of one name — Ns::Fn against a top-level Ns_Fn, and a Function and a GraphFunction of one name, included. | Rename one of them. |
| `DSH4208: A value of type {0} has no method '{1}'.` | The called name declares nothing — usually an unqualified call to a namespace member, or the DreamShaderFn_ spelling. | Write Ns::Fn(…); names are case-sensitive. |
| `DSH5260: Expected a name after '::', found {0}.` | Nothing after the :: of a qualified call. | Write N::F(...). |
| `DSH4216: '{0}' takes its arguments in order and has no argument called '{1}'.` | A named argument names no parameter. | Correct the name, or pass the argument positionally. [Details](../graph/calls.md) |
| `DSH4217: '{0}' is missing its '{1}' argument.` | A call leaves an input out. | Pass it. |
| `DSH4224: '{0}' takes at most {1} arguments.` | Too many arguments. | Match the declaration. |
| `DSH4239: '{0}' is an out parameter of '{1}', so its argument has to be a variable.` | A receiver that is not a variable. | Declare a local of the parameter's type and pass that. |
| `DSH4218: '{0}' writes back {1}, and this variable is {2}; an out argument has to match exactly.` | A receiver whose type differs from the output's. | Declare the variable with the exact type, then convert after the call. |
| `DSH5283: '{0}' is not declared, and as in 1.x it is declared here as a local of type {1} receiving '{2}' of '{3}'.` | A statement call's receiver was never declared; as in 1.x, it is declared on the spot (info). | Nothing has to change; dsc migrate writes the declaration out. |
| `DSH6310: Expected only Function and GraphFunction blocks inside the namespace '{0}', found {1}.` | Anything else inside a Namespace body — a nested Namespace or an import included. | Move the other block out of the namespace. |

## Which one to use

| Situation | Use |
| :-- | :-- |
| Reusable pure math | `Function`, in a `.dsh` |
| Reusable logic that needs real material nodes | `GraphFunction` |
| A reusable asset other materials call | `ShaderFunction`, in a `.dsf` |
| A `UMaterialFunction` that already exists | `VirtualFunction` |
| `MaterialAttributes` inputs or Substrate plumbing | `ShaderFunction` — never `Function` |
| A public library API | stable names in a `Namespace`, shipped as a package |

## Where to next

  - [Calls](../graph/calls.md) — Every argument form, default and output selectors.
  - [Imports and Namespaces](import.md) — Bringing a helper header into the file that calls it.
  - [UE.* Nodes](../builtins/ue.md) — Everything a GraphFunction body can lift.
  - [Types and Values](types.md) — Signature type tokens and their HLSL spellings.
