---
title: "项目结构"
description: "DShader/ 如何组织、六种源文件各存放什么、插件如何贡献自己的源码根、插件拥有哪些目录，以及决定哪些内容会被编译的 Package 规则。"
---

DreamShaderLang 源文件位于项目根目录下的一个目录中——默认是 `DShader`，由
*Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Paths ▸ Source Directory* 设置。相对路径会相对于项目
目录解析。

源目录、其 `Packages` 子文件夹以及生成的 shader 目录由插件在模块
启动时创建——也就是编辑器首次启动时，无论你是否编写过任何源文件。

（自 1.6.0 起）它不再是唯一的根：**每个启用了 `DShader` 文件夹的插件都会贡献一个自己的源
根**——见下方的[插件源码根](#plugin-source-roots)。

<a id="six-kinds-of-source-file"></a>

## 六种源文件

文件夹由你自行选择；重要的是扩展名。

| 扩展名 | 是什么 | 产出 |
| --- | --- | --- |
| `.dss`（自 2.0.0 起） | DreamShaderLang 2.0 编译单元：一个材质**或**材质函数（包含 layer 和 blend） | 每个 `export` 一个资产 |
| `.dsi`（自 2.0.0 起） | 材质实例：父级和覆盖项 | 一个 `UMaterialInstanceConstant` |
| `.dsp`（自 2.1.0 起） | Custom Pass 管线（UE 5.8） | 一个 `UDreamPassPipeline` |
| `.dsm` | 一个 1.x 材质 | 其 `Shader`，外加在它旁边声明的函数和 layer 资产 |
| `.dsf` | 一个 1.x 材质函数文件 | 它声明的函数资产 |
| `.dsh` | header：辅助项、`VirtualFunction` / `extern` 声明、共享常量；两种方言均可容纳 | 不产出任何内容——它只通过导入或包含它的文件进入编译 |

请将新源文件写为 `.dss`；`.dsm` / `.dsf` 是 1.x 语法，仍然通过同一个编译器构建，因此
项目可以用 [`dsc migrate`](../tools/migrate.md) 每次迁移一个文件。每种类型可以包含什么
见[文件模型](../language/source-files.md)。

<a id="the-tree"></a>

## 目录树

```text
MyProject/
├─ DShader/
│  ├─ Materials/
│  │  ├─ M_Sample.dss
│  │  └─ M_Legacy.dsm
│  ├─ Functions/
│  │  ├─ F_Tint.dss
│  │  └─ F_Old.dsf
│  ├─ Instances/
│  │  └─ MI_Sample_Red.dsi
│  ├─ Passes/
│  │  ├─ P_Outline.dsp
│  │  └─ M_OutlineComposite.dss
│  ├─ Shared/
│  │  ├─ Common.dsh
│  │  └─ Color.dsh
│  ├─ VirtualFunctions/
│  │  └─ Engine.dsh
│  ├─ Decompiled/
│  │  ├─ Materials/
│  │  └─ Functions/
│  ├─ Packages/
│  │  └─ @typedreammoon/
│  │     └─ dream-noise/
│  │        ├─ dreamshader.package.json
│  │        └─ Library/
│  │           └─ Noise.dsh
│  ├─ .dreampass/
│  ├─ DreamShader.code-workspace
│  └─ dreamshader.lock.json
├─ Intermediate/
│  └─ DreamShader/
│     └─ GeneratedShaders/
└─ Plugins/
   └─ DreamShader/
```

<a id="plugin-source-roots"></a>

## 插件源码根

（自 1.6.0 起）

插件可以随附构建其材质的源文件，而不必将它们放在项目的目录树中。

```text
Plugins/
└─ MoonToon/
   ├─ DShader/
   │  ├─ Materials/
   │  │  └─ M_Toon.dss
   │  ├─ Shared/
   │  │  └─ Toon.dsh
   │  └─ Packages/
   └─ MoonToon.uplugin
```

| 行为 | 规则 |
| :-- | :-- |
| 发现 | 无需配置。开关是 *Project Settings ▸ … ▸ Paths ▸ **Scan Plugin Source Directories***（默认开启） |
| 默认资产目标 | 插件根下的文件会生成到**其自身插件的挂载点**——`/MoonToon`，而不是 `/Game`。在 1.x 文件中，只有缺失或全为空白字符的 `Root` 才会被默认化，因此 `Root="/"` 会重新选择回到 `/Game` |
| 导入 | **相对路径永不跨越根。** 文件只会相对于其自身所在的根以及该根的 `Packages` 解析；要使用另一个根中的 header，请写入以 `/` 开头的路径（先找自己的根，再找其他根）、复制它，或将其作为 Package 交付——见[导入](../language/import.md#crossing-a-root-deliberately) |
| 可写性 | **只有项目根可写。** VirtualFunction 同步会跳过插件根下的文件，并且对于插件随附的源文件，*Adopt* 会被禁用：插件按其作者撰写的样子交付其定义 |
| 监视器 | 每个根一个监视器，因此插件的源文件也会获得 *Auto Compile On Save* |
| 工作区 | `DreamShader.code-workspace` 每个根有一个 `folders` 条目——项目为 `"."`，插件为 `Plugin: <Name>` |
| Material Content Browser | **Root** 一列标明 *Project* 或所属插件，搜索框也会匹配根名称——输入插件名称即可筛选出它随附的所有内容 |
| Cook | 另一个插件自身源文件下的失败（`Root` 指定了引擎或商城插件，`DSH8090`）不再中止宿主项目的 cook；它会被记录为 `[Cook] Not this project's to build`（自 2.1.2 起）。项目自身的源文件仍会中止它 |

**警告：**

与现有根**重叠**的插件 `DShader` 文件夹会被忽略并给出警告，而不是让一个文件有两个
所有者。当 *Source Directory* 指向某个插件文件夹，或指向包含某个插件文件夹的位置时，就会发生这种情况。

**注意：**

根列表会被缓存，仅在 *Source Directory* 或扫描开关发生变化时，或在
Material Content Browser 中刷新时才会重建——这也是拾取你刚创建的 `DShader` 文件夹或会话中途挂载的插件的方式。
但**监视器只在启动时注册一次**，因此对于会话中途出现的根，其 *Auto Compile On Save* 要等到下次编辑器启动才会
开始工作。

<a id="directory-roles"></a>

## 目录角色

| 目录 | 归属 | 内容 |
| --- | --- | --- |
| `DShader/` | 你 | 源根。按你喜欢的任何方式组织；此处的布局是约定，而非规则。 |
| `DShader/Materials/` | 你 | 材质源文件，`.dss` 或 `.dsm`，通常一个文件一个材质。 |
| `DShader/Functions/` | 你 | 材质函数：带导出函数的 `.dss`，或 1.x 的 `.dsf`。 |
| `DShader/Instances/` | 你 | `.dsi` 材质实例。 |
| `DShader/Shared/` | 你 | 项目内部的 `.dsh` header。 |
| `DShader/VirtualFunctions/` | 编辑器 | *CreateVirtualFunction* 为现有 `UMaterialFunction` 写入声明的位置。 |
| `DShader/Decompiled/` | 编辑器 | *Export .dss* / *Export .dsi* / *Export Legacy .dsm / .dsf* 的输出，分为 `Materials`、`Functions`、`Layers`、`LayerBlends`。 |
| `DShader/Packages/` | 扩展 | 已安装的共享库。始终是源根下那个字面量子文件夹 `Packages`；不能单独配置。 |
| `DShader/.dreampass/`（自 2.1.0 起） | 插件 | Custom Pass HLSL 的快照；只有通过预检查的才会进入全局 shader 槽位。随源文件一起提交。 |
| `DShader/DreamShader.code-workspace` | 插件 | 每次 *Open Dream Shader Workspace* 时整体重写。 |
| `DShader/dreamshader.lock.json` | 扩展 | 记录已安装的 Package 版本。插件既不读取也不写入它。 |
| `Intermediate/DreamShader/GeneratedShaders/` | 插件 | 映射到 `/DreamShaderGenerated`。（自 2.0.0 起）`Function` 的 HLSL 会写入其 Custom 节点，因此编译不会写入此处；在此发现的 `.ush` 是 1.x 构建遗留的，*Clean Generated Shaders* 会将其删除。 |

**警告：**

`DreamShader.code-workspace` 在每次调用时都从固定对象序列化而来——从不读取、合并或保留。
手动添加的 `launch`、`tasks`、`extensions` 或额外的 `settings` 都会被销毁。请将个人配置保存在
`DShader/.vscode/settings.json` 中，该命令永远不会触碰它。

<a id="naming-conventions"></a>

## 命名约定

| 类型 | 约定 |
| --- | --- |
| 材质 | `M_*.dss` / `M_*.dsm` |
| 材质函数 | `F_*.dss` / `F_*.dsf`；layer 为 `L_*`，blend 为 `LB_*` |
| 材质实例 | `MI_*.dsi` |
| Custom Pass 管线 | `.dsp`，放在其 pass 材质旁边 |
| 共享 header | 按领域命名——`Texture.dsh`、`Color.dsh` |
| Package 入口点 | `Library/<Name>.dsh` |

`.dss` 资产以其 `export` 函数命名，因此保持文件名和 `export` 名称一致可以避免
意外。扩展名匹配不区分大小写，因此 `M_Water.DSM` 也是材质文件。标识符则是另一回事：
插件在将命名空间和函数名净化成 HLSL 和资产
标识符时会替换非 ASCII 字符，因此请让这些名称保持 ASCII。

<a id="imports-and-includes"></a>

## 导入与包含

让每个材质的导入保持在少量稳定的入口点：

```c
// 1.x
import "Shared/Common.dsh";
import "@typedreammoon/dream-noise/Library/Noise.dsh";
```

```hlsl
// .dss
#include "Shared/Common.dsh"
#include "@typedreammoon/dream-noise/Library/Noise.dsh"
```

一个 specifier 会依次尝试三个候选：导入文件所在目录、**所属根**的源目录，以及该根的 `Packages` 目录。第一个存在**且仍位于其包含根内**的候选胜出 —— 包中的文件不能用 `../` 爬出去。没有扩展名的 specifier 会被追加 `.dsh`；只有 header 才能被引入（导入 `.dsf` / `.dsm` 是 `DSH2252`）。解析不到任何位置的则是 `DSH8292`。完整规则见
[导入与命名空间](../language/import.md)。

浅层导入还有另一项好处：构建键覆盖文件引入的每个 header，所以编辑一个 header 会让所有包含它的文件失效。

<a id="what-is-compiled-and-what-is-not"></a>

## 什么会被编译，什么不会

每条构建路径都采用相同的文件 *(自 2.1.2 起)* —— 启动时的扫描、一次保存及其排队的依赖项、*Recompile All*、commandlet 的 `compile -All`、cook 和 Material Content Browser：

| 位置 | 构建什么 |
| --- | --- |
| 源码根，在其 `Packages` 之外 | 每个 `.dss`、`.dsi`、`.dsp`、`.dsm` 和 `.dsf`；`.dsh` 仅通过包含它的源文件构建 |
| 包的 `Assets/` 文件夹 | 其 `.dss`、`.dsf` 和 `.dsi`，构建到包的资产根（`/Game/DreamShaderPackages/<scope>/<name>`） |
| 请求材质函数的包头 —— `/// @generate`、`#pragma generate`、1.x `Generate =` | 那些材质函数 |
| 包中的其他任何内容 —— `Examples/`、`Assets` 之外的 `.dss` 或 `.dsf` | 什么都不会构建，在任何路径上 |

要以示例为例，把它从 `DShader/Packages` 复制到 `DShader/` 下的某处。直到 2.1.2，包中 `Assets` 之外的 `.dsf`、`.dss`、`.dsi` 或 `.dsp` 会被文件监视器和 *Recompile DSM* 编译，但不会被 `compile -All`、cook 和 Material Content Browser 编译；请把这类源文件移到 `Assets` 下。见
[包 ▸ 包资产](../../tools/packages.md#package-assets)。

**注意：**

标记为 `NoGeneration` 的源文件（`#pragma nogeneration`、`/// @nogeneration`、1.x `NoGeneration = true`）会像其他文件一样被枚举，但只有点名它的编译才会构建其资产 —— 见
[NoGeneration](../generation/no-generation.md)。

包的 `.dsh` header 可以被导入，但它从不参与 `VirtualFunction` 声明扫描 —— 包中随附的 `VirtualFunction` 永远不会与它的资产进行校验。

<a id="source-control"></a>

## 版本控制

| 内容 | 是否提交？ |
| --- | --- |
| 每个源文件 —— `.dss`、`.dsi`、`.dsp`、`.dsh`、`.dsm`、`.dsf` | 提交 —— 这就是材质逻辑 |
| `DShader/.dreampass/` | 提交，当你使用 Custom Pass 时 —— HLSL pass 的快照 |
| `dreamshader.lock.json` | 提交，如果团队会安装包 |
| `Config/DefaultEngine.ini` | 提交 —— 项目设置存在这里并共享 |
| `DShader/Packages/` | 团队自定：提交它，或按锁定文件重新安装 |
| 生成的 `.uasset` | 两种做法都行 —— 只提交源文件并在 CI 中重新生成，或者把生成资产也提交；见[版本控制](../generation/source-control.md) |
| `Intermediate/DreamShader/` | 不提交 |

团队内统一一个 *Source Directory*，这样导入在每台机器上的解析结果才一致。构建键按项目相对路径记录源文件（自 2.1.1 起），所以另一份检出不会把每个资产都重建一遍。插件的
`DShader` 文件夹随插件一起提交 —— 它和插件的 `Content/` 属于同一份交付物。

<a id="next"></a>

## 下一步

- [文件模型](../language/source-files.md) —— 每种 1.x 扩展名可以包含什么
- [DreamShaderLang 2.0](../language-v2/index.md) —— `.dss`、`.dsi` 和 `.dsp`
- [导入与命名空间](../language/import.md) —— specifier 语法和循环处理
- [包](../tools/packages.md) —— 安装根，以及插件没有实现的那一半
- [日常工作流](workflow.md) —— 一次保存实际触发了什么
