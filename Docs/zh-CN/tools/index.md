---
title: "工具"
description: "工具"
---

<a id="tools"></a>

# 工具

> [DreamShader](../index.md) » **工具**

DreamShader 的编辑器端界面：菜单、停靠的浏览器标签页、预览渲染器、反编译器、VirtualFunction 操作、资产重命名同步、磁盘桥接以及无头命令小程序。

| | |
| :-- | :-- |
| 实现于 | `DreamShaderEditor` 模块（类型 `Editor`，加载阶段 `Default`） |
| 命名空间 | `UE::DreamShader::Editor::Private` — 没有公开的编辑器头文件 |
| 插件依赖 | `WebSocketNetworking`（预览流送）、`SQLiteCore`（桥接数据库） |
| 禁用方式 | `-NoDreamShaderEditorBridge` 命令行开关，以及任何命令小程序运行 |

本页面的所有内容都在 `UnrealEditor.exe` 内运行。打包后的游戏中没有这些内容：该模块仅限编辑器使用，运行时 `DreamShader` 模块不包含任何 UI。

<a id="pages"></a>

## 页面

| 页面 | 涵盖内容 |
| :-- | :-- |
| [编辑器集成](editor-integration.md) | 每个菜单项、工具栏按钮、上下文菜单操作和标签页生成器，包括标签、工具提示、图标和效果；Custom Pass 管线的详情面板 *(2.1.0)* |
| [材质内容浏览器](material-browser.md) | 停靠的浏览器标签页：来源和资产并排显示，包含状态、诊断、来源追踪、依赖关系和继承关系；实时预览；实例工厂和*实例化*；`.dsp` 管线及其*新建*模板 *(2.1.0)* |
| [预览](preview.md) | 缩略图渲染器、流式 WebSocket 预览、网格集和各项限制 |
| [反编译器](decompiler.md) | 将现有材质、函数、层、混合或实例导出回源代码：`.dss` / `.dsi`（2.0，默认）或 `.dsm` / `.dsf`；Custom Pass 管线导出为 `.dsp` *(2.1.0)* |
| [迁移](migrate.md) | `dsc migrate`：将 1.x 源代码重写为 `.dss`，每个重写都会在写入之前得到验证 *(2.0)* |
| [VirtualFunction 工具](virtual-function-tools.md) | 条件式 VirtualFunction 上下文菜单和启动同步服务 |
| [资产重命名同步](asset-rename-sync.md) | 将重命名或移动过的资产追踪到引用它的 `.dsm` / `.dsf` / `.dsh` 文件中 *(自 1.9.0 起)* |
| [工作区](workspace.md) | 生成的 `DreamShader.code-workspace`、VSCode 的发现与启动、导出的清单 — 其中包括 `pass-keys.json` *(2.1.0)* |
| [包](packages.md) | `DShader/Packages`：插件实现了什么以及未实现什么 |
| [命令小程序](commandlet.md) | `-run=DreamShader` — 无头编译、反编译、迁移、检查和 IR 工具；Custom Pass 槽位注册表，`pass-registry` *(2.1.0)* |
| [桥接](bridge.md) | 请求文件、诊断接收端、`bridge.db`、WebSocket 协议；对 Custom Pass 着色器文件的监视 *(2.1.0)* |

<a id="which-side-implements-what"></a>

## 各方实现了什么

DreamShader 以两个独立产品的形式发布，它们通过 `<Project>/Saved/DreamShader/Bridge/` 中的文件和一个本地回环 WebSocket 进行通信。本手册记录的是 Unreal 插件。编辑器扩展的行为仅在插件的契约依赖它时才会被提及，并会被明确标注。

| 界面 | 实现方 | 备注 |
| :-- | :-- | :-- |
| 菜单、工具栏、上下文菜单、标签页 | Unreal 插件 | [编辑器集成](editor-integration.md) |
| 保存时自动编译、诊断存储 | Unreal 插件 | 写入三个诊断接收端；参见[桥接](bridge.md) |
| 预览渲染（PNG 帧） | Unreal 插件 | 渲染器、网格集和钳制都是插件端的 — [预览](preview.md) |
| 预览摄像机控制、俯仰角钳制、帧确认 | 编辑器扩展 | 插件自身不应用任何俯仰角钳制 |
| 反编译器 | Unreal 插件 | [反编译器](decompiler.md) |
| `.dss` / `.dsi` / `.dsm` / `.dsh` / `.dsf` 语法高亮、补全、悬停 | 编辑器扩展 | 由插件导出的清单提供 — [工作区](workspace.md) |
| `.dsp` 高亮和补全 | 编辑器扩展 | 插件为其导出 `pass-keys.json` 和符号索引 — [工作区](workspace.md#pass-keysjson) *(2.1.0)* |
| `DreamShader.code-workspace` 文件 | Unreal 插件 | 每次*打开 Dream Shader 工作区*时重写 |
| 包清单（`dreamshader.package.json`）、锁文件、安装/更新命令 | 编辑器扩展 | **没有任何插件 C++ 会读取这两个文件** — [包](packages.md) |
| `DShader/Packages` 目录创建、导入解析、自动编译排除 | Unreal 插件 | [包](packages.md) |

编辑器扩展作为 `TypeDreamMoon/dreamshader-language-support` 单独发布；其设置、命令和包存储不在本手册的范围内。

<a id="startup-order"></a>

## 启动顺序

桥接和浏览器标签页从 `StartupModule` 注册。跳过其中任何一步也会跳过其下方的所有内容。

| # | 步骤 | 跳过时机 |
| :-- | :-- | :-- |
| 1 | 如果正在运行命令小程序：当 `-run=` 包含 `Cook` 且 `-cookworker` 不存在时安装 cook 钩子，然后停止 | — |
| 1a | 注册[管线详情面板](editor-integration.md#pass-pipeline-details-panel) *(自 2.1.0 起)* | 命令小程序（步骤 1 已停止） |
| 2 | 完全退出 | 命令行中开启了 `-NoDreamShaderEditorBridge` |
| 3 | 创建并启动编辑器桥接 | 同上 |
| 4 | 注册材质内容浏览器浮动标签页及其菜单项 | 同上 |
| 5 | 将[资产重命名同步服务](asset-rename-sync.md)订阅到 `OnAssetRenamed` *(自 1.9.0 起)* | 同上，以及资产注册表不可用时 |

随后桥接自身的启动会重置 `bridge.db`、导出清单（`pass-keys.json` 及 `settings.json` *(自 2.1.0 起)*）、运行[VirtualFunction 同步服务](virtual-function-tools.md#startup-sync-service)、将完整扫描排队、在端口 `17864` 上打开[预览 WebSocket 服务器](preview.md#streaming-preview)、注册源目录监视器，并安装菜单。

<a id="notes"></a>

## 说明

- 菜单注册是幂等的，并在编辑器关闭期间直接退出。桥接菜单由 ToolMenu 所有者 `DreamShaderEditor` 拥有；浏览器标签页自身的条目使用 `DreamShaderMaterialBrowser`。
- 所有编辑器日志行都写入 `LogDreamShader` 类别。
- 桥接弹出的通知在 4 秒后过期；材质内容浏览器自身操作（编译、实例化）弹出的通知在 3.5 秒后过期。
- 编辑器会保持一个 ThinCustom 材质为临时状态 — 没有 `.uasset` — 直到它被实例化；每次成功编译都会保存一个 `Graph` 材质和一个材质函数 *(自 2.0.0 起)*。参见[临时材质](../generation/ephemeral.md)。

<a id="see-also"></a>

## 另请参阅

- [编辑器集成](editor-integration.md) — 完整的菜单和命令界面
- [项目设置](../settings/project.md) — 这些工具读取的设置
- [生成](../generation/index.md) — 一次编译实际做了什么
- [诊断索引](../diagnostics/index.md) — 按阶段列出的所有消息
- [入门](../getting-started.md) — 首次运行演练
