# Disabling diagnostics

> [DreamShader](../index.md) » [DreamShaderLang 2.0](index.md) » **Disabling diagnostics**

*(after 2.1.2)* A source can turn off warnings and errors by their code, for the whole file or for
one line. Both languages take every spelling below.

| Spelling | Disables |
| :-- | :-- |
| `#pragma disable_warning(DSH7224, DSH8237)` | those warnings, everywhere in the file — and, in the file being compiled, every diagnostic of the compile, the headers it includes as well |
| `#pragma disable_error(DSH8219)` | those errors, the same way |
| `code; // disable_warning DSH5251` | that warning on this line |
| `// disable_error DSH4210` alone on its line | that error on the next line that holds code (a blank line or an ordinary comment in between does not count; a `///` line does) |

A list is codes separated by commas or spaces; case does not matter. A word that is not a code is
[`DSH7236`](../diagnostics/DSH7xxx.md#dsh7236) and is ignored, and so is a list with no code at all —
neither disables "everything". The `#pragma` lines may stand anywhere at file scope; their position
does not matter.

```hlsl
#pragma disable_warning(DSH9011)        // this material is slow to compile on purpose

// disable_warning DSH7224
/// @library Legacy
float Helper(float x) { return x; }
```

A `///` line is documentation to the end of the line, so a `// disable_…` after a doc comment is part
of the doc text; put it on the line above, as here.

## What disabling does

- **A warning** is not reported: not in the compile's result, not in the diagnostics the bridge
  writes, not in the editor's log.
- **An error** is not reported **and does not count**: a stage that would have stopped because of it
  goes on. Whatever the error was about is then whatever the compiler makes of it — a later stage may
  report something else instead. When a stage cannot go on without it and nothing else says why, the
  compile fails with the disabled error after all, followed by
  `(The source disables DSHnnnn, but the compile could not go on past it.)`.
- **An info** cannot be disabled — the pipeline reads some of them back (a skipped asset, a parent
  compiled first).

## Where the lists are read from

From the text of each file, line by line, before the file is parsed: a `//` comment never reaches the
parser, and an `#if` does not hide either spelling. The file being compiled is read when the compile
starts. A header is read when one of its own diagnostics comes up, and its lists cover that header
only — its `#pragma disable_*` lines do not reach the files that include it.

A source compiled first on behalf of another (a `.dsi`'s parent) is compiled with its own lists, not
with those of the source that asked for it.

The project-wide warnings with no position — [`DSH9011`](../diagnostics/DSH9xxx.md#dsh9011),
[`DSH9012`](../diagnostics/DSH9xxx.md#dsh9012), `DSH8155` — can only be disabled by `#pragma
disable_warning`.

## See also

- [Diagnostics index](../diagnostics/index.md) — every `DSHnnnn`
- [NoGeneration](../generation/no-generation.md) — another file-level `#pragma`
