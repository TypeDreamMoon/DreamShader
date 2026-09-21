# DreamShaderParser.h *(retired in 2.0)*

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderParser.h**

> [!IMPORTANT]
> **`DreamShaderParser.h` and `FTextShaderParser` no longer exist.** Through 1.9.x,
> `FTextShaderParser::Parse` in the `DreamShader` module turned 1.x source text into an
> [`FTextShaderDefinition`](types.md#ftextshaderdefinition), and the 1.x generator built assets from
> that struct. 2.0 retired both: a `.dsm` / `.dsf` / `.dsh` is now read by the **legacy front
> end** of the one compiler, into the same AST a `.dss` parses to.

## What to call instead

| 1.x | 2.0 |
| :-- | :-- |
| `#include "DreamShaderParser.h"` (module `DreamShader`) | `#include "Lang/LangParser.h"` (module `DreamShaderLang`, `Core` only) |
| `FTextShaderParser::Parse(SourceText, OutDefinition, OutError)` | `Lang::ParseDreamShaderLang(Lang::FLangSourceText(Path, Text), Options)` |
| one `FText` error, first error only | `FLangParseResult::Diagnostics` — every diagnostic, each with a `DSHnnnn` code, a severity and a span with line, column and length |
| `FTextShaderDefinition` | `FLangParseResult::Module` (`Lang::FModule`, the AST both front ends produce) plus `FLangParseResult::Legacy` (`Lang::FLegacyMigrationInfo`: the 1.x blocks, sections, parameter declarations, asset references and renames behind that AST) |
| `Definition.Warnings` | diagnostics of severity `Warning` in the same sink |
| text had to have its `import`s expanded first | `import` is an `#include`; it is resolved at bind time through `FBindOptions::IncludeResolver` |

```cpp
#include "Lang/LangParser.h"

using namespace UE::DreamShader::Lang;

const FLangSourceText Source(TEXT("DShader/Materials/M_Panel.dsm"), Text);   // the extension picks the front end
const FLangParseResult Parsed = ParseDreamShaderLang(Source);

if (Parsed.Succeeded() && Parsed.Legacy.IsValid())
{
    // Parsed.Module: functions, uniforms, pragmas -- what the 1.x blocks mean in 2.0 terms.
    // Parsed.Legacy: what the 1.x text said, for tools that need the 1.x view (migrate, the language service).
}
```

`ELangFrontend::Auto` decides by extension: `.dsm` and `.dsf` are 1.x, `.dss` and `.dsi` are 2.0, and
a `.dsh` header is dispatched **per declaration**, so one header may hold both dialects while a
project migrates.

## What changed in behaviour

The legacy front end keeps 1.x's **documented** leniencies and drops its undocumented silent ones:
what 1.x accepted without a word and then built something other than what the text said is now a
diagnostic. The rules are catalogued as `L1`–`L26`; each one that rewrites or drops something says so
with a code (`DSH5254`, `DSH5275`–`DSH5292`, …), and [`dsc migrate`](../tools/migrate.md) writes the
same rules out as explicit 2.0 text.

1.x parser codes retired with it: `DSH2007`, `DSH3133`, `DSH3137` and their neighbours are no longer
raised; the legacy front end's own are `DSH2200`–`DSH2258`, `DSH3250`–`DSH3278` and
`DSH6300`–`DSH6330`.

## See also

- [`DreamShaderLang`](lang-module.md) — every entry point of the language module
- [`DreamShaderTypes.h`](types.md) — the 1.x data model, which the asset layer still uses
- [Migrate](../tools/migrate.md) — turning 1.x sources into `.dss`
- [Diagnostics index](../diagnostics/index.md)
