# Contributing to DreamShader

[English](CONTRIBUTING.md) · [简体中文](CONTRIBUTING.zh-CN.md)

Thank you for helping. This page says how to report a problem, how to propose a change, and what a pull
request needs before it can be merged.

## Ways to contribute

- **Bug reports** — wrong output, a crash, or behavior that differs from the docs.
- **Feature and syntax proposals** — something DreamShaderLang cannot express yet.
- **Docs and translation fixes** — a page that is wrong, unclear, missing or badly translated.
- **Small fixes** — a typo, a wrong message, a one-file bug fix. Open the pull request directly.
- **Feature pull requests** — open a proposal issue first, and wait until the design is agreed. This saves you
  from writing code that cannot be merged.

## Reporting a bug

1. Search the [existing issues](https://github.com/TypeDreamMoon/DreamShader/issues) first.
2. Run this in your project root and copy the output. It starts no engine and prints no paths:

   ```powershell
   pwsh -File Plugins/DreamShader/.skill/dsc.ps1 info
   ```

3. Open a [new issue](https://github.com/TypeDreamMoon/DreamShader/issues/new/choose) and pick a form.
   Paste the output of step 2 into **Environment**.
4. Give the smallest `.dss` / `.dsi` / `.dsp` / `.dsm` / `.dsf` source that shows the problem. It should build on
   its own, without the rest of your project.

If an AI tool wrote source that does not build or uses names that do not exist, use the **AI wrote it wrong**
form. Those reports improve the agent guide and the skills.

## Questions and ideas

Ask usage questions and share ideas in
[GitHub Discussions](https://github.com/TypeDreamMoon/DreamShader/discussions). Issues are for bugs and
agreed work.

## Development setup

- Unreal Engine 5.3–5.8 on Win64. Development targets 5.8; Custom Pass (`.dsp`) needs 5.8.
- PowerShell 7 (`pwsh`) for every script in the repository.
- Work in a test host, a small project that holds a git worktree of this repository:

  ```powershell
  pwsh -NoProfile -File Tools/TestHost/New-DreamShaderTestHost.ps1 -Root <host dir> -Branch <branch>
  ```

- The test runner below builds the host's editor target before it runs.

Details: [Tools/TestHost/README.md](Tools/TestHost/README.md) and the
[contributing manual](Docs/contributing/index.md) (modules, build rules, the engine matrix).

## Before you open a pull request

Run the whole suite from the host's `Plugins/DreamShader/`. Close any editor open on the host first:

```powershell
pwsh -NoProfile -File Tools/Tests/Invoke-DreamShaderTests.ps1 -Preset Suite
```

The exit code is the number of unexpected failures; `0` is a pass. If you changed `.skill/dsc.ps1`, also run
`Tools/Tests/Test-DscAssetCleanup.ps1` and `Tools/Tests/Test-DscInfo.ps1` (seconds, no engine).

Update these in the same pull request when your change touches them:

- **Docs** — the English page under `Docs/` that describes the behavior.
- **CHANGELOG** — an entry under `## Unreleased` in `CHANGELOG.md` for a user-visible change.
- **Diagnostics** — after adding or removing a `DSHnnnn` code, run `pwsh -File .skill/gen-diagnostics.ps1`.
- **Localization** — after changing user-facing C++ text, run `pwsh -File Tools/Localization/localization_lint.ps1`.
- **Engine versions** — a version gate must build on each engine it affects: `.skill/build-plugin.ps1`.

More on tests: [Docs/contributing/testing.md](Docs/contributing/testing.md).

## Commits and pull requests

- Keep each commit small and about one topic.
- Write the message like the history: `fix(scope): what changed`, `docs: …`, `feat(tools): …`.
- Name the branch `<type>/<topic>`, for example `fix/static-switch-bool-branches` or `docs/unify-labels`.
- Fill in the pull request template: what, why, the test command and its result line.
- No absolute local paths, user names or e-mail addresses in committed files.

## AI-assisted contributions

AI-assisted pull requests are welcome, under these rules:

- Tell the agent to read [`AGENTS.md`](AGENTS.md) first.
- Say in the pull request which tool and model you used.
- Paste the result line of the test command. A pull request without test evidence is not reviewed.
- You are responsible for every line you submit. Read and test it as if you wrote it.
- Bulk AI-generated pull requests without test evidence will be closed.

## Docs and translations

English is the source. Edit the English page under `Docs/`.

`Docs/zh-CN/` mirrors the English pages. After a change reaches `main`, CI translates the changed paragraphs
with [dreamdocs](https://github.com/TypeDreamMoon/dreamdocs) and opens a pull request for review. You do not need
to translate your own English change. To correct a Chinese translation, edit the Chinese page directly; the next
run keeps your edit. `README.zh-CN.md` and `CONTRIBUTING.zh-CN.md` are kept by hand.

The published manual is <https://shader.toolchain.64hz.cn>.

## License

DreamShader is released under the [MIT license](LICENSE). By contributing, you agree that your contribution is
licensed under the same license. There is no contributor license agreement (CLA).

## Code of conduct and security

- [Code of conduct](https://github.com/TypeDreamMoon/.github/blob/main/CODE_OF_CONDUCT.md)
- [Security policy](https://github.com/TypeDreamMoon/.github/blob/main/SECURITY.md) — report a vulnerability
  privately, not in an issue.
