<a id="ai-agents"></a>

# AI 智能体

> [DreamShader](../index.md) » [Tools](index.md) » **AI 智能体**

编码智能体如何在你的项目中编写 DreamShaderLang 并检查自己的工作：智能体指南、六个技能、如何将它们发布到项目中、可选的 MCP 服务器，以及行之有效的循环。

| | |
| :-- | :-- |
| 智能体指南 | 仓库根目录下的 [`AGENTS.md`](https://github.com/TypeDreamMoon/DreamShader/blob/main/AGENTS.md)；`CLAUDE.md` 指向它 |
| 技能 | `Plugins/DreamShader/.skill/<skill>/SKILL.md` — 纯 Markdown |
| 驱动脚本 | `Plugins/DreamShader/.skill/dsc.ps1` — 无头 [commandlet](commandlet.md)，并给出明确的判定结果 |
| Shell | PowerShell 7（`pwsh`） |

DreamShaderLang 不在任何模型的训练数据中。凭记忆编写它的智能体只会套用 HLSL 或 GLSL 的习惯，而这些猜测都会失败。让智能体在这里发挥作用的是循环：阅读参考、编写、无头编译、阅读诊断信息、修复。

<a id="the-agent-guide"></a>

## 智能体指南

`AGENTS.md` 是任何智能体的入口，无论是否使用技能。它分为两部分：在游戏项目中编写源文件，以及修改插件本身。当智能体在插件仓库本身中工作时，它会按文件名找到该文件；Claude Code 则通过仓库的 `CLAUDE.md` 读取它。

<a id="giving-the-guide-to-the-agent-in-your-game-project"></a>

## 在游戏项目中把指南交给智能体

智能体会读取其工作文件夹根目录下的指令文件。在游戏项目中，该根目录是项目根目录，而不是 `Plugins/DreamShader/`，所以它不会自行读取 `Plugins/DreamShader/AGENTS.md`。

将这一行添加到项目的指令文件（`AGENTS.md`，或你的工具所读取的任何文件）中：

```text
Before writing or fixing any DreamShader source, read Plugins/DreamShader/AGENTS.md and follow its Authoring section.
```

在 Claude Code 中，你也可以改为导入该指南。将这一行添加到项目的 `CLAUDE.md` 中：

```text
@Plugins/DreamShader/AGENTS.md
```

<a id="skills"></a>

## 技能

| 技能 | 适用场景 |
| :-- | :-- |
| `dream-shader-create` | 根据描述编写新的材质、函数、实例或 Custom Pass 管线 |
| `dream-shader-verify` | 检查或编译单个源文件或整个 `DShader/` 目录树 |
| `dream-shader-diagnose` | 构建因 `DSHnnnn` 代码而失败，或结果看起来不正确 |
| `dream-shader-decompile` | 将现有的材质、函数、实例或管线资产转换为源码 |
| `dream-shader-optimize` | 让反编译或迁移后的源码读起来像手写源码 |
| `dream-shader-migrate` | 将 1.x 的 `.dsm` / `.dsf` / `.dsh` 重写为 2.0 的 `.dss` |

每个技能都会运行 `dsc.ps1` 并链接到 `.skill/reference/` — `dss.md` 用于 `.dss` 和 `.dsi`，`dsp.md` 用于 `.dsp`，`legacy.md` 用于 1.x 源文件。驱动脚本的完整动词列表见 [`.skill/README.md`](https://github.com/TypeDreamMoon/DreamShader/blob/main/.skill/README.md)。

<a id="publishing-the-skills-into-a-project"></a>

## 将技能发布到项目中

Claude Code 会从智能体工作文件夹或其上层文件夹的 `.claude/skills/` 加载技能。插件将它们保存在 `.skill/` 中，因此请从项目根目录发布一次：

```powershell
pwsh -File Plugins/DreamShader/.skill/sync-skills.ps1
```

该脚本会将每个技能复制到 `<project>/.claude/skills/`，并为其新位置重写链接。

| 参数 | 作用 |
| :-- | :-- |
| *（无）* | 发布到插件上层最近的 `.uproject` |
| `-Target <dir>` | 发布到另一个 `.claude/skills` 目录 |
| `-Check` | 进行比较且不写入任何内容；当已发布的副本过期时以 `1` 退出 |
| `-Prune` | 同时删除源文件已不存在的已发布文件 |

> [!IMPORTANT]
> **已发布的副本不会自行更新。** 更新插件后，请运行 `sync-skills.ps1 -Check`，并在其以 `1` 退出时再次运行 `sync-skills.ps1`。读取过期技能的智能体会写出过期的语法。

Claude Code 以外的智能体不会自行加载技能。请将它们指向所需的 `SKILL.md`，或指向 `AGENTS.md`；命令是相同的。

<a id="the-mcp-server-optional"></a>

## MCP 服务器（可选）

[dream-mcp](https://github.com/TypeDreamMoon/dream-mcp) 是 Dream 插件的 MCP 服务器。连接到它后，智能体会得到一个 `dream_shader` 工具，其操作正是插件自身的 [DreamMCP 端点](dreammcp.md)：

| 操作 | 作用 |
| :-- | :-- |
| `check` | 检查一个源或所有源，不写入任何内容 |
| `compile` | 构建某个源的资产；使用 `shaders: true` 时还会编译其着色器 |
| `diagnostics` | 上次检查或编译发现的内容，不进行构建 |
| `lookup` | `UE.*` 或 `Substrate.*` 节点的输入、输出和属性 |
| `preview` | 将材质渲染为 PNG 并说明其显示的内容；使用 `frames` 时还会说明其随时间的变化 |
| `decompile` | 将现有的材质、函数或实例写出为源 |

调用在哪里运行：

- **编辑器已打开该项目。** 由编辑器提供服务，与保存源时相同：Content Browser 和 VSCode 扩展都能看到结果，材质会留在内存中，直到用户保存它。
- **没有打开编辑器。** 服务器会在后台为该项目启动一个无窗口的引擎，并在后续调用中重复使用它，因此只有第一次调用需要等待引擎启动。它会在空闲时、编辑器打开时或服务器退出时停止。

每个回答都具有相同的结构：一个结果、一行摘要、带有代码、行号和指向其页面链接的诊断信息、写入的资产，以及下一步该做什么。智能体从不读取日志。

要安装服务器并将其注册到你的智能体，请按照 [dream-mcp README](https://github.com/TypeDreamMoon/dream-mcp#readme) 操作。插件端随 DreamShader 一起提供：Project Settings › DreamShader 下的 *Enable DreamMCP Endpoint* 可开启或关闭该端点（[project settings](../settings/project.md)），而 `-NoDreamMCP` 可在单次运行中关闭它。

不使用服务器时，技能和 `dsc.ps1` 按本页所述方式工作。发现 `dream_shader` 工具的技能会用该工具完成相同的步骤，而不是使用 driver。

<a id="the-loop"></a>

## 这个循环

每次运行 `dsc.ps1` 都会启动引擎，耗时数十秒，而检查多个文件与检查一个文件的耗时大致相同。因此先编写，再用一次运行完成检查。

1. 阅读对应文件类型的参考文档：`.skill/reference/dss.md`、`dsp.md` 或 `legacy.md`。
2. 在 `DShader/` 下编写源文件。沿用项目现有的文件夹和命名结构。
3. 检查（不写入任何内容）：

   ```powershell
   pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check -All
   ```

4. 在 [诊断](../diagnostics/index.md) 中查找每个 `DSHnnnn` 代码，修复后再次检查。
5. 每个新文件编译一次，并让这次运行删除它创建的内容：

   ```powershell
   pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/UI/M_Panel.dss -Force -CleanNew
   ```

6. 对于 `/// @custom` 主体或带 HLSL pass 的 `.dsp`，还要运行 `check <file> -Shaders`。这是唯一会编译该 HLSL 的步骤，而且它会保存资产。
7. 在编辑器中打开材质查看。无头运行无法判断它的外观。

> [!WARNING]
> **绝不要让 agent 在你的项目中运行 `compile -All`。** 它会把每个插件的源码重新构建到各自的 `Content/` 文件夹中，并可能覆盖你并不想改动的资产。`check -All` 是安全的。

<a id="when-the-agent-gets-it-wrong"></a>

## 当 agent 出错时

使用 **AI wrote it wrong / AI 写错了** 表单提交 issue。请提供工具和模型、提示词，以及它编写的源码。这些报告会改进 `AGENTS.md`、技能和示例。对于插件本身的 bug，请先运行 `pwsh -File Plugins/DreamShader/.skill/dsc.ps1 info` 并将其输出粘贴到 bug 表单中。参见 [CONTRIBUTING.md](https://github.com/TypeDreamMoon/DreamShader/blob/main/CONTRIBUTING.md)。

<a id="see-also"></a>

## 另见

- [Commandlet](commandlet.md) — `dsc.ps1` 背后的每个 `-run=DreamShader` 动词和标志
- [诊断索引](../diagnostics/index.md) — 每个代码
- [DreamShaderLang 2.0](../language-v2/index.md) — 语言参考
- [Contributing](../contributing/index.md) — 修改插件本身
