---
title: 项目结构
description: DShader/ 怎么组织、六种源文件各放什么、插件如何贡献自己的源码根、哪些目录归插件所有，以及决定什么会被编译的 Package 规则。
---

DreamShaderLang 的源文件放在项目根目录下的一个目录里 —— 默认是 `DShader`，由
*Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Paths ▸ Source Directory* 配置。相对路径相对项目
目录解析。

源目录、它的 `Packages` 子目录和生成 shader 目录都由插件在模块启动时创建，也就是编辑器第一次启动时，
无论你有没有写过任何源文件。

自 （自 1.6.0 起） 起，这不再是唯一的根：**每个带 `DShader` 目录的已启用插件都贡献一个自己的源码根** ——
见下面的[插件源码根](#插件源码根)。

<a id="六种源文件"></a>

## 六种源文件

文件夹怎么分由你决定，起作用的是扩展名。

| 扩展名 | 是什么 | 产出 |
| --- | --- | --- |
| `.dss` （自 2.0.0 起） | DreamShaderLang 2.0 编译单元：材质**或**材质函数（含 layer / blend） | 每个 `export` 一个资产 |
| `.dsi` （自 2.0.0 起） | 材质实例：父材质和覆盖项 | 一个 `UMaterialInstanceConstant` |
| `.dsp` （自 2.1.0 起） | Custom Pass 管线（UE 5.8） | 一个 `UDreamPassPipeline` |
| `.dsm` | 1.x 材质 | 它的 `Shader`，外加旁边声明的函数和层资产 |
| `.dsf` | 1.x 材质函数文件 | 它声明的函数资产 |
| `.dsh` | header：helper、`VirtualFunction` / `extern` 声明、共享常量；两种方言都能放 | 不产出 —— 只通过 import / include 它的文件进入编译 |

新源文件请写成 `.dss`；`.dsm` / `.dsf` 是 1.x 语法，照样由同一个编译器构建，所以项目可以用
[`dsc migrate`](../tools/migrate.md) 一个文件一个文件地迁移。每种文件能写什么见[文件模型](../language/source-files.md)。

<a id="目录树"></a>

## 目录树

```text
MyProject/
  DShader/
    Materials/
      M_Sample.dss
      M_Legacy.dsm
    Functions/
      F_Tint.dss
      F_Old.dsf
    Instances/
      MI_Sample_Red.dsi
    Passes/
      P_Outline.dsp
      M_OutlineComposite.dss
    Shared/
      Common.dsh
      Color.dsh
    VirtualFunctions/
      Engine.dsh
    Decompiled/
      Materials/
      Functions/
    Packages/
      @typedreammoon/
        dream-noise/
          dreamshader.package.json
          Library/
            Noise.dsh
    .dreampass/
    DreamShader.code-workspace
    dreamshader.lock.json
  Intermediate/
    DreamShader/
      GeneratedShaders/
  Plugins/
    DreamShader/
```

<a id="插件源码根"></a>

## 插件源码根

（自 1.6.0 起）

一个插件可以自带那些构建它材质的源文件，而不必把它们停在工程的树里。

```text
Plugins/
  MoonToon/
    DShader/
      Materials/
        M_Toon.dss
      Shared/
        Toon.dsh
      Packages/
    MoonToon.uplugin
```

| 行为 | 规则 |
| :-- | :-- |
| 发现 | 无需配置。开关是 *Project Settings ▸ … ▸ Paths ▸ **Scan Plugin Source Directories***（默认开） |
| 默认资产目标 | 插件根下的文件生成到**它自己插件的挂载点** —— `/MoonToon`，不是 `/Game`。1.x 文件只有缺失或全空白的 `Root` 才被默认化，所以 `Root="/"` 可以显式选回 `/Game` |
| import | **相对路径永不跨根**。一个文件只针对自己所在的根及该根的 `Packages` 解析；要用别的根里的 header，写以 `/` 开头的路径（先找自己的根、再找其他根）、复制过来，或者走 Package —— 见 [import 与命名空间](../language/import.md#刻意跨根) |
| 可写性 | **只有工程根是可写的**。VirtualFunction 同步跳过插件根下的文件，*Adopt* 对插件随附的源文件禁用：插件按作者写的样子交付定义 |
| 监视器 | 每个根注册一个监视，所以插件的源文件同样享受 *Auto Compile On Save* |
| workspace | `DreamShader.code-workspace` 每个根一个 `folders` 条目 —— 工程是 `"."`，插件是 `Plugin: <Name>` |
| Material Content Browser | **Root** 一列标出 *Project* 或所属插件，搜索框也匹配根名 —— 输入插件名就能筛出它带的全部文件 |
| Cook | 插件自己的源文件编译失败（例如 `Root` 指向引擎或商城插件，`DSH8090`）不再中止宿主项目的 cook，只记一行 `[Cook] Not this project's to build` （自 2.1.2 起）；项目自己的源文件仍然会中止 |

**警告：**

和某个既有根**重叠**的插件 `DShader` 目录会被忽略并给出警告，而不是把同一个文件交给两个所有者。
把 *Source Directory* 指向一个插件目录、或指向一个包含插件目录的位置时，就会出现这种情况。

**提示：**

根列表是缓存的，只在 *Source Directory* 或那个扫描开关变化、或在 Material Content Browser 里刷新时重建 ——
后者正是用来拾取「你刚建的 `DShader` 文件夹」或「会话中途挂载的插件」的。但 **watcher 只在启动时注册一次**，
所以会话中途才出现的根，它的 *Auto Compile On Save* 要到下次编辑器启动才开始工作。

<a id="各目录职责"></a>

## 各目录职责

| 目录 | 归属 | 内容 |
| --- | --- | --- |
| `DShader/` | 你 | 源文件根目录。下面怎么分都行，这里给的是约定而不是规则。 |
| `DShader/Materials/` | 你 | 材质源文件，`.dss` 或 `.dsm`，通常一个文件一个材质。 |
| `DShader/Functions/` | 你 | 材质函数：导出函数的 `.dss`，或 1.x 的 `.dsf`。 |
| `DShader/Instances/` | 你 | `.dsi` 材质实例。 |
| `DShader/Shared/` | 你 | 项目内部的 `.dsh` header。 |
| `DShader/VirtualFunctions/` | 编辑器 | *CreateVirtualFunction* 为已有 `UMaterialFunction` 写声明的位置。 |
| `DShader/Decompiled/` | 编辑器 | *Export .dss* / *Export .dsi* / *Export Legacy .dsm / .dsf* 的输出目录，分为 `Materials`、`Functions`、`Layers`、`LayerBlends`。 |
| `DShader/Packages/` | 扩展 | 已安装的共享库。永远是源根下那个字面量子目录 `Packages`，不能单独配置。 |
| `DShader/.dreampass/` （自 2.1.0 起） | 插件 | Custom Pass HLSL 的快照，通过预检查的才进 global shader 槽位。随源文件一起入库。 |
| `DShader/DreamShader.code-workspace` | 插件 | 每次执行 *Open Dream Shader Workspace* 都会整体重写。 |
| `DShader/dreamshader.lock.json` | 扩展 | 记录已安装 Package 的版本。插件既不读也不写。 |
| `Intermediate/DreamShader/GeneratedShaders/` | 插件 | 映射到 `/DreamShaderGenerated`。（自 2.0.0 起） 起 `Function` 的 HLSL 写进它的 Custom 节点本身，编译不再往这里写文件；这里的 `.ush` 是 1.x 构建留下的，*Clean Generated Shaders* 会删掉它们。 |

**警告：**

`DreamShader.code-workspace` 每次调用都从一个固定对象序列化而来 —— 从不读取、合并或保留原文件。手动
  加的 `launch`、`tasks`、`extensions` 或额外 `settings` 都会被销毁。把个人配置放在
  `DShader/.vscode/settings.json`，这个命令不会碰它。

<a id="命名约定"></a>

## 命名约定

| 类型 | 约定 |
| --- | --- |
| 材质 | `M_*.dss` / `M_*.dsm` |
| 材质函数 | `F_*.dss` / `F_*.dsf`；layer `L_*`，blend `LB_*` |
| 材质实例 | `MI_*.dsi` |
| Custom Pass 管线 | `.dsp`，和它的 pass 材质放在一起 |
| 共享 header | 按领域命名 —— `Texture.dsh`、`Color.dsh` |
| Package 入口 | `Library/<Name>.dsh` |

`.dss` 的资产名就是它 `export` 函数的名字，所以文件名和 `export` 名保持一致最省心。扩展名比较是大小写不敏感的，所以
`M_Water.DSM` 也是材质文件。标识符则是另一回事：插件把命名空间和函数名净化成 HLSL 与资产标识符时会替换掉非 ASCII 字符，
所以这些名字请用 ASCII。

<a id="import-与-include"></a>

## import 与 include

每个材质尽量只引入少量稳定入口：

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

一个 specifier 会依次尝试三个候选：导入方文件所在目录、**所属根**的源目录、该根的 `Packages` 目录。
第一个存在**且没有跑出自己包含根**的候选胜出 —— Package 内部的文件不能用 `../` 爬出去。没有扩展名的
specifier 会被补上 `.dsh`；只有 header 能被引入（import `.dsf` / `.dsm` 是 `DSH2252`）。解析不到是 `DSH8292`。
完整规则见 [import 与命名空间](../language/import.md)。

引入保持浅层还有一个好处：构建键覆盖每个被引入的 header，改一个 header 会让所有引入它的文件都失效。

<a id="什么会被编译什么不会"></a>

## 什么会被编译，什么不会

插件有两个枚举器，它们排除的东西不一样。

| 枚举器 | 扩展名 | 排除 | 使用者 |
| --- | --- | --- | --- |
| 全量源枚举 | `.dsm`、`.dsh`、`.dsf`、`.dss`、`.dsi`、`.dsp` | 任意根的 `Packages` 下的**全部**文件 | 启动时的 Ephemeral 生成、commandlet 的 `compile -All`、cook、Material Content Browser 列表、VirtualFunction 同步 |
| 材质源枚举 | `.dsm`、`.dsf`、`.dss`、`.dsi`、`.dsp` | 仅 `Packages` 下的 `.dsm` | *Recompile DSM*、自动编译队列、依赖图 |

**警告：**

`Packages` 排除对 `.dsm` 是完整的，对其他会构建东西的文件只是部分的。Package 里的 `.dsf`、`.dss`、`.dsi` 或 `.dsp` 在交互式
  编辑器会话中**会**被编译 —— 文件监视器和 *Recompile DSM* 都会拾取它 —— 但 `compile -All`、cook 和 Material Content Browser
  **不会**。也就是说，这样的 Package 在本地能用，在 headless 构建里会静默地什么都不生成。库代码请以 `.dsh` header 形式分发，
  或者把源文件从 `Packages` 复制到自己的源码树里。见 [Package ▸ 源文件枚举](../tools/packages.md#源文件枚举)。

**警告：**

Package 内的 `Examples/**/*.dsm` 在任何路径下都不会被编译，也不会出现在 Material Content Browser 里。要用示例，把它从
  `DShader/Packages` 复制到 `DShader/` 下。

Package 的 `.dsh` header 完全可以被引入，但从不参与 `VirtualFunction` 声明扫描 —— Package 里带的
`VirtualFunction` 永远不会与它的资产做校验。

<a id="版本管理"></a>

## 版本管理

| 内容 | 是否提交 |
| --- | --- |
| 每个源文件 —— `.dss`、`.dsi`、`.dsp`、`.dsh`、`.dsm`、`.dsf` | 提交 —— 这就是材质逻辑本身 |
| `DShader/.dreampass/` | 用 Custom Pass 时提交 —— HLSL pass 的快照 |
| `dreamshader.lock.json` | 如果团队会安装 Package，提交 |
| `Config/DefaultEngine.ini` | 提交 —— 项目设置存在这里，是共享的 |
| `DShader/Packages/` | 团队自定：直接入库，或按 lock 文件重新安装 |
| 生成的 `.uasset` | 两种做法都行 —— 只提交源码、在 CI 里重新生成，或者把生成资产也提交；见[版本控制](../generation/source-control.md) |
| `Intermediate/DreamShader/` | 不提交 |

团队内统一 *Source Directory*，这样引入在每台机器上的解析结果才一致。构建键按项目相对路径记录源文件 （自 2.1.1 起），
所以另一份检出不会把每个资产都重建一遍。插件带的 `DShader` 目录随插件一起提交 —— 它和插件的 `Content/` 属于同一份交付物。

<a id="下一步"></a>

## 下一步

- [文件模型](../language/source-files.md) —— 每种 1.x 扩展名可以写什么
- [DreamShaderLang 2.0](../language-v2/index.md) —— `.dss`、`.dsi` 和 `.dsp`
- [import 与命名空间](../language/import.md) —— specifier 语法和循环处理
- [Package](../tools/packages.md) —— 安装根目录，以及插件没有实现的那一半
- [日常工作流](workflow.md) —— 一次保存到底触发了什么
