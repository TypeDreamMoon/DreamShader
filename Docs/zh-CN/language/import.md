---
title: import 与命名空间
description: import 如何让一个 .dsh header 的声明在当前文件里可见 —— 识别规则、specifier 规范化、源码根、header 怎么被读取，以及命名空间如何跨文件工作。
---

`import` 让一个 `.dsh` header 的声明在当前文件里可见。它是语言的一个 token、一条顶层声明，由 legacy 前端读取；
绑定器通过 include 解析器找到这个 header，单独预处理、解析它，再把它的名字登记进当前文件 （自 2.0.0 起）。
1.9.x 及以前，编辑器的源加载器会在解析之前逐行剥掉 `import`，把 header 的文本粘贴进来。

```text
<file>.dsm
  │
  ├─ 预处理器              按行求值 #if；没选中的分支里的 import 根本不会被读取
  │
  ├─ legacy 前端           块、section、Graph 语句、import  ->  2.0 语法树
  │
  ├─ 绑定器                解析每个 import；header 单独预处理、解析，
  │                        它的声明登记进本文件
  │
  └─ IR / 发射器           材质节点  ->  资产
```

`.dss` 把同一件事写成 `#include "…"`，也接受 `import "…";`。

<a id="语法"></a>

## 语法

```c
import "<path>" [;]
import '<path>' [;]          // 在 .dsm、.dsf 或 .dsh 里

<path> := 指向一个 .dsh 的路径，相对本文件或它所属的源码根，
          或者以 "/" 开头；".dsh" 可以省略
```

`;` 是可选的 （自 1.2.2 起）。`import` 是小写的；其他大小写读法相同，但给出警告 `DSH2253`。

```c
import "Shared/Common";                                   // -> 自己所在根的 Shared/Common.dsh
import '@typedreammoon/dream-noise/Library/Noise.dsh';    // -> <根>/Packages/@typedreammoon/…
import "../Lib/Color.dsh"                                 // -> 相对导入方所在目录
import "/Shared/Toon.dsh";                                // -> 先找自己的根，再找其他根（2.0.0 起）
```

<a id="识别"></a>

### 识别

`import` 是一个 token，解析器像读其他顶层声明一样读它 （自 2.0.0 起）；1.x 那套在原始文本里逐行识别的规则
已经没有了。

| 规则 | |
| :-- | :-- |
| 位置 | 文件顶层、块与块之间 |
| 注释 | 普通注释：写在 `/* … */` 里或 `//` 之后的 `import` 就是被注释掉了（1.x 的加载器照样执行 `/* … */` 里的 import） |
| 路径 | 一个字符串：`"…"`，支持常规转义；在 1.x 文件和 `.dsh` 里也可以是同一行里的 `'…'` |
| `;` | 可选 |
| `#if` | 由 [预处理器](lexical.md#预处理器) 先处理：没选中的分支里的 `import` 不会被读取 |

| 被拒绝 | 诊断 |
| :-- | :-- |
| `import` 后面不是字符串 | `DSH2252` |
| 路径指向 `.dsf` 或 `.dsm` —— 材质文件和函数文件各自单独编译，从不被包含 | `DSH2252` |
| 带根限定的路径（`Project:Shared/Common.dsh`、`Plugin.X:…`） | `DSH2252` |
| 解析到的文件不是 `.dsh`（或 `.dss`） | `DSH8295` |

**提示：**

编辑器的**依赖扫描器**仍然把 `import` 行当文本读，用来判断保存一个 header 时该重新编译哪些源文件：去空白后以
`import` 开头（忽略大小写）、然后是 `"…"` 或 `'…'` 路径、可选的 `;`、之后只有 `//` 注释的行。它读的是原始文件，
`#if` 的所有分支都算，所以只有死分支导入的 header 改了，导入方照样会重建。这是有意的：多重建一次花几秒，漏重建
一次就留下过期的资产。

<a id="specifier-规范化"></a>

## Specifier 规范化

| # | 步骤 |
| :-- | :-- |
| 1 | 去掉首尾空白 |
| 2 | 把每个 `\` 换成 `/` |
| 3 | 去掉**全部**前导 `./` |
| 4 | 如果结果完全没有扩展名，追加 `.dsh` |

所以 `import "Shared/Common"` 和 `import "Shared/Common.dsh"` 是同一条指令。

**提示：**

第 4 步问的是路径有没有扩展名，而不是有没有*已知的*扩展名，而且它只看最后一段路径。
`Shared/Common.v2` 算作"已经有扩展名"，所以不会追加 `.dsh`，这个 specifier 只有在恰好存在同名文件时
才解析得到。*目录*名里的 `.` —— 比如 `@scope/pkg.v2/Lib` —— 不算，仍然会追加 `.dsh`。

<a id="解析"></a>

## 解析

| 路径 | 相对于 |
| :-- | :-- |
| 一个存在的绝对文件路径 | 那个文件 |
| 以 `/` 开头 （自 2.0.0 起）：`/Shared/Common.dsh` 或 `/Game/Shared/Common.dsh` | 去掉锚（`/` 或 `/Game/`），先找导入方自己的根 —— 源目录、再 packages 目录 —— 然后按根的顺序找其他每个根的这两个目录，第一个存在的文件胜出。永远不相对导入方文件 |
| 其他 | 下面三个候选 |

三个候选按顺序尝试。每个候选都配一个**包含根**；解析结果落在根之外的候选会被跳过而不是报错，第一个
在磁盘上存在的候选胜出。

| # | 候选 | 包含根 |
| :-- | :-- | :-- |
| 1 | `<导入方文件所在目录>/<specifier>` | 所有根的源目录和 packages 目录中，包含该导入方的、最长的那个；都不包含时是文件自己的目录 |
| 2 | `<所属根的源目录>/<specifier>` | 该源目录 |
| 3 | `<所属根的 packages 目录>/<specifier>` | 该 packages 目录 |

| 目录 | 默认值 | 项目设置 |
| :-- | :-- | :-- |
| 项目源目录 | `<Project>/DShader` | *Source Directory* |
| 插件源目录 （自 1.6.0 起） | `<Plugin>/DShader` | *Scan Plugin Source Directories* |
| Packages | `<源目录>/Packages` | 推导得到，不能单独配置 |

哪里都解析不到是 `DSH8292`，消息里说明原因。

<a id="源码根"></a>

### 源码根

（自 1.6.0 起）

候选 2 和 3 属于导入方文件的**所属根** —— 也就是该文件所在的那个源码根。

**警告：**

**相对 specifier 永不跨根。** `Plugins/MoonToon/DShader` 下的一个 `.dsm` 写 `Shared/Common.dsh`
够不到 `<Project>/DShader`，反过来也一样。

这条规则的存在是为了让「装一个插件」不能改变一个既有 import 的含义。如果项目根是全局兜底，
两个都带 `Shared/Common.dsh` 的插件就会按扫描顺序解析，而禁用一个插件会静默改变另一个根的 import 指向。

不属于**任何**根的文件 —— 测试夹具、`-Source` 指到树外的 commandlet —— 候选 2 和 3 用项目的源目录和
packages 目录，也就是根这个概念存在之前每个文件的行为。

<a id="刻意跨根"></a>

### 刻意跨根

1.6.0 – 1.9.x 用根限定符加 `:` —— `Project:Shared/Common.dsh`、`Plugin.MoonToon:Shared/Toon.dsh` —— 让 import
指向另一个根的源目录和 packages 目录。`import` 现在拒绝这种写法（`DSH2252`）（自 2.0.0 起）。要用另一个根
里的 header，有三种办法：

- 以 `/` 开头的路径：先找导入方自己的根、再找其他每个根，所以自己根里的同名文件胜出；
- 把它复制到本根；
- 通过 [package](../tools/packages.md) 分发。

**警告：**

**一个伸手到项目根里的插件不再是自包含的**：把它交付到另一个项目，这条 import 就悬空了。

包含比较在所有平台上都忽略大小写；至于候选最终能不能被找到，仍然遵循文件系统自身的大小写行为。

包含检查正是阻止相对 specifier 爬出目录树的机制。`..` 段会在检查*之前*被解析掉，因此：

- 对于直接位于 `DShader` 下的文件，`import "../Secret.dsh"` 解析到源目录之上，候选 1 被跳过；候选 2 和 3
  也会塌缩同样的 `..` 并落在各自的根之外，同样被跳过；
- 对于位于 `DShader/Packages/@scope/pkg/` 下的文件，`..` 可以在 `DShader/Packages` 内任意穿行，因为那
  才是为它选定的包含根；
- 对于两个目录都不属于的源文件，包含根就是它自己的目录，任何带 `..` 的 specifier 都无法通过候选 1 解析。

<a id="package-风格路径"></a>

### Package 风格路径

`@scope/name/…` **不是**一种独立的路径语法。`@` 只是普通的目录名字符，
`"@typedreammoon/dream-noise/Library/Noise.dsh"` 能通过候选 3 解析，纯粹是因为
`DShader/Packages/@typedreammoon/dream-noise/Library/Noise.dsh` 在磁盘上存在。没有 scope 注册表，没有
版本解析，也没有被特殊处理的根。

**提示：**

候选 1 和 2 仍然会先被尝试，所以放在导入方旁边、或者放在 `DShader` 下的同名文件会**遮蔽** package 里
的那一份。

这套约定假定的目录布局见 [Package](../tools/packages.md)。

<a id="header-是怎么被读取的"></a>

## header 是怎么被读取的

什么都不内联 （自 2.0.0 起）。每个 header 单独读取，它的声明加进导入方文件的声明里。

| 行为 | 规则 |
| :-- | :-- |
| 预处理 | 每个 header **单独**对着这次编译的 define 表预处理：header 里的 `#define` 传不到导入它的文件。header 的 `#if` 出错是 `DSH8291` |
| 解析 | 每个 header 用它自己的方言单独解析：`.dsh` 可以让 1.x 块和 2.0 声明并存，不管导入它的是哪种文件。header 的语法错误报在它自己的行上，随后这条 import 是 `DSH8294`；读不了是 `DSH8293` |
| 名字 | header（以及它导入的 header）的每个声明在导入方文件里都可见。同一个名字在 header 和导入方、或两个 header 里都声明了，是 `DSH4210` |
| 菱形依赖 | 被导入两次的 header 只登记一次 |
| 循环 | 直接或经由别的 header 导入自己，是 `DSH4211` |
| 资产 | 没有：header 不能放资产块（`DSH2249`），所以 import 只带来名字、永远不带来资产，一个文件也最多只有一个 `Shader`（`DSH2250`） |
| 重建 | 这次编译读到的每个 header 的预处理后文本都计入该源文件的构建键；保存一个 header 会把导入它的每个源文件排进重编队列 |

**提示：**

**1.9.x 及以前**，加载器在解析之前把整个 import 闭包粘贴成一段文本，所以"一个 `Shader`"的规则是闭包范围的，
`.dsh` 可以导入 `.dsf` 并把它的 `ShaderFunction` 块作为导入方的一部分编译，每个位置都得从拼装后的文本映射回去。
这些都不存在了。

<a id="跨文件的命名空间"></a>

## 跨文件的命名空间

`Namespace` 不是模块，和 `import` 没有关系。导入 header 里的命名空间登记进导入方文件之后无需任何额外声明就
可见 —— 它们跨文件冲突的方式，也和在同一个文件里一模一样。

```c
// DShader/Lib/Common.dsh
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

```c
import "Lib/Common.dsh";

Graph = {
    vec3 Tinted;
    Common::ApplyTint(Base, Tint, Tinted);
}
```

| 规则 | 后果 |
| :-- | :-- |
| 重复打开是允许且不作检查的 | 两个导入 header 里的 `Namespace(Name="Common")` 都会加 `Common::` 前缀。 |
| 成员只能通过限定名（或压平后的 `Common_ApplyTint`）访问 | 没有 `using`，没有名字导入，也没有不限定的回退。 |
| `import` 只能写在文件作用域 | 写在 `Namespace` 体内是 `DSH6310` （自 2.0.0 起）。 |
| 限定名重复在绑定时发现 | `DSH4210`，消息里点名两处声明所在的文件。 |

声明规则、`::` 解析和函数体规范化见 [函数](functions.md#namespace)。

<a id="源码行映射"></a>

## 源码行映射

没有什么需要映射的了 （自 2.0.0 起）。诊断带着它所在的文件 —— header 里的错误就是 header —— 以及构造的行和
列，打印成 `<file>(<line>,<column>): DSHnnnn: <message>`。1.9.x 的位置来自拼装文本里的 `near index` 偏移，在
section 内部并不可靠；那套机制已经没有了。

<a id="组织一个项目"></a>

## 组织一个项目

| 变更 | 会重新编译什么 |
| :-- | :-- |
| 改了一个 `.dsm` | 那个材质，以及该文件里声明的每个函数资产 |
| 改了一个 `.dsf` | 它声明的那些函数资产 |
| 改了一个 `.dsh` | 直接或间接导入它的 `.dsm` / `.dsf`（以及 `.dss`） |
| 改了一个 package 文件 | 导入它的那些源文件 |

给项目定一个稳定的入口 header，能让这张图保持很浅：

```c
// DShader/Shared/Common.dsh
import "Shared/Color.dsh";
import "Shared/Texture.dsh";

Namespace(Name="Project")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }
}
```

```c
// 每个材质只导入一个文件
import "Shared/Common.dsh";
```

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH2252: Expected a double-quoted path after 'import', found {0}.` | 前端不读的 import：import 后面不是带引号的路径；路径指向 .dsf 或 .dsm；或者是 1.6.0 的根限定写法（Project:…、Plugin.X:…）。 | 写 import "Shared/Common.dsh";，路径相对本文件或它所属的源码根；够另一个根里的 header 用以 / 开头的路径，或复制过来、或通过 package。 |
| `DSH2253: '{0}' is read as 'import'; write it in lower case.` | import 写成了别的大小写（警告）。 | 写小写的 import。 |
| `DSH8292: '{0}', included from '{1}', could not be resolved: {2}.` | 没有任何源码根含有这个文件。没有扩展名的 specifier 会补 .dsh。 | 检查拼写和扩展名；要从根锚定而不是相对导入方，路径以 / 开头。 |
| `DSH8295: '{0}' is not a DreamShader header; an include names a '.dsh' (or a '.dss'), not a '{1}' file.` | 解析到的文件不是 header —— 比如一个 .dsm 或 .dsf。 | 导入声明所在的 .dsh；要共享的声明挪进 .dsh。 |
| `DSH8293: '{0}', included from '{1}', resolved but could not be read.` | 文件找到了但打不开 —— 权限或被别的工具独占锁定，不是拼写问题。 | 确认文件可读。 |
| `DSH8294: '{0}', included from '{1}', could not be parsed; its own errors are above.` | 被导入的 header 解析失败；它自己的错误已经先报出来了，报在 header 自己的行上。 | 修正 header 自己的错误。 |
| `DSH8291: '{0}' failed conditional compilation: {1}: {2}` | header 单独预处理时失败；消息里带预处理器自己的 DSH103x 码。 | 看里面那个码；记住 #define 只在本文件有效。 [详解](lexical.md#预处理器) |
| `DSH4211: Including '{0}' from '{1}' closes a cycle; a header may not include itself, directly or through another header.` | 沿着 import 走回到了一个正在读取的文件。 | 把两边都需要的共享声明抽到第三个 header 里，两边都导入它。 |
| `DSH4210: '{0}' is already declared in this file; one name declares one thing.` | header 声明的名字在导入方文件或另一个 header 里又声明了一次；消息里点名两处所在的文件。 | 给其中一个改名。 |
| `DSH2249: Expected only Function, GraphFunction, Namespace and VirtualFunction blocks in a '.dsh' header, found the asset block '{0}', which belongs in a .dsm or .dsf file.` | 被导入的 .dsh 里有资产块。 | 把这个块挪进它自己的 .dsm / .dsf。 [详解](source-files.md) |

<a id="继续阅读"></a>

## 继续阅读

- [文件模型](source-files.md) — 每种文件允许放什么，以及源码根。
  - [函数](functions.md) — Namespace 的声明规则与限定调用。
  - [Package](../tools/packages.md) — DShader/Packages 与 @scope/name 布局。
  - [项目设置](../settings/project.md) — Source Directory，它会移动全部搜索根。
