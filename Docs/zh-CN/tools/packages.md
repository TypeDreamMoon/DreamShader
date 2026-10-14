---
title: Package
description: DShader/Packages —— 插件实现的 import 根、扩展实现的 manifest 与锁文件，以及两者之间那道枚举缺口。
---

Package 就是一个可复用的 DreamShaderLang 库：`.dsh` 头文件、`.dsf` 函数文件、示例、README。
Package 安装在 `DShader/Packages` 下，1.x 文件通过 `import`、`.dss` 通过 `#include` 触达 （自 2.0.0 起）。
每个源码根都有自己的一个：工程的那个，以及带源码根的插件的 `<Plugin>/DShader/Packages`。

| 项目 | 值 |
| :-- | :-- |
| 类型 | 目录约定 |
| 路径 | `<SourceDirectory>/Packages` —— 默认 `<Project>/DShader/Packages` |
| 创建者 | `DreamShader` 运行时模块，启动时无条件创建 |
| 可配置性 | 只能间接配置，通过 *Source Directory* 项目设置 |

**警告：**

**关于 package，插件只实现了两件事**：import 根，以及把 package 文件排除出源文件枚举。
manifest、锁文件、商店和安装命令，都是**编辑器扩展**实现的约定 —— 插件里没有任何 C++ 读这两个 JSON 文件。
排查 package 问题之前先读[职责划分](#职责划分)，因为它决定了你该去看哪个仓库。

<a id="目录结构"></a>

## 目录结构

```text
MyProject/
  DShader/
    Materials/
      M_Sample.dsm
    dreamshader.lock.json
    Packages/
      @scope/
        package-name/
          dreamshader.package.json
          README.md
          LICENSE
          Library/
            Noise.dsh
          Examples/
            M_NoisePreview.dsm
```

| 路径 | 插件会编译吗？ |
| :-- | :-- |
| `DShader/Materials/M_Sample.dsm` | 会 —— 普通源文件 |
| `DShader/Packages/**/Library/*.dsh` | 从不直接编译；**可以被 import**，它的声明被声明进 import 它的文件 |
| `DShader/Packages/**/*.dsf`（以及 `.dss` / `.dsi` / `.dsp`） | 见[枚举缺口](#源文件枚举) —— 交互式会，无头不会 |
| `DShader/Packages/**/Examples/*.dsm` | **任何路径下都不会** |
| `DShader/dreamshader.lock.json` | 插件根本不读 |

目录名是硬编码的：package 根永远是配置的源目录下那个字面量子目录 `Packages`。改 *Source Directory* 会把它一起搬走；
没有单独的 package 目录设置。

<a id="职责划分"></a>

## 职责划分

标注 *扩展* 的每一项都由[编辑器扩展](vscode.md)在它们各自的仓库里定义和执行，
插件侧完全观察不到。

| 方面 | 实现方 |
| :-- | :-- |
| 启动时创建 `<SourceDirectory>/Packages` | **插件** |
| 把 `<SourceDirectory>/Packages` 作为第三个 `import` / `#include` 解析根 | **插件** |
| 把 package 文件排除出源文件枚举 | **插件** |
| `dreamshader.package.json` —— 存在性、字段、校验 | 扩展 |
| `dreamshader.lock.json` —— 存在性、内容、更新 | 扩展 |
| 安装、更新、移除 package | 扩展 |
| Package 商店、index 源、GitHub topic 搜索 | 扩展 |
| Package 脚手架 / "创建 package" 流程 | 扩展 |
| 带 scope 的 `@scope/name` 命名 | 扩展 |

**提示：**

一个 package 能被解析，纯粹是因为它的文件存在于 `<SourceDirectory>/Packages` 下。删掉 manifest 不会让
`import` 失效，加上 manifest 也不会改变插件的行为。manifest 和锁文件是给扩展的安装器和商店用的。

<a id="import-解析"></a>

## Import 解析

`import` 说明符 —— 或者 `.dss` 里 `#include` 的路径 —— 先被归一化，然后按顺序试三个候选根。第一个既待在自己根内部、
又在磁盘上存在的候选胜出。这个解析器由编译器的 include 解析器和编辑器的依赖扫描器共用，所以两边结论一致。
（自 2.0.0 起） 编译器把找到的头文件单独读取、把它的名字声明进 include 它的文件；不会把任何东西当文本贴进来。

<a id="说明符归一化"></a>

### 说明符归一化

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 剥掉首尾空白 |
| 2 | 每个 `\` 换成 `/` |
| 3 | 反复剥掉**所有**前导 `./` 序列 —— `././X` 变成 `X` |
| 4 | 结果没有扩展名时，追加 `.dsh` |

第 4 步意味着 `import "@scope/pkg/Library/Noise"` 和 `import "@scope/pkg/Library/Noise.dsh"` 是同一个 import。
只有头文件能被 import （自 2.0.0 起）：指向 `.dsf` 或 `.dsm` 的 `import` 是 `DSH2252`，解析到 `.dsh`（或 `.dss`）之外的 include 是 `DSH8295`。

<a id="候选根"></a>

### 候选根

| 顺序 | 候选路径 | 必须待在的根 |
| :-- | :-- | :-- |
| 1 | `<发起 import 的文件所在目录>/<specifier>` | 该文件自己的 import 根 —— 任何一个根的源目录或 packages 目录中包含它的那个（取最长匹配），都不包含时取文件自己所在目录 |
| 2 | `<所属根的源目录>/<specifier>` | 那个源目录 |
| 3 | `<所属根的源目录>/Packages/<specifier>` | 那个 packages 目录 |

所属根就是发起 import 的文件所在的源码根 —— 工程文件是 `<SourceDirectory>` —— 所以插件的源文件按插件自己的 `Packages` 解析。

对住在 package 里的文件来说，候选 1 的根是 `Packages`，所以 package 内的 `.dsh` 可以用相对说明符触达兄弟文件，
但**爬不出 package 树** —— 用 `../` 时逃逸守卫会否掉这个候选，解析继续落到根 2 和根 3。

任何根下都解析不到的说明符是 `DSH8292`。完整的语法、头文件怎么被读取以及环处理，见
[import 与命名空间](../language/import.md)。

<a id="源文件枚举"></a>

## 源文件枚举

插件有**两个**枚举器。它们排除的东西不一样，而这个不对称决定了哪些 package 文件会被编译。

| 枚举器 | 扫描的扩展名 | 排除 | 排序 |
| :-- | :-- | :-- | :-- |
| 全量源枚举 | `.dsm`、`.dsh`、`.dsf`、`.dss`、`.dsi`、`.dsp` | 任何根的 `Packages` 下的**一切** | 是 |
| 材质源枚举 | `.dsm`、`.dsf`、`.dss`、`.dsi`、`.dsp` —— 从不含 `.dsh` | 只排除任何根的 `Packages` 下的 `.dsm` | 否 |

排除判定是与 package 目录做大小写不敏感的路径前缀比较，既命中 package 目录本身，也命中它下面任意深度的一切。
带 `-All` 的[命令行](commandlet.md)命令 （自 2.0.0 起） 用的是各自的扫描，它们全都丢掉 `Packages` 下的一切。

| 特性 | 枚举器 | 看得到 package `.dsm` | 看得到 package `.dsf`、`.dss`、`.dsi`、`.dsp` | 看得到 package `.dsh` |
| :-- | :-- | :-- | :-- | :-- |
| 启动时的 Ephemeral 生成 | 全量 | 否 | 否 | 否 |
| [命令行](commandlet.md) `compile -All` | 全量，去掉头文件 | 否 | 否 | 否 |
| Cook 期落盘 | 全量 | 否 | 否 | 否 |
| Material Content Browser 的源文件列表 | 全量 | 否 | 否 | 否 |
| VirtualFunction 声明同步 | 全量 | 否 | 否 | 否 |
| *Recompile DSM* / 全量重扫队列 | 材质 | 否 | **是** | 否 |
| 依赖图重建 | 材质 | 否 | **是** | 只作为依赖 |
| 文件 watcher，保存时 | 自己的判定 | 否 | **是** | 重编 import 它的源文件 |

**警告：**

**`Packages` 排除对 `.dsm` 是完整的，对其他每种会构建东西的文件只是部分的。** 自动编译路径判定的是"`Packages` 下的一个 `.dsm`"，
而别的文件都不满足这个条件 —— 于是 package 里的 `.dsf`、`.dss`、`.dsi` 或 `.dsp` 会被 *Recompile DSM* 排入队列、
在文件 watcher 看到它被保存时重新编译、并参与依赖图，同时对启动生成、`compile -All`、cook、Material Content Browser 和
VirtualFunction 同步保持不可见。

因此，一个附带这类源文件的 package 会在交互式编辑器会话里生成它们的资产，
而在无头构建里**不会**。请把库代码以 `.dsh` 头文件形式发布，或者把源文件从 `Packages` 复制到项目自己的源树里。

**警告：**

package 里的 `Examples/**/*.dsm` **在任何路径下都不会被编译**，也不会出现在 Material Content Browser 里。
要用某个示例，把它从 `DShader/Packages` 复制到 `DShader/` 或它下面任何不叫 `Packages` 的子目录。

package 的 `.dsh` 头文件完全可以被 import，但从不被扫描 `VirtualFunction` 声明，
所以 package 里附带的 `VirtualFunction` 块永远不会被校验，也不会与它的 `UMaterialFunction` 资产同步。
见[编辑器工具](editor-integration.md#virtualfunction-工具)。

<a id="dreamshaderpackagejson"></a>

## `dreamshader.package.json`

放在 package 根目录。**插件不读它。** 下面这些字段是扩展的安装器和商店使用的约定，以扩展仓库为准。

```json
{
  "name": "@typedreammoon/dream-noise",
  "version": "1.0.0",
  "displayName": "Dream Noise",
  "description": "Reusable noise functions for DreamShaderLang.",
  "author": "TypeDreamMoon",
  "repository": "https://github.com/TypeDreamMoon/dream-noise",
  "license": "MIT",
  "dreamshader": {
    "language": "DreamShaderLang",
    "version": ">=1.0.0",
    "entry": "Library/Noise.dsh"
  },
  "keywords": ["noise", "fbm", "voronoi"]
}
```

| 字段 | 类型 | 用途 |
| :-- | :-- | :-- |
| `name` | string | 包身份；普通 `name` 或带 scope 的 `@scope/name` |
| `version` | string | 包版本；推荐 SemVer |
| `displayName` | string | 商店里展示的可读名 |
| `description` | string | 商店里展示的简短说明 |
| `author` | string | 作者署名 |
| `repository` | string | Git 地址；用于更新已安装的包 |
| `license` | string | SPDX 标识符 |
| `dreamshader.language` | string | 语言标识，`DreamShaderLang` |
| `dreamshader.version` | string | 包面向的 DreamShaderLang 版本范围 |
| `dreamshader.entry` | string | 推荐入口头文件，用于文档和商店展示 |
| `keywords` | string[] | 商店搜索标签 |

给仓库打上 GitHub topic `dreamshader-package`，它就能在商店里被发现。

<a id="dreamshaderlockjson"></a>

## `dreamshader.lock.json`

| 项目 | 值 |
| :-- | :-- |
| 路径 | `<SourceDirectory>/dreamshader.lock.json` |
| 写入方 | 扩展，在安装和更新时 |
| 读取方 | 扩展 |
| 记录 | 包名、版本、仓库、commit、安装路径 |

**插件既不读也不写。** 删掉它对编译毫无影响；它的存在是为了让团队看清签入的是哪些 package 版本。
如果团队会安装 package，请把它提交进版本库。

<a id="扩展命令"></a>

## 扩展命令

VSCode 扩展提供的命令面板条目。安装和更新需要 `PATH` 上有可用的 `git` —— 插件从不下载任何东西。

```text
DreamShaderLang: Install Package from GitHub
DreamShaderLang: Browse Package Store
DreamShaderLang: Update Installed Packages
DreamShaderLang: Remove Installed Package
DreamShaderLang: Open Packages Folder
DreamShaderLang: Add Package Store Index Source
DreamShaderLang: Remove Package Store Index Source
DreamShaderLang: Create Package Step by Step
```

安装既接受 `owner/repo` 简写，也接受完整的 `https://github.com/owner/repo` 地址。商店会读取一个或多个
index JSON 文件（通过 `dreamshader.packageStoreIndexUrls` 配置），外加可选的 GitHub topic 搜索 ——
见 [VSCode 与 Rider](vscode.md#vscode-设置)。

<a id="编写一个-package"></a>

## 编写一个 package

<a id="建目录"></a>

### 建目录

用带 scope 的名字，这样绝不会和别人撞：`DShader/Packages/@scope/package-name/`。

<a id="加-manifest"></a>

### 加 manifest

package 根目录下的 `dreamshader.package.json`，至少要有 `name` 和 `version`。插件不需要它，商店和安装器需要。

<a id="公共-api-放-library"></a>

### 公共 API 放 `Library/`

一个主题一个 `.dsh`，用 `Namespace` 包起来，这样 helper 名字不会和使用方项目里的撞。
把 `dreamshader.entry` 指向用户应该 import 的那个头文件。

<a id="示例放-examples"></a>

### 示例放 `Examples/`

它们在 package 内部永远不会被编译 —— 这正是你想要的：既能说明用法，又不会在每个安装了它的项目里生成资产。

<a id="发布"></a>

### 发布

推到 GitHub 并打上 topic `dreamshader-package`；希望在商店里被发现的话，再往某个 package store index 里加一条。

一个库头文件：

```c
Namespace(Name = "DreamNoise")
{
    Function float Remap01(float x)
    {
        return saturate(x * 0.5 + 0.5);
    }
}
```

以及使用它的项目材质：

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
        float N  = DreamNoise::Remap01(UV.x * Scale);
        Color = vec3(N, N, N);
    }
}
```

<a id="版本号"></a>

### 版本号

| 变更 | 建议的版本位 |
| :-- | :-- |
| 新增 helper，不破坏已有调用 | minor |
| 修实现，保持 API 不变 | patch |
| 重命名或移除公共 helper | major |
| 显著改变生成产物的行为 | major，并在 README 里写迁移说明 |

<a id="设计建议"></a>

### 设计建议

| 建议 | 原因 |
| :-- | :-- |
| 公共 helper 放进 namespace | 包名和项目名不会冲突 |
| 导出稳定的入口头文件 | 材质不该依赖你内部的文件布局 |
| 库代码用 `.dsh` 发布，不要用 `.dsf` | package 里的 `.dsf`（以及 `.dss` / `.dsi` / `.dsp`）对无头构建不可见 —— 见[上文](#源文件枚举) |
| 示例放 `Examples/` | 既说明用法，又不会被编译 |
| 不要写项目专属资产路径 | package 应该能原样在项目之间搬 |
| README 里写清 import 那一行 | 那是每个用户唯一必须知道的东西 |

<a id="说明"></a>

## 说明

- package 目录在模块启动时就会创建，即使它是空的，即使在 commandlet 进程里也一样，
  与源目录和生成着色器目录一起创建。
- package 文件就是普通文件。没有任何东西阻止你原地编辑 package 里的 `.dsh` ——
  但扩展的更新命令会把它覆盖掉。
- 因为排除是路径判定，把非 package 路径指向 package 内容的符号链接或 junction，
  会按它解析出来的真实路径处理。

<a id="示例"></a>

## 示例

一个安装在 `DShader/Packages/@typedreammoon/dream-noise/` 的 package，它的说明符是这样解析的：

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

<a id="继续阅读"></a>

## 继续阅读

- [import 与命名空间](../language/import.md) — 指令的语法、环处理和行号映射。
  - [文件模型](../language/source-files.md) — .dsm、.dsh 和 .dsf 各自能装什么。
  - [VSCode 与 Rider](vscode.md) — 拥有安装器和商店的那两个扩展。
  - [命令行](commandlet.md) — compile -All，它用的是全量枚举器。
  - [项目结构](../getting-started/project-layout.md) — Packages 在源树里的位置。
