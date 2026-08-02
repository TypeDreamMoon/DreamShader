# Release

> [DreamShader](../index.md) » [Contributing](index.md) » **Release**

The tag conventions the release workflow enforces, what the workflow does, and exactly what the
published archive contains.

| | |
| :-- | :-- |
| Declared in | `.github/workflows/release.yml` — the only workflow in the repository |
| Kind | GitHub Actions workflow |
| Runner | `windows-latest`; every step is `pwsh` |
| Permissions | `contents: write` |
| Version source of truth | `DreamShader.uplugin` → `VersionName` and `IsBetaVersion` |

## Synopsis

```powershell
git tag v<major>.<minor>.<patch>[b]
git push origin v<major>.<minor>.<patch>[b]
```

The trailing `b` marks a beta. The workflow also accepts a manual run:

```text
workflow_dispatch(version = [<slug>], prerelease = { true | false })
```

`version` is optional and, when given, must equal the slug the descriptor implies (a leading `v` is
stripped before the comparison). `prerelease` defaults to `false` and can only add the pre-release
flag, never remove it.

## Tag conventions

| Kind | Tag pattern | Example | Slug | Pre-release |
| :-- | :-- | :-- | :-- | :-- |
| Stable | `v*.*.*` | `v1.5.0` | `1.5.0` | no, unless the dispatch input forces it |
| Beta | `v*.*.*b` | `v1.5.0b` | `1.5.0b` | yes |

The tag is **derived, not chosen**: the workflow computes it from the descriptor and refuses to run
if the pushed tag differs. Tagging is therefore a two-step change — edit `DreamShader.uplugin`
first, commit, then tag.

## Version derivation

| Step | Rule |
| :-- | :-- |
| 1 | Read `VersionName` from `DreamShader.uplugin`. Empty is an error. |
| 2 | It must match `^\s*([0-9]+\.[0-9]+\.[0-9]+)`. A decorated name is fine — `"1.5.0 - Beta"` yields the base `1.5.0`. |
| 3 | Beta when `IsBetaVersion` is `true` **or** `VersionName` matches `(?i)beta`. |
| 4 | Slug = base version, plus a `b` suffix when beta. |
| 5 | Tag = `v` + slug. |
| 6 | On a tag push, the pushed tag must equal the derived tag. On a manual run, a non-empty `version` input must equal the slug. |
| 7 | Export `DREAMSHADER_VERSION`, `DREAMSHADER_BASE`, `DREAMSHADER_SLUG`, `DREAMSHADER_TAG` and `DREAMSHADER_PRERELEASE` for the later steps. |

For the current descriptor — `VersionName` `1.5.1`, `IsBetaVersion` `false`, `Version` `151` —
this yields base `1.5.1`, slug `1.5.1`, tag `v1.5.1`, pre-release `false`, and the archive
`DreamShader-1.5.1.zip`.

> [!NOTE]
> `Version` (the integer, `150`) is never read by the workflow. It is the descriptor's own numeric
> version and is only meaningful to Unreal. Bump it alongside `VersionName` by hand.

## Workflow steps

| # | Step | What it does |
| :-- | :-- | :-- |
| 1 | Checkout | `actions/checkout@v4` with `fetch-depth: 0` |
| 2 | Resolve release version | The seven rules above; throws on any mismatch |
| 3 | Package plugin | Stages the shipped items and zips them (see below) |
| 4 | Download latest VSCode extension release assets | `gh release download --repo TypeDreamMoon/dreamshader-language-support --pattern '*'` into `artifacts/vscode-extension`; throws when nothing is downloaded |
| 5 | Build release notes | Extracts this version's `CHANGELOG.md` section and assembles the body |
| 6 | Publish GitHub Release | Creates the release, or updates it if the tag already has one |

## Archive contents

The staging directory is `<RUNNER_TEMP>/DreamShaderRelease/DreamShader/`, and it is compressed to
`artifacts/DreamShader-<slug>.zip`. The archive's single top-level entry is the folder `DreamShader`,
which is what the install instructions tell users to drop into `Plugins/`.

Ten items are copied. A missing item is skipped rather than failing the release, but it now emits a
`::warning::` annotation on the run summary — silence is how three of them stayed out of every
archive up to `1.5.0` without anyone noticing.

| Shipped | |
| :-- | :-- |
| `Source/` | |
| `Resources/` | |
| `Shaders/` | *(since 1.5.1)* `DreamShaderBuiltins.ush`, so `/Plugin/DreamShader/…` resolves in an archive install |
| `Docs/` | |
| `.skill/` | *(since 1.5.1)* the agent skill set and its driver |
| `DreamShader.uplugin` | |
| `README.md` | |
| `README.zh-CN.md` | *(since 1.5.1)* — `README.md` links to it, so the archive's link used to dangle |
| `CHANGELOG.md` | |
| `LICENSE` | |

Everything else in the repository is absent:

| Not shipped | Consequence for an archive install |
| :-- | :-- |
| `Tests/` | The fixture corpus is absent; the two data-driven runners enumerate zero sub-tests |
| `Config/` | `FilterPlugin.ini` is absent. It only holds the stock commented template and declares no packaged files |
| `Images/` | README artwork is missing, so the readme's images do not render locally |
| `.github/` | The workflow itself is not redistributed |
| `Binaries/`, `Intermediate/` | The archive is **source-only**; the consumer's first editor launch compiles the three modules |

> [!NOTE]
> Up to and including `1.5.0` the archive shipped no `Shaders/` folder, so anything resolving
> `/Plugin/DreamShader/...` had no file to read in an install made from the release zip. Installs
> from those archives still need `Shaders/DreamShaderBuiltins.ush` copied in by hand. See
> [HLSL library](../builtins/hlsl-library.md).

> [!NOTE]
> The archive ships no `Binaries/`, so the plugin is compiled by the consuming project. That
> requires a C++ project (or one converted to C++) and a matching engine toolchain — see
> [Contributing](index.md#engine-versions) for the MSVC caveat on UE 5.3 and 5.4.

## Release notes

The body is assembled in this order.

| Part | Content |
| :-- | :-- |
| Pre-release callout | Added only for a beta: a warning that APIs and generated-asset layout may still change before the stable base version, with a link to the issue tracker |
| Installation | Three steps: download `DreamShader-<slug>.zip`, extract the `DreamShader` folder into `Plugins/`, enable **DreamShader** in *Edit ▸ Plugins* and restart |
| Compatibility line | The literal string `Compatible with Unreal Engine 5.3 - 5.8 (Win64).` |
| What's changed | The `## <VersionName>` section of `CHANGELOG.md`, verbatim |
| Assets | A two-row table: the plugin zip, and `*.vsix` for the VSCode extension |
| Links | Documentation `https://lang.64hz.cn/`, the changelog at the release tag, and the VSCode extension repository |

Changelog extraction details:

- The section is found by matching `^##\s+<VersionName>\b`, so the changelog heading must *begin*
  with the `VersionName` exactly as written, decorations and all. Trailing text is fine, which is why
  `## 1.5.1 - 2026-08-02` matches the current `VersionName` `1.5.1`. The failure runs the other way:
  a decorated name such as `1.5.0 - Beta` is **not** matched by a bare `## 1.5.0` heading.
- It ends at the next `##` heading.
- Leading blockquote lines are stripped, so the changelog's own beta note does not duplicate the
  workflow's pre-release callout.
- When no matching section exists, the body falls back to a single link to `CHANGELOG.md` at the
  release tag. **This is silent** — a mistyped heading produces a release with no notes rather than a
  failure.

## Publication

| Situation | Action |
| :-- | :-- |
| No release exists for the tag | `gh release create` with the zip and every downloaded VSCode asset |
| The run is a manual dispatch, not a tag push | `--target <commit sha>` is added so the tag is created on the dispatched commit |
| A release already exists for the tag | `gh release upload --clobber` for the assets, then `gh release edit` for the title and notes |
| Beta, or the dispatch input `prerelease` is `true` | `--prerelease` on create, or a second `gh release edit --prerelease` on update |

The release title is `DreamShader <VersionName>` — the name verbatim, decorations and all, for
example `DreamShader 1.5.1`. Assets are the plugin zip followed by every file downloaded from the
VSCode extension's latest release, sorted by name.

## Diagnostics

Every message the workflow can throw. Runtime substitutions are shown as `{Placeholder}`.

| Message | Step | Cause |
| :-- | :-- | :-- |
| `DreamShader.uplugin VersionName is empty.` | Resolve release version | `VersionName` is absent or whitespace |
| `DreamShader.uplugin VersionName '{VersionName}' must start with an X.Y.Z version.` | Resolve release version | `VersionName` does not begin with three dot-separated integers |
| `Pushed tag '{PushedTag}' does not match the tag derived from DreamShader.uplugin ('{DerivedTag}'). Fix VersionName / IsBetaVersion, or retag.` | Resolve release version | The tag and the descriptor disagree — usually a forgotten `b`, or a bump that was tagged before it was committed |
| `Requested version '{Input}' does not match DreamShader.uplugin ('{Slug}').` | Resolve release version | A manual run passed a `version` input that is not the derived slug |
| `No assets were downloaded from TypeDreamMoon/dreamshader-language-support latest release.` | Download VSCode assets | The extension repository's latest release has no assets, or the download failed |

Each step sets `$ErrorActionPreference = 'Stop'`, so any other PowerShell failure aborts the job as
well.

## Notes

- The workflow never builds the plugin. It stages files and publishes them; compilation happens on
  the consumer's machine. Run `RunUAT BuildPlugin` yourself before tagging — see
  [Contributing](index.md#building).
- The workflow never runs the automation suite either. See [Testing](testing.md).
- Re-running the workflow for an existing tag is safe: assets are clobbered and the notes are
  rewritten from the current `CHANGELOG.md`.
- The VSCode extension is released from its own repository. The workflow always pulls that
  repository's **latest** release, so a plugin release published while the extension is mid-release
  will carry the previous `.vsix`.
- `DocsURL` in the descriptor points at <https://lang.64hz.cn/>, which is built from `Docs/`. The
  `Docs/` tree is shipped in the archive, so the manual is also available offline next to the plugin.

## Example

Cutting the `1.5.1` stable release:

```powershell
# 1. Bump the descriptor: VersionName = "1.5.1", IsBetaVersion = false, Version = 151.
# 2. Add a matching "## 1.5.1 - 2026-08-02" section to CHANGELOG.md.
git add DreamShader.uplugin CHANGELOG.md
git commit -m "Release 1.5.1"
git push

# 3. Tag exactly what the descriptor implies: base 1.5.1, no beta 'b'.
git tag v1.5.1
git push origin v1.5.1
```

Published result:

```text
release      DreamShader 1.5.1
tag          v1.5.1
assets       DreamShader-1.5.1.zip
             <every asset from the latest dreamshader-language-support release, e.g. *.vsix>
zip layout   DreamShader/Source/  DreamShader/Resources/  DreamShader/Shaders/
             DreamShader/Docs/  DreamShader/.skill/
             DreamShader/DreamShader.uplugin
             DreamShader/README.md  DreamShader/README.zh-CN.md
             DreamShader/CHANGELOG.md  DreamShader/LICENSE
```

## See also

- [Contributing](index.md) — building the plugin, the source tree, and the engine-version matrix
- [Testing](testing.md) — the suite to run before tagging, and why the corpus is absent from the archive
- [HLSL library](../builtins/hlsl-library.md) — `DreamShaderBuiltins.ush`, shipped in the archive since `1.5.1`
- [Workspace and editor extensions](../tools/workspace.md) — the VSCode extension shipped alongside the plugin
- [DreamShader module](../api/dreamshader-module.md) — the `/Plugin/DreamShader` shader mapping registered at startup
- [DreamShader reference](../index.md) — the manual the `Docs/` folder becomes
</content>
