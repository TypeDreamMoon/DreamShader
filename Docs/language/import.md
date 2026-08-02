# import

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **import**

A line directive that inlines another DreamShaderLang source file into the current translation unit.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` |
| Kind | directive |
| Processed by | the editor source loader, **before** the declaration parser runs |
| Recognized | line by line, on the whole file, in text order |
| Case rule | `import` is matched case-insensitively |

## Synopsis

```c
import "<specifier>" [;] [// <comment>]
import '<specifier>' [;] [// <comment>]
```

The directive must be the first thing on its line — leading whitespace is allowed, anything else is
not. After the closing quote only an optional `;` *(since 1.2.2)* and an optional `//` comment may
follow.

## Recognition

Each physical line is tested against these rules, in order. A line that fails any of them is not an
import and is passed through to the parser unchanged.

| # | Rule |
| :-- | :-- |
| 1 | the trimmed line must not start with `//` |
| 2 | the trimmed line must start with `import`, ignoring case |
| 3 | the character after `import` must be whitespace, unless the line is exactly `import` — this is what rejects `importfoo` |
| 4 | the remainder, trimmed, must start with `"` or `'` |
| 5 | the closing quote must match the opening one; a `\` inside the quotes escapes the next character |
| 6 | an unterminated quote makes the line an ordinary line, not an error |
| 7 | after the closing quote, an optional `;` may follow |
| 8 | after that, the rest of the line must be empty or start with `//` |
| 9 | the extracted specifier must be non-empty after trimming |

> [!WARNING]
> Rule 1 only knows about `//`. An `import` line inside a `/* … */` block comment **is still
> honoured** — the loader has no notion of block comments. Commenting out a block of imports with
> `/* … */` silently keeps importing them; use `//` on each line instead.

> [!NOTE]
> `import` is not a keyword in the declaration grammar. If the parser is handed raw text that still
> contains an import line — for example by calling the runtime parser directly instead of going
> through the source loader — that line fails with `Unexpected token near index {Index}.`

An `import` must be alone on its line. `Shader(Name="X") import "Common.dsh";` is not recognized as
an import, and the text is handed to the parser as written.

## Specifier normalization

The extracted specifier is normalized before resolution:

| # | Step |
| :-- | :-- |
| 1 | trim leading and trailing whitespace |
| 2 | replace every `\` with `/` |
| 3 | strip **all** leading `./` sequences |
| 4 | if the result has no extension at all, append `.dsh` |

So `import "Shared/Common"` and `import "Shared/Common.dsh"` are the same directive. Importing a
`.dsf` or a `.dsm` therefore **requires the explicit extension** *(`.dsf` since 1.3.5)*.

> [!NOTE]
> Step 4 asks whether the path has an extension, not whether it has a known one, and it looks only at
> the last path segment. A file name containing a `.` — `Shared/Common.v2` — counts as "already has
> an extension", so no `.dsh` is appended and the specifier resolves only if a file with exactly that
> name exists. A `.` in a *directory* component — `@scope/pkg.v2/Lib` — does not count, and `.dsh` is
> still appended.

## Resolution

Three candidate paths are tried in order. Each is paired with a **containment root**; a candidate
that resolves outside its root is skipped rather than reported, and the first candidate that exists
on disk wins.

| # | Candidate | Containment root |
| :-- | :-- | :-- |
| 1 | `<directory of the importing file>/<specifier>` | the longer of the source directory and the packages directory that contains the importing file; the importing file's own directory when it is under neither |
| 2 | `<source directory>/<specifier>` | the source directory |
| 3 | `<packages directory>/<specifier>` | the packages directory |

| Directory | Default | Project setting |
| :-- | :-- | :-- |
| Source | `<Project>/DShader` | *Source Directory* |
| Packages | `<Source>/Packages` | derived; not separately configurable |

The containment comparison is case-insensitive on every platform. Whether a candidate is then found
still goes through an ordinary file-existence check, which follows the file system's own case
behaviour.

The containment check is what stops a specifier from climbing out of the tree. `..` segments are
resolved before the check, so:

- from a file directly under `DShader`, `import "../Secret.dsh"` resolves above the source directory
  and candidate 1 is skipped; candidates 2 and 3 collapse the same `..` and land outside their own
  roots, so they are skipped by the same check;
- from a file under `DShader/Packages/@scope/pkg/`, `..` may traverse anywhere inside
  `DShader/Packages`, because that is the containment root chosen for it;
- for a source file under neither directory, the containment root is its own directory, so no `..`
  specifier can resolve at all.

### Package-style paths

`@scope/name/…` is **not** a distinct path syntax. `@` is an ordinary directory-name character, and a
specifier such as `"@typedreammoon/dream-noise/Library/Noise.dsh"` resolves through candidate 3
simply because `DShader/Packages/@typedreammoon/dream-noise/Library/Noise.dsh` exists on disk. There
is no scope registry, no version resolution and no special-cased root.

> [!NOTE]
> Candidates 1 and 2 are still tried first. A file named
> `@typedreammoon/dream-noise/Library/Noise.dsh` next to the importing file, or under `DShader`
> itself, shadows the package copy.

See [Packages](../tools/packages.md) for the directory layout this convention assumes.

## Inlining, cycles and ordering

The loader walks the import graph depth-first and produces one flat text for the parser.

| Behaviour | Rule |
| :-- | :-- |
| order | an import is fully inlined **before** the rest of the importing file is emitted, so a dependency always precedes its dependent |
| diamonds | a file already inlined anywhere in this translation unit is skipped silently — its text appears exactly once |
| cycles | re-entering a file that is still being inlined fails with `DreamShader import cycle detected at '{Path}'.` |
| unreadable files | `DreamShader could not read '{Path}'.` |
| unresolved specifiers | `DreamShader import '{Specifier}' referenced from '{Path}' could not be resolved.` |

Each file's contribution is wrapped in marker comments, and every import line is replaced by an empty
line so that the lines below it keep their original numbers:

```text
// Begin DreamShader source: <absolute path>
…file text, with each import line blanked…

// End DreamShader source: <absolute path>
```

> [!WARNING]
> Because the whole closure becomes one parse unit, the "at most one `Shader` block" rule is
> closure-wide. Importing two files that each declare a `Shader` fails with
> `Only one top-level Shader block is currently supported.`, even though neither file breaks the rule
> on its own.

> [!NOTE]
> The `.dsh` / `.dsf` content rules are applied to each file's own text, not to the assembled
> closure. A `.dsh` may import a `.dsf` that declares `ShaderFunction` blocks, and those blocks are
> compiled as part of the translation unit. See [Source files](source-files.md).

## Source-line mapping

Diagnostics are mapped back from the assembled text to the file the author wrote *(fixed in 1.4.1)*.

- The mapper scans the error text for the literal `near index ` and reads the integer that follows.
  **That is the only channel by which a parse error carries a position** — which is why so many
  messages end in `near index {Index}.`
- It then walks the assembled text, tracking the current `// Begin DreamShader source:` file and a
  per-file line counter that resets at each marker. Marker lines do not advance the counter.
- A located message is formatted `<file>(<line>,<column>): <message>`; when mapping fails the form is
  `<file>: <message>`.
- `Graph` errors are anchored separately: the parser records where each `Graph` body starts, and a
  graph-relative line and column are offset onto that origin. The column offset applies only to the
  first line of the body.

> [!NOTE]
> Three limits are worth knowing when a reported position looks wrong.
>
> 1. Errors raised inside a section body carry an index relative to **that body**, but the mapper
>    treats every index as an offset into the assembled text. Positions for in-section errors are
>    therefore not reliable.
> 2. An index landing exactly on a line's first character can be attributed to the previous line.
> 3. Most statement-level messages carry no `near index` at all and are reported as
>    `<file>: <message>` with no line or column.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

| Message | Cause |
| :-- | :-- |
| `DreamShader import '{Specifier}' referenced from '{Path}' could not be resolved.` | none of the three candidates existed, or every existing one was outside its containment root |
| `DreamShader import cycle detected at '{Path}'.` | a file imported itself, directly or transitively |
| `DreamShader could not read '{Path}'.` | the resolved file could not be loaded |
| `Only one top-level Shader block is currently supported.` | two `Shader` blocks in the closure |
| `Unexpected token near index {Index}.` | an `import` line reached the declaration parser |
| `DreamShader header '{Path}' may only declare Function/Namespace/GraphFunction/VirtualFunction blocks and imports.` | an imported `.dsh` breaks its content rule |
| `DreamShader function file '{Path}' may only declare imports, Function/Namespace/GraphFunction/VirtualFunction blocks, and ShaderFunction/ShaderLayer/ShaderLayerBlend blocks.` | an imported `.dsf` breaks its content rule |

## Example

```text
<Project>/DShader/
├── Materials/
│   └── M_Water.dsm
├── Functions/
│   └── F_Tint.dsf
├── Shared/
│   └── Common.dsh
└── Packages/
    └── @typedreammoon/
        └── dream-noise/
            └── Library/
                └── Noise.dsh
```

```c
// DShader/Shared/Common.dsh
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

```c
// DShader/Materials/M_Water.dsm
import "Shared/Common";                                   // -> DShader/Shared/Common.dsh
import '@typedreammoon/dream-noise/Library/Noise.dsh';    // -> DShader/Packages/@typedreammoon/...
import "../Functions/F_Tint.dsf"                          // -> DShader/Functions/F_Tint.dsf

Shader(Name="Materials/M_Water")
{
    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        vec3 Base = vec3(0.1, 0.3, 0.6);
        Color = Common::ApplyTint(Base, vec3(1.0, 0.9, 0.8));
    }
}
```

The first specifier has no extension, so `.dsh` is appended. The second is found only by candidate 3.
The third climbs one directory, which stays inside `DShader`, so candidate 1 accepts it. All three
directives omit or include the `;` freely.

Assembled text handed to the parser:

```text
// Begin DreamShader source: <Project>/DShader/Shared/Common.dsh
Namespace(Name="Common")
…
// End DreamShader source: <Project>/DShader/Shared/Common.dsh

// Begin DreamShader source: <Project>/DShader/Packages/@typedreammoon/dream-noise/Library/Noise.dsh
…
// End DreamShader source: …/Noise.dsh

// Begin DreamShader source: <Project>/DShader/Functions/F_Tint.dsf
…
// End DreamShader source: <Project>/DShader/Functions/F_Tint.dsf

// Begin DreamShader source: <Project>/DShader/Materials/M_Water.dsm
                                     <- three blank lines where the imports were
Shader(Name="Materials/M_Water")
…
// End DreamShader source: <Project>/DShader/Materials/M_Water.dsm
```

## See also

- [Source files](source-files.md) — the content rule applied to each imported file
- [Lexical elements](lexical.md) — why a `//` inside an import line is tolerated and a `/* */` is not
- [Namespace](namespace.md) — the usual reason to import a header
- [Function](function.md) · [GraphFunction](graph-function.md) — what a `.dsh` may declare
- [VirtualFunction](virtual-function.md) — declarations written to `DShader/VirtualFunctions`
- [Packages](../tools/packages.md) — `DShader/Packages` and the `@scope/name` layout
- [Project settings](../settings/project.md) — *Source Directory*, which moves all three search roots
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
