---
title: 文件模型
description: 三种 1.x 源文件、各自允许声明的顶层块、这条规则如何执行，以及一个文件和它导入的 header 如何一起编译。
---

DreamShaderLang 1.x 的源文件分三类。三者由同一个词法器读成 token，再由 **legacy 前端**读成和 `.dss` 一样
的语法树，交给 2.0 编译器构建 （自 2.0.0 起）。扩展名决定两件事：这个文件允许声明什么，以及编译它会
不会产出资产。

| 扩展名 | 名称 | 作用 |
| :-- | :-- | :-- |
| `.dsm` | Dream Shader Material | 材质入口。每个文件最多一个 `Shader` 块，旁边还可以放函数块和层块。 |
| `.dsf` | Dream Shader Function （自 1.3.5 起） | 材质函数与材质层的资产入口。 |
| `.dsh` | Dream Shader Header | 只放共享声明，可以同时放 2.0 声明。自身不产出任何资产，通过 `import` 被使用。 |

扩展名按大小写不敏感比较，所以 `M_Water.DSM` 也是材质文件。

**提示：**

新源码用 [DreamShaderLang 2.0](../language-v2/index.md)（`.dss`）写。1.x 源码继续由同一个编译器构建，
[`dsc migrate`](../tools/migrate.md) 可以把一个 1.x 文件改写成 `.dss`，并证明改写后构建出同一张图。

<a id="语法概览"></a>

## 语法概览

```c
// <name>.dsm — Dream Shader Material
[ import "<header>.dsh" ; ]…
[ Shader( Name = "…" [, Root = "…"] ) { … } ]          // 每个文件最多一个
[ <any function, layer, virtual-function or namespace block> ]…
```

```c
// <name>.dsf — Dream Shader Function
[ import "<header>.dsh" ; ]…
[ { ShaderFunction | ShaderLayer | ShaderLayerBlend }( Name = "…" [, Root = "…"] ) { … } ]…
[ { VirtualFunction | Namespace | Function | GraphFunction } … ]…
```

```c
// <name>.dsh — Dream Shader Header
[ import "<header>.dsh" ; ]…
[ { VirtualFunction | Namespace | Function | GraphFunction } … ]…     // 可与 2.0 声明并存
```

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

<a id="各类文件允许包含什么"></a>

## 各类文件允许包含什么

| 顶层块 | `.dsm` | `.dsf` | `.dsh` |
| :-- | :-: | :-: | :-: |
| `Shader` | 一个 | 不允许 —— `DSH2259` （自 2.1.1 起） | 不允许 |
| `ShaderFunction` | 允许 | 允许 | 不允许 |
| `ShaderLayer` | 允许 | 允许 | 不允许 |
| `ShaderLayerBlend` | 允许 | 允许 | 不允许 |
| `MaterialLayer`（已弃用） | 允许 | 允许 | 不允许 |
| `MaterialLayerBlend`（已弃用） | 允许 | 允许 | 不允许 |
| `VirtualFunction` | 允许 | 允许 | 允许 |
| `Namespace` | 允许 | 允许 | 允许 |
| `Function` | 允许 | 允许 | 允许 |
| `GraphFunction` | 允许 | 允许 | 允许 |
| `import` 一个 `.dsh` | 允许 | 允许 | 允许 |
| 2.0 声明（`uniform`、`export`、`#pragma`…） | 不允许 | 不允许 | 允许 |

一个 `.dsm` 会构建它的 `Shader` 以及旁边的每个 `ShaderFunction` 和层块，各自成为一个资产，和 1.x 一样；
材质可以按名字调用这些函数，函数资产会先构建 （自 2.1.1 起）（2.0.0 – 2.1.0 以 `DSH6201` 拒绝这种组合）。
`.dss` 只能产出材质**或**函数资产，不能两者都有，所以 [`dsc migrate`](../tools/migrate.md) 改写这种
文件之前，要先把每个函数挪进它自己的 `.dsf`。每个块具体声明什么见 [顶层块](blocks.md)。

<a id="这条限制是怎么执行的"></a>

## 这条限制是怎么执行的

由解析器逐个声明判断，再由绑定器在一个文件的全部声明上检查 （自 2.0.0 起）。

| 规则 | 由谁检查 | 诊断 |
| :-- | :-- | :-- |
| `.dsh` 不放资产块（`Shader`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend` 或层的旧拼法） | legacy 前端，在块关键字处 | `DSH2249` |
| `.dsm` / `.dsf` 只放 1.x 块 —— `uniform`、`export`、`#pragma` 是 2.0 语法 | legacy 前端 | `DSH2248` |
| `.dss` 只放 2.0 声明 —— 那里出现 1.x 块关键字 | 2.0 解析器 | `DSH3222` |
| 每个文件一个 `Shader` 块 | legacy 前端 | `DSH2250` |
| `.dsf` 不放 `Shader` 块 （自 2.1.1 起） | legacy 前端，在块关键字处 | `DSH2259` |
| `.dsm` / `.dsf` 至少声明一个块 | legacy 前端 | `DSH2254` |
| `import` 指向一个 `.dsh` | legacy 前端 | `DSH2252` |

`.dsh` 可以让 1.x 块和 2.0 声明并存，所以一个项目可以一次迁移一个文件。注释或字符串里提到 `Shader(`
只是注释或字符串。

**提示：**

**1.9.x 及以前**这条规则是一次忽略大小写的**子串扫描**：在文件去掉 `import` 行之后的文本里找 `Shader(`、
`ShaderFunction(` …… —— 注释和字符串里的也算，而括号前多一个空格的 `Shader (` 又能漏过去。现在不再扫描
文本，前端读的是块关键字本身，所以 `.dsf` 里的 `Shader` 块不管怎么写都是 `DSH2259`（2.0.0 – 2.1.0 不拒绝
它，那个文件会像 `.dsm` 一样构建出材质）。

<a id="一个文件和它导入的-header"></a>

## 一个文件和它导入的 header

编译的单位是**一个源文件**。它导入的 `.dsh` 各自独立地预处理、解析，再把声明登记进
这个文件 （自 2.0.0 起），而不是先把文本拼成一段。

- **每个文件最多一个 `Shader` 块**（`DSH2250`）。header 里根本不能有 `Shader`（`DSH2249`），所以 import 永远
  不会带进第二个。1.9.x 把整个 import 闭包内联成一段文本，这条限制因此是闭包范围的；现在不是了。
- **只有 `.dsh` 能被导入** （自 2.0.0 起）。导入 `.dsf` 或 `.dsm` 是 `DSH2252`：材质文件和函数文件各自
  单独编译，其他文件通过 [`VirtualFunction`](blocks.md#virtualfunction) 使用它们产出的资产。
- 名字是整个文件范围的：header 里声明的名字和本文件、或两个 header 之间重名，就是 `DSH4210`。见
  [import 与命名空间](import.md)。

<a id="各类文件产出什么"></a>

## 各类文件产出什么

| 类型 | 编译身份 | 产出 |
| :-- | :-- | :-- |
| `.dsm` | 材质入口 | 其 `Shader` 块对应的 `UMaterial`（或 ThinCustom 实例），外加它旁边声明的每个函数和层资产 |
| `.dsf` | 资产入口 | 它声明的函数资产 |
| `.dsh` | 不是入口 | 没有产出 —— header 只通过导入它的源文件进入编译；直接编译一个 `.dsh` 是 `DSH8296` |

没有 `Shader` 块的 `.dsm` 仍然可以编译，它会产出自己声明的那些函数资产。一次编译只构建本文件声明的东西，
从不构建它导入的 header 里的东西 —— header 里没有资产块。

<a id="文件是怎么被找到的"></a>

## 文件是怎么被找到的

发现过程扫描的是一组**源码根**，而不是单个目录 （自 1.6.0 起）。

| 根 | 目录 | 可写 | 什么时候加入 |
| :-- | :-- | :-- | :-- |
| 项目 | *Source Directory*，默认 `<Project>/DShader` | 是 | 总是 |
| 每个插件一个 | `<Plugin>/DShader` | 否 | 插件已启用、目录存在，且打开了 *Scan Plugin Source Directories* |

每个根有自己的 `<Root>/Packages`。根是 import 解析的单位：一个 specifier 只在导入方所属的那个根里解析，见
[import 与命名空间](import.md#源码根)。编辑器只会改写项目根下的文件；插件根照常发现、解析和编译，
但从不被改写。插件根下没写 `Root=` 的文件，默认生成到**该插件的挂载点**而不是 `/Game`；写 `Root="/"` 可以
回到 `/Game`。见 [资产路径](../generation/asset-paths.md)。

| 扫描 | 扩展名 | 排除 | 用途 |
| :-- | :-- | :-- | :-- |
| 全部源文件 | `.dsm`、`.dsf`、`.dsh`；2.0 线上还有 `.dss`、`.dsi` 与 `.dsp` （自 2.1.0 起） | 任何根的 packages 目录下的一切 | 依赖图、workspace 生成、编辑器文件列表 |
| 可生成文件 | `.dsm`、`.dsf`；2.0 线上还有 `.dss`、`.dsi`、`.dsp` —— 永远不含 `.dsh` | 任何根的 packages 目录下的 `.dsm` 文件 | 批量编译 / 全部生成 |

**提示：**

这两条排除规则并不对称。全量扫描会丢掉 packages 目录下的**每一个**文件；可生成扫描只丢掉 package 里的
**`.dsm`**。所以放在 `DShader/Packages` 里的 `.dsf` 仍然会被当作可生成文件，全部生成时会产出函数资产 ——
2.0 线上的 `.dss`、`.dsi`、`.dsp` 也一样。

| 目录 | 默认值 | 项目设置 |
| :-- | :-- | :-- |
| 源目录 | `<Project>/DShader` | *Source Directory* |
| Packages | `<Source>/Packages` | 由 *Source Directory* 推导，不能单独配置 |
| 插件源目录 | `<Plugin>/DShader` | *Scan Plugin Source Directories*（只有开关，路径固定） |
| 生成的 shader | `<Project>/Intermediate/DreamShader/GeneratedShaders` | *Generated Shader Directory* |

配置成相对路径时以项目目录为基准解析。项目的源目录、packages 目录和生成 shader 目录会在模块启动时创建；
插件的 `DShader` 目录从不创建，只会被发现。见 [项目设置](../settings/project.md) 和
[Package](../tools/packages.md)。

<a id="示例"></a>

## 示例

```text
<Project>/DShader/
├── Materials/
│   └── M_Water.dsm
├── Functions/
│   └── F_Tint.dsf
├── Shared/
│   └── Common.dsh
└── Packages/
    └── @typedreammoon/
        └── dream-noise/
            └── Library/
                └── Noise.dsh
```

```c
// DShader/Shared/Common.dsh — header：只放 helper
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

```c
// DShader/Functions/F_Tint.dsf — 函数文件：声明 ShaderFunction 块
import "Shared/Common.dsh";

ShaderFunction(Name="Functions/F_Tint")
{
    Inputs = {
        vec3 InColor;
        opt float Strength = 1.0;
    }

    Outputs = {
        vec3 OutColor;
    }

    Graph = {
        OutColor = Common::ApplyTint(InColor, vec3(1.0, 0.9, 0.8)) * Strength;
    }
}
```

```c
// DShader/Materials/M_Water.dsm — 材质文件：一个 Shader 块
import "Shared/Common.dsh";

Shader(Name="Materials/M_Water")
{
    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = Common::ApplyTint(vec3(0.1, 0.3, 0.6), vec3(1.0, 0.9, 0.8));
    }
}
```

每个源文件构建自己的资产；header 什么都不构建，被两者读取：

```text
M_Water.dsm   ->  /Game/Materials/M_Water        UMaterial
F_Tint.dsf    ->  /Game/Functions/F_Tint         UMaterialFunction
Common.dsh    ->  （无）
```

<a id="说明"></a>

## 说明

- `.dsh` 是放 `VirtualFunction` 声明的自然位置。编辑器的 *Create Virtual Function* 动作就会在
  `DShader/VirtualFunctions` 下写一个。
- 扩展名还决定无后缀 import specifier 的默认扩展名：没有扩展名时补 `.dsh`。
- `.dsm` 或 `.dsf` 必须至少有一个块（`DSH2254`）。空的 `Namespace` 也算一个块，`.dsh` 可以只有 import
  （自 2.0.0 起）。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH2249: Expected only Function, GraphFunction, Namespace and VirtualFunction blocks in a '.dsh' header, found the asset block '{0}', which belongs in a .dsm or .dsf file.` | 一个 .dsh 里有构建资产的块（Shader、ShaderFunction、层块）。header 会被导入到别的文件里，资产块放在这里会被每个导入方各构建一次。 | 把这个块挪进它自己的 .dsm / .dsf，header 里只留函数和 VirtualFunction。 |
| `DSH2248: Expected a 1.x block in a '.{0}' file, found {1}, which is 2.0 syntax; 2.0 declarations belong in a .dss file or a .dsh header.` | .dsm 或 .dsf 里出现了 2.0 语法（uniform、export 函数、#pragma），而这两种文件只由 1.x 前端读取。 | 把 2.0 声明放进 .dss，或放进两种方言都接受的 .dsh；dsc migrate 可以改写整个 1.x 文件。 [详解](../tools/migrate.md) |
| `DSH3222: Expected a 2.0 declaration, found the 1.x declaration '{0}'; 1.x declarations belong in a .dsh header or in a .dsm or .dsf file.` | 一个 .dss 里出现了 1.x 块关键字。扩展名决定前端，.dss 选的是 2.0 前端。 | 把 1.x 源码留在 .dsm / .dsf / .dsh，或把这条声明改写成 2.0。 [详解](../language-v2/index.md) |
| `DSH2250: Expected one Shader block in a file, found a second one.` | 一个文件里有两个 Shader 块。一个材质源文件就是一个材质。 | 每个材质各用一个文件。 |
| `DSH2259: Expected ShaderFunction, ShaderLayer and ShaderLayerBlend blocks in a '.dsf' file, found a Shader block, which belongs in a .dsm file.` | 一个 .dsf 里有 Shader 块。.dsf 构建函数资产，材质属于 .dsm；1.x 生成时也拒绝它。这个块被跳过，文件其余部分照常读取。 | 把 Shader 块挪进 .dsm；.dsm 可以把 ShaderFunction 和层块留在 Shader 旁边，所以把整个文件改成 .dsm 也行。 |
| `DSH2254: Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace or VirtualFunction block in this 1.x file, found none.` | 文件里一个块都没有：是空的、全部被注释掉，或被 #if 裁掉了。 | 至少声明一个块；如果是遗留文件就删掉。 [详解](blocks.md) |
| `DSH2252: Expected a double-quoted path after 'import', found {0}.` | 前端不读的 import：import 后面不是带引号的路径；路径指向 .dsf 或 .dsm；或者是 1.6.0 的根限定写法（Project:…、Plugin.X:…）。 | 写 import "Shared/Common.dsh";，路径相对本文件或它所属的源码根；够另一个根里的 header 用以 / 开头的路径。 [详解](import.md) |
| `DSH8296: '{0}' is not a source the compiler builds on its own; it builds '.dss', '.dsi', '.dsp', '.dsm' and '.dsf' files, and a '.dsh' header only through the source that includes it.` | 编译器收到一个它不单独构建的文件：一个 .dsh，或者扩展名不是这五种之一的文件。 | 编译导入这个 header 的源文件。 |

`import` 无法解析、无法读取、解析出错以及循环的诊断见 [import 与命名空间](import.md#诊断)。

<a id="继续阅读"></a>

## 继续阅读

- [词法与大小写](lexical.md) — Token、大小写敏感性矩阵、字面量、语句切分与预处理器。
  - [顶层块](blocks.md) — 源文件可以声明的七种块，以及各自产出什么。
  - [import 与命名空间](import.md) — header 怎么被找到、读取，并把声明交给导入它的文件。
  - [Package](../tools/packages.md) — DShader/Packages 布局与 @scope/name 约定。
