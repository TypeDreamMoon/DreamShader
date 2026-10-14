---
title: "测试"
description: "测试"
---

<a id="testing"></a>

# 测试

> [DreamShader](../index.md) » [贡献](index.md) » **测试**

插件的 Unreal 自动化测试套件，以及其中十三个测试在运行时枚举的磁盘夹具语料库。

| | |
| :-- | :-- |
| 声明于 | `Source/DreamShaderEditor/Private/Tests/` — 37 个翻译单元，全部位于 `#if WITH_DEV_AUTOMATION_TESTS` 中 |
| 类型 | Unreal 自动化测试 |
| 标志 | 每个声明均带有 `EAutomationTestFlags::EditorContext \| EAutomationTestFlags::EngineFilter`；Custom Pass 渲染、生命周期和插槽注册表测试额外添加 `NonNullRHI` |
| 语料库根目录 | `<Plugin>/Tests/Corpus` |
| 编辑器界面 | *Tools ▸ Session Frontend ▸ Automation*，按测试名称前缀过滤 |

<a id="synopsis"></a>

## 概要

```powershell
& "<EngineDir>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" --% "<ProjectDir>\<Project>.uproject" -ExecCmds="Automation RunTests <filter>; Quit" -nullrhi -unattended -nopause -nosplash -log
```

`<filter>` 是测试名称的任意前缀；`DreamShader` 运行所有内容。`--%` 是 PowerShell 的停止解析标记：其后的所有内容都会原样传递给可执行文件，因此 `-ExecCmds=` 周围的引号会保留。去掉 `--%` 的同一行就是粘贴到 `cmd.exe` 中的内容。

可选开关：`-nullrhi`、`-DreamShaderUpdateGolden`、`-NoDreamShaderEditorBridge`。参见[命令行开关](#command-line-switches)。

<a id="test-counts"></a>

## 测试数量

| 数量项 | 值 |
| :-- | :-- |
| 测试声明（`IMPLEMENT_*_AUTOMATION_TEST`） | 277 |
| 简单声明（每个一个测试） | 264 |
| 复杂声明（数据驱动运行器） | 13 — 参见[语料库运行器](#corpus-runners--13-declarations) |
| 语料库夹具 | `Tests/Corpus/` 下的 513 个源文件 |
| `.expected.json` 基准文件 | 508 |
| 可单独运行的测试 | **945**，在一次 `DreamShader` 运行中（2026-09-21） |

复杂运行器在运行时枚举语料库树，因此可运行测试数量会随夹具数量变化，且**添加夹具时无需更改任何 C++ 代码**。`DreamShader.Lang2.RoundtripIR` 会扫描属于其他层的夹具，这就是可运行数量并非“简单测试 + 夹具”的原因。

<a id="test-groups"></a>

## 测试分组

| 前缀 | 层 | 要求 |
| :-- | :-- | :-- |
| `DreamShader.Lang.Parse.*` | 数据驱动解析语料库 | 快速；无资源 I/O |
| `DreamShader.Lang.Diagnostics.*` | 纯诊断辅助函数 | 毫秒级；无编辑器状态 |
| `DreamShader.Lang.Import.*` | 纯导入说明符辅助函数 | 毫秒级；无编辑器状态 |
| `DreamShader.Lang.ParameterExpressions.*` | 1.x 的 `Properties` 接口，通过旧版前端 | 快速；仅解析器 |
| `DreamShader.Commandlet.Args.*` | 命令行程序参数解析 | 毫秒级；纯函数 |
| `DreamShader.Commandlet.Compile.*` | 命令行程序运行器冒烟测试 | 慢速；编辑器；写入真实的 `/Game` 资源 |
| `DreamShader.Compiler.Parser.*` | 1.x 源文件通过编译器端到端编译 | 慢速；编辑器 |
| `DreamShader.Compiler.Generate.*` | 1.x 源文件通过编译器端到端编译 | 慢速；编辑器 |
| `DreamShader.Compiler.SourceHash.*` | 重新生成跳过检查 | 慢速；编辑器 |
| `DreamShader.Lang2.{Lexer,Expressions,Statements,Declarations,Printer,Trivia}.*` | 2.0 前端，逐单元测试 | 毫秒级；仅 `Core` |
| `DreamShader.Lang2.{Binder,IR,IRCompare,InstanceSource,Migrate,LegacyRules}.*` | 绑定器、IR、比较器、`.dsi` 文本、迁移器 | 快速；手工构建的目录，无资源 |
| `DreamShader.Lang2.{IRLayout,SubstrateSugar,Format}.*` | 三种 IR 图布局、Substrate 语法糖（每种写法与其代表的节点对照，并通过反编译器读回）、`dsc fmt` 的核心 | 快速；手工构建的目录，无资源 |
| `DreamShader.Lang2.Corpus*.*` | 数据驱动文本层：`Lang/`、`IR/`、`Legacy/Parse`、`Legacy/IR`、`Decompile/`、`Migrate/` | 快速；无资源 I/O |
| `DreamShader.Lang2.RoundtripIR.*` | 每个可降级的夹具：IR → 文本 → IR 等价 | 快速；无资源 I/O |
| `DreamShader.Compiler2.{Smoke,Instance,Decompile,Migrate,Provenance,Parity}.*` | 2.0 管线、实例、反编译服务、`dsc migrate`、Adopt、与 1.x 捕获的图一致性 | 慢速；编辑器 |
| `DreamShader.Compiler2.Corpus*.*` | 数据驱动编译层：`Compile/`、`Legacy/Compile`、`Instance/` | 慢速；编辑器 |
| `DreamShader.Compiler2.Roundtrip.*` | `.dss` → 资源 → 反编译 → 资源，两个转储相等 | 慢速；编辑器 |
| `DreamShader.Compiler.{Divergence,Atomic,Tweaked,Persistence,BuildKey,Order}.*` | 手工编辑检测、原子重建、Ephemeral/Materialized、构建键 | 慢速；编辑器 |
| `DreamShader.Browser.*` · `DreamShader.AssetRenameSync.*` · `DreamShader.Preview.*` | 材质内容浏览器模型、资源重命名重写、预览探测（`Preview.ProbePreview.RendersBinding` 需要真实 RHI） | 混合 |
| `DreamShader.Gen.Graph.*` | 对生成的图进行节点形状断言 | 慢速；编辑器 |
| `DreamShader.Gen.Parameters.*` | 参数节点创建和引脚连线 | 慢速；编辑器 |
| `DreamShader.Gen.Wiring.*` | 生成图中的条件连线 | 慢速；编辑器 |
| `DreamShader.Gen.Layout.*` | 所放置图的几何结构 | 慢速；编辑器 |
| `DreamShader.DumpGraph.*` *（自 1.9.0 起）* | 规范的图转储 — 参见[图基线](#graph-baseline-since-190) | 慢速；编辑器 |
| `DreamShader.Roundtrip.*` | 反编译 → 重新生成保真度 | 慢速；编辑器；其中两个还需要真实 RHI |
| `DreamShader.Render.*` | 像素一致性 | 需要真实 RHI |
| `DreamShader.Lang2.Pipeline.*` | `.dsp`：解析、打印、`fmt`、绑定、每条规则的代码、比较、Adopt 的重写、`.dss` 中的传递节点 | 快速；仅 `Core`，手工构建的引擎事实 |
| `DreamShader.Lang2.Corpus.Pipeline.*` · `DreamShader.Lang2.CorpusIR.Pipeline.*` | `Tests/Corpus/Lang/Pipeline` 和 `Tests/Corpus/IR/Pipeline` 的 `.dsp` 夹具 | 快速；无资源 I/O |
| `DreamShader.Compiler2.Pipeline.*` | `.dsp` 到 `UDreamPassPipeline`：字段、渲染目标、依赖关系、往返；插槽注册表及其预检查 | 慢速；编辑器；`Registry` 和 `RegistryPrecheck` 需要真实 RHI，并首先备份 `<DShader>/.dreampass` |
| `DreamShader.Pass.Logic.*` | 无渲染的 Custom Pass 运行时：写法、参数混合、插槽打包、资源的检查、哪些管线适用于某个视图、源文件 | 快速 |
| `DreamShader.Pass.Render.*` · `DreamShader.Pass.Lifecycle.*` | Custom Pass 通道渲染到场景捕获并逐像素读回；帧中途的编辑、移除和世界销毁 | `NonNullRHI`：在 `-nullrhi` 下跳过，在下面的 RHI 门中运行 |

源码中记录的划分：快速的 `DreamShader.Lang.*` 和 `DreamShader.Lang2.*` 层作为拉取请求的门禁，慢速的 `DreamShader.Compiler*` 和 `DreamShader.Gen.*` 层每晚运行。整个套件无头运行大约需要一分半钟。

> [!WARNING]
> **请先关闭编辑器。** 一次运行会在 `/Game/DreamShaderTests` 下编译源文件并写入资源；如果同一项目上打开着编辑器，两者会争夺桥接的写入所有权和包文件。

<a id="running-headlessly"></a>

## 无头运行

命令行程序驱动程序的按所有权清理可以在没有 Unreal 的情况下测试：

```powershell
pwsh -NoProfile -File Tools/Tests/Test-DscAssetCleanup.ps1
```

这会在一个临时的假项目中练习驱动程序的真实辅助函数，包括并发无关写入、预先存在的资源、保存后编辑、Content 边界、无效和已消耗的保存清单，以及无法列出的 Content 文件夹。

| 目标 | 过滤器 |
| :-- | :-- |
| 快速门禁 — 1.x 解析等价语料库、纯辅助函数、预处理器 | `DreamShader.Lang.` |
| 快速门禁 — 2.0 前端、绑定器、IR、反编译和迁移文本层 | `DreamShader.Lang2` |
| 仅命令行程序参数辅助函数 | `DreamShader.Commandlet.Args` |
| 1.x 源文件端到端编译（原 Generate 语料库） | `DreamShader.Compiler2.CorpusLegacy` |
| 2.0 管线端到端 | `DreamShader.Compiler2` |
| 1.x 时代的端到端测试 | `DreamShader.Compiler.` |
| 反编译往返 | `DreamShader.Roundtrip` |
| 图转储确定性 | `DreamShader.DumpGraph` |
| Custom Pass，无渲染器 | `DreamShader.Lang2.Pipeline+DreamShader.Compiler2.Pipeline+DreamShader.Pass.Logic` |
| Custom Pass，已渲染（UE 5.8，真实 RHI — 见下文） | `DreamShader.Pass.Render+DreamShader.Pass.Lifecycle+DreamShader.Compiler2.Pipeline.Registry` |
| 所有内容 | `DreamShader` |

Custom Pass 渲染测试需要渲染设备但不需要窗口：将它们作为独立门禁运行，不使用 `-nullrhi`，并使用 `-RenderOffscreen -d3d12`：

```powershell
& "<EngineDir>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" --% "<ProjectDir>\<Project>.uproject" -ExecCmds="Automation RunTests DreamShader.Pass.Render+DreamShader.Pass.Lifecycle; Quit" -RenderOffscreen -d3d12 -unattended -nopause -nosplash -NoDreamShaderEditorBridge -log
```

`Tools/Tests/Invoke-DreamShaderTests.ps1` 在独立宿主项目上运行两个门控
(`Tools/TestHost`)：`-nullrhi` 下的 `Suite` 预设，以及离屏的 `Rhi` 预设。

<a id="both-sides-of-the-custom-pass-gate"></a>

### Custom Pass 门控的两侧

在 UE 5.8 之前，[`DREAMSHADER_WITH_CUSTOM_PASS`](../api/version-compat.md#dreamshader_with_custom_pass)
为 `0`，插件在构建时不包含 Custom Pass 的渲染器部分。当向 UBT 传入 `-ProjectDefine:DREAMSHADER_FORCE_NO_CUSTOM_PASS` 时，5.8 的机器也会构建该部分，
测试脚本的 `-WithoutCustomPass` 会传入该参数：

```powershell
pwsh -NoProfile -File Tools\Tests\Invoke-DreamShaderTests.ps1 -Preset Suite -WithoutCustomPass -Filter DreamShader.Lang2.Pipeline+DreamShader.Compiler2.Pipeline+DreamShader.Pass.Logic
```

在 `0` 下，管线会测试构建资源是否被编译排除、
`DreamShader.Compiler2.Pipeline.NeedsCustomPass` 是否被编译包含：一次 `.dsp` 构建会报告 `DSH8300` 并且
不写入任何内容。渲染和生命周期测试会随渲染器一起被编译排除。

下一次不带该开关的构建会自行恢复渲染器：更改后的 UBT 命令行
会使 UBT 的 makefile 失效，因此规则会被重新求值。由
规则文件读取的环境变量不会被看到——UBT 会保留它上次求值的规则，直到规则文件发生变化，并且
在 5.8 上，即使在 `-NoUBTMakefiles` 下它也会加载其 makefile。运行前，脚本会从 UBT 为 `DreamShaderPass` 写入的定义中读取
二进制文件位于哪一侧，并在与所要求的一侧不符时停止，
例如在构建了另一种类型之后出现 `-NoBuild` 的情况。

<a id="command-line-switches"></a>

### 命令行开关

| 开关 | 对测试套件的影响 |
| :-- | :-- |
| `-nullrhi` | 无渲染设备。`DreamShader.Render.ThinCustomVsGraphParity` 和 `DreamShader.Roundtrip.MTestToonRenderParity` 自动跳过；`NonNullRHI` 测试（Custom Pass 渲染、生命周期、槽注册表）完全不运行。其他所有测试仍会运行。 |
| `-DreamShaderUpdateGolden` | 每个语料运行器都会根据实际结果**重写**每个 `.expected.json`，而不是对其进行断言。参见[重新生成黄金文件](#regenerating-goldens)。 |
| `-NoDreamShaderEditorBridge` | 跳过创建编辑器桥接和材质内容浏览器：没有目录监视器、启动时没有临时生成阶段、`127.0.0.1:17864` 上没有 WebSocket 监听器。当一次运行不能与桥接竞争相同源文件时很有用。 |
| `-unattended -nopause -nosplash` | 标准无头标志；没有模态对话框、没有启动画面、退出时不按键。 |
| `-log` / `-stdout` | 将日志路由到控制台。 |

> [!WARNING]
> **被跳过的测试会报告成功。** 测试套件中的每条跳过路径都会调用 `AddInfo(...)`，然后
> 调用 `return true`。因此一次 `-nullrhi` 运行会显示 `DreamShader.Render.ThinCustomVsGraphParity` 和
> `DreamShader.Roundtrip.MTestToonRenderParity` 为绿色，**却没有比较过一个像素**，而
> 在 UE 5.3 上的运行会显示 `DreamShader.Compiler.Generate.SubstrateMaterial` 为绿色，却没有
> 编译过 Substrate 材质。要证明渲染一致性，请在不带 `-nullrhi` 的情况下运行，并阅读日志中的信息
> 行。

<a id="skip-conditions"></a>

### 跳过条件

测试在未断言任何内容的情况下返回成功的所有条件。

| 测试 | 条件 | 消息 |
| :-- | :-- | :-- |
| `DreamShader.Compiler.Generate.SubstrateMaterial` | `DREAMSHADER_WITH_SUBSTRATE_BUILTINS` 为 0 — UE < 5.4 | `DreamShader Substrate builtins are not available for this Unreal Engine version; skipping the compile test.` |
| `DreamShader.Render.ThinCustomVsGraphParity` | `GUsingNullRHI` 或 `!FApp::CanEverRender()` | `Skipping ThinCustom-vs-Graph render parity: no usable RHI (-nullrhi). Run without -nullrhi for the full pixel comparison.` |
| `DreamShader.Roundtrip.MTestToonRenderParity` | `GUsingNullRHI` 或 `!FApp::CanEverRender()` | `Skipping M_Test_Toon round-trip render parity: no usable RHI (run without -nullrhi).` |
| `DreamShader.Roundtrip.MTestToonRenderParity` | 它进行往返转换的项目资源不存在 | `Skipping M_Test_Toon round-trip: '{ObjectPath}' is not present in this project.` |

本页每个消息表中，运行时替换都显示为 `{Placeholder}`。

<a id="shared-harness"></a>

## 共享测试框架

编辑器级测试都使用相同的脚手架。

| 辅助工具 | 行为 |
| :-- | :-- |
| `MakeUniqueTestAssetName(Prefix)` | `<Prefix>_<GUID digits>` — 两次运行不会冲突。 |
| `GetAutomationSourceDirectory()` | `<SourceDirectory>/Tests/Automation`，即默认情况下为 `DShader/Tests/Automation`。 |
| `WriteAutomationSourceFile(...)` | 以不带 BOM 的 UTF-8 格式将测试的 `.dsm` / `.dsf` 写入那里。失败消息：`Failed to write DreamShader automation source file '{Path}'.` |
| `MakeAutomationObjectPath(Name)` | `/Game/DreamShaderTests/Automation/<Name>.<Name>` |
| `FScopedDreamShaderAutomationArtifacts` | RAII 清理：使用 `ObjectTools::DeleteObjectsUnchecked` 删除每个已注册资源，然后删除每个已注册源文件。 |
| `AddExpectedNewAssetProbeWarnings(...)` | 抑制 `SkipPackage: <pkg>` 和对象路径探测警告，以出现次数 `-1` 注册（如果存在则抑制，不要求存在），因为较新的引擎并不总是发出它们。 |
| `AddExpectedAutomationCleanupWarnings(...)` | 抑制 `package was marked as deleted in editor, but has been modified on disk`。 |
| `FDreamShaderQuietAutomationTestBase` | 基类，其 `SuppressLogErrors()` / `SuppressLogWarnings()` 返回 `true`；需要它是因为图自动布局会触发一个良性的引擎 `SlowTask` ensure。 |
| `FScopedDreamShaderGraphBackendPin` | RAII：在作用域内强制 `UDreamShaderSettings::DefaultBackend = Graph` 并恢复先前的值。 |

> [!NOTE]
> `Compile/` 语料黄金文件编码的是 **Graph-backend** 语义，因此该运行器在每个用例期间都固定
> `DefaultBackend = Graph`。`Legacy/Compile/` 是例外：它的
> 黄金文件来自 2026-09-15 的 1.x 捕获，并固定为 **ThinCustom**，即项目默认值。

<a id="test-index"></a>

## 测试索引

按翻译单元列出的每个声明。

<a id="dreamshaderautomationtestscpp--30-declarations"></a>

### `DreamShaderAutomationTests.cpp` — 30 个声明

除非注释另有说明，否则其中每一个都会向 `DShader/Tests/Automation` 写入一个源文件，
从中生成，针对生成的资源进行断言，并在退出时删除两者。

| 测试 | 宏 | 备注 |
| :-- | :-- | :-- |
| `DreamShader.Compiler.Parser.MinimalMaterial` | SIMPLE | — |
| `DreamShader.Compiler.Generate.MinimalMaterial` | SIMPLE | — |
| `DreamShader.Compiler.Generate.DsfWithImport` | SIMPLE | — |
| `DreamShader.Compiler.Generate.SubstrateMaterial` | SIMPLE | 在 UE 5.4 以下跳过 |
| `DreamShader.Compiler.SourceHash.SkipUnchangedMaterial` | SIMPLE | 断言对未更改源码的第二次编译会被跳过 |
| `DreamShader.Commandlet.Compile.SingleSourceSmoke` | SIMPLE | 运行 commandlet 编译路径，该路径会持久化一个真实的 `/Game` 资产 |
| `DreamShader.Gen.Wiring.TruthyCondition` | CUSTOM_SIMPLE，quiet base | — |
| `DreamShader.Roundtrip.MaterialDecompiles` | CUSTOM_SIMPLE，quiet base | — |
| `DreamShader.Roundtrip.SubstrateMaterialRegenerates` | CUSTOM_SIMPLE，quiet base | — |
| `DreamShader.Roundtrip.SwitchTypedAppendRegenerates` | CUSTOM_SIMPLE，quiet base | — |
| `DreamShader.Roundtrip.CustomAdditionalOutputRegenerates` | CUSTOM_SIMPLE，quiet base | — |
| `DreamShader.Roundtrip.StaticBoolFunctionInputRegenerates` | CUSTOM_SIMPLE，quiet base | — |
| `DreamShader.Gen.Graph.ArithmeticNodes` | CUSTOM_SIMPLE，quiet base | — |
| `DreamShader.Gen.Graph.MathBuiltinNodes` | CUSTOM_SIMPLE，quiet base | — |
| `DreamShader.Gen.Parameters.NodeCreation` | CUSTOM_SIMPLE，quiet base | 声明带内联默认值和不带内联默认值的参数，并断言创建了匹配的 `UMaterialExpression*Parameter` 节点 |
| `DreamShader.Gen.Parameters.OtherNodeCreation` | CUSTOM_SIMPLE，quiet base | 对 Scalar / Vector / Dynamic 之外的参数类型采用相同轴线。纹理对象和资产必需类型不在范围内 |
| `DreamShader.Gen.Parameters.InputWiring` | CUSTOM_SIMPLE，quiet base | `Param(InputPin = expr)` 调用形式；断言命名引脚最终被连接 |
| `DreamShader.Gen.Layout.NoOverlap` | CUSTOM_SIMPLE，quiet base | 生成一个四输出材质，并断言没有两个已放置节点重叠，且没有节点超出其注释框；使用与放置时相同的 `EstimateMaterialNodeSize` 测量每个节点。该夹具需要平衡树（rank 是到汇点的距离，因此链式结构每列只留一个节点，没有可碰撞对象），还需要一对八参数 `Function` 调用点——普通数学节点默认 `bCollapsed`，不绘制预览且高度约为 124，可避开任何合理的固定行距；调用点生成的 `Custom` 节点约为 416 |
| `DreamShader.Compiler.Generate.InstanceAlias` | SIMPLE | — |
| `DreamShader.Compiler.Generate.ThinCustomBackend` | SIMPLE | — |
| `DreamShader.Render.ThinCustomVsGraphParity` | CUSTOM_SIMPLE，quiet base | **需要真实的 RHI。** 参见 [渲染一致性](#render-parity) |
| `DreamShader.Roundtrip.MTestToonRenderParity` | CUSTOM_SIMPLE，quiet base | **需要真实 RHI** 以及项目专用资产；两者任一缺失时跳过 |
| `DreamShader.Roundtrip.RenamedChannelUsesMask` | CUSTOM_SIMPLE，quiet base | — |
| `DreamShader.Compiler.Generate.ThinCustomTexture` | SIMPLE | — |
| `DreamShader.Compiler.Generate.ThinCustomUI` | SIMPLE | — |
| `DreamShader.Compiler.Generate.ThinCustomPostProcess` | SIMPLE | — |
| `DreamShader.Compiler.Generate.ThinCustomSceneReads` | SIMPLE | — |
| `DreamShader.Compiler.Generate.ThinCustomMaterialAttributes` | SIMPLE | — |
| `DreamShader.Compiler.Generate.InstanceAliasStateReads` | SIMPLE | — |
| `DreamShader.Compiler.Generate.InstanceAliasImportedFunction` | SIMPLE | — |
| `DreamShader.Compiler.Generate.InstanceAliasBaseOverrides` | SIMPLE | — |

<a id="dreamshaderparametertestscpp--3-declarations"></a>

### `DreamShaderParameterTests.cpp` — 3 个声明

| 测试 | 覆盖内容 |
| :-- | :-- |
| `DreamShader.Lang.ParameterExpressions.ParseAll` | 24 个声明，覆盖 23 个不同的参数节点关键字；`ScalarParameter`、`VectorParameter` 和 `TextureObjectParameter` 各出现两次，一次带内联默认值，一次不带 |
| `DreamShader.Lang.ParameterExpressions.GroupScope` | `Group("X") { … }` 标记分组；松散属性保持未分组且未排序；自动 `SortPriority` 是从 0 开始、步长为 10 的全局计数器；显式 `SortPriority` 优先且不占用计数器槽位；`Slider(0, 1)` 会展开为恰好两个反射属性 |
| `DreamShader.Lang.ParameterExpressions.NestedGroupScope` | 嵌套分组通过 `\|` 组合——`Group("Surface") { Group("SS") { … } }` 产生 `Surface\|SS`；字面量 `Group("Manual\|Literal")` 原样传递 |

<a id="dreamshaderpurefunctiontestscpp--8-declarations"></a>

### `DreamShaderPureFunctionTests.cpp` — 8 个声明

不依赖编辑器、世界或资产；这些测试在毫秒级内运行。

| 测试 | 覆盖内容 |
| :-- | :-- |
| `DreamShader.Lang.Diagnostics.ParseErrorLocation` | `TryParseErrorLocation`，包括将行/列限制到 `>= 1` 并拒绝非数字坐标 |
| `DreamShader.Lang.Diagnostics.BuildGenerateDiagnostics` | `BuildGenerateErrorDiagnostics` 行拆分 |
| `DreamShader.Lang.Diagnostics.TextWireUtils` | 诊断传输文本在 `en-US` 和 `zh-Hans` 编辑器区域设置下相同 |
| `DreamShader.Lang.Diagnostics.NoFile` | 无文件、以空路径报告的诊断不携带位置信息：其记录或编译错误中都没有编造的文件 |
| `DreamShader.Lang.Import.ExtractImportPath` | `TryExtractImportPathFromLine`：引号规则、注释拒绝、尾部垃圾拒绝 |
| `DreamShader.Lang.Import.NormalizeSpecifier` | `NormalizeImportSpecifier`：无扩展名的说明符获得 `.dsh`，反斜杠和前导 `./` 被剥离 |
| `DreamShader.Commandlet.Args.SplitAndGet` | Commandlet 键/值规范化以及 `Params → Switches → Tokens` 搜索顺序，基于来自 `UCommandlet::ParseCommandLine` 的列表 |
| `DreamShader.Commandlet.Args.FlagValues` | `HasCommandletFlag`：逐行验证 [布尔标志](../../tools/commandlet.md#boolean-flags) 表格行，任何其他值均为 `DSH9110` |

<a id="corpus-runners--13-declarations"></a>

### 语料运行器 — 13 个声明

| 测试基名 | 语料子树 |
| :-- | :-- |
| `DreamShader.Lang.Parse` | `Parse/` — 1.x 解析等价集，由旧版前端读取 |
| `DreamShader.Lang2.Corpus` | `Lang/` |
| `DreamShader.Lang2.CorpusIR` | `IR/`（包含其 `Instances/*.dsi`） |
| `DreamShader.Lang2.CorpusLegacyParse` | `Legacy/Parse/` |
| `DreamShader.Lang2.CorpusLegacyIR` | `Legacy/IR/` |
| `DreamShader.Lang2.CorpusDecompile` | `Decompile/` |
| `DreamShader.Lang2.CorpusMigrate` | `Migrate/` |
| `DreamShader.Lang2.RoundtripIR` | 扫描 `IR/`、`Lang/Examples/` 和 `Compile/`，不使用文本黄金文件 |
| `DreamShader.Lang2.Trivia.CommentInvariant` | 两种语言中每个可解析的夹具，每个语料目录一个子测试：没有注释在解析 → 打印过程中丢失 |
| `DreamShader.Compiler2.Corpus` | `Compile/` |
| `DreamShader.Compiler2.CorpusLegacy` | `Legacy/Compile/` — 原 `Generate/` 夹具 |
| `DreamShader.Compiler2.CorpusInstance` | `Instance/` |
| `DreamShader.Compiler2.Roundtrip.Corpus` | `Roundtrip/` |

四个 `Compiler2` 运行器是带 quiet base 的 `IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST`（图自动布局会触发一个良性的引擎 ensure）；其余为 `IMPLEMENT_COMPLEX_AUTOMATION_TEST`。

<a id="graph-baseline-since-190"></a>

## 图基线 *(自 1.9.0 起)*

自动化套件说编译仍然*成功*。它并没说编译器仍然产生**相同的图**——而对于编译器重写来说，这是唯一重要的问题。[`dump-graph`](../../tools/commandlet.md#dump-graph-since-190) commandlet 动词填补了这一空白：它为每个生成的资产写出一份规范化 JSON（节点类、反射属性、连接、引脚顺序；不含坐标、颜色或 GUID），然后通过普通文本 diff 比较两次捕获。

为整个项目捕获基线。编译步骤不是可选的——`dump-graph` 从不写入资产，因此磁盘上已有的资产会按现状转储，而不是重新构建：

```powershell
& $UnrealEditorCmd $Project -run=DreamShader compile -All -Force -unattended -nopause -nullrhi -nosplash -stdout -log
& $UnrealEditorCmd $Project -run=DreamShader dump-graph -All -Out="I:/Baseline/before" -unattended -nopause -nullrhi -nosplash -stdout -log
```

然后，在更改之后：

```powershell
git diff --no-index -- I:/Baseline/before I:/Baseline/after
```

每个差异块都是一个行为差异。`./dsc.ps1 dump-graph -All -Out …` 是同样两条命令的简短写法。

三个自动化测试守护转储本身，而第一个是无法通过阅读代码替代的——它生成同一份源码**两次**并断言两个 JSON 文件逐字节相同，这是发现泄漏的 GUID、以路径名作种子的节点颜色或对象名后缀的唯一方法：

| 测试 | 断言 |
| :-- | :-- |
| `DreamShader.DumpGraph.Determinism` | 对同一份源码的两次完整生成转储出相同的字节；LF 结尾；一个尾部换行 |
| `DreamShader.DumpGraph.ExcludesEditorState` | 没有任何 `MaterialExpressionGuid`、`EditorX/Y`、`NodeColor`、`Desc`、`bCollapsed`、`VariableGuid`… 键留存——而行为键则会留存 |
| `DreamShader.DumpGraph.NodeCount` | 微小夹具转储三个节点，图中的每个表达式恰好出现一次，且 id 从 `n0`…`n(N-1)` |

三者都将项目的 `DefaultBackend` 固定为 `Graph` 与 `FScopedDreamShaderGraphBackendPin`，并清理其源文件、资产和临时输出树。

<a id="render-parity"></a>

## 渲染一致性

两种像素测试都会渲染两个材质并逐帧比较。
`ThinCustomVsGraphParity` 从同一主体构建成对内容，每个后端一个（`Graph` 和
`ThinCustom`）；`MTestToonRenderParity` 将项目材质与其自身反编译源码重新生成的材质进行比较
（`Original` 对 `RoundTripTwin`）。

| 标准 | 值 |
| :-- | :-- |
| 渲染尺寸 | 256 × 256 |
| 网格 | `sphere` |
| 环绕偏航角 / 俯仰角 | `-157.5` / `-11.25` |
| 每通道容差 | `2` |
| 允许的差异像素数 | `PixelCount / 1000`，即 0.1 % |
| 内容哨兵 — ThinCustom 一致性 | 超过 5 % 的像素必须以绿色为主（`G > R + 30` 和 `G > B + 30`） |
| 内容哨兵 — `MTestToonRenderParity` | 超过 5 % 的像素与清除颜色 `(44, 44, 48)` 的差异必须大于 8 |
| 预热 | 每个孪生体先进行一次丢弃渲染，然后 `FAssetCompilingManager::FinishCompilationForObjects`、`GShaderCompilingManager->FinishAllCompilation()` 和 `FlushRenderingCommands()` |
| 失败产物 | `<Project>/Saved/DreamShaderTests/Parity_<Case>_<Backend>.png` 用于 `ThinCustomVsGraphParity`，`<Project>/Saved/DreamShaderTests/MTestToon_<Label>.png` 用于 `MTestToonRenderParity` |

一致性用例 — 同一主体的 ThinCustom 与 Graph 孪生体，仅在 `Backend` 上不同：

| 用例 | 测试内容 |
| :-- | :-- |
| `FlatParams` | 在 Unlit、Opaque 材质上按名称绑定 `ScalarParameter` / `VectorParameter` |
| `UvTexture` | `TextureObjectParameter` 加上 `UE.TexCoord(Index=0)` 和 `SampleTexture2D` — 插值器和纹理路径 |
| `LitAttributes` | `DefaultLit` 材质上的 `MaterialAttributes` 输出 — MakeMaterialAttributes 路径 |

<a id="the-corpus"></a>

## 语料库

`Tests/Corpus` 位于**插件**之下，而不是项目的 `DShader` 树之下，因此它随插件一起分发，并且永远不会被普通的源文件发现机制拾取。
[`Tests/Corpus/README.md`](../../../Tests/Corpus/README.md) 是该树、每个黄金基准模式以及每个运行器的参考；本节是地图。

| 层 | 源文件 | 用例做什么 | 需求 |
| :-- | --: | :-- | :-- |
| `Parse/` | 36 | 1.x 文本由旧版前端完全按照 1.x 的方式接受或拒绝 | `Core` |
| `Lang/` | 72 | 一个 `ParseDreamShaderLang`，结构计数，打印 → 解析 → 打印字节完全一致 | `Core` |
| `IR/` | 167 | 绑定 → 构建 → 通过 → 验证；黄金基准是 IR 转储 | `Core`，手工制作的目录 |
| `Legacy/Parse/` | 121 | 1.x 文本 → 旧版前端（带琐碎内容）→ 其打印出的 2.0 文本，该文本必须能作为 2.0 解析 | `Core` |
| `Legacy/IR/` | 54 | 每条文档化的 1.x 规则（L2–L27）对应一个夹具，以及 Substrate 语法糖按 1.x 的写法 | `Core`，手工制作的目录 |
| `Decompile/` | 25 | `.dss` → IR → AST → 文本，并且该文本可降低为等价 IR | `Core`，手工制作的目录 |
| `Migrate/` | 22 | 1.x 文本 → 迁移器 → 2.0 文本：无注释丢失，可构建，IR 等价 | `Core`，手工制作的目录 |
| `Compile/` | 12 | 整个管线到资产；黄金基准保存图转储 | 编辑器 |
| `Legacy/Compile/` | 12 | 1.x 源文件同理 | 编辑器 |
| `Instance/` | 10 | 一个 `.dsi`，其 `Parent` 命名的同级 `.dss` | 编辑器 |
| `Roundtrip/` | 11 | `.dss` → 资产 → 反编译服务 → 文本 → 资产；两个转储相等 | 编辑器 |
| `Parity/` | — | 仅黄金基准：`Lang/Examples` 源文件必须复现的 1.x 图捕获 | 编辑器 |

发现按层的扩展名递归进行，然后排序。

> [!WARNING]
> `Tests/` **不在**[发布存档](release.md#archive-contents)中。从发布 zip 安装的插件没有语料库，因此十三个运行器枚举到零个子测试，测试套件会静默收缩为其简单声明。请从仓库检出中运行语料库。

<a id="naming-convention"></a>

### 命名约定

| 约定 | 规则 |
| :-- | :-- |
| 层前缀 | 每个目录一个短前缀（`L_` 词法，`T_` 顶层，`S_` 节，`Ty_` 类型，`E_` 表达式，`D_` 声明或反编译，`Rf_` 反射，`Cu_` 自定义，`M_` 材质，`I_` 实例，`H_` 头文件……） |
| 负向用例 | 文件名**包含 `.bad.`**，不区分大小写匹配。没有黄金基准时，默认预期翻转为“这失败” |
| 黄金基准名称 | 仅剥离最后一个扩展名，因此 `X.bad.dsm` 与 `X.bad.expected.json` 配对，`X.dsm` 与 `X.expected.json` 配对 |
| 黄金基准存在性 | 可选 — 缺失意味着默认值适用：正向 ⇒ 必须成功，`.bad.` ⇒ 必须失败 |
| 子测试名称 | 相对于层目录的路径，剥离扩展名并将 `/` 和 `\` 替换为 `.` |

生成的完整测试名称：

```text
DreamShader.Lang.Parse.TopLevel.T_MinimalShader
DreamShader.Lang2.CorpusIR.Reflected.Rf_LateBoundPin
DreamShader.Compiler2.CorpusLegacy.Material.M_Surface
```

<a id="entry-point-per-layer"></a>

### 每层的入口点

| 层 | 入口点 |
| :-- | :-- |
| `Parse/`，`Lang/` | `ParseDreamShaderLang` — `Auto` 为 `.dsm` / `.dsf` 选择旧版前端，并在 `.dsh` 中按声明选择 |
| `IR/`，`Legacy/IR/` | `BindDreamShaderLang` → `BuildDreamShaderIR` → `RunDreamShaderIRPasses` → `ValidateDreamShaderIR` |
| `Legacy/Parse/` | `ParseDreamShaderLang` 带 `bKeepTrivia` → `PrintDreamShaderLang`，并将打印出的文本再次作为 2.0 解析 |
| `Decompile/` | `RaiseDreamShaderIR` → `BuildDreamShaderAstFromIR` → 打印 → 再次降低 → `AreDreamShaderIRModulesEquivalent` |
| `Migrate/` | 旧版解析 + 绑定 → `MigrateDreamShaderLegacyModule` → 打印 → 注释计数 → 作为 2.0 构建 → IR 等价 |
| `Compile/`，`Legacy/Compile/`，`Instance/`，`Parity/` | `CompileDreamShaderTestAssets` — 编译器服务的 `CompileAssets` 之上的测试门面 —— 然后 `dump-graph` 对每个资产的转储 |
| `Roundtrip/` | 编译 → 对整个源文件执行 `RunDreamShaderDecompileRequest` → 在另一个临时根目录下编译文本 → 比较两个转储 |

编译层会将每个夹具复制到临时源根目录并构建到
`/Game/DreamShaderTests/...` 中，然后清理两者。

<a id="expectedjson"></a>

## `.expected.json`

每个字段都是选择加入的；缺失的字段不做任何断言。下表是 **`Parse/` 层**的黄金基准（`"entryPoint": "parse"`）：由于 1.x 解析器已在 2.0 中退役，其 `definition` 字段是从旧版前端的 AST 和 `FLegacyMigrationInfo` 中读取的，而不是从
`FTextShaderDefinition` 中读取，字段名相同。其他每一层都有自己的形态 — `lang`、`ir`、
`legacy`、`legacy-ir`、`decompile`、`migrate`、`compile`、`roundtrip` — 记录在
[`Tests/Corpus/README.md`](../../../Tests/Corpus/README.md) 中。它们共享两个约定：
`errorContains` / `warningsContain` / `infosContain` 保存 `DSHnnnn` 代码而非消息文本，
以及 `...Pending: true` 标志（`textPending`、`irPending`、`graphPending`）会跳过尚未有人审核的黄金基准的字节比较 — 移除该标志是审核者的行为，绝不是编写者的行为。

| 字段 | 类型 | 层 | 含义 |
| :-- | :-- | :-- | :-- |
| `entryPoint` | string | — | 在 `Parse/` golden 中**仅供参考**：其解码器从不读取它。`Parse/` writer 会输出 `"parse"` |
| `outcome` | string | all | `"error"`，以不区分大小写方式比较，期望失败；**任何其他值，包括 `"ok"`，都期望成功**。覆盖 `.bad.` 文件名默认值 |
| `errorContains` | string[] | all | 在预期失败时检查：每个子字符串都必须出现在某个错误诊断中，以不区分大小写方式比较——实际中即 `DSHnnnn` 代码。`messageContains`，即 1.x Generate 层的拼写，会被读入同一列表 |
| `warningsContain` | string[] | all | 每个子字符串都必须出现在某个警告诊断中，不区分大小写 |
| `definition.name` | string | Parse | 第一个产品块的 `Name=` |
| `definition.settings` | object | Parse | 字符串 → 字符串。通过 `TryGetSetting` 断言：**键不区分大小写，值精确匹配**；列出的每个键都必须存在 |
| `definition.outputDeclarations` | number | Parse | 输出声明的数量 |
| `definition.outputs` | number | Parse | 输出绑定的数量 |
| `definition.materialFunctions` | number | Parse | `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` 块的数量 |
| `definition.materialFunction0Kind` | string | Parse | 值之一：`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend`。仅当至少存在一个材质函数时检查 |
| `definition.virtualFunctions` | number | Parse | `VirtualFunction` 块的数量 |
| `definition.codeNotEmpty` | bool | Parse | `Definition.Code` 是否非空 |

> [!WARNING]
> `outcome` 仅与 `"error"` 进行比较（不区分大小写，因此 `"Error"` 和 `"ERROR"`
> 都没问题）。一个笔误——`"fail"`、带尾随空格的 `"error "`、`"errors"`——会让 golden 期望**成功**，而一个本应
> 固定失败结果的夹具就会悄然开始断言相反的结果。请只使用 `"ok"` 或 `"error"`。

<a id="adding-a-fixture"></a>

## 添加夹具

1. 将源文件放入该层的目录——`Tests/Corpus/IR/<Area>/X.dss`、
   `Tests/Corpus/Legacy/Compile/Material/M_X.dsm`、…… 使用层前缀；对负向用例在名称主干中添加 `.bad.`。
2. 可选地添加 `<same stem>.expected.json`，并包含上表中字段的任意子集。没有 golden 的正向用例
   仍断言“必须成功”；没有 golden 的 `.bad.` 用例断言“必须失败”。
3. **不要编写 C++，也不要重新编译。** 每个运行器都会在下次运行时枚举其目录树。
4. 运行该层一次以查看新的子测试出现：
   `-ExecCmds="Automation RunTests DreamShader.Lang.Parse; Quit"`。
5. 要从实际结果引导生成 golden，请添加 `-DreamShaderUpdateGolden` 并再次运行，然后在提交前
   手动审查写出的 JSON。新的文本、IR 或图形 golden 在写入时会设置
   其 `...Pending` 标志；在逐行阅读 golden 后删除该标志。

<a id="regenerating-goldens"></a>

### 重新生成 golden

在命令行中加上 `-DreamShaderUpdateGolden` 时，每个运行器会写出 golden 而不是
对其进行断言，并记录 `Updated golden '{Path}'.`；若写入失败，则记录 `Failed to write golden '{Path}'.`

`Parse/` writer 输出什么：`entryPoint: "parse"`、`outcome`，然后是
`errorContains: [<the codes>]` 或一个 `definition` 对象，包含 `name`（为空时省略）、
`outputDeclarations`、`outputs`、`materialFunctions`、`materialFunction0Kind`（仅当至少存在一个时）、
`virtualFunctions`、`codeNotEmpty` 和 `settings`（仅当非空时）；另外，当解析产生警告时，
会有一个顶层 `warningsContain`。其他层的 writer 及其 schema 在
语料库 README 中描述。

输出为格式化美观的 JSON。

> [!WARNING]
> `-DreamShaderUpdateGolden` 会按代码当前的行为重写 golden。在未审查的更改之后运行它
> 会把回归当作新的基线接受。提交前请对每个被触及的
> `.expected.json` 进行差异对比，并且绝不要把该开关放入 CI 任务。

<a id="diagnostics"></a>

## 诊断

运行器发出的断言和基础设施消息。运行时替换显示为
`{Placeholder}`；`{Case}` 是夹具的**绝对源路径**，而不是其子测试名称。

| 消息 | 层 | 原因 |
| :-- | :-- | :-- |
| `[{Case}] the legacy front end should REFUSE this source` | Parse | 夹具被接受但预期失败 *(自 2.0.0 起)* |
| `[{Case}] the legacy front end should ACCEPT this source but reported: {Error}` | Parse | 夹具被拒绝但预期成功 *(自 2.0.0 起)* |
| `[{Case}] an error contains '{Substring}' (actual: {Error})` | Parse | 未找到 `errorContains` 条目 |
| `[{Case}] a warning contains '{Substring}' (actual: {Warnings})` | Parse | `warningsContain` 条目未匹配任何警告 |
| `[{Case}] name == '{Expected}' (actual '{Actual}')` | Parse | `definition.name` 不匹配 |
| `[{Case}] outputDeclarations` | Parse | `definition.outputDeclarations` 不匹配 |
| `[{Case}] outputs` | Parse | `definition.outputs` 不匹配 |
| `[{Case}] materialFunctions` | Parse | `definition.materialFunctions` 不匹配 |
| `[{Case}] materialFunction0Kind` | Parse | `definition.materialFunction0Kind` 不匹配 |
| `[{Case}] virtualFunctions` | Parse | `definition.virtualFunctions` 不匹配 |
| `[{Case}] codeNotEmpty == {Value}` | Parse | `definition.codeNotEmpty` 不匹配 |
| `[{Case}] setting '{Key}' present` | Parse | `definition.settings` 中的某个键缺失 |
| `[{Case}] setting '{Key}' == '{Expected}' (actual '{Actual}')` | Parse | 该键的值不同 |
| `Cannot read corpus source '{Path}'.` | all | 无法读取夹具文件 |
| `Cannot read golden '{Path}'.` | all | `.expected.json` 存在但无法读取 |
| `Malformed golden '{Path}': {Reason}` | all | golden 不是有效 JSON（`invalid JSON`）或无法解码 |
| `Updated golden '{Path}'.` | all | `-DreamShaderUpdateGolden` 写入了 golden |
| `Failed to write golden '{Path}'.` | all | `-DreamShaderUpdateGolden` 无法写入它 |
| `DreamShader {layer} corpus test invoked without a source path.` | every runner | 子测试以空命令参数启动；`{layer}` 是 `parse`、`lang`、`IR`、`legacy parse`、`legacy IR`、`legacy compile`、`compile`、`instance`、`decompile`、`migrate` 或 `roundtrip`（该运行器为 `DreamShader RoundtripIR test …`） |
| `Failed to write DreamShader automation source file '{Path}'.` | harness | 测试无法写入其临时源文件 |

1.x Generate 运行器及其 `generation should …` 消息已消失 *(自 2.0.0 起)*：其夹具
为 `Legacy/Compile/`，通过 2.0 管线编译。其他层的断言消息位于
其运行器中，以及 [`Tests/Corpus/README.md`](../../../Tests/Corpus/README.md) 中。

<a id="example"></a>

## 示例

随附的负向夹具，用于固定“每个解析单元一个 `Shader`”规则。
`Tests/Corpus/Parse/TopLevel/T_TwoShaders.bad.dsm`：

```c
Shader(Name="DreamShaderTests/Corpus/M_First")
{
    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = { Color = vec3(1.0, 0.0, 0.0); }
}

Shader(Name="DreamShaderTests/Corpus/M_Second")
{
    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = { Color = vec3(0.0, 1.0, 0.0); }
}
```

仅 `.bad.` 名称主干本身会断言“解析必须失败”。其 golden，
`T_TwoShaders.bad.expected.json`，还固定了旧版前端为第二个 `Shader`
块抛出的代码，[`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250)：

```json
{
	"entryPoint": "parse",
	"outcome": "error",
	"errorContains": ["DSH2250"]
}
```

运行它：

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" --% "D:\Work\MyProject\MyProject.uproject" -ExecCmds="Automation RunTests DreamShader.Lang.Parse.TopLevel; Quit" -nullrhi -unattended -nopause -nosplash -log
```

新的子测试显示为：

```text
DreamShader.Lang.Parse.TopLevel.T_TwoShaders.bad
```

<a id="see-also"></a>

## 另请参阅

- [贡献](index.md) — 构建插件及源码树布局
- [发布](release.md) — 发布归档中为何不含 `Tests/`
- [Commandlet](../tools/commandlet.md) — `-run=DreamShader`，另一个无头入口点
- [编辑器桥接](../../tools/bridge.md) — `-NoDreamShaderEditorBridge` 会关闭什么
- [预览](../../tools/preview.md) — 一致性测试所驱动的渲染器
- [后端](../settings/backend.md) — `Graph` 与 `ThinCustom`，一致性用例所比较的维度
- [项目设置](../settings/project.md) — `DefaultBackend`，`Compile/` 运行器将其固定为 `Graph`
- [`DreamShaderLang`](../api/lang-module.md) — `ParseDreamShaderLang` 以及文本层调用的所有其他入口点
- [诊断索引](../diagnostics/index.md) — 测试夹具所断言的各类消息
- [版本兼容性](../api/version-compat.md) — `DREAMSHADER_WITH_SUBSTRATE_BUILTINS` 与 UE 5.4 跳过
</content>
