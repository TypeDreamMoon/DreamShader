---
title: "import"
description: "import"
---

<a id="import"></a>

# import

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **import**

一条顶层语句，使某个 `.dsh` header 的声明在当前文件中可见。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf`、`.dsh`；`.dss` 将其写作 `#include "…"`，并同样接受 `import "…";` |
| 种类 | 顶层声明 |
| 名称 | 一个 `.dsh` header *（自 2.0.0 起；到 1.9.x 为止也可以是 `.dsf`）* |
| 处理者 | legacy 前端，负责读取该行；绑定器，通过 include 解析器解析它，并把该 header 的名字声明进当前文件 *（自 2.0.0 起；到 1.9.x 为止，由编辑器的源加载器在解析之前把 header 的文本粘贴进来）* |
| 大小写规则 | `import` 为小写；其他大小写读法相同，但给出警告 [`DSH2253`](../diagnostics/DSH2xxx.md#dsh2253) |

<a id="synopsis"></a>

## 语法

```c
import "<path>" [;]
import '<path>' [;]          // in a .dsm, .dsf or .dsh

<path> := a path to a .dsh, relative to this file or to its source root,
          or "/"-anchored; the ".dsh" may be left off
```

路径在导入文件自身的源码根内解析——见 [源码根](#roots)。1.6.0 的根限定形式（`Project:…`、`Plugin.<Name>:…`）会被拒绝 *（自 2.0.0 起）*。

<a id="recognition"></a>

## 识别

*（自 2.0.0 起）* `import` 是语言的一个 token，解析器像读其他顶层声明一样读它。1.x 那套在原始文本中逐行识别的规则已经没有了。

| 规则 | |
| :-- | :-- |
| 位置 | 文件顶层，块与块之间 |
| 注释 | 普通注释：写在 `import` 里或 `//` 之后的 `/* … */` 会被注释掉 *（1.x 的加载器照样执行 `/* … */` 里的一个）* |
| 路径 | 一个字符串：`"…"`，支持常规转义；在 1.x 文件或 `.dsh` 中，也可以是同一行里的 `'…'` |
| `;` | 可选 |
| `#if` | 由 [预处理器](preprocessor.md) 先解析：没选中的分支里的 `import` 不会被读取 |

| 被拒绝 | 代码 |
| :-- | :-- |
| `import` 后面不是字符串 | [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252) |
| 路径指向 `.dsf` 或 `.dsm` —— 材质文件或函数文件各自单独编译，从不被包含 | `DSH2252` |
| 带根限定的路径（`Project:Shared/Common.dsh`、`Plugin.X:…`） | `DSH2252` |
| 解析到的文件不是 `.dsh`（或 `.dss`） | [`DSH8295`](../diagnostics/DSH8xxx.md#dsh8295) |

> [!NOTE]
> 编辑器的**依赖扫描器**仍然把 `import` 行当作文本读取，以便知道保存一个 header 时该重新编译哪些源文件：一个去空白后以 `import` 开头（忽略大小写）、然后是 `"…"` 或 `'…'` 路径、可选的 `;`、之后只有 `//` 注释的行。它读的是原始文件，`#if` 分支也算在内，所以一个只被未选中分支导入的 header 改了，仍然会触发导入方重建。这是有意的：多重建一次花几秒，漏重建一次就留下过期的资产。

<a id="specifier-normalization"></a>

## Specifier 规范化

路径在解析之前会先规范化：

| # | 步骤 |
| :-- | :-- |
| 1 | 去掉首尾空白 |
| 2 | 把每个 `\` 换成 `/` |
| 3 | 去掉**全部**前导 `./` 序列 |
| 4 | 如果结果完全没有扩展名，追加 `.dsh` |

所以 `import "Shared/Common"` 和 `import "Shared/Common.dsh"` 是同一条指令。

> [!NOTE]
> 第 4 步问的是路径有没有扩展名，而不是有没有已知的扩展名，而且它只看最后一段路径。文件名里含有 `.` 的 —— `Shared/Common.v2` —— 算作「已经有扩展名」，所以不会追加 `.dsh`，这个 specifier 只有在恰好存在同名文件时才能解析到。*目录*名里的 `.` —— `@scope/pkg.v2/Lib` —— 不算，仍然会追加 `.dsh`。

<a id="resolution"></a>

## 解析

| 路径 | 相对于 |
| :-- | :-- |
| 一个存在的绝对文件路径 | 那个文件 |
| `/` 锚定 *（自 2.0.0 起）*：`/Shared/Common.dsh`，或 `/Game/Shared/Common.dsh` | 去掉锚（`/` 或 `/Game/`），先找导入方自己的根——其源目录，再其 packages 目录——然后按根的顺序找其他每个根的这两个目录；第一个存在的文件胜出。永远不相对导入方文件 |
| 其他 | 下面三个候选 |

三个候选按顺序尝试。每个候选都配一个**包含根**；解析结果落在其根之外的候选会被跳过而不是报错，第一个在磁盘上存在的候选胜出。

| # | 候选 | 包含根 |
| :-- | :-- | :-- |
| 1 | `<directory of the importing file>/<specifier>` | 所有根中，包含该导入文件的最长的源目录或 packages 目录；都不包含时是导入文件自己的目录 |
| 2 | `<owning root's source directory>/<specifier>` | 该源目录 |
| 3 | `<owning root's packages directory>/<specifier>` | 该 packages 目录 |

| 目录 | 默认值 | 项目设置 |
| :-- | :-- | :-- |
| 源目录 | `<Project>/DShader` | *Source Directory* |
| Packages | `<Source>/Packages` | 推导得到；不能单独配置 |

包含比较在所有平台上都忽略大小写。至于候选最终能不能被找到，仍然走普通的文件存在性检查，遵循文件系统自身的大小写行为。

哪里都解析不到的路径是 [`DSH8292`](../diagnostics/DSH8xxx.md#dsh8292)，消息中说明原因。

<a id="roots"></a>

### 源码根

候选 2 和 3 属于导入文件的**所属根**——该文件所在的源码根。相对 specifier 永不离开它：`Plugins/MoonToon/DShader` 下的 `.dsm` 不能通过写 `Shared/Common.dsh` 够到 `<Project>/DShader`，反过来也一样。

这条规则的存在是为了让添加一个插件不能改变既有 import 的含义。如果项目根是全局兜底，两个都带 `Shared/Common.dsh` 的插件就会按扫描顺序解析，而禁用一个插件会静默改变另一个根的 import 指向。

不属于**任何**根的文件——测试夹具、`-Source` 指向树外的 commandlet——候选 2 和 3 用项目的源目录和 packages 目录，也就是根这个概念存在之前每个文件的行为。

根列表以及插件如何贡献根，见 [源文件](source-files.md#source-roots)。

<a id="crossing-a-root-deliberately"></a>

### 刻意跨根

到 1.9.x 为止，根限定符加 `:` —— `Project:Shared/Common.dsh`、`Plugin.MoonToon:Shared/Toon.dsh` *（自 1.6.0 起）* —— 让 import 指向另一个根的源目录和 packages 目录。`import` 拒绝这种形式 *（自 2.0.0 起）*，报 [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252)。要用另一个根提供的 header，可以通过 `/` 锚定的路径，它会先找导入方自己的根、再找其他每个根——所以自己根里的同名文件胜出——或者把它复制到本根，或者通过 [package](../tools/packages.md)。

> [!WARNING]
> 一个伸手到项目根里的插件不再是自包含的：把它交付到另一个项目，这条 import 就悬空了。

包含检查正是阻止相对 specifier 爬出目录树的机制。`..` 段会在检查之前被解析掉，因此：

- 对于直接位于 `DShader` 下的文件，`import "../Secret.dsh"` 解析到源目录之上，
  因此候选 1 被跳过；候选 2 和 3 会塌缩同样的 `..` 并落在各自的根之外，因此被同样的检查跳过；
- 对于位于 `DShader/Packages/@scope/pkg/` 下的文件，`..` 可以在 `DShader/Packages` 内任意穿行，
  因为那才是为它选定的包含根；
- 对于不属于任何根的源目录或 packages 目录的源文件，包含根就是它自己的目录，
  因此没有任何 `..` specifier 能通过候选 1 解析。

<a id="package-style-paths"></a>

### Package 风格路径

`@scope/name/…` **不是**一种独立的路径语法。`@` 只是普通的目录名字符，
像 `"@typedreammoon/dream-noise/Library/Noise.dsh"` 这样的 specifier 能通过候选 3 解析，纯粹是因为磁盘上存在 `DShader/Packages/@typedreammoon/dream-noise/Library/Noise.dsh`。
没有 scope 注册表，没有版本解析，也没有被特殊处理的根。

> [!NOTE]
> 候选 1 和 2 仍然会先被尝试。放在导入方文件旁边、或者放在 `DShader` 下
> 的同名 `@typedreammoon/dream-noise/Library/Noise.dsh` 文件，会遮蔽 package 里的那一份。

这套约定假定的目录布局见 [Package](../tools/packages.md)。

<a id="how-a-header-is-read"></a>

## header 是怎么被读取的

*（自 2.0.0 起）* 什么都不内联。每个 header 单独读取，它的声明加进导入方文件的声明里。

| 行为 | 规则 |
| :-- | :-- |
| 预处理 | 每个 header **单独**对着这次编译的 define 表预处理：header 里的 `#define` 传不到导入它的文件。header 的 `#if` 失败是 [`DSH8291`](../diagnostics/DSH8xxx.md#dsh8291) |
| 解析 | 每个 header 用它自己的方言单独解析：`.dsh` 可以让 1.x 块和 2.0 声明并存，不管导入它的是哪种文件。header 的语法错误报在它自己的行上，随后这条 import 是 [`DSH8294`](../diagnostics/DSH8xxx.md#dsh8294)；读不了是 [`DSH8293`](../diagnostics/DSH8xxx.md#dsh8293) |
| 名字 | header（以及它导入的 header）的每个声明在导入方文件里都可见。同一个名字在 header 和导入方文件、或两个 header 里都声明了，是 [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210) |
| 菱形依赖 | 被导入两次的 header 只声明一次 |
| 循环 | 直接或经由别的 header 导入自己，是 [`DSH4211`](../diagnostics/DSH4xxx.md#dsh4211) |
| 资产 | 没有：header 不能放资产块（[`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249)），所以 import 只带来名字、永远不带来资产，一个文件也最多只有一个 `Shader`（[`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250)） |
| 重建 | 这次编译读到的每个 header 的预处理后文本都计入该源文件的[构建键](../generation/caching.md)；保存一个 header 会把导入它的每个源文件排进重编队列 |

> [!NOTE]
> **1.9.x 及以前**，加载器在解析之前把整个 import 闭包粘贴成一段文本，所以
> "一个 `Shader`"的规则是闭包范围的，`.dsh` 可以导入 `.dsf` 并把它的
> `ShaderFunction` 块作为导入方的一部分编译，每个位置都得从拼装后的文本映射回去。

<a id="source-line-mapping"></a>

## 源码行映射

没有什么需要映射的了 *（自 2.0.0 起）*。诊断带着它所在的文件 —— header 里的错误就是 header ——
以及构造的行和列，打印成 `<file>(<line>,<column>): DSHnnnn: <message>`。1.9.x 的位置来自拼装文本里的 `near index`
偏移，在 section 内部并不可靠；那套机制已经没有了。见
[诊断](../diagnostics/index.md#message-locations)。

<a id="diagnostics"></a>

## 诊断

| 码 | 触发条件 |
| :-- | :-- |
| [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252) | 该行不是对 header 的 import：`import` 后面没有字符串、是 `.dsf` / `.dsm` 路径，或带根限定的路径 |
| [`DSH2253`](../diagnostics/DSH2xxx.md#dsh2253) | *（警告）* `import` 写成了别的大小写 |
| [`DSH8292`](../diagnostics/DSH8xxx.md#dsh8292) | 路径解析不到任何文件 |
| [`DSH8295`](../diagnostics/DSH8xxx.md#dsh8295) | 解析到的文件不是 header |
| [`DSH8293`](../diagnostics/DSH8xxx.md#dsh8293) | header 读不了 |
| [`DSH8291`](../diagnostics/DSH8xxx.md#dsh8291) | header 条件编译失败 |
| [`DSH8294`](../diagnostics/DSH8xxx.md#dsh8294) | header 解析失败；它自己的错误会先报出来 |
| [`DSH4211`](../diagnostics/DSH4xxx.md#dsh4211) | 这条 import 闭合了一个循环 |
| [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210) | header 声明的名字又被声明了一次 |
| [`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249) | header 里有资产块 |

<a id="example"></a>

## 示例

```text
<Project>/DShader/
├── Materials/
│   └── M_Water.dsm
├── Shared/
│   └── Common.dsh
└── Packages/
    └── @typedreammoon/
        └── dream-noise/
            └── Library/
                └── Noise.dsh
```

```c
// DShader/Shared/Common.dsh
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

```c
// DShader/Materials/M_Water.dsm
import "Shared/Common";                                   // -> DShader/Shared/Common.dsh
import '@typedreammoon/dream-noise/Library/Noise.dsh';    // -> DShader/Packages/@typedreammoon/...

Shader(Name="Materials/M_Water")
{
    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        vec3 Water = vec3(0.1, 0.3, 0.6);
        Color = Common::ApplyTint(Water, vec3(1.0, 0.9, 0.8));
    }
}
```

第一条路径没有扩展名，所以会追加 `.dsh`；`M_Water.dsm` 旁边不存在候选 1，
所以候选 2 在 `DShader` 下找到了它。第二条只有候选 3 能找到。
两条指令都随意省略或包含 `;`。

每个 header 都单独预处理、单独解析；`Common::ApplyTint` 以及 `Noise.dsh`
声明的任何东西随后都是 `M_Water.dsm` 的名字，`Common.dsh` 里的诊断报在
`DShader/Shared/Common.dsh(<line>,<column>)`。

同样两行的 `.dss` 写法：

```hlsl
#include "Shared/Common.dsh"
#include "@typedreammoon/dream-noise/Library/Noise.dsh"
```

<a id="see-also"></a>

## 另见

- [源文件](source-files.md) — 每种文件允许放什么
- [词法元素](lexical.md) — 字符串与注释
- [命名空间](namespace.md) — 导入 header 的常见理由
- [Function](function.md) · [GraphFunction](graph-function.md) — `.dsh` 可以声明什么
- [VirtualFunction](virtual-function.md) — 写到 `DShader/VirtualFunctions` 的声明
- [Package](../tools/packages.md) — `DShader/Packages` 与 `@scope/name` 布局
- [预处理器](preprocessor.md) — `#if`，它在每个文件被解析之前运行
- [项目设置](../settings/project.md) — *Source Directory*，它会移动全部三个搜索根
- [诊断](../diagnostics/README.md) — 每一个码
