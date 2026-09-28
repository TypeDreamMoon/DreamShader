# DreamShaderLang 2.0 — what exists today

> [DreamShader](../index.md) » **DreamShaderLang 2.0**

> [!IMPORTANT]
> **One compiler builds everything.** There is one pipeline -- preprocess → parse →
> bind → lower to a graph IR → run the passes → validate → emit -- and two front ends in front of it:
> a `.dss` is read by the 2.0 parser, and a `.dsm` / `.dsf` by a **legacy front end** that reads the
> [1.x language](../language/index.md) into the same tree. The 1.x generator is gone; what it built is
> reproduced node for node (68 of 68 real sources, measured against frozen 1.x graph dumps).
>
> New with it: **material instances as source** ([`.dsi`](instances.md)), a **decompiler that writes
> `.dss`**, and **[`dsc migrate`](../tools/migrate.md)**, which rewrites a 1.x file as 2.0 and proves
> the rewrite before it writes anything.

DreamShaderLang 2.0 drops the 1.x section blocks (`Shader`, `Properties`, `Outputs`, `Graph`, …) and
writes a material as what it always was underneath: **HLSL with declarations**. Metadata that used
to need a section now rides on `///` doc comments and `#pragma` lines, so a `.dss` file is one
source with no side files and no strip step.

| | |
| :-- | :-- |
| Extensions | `.dss` — a 2.0 compilation unit · `.dsi` — a [material instance](instances.md) · `.dsh` — a shared header, which may hold both dialects |
| 1.x extensions | `.dsm` / `.dsf` — read by the legacy front end, built by the same compiler |
| Modules | [`DreamShaderLang`](../api/lang-module.md) (`Core` only) — both front ends, binder, IR, decompile and migrate · `DreamShaderCompiler` (editor) — pipeline, emitter, assets |
| Tests | `DreamShader.Lang2.*`, `DreamShader.Compiler2.*` |
| Status | released in `2.0.0`: both front ends, binder, IR, passes, validator, emitter, node ↔ source navigation, `.dsi`, the 2.0 decompiler, `dsc migrate`, the Substrate sugar, the IR graph layouts, `dsc fmt` |

## Two short examples

A material — one `export void <Name>(inout material m)` is the entry, and the signature is the only
thing that declares it:

```hlsl
// M_TeleportGlow.dss
#pragma material(ShadingModel = Unlit, BlendMode = Additive)

/// @group Glow|Look   @desc Multiplied on top of the particle colour
uniform float4 Tint = float4(1, 1, 1, 1);

/// @group Glow|Look   @desc Overall emissive gain
uniform float Intensity = 0.7;

float GlowMask(float2 UV)
{
    float2 P = UV * 2.0 - 1.0;
    return pow(saturate(1.0 - dot(P, P)), 2.4);
}

export void M_TeleportGlow(inout material m)
{
    float2 UV = UE.TexCoord(Index = 0);
    m.EmissiveColor = Tint.rgb * GlowMask(UV) * Intensity;
}
```

A material function — extra outputs are HLSL `out` parameters, optional inputs are default
arguments, and `extern` binds a prototype to an asset that already exists:

```hlsl
// MF_ToonUV.dss
/// @asset /MoonToon/MaterialFunctions/Utils/MF_UVChannelSwitch
extern float2 MF_UVChannelSwitch(float UVChannelIndex);

/// @library MoonToon|Shared
/// @param UVChannel    Which UV channel to read.
export float2 MF_ToonUV(float UVChannel = 0.0, float4 ScaleOffset = float4(1, 1, 0, 0))
{
    float2 Selected = MF_UVChannelSwitch(UVChannel);
    return Selected * ScaleOffset.xy + ScaleOffset.zw;
}
```

## What the parser accepts

### Declarations

| Form | Notes |
| :-- | :-- |
| `uniform <Type> Name = <init>;` | A material parameter. `uniform bool` is a dynamic scalar by default; `/// @static` asks for a static switch. |
| `static const <Type> Name = <init>;` | A compile-time constant. `static`, `const` and a bare global are accepted too. |
| `<Type> Name(params) { … }` | A file-local helper; no linkage keyword. |
| `export <Type> Name(params) { … }` | Produces an asset. `void (inout material)` means a material; anything else, a material function. |
| `extern <Type> Name(params);` | A prototype bound to an existing asset through `/// @asset`. No body. |
| `struct Name { … };` | Fields may carry their own `///` block and array dimensions. No methods, no nesting. |
| `#include "path"` · `import "path";` | The same thing; the spelling is recorded so the printer reproduces what was written. The path must be **double-quoted**: `#include <Engine/Private/Common.ush>` is HLSL's spelling, not this one, and is `DSH3203`. The parser does not open the file; the binder reads and binds it. |
| `#pragma material(Key = Value, …)` | File-level material settings. |
| `#pragma layout(Node\|Comment, Key = Value, …)` | Decompiler-written node coordinates. |
| `#pragma region Name` · `#pragma endregion` | The 2.0 spelling of the 1.x `#Region` comment box. |

Types: `float half double int uint bool`, their vectors (`float2`..`float4`) and matrices
(`float2x2`..`float4x4`), `Texture2D TextureCube Texture2DArray Texture3D VolumeTexture`,
`SamplerState`, `material`, `Substrate`, `void`, and any other name — a user `struct`, or a spelling
the semantic pass will judge later.

### `///` doc blocks

The `///` lines above a declaration are its doc block. They are collected until the first token that is not a `///` line, so a blank line between them does not end the block. Per line:
`@key value @key value …` — a key is `@` + identifier, and its value runs to the next ` @` or to the
end of the line, so `@desc Contact: name@example.com` keeps the address inside the value. Text
before the first `@` is free text. Keys are lower-cased, and unknown keys are kept verbatim for the
semantic pass. A `@` not followed by an identifier is a warning (`DSH3220`) and is kept as ordinary
text wherever it falls — it does not start a directive. A block with nothing under it documents
nothing: it is dropped with a warning (`DSH3221`) and never reaches the tree.

Keys the tree carries today include `@group`, `@desc`, `@name`, `@sort`, `@slider`, `@default`,
`@sampler`, `@static`, `@asset`, `@library`, `@layer`, `@layerblend`, `@param` and `@custom` — the
parser records them all; only `@custom` changes parsing, by making the body opaque.

### Statements and expressions

Every statement kind: variable declaration, expression, block, `if` / `else`, `for`, `while`,
`do … while`, `return`, `break`, `continue`, `discard`, and the empty `;`. A `for` init may be a
declaration or an expression, and any of the three parts may be empty.

Full C expression precedence, right-associative assignment and `?:`, prefix and postfix `++`/`--`,
casts `(float3)x`, constructors `float3(…)`, member/swizzle/index chains, and `{ … }` initializer
lists (only as an initializer). Named arguments — `UE.TexCoord(Index = 0)` — are parsed as such;
a positional argument may not follow a named one.

Literals keep their lexeme exactly as written, so `1.0f`, `0x10`, `2u` and `.5` all print back
unchanged.

### `/// @custom` bodies

A function whose doc block carries `@custom` has its body captured **verbatim** instead of parsed:
braces inside strings, comments and `#` directives do not end the capture, and the text is handed to
the engine's shader compiler as-is. This is how a 2.0 file keeps the behaviour of a 1.x Custom node.

**The body is not DreamShaderLang, and this front end says nothing about it.** Only the braces are
read — they still have to balance, and a string, a comment or a `#` line inside cannot unbalance
them. Everything else is sliced out of the source verbatim, so a character that would be a lexical
error anywhere else — a `$` register name, a `@` intrinsic, a `'`, a `#` in the middle of a line —
is reported by nothing here. `dsc check --shaders` is what gets to complain about that text. The
same `#` one line outside the body is still `DSH2106`; the silence is scoped to the body.

With one exception, which is the point of it: **a `UE.` node call inside the body is lifted into the
graph.** The call's text stays where it is; the compiler builds that node, wires it to an extra input
of the Custom node (`_ds_<Function>_UE0`, `_UE1`, ...) and puts the input's name in the call's place.

```hlsl
/// @custom
float Pulse(float2 UV, float Gain)
{
    float T = UE.Time();                                              // a Time node, wired in
    float Mixed = UE.LinearInterpolate(A = 0.0, B = Gain, Alpha = 0.5); // built once per call site
    return sin(UV.x * 8.0 + T) * Mixed;
}
```

A lifted call is evaluated in the graph, outside the HLSL: it may read the function's parameters (they
are replaced by each call's arguments) and the file's globals, and nothing the body declares
(`DSH6326`). An output may be selected after the call (`UE.SceneTexture(...).InvSize`, `[k]`). A
function with lifted calls cannot be called from another HLSL body (`DSH6327`), because those inputs
belong to its own node.

**What the node looks like.** A function that returns a value is a Custom node with that output. A
`void` function's **first `out` parameter is the node's primary output** and the other `out`s are its
additional outputs, which is the node a 1.x `Function` always made; a `void` function without any
`out` returns an unused float. `/// @name` on a `@custom` function sets the node's title (its
Description), which is otherwise the function's name.

## What the pipeline does today

What happens to a parsed file:

- **Assets.** A `.dss` compiles to a `UMaterial`, a `UMaterialFunction`, a Material Layer or a Layer
  Blend, through the 1.x asset factory, digest, provenance, atomic rebuild and graph layout — so
  divergence detection, *Revert* / *Adopt* / *Detach* and the Content Browser all behave as they do
  for a `.dsm`.
- **Semantics.** Names are resolved, expressions are typed, calls are matched against signatures or
  against the engine's expression catalog, `///` directive values are validated, and `#include` /
  `import` paths are read and bound (a name defined twice across files is `DSH4210`, a cycle is
  `DSH4211`).
- **A graph IR** between the two, with constant folding, structural de-duplication and dead-node
  pruning, and a validator that runs before anything touches an asset. `dsc dump-ir` prints it.
- **A symbol index** (`dsc index`) and **node ↔ source navigation**: every emitted expression carries
  its source line in the asset's `DreamShader.SourceSpans` metadata, the Material Editor's node
  context menu gets *DreamShader ▸ Open Source Line*, and the editor bridge answers `reveal-node` in
  the other direction.
- **A shader check.** `dsc check -Shaders` builds the products and reports HLSL compile errors
  against the source line they came from.
- **The 1.x language**, through the legacy front end: every documented 1.x leniency is a numbered rule
  that applies to 1.x text only and says so where it applies (`DSH5275`–`DSH5292`), so a 1.x source
  builds what it always built while a `.dss` stays strict. Silent 1.x behaviour that was never
  documented is an error with a message that says what 1.x did (`DSH2200`–`DSH2222`).
- **Material instances** as [`.dsi`](instances.md) sources, checked against their parent's parameters.
- **The way back.** `dsc decompile` reads a material, function, layer, blend or instance into the IR
  and prints 2.0 text ([decompiler](../tools/decompiler.md)); [`dsc migrate`](../tools/migrate.md)
  rewrites 1.x sources as `.dss`. Both are proved by compiling the text they wrote and comparing IRs.
- **Comments survive a rewrite.** The parser keeps `//` and `/* */` comments as trivia on the
  declaration or statement they stand by, and the printer writes them back.

### Pins, defaults, layers and blends

| Feature | Spelling |
| :-- | :-- |
| A StaticBool function pin | `/// @static <Parameter>` on the function, for a `bool` parameter (`DSH7231` otherwise). An `if` on it is a StaticSwitch. |
| A default that is a graph expression | `float Row = UE.TexCoord(CoordinateIndex = 1).r` -- wired to the input's Preview pin. A constant default stays a preview value. |
| A texture nobody can override | `/// @default /Game/T_Noise` + `static const Texture2D Noise;` -- a TextureObject node, not a parameter. `@sampler` applies. |
| A layer blend | `/// @layerblend export void B(material Bottom, material Top, inout material Result)` -- two material inputs and a result that **starts empty**; `Bottom` / `Top` (also `Base`) set the pins' blend relevance. |
| A material layer | `/// @layer export void L(inout material m)` -- the input is optional, as the engine's own layer templates make it. |
| A required pin left open | a warning (`DSH5219`), not an error: only the material compile knows whether the node reads a default for it. |
| A pin named per node | for a class whose nodes name their pins after a property (`Substrate.MoonToonModifier`, TextureSample's derivative pins, `UE.LandscapeLayerBlend(Layer_Grass = ...)` after its `Layers`), a name the catalog does not list is looked up on the built node (`DSH5291`, refused there with `DSH8212`). The pin's own property name always resolves. |
| Unpassed optional inputs | stay unconnected on the call node; the callee's default applies. |

### Substrate, layout and tooling

| Feature | Spelling |
| :-- | :-- |
| [Substrate sugar](substrate.md) | `A + B`, `A * w`, `lerp(A, B, t)` over Substrate values; `Substrate.Slab(BaseColor = ..., Metallic = ...)`; a run-time `if` over Substrate values; `Substrate S = Substrate.Slab(); S.Roughness = r;`; `#pragma material(Substrate = Legacy \| Bridge \| Native)` |
| A layout that reads the source | *Project Settings ▸ DreamShader ▸ Graph Layout Style* — [Blocks (the default), Source Bands or Layered](../generation/graph-layout.md#layout-styles), or the 1.x `Classic`; `dsc dump-layout` draws all three without building anything |
| A formatter | [`dsc fmt`](../tools/commandlet.md#fmt) — the printer, with a check that refuses to write a file it cannot vouch for |
| The generated assets, listed | [`dsc list-generated`](../tools/commandlet.md#list-generated), for [source control](../generation/source-control.md) |

## What the pipeline does *not* do yet

- **No Material Layer *stack*.** `@layer` and `@layerblend` produce the two function kinds, but the
  material-level layer stack is a future `#pragma material` key.
- **No `let` / `auto`, and a node is not a value you can store.** Write the call where its output is
  read; two identical calls become one node, so re-writing it costs nothing.
- **No matrices in the graph.** A matrix that does not fold away is `DSH4361` wherever it appears —
  including a `@custom` pin, which the engine has no matrix type for. Compute it inside the custom
  body instead.
- **No unbounded loops.** `for` / `while` / `do` are unrolled when the trip count is a provable
  constant (`MaxUnrolledIterations`, 64 by default); anything else is `DSH4360` with a pointer at
  `/// @custom`. `discard` inside a branch is `DSH4362` — the graph has no form for it.
- **No `material` into a Custom node's input.** The engine's custom-expression translator has no
  material-attributes input pin, so a `/// @custom` function with a `material` parameter is
  `DSH6252`. Outputs may be attributes.
- **No hyperbolics.** `sinh` / `cosh` / `tanh` have no material node; write them in a `@custom` body.
- **No preprocessing inside the parser.** The parser expects text whose `#if` has already been
  resolved; a stray `#if` reaching it is `DSH3201` at file scope and `DSH2160` inside a function
  body, where the fix is to mark the function `/// @custom`.
- **No `switch`** (`DSH2160`), and no comma operator.
- **No layer parameters in a `.dsi`.** An instance file sets global parameters; a parameter that
  exists only on a material layer or blend is `DSH7268`.

## Diagnostics

Every 2.0 front-end message carries a stable `DSHnnnn` code and a localisable text. The codes are
the contract; the wording is not.

| Range | Raised by |
| :-- | :-- |
| `DSH2101`–`DSH2119` | the lexer. Allocated today: `2101` unknown character · `2102` unterminated block comment · `2103` unterminated string · `2104` unknown escape · `2105` malformed number · `2106` a `#` that is not the first thing on its line |
| `DSH2150`–`DSH2189` | expressions and statements. Allocated today: `2150`–`2155`, `2157`–`2165` |
| `DSH2200`–`DSH2269` | the legacy front end: 1.x Graph statements (`2200`–`2222`) and top-level blocks (`2240`–`2258`) |
| `DSH3200`–`DSH3249` | declarations, types, `#` directives, `///` blocks. Allocated today: `3200`–`3208`, `3210`, `3211`, `3213`–`3218`, `3220`–`3222` |
| `DSH3250`–`DSH3299` | the legacy front end's sections: Properties, Settings, Outputs, Inputs, Layout |
| `DSH4200`–`DSH4299` | the binder — names, types, expressions, statements, loops, regions |
| `DSH4300`–`DSH4349` | the IR validator |
| `DSH4350`–`DSH4399` | lowering refusals — matrices, unprovable loops, `discard` in a branch, an attribute read before it is written |
| `DSH5200`–`DSH5249` | reflected `UE.*` calls, the expression catalog, material attributes |
| `DSH5250`–`DSH5292` | 1.x call spellings (`5250`–`5265`) and the numbered legacy rules (`5275`–`5292`) |
| `DSH6200`–`DSH6219` | function kinds, the entry, `export` / `extern` |
| `DSH6220`–`DSH6249` | helper inlining |
| `DSH6250`–`DSH6299` | `/// @custom` HLSL |
| `DSH6300`–`DSH6330` | 1.x `Function` / `GraphFunction` / `Namespace` / `VirtualFunction`, and calls lifted out of a verbatim body |
| `DSH7200`–`DSH7249` | uniforms, `///` directives, `#pragma material` |
| `DSH7250`–`DSH7270` | `.dsi`: `#pragma instance` and overrides |
| `DSH8200`–`DSH8299` | the emitter, asset creation and the pipeline driver (`8240`–`8265`: material instances) |
| `DSH9020`–`DSH9059` | the tools (`check`, `dump-ir`, `index`, `export-catalog`) and node ↔ source navigation |
| `DSH9060`–`DSH9089` | the decompiler: graph import (`9060`–`9074`), IR to source (`9075`–`9084`), the service (`9085`–`9089`) |
| `DSH9090`–`DSH9099` | `dsc migrate` |
| `DSH9100`–`DSH9109` | `.dsi` read-back and the instance source rewriter |

See the [diagnostics index](../diagnostics/index.md) for cause and fix per code.

## Running the tests

`dsc.ps1` drives the pipeline headlessly (`compile`, `check`, `dump-ir`, `index`, `export-catalog` —
see [Commandlet](../tools/commandlet.md)); the automation suite is how the layers are pinned.

In the editor: *Tools ▸ Test Automation*, filter for `DreamShader.Lang2` or `DreamShader.Compiler2`.

Headless:

```
"F:\UnrealEngine\UE_Moon\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "I:\UnrealProject_Moon\DEV_58\MoonEngineSample\MoonEngineSample.uproject" ^
  -ExecCmds="Automation RunTests DreamShader.Lang2; Quit" ^
  -nullrhi -unattended -nopause -nosplash -log
```

| Test | Covers |
| :-- | :-- |
| `DreamShader.Lang2.Corpus.*` | The fixture corpus under `Tests/Corpus/Lang/` — one sub-test per file, asserting the parse outcome, the diagnostic codes, structural counts and the print/parse/print round trip |
| `DreamShader.Lang2.Lexer.*` | Tokens, literals, comments, `#` lines |
| `DreamShader.Lang2.Declarations.*` | Storage, linkage, `struct`, `#pragma`, `///` blocks |
| `DreamShader.Lang2.Expressions.*` | Precedence, calls, casts, constructors, chains |
| `DreamShader.Lang2.Statements.*` | Every statement kind, `for` shapes, recovery |
| `DreamShader.Lang2.Printer.*` | Round-tripping and formatting |
| `DreamShader.Lang2.Binder.*` | Name resolution, typing, call matching, function kinds, directives |
| `DreamShader.Lang2.IR.*` | Lowering, the passes, the validator |
| `DreamShader.Lang2.CorpusIR.*` | `Tests/Corpus/IR/` — `.dss` in, `DumpDreamShaderIRText` golden out |
| `DreamShader.Compiler2.{Smoke,Corpus,Parity}.*` | End to end to a real asset, and the `dump-graph` diff against the 1.x twin of the same material |

To cover a new construct, drop a `.dss` (plus an optional `.expected.json`) into the corpus tree —
no C++, no recompile. The golden schema is documented in
[`Tests/Corpus/README.md`](../../Tests/Corpus/README.md).

## See also

- [Substrate sugar](substrate.md) — operators, legacy parameters on a slab, values built member by member, `Substrate =`
- [Material instances](instances.md) — `.dsi`
- [`DreamShaderLang` C++ API](../api/lang-module.md) — the module, its headers and entry points
- [Language reference (1.x)](../language/index.md) — the syntax that ships today
- [Preprocessor](../language/preprocessor.md) — `#if` and the define table, shared by both syntaxes
- [Diagnostics index](../diagnostics/index.md) — every `DSHnnnn`
