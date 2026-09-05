# Preprocessor

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Preprocessor**

`#if` / `#ifdef` / `#ifndef` / `#elif` / `#else` / `#endif`, plus `#define` and `#undef`:
line-oriented directives evaluated at generation time, before the source is parsed. A branch that is
not taken never reaches the parser and never becomes a node.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` |
| Kind | directives |
| Processed by | the preprocessor, **before** `import` extraction and before the declaration parser |
| Recognized | line by line, over each file's own text, outside `Function` bodies |
| Case rule | the eight keywords are matched **lowercase only** |
| Since | `1.9.0` |

## Synopsis

```c
#if      <expr>
#ifdef   <NAME>        // exactly #if  defined(NAME)
#ifndef  <NAME>        // exactly #if !defined(NAME)
[#elif   <expr>]…
[#else]
#endif

#define  <NAME> [<value>]
#undef   <NAME>
```

A directive occupies its whole line. Leading whitespace is allowed, whitespace after the `#` is
allowed, and a trailing `//` comment is allowed. Everything else on the line belongs to the directive.

`#ifdef` and `#ifndef` are pure sugar — they desugar to `#if defined(NAME)` and `#if !defined(NAME)`
before anything else happens, and behave identically from there, including what they contribute to
the [build key](#rebuilds). They exist because the spelling decision was "match HLSL", and a HLSL
author writes them from muscle memory.

## What this is for

`#if` does the one thing a [static switch](../parameters/parameter-nodes.md) cannot: cut the
**declaration** layer.

A static switch is a node. It lives inside `Graph = { … }` and it selects between two values. It has
nowhere to stand when the difference between two builds of a material is not a value but a
*declaration* — a different `ShadingModel`, a different set of `Outputs`, a different `import`,
a whole `ShaderFunction` block that should not exist at all. Under Substrate a material may need all
four, and no arrangement of switch nodes expresses any of them.

`#if` is text, so it can cut any of it.

### Choosing between `#if` and a static switch

| | `#if` | `StaticSwitchParameter` / `StaticBool` |
| :-- | :-- | :-- |
| Evaluated | at generation time, over source text | when the material's shader permutation is compiled |
| Can select between | any two spans of lines, anywhere outside an HLSL body | two expressions inside a `Graph` body |
| Reaches `Settings`, `Outputs`, `Properties`, `import`, whole blocks | **yes** | no |
| Chosen by | the project's define table — one answer per compile | a material instance — a different answer per instance |
| Untaken branch ends up | nowhere; it is not in the asset, so it is never validated | in the asset, as nodes, compiled into its own permutation |
| Changing the answer costs | a rebuild of every source that reads the define | ticking a checkbox on an instance |
| Visible to an artist | no — the asset shows only the branch that was taken | yes — as a parameter, with the other branch still in the graph |

Read that table as one rule: **`#if` is a project-wide, build-time decision; a static switch is a
per-instance, artist-facing one.**

- The environment decides, and the difference is in a declaration → `#if`. Substrate on or off, an
  engine-version gate, a platform-specific `Outputs` block.
- Someone tweaking a material instance should decide → static switch. Always. A `#if` cannot be
  flipped from an instance, and flipping it project-wide rebuilds every asset that reads it.
- The difference is one expression inside `Graph`, and every consumer would pick the same branch →
  either works; prefer the static switch. It needs no define registered, and it keeps one asset that
  can still be told to do the other thing.

> [!WARNING]
> Do not reach for `#if` to keep a permutation count down. That is what a static switch's
> `StaticSwitchParameter` already does at the instance level; replacing it with `#if` trades a
> permutation for a rebuild and takes the choice away from whoever is using the material.

## Directives

### Recognition

Each physical line is tested. A line that fails any rule is not a directive and is passed through to
the parser unchanged.

| # | Rule |
| :-- | :-- |
| 1 | the line must be outside every `Function` / `GraphFunction` body — see [below](#function-and-graphfunction-bodies-are-the-shader-compilers) |
| 2 | the trimmed line must not start with `//` |
| 3 | the first non-whitespace character must be `#` |
| 4 | whitespace may follow the `#` — `#  if FOO` is the same directive as `#if FOO` |
| 5 | the keyword must be one of `if` `ifdef` `ifndef` `elif` `else` `endif` `define` `undef`, spelled in **lowercase** |
| 6 | `#if` / `#elif` require an expression; `#ifdef` / `#ifndef` / `#undef` require one name; `#define` takes a name plus an optional value running to end of line; `#else` / `#endif` take nothing |
| 7 | after the operand, only whitespace and a `//` comment may follow — anything else is `DSH1042`. `#define` is exempt, having no operand end to check |
| 8 | `#if` blocks nest, to a depth of 64 |
| 9 | `#else` closes the chain — no `#elif` and no second `#else` may follow it |

Commenting a directive out therefore works exactly as it looks: `// #if FOO` is an ordinary comment
line, by rule 2.

The trailing-comment strip in rule 7 is quote-aware, so a `//` inside a string literal does not end
the directive early: `#if DS_HOST == "http://build"` compares against the whole URL.

> [!WARNING]
> **Rule 7 applies to `#else` and `#endif` unconditionally**, whether or not they sit inside a branch
> that was cut — they belong to the *chain*, not to a branch, exactly as the `DSH1032` / `DSH1033`
> pairing checks do. The habit this catches is C's, where a long chain is labelled at the bottom:
>
> ```c
> #endif MOONTOON_LEGACY     // DSH1042 — MOONTOON_LEGACY is a stray token
> #endif // MOONTOON_LEGACY  // correct
> ```
>
> `#define` is the one directive with no trailing check at all, because its value runs to the end of
> the line. See [`#define` values are text](#define-values-are-text).

### What is *not* a directive

DreamShaderLang had two other `#` constructs before this feature existed, and neither may break.
Which `#` lines the preprocessor claims is decided by **where the line is**, and then by **the word
after the `#`** — matched against a closed list, with nothing waved through on shape alone:

| Where | After the `#` | Handling |
| :-- | :-- | :-- |
| inside a `Function` / `GraphFunction` body | anything at all | **passed through verbatim** — not recognized, not paired, not counted toward nesting, never in the touched set |
| outside | `region` or `endregion`, in **any** case | **passed through** to the generator |
| outside | one of the eight keywords, in **lowercase** | a directive |
| outside | anything else | `DSH1035` |

The region row names those two words explicitly instead of waving through some broader shape, and
that is the whole of the interoperation between the two syntaxes: `#Region` keeps the
case-insensitive matching it has always had, and nothing else gets a free pass on its account.

The last row is what that buys. `#include` at the declaration level really is a mistake — the
spelling there is [`import`](import.md) — and a typo such as `#ifdefined` or `#endfi` must not pass
silently, because a swallowed `#endfi` leaves everything below it unconditionally enabled. **A
mis-cased directive is reported for the same reason**: `#IF FOO` and `#Endif` are `DSH1035`, not
lines that quietly do nothing.

`DSH1035` picks its suggestion from what you wrote:

| Written | Suggested |
| :-- | :-- |
| a near-miss of a keyword — `#ifdefined`, `#endfi`, `#IF` | the correct lowercase spelling |
| `#include` | [`import "…"`](import.md), the DreamShaderLang spelling |
| anything else | the general rule — the eight keywords, lowercase |

`#Region` is a different thing wearing a similar hat, and the two never collide:

| | `#if` family | `#Region` / `#EndRegion` |
| :-- | :-- | :-- |
| Spelling | lowercase only — `#IF` is `DSH1035` | matched **case-insensitively**, so `#Region` and `#REGION` are the same |
| Read by | the preprocessor, before parsing | the generator, over a stored `Graph` body |
| Valid | on any line outside a `Function` / `GraphFunction` body | only inside `Graph = { … }` |
| Effect | removes lines from the text the parser sees | names a span of statements, which becomes a comment box |
| Blank-line rule | a directive line is emitted as an empty line | a directive line is emitted as an equal-length run of spaces |
| Counts as a directive for [Adopt](#a-source-with-directives-cannot-be-adopted) | yes | **no** |

### `Function` and `GraphFunction` bodies are the shader compiler's

A `Function` or `GraphFunction` body is raw HLSL, and HLSL has a preprocessor of its own. Directives
written there address **that** preprocessor, with the shader compiler's defines — `PIXELSHADER`,
`MATERIALBLENDING_MASKED`, the engine's own environment — none of which DreamShader knows or should
be guessing at.

```c
// The engine exposes blend mode only as a shader define, so this has to be a Custom node.
Function MoonToonBlendModeSwitch(in float3 Opaque, in float3 Masked, out float3 Result)
{
#if MATERIALBLENDING_SOLID
    Result = Opaque;
#elif MATERIALBLENDING_MASKED
    Result = Masked;
#else
    Result = 0;
#endif
}
```

**DreamShader's preprocessor does not descend into those bodies.** Every line between a `Function`
signature's `{` and its matching `}` is passed through verbatim, `#` lines included: they are not
recognized as directives, they do not pair with anything outside, they do not count toward nesting
depth, and they contribute nothing to the touched set. The lines above reach the generated `.ush`
unchanged and are resolved when the material's shader is compiled.

> [!IMPORTANT]
> This boundary is the difference between a working function and a silently wrong one. Evaluated
> against DreamShader's define table, `MATERIALBLENDING_SOLID` is simply not defined, so it reads
> `0`, every `#elif` reads `0`, the `#else` branch wins unconditionally, and the function compiles
> cleanly while returning the wrong value for every blend mode. There is no diagnostic to notice,
> because nothing went wrong — the wrong preprocessor answered.
>
> This is not a hypothetical. Across the whole source tree, **every** HLSL `#` directive in a `.dsm`,
> `.dsf` or `.dsh` — nine files, six `#include` and three `#if` / `#elif` / `#else` / `#endif`
> chains — sits inside a `Function` body, with no exceptions. The opaque region is what keeps them
> working.
>
> The rule is worth stating the other way round too: **inside a `Function` body you cannot write a
> DreamShader `#if`.** Conditional HLSL there is conditional at *shader* compile time. To pick
> between two function bodies at generation time, put the `#if` around the `Function` blocks
> themselves, where DreamShader can see it:
>
> ```c
> #if DS_SUBSTRATE
> Function ApplyShading(in float3 C, out float3 R) { R = SubstratePath(C); }
> #else
> Function ApplyShading(in float3 C, out float3 R) { R = LegacyPath(C); }
> #endif
> ```

A body is found by taking the `{` that closes a `Function` / `GraphFunction` signature — which may
span several lines and may carry `SelfContained` or `Inline` — and scanning to its matching `}`,
ignoring braces inside comments and string literals. `GraphFunction` is treated as opaque on the same
terms, because it too can carry bare HLSL, and a DreamShader `#if` was never useful in an HLSL body
in the first place: HLSL's own preprocessor is already there.

## Conditional expressions

### Grammar

```c
expr    := or
or      := and     ( '||' and )*
and     := equ     ( '&&' equ )*
equ     := rel     ( ('==' | '!=') rel )*
rel     := add     ( ('<' | '<=' | '>' | '>=') add )*
add     := mul     ( ('+' | '-') mul )*
mul     := unary   ( ('*' | '/' | '%') unary )*
unary   := ('!' | '-' | '+')? primary
primary := '(' expr ')'
         | 'defined' ( '(' IDENT ')' | IDENT )
         | IDENT
         | INT
         | STRING
```

`&&` and `||` **short-circuit**: the right-hand side is not evaluated when the left already decides
the answer, and a define only the right-hand side would have read is therefore not read at all.

### Values

There are two kinds of value: a 64-bit signed **integer**, and a **string**. There is no float, and
no boolean — a condition is true when it evaluates to a non-zero integer.

| Expression | Evaluates to |
| :-- | :-- |
| an integer literal | itself; decimal, or `0x` / `0X` hexadecimal |
| a quoted `"…"` literal | that string |
| `defined(X)` or `defined X` | `1` when `X` is defined, `0` when it is not — the value is not looked at |
| an identifier that is **not defined** | the integer `0`, as in C |
| an identifier defined to an **empty** value | the integer `1` |
| an identifier whose whole value is an integer literal | that integer |
| any other defined identifier | its value, as a string |

Those last four rows are tried **in that order**, and the empty-value row has to come before the
parse: an empty string does not parse as an integer, so testing that first would make every bare
`#define FOO` marker a *string* — and a string in a `#if` is an error, which would close off the most
common spelling of all. A signed literal counts, so `#define FOO -3` is the integer `-3`, even though
the grammar's `INT` is unsigned and the `-` is nominally a unary operator.

| Operation | Rule |
| :-- | :-- |
| `==` and `!=` between two integers | numeric |
| `==` and `!=` between two strings | literal, and **case-sensitive** — `DS_PLATFORM == "windows"` is false on Windows |
| `==` and `!=` between a string and an integer | `DSH1040`, not a silent `false` |
| `<` `<=` `>` `>=`, arithmetic, `!`, `&&`, `\|\|`, and a bare boolean test | integers only — a string operand is `DSH1040` |
| `/` and `%` | truncate toward zero, as in C and HLSL: `-7 / 2` is `-3` and `-7 % 2` is `-1` |
| `/` and `%` by zero | `DSH1041` |

> [!WARNING]
> **A string is never coerced to a truth value.** `#if DS_PLATFORM` is `DSH1040`, not "true because
> it is non-empty". The same goes for `!DS_PLATFORM`, for `DS_PLATFORM && X`, and for comparing a
> string against a number. Silently treating a string as true or false is how a platform gate ends up
> firing everywhere, so it is refused instead — write `DS_PLATFORM == "Windows"`.

> [!NOTE]
> `DS_PLUGIN_VERSION` and `DS_PLATFORM` are strings, so `DS_PLUGIN_VERSION >= "1.9.0"` is `DSH1040`,
> not a version test. Compare a string with `==` / `!=` and nothing else; for a version gate use the
> integer engine defines.

### Examples

```c
#if DS_SUBSTRATE
#elif defined(MOONTOON_LEGACY_TOON)
#elif DS_PLATFORM == "Windows"
#else
#endif
```

An engine-version gate needs both halves of the version, or it will read `6.0` as older than `5.7`:

```c
#if DS_ENGINE_MAJOR > 5 || (DS_ENGINE_MAJOR == 5 && DS_ENGINE_MINOR >= 7)
```

### `#define` values are text

A `#define`'s value is **everything after the name, to the end of the line**. It is stored as text and
never tokenized, never expanded, and never evaluated:

```c
#define PP_SUM  1 + 1      // the five-character string "1 + 1", NOT the integer 2
#define PP_C    5 // five  // the integer 5 — the trailing comment is stripped first
#define PP_NOTE two words  // the string "two words"
#define PP_MARK            // empty: a marker, which reads as the integer 1
```

`#if PP_SUM == 2` is therefore `DSH1040` — a string against an integer — not `true`. This is the one
place where DreamShader's `#define` is deliberately weaker than C's: there are no macros here, only
named values for a `#if` to test.

Two consequences follow from the value running to end of line:

- **`#define` is the only directive with no trailing-token check.** `#define A B C` is legal, with the
  value `B C`. Every other directive reports `DSH1042` for a stray token — including `#undef A B`,
  which has no such excuse.
- **The `//` strip happens before the value is taken**, so a comment never lands in the value. It is
  quote-aware, so `#define PP_URL "http://x"` keeps its whole string.

A `#define` or `#undef` with no name at all is `DSH1038`, the name diagnostic — `DSH1036` is reserved
for the `#if` family missing its operand.

## Where defines come from

Five tiers, resolved in this order. A later tier overwrites an earlier one — **except `Builtin`,
which never loses.**

| # | Tier | Set from | Beats |
| --: | :-- | :-- | :-- |
| 1 | `Builtin` | the `DS_` environment facts below | everything — this tier is read-only |
| 2 | `Settings` | *Preprocessor Defines* in [Project settings](../settings/project.md) | nothing — the lowest tier |
| 3 | `Registered` | `RegisterDreamShaderDefine()` from C++ | `Settings` |
| 4 | `Provider` | a define-provider delegate, pulled fresh at resolve time | `Registered` |
| 5 | `CommandLine` | `-Define=NAME=VALUE` on the [commandlet](../tools/commandlet.md) | everything but `Builtin` |

Names are **case-sensitive**, matching C and HLSL: `Foo` and `FOO` are two defines. A name must match
`[A-Za-z_][A-Za-z0-9_]*`.

### The builtin `DS_` constants

| Name | Type | Value | Taken from |
| :-- | :-- | :-- | :-- |
| `DS_ENGINE_MAJOR` | integer | the engine major version — `5` | `DREAMSHADER_UE_MAJOR` |
| `DS_ENGINE_MINOR` | integer | the engine minor version — `3` … `8` | `DREAMSHADER_UE_MINOR` |
| `DS_ENGINE_PATCH` | integer | the engine hotfix version | `DREAMSHADER_UE_PATCH` |
| `DS_SUBSTRATE` | integer | `1` when Substrate is enabled, `0` otherwise | the `r.Substrate` CVar |
| `DS_PLATFORM` | string | the ini platform name, for example `"Windows"` | `FPlatformProperties::IniPlatformName()` |
| `DS_PLUGIN_VERSION` | string | the plugin's `VersionName`, for example `"1.9.0"` | the plugin descriptor; `"unknown"` if unavailable |

Three of those sources are chosen rather than obvious, and each choice is visible in behaviour:

- **The engine version comes from DreamShader's own macros, not `ENGINE_MAJOR_VERSION`.** They are
  the same macros the plugin's C++ `DREAMSHADER_UE_VERSION_AT_LEAST` guards read, so a fork that
  overrides them for compatibility testing gets one answer, not two that disagree.
- **`DS_PLATFORM` is the *ini* platform name, never `PlatformName()`.** The latter folds in
  editor/server/client, answering `WindowsEditor` in the editor and `Windows` during a cook — so one
  source would preprocess two different ways on the two sides of a cook, and the asset the editor
  showed would not be the asset that shipped.
- **`DS_PLUGIN_VERSION` is always defined**, falling back to `"unknown"` rather than going undefined.
  Otherwise `defined(DS_PLUGIN_VERSION)` could flip for a reason that has nothing to do with the
  source, and take a build key with it.

Everything in this table is an **invariant of the running process**, and that is a requirement rather
than a coincidence. A define is evaluated once, at generation time, and its effect is then baked into
a saved asset; a value that can change mid-session makes the build unreproducible and the asset's
recorded build key a lie. `r.Substrate` qualifies only because it is a read-only CVar, fixed at
startup.

> [!WARNING]
> **The `DS_` prefix is reserved, as a prefix, not as a list.** Nothing may define, redefine or
> undefine a name beginning with `DS_`, whether or not DreamShader currently ships that name. The
> refusal is reported differently depending on where the attempt came from, because only one of them
> has a file and a line to point at:
>
> | Attempted from | Result |
> | :-- | :-- |
> | `#define DS_FOO` or `#undef DS_FOO` in a source | the compile fails with `DSH1039` |
> | `RegisterDreamShaderDefine("DS_FOO", …)` | returns `false`, logs an error, table unchanged |
> | *Preprocessor Defines*, or `-Define=DS_FOO=…` | the entry is dropped with a warning; the compile proceeds |
>
> A prefix rule rather than a fixed list is what keeps a future builtin from silently losing to a
> define some project already registered under that name.
>
> The prefix test is itself **case-sensitive**, so `ds_foo` and `Ds_Foo` are ordinary names and may be
> defined freely — they cannot collide with a builtin, so there is nothing to protect. That the check
> must be case-sensitive is not a nicety either: were it not, `ds_substrate` would pass the reserved
> test and then land in `DS_SUBSTRATE`'s slot, overwriting a read-only builtin through the back door.

### Project settings

*Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Compiler ▸ **Preprocessor Defines*** is a
`TMap<FString, FString>` written to the project's `DefaultEngine.ini`. It is the checked-in tier: the
switches that describe *this project*, shared by everyone who clones it.

```ini
[/Script/DreamShader.DreamShaderSettings]
+PreprocessorDefines=(("MOONTOON_LEGACY_TOON", "1"))
+PreprocessorDefines=(("TOON_QUALITY", "2"))
```

Editing the map bumps the define revision, which invalidates the in-memory materials — see
[Rebuilds](#rebuilds).

### From C++

Two entry points, both free functions in `UE::DreamShader`, declared in
`Public/DreamShaderDefineTable.h`.

**Direct registration** is the simple one. The `SourceTag` identifies the contributor, so a whole
set can be withdrawn at shutdown:

```cpp
#include "DreamShaderDefineTable.h"

void FMoonToonModule::StartupModule()
{
    using namespace UE::DreamShader;

    RegisterDreamShaderDefine(TEXT("MOONTOON"),         TEXT("1"), TEXT("MoonToon"));
    RegisterDreamShaderDefine(TEXT("MOONTOON_VERSION"), TEXT("3"), TEXT("MoonToon"));
}

void FMoonToonModule::ShutdownModule()
{
    UE::DreamShader::UnregisterDreamShaderDefinesFrom(TEXT("MoonToon"));
}
```

`RegisterDreamShaderDefine` returns `false` for an invalid or reserved name and leaves the table
alone. Registering the same name twice from the same tag overwrites.

**A provider** is the one to reach for by default. Direct registration has to have happened before
the first compile, which makes it a question about module load order; a provider is pulled at the
moment the table is resolved, so load order stops mattering and the value may depend on state that
was not ready at `StartupModule` time:

```cpp
#include "DreamShaderDefineTable.h"

FDelegateHandle Handle = UE::DreamShader::RegisterDreamShaderDefineProvider(
    UE::DreamShader::FDreamShaderDefineProviderDelegate::CreateLambda(
        [](UE::DreamShader::FDreamShaderDefineTable& InOutTable)
        {
            const int32 Quality = GetMoonToonQualityLevel();
            InOutTable.Set(
                TEXT("TOON_QUALITY"),
                FString::FromInt(Quality),
                UE::DreamShader::EDreamShaderDefineSource::Provider,
                TEXT("MoonToon"));
        }));

// ... at shutdown
UE::DreamShader::UnregisterDreamShaderDefineProvider(Handle);
```

Whatever a provider computes must still obey the invariance rule: it is asked once per compile, and
its answer is baked into the asset.

Two more functions are worth knowing. `ResolveDreamShaderDefines()` returns the table one compile
will see, with all five tiers already merged — useful for logging what a build actually ran with.
`GetDreamShaderDefineRevision()` is bumped by every change to every tier; anything caching compiled
output against the define set compares it and invalidates when it moves.

> [!NOTE]
> **Use `FDreamShaderDefineMap` / `FDreamShaderDefineValueMap`, not a plain `TMap<FString, …>`, for
> anything keyed by a define name.** Both aliases pin `ESearchCase::CaseSensitive` key matching in
> their key funcs, because a default `TMap<FString, …>` does not promise one answer about case: an
> `FString` has no in-class `operator==`, `Equals` has already moved its default to case-sensitive in
> this engine tree, and `GetTypeHash(FString)` is still the case-*insensitive* `Strihash`. Leaving it
> to the container would make case sensitivity — a language rule — an engine-version detail, so a
> plugin built against 5.6, 5.7 and 5.8 would have three different languages.
>
> `EDreamShaderDefineSource::SourceFile`, the enumerator for a `#define` in a source, is listed after
> the five tiers and **takes no part in the ordering**. Those definitions live only in the private
> copy of the table the preprocessor discards when it returns, where they simply overwrite whatever
> the injected tiers said for the remainder of that one file. A `#define` of a reserved name is still
> refused by `Set()`, which is what raises `DSH1039`.

### Command line

The [commandlet](../tools/commandlet.md) takes `-Define=NAME=VALUE`, or `-D=NAME=VALUE` for short.
**The option is repeatable** — each occurrence adds a name — and it is the most explicit tier, so it
outranks everything but the builtins.

```powershell
-run=DreamShader compile -All -Force `
  -Define=MOONTOON_LEGACY_TOON=1 `
  -Define=TOON_PROFILE=cinematic `
  -D=CI_BUILD
```

`-Define=CI_BUILD` with no `=VALUE` defines the name to an empty value — `defined(CI_BUILD)` is `1`
and `#if CI_BUILD` is true. See [Values](#values).

## `#define` is file-local

A `#define` is visible **only in the file that writes it**. It does not reach the file that imports
that file, and it does not reach a file imported after it. This is not C.

```c
// Shared/Common.dsh
#define TOON_FANCY 1
```

```c
// Materials/M_Toon.dsm
import "Shared/Common.dsh";

#if TOON_FANCY        // ← 0. The header's #define is not visible here.
```

The reason is an ordering trade. The preprocessor runs **before** imports are extracted and inlined,
which is what lets an `#if` wrap an `import` line:

```c
#if DS_SUBSTRATE
import "Shared/SubstrateHelpers.dsh";
#else
import "Shared/LegacyHelpers.dsh";
#endif
```

Making `#define` behave like C's would mean preprocessing the *assembled* text, after inlining — and
then the imports would already be in it, so an `#if` could no longer decide which ones to pull. The
two cannot both be had. Wrapping `import` won, and C's include-order-dependent macro state, which is
a decades-old source of bugs, is not missed.

**To define a switch centrally, use a channel built for it**: *Preprocessor Defines* in the project
settings, `RegisterDreamShaderDefine` from C++, or `-Define=` for a one-off build. A `#define` in a
source file is for a local abbreviation within that file, and nothing more.

> [!NOTE]
> A `#define` or `#undef` inside a branch that is not taken does nothing, exactly as the rest of that
> branch does nothing. The directive lines are cut with everything else.

## Consequences

The first four change how you work, not just what compiles. Read them before writing the first
`#if`; the last two explain why the rest of the toolchain keeps working across a cut.

### An inactive branch is not checked at all

The text is cut before the parser sees it, so a branch that is not taken is never lexed, never
parsed, never type-checked, and never generated. It may contain anything at all — invalid syntax, a
call to a function that does not exist, a `ShadingModel` that was removed two engine versions ago —
and the compile is green.

This is C's bargain and it is the price of cutting the declaration layer: the thing that makes `#if`
able to remove a `Settings` line is the same thing that makes it unable to check one.

The practical consequence is that **a branch nobody currently builds rots, silently.** Two habits
help:

- Keep branches short. A `#if` around three lines of `Settings` will be noticed when it breaks; a
  `#if` around two hundred lines will not.
- Compile every combination you actually ship. A commandlet run per define set, with `-Force`, is a
  CI gate that catches exactly this — see [Command line](#command-line).

### A source with directives cannot be adopted

[**Adopt Into Source**](../generation/divergence.md#adopt-into-source) rewrites a source file from
the asset's current contents. The asset only ever holds the *result* of the cut — the branch that was
taken, with no record that a branch existed. Rewriting the source from it would therefore delete
every directive in the file and silently replace a conditional material with whichever build happened
to be on disk.

So Adopt refuses, with `DSH8149`, as soon as the source contains any directive at all — taken or not,
valid or not, and including a file whose only directive is a `#define`.

Two things do **not** count, and both matter in practice. `#Region` is not a preprocessor directive.
And neither is a `#if` inside a `Function` body: the gate applies the same opaque-region rule as the
preprocessor itself, so a material function whose HLSL body branches on `MATERIALBLENDING_SOLID` or
`PIXELSHADER` stays adoptable. Those files contain no DreamShader conditional, and refusing them
would have made the feature cost something to sources that never used it.

The other two ways out of a divergence are unaffected, because neither writes the source:

| Action | With directives in the source |
| :-- | :-- |
| **Revert to Source** | works — discards the hand edits and rebuilds from the source, conditionals and all |
| **Adopt Into Source** | **refused**, `DSH8149` |
| **Detach From DreamShader** | works — drops the stamps and stops managing the asset |
| **VirtualFunction sync** | **refused**, `DSH9001` — see below |

To take a hand edit back into a conditional source, use *Export DSM* and merge the change by hand
into the right branch.

### VirtualFunction sync refuses the same sources

The [startup sync service](../tools/virtual-function-tools.md#startup-sync-service) rewrites
`VirtualFunction` declarations in place, and it refuses a source containing directives with
`DSH9001`. The reason is sharper than Adopt's, and so is the risk.

Sync splices new text over a declaration using recorded **byte** offsets. Line-count conservation
buys line alignment and nothing more: a cut line becomes a *shorter* empty line, so every byte offset
past the first cut points somewhere else. There is no offset map in the contract to correct with, and
if a preprocessed string ever reached the file writer it would flatten every dead branch in the file
**permanently**.

What makes it worse than Adopt is that nobody asked for it: Adopt is a menu item a person clicks,
while sync runs unattended at bridge startup and walks every writable source in the project.

> [!NOTE]
> Only the **writing** path refuses. The read-only callers of the same scanner — the context menu,
> *Open Virtual Function*, copying the call example — keep reading the raw text, so they see the union
> over all branches. That is the same rule the dependency graph follows, and for the same reason.
>
> The refusal is also narrowed by a `VirtualFunction` substring test first, so a conditional material
> that sync would never have touched does not collect an error at every editor start.

### The decompiler never emits a directive

Decompilation reads an asset, and an asset holds one graph — the branch that was taken. There is
nothing in it from which a `#if` could be reconstructed.

**Conditional compilation is one-way syntax.** `.dsm` → asset preserves it; asset → `.dsm` does not.
A round trip through the [decompiler](../tools/decompiler.md) always returns a source with the
conditionals flattened away, which is another way of saying the same thing `DSH8149` says.

### Line numbers, and what the dependency graph sees

Two invariants make the rest of the toolchain keep working across a cut.

**The output has exactly as many lines as the input.** Every directive line and every cut line is
emitted as an *empty* line rather than removed. Diagnostic line numbers, the
[import line mapping](import.md#source-line-mapping), and the editor's error markers all count lines
inside each file's `// Begin/End DreamShader source:` block, so a source with a hundred lines cut
still reports errors at the line you wrote them on.

**The dependency graph reads the raw file, not the preprocessed one.** It scans for `import` lines
without evaluating any condition, so a file's dependencies are the **union over all branches**.
Editing a `.dsh` that only a dead branch imports still marks its dependents for rebuild.

That is deliberate, and it is the conservative direction: rebuilding more often than strictly
necessary costs time, while failing to rebuild produces an asset that does not match its source.

### Rebuilds

The [build key](../generation/caching.md#it-is-a-build-key-not-just-a-source-hash) folds in the
defines a source **actually read**, not the whole table. Changing a define rebuilds the sources that
read it and skips the ones that do not.

| What was read | Recorded as |
| :-- | :-- |
| a define that was defined | its value |
| a define that was read while **not** defined | the sentinel `<undef>` |
| a define only a short-circuited operand would have read | nothing — it was not read |
| a define mentioned inside a branch that was cut | nothing — see below |

The sentinel is the part that is easy to get wrong and expensive to get wrong: without it, a name
absent from the map is a name whose *later definition* would not change the hash, and a source that
starts taking a different branch without its asset rebuilding is silent data loss.

Omitting the defines inside a cut branch is nonetheless sound. Preprocessing is deterministic, and
the position of the *k*-th evaluated condition depends only on the results of the *k*−1 before it, so
if every recorded value is unchanged the output is byte-identical. A define inside a dead branch
cannot matter until the condition that killed the branch changes — and that condition **is** recorded.

> [!NOTE]
> The test is **who decided this read**. A name that a `#define` earlier in the same file had defined,
> or that a `#undef` there had removed, is answered entirely by the source text — which is already in
> the hash — so it is not recorded. Every other read is answered by the injected table and is
> recorded, with the sentinel when the table had nothing. That second half is the one that is easy to
> talk yourself out of: `#if defined(FOO)` where nobody defines `FOO` is false *because the table has
> no `FOO`*, so skipping it would mean that injecting `FOO` later changes no hash, rebuilds nothing,
> and silently takes the other branch.

Changing the define table also bumps the revision that the ThinCustom in-memory materials are keyed
against, so the editor rebuilds them rather than leaving materials on screen that were generated
under the previous set. Without that, what the editor showed and what a commandlet wrote to disk
could disagree with no visible cause.

## Notes

- **Directives are found by a line scan, so a block comment does not hide one.** A `#if` inside
  `/* … */` is still a directive, exactly as an `import` inside a block comment is still an import.
  Comment directives out with `//` per line. See [`import`](import.md#recognition) and
  [Lexical elements](lexical.md#comments).
- **`#if` works inside a `Graph` body.** A `Graph` body is DreamShaderLang, not HLSL, so its
  conditionals are DreamShader's. `Function` and `GraphFunction` bodies are the exception, and the
  only one — see [above](#function-and-graphfunction-bodies-are-the-shader-compilers).
- **Cutting away a whole `Shader` block does not delete its asset.** The file simply generates
  nothing, the previously generated asset is left on disk as an orphan, and a warning says so.
  Silently deleting an asset is far more dangerous than leaving one behind; remove it yourself, or
  guard the narrower thing instead of the whole block.
- **A define read through `defined(X)` is recorded like any other read.** Defining `X` later
  therefore rebuilds the sources that asked about it, including the ones that only asked whether it
  existed.
- **DreamShader's preprocessor has no `#include` and no macro expansion.** A `#define`'s value is a
  value for a `#if` to test; it is never substituted into the source text, and there are no
  function-like macros. Use [`import`](import.md) to bring in another file — writing `#include` at
  the declaration level is `DSH1035`, and the message says so. The `#include` that DreamShaderLang
  does have is an HLSL directive inside a `Function` body, where the preprocessor does not look at
  all; it is hoisted into the generated `.ush` and resolved by the shader compiler. See
  [Function ▸ Includes](function.md#includes).
- **A source with no directives comes back byte for byte.** Line endings are untouched and no
  trailing newline is added, so putting a file through the preprocessor can never be the reason it
  shows up as modified.
- **Nesting is counted inside skipped branches too.** A `#if` nested under a false `#if` still adds a
  level, because the scanner has to pair directives it is not evaluating. Sixty-four levels are
  legal; the sixty-fifth is `DSH1037`.
- **The parser never sees a directive.** By the time the declaration grammar runs, every directive
  line is an empty line, which is why none of the eight keywords appears in the
  [keyword index](keywords.md) as something the parser knows.
- **The `.dsh` / `.dsf` content rule is applied to the cut text.** That scan is a literal substring
  search over the file after its directives have been resolved, so a `Shader(` inside a branch that
  was cut is no longer there to be rejected — and one inside a branch that was *taken* still is. A
  header may therefore carry a `Shader` block behind a `#if` that is never true, which is legal and
  almost certainly a mistake. See [Source files](source-files.md#how-the-restriction-is-enforced).

## Diagnostics

Preprocessor failures are raised at the **driver** stage, alongside the source-file and import
diagnostics, and carry the file and line of the offending directive.

| Code | Cause |
| :-- | :-- |
| `DSH1030` | end of file reached with a `#if` still open |
| `DSH1031` | `#endif` with no matching `#if` |
| `DSH1032` | `#elif` or `#else` with no matching `#if` |
| `DSH1033` | `#elif` or a second `#else` after an `#else` |
| `DSH1034` | the conditional expression is **incomplete or malformed** |
| `DSH1035` | a `#` line outside a `Function` body that is neither one of the eight lowercase keywords nor `#Region` / `#EndRegion` — `#include`, `#endfi`, and mis-cased spellings such as `#IF` |
| `DSH1036` | `#if` or `#elif` with no expression after it |
| `DSH1037` | nesting deeper than 64 levels — the 65th `#if` is the one that fails |
| `DSH1038` | `#define` or `#undef` with a missing name, or one that is not `[A-Za-z_][A-Za-z0-9_]*` |
| `DSH1039` | an attempt to define or undefine a reserved `DS_` name |
| `DSH1040` | a string where an integer was required: a comparison against a number, arithmetic, `!`, `&&`, `\|\|`, or a bare boolean test |
| `DSH1041` | division or modulo by zero |
| `DSH1042` | the expression or name **parsed completely** and there are still tokens after it |
| `DSH8149` | **Adopt Into Source** on a file containing directives — see [above](#a-source-with-directives-cannot-be-adopted) |
| `DSH9001` | **VirtualFunction sync** on a file containing directives — see [above](#virtualfunction-sync-refuses-the-same-sources) |

`DSH1034` and `DSH1042` split on one mechanical question — *did the expression finish?* — with no
judgement involved:

| Written | Code | Because |
| :-- | :-- | :-- |
| `#if (1` | `DSH1034` | the parenthesis never closes |
| `#if 1 &&` | `DSH1034` | `&&` has no right operand |
| `#if 1 &&)` | `DSH1034` | `)` is not a valid operand, so the expression stops unfinished there |
| `#if 1 2` | `DSH1042` | `1` is already a complete expression |
| `#if 1)` | `DSH1042` | likewise — the `)` is surplus |
| `#ifdef A B` | `DSH1042` | `A` is a valid name; `B` is surplus |
| `#undef A B` | `DSH1042` | same |
| `#else junk` | `DSH1042` | `#else` takes no operand at all |
| `#define A B C` | *none* | `#define` takes its value to end of line |

The split is by **what the reader sees**, not by which component threw. `#ifdef A B` desugars to
`#if defined(A) B`, so any rule that gave the sugar and the desugared form different codes would be
incoherent. Whether the surplus token is a `B`, a `2` or a `)`, it is one error with one fix: delete
it, or comment it out.

Full text and per-code notes are in [DSH1xxx](../diagnostics/DSH1xxx.md),
[DSH8xxx](../diagnostics/DSH8xxx.md) and [DSH9xxx](../diagnostics/README.md).

## Example

The Substrate case, which is the reason the feature exists. Two of the three differences are in the
declaration layer, where no switch node can reach: the `ShadingModel` line is a `Settings` key, and
the two branches do not declare the same `Outputs` — not the same *names*, and not even the same
*types*. A `Substrate` value and a `vec3` are not two inputs of one switch node; they are two
different materials.

```c
Shader(Name="Materials/M_Foo", Root="Game")
{
    Settings = {
        Domain = "Surface";
#if !DS_SUBSTRATE
        ShadingModel = "DefaultLit";
#endif
    }

    Properties {
        vec3 Tint = vec3(0.8, 0.2, 0.1);
    }

    Outputs = {
#if DS_SUBSTRATE
        Substrate Surface;
        Base.FrontMaterial = Surface;
#else
        vec3 BaseColor;
        Base.BaseColor = BaseColor;
#endif
    }

    Graph = {
#if DS_SUBSTRATE
        Surface = Substrate.Slab(DiffuseAlbedo = Tint, Roughness = 0.4);
#else
        BaseColor = Tint;
#endif
    }
}
```

The Substrate branch writes no `ShadingModel` at all, because binding `Base.FrontMaterial`
[force-sets it](../builtins/substrate.md#binding-a-substrate-value-to-basefrontmaterial) and an
explicit setting there may only say `"Substrate"`. Guarding the *other* branch's line is what lets
one source satisfy both rules.

With `DS_SUBSTRATE` at `1`, the text that reaches the parser is:

```c
Shader(Name="Materials/M_Foo", Root="Game")
{
    Settings = {
        Domain = "Surface";



    }

    Properties {
        vec3 Tint = vec3(0.8, 0.2, 0.1);
    }

    Outputs = {

        Substrate Surface;
        Base.FrontMaterial = Surface;




    }

    Graph = {

        Surface = Substrate.Slab(DiffuseAlbedo = Tint, Roughness = 0.4);



    }
}
```

Thirty-one lines in, thirty-one lines out. An error inside the `Graph` block is still reported on the
line the author wrote it on: the `Substrate.Slab` call is line 26 of both texts.

## See also

- [Layout](layout.md#region--endregion) — `#Region` / `#EndRegion`, the other `#` syntax
- [`import`](import.md) — what `#if` may wrap, and the line mapping the blank lines protect
- [Function](function.md#includes) — HLSL bodies, and the `#include` hoist
- [Lexical elements](lexical.md) — comments, and why a block comment does not hide a directive
- [Source files](source-files.md) — the three file kinds directives may appear in
- [Project settings](../settings/project.md) — *Preprocessor Defines*
- [Commandlet](../tools/commandlet.md) — `-Define=NAME=VALUE`
- [Caching](../generation/caching.md) — the build key the touched defines fold into
- [Divergence](../generation/divergence.md) — Adopt, Revert and Detach
- [VirtualFunction tools](../tools/virtual-function-tools.md) — the startup sync that refuses with `DSH9001`
- [Decompiler](../tools/decompiler.md) — the direction that cannot round-trip
- [Parameter nodes](../parameters/parameter-nodes.md) — `StaticSwitchParameter`, the other way to branch
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
