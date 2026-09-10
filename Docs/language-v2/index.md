# DreamShaderLang 2.0 — what exists today

> [DreamShader](../index.md) » **DreamShaderLang 2.0**

> [!IMPORTANT]
> **This is a front end, not a pipeline.** As of milestone **M1** of the `2.0` line, DreamShaderLang
> 2.0 can *read* a `.dss` file — lex it, parse it into a tree, print that tree back out — and
> nothing more. There is no semantic analysis, no HLSL emission and **no asset generation** behind
> it yet. Everything that actually builds a `UMaterial` today still goes through the
> [1.x language](../language/index.md), which is unchanged and stays supported.

DreamShaderLang 2.0 drops the 1.x section blocks (`Shader`, `Properties`, `Outputs`, `Graph`, …) and
writes a material as what it always was underneath: **HLSL with declarations**. Metadata that used
to need a section now rides on `///` doc comments and `#pragma` lines, so a `.dss` file is one
source with no side files and no strip step.

| | |
| :-- | :-- |
| Extensions | `.dss` — a 2.0 compilation unit · `.dsh` — a shared header |
| 1.x extensions | `.dsm` / `.dsf` — unchanged, second front end, arrives in M4 |
| Module | [`DreamShaderLang`](../api/lang-module.md) (`Core` only) |
| Tests | `DreamShader.Lang2.*` |
| Status | M1: lexer + 2.0 parser + printer + corpus |

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

## What M1 does *not* do yet

- **No assets.** Nothing generates a `UMaterial` or `UMaterialFunction` from a `.dss`. The 1.x
  pipeline is untouched and remains the way to build.
- **No semantics.** The tree is syntax only. Names are not resolved, types are not checked,
  overloads are not selected, `///` values are not validated, and `#include` / `import` paths are
  not read. A `.dss` that parses cleanly may still be nonsense.
- **No legacy front end.** Parsing a `.dsm` or `.dsf` through the 2.0 entry point reports `DSH2199`
  until M4; use the [1.x parser](../api/parser.md) for those.
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

See the [diagnostics index](../diagnostics/index.md) for cause and fix per code.

## Running the tests

The 2.0 front end has no CLI and no editor UI yet; the automation suite is how you exercise it.

In the editor: *Tools ▸ Test Automation*, filter for `DreamShader.Lang2`.

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

To cover a new construct, drop a `.dss` (plus an optional `.expected.json`) into the corpus tree —
no C++, no recompile. The golden schema is documented in
[`Tests/Corpus/README.md`](../../Tests/Corpus/README.md).

## See also

- [`DreamShaderLang` C++ API](../api/lang-module.md) — the module, its headers and entry points
- [Language reference (1.x)](../language/index.md) — the syntax that ships today
- [Preprocessor](../language/preprocessor.md) — `#if` and the define table, shared by both syntaxes
- [Diagnostics index](../diagnostics/index.md) — every `DSHnnnn`
