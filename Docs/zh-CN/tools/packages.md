---
title: "Package"
description: "Package"
---

<a id="packages"></a>

# Package

> [DreamShader](../index.md) » [工具](index.md) » **Package**

`DShader/Packages` —— 可复用 DreamShaderLang 库的安装根目录，1.x 文件中的 `import` 以及 `.dss` 中的 `#include` 都能解析到这里。Package 是别人的源码：插件只通过引用它的文件来编译它，唯一例外是 package 主动要求构建的内容 —— 它的[资产](#package-assets) *（2.1.2 之后）*：用于 Material Editor 的材质函数，生成到 package 自己的资产文件夹中。每个源码根都有一个：工程的那个，以及随附[源码根](../language/source-files.md#source-roots)的插件的 `<Plugin>/DShader/Packages`。

| | |
| :-- | :-- |
| 类型 | 目录约定 |
| 路径 | `<SourceDirectory>/Packages` —— 默认 `<Project>/DShader/Packages` |
| 创建者 | `DreamShader` 运行时模块，启动时无条件创建 |
| 可配置性 | 只能间接配置，通过 *Source Directory* 项目设置 |
| 实现方 | 插件：import 根、枚举规则和 package 资产。其余一切：编辑器扩展 |

<a id="synopsis"></a>

## 概要

```text
<Project>/
└─ DShader/                              <SourceDirectory>
   ├─ Materials/                         ordinary sources — compiled, listed, synced
   │  └─ M_Sample.dsm
   ├─ dreamshader.lock.json              written by the extension
   └─ Packages/                          <SourceDirectory>/Packages
      └─ @scope/
         └─ package-name/
            ├─ dreamshader.package.json  read by the extension
            ├─ README.md
            ├─ LICENSE
            ├─ Library/
            │  └─ Noise.dsh              importable; an asset header when it asks for functions
            ├─ Assets/                   (after 2.1.2) built like the project's own sources
            │  └─ MF_Noise.dss           -> /Game/DreamShaderPackages/typedreammoon/package-name/MF_Noise
            └─ Examples/
               └─ M_NoisePreview.dsm     never compiled by the plugin
```

目录名是硬编码的：package 根永远是配置的源目录下那个字面量子目录 `Packages`。更改 *Source Directory* 会把它一起搬走；没有单独的 package 目录设置。

<a id="division-of-labour"></a>

## 职责划分

Unreal 插件关于 package 只实现三件事，其余一概不涉及。所有归在"扩展"之下的内容，都是由[编辑器扩展](workspace.md#editor-extensions)在它们各自的仓库里定义并执行的约定，从插件侧完全观察不到。

| 方面 | 实现方 |
| :-- | :-- |
| 编辑器启动时创建 `<SourceDirectory>/Packages` | **插件** |
| 把 `<SourceDirectory>/Packages` 作为第三个 `import` / `#include` 解析根 | **插件** |
| 把 package 文件排除出源文件枚举，package 资产除外 | **插件** |
| [Package 资产](#package-assets)：`Assets` 文件夹、资产头文件、package 资产根 | **插件** |
| `dreamshader.package.json` —— 存在性、字段、校验 | 扩展 |
| `dreamshader.lock.json` —— 存在性、内容、更新 | 扩展 |
| 安装、更新、移除 package | 扩展 |
| Package 商店、index 源、GitHub topic 搜索 | 扩展 |
| Package 脚手架 / "创建 package" 流程 | 扩展 |
| 带 scope 的 `@scope/name` 命名 | 扩展 |

> [!NOTE]
> 此插件中没有任何 C++ 读取 `dreamshader.package.json` 或 `dreamshader.lock.json`。一个 package 能被解析，纯粹是因为它的文件存在于 `<SourceDirectory>/Packages` 之下；删除 manifest 不会破坏 `import`，添加 manifest 也不会改变插件的行为。manifest 和锁文件是给扩展的安装器和商店用的。

<a id="import-resolution"></a>

## Import 解析

一个 `import` 说明符 —— 或者 `.dss` `#include` 的路径 —— 先被归一化，然后按顺序对三个候选根依次尝试。第一个既待在自己根内部、又在磁盘上存在的候选胜出。这个解析器由编译器的 include 解析器和编辑器的依赖扫描器共用，所以两边结论一致。*（自 2.0.0 起）* 编译器把找到的头文件单独读取，并把它的名字声明进 include 它的文件；不会把任何东西当文本贴进来。

<a id="specifier-normalization"></a>

### 说明符归一化

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 剥掉首尾空白 |
| 2 | 每个 `\` 换成 `/` |
| 3 | 反复剥掉**所有**前导 `./` 序列（`././X` 变成 `X`） |
| 4 | 结果没有扩展名时，追加 `.dsh` |

第 4 步意味着 `import "@scope/pkg/Library/Noise"` 和 `import "@scope/pkg/Library/Noise.dsh"` 是同一个 import。只有头文件能被 import *（自 2.0.0 起）*：指向 `.dsf` 或 `.dsm` 的 `import` 是 [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252)，解析到 `.dsh`（或 `.dss`）之外的 include 是 [`DSH8295`](../diagnostics/DSH8xxx.md#dsh8295)。

<a id="candidate-roots"></a>

### 候选根

| 顺序 | 候选路径 | 必须待在的根 |
| :-- | :-- | :-- |
| 1 | `<directory of the importing file>/<specifier>` | 发起 import 的文件自己的 import 根 —— 任何一个根的源目录或 packages 目录中包含它的那个（取最长匹配），都不包含时取文件自己所在目录 |
| 2 | `<owning root's source directory>/<specifier>` | 那个源目录 |
| 3 | `<owning root's source directory>/Packages/<specifier>` | 那个 packages 目录 |

所属根就是发起 import 的文件所在的源码根 —— 工程文件是 `<SourceDirectory>` —— 所以插件的源文件按插件自己的 `Packages` 解析。

因为对住在 package 里的文件来说，候选 1 的根是 `Packages`，所以 package 内的 `.dsh` 可以用相对说明符触达兄弟文件，但无法用 `../` 爬出 package 树；逃逸守卫会否掉这个候选，解析继续落到根 2 和根 3。

任何根下都解析不到的说明符是 [`DSH8292`](../diagnostics/DSH8xxx.md#dsh8292)。完整的 `import` 语法、头文件如何被读取以及环处理，见 [import](../language/import.md)。

<a id="source-file-enumeration"></a>

## 源文件枚举

根目录 `Packages` 树下的所有内容都会从每条构建路径中排除，只有两个例外 —— 即 package 主动要求构建的文件 *（2.1.2 之后）*：

| Package 文件 | 是否构建 | 其资产去哪 |
| :-- | :-- | :-- |
| package 的 `Assets` 文件夹下的 `.dss`、`.dsf` 或 `.dsi` | 是，像工程的源文件一样 | package 的资产根，加上该文件在 `Assets` 下的文件夹 |
| 要求材质函数的 `.dsh`（[资产头文件](#asset-headers)），不在 `Examples` 下 | 是，作为那些函数的包装 | package 的资产根，按每个函数要求的名字 |
| 任何其他 `.dsh` | 只通过引用它的文件 | — |
| 其他任何内容：`.dsm`、`.dsp`、`Examples` 文件、`Assets` 之外的源文件 | **从不** | — |

判定是大小写不敏感的路径前缀比较。一个 *package* 是 `Packages` 下包含某个文件的文件夹：带 scope 的 package 是 `Packages/@scope/name`，否则是 `Packages/name`；直接位于 `Packages` 下（或直接位于 `@scope` 文件夹下）的文件不属于任何 package，永远不会被构建。资产头文件通过其文本识别：它包含 `/// @generate`、`#pragma generate` 或 `Generate =`。

各特性如何处理 package 树：

| 特性 | Package `Assets` 源文件和资产头文件 | 其他所有 package 文件 |
| :-- | :-- | :-- |
| 启动时的 Ephemeral 生成 | 构建 | 排除 |
| [命令行](commandlet.md) `compile`、`dump-graph`、`check`、`dump-ir`、`index`、`list-generated` 带 `-All` | 构建 / 读取 | 排除 |
| Cook 期落盘 | 构建 | 排除 |
| *Recompile DSM* / 全量重扫队列、依赖图 | 构建 | 排除 |
| 文件 watcher，保存时 | 该文件被重新构建，以及引用它的文件 | `.dsh` 会重建引用它的文件；其他内容被忽略 |
| Material Content Browser 的源文件列表 | 列出 | 排除 |
| VirtualFunction 声明同步、资产重命名同步、`migrate -All`、`fmt -All`、`pass-registry -All` | 排除：这些会重写源文件，而 package 的文件属于其安装器 | 排除 |

> [!NOTE]
> 在 2.1.2 之前，规则不同且不一致：package 中任意位置的 `.dsf`、`.dss`、`.dsi` 或 `.dsp` 会由交互式路径（*Recompile DSM*、文件 watcher、依赖图）编译，而所有批处理路径（启动、`compile -All`、cook）都不会，因此这样的 package 会在编辑器会话中构建其资产，而不是在无头构建中。现在只有 `Assets` 下的内容会被构建，且所有路径都会构建；请把 package 的可构建源文件移到那里。

> [!WARNING]
> package 中的 `Examples/**` 永远不会被任何路径编译，也永远不会出现在 Material Content Browser 中。要使用某个示例，请把它从 `DShader/Packages` 复制到 `DShader/`（或它下面任何不是 `Packages` 的子目录）。

<a id="package-assets"></a>

## Package 资产

*（2.1.2 之后）* package 可以附带材质函数，Material Editor 会像使用其他材质函数一样使用它们：美术师把 `MF_BL_TexNoise3D` 拖进图表，没有人为此编写 DreamShader。插件会在安装该 package 的工程中，把它们构建到 package 自己的文件夹里，并在 package 变化时重新构建它们。*调用* package 函数的源文件 —— `.dss` 中的 `BL::TexNoise3D(...)`，或 1.x 的 `Graph` —— 仍然会像以前一样内联它；生成的函数是第二种途径，而不是替代品。

<a id="the-asset-root"></a>

### 资产根

package 构建的所有内容都放在其 **资产根** 下，除非源文件指定了自己的 `Root`：

| Package | 资产根 |
| :-- | :-- |
| `<Project>/DShader/Packages/@typedreammoon/dreamshader-blender` | `/Game/DreamShaderPackages/typedreammoon/dreamshader-blender` |
| `<Project>/DShader/Packages/dream-noise` | `/Game/DreamShaderPackages/dream-noise` |
| `<Plugin>/DShader/Packages/@scope/name` | `/<Plugin>/DreamShaderPackages/scope/name` |

`@` 会被丢弃，package 路径无法容纳的任何字符都会变成 `_`。`Assets` 下的源文件会在 `Assets` 下添加其文件夹（`Assets/Noise/MF_X.dss` → `<asset root>/Noise/MF_X`）；没有 `Root=` 的 1.x `Name=` 相对于资产根；显式的 `Root` 优先。package 构建的任何内容都不会落在工程自己的文件夹中，因此移除 package 不会影响工程自己的资产。

<a id="the-assets-folder"></a>

### `Assets` 文件夹

语言支持的任何源文件都可以放在 `Assets` 下，并像工程自己的源文件一样被构建 —— 导出函数的 `.dss`、带 `ShaderFunction` 的 1.x `.dsf`，甚至 `.dss` 材质或 `.dsi` 实例。它通过相对路径包含 package 的头文件：

```hlsl
// Packages/@typedreammoon/dreamshader-blender/Assets/MF_BL_TexNoise3D.dss
#include "../Library/Noise.dsh"

/// @library Blender|Noise
export void MF_BL_TexNoise3D(float3 co, float scale, float detail, float roughness,
                             float lacunarity, float distortion, float normalize01,
                             out float fac, out float3 color)
{
    BL::TexNoise3D(co, scale, detail, roughness, lacunarity, distortion, normalize01, fac, color);
}
```

```c
// Packages/@typedreammoon/dreamshader-blender/Assets/MF_BL_Checker.dsf
import "../Library/Checker.dsh";

ShaderFunction(Name = "MF_BL_Checker")
{
    Inputs  = { vec3 co; float scale = 5.0; }
    Outputs = { float fac; }
    Graph   = { BL::Checker(co, scale, fac); }
}
```

`Assets` 下的 `.dsp` 不会被构建：Custom Pass 管线不是 package 资产。

<a id="asset-headers"></a>

### 资产头文件

对一个包含五十个函数的库来说，为每个函数编写包装器很繁琐。相反，头文件可以标记它想要生成的函数，插件会自己编写包装器：

| | 单个函数 | 下方 / 块中的每个函数 |
| :-- | :-- | :-- |
| 2.0 | 函数上的 `/// @generate MF_Circle` | `#pragma generate(Functions = "MF_{Name}")` |
| 1.x | `Function(Generate = "MF_Circle") Circle(...)`，也作用于 `GraphFunction` | `Namespace(Name = "BL", Generate = "MF_BL_{Name}")` |
| Material Editor 函数库中的类别 | `/// @library Cat\|Sub` · `Library = "Cat\|Sub"` | pragma 或 `Namespace` 上的同一个键 |
| 描述 | `/// @desc ...` · `Description = "..."` | pragma 上的 `Desc = "..."` · `Namespace` 上的 `Description = "..."` |
| 让单个函数退出生成 | `/// @generate none` | `Function(Generate = "")` |

```c
// Packages/@typedreammoon/dreamshader-blender/Library/Noise.dsh — 1.x
Namespace(Name = "BL", Generate = "MF_BL_{Name}", Library = "Blender|Noise")
{
    Function TexNoise3D(in vec3 co, in float scale, ..., out float fac, out vec3 color) { ... }
    Function(Generate = "") NoiseHelper(in float x, out float y) { ... }   // not generated
}
```

```hlsl
// Packages/@scope/shapes/Library/Shapes.dsh — 2.0
/// @generate Shapes/MF_Circle
/// @library Shapes
float Circle(float2 uv, float radius = 0.5) { return step(length(uv - 0.5), radius); }

#pragma generate(Functions = "MF_{Name}", Library = "Shapes")
float Ring(float2 uv) { return Circle(uv, 0.4) - Circle(uv, 0.3); }
/// @generate none
float Helper(float x) { return x; }
```

- `{Name}` 是函数自己的名字 —— 命名空间成员则不带命名空间。它是唯一的占位符（1.x 头文件中为 [`DSH6332`](../diagnostics/DSH6xxx.md#dsh6332)，pragma 中为 [`DSH8273`](../diagnostics/DSH8xxx.md#dsh8273)）。
- 名字可以包含文件夹：`Shapes/MF_Circle` 即 `<asset root>/Shapes/MF_Circle`。
- 一个 pragma 会影响它之后的辅助函数 —— 没有 `export`、没有 `extern`、有函数体 —— 直到下一个 `#pragma generate(...)`；`#pragma generate()` 结束它。
- 材质函数为每个 `in` 参数提供一个输入（其默认值（如果有）即输入的预览值），为每个 `out` 参数提供一个输出，并在函数有返回值时提供一个返回值输出。函数上的 `/// @static` 或 `/// @param` 会沿用。
- 不能成为材质函数的是 [`DSH8274`](../diagnostics/DSH8xxx.md#dsh8274)：`export` 或 `extern` 函数、没有函数体的函数、数组参数。两个函数要求同一个资产是 [`DSH8275`](../diagnostics/DSH8xxx.md#dsh8275)。pragma 的未知键是 [`DSH8276`](../diagnostics/DSH8xxx.md#dsh8276)，1.x 函数头文件的未知键是 [`DSH6331`](../diagnostics/DSH6xxx.md#dsh6331) —— 都是警告。
- 这些注解只在 package 的头文件中有意义。在其他任何地方 —— 工程头文件、`.dsm` —— `@generate` 是 [`DSH7234`](../diagnostics/DSH7xxx.md#dsh7234)，不会生成任何东西；工程应改为编写 `export` 函数或 `ShaderFunction`。

头文件会作为插件在内存中编写的 `.dss` 来编译 —— 即头文件的 `#include`，加上每个函数一个 `export` 包装器 —— 因此之后的一切都是导出函数的普通编译。资产将 **头文件** 命名为其源文件：*Revert to Source*、构建键、cook 和文件 watcher 都会回到它。

<a id="updating-hand-edits-and-removal"></a>

### 更新、手动编辑与移除

| | 会发生什么 |
| :-- | :-- |
| package 被更新 | 更新改变的所有源文件都会按其构建键重新构建其资产，就像工程源文件一样 |
| 有人在 Material Editor 中编辑了生成的函数 | [手动编辑保护](../generation/divergence.md)会拒绝重建它，直到 *Revert to Source*；*Adopt Into Source* 被拒绝，[`DSH8133`](../diagnostics/DSH8xxx.md#dsh8133) —— package 的文件属于其安装器，更新会覆盖已采纳的内容 |
| 某个函数不再生成，或 package 被移除 | 其资产仍然保留。`list-generated -All` 会列出 package 资产根下不再被任何源文件构建的每个资产，[`DSH9111`](../diagnostics/DSH9xxx.md#dsh9111)；当没有东西使用它时再删除 |

package `.dsh` 头文件完全可以导入，但从不被扫描 `VirtualFunction` 声明，因此 package 中附带的 `VirtualFunction` 块永远不会被校验，也不会针对其 `UMaterialFunction` 资产刷新。见 [VirtualFunction 工具](virtual-function-tools.md)。

<a id="dreamshaderpackagejson--extension-convention"></a>

## `dreamshader.package.json` —— 扩展约定

放在 package 根目录下。**插件不读取它。** 下面的字段是 VSCode 扩展的安装器和商店使用的约定；扩展仓库是权威来源。

| 字段 | 类型 | 用途 |
| :-- | :-- | :-- |
| `name` | string | package 身份；普通 `name` 或带 scope 的 `@scope/name`。 |
| `version` | string | package 版本；推荐 SemVer。 |
| `displayName` | string | 商店中显示的可读名称。 |
| `description` | string | 商店中显示的简短描述。 |
| `author` | string | 作者署名。 |
| `repository` | string | Git 地址；用于更新已安装的 package。 |
| `license` | string | SPDX 标识符。 |
| `dreamshader.language` | string | 语言标识，`DreamShaderLang`。 |
| `dreamshader.version` | string | package 面向的 DreamShaderLang 版本范围。 |
| `dreamshader.entry` | string | 推荐入口头文件，用于文档和商店展示。 |
| `keywords` | string[] | 商店搜索词。 |

在仓库上打上 GitHub topic `dreamshader-package`，它就能在商店里被发现。

<a id="dreamshaderlockjson--extension-convention"></a>

## `dreamshader.lock.json` —— 扩展约定

| | |
| :-- | :-- |
| 路径 | `<SourceDirectory>/dreamshader.lock.json` |
| 写入方 | 扩展，在安装和更新时 |
| 读取方 | 扩展 |
| 记录 | 包名、版本、仓库、commit、安装路径 |

**插件既不读也不写。** 删除它对编译毫无影响；它的存在是为了让团队看清签入了哪些 package 版本。

<a id="extension-commands"></a>

## 扩展命令

VSCode 扩展提供的命令面板条目。安装和更新需要 `git`
在 `PATH` 上可用。名称是扩展的，不是插件的。

| 命令 |
| :-- |
| `DreamShaderLang: Install Package from GitHub` |
| `DreamShaderLang: Browse Package Store` |
| `DreamShaderLang: Update Installed Packages` |
| `DreamShaderLang: Remove Installed Package` |
| `DreamShaderLang: Open Packages Folder` |
| `DreamShaderLang: Add Package Store Index Source` |
| `DreamShaderLang: Remove Package Store Index Source` |
| `DreamShaderLang: Create Package Step by Step` |

安装既接受 `owner/repo` 简写，也接受完整的 `https://github.com/owner/repo` URL。

<a id="notes"></a>

## 说明

- package 目录在模块启动时就会创建，即使它是空的，即使在 commandlet 进程里也一样，
  与源目录和生成着色器目录一起创建。
- package 文件就是普通文件。没有任何东西阻止 package 里的 `.dsh` 被原地编辑；
  扩展的更新命令会把它覆盖掉。
- 因为排除是路径判定，把非 package 路径指向 package 内容的符号链接或 junction，
  会按它解析出来的真实路径处理。
- 插件从不下载任何东西。安装完全是扩展的 `git` 工作。

<a id="example"></a>

## 示例

一个安装在 `DShader/Packages/@typedreammoon/dream-noise/` 的 package，被某个项目材质使用：

```c
import "@typedreammoon/dream-noise/Library/Noise.dsh";

Shader(Name="Materials/M_Noise")
{
    Properties = {
        float Scale = 4.0 [Slider(0.1, 32)];
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        vec2  UV = UE.TexCoord(Index = 0);
        float N  = FBM(UV * Scale);
        Color = vec3(N, N, N);
    }
}
```

说明符的解析：

```text
specifier   @typedreammoon/dream-noise/Library/Noise.dsh
candidate 1 <Project>/DShader/Materials/@typedreammoon/dream-noise/Library/Noise.dsh   missing
candidate 2 <Project>/DShader/@typedreammoon/dream-noise/Library/Noise.dsh             missing
candidate 3 <Project>/DShader/Packages/@typedreammoon/dream-noise/Library/Noise.dsh    resolved
```

省掉扩展名，解析结果完全相同：

```c
import "@typedreammoon/dream-noise/Library/Noise";
```

<a id="see-also"></a>

## 另请参阅

- [import](../language/import.md) — 指令的语法、头文件如何被读取、环检测
- [源文件](../language/source-files.md) — `.dsm`、`.dsh` 和 `.dsf` 各自能包含什么
- [工作区与编辑器扩展](workspace.md) — 拥有安装器和存储的扩展
- [命令行](commandlet.md) — `compile -All`，它使用全量枚举器
- [编辑器桥接](bridge.md) — 使用材质枚举器的重编译请求
- [VirtualFunction 工具](virtual-function-tools.md) — 排除 package 头文件的同步过程
- [Material Content Browser](material-browser.md) — 源文件列表中不含 package 文件
- [项目设置](../settings/project.md) — `SourceDirectory`，package 根挂在它下面
- [路径](../parameters/path.md) — 资产路径根，与 import 根不同的解析器
