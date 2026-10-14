---
title: "DreamShader 文档"
description: "DreamShader 文档"
---

<a id="dreamshader-documentation"></a>

# DreamShader 文档

[English](README.md) · [简体中文](README.md)

本目录是 DreamShader 手册的英文原文。教程和参考页按主题组织在一起，可以在 GitHub 上阅读，也可以在本地 Markdown 阅读器中阅读。[文档站点](https://shader.toolchain.64hz.cn/en/docs)发布这些英文原文以及本插件维护的、经过审阅的中文译文。

<a id="start-here"></a>

## 从这里开始

1. [关于 DreamShader](getting-started/about.md) —— 插件、其模块及支持的引擎。
2. [安装](getting-started/installation.md) —— 安装、启用并检查插件。
3. [第一个材质](getting-started/first-material.md) —— 编写并找到生成的材质。
4. [项目结构](getting-started/project-layout.md) —— 源根目录、文件夹和文件。
5. [日常工作流](getting-started/workflow.md) —— 保存、编译、重新生成和诊断。

简明的[快速上手参考](../getting-started.md)仍可用于快速完成设置。

<a id="topics"></a>

## 主题

| 主题 | 指南与参考 |
| --- | --- |
| 语言 | [DreamShaderLang 2.0](language-v2/index.md)；[1.x 参考](../language/index.md)、[块](language/blocks.md)、[Section](language/sections.md)、[函数](language/functions.md) |
| 图表 | [语句、表达式与调用](graph/index.md) |
| 节点与参数 | [内置](../builtins/index.md)、[参数](../parameters/index.md)、[设置](../settings/index.md) |
| 生成 | [管线与资产生命周期](generation/index.md)、[输出绑定](generation/outputs.md)、[版本控制](generation/source-control.md) |
| 示例 | [参考示例](../examples/index.md)、[常用写法](examples/patterns.md)、[完整示例](examples/recipes.md)、[Custom Pass 示例](examples/custom-pass.md) |
| 工具 | [编辑器工具](../tools/index.md)、[VSCode 与 Rider](tools/vscode.md)、[命令行](tools/commandlet.md) |
| Custom Pass | [运行时](runtime/index.md)、[HLSL pass](../runtime/hlsl.md)、[管线语言](language-v2/passes.md) |
| 诊断 | [错误与警告速查](diagnostics/index.md)、[限制](diagnostics/limitations.md) |
| 开发 | [C++ API](api/index.md)、[贡献](../contributing/index.md)、[测试](../contributing/testing.md)、[发布](../contributing/release.md) |
| 发布历史 | [带说明的版本记录](changelog/dreamshader.md)、[仓库变更日志](../../CHANGELOG.md) |

[完整参考索引](index.md)列出了各技术参考页。

<a id="maintaining-the-manual"></a>

## 维护手册

编辑相应主题目录中的英文 Markdown。保持示例、标识符和相对链接在插件仓库中可用；网站展示属于文档站点。

站点的 `docs-sync/config.json` 将源路径映射到已发布的页面 ID。移动源文件需要更新该映射和指向它的相对链接，同时保留其页面 ID 以保持现有 URL 稳定。英文页面由这些源文件生成；AI 生成的中文更新需经过拉取请求审阅后才能发布。

发布还会更新[发布说明页面](changelog/dreamshader.md)：它是为手册读者编写的——说明对他们而言有哪些变化，并链接到相关页面——而不是 `CHANGELOG.md` 的副本，因此两个文件都不由另一个生成。当 `CHANGELOG.md` 增加条目时，更新其 *Current release* 行并添加该版本的章节；提及当前版本的页面（如[限制](diagnostics/limitations.md)）也会同时审阅。

`zh-CN/` 目录树镜像英文源路径。导入的译文未经审阅；[首次迁移快照](TRANSLATIONS.md)保留了最初导入和缺失的翻译清单。要查看当前状态，请运行 `python -B Docs/translation-status.py` 或查看网站仓库中的翻译状态 Issue。`.translations/` 记录源版本，但不将导入视为批准。仅限网站的说明保留在网站仓库中。

在本地使用 `python -B Docs/translation-status.py` 检查翻译状态；添加 `--all` 以包含所有源文件，或添加 `--json` 以输出结构化结果。这个只读工具不需要网站仓库或 Git 检出。
