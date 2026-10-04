# Function

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Function**

A top-level block that declares a reusable HLSL helper: the body is HLSL text, kept verbatim, and
every call site becomes one `UMaterialExpressionCustom` node whose code is that body.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` — and inside a [`Namespace`](namespace.md) body |
| Kind | top-level block |
| Generates | no asset and no file. Each call site is one Custom node; its code holds the body and every other `Function` the body calls *(since 2.0.0: no `.ush` include is written)* |
| Multiplicity | any number per file; one name declares one thing |

The legacy front end reads a `Function` as a `/// @custom` function of the [2.0 language](../language-v2/index.md)
(`/// @custom selfcontained` with the modifier); [`dsc migrate`](../tools/migrate.md) writes it that way.

## Synopsis

```c
Function [ { Inline | SelfContained } ] [ <return-type> ] <name> ( [ <parameter-list> ] )
{
    <hlsl>
}
```

```c
<parameter-list> ::= <parameter> [ , <parameter> ] …
<parameter>      ::= [ { in | out } ] <type-token> <parameter-name>
```

The keyword `Function` is matched **case-sensitively**. `function`, `FUNCTION` and `Functions` are not
the keyword: the file reports [`DSH2240`](../diagnostics/DSH2xxx.md#dsh2240), or
[`DSH2248`](../diagnostics/DSH2xxx.md#dsh2248) when what follows reads as a 2.0 declaration. See
[Lexical elements](lexical.md#case-sensitivity).

Everything else in the declaration — the `Inline` / `SelfContained` modifier, the `in` / `out`
qualifiers, and every type token — is matched **case-insensitively**.

There is no `(Key = Value)` header, no `Settings`, and no sections of any kind. The `{ … }` after the
parameter list is raw HLSL, captured verbatim: only its braces are read, and a brace inside a string
or a comment does not count.

## Declaration order

The parser reads identifiers left to right and disambiguates by lookahead:

| Form | Read as |
| :-- | :-- |
| `Function Name(…)` | name |
| `Function Type Name(…)` | return type, then name |
| `Function SelfContained Name(…)` | modifier, then name |
| `Function SelfContained Type Name(…)` | modifier, return type, then name |
| `Function Type SelfContained Name(…)` | return type `Type`, name `SelfContained`, then a stray `Name` where `(` belongs — [`DSH6319`](../diagnostics/DSH6xxx.md#dsh6319) |

The rule is mechanical: after one identifier, a `(` makes it the name; another identifier makes it the
*return type* and the next identifier the name. The modifier is therefore fixed in first position.

## Modifiers

| Modifier | Accepted on | Effect |
| :-- | :-- | :-- |
| `SelfContained` | `Function` only | the body is taken exactly as written: no other `Function` it calls is embedded into its node — see [below](#inline--selfcontained-mode) |
| `Inline` | `Function` only | the old spelling of `SelfContained`: same effect, plus a warning ([`DSH6306`](../diagnostics/DSH6xxx.md#dsh6306)) |

Both spellings are case-insensitive. On a [`GraphFunction`](graph-function.md) either one is
[`DSH6307`](../diagnostics/DSH6xxx.md#dsh6307) *(since 2.0.0; 1.x read the word as a return type)*.

> [!WARNING]
> A function may not be **named** `Inline` or `SelfContained`. `Function SelfContained(in float a,
> out float b) { … }` takes the modifier branch, then finds `(` where the name should be:
> [`DSH6300`](../diagnostics/DSH6xxx.md#dsh6300).

## Parameters

A parameter is the words up to the next `,` or `)`. A word is an identifier or a keyword, so any
whitespace — tabs and line breaks included — only separates them. Two or three words are accepted:

| Words | Form | Qualifier |
| :-- | :-- | :-- |
| 2 | `<type> <name>` | defaults to `in` |
| 3 | `<qualifier> <type> <name>` | as written |
| fewer than 2, more than 3, or anything that is not a word (`[2]`, `= 1.0`) | — | `DSH6301` |

| Rule | Behaviour |
| :-- | :-- |
| Qualifiers | Only `in` and `out`, compared in lower case, so `In`, `OUT`, `iN` all work. |
| `inout` | **Does not exist** in a 1.x function: [`DSH6302`](../diagnostics/DSH6xxx.md#dsh6302). |
| Empty parameters | A lone `,` is skipped, so a trailing comma before `)` is tolerated. |
| Order | Parameters keep their declaration order. `in` parameters become the node's input pins; `out` parameters its outputs. The two may be interleaved. |
| Reserved name | A parameter named `__return` (ignoring case) is [`DSH6303`](../diagnostics/DSH6xxx.md#dsh6303). |
| Duplicate name | [`DSH4214`](../diagnostics/DSH4xxx.md#dsh4214). |
| Not available | `opt`, default values, and `[ … ]` metadata blocks are **not** part of this grammar (`DSH6301`). They belong to the [`Inputs` / `Outputs` sections](inputs-outputs.md) of `ShaderFunction` and `VirtualFunction`. |

### At least one output

A `Function` must produce something: **either** at least one `out` parameter **or** a return type.
Neither is [`DSH6305`](../diagnostics/DSH6xxx.md#dsh6305).

## Return type

| Form | Legal | Result |
| :-- | :-- | :-- |
| `Function Name(in T a, out U r)` | yes | one output, `r` |
| `Function Name(in T a, out U r1, out V r2)` | yes | `r1` is the node's first output, `r2` an additional output |
| `Function U Name(in T a) { return expr; }` | yes | the returned value is the node's output |
| `Function U Name(in T a, out V r)` | **no** | [`DSH6304`](../diagnostics/DSH6xxx.md#dsh6304) |
| `Function Name(in T a)` | **no** | `DSH6305` |
| `Function U Name(…) { return; }` | **no** | a top-level bare `return;` under a return type: [`DSH6308`](../diagnostics/DSH6xxx.md#dsh6308) |
| `Function Name(…, out U r) { … return; … }` | **no** | a `return;` at any depth in a function without a return type: [`DSH6256`](../diagnostics/DSH6xxx.md#dsh6256) *(since 2.0.0)* |

So: **a return type implies exactly one output, and forbids every explicit `out`.** Multiple outputs
require `out` parameters and no return type.

### `return` lowering

*(since 2.0.0)* `return` is no longer rewritten. 1.x replaced every top-level `return` with an
assignment to a synthetic `__return` result; 2.0 keeps the body's `return` statements as written,
because the body now *is* the body of the HLSL function that returns the node's first output — the
Custom node's own, or the helper's when another body embeds it. A `return expr;` therefore returns the
value at any brace depth; the nested case that bypassed `__return` in 1.x behaves like the top-level
one.

What is still checked: a top-level bare `return;` under a return type (`DSH6308`), a `return;`
anywhere in a function without one (`DSH6256`), and a return type whose body contains no `return` at
all, which is a warning that the output will be 0
([`DSH6257`](../diagnostics/DSH6xxx.md#dsh6257)). A body that is one bare expression is given its
`return` instead. The name `__return` stays reserved (`DSH6303`).

## Type tokens

The 15 GLSL aliases (`vec2`…`vec4`, `ivec*`, `uvec*`, `bvec*`, `mat2`…`mat4`) are normalised to their
HLSL spellings at parse time, on the return type and every parameter type, and a scalar or vector
spelled in another case (`Float3`) is read as its lower-case spelling. A token that names no type is
[`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201) at the declaration *(since 2.0.0; 1.x only failed when
the function was called)*. The full alias list is in [Type tokens](types.md).

| Token(s) | Valid as `in` | Valid as `out` / return |
| :-- | :-- | :-- |
| `float`, `half`, `int`, `uint`, `bool`, their vectors `…2`–`…4`, and the GLSL aliases | yes | yes — a node output is `Float1`…`Float4` |
| `MaterialAttributes` | **no** — [`DSH6210`](../diagnostics/DSH6xxx.md#dsh6210), and [`DSH6252`](../diagnostics/DSH6xxx.md#dsh6252) at the node | yes |
| `StaticBool`, `StaticBoolParameter` | yes — read as `bool` | yes — read as `bool` |
| `Texture2D`, `TextureCube`, `Texture2DArray`, `Texture3D`, `VolumeTexture` | yes | **no** — [`DSH6254`](../diagnostics/DSH6xxx.md#dsh6254) |
| `SamplerState` | a sampler *(since 2.0.0; 1.x read it as a Texture2D)* | **no** — `DSH6254` |
| `Substrate` | **no** — [`DSH6253`](../diagnostics/DSH6xxx.md#dsh6253) | **no** — `DSH6253` |
| matrices (`float3x3`, `mat3`, …) | not across a node pin: [`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361) at a graph call | same |

The generated HLSL spells every type the 2.0 way: `vec3` is `float3`, `MaterialAttributes` is
`FMaterialAttributes`, and `VolumeTexture` is `Texture3D`.

> [!NOTE]
> `Substrate` is never usable on a `Function` or a [`GraphFunction`](graph-function.md): a Custom node
> has no Substrate pins in any engine version. Build the Substrate part in a
> [`ShaderFunction`](shader-function.md) or in the `Shader`'s `Graph`. See
> [Substrate builtins](../builtins/substrate.md).

### Texture parameters and samplers

Five tokens are *texture parameters*: `Texture2D`, `TextureCube`, `Texture2DArray`, `Texture3D`,
`VolumeTexture` (case-insensitive).

| Where | What happens |
| :-- | :-- |
| On the node | a texture `in` parameter is one pin carrying the texture object; the engine declares `SamplerState <Name>Sampler` beside it. Use that name in the body. |
| In an embedded helper | its HLSL signature gains `SamplerState <Name>Sampler` right after each texture parameter, and every call to it from another body passes `<argument>Sampler` after the texture argument. |
| That call | has to pass every argument ([`DSH6262`](../diagnostics/DSH6xxx.md#dsh6262)), and its texture argument has to be a plain name ([`DSH6263`](../diagnostics/DSH6xxx.md#dsh6263)). |
| A parameter called `<Texture>Sampler` | collides with the paired name: [`DSH6255`](../diagnostics/DSH6xxx.md#dsh6255). |

`SamplerState` is **not** a texture parameter and receives no companion.

*(since 2.0.0)* `<Texture>.Sample(UV)` on a texture parameter — and `SampleLevel`, `SampleBias`,
`SampleGrad` with one argument fewer than HLSL's own form — is rewritten to the engine's
`Texture2DSample(<Texture>, <Texture>Sampler, UV)` family. The native HLSL spellings stay as written.

## Body normalisation

Every `Function` and `GraphFunction` body — and nothing else in the language — is passed through an
identifier-level rewrite before it is stored. The scan skips comments and `"…"` strings. Two things
happen:

1. **Alias substitution.** Whole identifiers are matched *case-insensitively* against 18 entries: the
   15 GLSL type aliases plus `mix` → `lerp`, `fract` → `frac`, `mod` → `fmod`.
2. **Qualified-name flattening.** A token of the form `A::B` (repeatable, no spaces around `::`) is
   replaced by the identifier-sanitized spelling of the whole thing — `Common::Remap01` becomes
   `Common_Remap01`.

> [!WARNING]
> Because the alias match is case-insensitive and applies to whole identifiers anywhere in the body,
> a helper of your own called `Mix`, `Mod`, `Fract`, `Vec3`, `Mat4` — or a *variable* by one of those
> names — is silently renamed in the emitted HLSL. There is no diagnostic: the `DSH5277` warning for a
> GLSL spelling applies to `Graph` text, not to a body. Rename the identifier, or spell it so it does
> not collide (`MixColor`, `ModValue`).

*(since 2.0.0)* The flattened `Common_Remap01` is exactly the name a namespaced function has, so a
`Ns::Fn(…)` call inside a body now reaches that function. See
[Namespace](namespace.md#calling-a-namespaced-function).

## Includes

`#include` directives written at the **top** of the body — only whitespace and comments may come
before them — are not part of the function. They are blanked out of the body (overwritten with spaces,
so every later line and column still matches the source) and *hoisted* onto the include list
(`IncludeFilePaths`) of every Custom node whose code holds the body: the function's own nodes, and the
node of any other `Function` that embeds it, the embedded functions' includes first.
*(since 2.0.0: there is no generated include to hoist into.)*

```c
Function float3 SampleWind(in float3 WorldPos)
{
    #include "/Plugin/DreamWind/Shared/DreamWindShared.h"   // hoisted
    FDreamWindSample S = DW_SampleAnalytic(DW_UnpackParams(P0, P1, P2), WorldPos);
    return S.Velocity;
}
```

Both `#include "…"` and `#include <…>` are accepted; the path is stored as written, delimiters
removed, and must be a virtual shader path the engine can resolve (`/Engine/…`, `/Plugin/<Name>/…`).
The engine puts a node's includes at file scope, so a header that defines functions works. Duplicates
on one node collapse to one entry. An empty path is
[`DSH6258`](../diagnostics/DSH6xxx.md#dsh6258); an unterminated one is left in the body for the
shader compiler. Comments before the directives stay in the body.

The scan stops at the first token that is not a leading `#include`: a directive that follows a
statement is **not** hoisted — it lands inside the generated function, which only works for headers
made of macros. See [DreamShaderBuiltins.ush](../builtins/hlsl-library.md#notes).

## Generated HLSL

*(since 2.0.0)* A call site's Custom node carries all of its HLSL in its own `Code`. The shape:

```hlsl
struct generated_wrapper_<Name>_<CRC32 %08X>        // only when the body calls other Functions
{
	<Type0> DreamShaderFn_<Callee>(<params>)          // one member per embedded Function,
	{                                                 // a callee before its caller
		<Out1> = (<Type1>)0;                          // every `out` but the first
		<Type0> <Out0> = (<Type0>)0;                  // a function without a return type
		// Begin DreamShader source: <file>
		// DreamShader custom: <Callee> line <N>
		<callee body, verbatim>
		// End DreamShader source: <file>
		return <Out0>;
	}
};
generated_wrapper_<Name>_<CRC32 %08X> __ds_wrapper_<CRC32 %08X>;

<Type0> <Out0> = (<Type0>)0;                          // a function without a return type
// Begin DreamShader source: <file>
// DreamShader custom: <Name> line <N>
<body, verbatim; calls to embedded functions go through __ds_wrapper_<CRC32 %08X>>
// End DreamShader source: <file>
return <Out0>;                                       // only when the body has no top-level return
```

| Element | Rule |
| :-- | :-- |
| Symbol name | `DreamShaderFn_` + the function's name with every non-`[A-Za-z0-9_]` character replaced by `_` and runs of underscores collapsed. A namespaced `Common::ApplyTint` is the function `Common_ApplyTint`, so `DreamShaderFn_Common_ApplyTint`. |
| Why the prefix | An unprefixed `Luminance(float3)` would shadow the engine intrinsic from `/Engine/Private/Common.ush` when a sibling member calls it. |
| A helper's return value | its return type; for a function without one, its first `out` parameter, which is why one body may call a multi-output `Function` as a value. |
| Helper parameters | declaration order; a `SamplerState <Name>Sampler` after each texture parameter; every `out` but the first as `out <Type> <Name>`, zero-initialised. |
| The node's own body | not wrapped in a function of DreamShader's: it is the Custom node's code. Its first output is the returned value, or a local for the first `out`; every other `out` is an additional output, which the engine declares and zero-initialises. |
| Markers | the `Begin` / `custom` / `End` comment lines around each body let a shader-compile error be mapped back to its source line; no rewrite adds or removes a line inside a body. |
| Return | `return 0.0;` (or the first `out`) is appended only when the body has no top-level `return`; a body that is one bare expression is wrapped in `return …;`. |

The node's title (its `Description`) is the function's name — `Ns::Fn` for a member of a
[`Namespace`](namespace.md). The [Generated HLSL](../generation/generated-hlsl.md) page describes the
`.ush` include 1.x wrote instead.

## `Inline` / `SelfContained` mode

*(since 2.0.0)* Every call site's node holds the function's own body and, **by default**, every other
`Function` that body calls, transitively, as members of the `generated_wrapper_*` struct — HLSL has no
nested functions, and a Custom node's code is a function body. 1.x did this only for a
`SelfContained` function and otherwise referenced its shared include.

`SelfContained` (and `Inline`) now means: the body is taken exactly as written, and nothing is
embedded into it. A call it makes to another `Function` is left as text with a warning
([`DSH6264`](../diagnostics/DSH6xxx.md#dsh6264)); it compiles only if one of the body's own
`#include`s defines that name.

| Aspect | Default | `Inline` / `SelfContained` |
| :-- | :-- | :-- |
| Where the body lives | in the code of every node that calls it, and as a wrapper member in every node whose body calls it | same |
| Other `Function`s its body calls | embedded, a callee before its caller | not embedded — `DSH6264` |
| `IncludeFilePaths` | the leading includes of the body and of every embedded function | the body's own |
| A call from the node's body to an embedded function | `__ds_wrapper_<CRC>.DreamShaderFn_<Name>(…)` | — |
| Sibling calls inside the wrapper | plain `DreamShaderFn_*`, with no `this.` qualifier | — |
| Recursion | [`DSH6260`](../diagnostics/DSH6xxx.md#dsh6260) | nothing is followed |

Both CRC32 values hash the calling function's name. A call from a body to something that is not a
`Function` — a `ShaderFunction` or a `VirtualFunction` — is
[`DSH6261`](../diagnostics/DSH6xxx.md#dsh6261); to a [`GraphFunction`](graph-function.md) with lifted
`UE.*` calls, [`DSH6327`](../diagnostics/DSH6xxx.md#dsh6327). A call that matches a function only in
case is a warning ([`DSH6259`](../diagnostics/DSH6xxx.md#dsh6259)) and is left for the shader compiler.

## Calling a Function

Both call forms live in a [`Graph`](../graph/index.md) block and are specified in
[Calling functions](../graph/calls.md). In summary:

| Form | Rule |
| :-- | :-- |
| `x = Fn(a, b);` — value call *(since 1.3.1)* | passes the inputs; its value is the function's first output — the return value, or the first `out` (legacy rule L3b). *(since 2.0.0)* also on a function with several outputs, whose other outputs are left unconnected; `Fn(a, b).Out` or `Fn(a, b, Output = "Out")` selects another. |
| `Fn(a, b, OutX, OutY);` — statement call | the inputs, then one receiver per output, in declaration order (legacy rule L5). A receiver must be a variable ([`DSH4239`](../diagnostics/DSH4xxx.md#dsh4239)) of the output's type ([`DSH4218`](../diagnostics/DSH4xxx.md#dsh4218)); one nobody declared is declared there ([`DSH5283`](../diagnostics/DSH5xxx.md#dsh5283)). |

Too many arguments are [`DSH4224`](../diagnostics/DSH4xxx.md#dsh4224); an input left out is
[`DSH4217`](../diagnostics/DSH4xxx.md#dsh4217). *(since 2.0.0)* Named arguments name a parameter
([`DSH4216`](../diagnostics/DSH4xxx.md#dsh4216) for a name it does not have), and a call that uses
them is matched by name and declaration order rather than as inputs-then-receivers. The mangled
spelling `DreamShaderFn_Luma(c)` no longer names the function *(since 2.0.0)*:
[`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208).

## Notes

- **One name declares one thing.** Two functions of one name — in a file, or in a file and a header it
  imports — are [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210); so are `A::B` and `A_B`, which are
  the same name. A function named like a builtin (`lerp`, `saturate`, …) is
  [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206).
- **Names are case-sensitive** *(since 2.0.0)*. `Luma` and `luma` are two functions. A `Graph` call
  that matches a function only in case still resolves when the match is unique, with
  [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275).
- **Headers are parsed on their own.** An imported `.dsh` is not pasted into the file; its functions
  are declared into it. See [import](import.md).
- A `.dsm` or `.dsf` containing only `Function` blocks is accepted and produces nothing.
- On UE 5.3 the generated Custom nodes display their code in the material graph; from UE 5.4 onward
  `ShowCode` is set to `false` on every generated Custom node.
- The legacy section-style body `Function Name { Inputs = { … } Code = { … } }` is **not** a supported
  form. A `{` where the parameter list's `(` should be is `DSH6300`.

## Diagnostics

Every diagnostic carries the line and column of the construct it is about. The tables say when each
code is raised; the code's page has the message.

### Parse time

| Code | Raised when |
| :-- | :-- |
| `DSH6300` | no name after `Function`, the modifier or the return type — including `Function SelfContained(` and the section-style `Function Name {` |
| `DSH6306` | `Inline` is used (warning) |
| `DSH6319` | no `(` after the name, or no `{` after the parameter list |
| `DSH6301` | a parameter is not `[in\|out] Type Name` |
| `DSH6302` | a qualifier other than `in` / `out`, `inout` included |
| `DSH6303` | a parameter is named `__return` |
| `DSH6304` | a return type and an `out` parameter |
| `DSH6305` | neither a return type nor an `out` parameter |
| `DSH6308` | a top-level bare `return;` under a return type |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | the parameter list or the body is never closed |

### Binding

| Code | Raised when |
| :-- | :-- |
| `DSH4201` | a type token names no type |
| `DSH4210` | the name is declared twice |
| `DSH4214` | two parameters share a name |
| `DSH6206` | the name is a builtin's |
| `DSH6210` | a `MaterialAttributes` input |

### Custom-node code

| Code | Raised when |
| :-- | :-- |
| `DSH6252` | a `MaterialAttributes` input on the node's own function |
| `DSH6253` | `Substrate` in the signature |
| `DSH6254` | a texture or sampler returned, or handed back through an `out` |
| `DSH6255` | two parameters end up with one HLSL name — usually `<Texture>Sampler` |
| `DSH6256` | a `return;` in a function without a return type |
| `DSH6257` | a return type, and no `return` in the body (warning) |
| `DSH6258` | a leading `#include` with an empty path |
| `DSH6259` | a call in the body matches a function only in case (warning) |
| `DSH6260` | functions whose bodies call each other in a cycle |
| `DSH6261` | the body calls a function that is not a `Function` |
| `DSH6262` | a call to a texture-taking function passes the wrong number of arguments |
| `DSH6263` | a texture argument of such a call is not a plain name |
| `DSH6264` | a `SelfContained` body calls another `Function` (warning) |
| `DSH6327` | the body calls a `GraphFunction` with lifted `UE.*` calls |

### Call time

| Code | Raised when |
| :-- | :-- |
| `DSH4208` | the called name declares nothing |
| `DSH5275` | the called name matches only in case (warning) |
| `DSH4216` | a named argument names no parameter |
| `DSH4217` | an input is not passed |
| `DSH4224` | too many arguments |
| `DSH4239` | a receiver is not a variable |
| `DSH4218` | a receiver's type does not match its output |
| `DSH5283` | an undeclared receiver is declared (info) |
| `DSH4361` | a matrix crosses a node pin |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | the emitter could not create the Custom node |

`{Name}` in the messages is the function's name; for a member of a [`Namespace`](namespace.md) see
[that page](namespace.md#notes).

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
// DShader/Lib/Color.dsh

Function float Luma(in vec3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

Function SelfContained Remap01(in float value, out float result)
{
    result = saturate(value * 0.5 + 0.5);
}

Function SampleTinted(in Texture2D tex, in vec2 uv, in vec3 tint, out vec3 rgb, out float alpha)
{
    float4 texel = Texture2DSample(tex, texSampler, uv);
    rgb   = texel.rgb * tint;
    alpha = texel.a;
}
```

Used from a `Shader`:

```c
import "Lib/Color.dsh"

Shader(Name="Materials/M_Tinted")
{
    Properties = {
        Texture2D BaseTex = Path(Game, "Textures/T_Base");
        vec3      Tint    = vec3(1.0, 0.6, 0.2);
    }
    Outputs = {
        vec3  Color;
        float Alpha;
        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }
    Graph = {
        vec2  UV = UE.TexCoord(Index = 0);
        vec3  Rgb;
        float A;
        SampleTinted(BaseTex, UV, Tint, Rgb, A);

        float Key;
        Remap01(Luma(Rgb), Key);

        Color = Rgb * Key;
        Alpha = A;
    }
}
```

Each call is a Custom node. The `Luma` node's code — note the body's own `return`:

```hlsl
// Begin DreamShader source: DShader/Lib/Color.dsh
// DreamShader custom: Luma line 4

    return dot(color, float3(0.299, 0.587, 0.114));
// End DreamShader source: DShader/Lib/Color.dsh
```

The `SampleTinted` node: pins `tex`, `uv`, `tint`; output `rgb` (`Float3`) and the additional output
`alpha` (`Float1`). Its code declares and returns the first `out`, and reads the sampler the engine
declares beside `tex`:

```hlsl
float3 rgb = (float3)0;
// Begin DreamShader source: DShader/Lib/Color.dsh
// DreamShader custom: SampleTinted line 14

    float4 texel = Texture2DSample(tex, texSampler, uv);
    rgb   = texel.rgb * tint;
    alpha = texel.a;
// End DreamShader source: DShader/Lib/Color.dsh
return rgb;
```

## See also

- [GraphFunction](graph-function.md) — the variant whose `UE.*` calls become real material nodes
- [Namespace](namespace.md) — `Ns::Fn` qualification and the flattened `Ns_Fn` name
- [Calling functions](../graph/calls.md) — value vs statement calls, argument rules
- [The 2.0 language](../language-v2/index.md) — `/// @custom`, which a `Function` is read as
- [Generated HLSL](../generation/generated-hlsl.md) — the `.ush` include 1.x wrote for `Function` blocks
- [Type tokens](types.md) — the complete token catalogue and the GLSL alias tables
- [Inputs / Outputs / Results](inputs-outputs.md) — the *section* parameter grammar, with `opt` and metadata
- [ShaderFunction](shader-function.md) — the block that generates a real `UMaterialFunction` asset
- [Source files](source-files.md) — which of `.dsm` / `.dsh` / `.dsf` may hold a `Function`
- [Lexical elements](lexical.md) — comments, identifiers, and the case-sensitivity matrix
- [HLSL library](../builtins/hlsl-library.md) — helpers available inside a `Function` body
- [Substrate builtins](../builtins/substrate.md) — why `Substrate` is a `GraphFunction`/`ShaderFunction` concern
- [Diagnostics index](../diagnostics/index.md) — every code, by stage
