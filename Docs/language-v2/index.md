# DreamShaderLang 2.0 — what exists today

> [DreamShader](../index.md) » **DreamShaderLang 2.0**

> [!IMPORTANT]
> **A `.dss` now compiles.** As of **M2+M3**, the 2.0 line is a whole pipeline: preprocess → parse →
> **bind → lower to a graph IR → run the passes → validate → emit**, ending in a real `UMaterial` or
> `UMaterialFunction` built by the same reflection factory, digest, provenance, atomic rebuild and
> layout the 1.x generator uses. `dsc compile` builds one; `dsc check` stops at IR validation and
> writes nothing; `dsc dump-ir` and `dsc index` show the middle.
>
> The **1.x language is untouched and stays supported**: `.dsm` and `.dsf` still go through the
> [1.x pipeline](../language/index.md), which M4 will fold into this one as a second front end.
> Parsing a `.dsm` through the 2.0 entry point is still `DSH2199` until then.

DreamShaderLang 2.0 drops the 1.x section blocks (`Shader`, `Properties`, `Outputs`, `Graph`, …) and
writes a material as what it always was underneath: **HLSL with declarations**. Metadata that used
to need a section now rides on `///` doc comments and `#pragma` lines, so a `.dss` file is one
source with no side files and no strip step.

| | |
| :-- | :-- |
| Extensions | `.dss` — a 2.0 compilation unit · `.dsh` — a shared header |
| 1.x extensions | `.dsm` / `.dsf` — unchanged, second front end, arrives in M4 |
| Module | [`DreamShaderLang`](../api/lang-module.md) (`Core` only) — front end, binder and IR; the emitter is in `DreamShaderEditor` |
| Tests | `DreamShader.Lang2.*`, `DreamShader.Compiler2.*` |
| Status | M1: lexer + 2.0 parser + printer + corpus · **M2+M3: binder, IR, passes, validator, emitter, `dsc check` / `dump-ir` / `index` / `export-catalog`, node ↔ source navigation** |

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

## What the M1 parser accepts

### Declarations

| Form | Notes |
| :-- | :-- |
| `uniform <Type> Name = <init>;` | A material parameter. `uniform bool` is a dynamic scalar by default; `/// @static` asks for a static switch. |
| `static const <Type> Name = <init>;` | A compile-time constant. `static`, `const` and a bare global are accepted too. |
| `<Type> Name(params) { … }` | A file-local helper; no linkage keyword. |
| `export <Type> Name(params) { … }` | Produces an asset. `void (inout material)` means a material; anything else, a material function. |
| `extern <Type> Name(params);` | A prototype bound to an existing asset through `/// @asset`. No body. |
| `struct Name { … };` | Fields may carry their own `///` block and array dimensions. No methods, no nesting. |
| `#include "path"` · `import "path";` | The same thing; the spelling is recorded so the printer reproduces what was written. The path must be **double-quoted**: `#include <Engine/Private/Common.ush>` is HLSL's spelling, not this one, and is `DSH3203`. Paths are **not** resolved in M1. |
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

## What the pipeline does today

Everything M1 listed as missing except the legacy front end is now in:

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

## What the pipeline does *not* do yet

- **No legacy front end.** Parsing a `.dsm` or `.dsf` through the 2.0 entry point reports `DSH2199`
  until M4; use the [1.x parser](../api/parser.md) for those. Both pipelines run side by side until
  then, and the 1.x generator is what still compiles `.dsm` / `.dsf`.
- **No decompiler into the IR** (M5), **no new layout** (M6) — the 1.x layout is called on the
  emitted graph, bridges and all — and **no Substrate sugar** (M7).
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
- **Comments other than `///` are not preserved.** They are not in the tree, so the printer does not
  reproduce them.

## Diagnostics

Every 2.0 front-end message carries a stable `DSHnnnn` code and a localisable text. The codes are
the contract; the wording is not.

| Range | Raised by |
| :-- | :-- |
| `DSH2101`–`DSH2119` | the lexer. Allocated today: `2101` unknown character · `2102` unterminated block comment · `2103` unterminated string · `2104` unknown escape · `2105` malformed number · `2106` a `#` that is not the first thing on its line |
| `DSH2150`–`DSH2189` | expressions and statements. Allocated today: `2150`–`2155`, `2157`–`2165` |
| `DSH2199` | the legacy front end was requested but is not available yet |
| `DSH3200`–`DSH3249` | declarations, types, `#` directives, `///` blocks. Allocated today: `3200`–`3208`, `3210`, `3211`, `3213`–`3218`, `3220`–`3222` |
| `DSH4200`–`DSH4299` | the binder — names, types, expressions, statements, loops, regions |
| `DSH4300`–`DSH4349` | the IR validator |
| `DSH4350`–`DSH4399` | lowering refusals — matrices, unprovable loops, `discard` in a branch, an attribute read before it is written |
| `DSH5200`–`DSH5299` | reflected `UE.*` calls, the expression catalog, material attributes |
| `DSH6200`–`DSH6219` | function kinds, the entry, `export` / `extern` |
| `DSH6220`–`DSH6249` | helper inlining |
| `DSH6250`–`DSH6299` | `/// @custom` HLSL |
| `DSH7200`–`DSH7249` | uniforms, `///` directives, `#pragma material` |
| `DSH8200`–`DSH8299` | the emitter, asset creation and the pipeline driver |
| `DSH9020`–`DSH9059` | the tools (`check`, `dump-ir`, `index`, `export-catalog`) and node ↔ source navigation |

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

- [`DreamShaderLang` C++ API](../api/lang-module.md) — the module, its headers and entry points
- [Language reference (1.x)](../language/index.md) — the syntax that ships today
- [Preprocessor](../language/preprocessor.md) — `#if` and the define table, shared by both syntaxes
- [Diagnostics index](../diagnostics/index.md) — every `DSHnnnn`
