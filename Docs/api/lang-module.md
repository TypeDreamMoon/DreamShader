# `DreamShaderLang`

> [DreamShader](../index.md) » [C++ API](index.md) » **`DreamShaderLang`**

`DreamShaderLang` is the front end **and the middle end**: the text goes in, a lowered graph comes
out. Lexer, preprocessor, parser, AST, printer, binder, the graph IR with its passes and validator,
and the diagnostic types live here — and nothing else does. It was split out of `DreamShader` for
2.0 so the language can be exercised without an editor, reused by a language service, and reasoned
about as a language rather than as a plugin.

The line the module does *not* cross is the engine: an `FIRModule` names no `UObject`, no asset and
no expression class object. Engine knowledge arrives as plain data in an
[`FBuiltinCatalog`](#ir--the-graph-ir), which the Editor module fills by reflection and can also
save to, and load from, JSON.

| | |
| :-- | :-- |
| Module | `DreamShaderLang` |
| Type / loading phase | `Runtime` / `PostConfigInit` |
| Dependencies | **`Core` only** |
| Export macro | `DREAMSHADERLANG_API` |
| Public headers | 4 at the module root + 7 under `Lang/` + 1 under `Semantic/` + 9 under `IR/` |
| Namespaces | `UE::DreamShader::Lang` (2.0 front end and binder) · `UE::DreamShader::IR` (the graph IR and the builtin catalog) · `UE::DreamShader` (preprocessor, define table, `FDreamShaderError`) |
| Reflected types | **none** — no `UCLASS`, no `USTRUCT`, no `UENUM` |
| Status | new in `2.0`. M1 shipped the lexer, the 2.0 parser and the printer; M2+M3 add the binder, the IR, its builder, passes and validator, and the custom-HLSL builder |

## The Core-only rule

`DreamShaderLang.Build.cs` declares exactly one dependency:

```csharp
PublicDependencyModuleNames.AddRange(new[] { "Core" });
```

This is a hard boundary, not a current state of affairs:

- **No `UObject`, no `Engine`, no `CoreUObject`.** Nothing in the module can name a `UMaterial`, a
  `UMaterialExpression` or an asset path type.
- **No `IPluginManager`, no `GetDefault<>()`, no config access.** The module cannot look anything up
  about the project it is running in.
- **Engine facts arrive as parameters.** The `DS_*` preprocessor constants, asset roots and
  expression class names are supplied by the host as plain data —
  [`DreamShaderDefineResolution.h`](index.md#public-headers) in the `DreamShader` module is the half
  that *builds* a define table by asking the engine; `DreamShaderLang` only *consumes* one.
- **Every header compiles in a commandlet, a test, or a future standalone language server.** The
  front end creates no objects and touches no files: `FLangSourceText` is handed text that somebody
  else read.

## Public headers

### `Lang/` — the 2.0 front end

Everything here is in `UE::DreamShader::Lang`. Include paths keep the `Lang/` prefix, e.g.
`#include "Lang/LangParser.h"`.

| Header | Purpose |
| :-- | :-- |
| `Lang/LangSource.h` | `FLangSpan` (offset + length + resolved 1-based line/column), `ELangFileKind`, `GetLangFileKindFromPath`, and `FLangSourceText` — one source text with its line table, the thing every span points into. |
| `Lang/LangDiagnostic.h` | `ELangSeverity`, `FLangDiagnostic` (a `DSHnnnn` code, a severity, an `FText` message, a span, a file path) and `FLangDiagnosticSink`, which collects them for one run. `FLangDiagnosticSink::ToWireString` renders `DSHnnnn: message` in the invariant (source) form for logs, JSON and test goldens. |
| `Lang/LangToken.h` | `ELangTokenKind`, `ELangKeyword`, `FLangToken`, `TryGetLangKeyword`, `GetLangTokenSpelling`. Type names are **not** keywords: `float3`, `Texture2D`, `material` and user struct names all reach the parser as identifiers. |
| `Lang/LangLexer.h` | `LexDreamShaderLang` and `FLangLexOptions`. One lexer serves both syntaxes. |
| `Lang/LangAst.h` | The tree both front ends produce: `FTypeRef`, `FDocBlock`/`FDocDirective`, `ENodeKind`, the expression / statement / declaration node structs, and `FModule`. Ownership is `TUniquePtr` down the tree; sub-kinds are told apart with `As<T>()` — no RTTI, no visitors. |
| `Lang/LangParser.h` | `ParseDreamShaderLang`, `ParseDreamShaderLangExpression`, `ELangFrontend`, `FLangParseOptions`, `FLangParseResult`. |
| `Lang/LangPrinter.h` | `PrintDreamShaderLang` and the per-node overloads, plus `FLangPrintOptions`. |

### `Semantic/` — the binder

`UE::DreamShader::Lang`, one header. Binding is the step between "it parses" and "it can be
lowered": every name resolved, every expression typed, every call matched against a signature or the
catalog, and the file's products decided.

| Header | Purpose |
| :-- | :-- |
| `Semantic/LangBound.h` | `BindDreamShaderLang`, `FBindOptions`, `FLangBindResult`, and `FBoundModule` — the AST plus side tables keyed by `const FNode*`: `FBoundStruct`, `FBoundGlobal`, `FBoundFunction` (with `EBoundFunctionKind`: entry, helper, export, extern, custom), `FBoundProduct`, `FBoundExpr` (`EBoundExprKind`), `FBoundDirectives` and the `Directive::` key names, the region tree, the resolved include paths, and the catalog the module was bound against. Also `BuildDreamShaderSymbolIndexJson` — the language service's symbol index, schema `dreamshader-symbol-index` version 1. |

A bound module **keeps pointers into the `FModule` it bound and into whatever the include resolver
returned**; both have to outlive it. Nothing downstream of the builder does — an `FIRModule` points
back at neither.

### `IR/` — the graph IR

`UE::DreamShader::IR`, nine headers. The IR is the material graph before it is a material graph:
nodes, operands, properties and products, with no engine type anywhere in it. Include paths keep the
`IR/` prefix.

| Header | Purpose |
| :-- | :-- |
| `IR/IRTypes.h` | `EIRTypeKind`, `FIRType` (kind + component count + texture kind + struct index), `TypeFromBuiltinRef`, `EIRConversion` and `ClassifyConversion` — the one place a "does this value fit that pin" question is answered. |
| `IR/IRCoreOps.h` | `EIROp`, `EIRTypingRule`, `FIRCoreOpInfo` and the lookups (`GetCoreOpInfo`, `FindCoreOpByHlslName`, `FindCoreOpByGlslAlias`, `FindCoreOpForBinary`, `FindCoreOpForUnary`, `IsCoreMathOp`). One table holds each core op's arity, typing rule, engine class and pin names; the binder reads the typing, the builder builds from it, the emitter reads the class and pins. |
| `IR/IRCatalog.h` | `FBuiltinCatalog` and its JSON schema: `FCatalogExpression` (class, pins, properties, outputs, aliases, `bIsCustomOutput`), `FCatalogPin`, `FCatalogProperty`, `FCatalogMaterialAttribute`, `ECatalogValueType`, `LoadBuiltinCatalogFromJson` / `SaveBuiltinCatalogToJson`, `TypeFromCatalogValueType`. **The only channel through which engine knowledge reaches this module.** |
| `IR/IR.h` | `FIRValue`, `FIRInput`, `FIRProperty` / `FIRPropertyValue` (`EIRPropertyKind`), the `Prop::` property-name constants, `FIRSourceRef`, `FIRNode`, `FIRRegion`, `FIRLayoutHint`, `FIRGraph`, `FIRProduct` (`EIRProductKind`, `EIRBackend`) and `FIRModule`. |
| `IR/IRBuilder.h` | `FIRBuildOptions` (`MaxUnrolledIterations`, `MaxInlineDepth`, `bKeepDebugNames`, an optional `Catalog` override) and `BuildDreamShaderIR`. |
| `IR/IRPasses.h` | `FIRPassOptions` (`bFoldConstants`, `bDedupe`, `bPrune`) and `RunDreamShaderIRPasses`. |
| `IR/IRValidator.h` | `ValidateDreamShaderIR` — one lowered module against the IR's own rules and against the catalog. An empty catalog skips the checks that need engine knowledge rather than failing them, so a hand-built graph can be validated in a unit test. |
| `IR/IRDump.h` | `DumpDreamShaderIRText` (the `dsc dump-ir` rendering, and the golden the IR corpus diffs) and `DumpDreamShaderIRJson`. |
| `IR/IRCustomHlsl.h` | `FCustomNodeCode` (`Code`, `IncludeFilePaths`, `InlinedFunctions`), `BuildDreamShaderCustomNodeCode`, and the `CustomCodeMarker` grammar with `TryParseCustomCodeBodyMarker` — the line markers a Custom node's body carries, which `dsc check -Shaders` maps shader-compile errors back through. |

> The IR holds **no source positions in its identity**: `FIRSourceRef` rides on a node for tooling
> (the asset's `DreamShader.SourceSpans` table, node ↔ source navigation) and is never part of a
> dedupe key. Two structurally identical nodes merge no matter where they were written.

### Module root — what moved here in 2.0

Three of the four headers below were in the `DreamShader` module through 1.9.x and moved into
`DreamShaderLang` for 2.0. They kept their `UE::DreamShader` namespace and their include paths, so
**existing callers need no source change** — only a `DreamShaderLang` entry in their `Build.cs`
dependency list. `DreamShaderLangModule.h` is new in 2.0.

| Header | Purpose |
| :-- | :-- |
| `DreamShaderPreprocessor.h` *(moved in 2.0)* | `PreprocessDreamShaderSource`, `FDreamShaderPreprocessResult`, `DreamShaderSourceHasPreprocessorDirectives`, `EvaluateDreamShaderConditionExpression`, `BuildDreamShaderDefineKeyFragment`. `#if` resolution, line-count-conserving. See [Preprocessor](../language/preprocessor.md). |
| `DreamShaderDefineTable.h` *(moved in 2.0)* | `EDreamShaderDefineSource`, `FDreamShaderDefineEntry`, the case-sensitive `FDreamShaderDefineMap` / `FDreamShaderDefineValueMap` aliases, `FDreamShaderDefineTable`, `IsReservedDreamShaderDefineName`, `IsValidDreamShaderDefineName`. Pure text; the engine-facing half that *fills* a table stayed behind as `DreamShaderDefineResolution.h` in the `DreamShader` module. |
| `DreamShaderDiagnostic.h` *(moved in 2.0)* | `FDreamShaderError` / `FDreamShaderTextError` — the 1.x code-carrying error struct, and the doc comment that records what each `DSHnnnn` leading digit means. |
| `DreamShaderLangModule.h` | `FDreamShaderLangModule`. Both lifecycle methods are empty: the module object exists only so the plugin descriptor can name it and dependents can link it. There is no singleton accessor and nothing to initialise. |

> `FDreamShaderError` (1.x, `FString`/`FText` message) and `FLangDiagnostic` (2.0, code + severity +
> `FText` + span) coexist on purpose: the 1.x pipeline still carries the former, and the 2.0 front
> end never produces one. They are not converted implicitly.

## Entry points

The module is a pipeline of six calls — preprocess, lex, parse, bind, lower, run the passes — plus a
validator, two dumps, a printer, and one call for parsing a single expression on its own.

| Call | Header | Contract |
| :-- | :-- | :-- |
| `PreprocessDreamShaderSource(...)` | `DreamShaderPreprocessor.h` | Resolves `#if` / `#define` against a define table, conserving line counts, so every later span still points at the author's line. |
| `LexDreamShaderLang(Source, Options, OutTokens, Diagnostics)` | `Lang/LangLexer.h` | Cuts a whole text into tokens. Returns `false` when any error was recorded; the array always ends with `EndOfFile`, so a caller can keep going. |
| `ParseDreamShaderLang(Source, Options)` | `Lang/LangParser.h` | One text → one `FModule`. The result's `Module` is present **even after errors**, holding whatever parsed cleanly, so a language service keeps working on a broken file; `Succeeded()` is `Module.IsValid() && !Diagnostics.HasErrors()`. Lexer and parser diagnostics are merged in source order, and a **lexical** diagnostic whose span falls inside a `/// @custom` body is dropped — that text is not this language (see *Opaque bodies* below). |
| `ParseDreamShaderLangExpression(Source, Diagnostics)` | `Lang/LangParser.h` | One text → one `FExpr`, for tests, the language service and `#pragma` values. A token left over after the expression loses the whole result (`DSH3211`) rather than silently truncating it; this is the only entry point that can raise that code. |
| `PrintDreamShaderLang(Module, Options)` | `Lang/LangPrinter.h` | `FModule` → `.dss` text. |
| `BindDreamShaderLang(Module, Options)` | `Semantic/LangBound.h` | One `FModule` → one `FBoundModule`. `FBindOptions::Catalog` is **required**; without an `IncludeResolver` every `#include` is refused. Like the parser it keeps going after an error, so ask `Succeeded()` rather than whether the sink is empty. The result borrows from `Module` and from the resolver's modules; both must outlive it. |
| `BuildDreamShaderSymbolIndexJson(Bound)` | `Semantic/LangBound.h` | Declarations, references, resolved includes and the parameter schema as JSON, for the language service. Works on a module that has errors — which is when an editor needs navigation most. |
| `BuildDreamShaderIR(Bound, Options, Diagnostics)` | `IR/IRBuilder.h` | `FBoundModule` → `TUniquePtr<FIRModule>`, every product lowered. **Returns a module even after errors** (the partial graph is what `dsc dump-ir` shows), so callers ask `Diagnostics.HasErrors()`; null means the bound module had no `FModule` at all. Nothing in the result points back into the AST. |
| `RunDreamShaderIRPasses(Module, Options, Diagnostics)` | `IR/IRPasses.h` | Constant folding, structural dedupe and dead-node pruning, in place. `FIRValue`s stay valid across it. Its diagnostics are informational only; a pass never fails a build. |
| `ValidateDreamShaderIR(Module, Catalog, Diagnostics)` | `IR/IRValidator.h` | `true` when the module is emittable. Diagnostics land in the sink either way, all stamped with the sink's own file path. |
| `DumpDreamShaderIRText(Module)` / `…Json(Module)` | `IR/IRDump.h` | The human rendering (`dsc dump-ir`, and the golden the IR corpus diffs) and the machine one (`dsc dump-ir -Json`). |
| `BuildDreamShaderCustomNodeCode(Bound, FunctionIndex, Out, Diagnostics)` | `IR/IRCustomHlsl.h` | One `/// @custom` function → the HLSL a Custom node carries, with its hoisted `#include`s and its embedded helpers. Deterministic: two calls for the same function produce the same string, which is what lets the dedupe key run over `Prop::Code`. |
| `LoadBuiltinCatalogFromJson(...)` / `SaveBuiltinCatalogToJson(...)` | `IR/IRCatalog.h` | The catalog's on-disk form. `dsc export-catalog` writes it; a tool that binds outside the editor reads it. |

```cpp
// MyTooling.Build.cs:  PrivateDependencyModuleNames.AddRange(new[] { "Core", "DreamShaderLang" });

#include "Lang/LangParser.h"
#include "Lang/LangPrinter.h"

using namespace UE::DreamShader::Lang;

void Normalise(const FString& Path, const FString& Text)
{
    const FLangSourceText Source(Path, Text);
    const FLangParseResult Result = ParseDreamShaderLang(Source);

    for (const FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s(%d,%d): %s"),
            *Diagnostic.FilePath, Diagnostic.Span.Line, Diagnostic.Span.Column,
            *FLangDiagnosticSink::ToWireString(Diagnostic));
    }

    if (Result.Succeeded())
    {
        const FString Printed = PrintDreamShaderLang(*Result.Module);
    }
}
```

### Front-end selection

`FLangParseOptions::Frontend` defaults to `ELangFrontend::Auto`, which decides from the file
extension carried by `FLangSourceText::GetPath()`:

| Extension | `ELangFileKind` | Front end |
| :-- | :-- | :-- |
| `.dss` | `Dss` | 2.0 |
| `.dsh` | `Dsh` | 2.0 (a shared header; both syntaxes may appear, and `export` is rejected) |
| `.dsm` | `Dsm` | legacy — **not available yet**, reports `DSH2199` |
| `.dsf` | `Dsf` | legacy — **not available yet**, reports `DSH2199` |

The legacy front end lands in M4 and lowers 1.x constructs onto the *same* AST node kinds, so
nothing downstream has to know which parser produced a module.

### Printer contract

The printer's promise is **structural fidelity, not textual**: `parse(print(parse(X)))` yields the
same tree as `parse(X)`. Concretely — parentheses are emitted where `FParenExpr` recorded them and
wherever precedence requires; `///` blocks are re-emitted one directive per line after the free
text; an opaque (`/// @custom`) body is written verbatim from `RawBody`; pragmas and includes keep
their spelling; literals keep the lexeme as written, so `1.0f`, `0x10` and `2u` survive. Comments
other than `///` are not in the tree and are **not** reproduced.

`FLangPrintOptions::NewLine` defaults to `\n`, but an opaque body is a slice of its source and
keeps that file's terminators, so printing a CRLF file yields mixed terminators. That is stable:
the next parse slices the same bytes back out, so the fixed point still holds.

### Opaque bodies

A function whose `///` block carries `@custom` has its body captured verbatim: `FFunctionDecl::Body`
is null, `bOpaqueBody` is true, `RawBody` is the text between the braces and `BodySpan` covers the
braces inclusive. The parser reads only the braces — they must balance, and strings, comments and
`#` lines inside cannot unbalance them because the lexer already made each one a single token.

Because the text is not DreamShaderLang, `ParseDreamShaderLang` **discards every lexical diagnostic
whose span starts inside a `BodySpan`**. A `$`, a `@`, a `'` or a mid-line `#` in there is not a
fact about the program, and the shader compiler is the one that gets to judge it. The suppression
is scoped to those spans: the same character one line outside is still `DSH2101` / `DSH2106`.

## Diagnostics

Every raise site names its code literally, so `.skill/gen-diagnostics.ps1` can find it:

```cpp
Diagnostics.Error(TEXT("DSH2151"), Span, LOCTEXT("ExpectedExpression", "expected an expression"));
```

The messages are `LOCTEXT` under a per-file `LOCTEXT_NAMESPACE` of the form
`"DreamShader.Lang.<Unit>"`, so the editor can localise them. **Assert on the code, never on the
message text.**

| Range | Raised by | Fixed anchors |
| :-- | :-- | :-- |
| `DSH2101`–`DSH2119` | the lexer (allocated: `2101`–`2106`) | `2101` unknown character · `2102` unterminated block comment · `2103` unterminated string · `2104` unknown escape · `2105` malformed number · `2106` a `#` that is not the first thing on its line |
| `DSH2150`–`DSH2189` | expressions and statements (allocated: `2150`–`2155`, `2157`–`2165`) | `2150` unexpected end of file · `2158` positional argument after a named one · `2160` unsupported statement (`switch`, and a `#` line inside a body) · `2162` an initializer list used as an expression |
| `DSH2199` | front-end selection | legacy front end requested but not available (until M4) |
| `DSH3200`–`DSH3249` | declarations, types, directives, doc blocks (allocated: `3200`–`3208`, `3210`, `3211`, `3213`–`3218`, `3220`–`3222`) | `3201` stray preprocessor directive · `3203` `#include` / `import` without a **double-quoted** path · `3204` expected a type name · `3208` function without a body · `3213` bad storage/linkage combination · `3220` malformed `@` directive (**warning**) · `3221` orphan `///` block (**warning**) · `3222` a 1.x declaration word |

The binder, the builder, the passes, the validator and the custom-HLSL builder raise through the
same sink, with their own ranges:

| Range | Raised by |
| :-- | :-- |
| `DSH4200`–`DSH4299` | the binder — names, types, expressions, statements, loops, regions |
| `DSH4300`–`DSH4349` | the IR validator |
| `DSH4350`–`DSH4399` | lowering refusals — a matrix that survives folding, a loop with no provable trip count, `discard` inside a branch, a material attribute read before it is written |
| `DSH5200`–`DSH5299` | reflected calls, the catalog, material attributes |
| `DSH6200`–`DSH6219` | function kinds, the entry, `export` / `extern` |
| `DSH6220`–`DSH6249` | helper inlining |
| `DSH6250`–`DSH6299` | `/// @custom` HLSL |
| `DSH7200`–`DSH7249` | uniforms, `///` directives, `#pragma material` |

`DSH1030`–`DSH1042` (the preprocessor's own codes) moved into this module with the preprocessor and
kept their numbers. The prose for every code lives under [Diagnostics](../diagnostics/index.md).

## Testing: the `Lang` corpus

The front end is covered by a **data-driven corpus**, not by hand-written per-feature tests. Fixtures
live under `Tests/Corpus/Lang/`, one `.dss` or `.dsh` per feature, each optionally paired with a
`<name>.expected.json` golden; a complex automation test enumerates the tree at runtime and surfaces
every fixture as its own sub-test.

| | |
| :-- | :-- |
| Test | `DreamShader.Lang2.Corpus.<Area>.<Fixture>` |
| Runner | `RunDreamShaderLangCorpusCase` in `Source/DreamShaderEditor/Private/Tests/DreamShaderTestCommon.h` |
| Registration | `Source/DreamShaderEditor/Private/Tests/DreamShaderCorpusLangTests.cpp` |
| Fixtures | `Tests/Corpus/Lang/{Lexical,Expressions,Statements,Declarations,Examples}/` — `.dss` and `.dsh`, plus one `.dsm` that pins front-end selection (`DSH2199`) |
| Golden schema | `"entryPoint": "lang"` — see [`Tests/Corpus/README.md`](../../Tests/Corpus/README.md) |

Each case is parsed once through `ParseDreamShaderLang`, then checked against its golden:

- `outcome` — `ok` or `error`. Without a golden the default is "a positive fixture parses, a
  `*.bad.*` fixture fails".
- `errorContains` / `warningsContain` — substrings (case-**sensitive**) of
  `FLangDiagnosticSink::ToWireString` of each diagnostic of that severity. In practice: the
  `DSHnnnn` code.
- `module` — structural counts over the parsed `FModule`: `declarations` (pragmas and includes
  count), `pragmas`, `includes`, `structs`, `uniforms` (`Uniform` storage only), `constants`
  (`StaticConst` only), `functions` (all three linkages), `exports`, `externs`, `opaqueBodies`,
  `entry` (the first function whose signature is `void (inout material)`, **regardless of linkage**)
  and `roundtrip`.
- `roundtrip` — print → parse → print is byte-identical, which is the printer's contract expressed
  as a test.

Adding coverage for a new construct is dropping a file in the tree; no C++ changes, no recompile.
Run the whole layer headlessly:

```
"F:\UnrealEngine\UE_Moon\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "I:\UnrealProject_Moon\DEV_58\MoonEngineSample\MoonEngineSample.uproject" ^
  -ExecCmds="Automation RunTests DreamShader.Lang2; Quit" ^
  -nullrhi -unattended -nopause -nosplash -log
```

Adding `-DreamShaderUpdateGolden` rewrites every golden from the actual result instead of asserting.
Review the diff by hand before committing — that switch is how a regression becomes a baseline.

Alongside the corpus, each front-end unit has its own focused test file
(`DreamShaderLangLexerTests.cpp`, `…DeclarationTests.cpp`, `…ExpressionTests.cpp`,
`…PrinterTests.cpp`), all under `DreamShader.Lang2.*`.

## See also

- [What the 2.0 pipeline does](../language-v2/index.md) — the language surface and the pipeline's limits, from the author's side
- [C++ API index](index.md) — the other three modules
- [Preprocessor](../language/preprocessor.md) — `#if` and the define table, documented from the language side
- [Diagnostics index](../diagnostics/index.md) — every `DSHnnnn` with cause and fix
- [Testing](../contributing/testing.md) — the automation suite and the fixture corpus
