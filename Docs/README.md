# DreamShader documentation

[English](README.md) · [简体中文](zh-CN/README.md)

This directory is the English source for the DreamShader manual. Tutorials and reference pages live
together by topic and can be read here on GitHub or in a local Markdown reader. The
[documentation site](https://shader.toolchain.64hz.cn/en/docs) publishes these English sources and the reviewed Chinese translations maintained in this plugin.

## Start here

1. [About DreamShader](getting-started/about.md) — the plugin, its modules and supported engines.
2. [Installation](getting-started/installation.md) — install, enable and check the plugin.
3. [Your first material](getting-started/first-material.md) — author and find a generated material.
4. [Project layout](getting-started/project-layout.md) — source roots, folders and files.
5. [Daily workflow](getting-started/workflow.md) — save, compile, regenerate and diagnose.

The compact [getting-started reference](getting-started.md) remains available for a quick setup.

## Topics

| Topic | Guides and reference |
| --- | --- |
| Language | [DreamShaderLang 2.0](language-v2/index.md); [1.x reference](language/index.md), [blocks](language/blocks.md), [sections](language/sections.md), [functions](language/functions.md) |
| Graphs | [Statements, expressions and calls](graph/index.md) |
| Nodes and parameters | [Builtins](builtins/index.md), [parameters](parameters/index.md), [settings](settings/index.md) |
| Generation | [Pipeline and asset lifecycle](generation/index.md), [output bindings](generation/outputs.md), [source control](generation/source-control.md) |
| Examples | [Reference examples](examples/index.md), [patterns](examples/patterns.md), [recipes](examples/recipes.md), [Custom Pass examples](examples/custom-pass.md) |
| Tools | [Editor tools](tools/index.md), [VSCode and Rider](tools/vscode.md), [commandlet](tools/commandlet.md) |
| Custom Pass | [Runtime](runtime/index.md), [HLSL passes](runtime/hlsl.md), [pipeline language](language-v2/passes.md) |
| Diagnostics | [Error and warning lookup](diagnostics/index.md), [limitations](diagnostics/limitations.md) |
| Development | [C++ API](api/index.md), [contributing](contributing/index.md), [testing](contributing/testing.md), [release](contributing/release.md) |
| Release history | [Version notes with explanations](changelog/dreamshader.md), [repository changelog](../CHANGELOG.md) |

The [complete reference index](index.md) lists the individual technical reference pages.

## Maintaining the manual

Edit the English Markdown in the relevant topic directory. Keep examples, identifiers and relative
links usable in the plugin repository; website presentation belongs to the documentation site.

The site's `docs-sync/config.json` maps source paths to published page IDs. Moving a source file
requires updating that mapping and incoming relative links, while retaining its page ID to keep
existing URLs stable. English pages are generated from these sources; AI-generated Chinese updates
are reviewed in a pull request before publication.

The `zh-CN/` tree mirrors English source paths. Imported translations are unreviewed; its README lists missing translations, and `.translations/` records source versions without treating an import as approval. Website-only notes remain in the website repository.

Check translation status locally with `python -B Docs/translation-status.py`; add `--all` to include every source or `--json` for structured output. This read-only tool does not need the website repository or a Git checkout.
