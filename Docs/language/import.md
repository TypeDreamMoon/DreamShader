# import

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **import**

A top-level line that makes the declarations of a `.dsh` header visible in the current file.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh`; a `.dss` spells it `#include "…"` and takes `import "…";` as well |
| Kind | top-level declaration |
| Names | a `.dsh` header *(since 2.0.0; through 1.9.x also a `.dsf`)* |
| Processed by | the legacy front end, which reads the line; the binder, which resolves it through the include resolver and declares the header's names into this file *(since 2.0.0; through 1.9.x the editor's source loader pasted the header's text in before parsing)* |
| Case rule | `import` is lower case; another casing is read the same, with the warning [`DSH2253`](../diagnostics/DSH2xxx.md#dsh2253) |

## Synopsis

```c
import "<path>" [;]
import '<path>' [;]          // in a .dsm, .dsf or .dsh

<path> := a path to a .dsh, relative to this file or to its source root,
          or "/"-anchored; the ".dsh" may be left off
```

A path resolves inside the importing file's own source root — see [Roots](#roots). The
root-qualified form of 1.6.0 (`Project:…`, `Plugin.<Name>:…`) is refused *(since 2.0.0)*.

## Recognition

*(since 2.0.0)* `import` is a token of the language, read by the parser like any other top-level
declaration. The 1.x rule that recognized it line by line, in the raw text, is gone.

| Rule | |
| :-- | :-- |
| where | at the top level of the file, between blocks |
| comments | ordinary comments: an `import` inside `/* … */` or after `//` is commented out *(the 1.x loader honoured one inside `/* … */`)* |
| the path | a string: `"…"`, with the usual escapes; in a 1.x file or a `.dsh` also `'…'` on one line |
| `;` | optional |
| `#if` | resolved first, by the [preprocessor](preprocessor.md): an `import` in a branch that is not taken is not read |

| Refused | Code |
| :-- | :-- |
| `import` not followed by a string | [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252) |
| a path that names a `.dsf` or a `.dsm` — a material or function file is compiled on its own, never included | `DSH2252` |
| a root-qualified path (`Project:Shared/Common.dsh`, `Plugin.X:…`) | `DSH2252` |
| a path that resolves to a file that is no `.dsh` (or `.dss`) | [`DSH8295`](../diagnostics/DSH8xxx.md#dsh8295) |

> [!NOTE]
> The editor's **dependency scanner** still reads `import` lines as text, to know which sources to
> recompile when a header is saved: a trimmed line that starts with `import`, ignoring case, then a
> `"…"` or `'…'` path, an optional `;` and nothing but a `//` comment. It reads the raw file, `#if`
> branches included, so a header imported only from a branch that is not taken still triggers a
> rebuild of its importers. That is deliberate: a needless rebuild costs seconds, a missed one leaves
> a stale asset.

## Specifier normalization

The path is normalized before resolution:

| # | Step |
| :-- | :-- |
| 1 | trim leading and trailing whitespace |
| 2 | replace every `\` with `/` |
| 3 | strip **all** leading `./` sequences |
| 4 | if the result has no extension at all, append `.dsh` |

So `import "Shared/Common"` and `import "Shared/Common.dsh"` are the same directive.

> [!NOTE]
> Step 4 asks whether the path has an extension, not whether it has a known one, and it looks only at
> the last path segment. A file name containing a `.` — `Shared/Common.v2` — counts as "already has
> an extension", so no `.dsh` is appended and the specifier resolves only if a file with exactly that
> name exists. A `.` in a *directory* component — `@scope/pkg.v2/Lib` — does not count, and `.dsh` is
> still appended.

## Resolution

| Path | Resolved against |
| :-- | :-- |
| an absolute file path that exists | that file |
| `/`-anchored *(since 2.0.0)*: `/Shared/Common.dsh`, or `/Game/Shared/Common.dsh` | the anchor (`/` or `/Game/`) dropped, then the importing file's own root — its source directory, then its packages directory — then every other root's two directories, in root order; the first file that exists wins. Never relative to the importing file |
| anything else | the three candidates below |

The three candidates are tried in order. Each is paired with a **containment root**; a candidate that
resolves outside its root is skipped rather than reported, and the first candidate that exists on
disk wins.

| # | Candidate | Containment root |
| :-- | :-- | :-- |
| 1 | `<directory of the importing file>/<specifier>` | the longest source or packages directory, across every root, that contains the importing file; the importing file's own directory when it is under none |
| 2 | `<owning root's source directory>/<specifier>` | that source directory |
| 3 | `<owning root's packages directory>/<specifier>` | that packages directory |

| Directory | Default | Project setting |
| :-- | :-- | :-- |
| Source | `<Project>/DShader` | *Source Directory* |
| Packages | `<Source>/Packages` | derived; not separately configurable |

The containment comparison is case-insensitive on every platform. Whether a candidate is then found
still goes through an ordinary file-existence check, which follows the file system's own case
behaviour.

A path that resolves nowhere is [`DSH8292`](../diagnostics/DSH8xxx.md#dsh8292), naming the reason.

### Roots

Candidates 2 and 3 belong to the **owning root** of the importing file — the source root the file
sits under. A relative specifier never leaves it: a `.dsm` under `Plugins/MoonToon/DShader` cannot
reach `<Project>/DShader` by writing `Shared/Common.dsh`, and vice versa.

The rule exists so that adding a plugin cannot change what an existing import means. Were the
project root a global fallback, two plugins shipping `Shared/Common.dsh` would resolve by scan order,
and disabling a plugin would silently redirect another root's imports.

A file under **no** root — a test fixture, a commandlet `-Source` pointing outside the tree — takes
the project's source and packages directories for candidates 2 and 3, which is what every file did
before roots existed.

See [Source files](source-files.md#source-roots) for the root list and how plugins contribute to it.

### Crossing a root deliberately

Through 1.9.x a root qualifier and a `:` — `Project:Shared/Common.dsh`,
`Plugin.MoonToon:Shared/Toon.dsh` *(since 1.6.0)* — pointed an import at another root's source and
packages directories. An `import` refuses that form *(since 2.0.0)* with
[`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252). A header another root ships is reached by a
`/`-anchored path, which looks in the importing file's own root first and then in every other root —
so a file of the same name in the own root wins — or by copying it into this root, or through a
[package](../tools/packages.md).

> [!WARNING]
> A plugin that reaches into the project root is no longer self-contained: ship it to another project
> and the import dangles.

The containment check is what stops a relative specifier from climbing out of the tree. `..` segments
are resolved before the check, so:

- from a file directly under `DShader`, `import "../Secret.dsh"` resolves above the source directory
  and candidate 1 is skipped; candidates 2 and 3 collapse the same `..` and land outside their own
  roots, so they are skipped by the same check;
- from a file under `DShader/Packages/@scope/pkg/`, `..` may traverse anywhere inside
  `DShader/Packages`, because that is the containment root chosen for it;
- for a source file under no root's source or packages directory, the containment root is its own
  directory, so no `..` specifier can resolve through candidate 1 at all.

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

## How a header is read

*(since 2.0.0)* Nothing is inlined. Each header is read on its own and its declarations are added to
the importing file's.

| Behaviour | Rule |
| :-- | :-- |
| preprocessing | each header is preprocessed **on its own**, against the compile's define table: a `#define` in a header does not reach the file that imports it. A header whose `#if` fails is [`DSH8291`](../diagnostics/DSH8xxx.md#dsh8291) |
| parsing | each header is parsed on its own, in its own dialect: a `.dsh` may hold 1.x blocks and 2.0 declarations side by side, whichever kind of file imports it. A header with a syntax error reports that error at its own line, and the import is then [`DSH8294`](../diagnostics/DSH8xxx.md#dsh8294); one that cannot be read is [`DSH8293`](../diagnostics/DSH8xxx.md#dsh8293) |
| names | every declaration of the header — and of the headers it imports — is visible in the importing file. A name declared twice, in the header and the importing file or in two headers, is [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210) |
| diamonds | a header reached twice is declared once |
| cycles | a header that imports itself, directly or through another, is [`DSH4211`](../diagnostics/DSH4xxx.md#dsh4211) |
| assets | none: a header holds no asset block ([`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249)), so an import adds names, never an asset, and a file still holds at most one `Shader` ([`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250)) |
| rebuilds | the preprocessed text of every header a compile read is part of that source's [build key](../generation/caching.md), and saving a header queues every source that imports it |

> [!NOTE]
> **Through 1.9.x** the loader pasted the whole import closure into one text before parsing, so the
> "one `Shader`" rule spanned the closure, a `.dsh` could import a `.dsf` and compile its
> `ShaderFunction` blocks as part of the importing file, and every position had to be mapped back
> from the assembled text.

## Source-line mapping

There is nothing to map *(since 2.0.0)*. A diagnostic carries the file it was raised in — the header,
for an error inside one — and the line and column of the construct, and is printed
`<file>(<line>,<column>): DSHnnnn: <message>`. Through 1.9.x positions came from a `near index`
offset into the assembled text and were unreliable inside sections; that mechanism is gone. See
[Diagnostics](../diagnostics/index.md#message-locations).

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252) | the line is not an import of a header: no string after `import`, a `.dsf` / `.dsm` path, or a root-qualified path |
| [`DSH2253`](../diagnostics/DSH2xxx.md#dsh2253) | *(warning)* `import` written in another case |
| [`DSH8292`](../diagnostics/DSH8xxx.md#dsh8292) | the path resolves to no file |
| [`DSH8295`](../diagnostics/DSH8xxx.md#dsh8295) | it resolves to a file that is not a header |
| [`DSH8293`](../diagnostics/DSH8xxx.md#dsh8293) | the header cannot be read |
| [`DSH8291`](../diagnostics/DSH8xxx.md#dsh8291) | the header fails conditional compilation |
| [`DSH8294`](../diagnostics/DSH8xxx.md#dsh8294) | the header cannot be parsed; its own errors are reported first |
| [`DSH4211`](../diagnostics/DSH4xxx.md#dsh4211) | the import closes a cycle |
| [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210) | a name the header declares is declared again |
| [`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249) | the header holds an asset block |

## Example

```text
<Project>/DShader/
├── Materials/
│   └── M_Water.dsm
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

Shader(Name="Materials/M_Water")
{
    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        vec3 Water = vec3(0.1, 0.3, 0.6);
        Color = Common::ApplyTint(Water, vec3(1.0, 0.9, 0.8));
    }
}
```

The first path has no extension, so `.dsh` is appended; candidate 1 does not exist beside
`M_Water.dsm`, so candidate 2 finds it under `DShader`. The second is found only by candidate 3.
Both directives omit or include the `;` freely.

Each header is preprocessed and parsed on its own; `Common::ApplyTint` and whatever `Noise.dsh`
declares are then names of `M_Water.dsm`, and a diagnostic inside `Common.dsh` is reported at
`DShader/Shared/Common.dsh(<line>,<column>)`.

The `.dss` spelling of the same two lines:

```hlsl
#include "Shared/Common.dsh"
#include "@typedreammoon/dream-noise/Library/Noise.dsh"
```

## See also

- [Source files](source-files.md) — what each kind of file may hold
- [Lexical elements](lexical.md) — strings and comments
- [Namespace](namespace.md) — the usual reason to import a header
- [Function](function.md) · [GraphFunction](graph-function.md) — what a `.dsh` may declare
- [VirtualFunction](virtual-function.md) — declarations written to `DShader/VirtualFunctions`
- [Packages](../tools/packages.md) — `DShader/Packages` and the `@scope/name` layout
- [Preprocessor](preprocessor.md) — `#if`, which runs on each file before it is parsed
- [Project settings](../settings/project.md) — *Source Directory*, which moves all three search roots
- [Diagnostics](../diagnostics/README.md) — every code
