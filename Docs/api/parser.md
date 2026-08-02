# DreamShaderParser.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderParser.h**

The single public entry point into the DreamShaderLang front end: one static function that turns
source text into an [`FTextShaderDefinition`](types.md#ftextshaderdefinition).

Defined in header `DreamShaderParser.h`.

| | |
| :-- | :-- |
| Module | `DreamShader` (Runtime) |
| Include | `#include "DreamShaderParser.h"` |
| Namespace | `UE::DreamShader` |
| Export macro | `DREAMSHADER_API` on the class |
| Pulls in | `DreamShaderTypes.h` — this is the only include, so `CoreMinimal.h` arrives transitively |

## Synopsis

```cpp
#include "DreamShaderTypes.h"

namespace UE::DreamShader
{
    class DREAMSHADER_API FTextShaderParser
    {
    public:
        static bool Parse(const FString& SourceText,
                          FTextShaderDefinition& OutDefinition,
                          FString& OutError);
    };
}
```

`FTextShaderParser` has no constructor, no data members and no other methods. It is a namespace with
a class shape; there is never a reason to instantiate it.

## `FTextShaderParser::Parse`

```cpp
static bool Parse(const FString& SourceText, FTextShaderDefinition& OutDefinition, FString& OutError);
```

| Parameter | Direction | Contract |
| :-- | :-- | :-- |
| `SourceText` | in | The complete text of one translation unit, **already import-expanded**. `Parse` does not resolve `import` directives; it treats an `import` line as an unrecognized top-level token. |
| `OutDefinition` | out | **Reset to a default-constructed `FTextShaderDefinition` on entry, unconditionally.** Any prior contents are discarded, including on the failure path. |
| `OutError` | out | `Reset()` on entry. Written only on failure; empty after a successful parse. |

| | |
| :-- | :-- |
| Returns | `true` on success — `OutDefinition` is populated and `OutError` is empty. `OutDefinition.Warnings` may still be non-empty. |
| | `false` on failure — `OutError` holds one message and `OutDefinition` holds whatever had been parsed before the failure. Do not consume it. |
| Reentrancy | Full. There is no global mutable state. |
| Allocation | Everything is by value into `OutDefinition`. Nothing is heap-owned by the parser after return. |

### Thread and context requirements

`Parse` is string processing. It creates no `UObject`, loads no asset, and touches no material API.
There is exactly one engine dependency:

> [!NOTE]
> Resolving a texture default written as `Path(Plugin.X, "…")` calls
> `IPluginManager::Get().FindPlugin`. That requires the plugin manager to be initialized. A source
> file with no plugin-rooted `Path(...)` has no engine dependency at all. A file that has one fails
> the parse outright when the plugin cannot be found, is not enabled, or cannot contain content —
> the path is never silently left empty.

Given an initialized plugin manager, `Parse` is safe to call from any thread, including a worker
thread and a commandlet with no editor. This is the only stage of the pipeline that is not
editor-only — [generation](../generation/index.md) requires the editor.

### Import expansion is the caller's job

The editor assembles the parse unit before calling: it reads the file, inlines every `import`
recursively, replaces the import lines with blank lines, and brackets each inlined file with
`// Begin DreamShader source: <path>` / `// End DreamShader source: <path>` markers so reported line
and column numbers stay in the file the author edited.

A caller that skips this step gets `Unexpected token near index {Index}.` on the first `import`.
See [`import`](../language/import.md).

## Accepted top-level keywords

The dispatcher tries these in order. Keyword matching is **case-sensitive**.

| # | Keyword | Effect | Lands in |
| :-: | :-- | :-- | :-- |
| 1 | `Shader` | Reads `Name`, optional `Root`, then the body. **At most one per parse unit.** | `Name`, `Root`, `Properties`, `Settings`, `OutputDeclarations`, `Outputs`, `Code`, `GraphRegions`, `Layout` |
| 2 | `Function` | Modern function declaration, non-graph | `Functions` |
| 3 | `GraphFunction` | Modern function declaration, graph-hoisting | `GraphFunctions` |
| 4 | `Namespace` | Namespace block; may contain **only** `Function` and `GraphFunction` | `Functions`, `GraphFunctions`, with names qualified as `Namespace::Function` |
| 5 | `VirtualFunction` | `Name` required, `Asset` optional at the header (may come from `Options`) | `VirtualFunctions` |
| 6 | `ShaderFunction` | `Kind = ShaderFunction` | `MaterialFunctions` |
| 7 | `ShaderLayerBlend` | `Kind = MaterialLayerBlend` | `MaterialFunctions` |
| 8 | `ShaderLayer` | `Kind = MaterialLayer` | `MaterialFunctions` |
| 9 | `MaterialLayerBlend` | **Deprecated** alias of `ShaderLayerBlend`; appends a warning *(deprecated in 1.3.0)* | `MaterialFunctions` |
| 10 | `MaterialLayer` | **Deprecated** alias of `ShaderLayer`; appends a warning *(deprecated in 1.3.0)* | `MaterialFunctions` |

Order matters at two places: `ShaderLayerBlend` is tested **before** `ShaderLayer`, and
`MaterialLayerBlend` **before** `MaterialLayer`, so the longer keyword always wins. Anything else at
top level produces `Unexpected token near index {Index}.`

## Signature rules for `Function` and `GraphFunction`

| Rule | Behaviour |
| :-- | :-- |
| Parameter shape | 2 or 3 whitespace-separated tokens. Two tokens (`float X`) means an implicit `in` qualifier. |
| Qualifier comparison | Lower-cased before comparison; only `in` and `out` are accepted. |
| Destination | `out` parameters go to `Results`; everything else goes to `Inputs`. |
| Return-type lowering | `Function float Foo(...)` synthesizes `Results[0]` named `__return` and rewrites every top-level `return <expr>;` into `__return = <expr>;`. |
| Bare `return;` | Rejected when the declaration has a return type. |
| Return type + `out` | Rejected. Use `out` parameters alone for multiple outputs. |
| No outputs at all | Rejected — a function must produce something. |
| Disambiguation | `Function Foo(...)` versus `Function float Foo(...)` is decided by looking ahead for `(`. |
| `SelfContained` / `Inline` | **`Function` only.** Either modifier — matched case-insensitively — sets `bSelfContained` and must be followed by a name. `GraphFunction` does not recognize them; the token is read as a name or return type instead. |

## Token normalization applied during the parse

These rewrites happen before anything is stored, so a `Type` field never contains a GLSL alias.

### Type tokens — 15 mappings, matched case-insensitively

| Written | Stored | Written | Stored | Written | Stored |
| :-- | :-- | :-- | :-- | :-- | :-- |
| `vec2` | `float2` | `ivec2` | `int2` | `uvec2` | `uint2` |
| `vec3` | `float3` | `ivec3` | `int3` | `uvec3` | `uint3` |
| `vec4` | `float4` | `ivec4` | `int4` | `uvec4` | `uint4` |
| `bvec2` | `bool2` | `mat2` | `float2x2` | | |
| `bvec3` | `bool3` | `mat3` | `float3x3` | | |
| `bvec4` | `bool4` | `mat4` | `float4x4` | | |

Any other token passes through trimmed but otherwise unchanged.

### Function-body text — the same 15 type aliases plus 3 function aliases

| Written | Stored |
| :-- | :-- |
| `mix` | `lerp` |
| `fract` | `frac` |
| `mod` | `fmod` |

Eighteen replacements in total. They are applied identifier-wise and **skip** string literals, `//`
line comments and `/* */` block comments. Namespace-qualified identifiers written `A::B` are
collapsed through [`SanitizeIdentifier`](dreamshader-module.md#sanitizeidentifier) to `A_B`.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section; the parser emits the
substituted text.

This table covers the messages `Parse` and its file-level helpers produce. Section bodies —
`Properties`, `Settings`, `Outputs`, `Options`, `Layout` — have their own diagnostics; the complete
cross-stage list is the [diagnostics index](../diagnostics/index.md).

### Errors — `Parse` returns `false`

| Message | Cause |
| :-- | :-- |
| `Only one top-level Shader block is currently supported.` | A second `Shader` block in the parse unit |
| `Shader(Name="...") is required.` | `Shader` header with no `Name` attribute |
| `Shader must provide a Graph block.` | A `Shader` was found, `Code` is empty, and no output declaration carries an initializer |
| `A top-level Shader, Function, GraphFunction, Namespace, ShaderFunction, ShaderLayer, ShaderLayerBlend, or VirtualFunction block was not found.` | The text parsed cleanly but declared nothing |
| `Unexpected token near index {Index}.` | An unrecognized token at top level |
| `Expected '{Char}' near index {Index}.` | A required opening delimiter is missing |
| `Unterminated '{Char}' block.` | End of text before the matching closing delimiter |
| `{Keyword}(Name="...") is required.` | `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` / `MaterialLayer` / `MaterialLayerBlend` with no `Name`. `{Keyword}` is the keyword actually written |
| `VirtualFunction(Name="...") is required.` | `VirtualFunction` with no `Name` attribute |
| `VirtualFunction name cannot be empty.` | `Name` present but whitespace-only |
| `VirtualFunction '{Name}' must provide Options = { Asset = Path(...); }.` | Neither an `Asset` attribute nor an `Options.Asset` entry |
| `VirtualFunction '{Name}' must declare at least one output.` | `Outputs` is empty |
| `Namespace(Name="...") is required.` | `Namespace` with no `Name` |
| `Namespace name cannot be empty.` | Whitespace-only namespace name |
| `Namespace name '{Name}' is not a valid identifier.` | Non-identifier characters in the name |
| `Namespace '{Name}' may only contain Function or GraphFunction blocks.` | Any other keyword inside a `Namespace` |
| `Function declaration is missing a valid function name.` | Identifier parse failed after `Function` |
| `GraphFunction declaration is missing a valid function name.` | Identifier parse failed after `GraphFunction` |
| `Function declaration is missing a valid function name after SelfContained.` | `SelfContained` or `Inline` not followed by a name |
| `{Keyword} declaration is missing a function name after the return type '{Type}'.` | A return type was written with no name after it. `{Keyword}` is `Function` or `GraphFunction` |
| `{Keyword} '{Name}' is missing a valid parameter list. {InnerError}` | The `( … )` list could not be extracted |
| `{Keyword} '{Name}' is missing a valid body block. {InnerError}` | The `{ … }` body could not be extracted |
| `Function '{Name}' has an invalid return type '{Type}'.` | The return-type token is empty after normalization |
| `Function '{Name}' has an invalid parameter declaration '{Parameter}'.` | Fewer than 2 or more than 3 whitespace-separated tokens, or an empty type or name |
| `Function '{Name}' parameter '{Parameter}' uses unsupported qualifier '{Qualifier}'. Supported qualifiers are in and out.` | A three-token parameter whose qualifier is neither `in` nor `out` |
| `Function '{Name}' parameter name '__return' is reserved for return-type lowering.` | A parameter literally named `__return`, compared case-insensitively |
| `Function '{Name}' has a return type and cannot also declare out parameters. Use out parameters without a return type for multiple outputs.` | A return type together with at least one `out` parameter |
| `Function '{Name}' must declare at least one out parameter.` | No return type and no `out` parameter |
| `A function with a return type cannot use a bare 'return;'. Return a value, e.g. 'return expr;'.` | `return;` inside a function declared with a return type |

### Warnings — appended to `OutDefinition.Warnings`

Warnings never fail the parse. The editor and the commandlet append them to the compile result under
a `Warnings:` header.

| Message | Cause |
| :-- | :-- |
| `MaterialLayerBlend is deprecated; use ShaderLayerBlend instead.` | The `MaterialLayerBlend` keyword was used |
| `MaterialLayer is deprecated; use ShaderLayer instead.` | The `MaterialLayer` keyword was used |
| `No Outputs block was provided. Generation requires explicit material property bindings.` | A `Shader` block with an empty `Outputs` array |

## Notes

- **`OutDefinition` is cleared before any work happens.** Reusing one `FTextShaderDefinition` across
  several `Parse` calls is safe but never accumulates; each call starts from scratch.
- **`OutError` is untouched on success**, so a caller may inspect it only after a `false` return.
- Exactly one message is produced per failed parse. The parser stops at the first error; there is no
  error-recovery mode and no multi-diagnostic result.
- Warnings and errors are independent. A parse can return `true` with warnings, and can return
  `false` having already collected warnings — in which case the warnings are discarded with the
  rest of the definition.
- `Parse` never logs. Nothing reaches `LogDreamShader` from this function; reporting is entirely the
  caller's decision.
- The "one `Shader` per parse unit" rule applies to the whole import closure, because the closure is
  what was flattened into `SourceText`.

## Example

```cpp
#include "DreamShaderModule.h"
#include "DreamShaderParser.h"
#include "Misc/FileHelper.h"

using namespace UE::DreamShader;

bool ValidateSourceFile(const FString& InPath, FString& OutError)
{
    if (!IsDreamShaderSourceFile(InPath))
    {
        OutError = FString::Printf(TEXT("'%s' is not a DreamShaderLang source file."), *InPath);
        return false;
    }

    FString SourceText;
    if (!FFileHelper::LoadFileToString(SourceText, *NormalizeSourceFilePath(InPath)))
    {
        OutError = FString::Printf(TEXT("Could not read '%s'."), *InPath);
        return false;
    }

    FTextShaderDefinition Definition;
    if (!FTextShaderParser::Parse(SourceText, Definition, OutError))
    {
        return false;   // OutError holds the one parse diagnostic.
    }

    for (const FString& Warning : Definition.Warnings)
    {
        UE_LOG(LogDreamShader, Warning, TEXT("%s: %s"), *InPath, *Warning);
    }

    return true;
}
```

Given a file that uses the deprecated layer keyword and declares no bindings:

```c
MaterialLayer(Name="Layers/ML_Rust")
{
    Outputs { MaterialAttributes Result; }
    Graph   { Result = UE.MakeMaterialAttributes(); }
}
```

the call returns `true` and logs:

```text
LogDreamShader: Warning: I:/Project/DShader/ML_Rust.dsm: MaterialLayer is deprecated; use ShaderLayer instead.
```

## See also

- [`DreamShaderTypes.h`](types.md) — every struct `Parse` fills
- [`DreamShaderModule.h`](dreamshader-module.md) — `NormalizeSourceFilePath`, `SanitizeIdentifier`, the file predicates
- [`DreamShaderCompiler`](compiler-module.md) — the layer above: requesting a full compile
- [Source files](../language/source-files.md) — what each extension may contain
- [`import`](../language/import.md) — how the parse unit is assembled before `Parse` sees it
- [Keyword index](../language/keywords.md) — every top-level keyword and alias
- [`Function`](../language/function.md) — the declaration form whose signature rules are tabulated above
- [`Namespace`](../language/namespace.md) — the block with the most restrictive contents rule
- [`VirtualFunction`](../language/virtual-function.md) — the block with the `Asset` and `Outputs` requirements
- [Generation](../generation/index.md) — what happens to a definition after a successful parse
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
