---
title: "DreamShader 文档"
description: "DreamShader 文档"
---

<a id="dreamshader-documentation"></a>

# DreamShader 文档

[English](../README.md) · 简体中文

本目录是 DreamShader 手册的英文原文。教程与参考页按主题放在一起，可以在 GitHub 上阅读，也可以用本地 Markdown 阅读器打开。[文档站](https://shader.toolchain.64hz.cn/en/docs)发布这些英文原文，以及本插件中维护的、经过审阅的中文译文。

<a id="start-here"></a>

## 从这里开始

1. [关于 DreamShader](getting-started/about.md) —— 插件、其模块及支持的引擎。
2. [安装](getting-started/installation.md) —— 安装、启用并检查插件。
3. [第一个材质](getting-started/first-material.md) —— 编写并找到生成的材质。
4. [项目结构](getting-started/project-layout.md) —— 源根目录、文件夹和文件。
5. [日常工作流](getting-started/workflow.md) —— 保存、编译、重新生成和诊断。

简洁的[入门参考](getting-started.md)仍可用于快速设置。

<a id="topics"></a>

## 主题

| 主题 | 指南与参考 |
| --- | --- |
| 语言 | [DreamShaderLang 2.0](language-v2/index.md)；[1.x 参考](language/index.md)、[块](language/blocks.md)、[Section](language/sections.md)、[函数](language/functions.md) |
| 图 | [语句、表达式与调用](graph/index.md) |
| 节点与参数 | [内置](builtins/index.md)、[参数](../parameters/index.md)、[设置](../settings/index.md) |
| 生成 | [管线与资产生命周期](generation/index.md)、[输出绑定](generation/outputs.md)、[版本控制](generation/source-control.md) |
| 示例 | [参考示例](examples/index.md)、[常用写法](examples/patterns.md)、[完整示例](examples/recipes.md)、[Custom Pass 示例](examples/custom-pass.md) |
| 工具 | [编辑器工具](../tools/index.md)、[VSCode 与 Rider](tools/vscode.md)、[命令行](tools/commandlet.md) |
| Custom Pass | [运行时](runtime/index.md)、[HLSL pass](../runtime/hlsl.md)、[管线语言](language-v2/passes.md) |
| 诊断 | [错误与警告速查](diagnostics/index.md)、[当前限制](diagnostics/limitations.md) |
| 开发 | [C++ API](api/index.md)、[参与贡献](contributing/index.md)、[测试](contributing/testing.md)、[发布](contributing/release.md) |
| 发布历史 | [带说明的版本记录](changelog/dreamshader.md)、[仓库更新日志](../../CHANGELOG.md) |

[完整参考索引](index.md)列出了各篇技术参考页。

<a id="maintaining-the-manual"></a>

## 维护手册

编辑相应主题目录中的英文 Markdown。保持示例、标识符和相对链接在插件仓库中可用；网站的呈现方式属于文档站。

文档站的 `docs-sync/config.json` 将源路径映射到已发布页面 ID。移动源文件需要更新该映射以及指向它的相对链接，同时保留其页面 ID 以保持现有 URL 稳定。英文页面由这些源文件生成；AI 生成的中文更新在发布前会经过 Pull Request 审阅。

发布时还会更新[版本说明页面](changelog/dreamshader.md)：它面向手册读者撰写——说明对他们而言有哪些变化，并链接到相关页面——并非 `CHANGELOG.md` 的副本，因此两者都不是由对方生成的。当 `CHANGELOG.md` 获得新版本时，更新其*当前版本*行并添加该版本的章节；提及当前版本的页面，例如[限制](diagnostics/limitations.md)，也在同一时间审阅。

`zh-CN/` 目录树与英文源路径保持一致。导入的译文未经审阅；[首次迁移快照](TRANSLATIONS.md)保留了原始的已导入和缺译清单。要了解当前状态，请运行 `python -B Docs/translation-status.py`，或查看网站仓库中的翻译状态 Issue。`.translations/` 记录源文件版本，但不将导入视为批准。仅网站使用的说明保留在网站仓库中。

在本地用 `python -B Docs/translation-status.py` 检查翻译状态；添加 `--all` 可包含全部源文件，或添加 `--json` 输出结构化结果。此只读工具无需网站仓库或 Git 检出。
