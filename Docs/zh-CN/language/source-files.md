---
title: "源文件"
description: "源文件"
---

<a id="source-files"></a>

# 源文件

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **源文件**

DreamShaderLang 的三种文件类型 —— `.dsm`、`.dsf` 和 `.dsh` —— 以及决定每种文件可包含哪些顶层块的规则。

| | |
| :-- | :-- |
| 声明于 | — |
| 类型 | 翻译单元 |
| 扩展名 | `.dsm` 材质 · `.dsf` 函数 *(自 1.3.5 起)* · `.dsh` header |
| 发现于 | 默认 `<Project>/DShader`，外加每个已启用插件的 `DShader`；可配置 |
| 由谁执行 | legacy 前端（`.dsh` 可以包含什么，每个文件一个 `Shader`）与绑定器（每个文件一种产物） *(自 2.0.0 起)* |

<a id="synopsis"></a>

## 语法概览

```c
// <name>.dsm — Dream Shader Material
[ import "<header>.dsh" ; ]…
[ Shader( Name = "…" [, Root = "…"] ) { … } ]          // at most one per file
[ { VirtualFunction | Namespace | Function | GraphFunction } … ]…
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
[ { VirtualFunction | Namespace | Function | GraphFunction } … ]…     // and 2.0 declarations, side by side
```

<a id="what-each-kind-may-contain"></a>

## 各类文件允许包含什么

| 顶层块 | `.dsm` | `.dsf` | `.dsh` | 参考 |
| :-- | :-- | :-- | :-- | :-- |
| `Shader` | 一个 | 不允许 —— [`DSH2259`](../diagnostics/DSH2xxx.md#dsh2259) | 不允许 | [Shader](shader.md) |
| `ShaderFunction` | 允许 | 允许 | 不允许 | [ShaderFunction](shader-function.md) |
| `ShaderLayer` | 允许 | 允许 | 不允许 | [ShaderLayer](shader-layer.md) |
| `ShaderLayerBlend` | 允许 | 允许 | 不允许 | [ShaderLayerBlend](shader-layer.md) |
| `MaterialLayer` *（1.3.0 中已弃用）* | 允许 | 允许 | 不允许 | [ShaderLayer](shader-layer.md) |
| `MaterialLayerBlend` *（1.3.0 中已弃用）* | 允许 | 允许 | 不允许 | [ShaderLayer](shader-layer.md) |
| `VirtualFunction` | 允许 | 允许 | 允许 | [VirtualFunction](virtual-function.md) |
| `Namespace` | 允许 | 允许 | 允许 | [Namespace](namespace.md) |
| `Function` | 允许 | 允许 | 允许 | [Function](function.md) |
| `GraphFunction` | 允许 | 允许 | 允许 | [GraphFunction](graph-function.md) |
| `import` 一个 `.dsh` | 允许 | 允许 | 允许 | [`import`](import.md) |
| 2.0 声明（`uniform`、`export`、`#pragma`…） | 不允许 | 不允许 | 允许 | [DreamShaderLang 2.0](../language-v2/index.md) |

一个 `.dsm` 会构建它的 `Shader` 以及旁边的每个 `ShaderFunction` 或层块，各自成为一个资产，和 1.x 一样；材质可以按名字调用这样的函数 *（2.0.0 – 2.1.0 以 [`DSH6201`](../diagnostics/DSH6xxx.md#dsh6201) 拒绝这种写法）*。
一个 `.dss` 产出材质或函数资产，不能两者都有（[`DSH6201`](../diagnostics/DSH6xxx.md#dsh6201)），所以 [`dsc migrate`](../tools/migrate.md) 无法
把这样的文件改写成一个 `.dss`：先给每个函数各自的 `.dsf`。

<a id="how-the-restriction-is-enforced"></a>

## 这条限制是怎么执行的

*（自 2.0.0 起）* 由解析器逐个声明判断，再由绑定器在一个文件的全部声明上检查。

| 规则 | 由谁检查 | 代码 |
| :-- | :-- | :-- |
| 一个 `.dsh` 不放资产块（`Shader`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend` 或层的旧拼法） | legacy 前端，在块关键字处 | [`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249) |
| 一个 `.dsm` / `.dsf` 只放 1.x 块 —— `uniform`、`export`、`#pragma` 是 2.0 语法 | legacy 前端 | [`DSH2248`](../diagnostics/DSH2xxx.md#dsh2248) |
| 一个 `.dss` 只放 2.0 声明 —— 那里出现 1.x 块关键字 | 2.0 解析器 | [`DSH3222`](../diagnostics/DSH3xxx.md#dsh3222) |
| 每个文件一个 `Shader` 块 | legacy 前端 | [`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250) |
| 一个 `.dsf` 不放 `Shader` 块 | legacy 前端，在块关键字处 | [`DSH2259`](../diagnostics/DSH2xxx.md#dsh2259) |
| 一个 `.dsm` / `.dsf` 至少声明一个块 | legacy 前端 | [`DSH2254`](../diagnostics/DSH2xxx.md#dsh2254) |
| 一个 `import` 指向一个 `.dsh` | legacy 前端 | [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252) |

一个 `.dsh` 可以让 1.x 块和 2.0 声明并存，所以一个项目可以一次迁移一个文件。注释或字符串里提到 `Shader(` 只是注释或字符串。

> [!NOTE]
> **1.9.x 及以前**这条规则是一次忽略大小写的**子串扫描**，在文件去掉
> `import` 行之后的文本里进行：一个 `.dsh` 会因为包含 `Shader(`、`ShaderFunction(`…… 而被拒绝 ——
> 注释或字符串里的也算 —— 一个 `.dsf` 会因为包含 `Shader(` 而被拒绝，而带一个空格的 `Shader (`
> 又能漏过去。现在不再扫描文本：前端读的是块关键字，所以一个 `.dsf` 带着
> `Shader` 块就是 `DSH2259`，不管怎么写 *（2.0.0 – 2.1.0 不拒绝它，那个文件
> 会像 `.dsm` 一样构建出材质）*。见 [已停用的解析器页面](../api/parser.md)。

<a id="source-roots"></a>

## 源码根

发现过程扫描的是一组**源码根**，而不是单个目录。

| 根 | 目录 | 可写 | 什么时候加入 |
| :-- | :-- | :-- | :-- |
| 项目 | *Source Directory*，默认 `<Project>/DShader` | 是 | 总是 |
| *（每个插件一个）* | `<Plugin>/DShader` | 否 | 插件已启用、目录存在，且打开了 *Scan Plugin Source Directories* |

每个根有自己的 `Packages` 文件夹，位于 `<Root>/Packages`。根是 import
解析的单位：一个 import specifier 在导入方所属的那个根的基准路径里解析，永远不在另一个根的基准路径里解析 —— 见 [`import`](import.md#roots)。

只有项目根是**可写**的。插件根和其他源码一样被发现、解析和编译，
但编辑器从不改写它下面的文件：[VirtualFunction
同步](../../tools/virtual-function-tools.md) 会跳过它们，而每一个写源文件的编辑器操作都以项目根为目标。

> [!NOTE]
> 一个插件的 `DShader` 文件夹如果和已有的根重叠，会被忽略，并给出同时点名两者的警告。
> 当 *Source Directory* 指向一个插件文件夹，或指向一个包含插件文件夹的目录时就会发生这种情况；
> 没有这项检查，同一个文件就会属于两个根，它的 import 会按扫描顺序解析。

源码目录监视器为每个根注册一个监视，所以 *Auto Compile On Save* 对插件源码的触发
和对项目源码完全一样。

> [!NOTE]
> 根列表是缓存的。编辑器打开期间创建的 `DShader` 文件夹，会在
> [材质内容浏览器](../../tools/material-browser.md) 里按下 **Refresh**（`F5`）时出现；它的
> **watch** 只在启动时注册一次，会在下次编辑器启动时挂上，那个根的 *Auto
> Compile On Save* 从那时起才开始工作。

插件根下没声明 `Root=` 的文件，默认指向**该插件的挂载点**
而不是 `/Game` —— `Plugins/MoonToon/DShader` 里的一个 `.dsm` 会在完全不写属性的情况下生成到 `/MoonToon`。写 `Root="/"` 可以
回到 `/Game`。见
[资产路径](../generation/asset-paths.md#the-plugin-source-root-default)。

<a id="file-discovery"></a>

## 文件发现

文件通过对每个源码根做递归扫描来找到。不返回目录，每个
结果都规范化为完整路径。

| 扫描 | 扩展名 | 排除 | 用途 |
| :-- | :-- | :-- | :-- |
| 全部源文件 | `.dsm`、`.dsf`、`.dsh`；2.0 线上还有 `.dss`、`.dsi` 以及 *(自 2.1.0 起)* `.dsp` | 任何根的 packages 目录下的一切 | 依赖图、workspace 生成、编辑器文件列表（结果已排序） |
| 可生成文件 | `.dsm`、`.dsf`；2.0 线上还有 `.dss`、`.dsi` 以及 *(自 2.1.0 起)* `.dsp` —— 永远不含 `.dsh` | 任何根的 packages 目录下的 `.dsm` 文件 | 批量编译 / 全部生成 |

> [!NOTE]
> 这两条排除规则并不对称。全量源文件扫描会丢掉 packages
> 目录下的**每一个**文件；可生成扫描只丢掉 package 里的 **`.dsm`** 文件。因此放在
> `DShader/Packages` 里的 `.dsf` 仍然会被当作可生成文件，全部生成时会产出函数
> 资产 —— 2.0 线上 `.dss`、`.dsi` 或 `.dsp` 也一样。

*(自 2.0.0 起)* 用 `-All` 指定文件的命令行动词使用各自的扫描：

| `-All` | 扩展名 | 排除 |
| :-- | :-- | :-- |
| `compile` | 不含 `.dsh` 的全量源文件扫描，每个 `.dsf` 优先 | 任何根的 packages 目录下的一切 |
| `check`、`dump-ir`、`dump-layout`、`index`、`list-generated` | `.dss`、`.dsi`、`.dsp`、`.dsm`、`.dsf` | 任何根的 packages 目录下的一切 |
| `fmt` | `.dss`、`.dsi`、`.dsp`、`.dsh` | 任何根的 packages 目录下的一切，以及所有不可写的根 —— 插件的源码只有在被指名时才会格式化 |

2.0 的种类见 [DreamShaderLang 2.0](../language-v2/index.md)：
[`.dsi`](../language-v2/instances.md) 材质实例与 *(UE 5.8)*
[`.dsp`](../language-v2/passes.md) Custom Pass 管线。

| 目录 | 默认值 | 项目设置 |
| :-- | :-- | :-- |
| 源目录 | `<Project>/DShader` | *Source Directory* |
| Packages | `<Source>/Packages` | 由 *Source Directory* 推导；不能单独配置 |
| 插件源目录 | `<Plugin>/DShader` | *Scan Plugin Source Directories*（只有开关，路径固定） |
| 插件 packages | `<Plugin>/DShader/Packages` | 推导得出；不能单独配置 |
| 生成的 shader | `<Project>/Intermediate/DreamShader/GeneratedShaders` | *Generated Shader Directory* |

配置成相对路径时以项目目录为基准解析。项目的源目录、packages 目录和生成 shader 目录会在模块启动时创建；
插件的 `DShader` 文件夹从不创建，只会被发现。见 [项目设置](../settings/project.md) 和
[Packages](../tools/packages.md)。

<a id="what-each-kind-generates"></a>

## 各类文件产出什么

| 类型 | 编译身份 | 产出 |
| :-- | :-- | :-- |
| `.dsm` | 材质入口 | 其 `Shader` 块的 `UMaterial`（或 ThinCustom 实例） |
| `.dsf` | 资产入口 | 它声明的函数资产 |
| `.dsh` | 不是入口 | 没有产出；header 通过指名它的文件的 [`import`](import.md) 被读取，直接编译它是 [`DSH8296`](../diagnostics/DSH8xxx.md#dsh8296) |

没有 `Shader` 块的 `.dsm` 仍然可以编译 —— 它会产出自己声明的那些函数资产。
一次编译只构建本文件声明的东西，从不构建它导入的 header 里的东西：header 里没有资产块。

<a id="notes"></a>

## 说明

- **扩展名按大小写不敏感比较。** `M_Water.DSM` 是材质文件。
- **每个文件最多一个 `Shader`**（[`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250)）。到 1.9.x
  为止，import 会在解析前内联成一段文本，因此这条限制是覆盖整个 import 闭包的；
  自 2.0.0 起，header 被单独读取，根本不能放 `Shader`，所以 import 永远不会带入一个。
- `.dsh` 是放 `VirtualFunction` 声明的自然位置；编辑器的 *Create Virtual
  Function* 动作会在 `DShader/VirtualFunctions` 下写一个。见
  [VirtualFunction 工具](../../tools/virtual-function-tools.md)。
- 只有 `.dsh` 可以被导入 *(自 2.0.0 起)*。导入 `.dsf` 或 `.dsm` 的
  `import` 是 [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252)：材质文件或函数文件各自
  单独编译，其他文件通过 [`VirtualFunction`](virtual-function.md) 使用它们产出的资产。

<a id="diagnostics"></a>

## 诊断

| 代码 | 引发条件 |
| :-- | :-- |
| [`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249) | `.dsh` 里有资产块 |
| [`DSH2248`](../diagnostics/DSH2xxx.md#dsh2248) | `.dsm` / `.dsf` 里有 2.0 语法 |
| [`DSH3222`](../diagnostics/DSH3xxx.md#dsh3222) | `.dss` 里有 1.x 块 |
| [`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250) | 一个文件里出现第二个 `Shader` 块 |
| [`DSH2259`](../diagnostics/DSH2xxx.md#dsh2259) | `.dsf` 里有 `Shader` 块 |
| [`DSH2254`](../diagnostics/DSH2xxx.md#dsh2254) | `.dsm` / `.dsf` 一个块都没声明 |
| [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252) | include 解析器不读取的 `import` —— 见 [`import`](import.md#diagnostics) |
| [`DSH8296`](../diagnostics/DSH8xxx.md#dsh8296) | 请求编译的不是编译器构建的源文件 —— 一个 `.dsh`，或其他扩展名 |

无法解析、读取或解析出错的 `import`，以及循环的诊断，见
[`import`](import.md#diagnostics)。所有代码：[诊断](../diagnostics/README.md)。

<a id="example"></a>

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
// DShader/Shared/Common.dsh — header: helpers only
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

```c
// DShader/Functions/F_Tint.dsf — function file: declares ShaderFunction blocks
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
// DShader/Materials/M_Water.dsm — material file: one Shader block
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
Common.dsh    ->  (nothing)
```

<a id="see-also"></a>

## 继续阅读

- [`import`](import.md) —— header 怎么被找到、读取，并把声明交给导入它的文件
- [Shader](shader.md) —— 仅限 `.dsm` 的顶层块
- [ShaderFunction](shader-function.md) —— `.dsf` 通常包含的块
- [VirtualFunction](virtual-function.md) —— 从 `.dsh` 声明已有资产
- [词法元素](lexical.md) —— 注释与字符串
- [Packages](../tools/packages.md) —— `DShader/Packages` 与 `@scope/name` 布局
- [项目设置](../settings/project.md) —— *Source Directory* 与 *Generated Shader Directory*
- [Commandlet](../tools/commandlet.md) —— 无界面地编译文件
- [测试](../contributing/testing.md) —— `Tests/Corpus` fixture 格式
