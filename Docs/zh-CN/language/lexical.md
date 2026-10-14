---
title: 词法与大小写
description: 大小写敏感性、空白与注释、标识符与保留字、字符串与数字字面量、语句切分，以及在解析之前按行运行的预处理器。
---

这一页讲字符级别的规则：哪些 token 在意大小写、注释和字面量怎么读、section 的内容怎么被切成语句，以及
在这一切之前运行的 `#if` 预处理器。

一个词法器把每个源文件 —— `.dss` 和 1.x 一样 —— 读成同一条 token 流，1.x 语法再由 legacy 前端从这些
token 里读出来 （自 2.0.0 起）。1.9.x 的声明语法和 `Graph` 语法各有一个字符扫描器，规则也各不相同。
对 `.dsm` / `.dsf` 有三个专门的选项：`///` 行是普通注释；同一行里的一对 `'…'` 是字符串（Content Browser
复制出来的 `Texture2D'…'` 外壳）；行首的 `#` 行是一个指令 token。

<a id="大小写敏感性"></a>

## 大小写敏感性

**顶层块关键字和小写的保留字是大小写敏感的。** 1.x 语法里其余类关键字的东西都忽略大小写匹配，下表标出了
例外。引擎定义的名字 —— 材质属性、节点类、引脚 —— 精确匹配；在 1.x 文件里只差大小写也能解析，但给出警告
`DSH5276`；文件自己声明的名字同理，警告是 `DSH5275` （自 2.0.0 起）。在语法之前运行的
[预处理器](#预处理器)从头到尾都大小写敏感。

| 构造 | 大小写敏感 |
| :-- | :-- |
| 顶层块关键字 —— `Shader`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend`、`MaterialLayer`、`MaterialLayerBlend`、`VirtualFunction`、`Namespace`、`Function`、`GraphFunction` | **是** |
| 保留字 —— `in`、`out`、`const`、`return`、`true`、`import`… | **是**，作为保留字；`In`、`Const`、`True` 是普通单词，1.x 语法在需要关键字的位置仍然把它们当关键字读 |
| Section 名 —— `Properties`、`Settings`、`Outputs`、`Inputs`、`Results`、`Options`、`Graph`、`Code`、`Layout` | 否 |
| 头部属性键 —— `Name=`、`Root=`、`Asset=` | 否 |
| `Settings` / `Options` 键、元数据键、`Expression( … )` 参数键、`Layout` 参数键 | 否 |
| `UE.*` 参数名、`Base.` 属性名 | 否，带 `DSH5276` |
| 类型 token —— `float3`、`vec3`、`Texture2D`、`ScalarParameter`… | 否 |
| `Properties` 中的 `const` 前缀、`Inputs` 中的 `opt` 前缀 | 否 |
| `Function` 之后的 `SelfContained` / `Inline` | 否 |
| `in` / `out` 参数限定符 | 否 |
| `true` / `false` | 否 |
| `Path(` 资产引用关键字 | 否 |
| `Base.` 绑定前缀、`Expression(`、`.Pin[` | 否 |
| `Group("…")` 属性分组头、`Slider(` 元数据简写 | 否 |
| `UE.` 内置前缀 | `Properties` 和 `GraphFunction` 函数体里否；`Graph` 里的调用**是** （自 2.0.0 起） |
| `#Region` / `#EndRegion` | 否 |
| `Node(` / `Comment(` layout 调用 | 否 |
| `import` | 否，其他大小写给警告 `DSH2253` |
| `#if` / `#ifdef` / `#ifndef` / `#elif` / `#else` / `#endif` / `#define` / `#undef` | **是 —— 只认小写**；`#IF` 是 `DSH1035`，不是静默无效 |
| define **名字**，以及保留前缀 `DS_` 的判定 | **是** —— `Foo` 和 `FOO` 是两个 define，`ds_foo` 不算保留 |
| `#if` 里的字符串比较 | **是** —— 在 Windows 上 `DS_PLATFORM == "windows"` 为假 |
| 调用参数哨兵 `default` | **是** —— 只认小写 （自 2.0.0 起） |
| `.dsm` / `.dsf` / `.dsh` 扩展名 | 否 |

所以 `shader(Name="X")` 和 `SHADER(Name="X")` 都是语法错误（`DSH2240`），而
`properties = { … }`、`settings { domain = "ui"; }` 和 `Shader(name="X")` 都能被接受。

关键字是整个单词：`ShaderFunction` 是它自己的标识符，永远不会被读成 `Shader` 加点别的，`ShaderLayerBlend`
也永远不会被读成 `ShaderLayer`。

<a id="语法概览"></a>

## 语法概览

```c
<token> := <identifier> | <keyword> | <string-literal> | <number> | <punctuation>

<identifier>     := { <letter> | _ } { <letter> | <digit> | _ }…
<qualified-name> := <identifier> :: <identifier>
<string-literal> := " { <character> | \<escape> }… "
<punctuation>    := { | } | ( | ) | [ | ] | ; | = | , | . | : | # | Graph 表达式的运算符
```

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

空白和注释可以出现在任意两个 token 之间，除此之外没有意义。

<a id="空白"></a>

## 空白

空格、制表符、垂直制表符、换页、回车和换行分隔 token。只认 ASCII 空白 （自 2.0.0 起）：其他 Unicode
空白（比如浏览器复制带进来的不换行空格）是 `DSH2101`。换行在语法里没有作用，只有
[预处理器](#预处理器)指令、`Graph` 体内的 `#Region` 指令（见 [Layout 与 #Region](layout.md)）
和诊断的行号关心它。

任意空白都能在任何 section 里分隔两个单词 （自 2.0.0 起）：类型和名字之间写制表符和写空格一样。

<a id="注释"></a>

## 注释

| 形式 | 规则 |
| :-- | :-- |
| `// …` | 行注释；一直到下一个换行为止，不含换行本身。在 1.x 文件里 `///` 是普通行注释 |
| `/* … */` | 块注释；在第一个 `*/` 处结束 |

- **块注释不嵌套。** `/* a /* b */ c */` 在第一个 `*/` 处结束，后面的 `c */` 又变回代码。
- **未闭合的块注释是错误** （自 2.0.0 起）：`DSH2102`，报在开头的 `/*` 处。1.x 会接受它并吞掉文件剩余部分。
- 注释在每个位置都被同样识别，`Graph` 体里也一样，统计花括号、圆括号、方括号时也是，所以注释里的花括号或
  引号永远不会让块失衡。

**警告：**

**注释能藏住 `import`，藏不住预处理指令。** `import` 是一个 token （自 2.0.0 起），所以写在 `/* … */`
里的 `import` 就是被注释掉了。`#if` 一族则是在理解注释之前按行扫描出来的，写在 `/* … */` 里的 `#if` 仍然是
指令；只有首个非空白字符是 `//` 的行才一定不是。注释指令请逐行写 `//`。

<a id="标识符"></a>

## 标识符

| 位置 | 可用字符 |
| :-- | :-- |
| 首字符 | ASCII 字母或 `_` |
| 后续字符 | ASCII 字母、数字或 `_` |

- 没有长度上限。只有 ASCII 字母算字母 （自 2.0.0 起），其他字母是 `DSH2101`。
- **块关键字、section 名和类型名不是保留的。** 叫 `Shader`、`Graph` 或 `float` 的属性、变量、参数、函数
  都可以；至于这个名字之后能不能解析，是它所处上下文的事。下面的小写保留字则是保留的。
- 每个 section 里的声明名都必须是一个标识符 token （自 2.0.0 起）。1.x 只检查 `Properties` 的名字非空，
  所以 `Properties { float 1Bad = 0; }` 能解析；现在 `1Bad` 是一个畸形数字（`DSH2105`），这条声明是 `DSH3250`。
- 带命名空间的名字用 `::`，例如 `Common::ApplyTint`。见 [函数](functions.md#namespace)。

<a id="保留字"></a>

### 保留字

所有文件共用一个词法器，它的关键字在 1.x 文件里同样保留，而且是小写 （自 2.0.0 起）：

`uniform` `static` `const` `extern` `export` `in` `out` `inout` `struct` `if` `else` `for` `while` `do`
`return` `break` `continue` `discard` `true` `false` `import`

它们都不能用作属性、输出、输入或 `Graph` 变量的名字。换个大小写就是普通单词：`Const` 和 `IN` 在 1.x 语法需要
`const` / `in` 的位置照样读成它们，`True` 读成 `true`。另外，`Function` / `GraphFunction` 的参数不能叫
`__return`（忽略大小写，`DSH6303`），预处理器的 define 名不能以 `DS_` 开头（`DSH1039`）。

<a id="名字净化"></a>

### 名字净化

名字进入生成的 HLSL 时会被净化：`A–Z a–z 0–9 _` 之外的每个字符变成 `_`，开头是数字则加一个 `_` 前缀，
连续的 `__` 塌缩成一个，结果为空或全是下划线时变成 `DreamShaderSymbol`。

```text
Common::ApplyTint   →   Common__ApplyTint   →   Common_ApplyTint
```

正是这次塌缩，让 `Common::ApplyTint` 和顶层的 `Common_ApplyTint` 是同一个名字（`DSH4210`）。

<a id="字符串字面量"></a>

## 字符串字面量

字符串以 `"` 开始，到**同一行**里下一个未转义的 `"` 结束，由词法器完成反转义。

| 转义 | 产生 |
| :-- | :-- |
| `\n` | 换行 |
| `\r` | 回车 |
| `\t` | 制表符 |
| `\"` | `"` |
| `\\` | `\` |
| `\0` | 什么都不产生 —— 字符串不能含 NUL |
| `\<其他字符>` | `DSH2104` （自 2.0.0 起）（1.x 丢掉反斜杠，留下那个字符） |

- 字符串永远不跨行 （自 2.0.0 起）：闭合 `"` 之前遇到换行就结束，未闭合的字符串是 `DSH2103`，在 section
  里和在 `Graph` 体里一样。1.x 允许字符串跨行，`Graph` 体里未闭合的字符串还会在输入末尾被静默闭合。
- 在 1.x 文件和 `.dsh` 里，同一行里的一对 `'` 也是字符串，没有转义 —— 也就是 Content Browser 引用的外壳
  `Texture2D'/Game/T.T'`。孤零零的 `'` 是 `DSH2101`。
- Windows 路径请写正斜杠：`"D:\new\textures"` 里的 `\n`、`\t` 会被当成转义。

任何可以写引号的地方 —— settings 值、元数据值、属性值、参数值 —— **引号都是可选的**：`Domain = UI;` 和
`Domain = "UI";` 等价。带引号的值就是字符串的文本，不带引号的值是它那些 token 的文本。

**提示：**

带引号的 `Settings` 或 `Options` 值不会被去空白：`Domain = " UI ";` 存下来的是带空格的 `UI`。

<a id="数字字面量"></a>

## 数字字面量

<a id="声明语法"></a>

### 声明语法

数字在任何地方都是 token （自 2.0.0 起），按下面 [Graph 表达式语法](#graph-表达式语法)的规则切分；然后
由 section 按消费者的要求读取这个 token 的文本：

| 消费者 | 接受什么 |
| :-- | :-- |
| 标量默认值 | 可选的正负号、数字、至多一个 `.`、可选的指数、可选的结尾 `f` / `F`；或者 `true` / `false` |
| 整数参数（`SortPriority`、`Layout` 坐标） | 可选的正负号和数字 |
| 布尔默认值 | 恰好 `true` 或 `false`，大小写不限 |
| 向量默认值 | `<anything>( <part> [, <part>]… )` —— 见下 |

不被接受的写法：默认值是 `DSH3254`，`SortPriority` 是 `DSH3258`，`Layout` 坐标是 `DSH3275`。

| 写法 | 读作 | 诊断 |
| :-- | :-- | :-- |
| `float Strength = 1.0f;` | `1.0` | 无 |
| `float Strength = 0.0f;` | `0.0` | 无 |
| `float Strength = 1abc;` | — | `DSH2105` 和 `DSH3254` （自 2.0.0 起）（1.x 静默读成 `1.0`） |
| `float Strength = abc;` | — | `DSH3254` |
| `float Strength = 0x1F;` | — | `DSH3254` |
| `float Strength = 1e3;` | `1000.0` | 无 （自 2.1.1 起）（2.0.0 – 2.1.0 是 `DSH3254`） |

向量字面量的形式和 1.x 一样故意写得很松。第一个 `(` 和**最后一个** `)` 界定分量，而 **`(` 之前的文本被完全
忽略** —— `float3(1,0,0)`、`vec3(1,0,0)`、`(1,0,0)` 和 `Nonsense(1,0,0)` 解析结果完全一样。内部按 `,` 切分且
不跟踪嵌套，所以某个分量里再嵌一层调用就会切错。每个分量可以是数字（可带指数和结尾的 `f`），也可以是
`true` / `false`。

| 分量个数 | 结果 |
| :-- | :-- |
| 1 | `(a, a, a, 1)` —— 铺满 x、y、z |
| 2 | `(a, b, 0, 0)` |
| 3 | `(a, b, c, 1)` |
| 4 或更多 | 取前四个，多出来的被忽略 |

未填充时的默认值是 `(0, 0, 0, 1)`。

<a id="graph-表达式语法"></a>

### Graph 表达式语法

| 元素 | 规则 |
| :-- | :-- |
| 起始 | 一个数字，**或者**紧跟数字的 `.` —— 所以 `.5` 合法 |
| 主体 | 数字，然后至多一个 `.` 和更多数字 |
| 指数 | `e` 或 `E`，可跟 `+` 或 `-`，然后是数字 |
| 后缀 | `f` `F` `h` `H` 表示浮点，`u` `U` `l` `L` 表示整数 |
| 十六进制 | `0x1F` 是一个十六进制整数 token；在 1.x 的 `Graph` 表达式里是 `DSH2222` （自 2.0.0 起）（1.x 把它读成 `0` 后面跟名字 `x1F`，值静默为 0） |
| 粘在一起的其他东西 | `1.2.3`、`1abc`、`1e`、`1.0u`、`1fu` 是一个畸形 token：`DSH2105` |

后缀是 token 文本的一部分，`0.55f` 就是数字 `0.55`。

2.0 语法的每个运算符都是 token （自 2.0.0 起）：`%`、`<`、`!`、`&&`、`?`、`[`、`]` 不再终止 `Graph`
表达式并丢掉后半截。1.x 的 `Graph` 没有的东西会被点名拒绝 —— `DSH2200` 到 `DSH2222`。见
[不支持的写法](../graph/unsupported.md)。

<a id="语句切分"></a>

## 语句切分

section 的内容是一串以 `;` 分隔的语句，逐个 token 读取 （自 2.0.0 起）。

- `;` 只在 `()`、`[]`、`{}` 之外分隔语句。字符串是一个 token，所以字符串里的 `;` 只是文本。
- 空语句被跳过，所以多余的 `;;` 无害。
- section 闭合 `}` 之前最后一条语句的 `;` 是可选的。
- section 闭合 `}` 之后的 `;` 是可选的，section 名和它的块之间的 `=` 也是可选的 （自 1.5.0 起）。
- 一个值 —— 默认值、设置、绑定源、参数 —— 一直延续到括号之外结束它的 `;`、`,` 或 `)`，所以不带引号的属性值
  可以含圆括号和逗号：`Asset = Path(Game, "MaterialFunctions/F_X")` 是一个值。

**提示：**

1.x 用三种不同的切分器切语句，其中两种找的是字面空格，所以在 `Inputs`、`Outputs`、`Results` 和 `Shader` 的
`Outputs` 里写 `float3<TAB>Color;` 会失败，`opt<TAB>float X` 也不会把输入标成可选。现在两者都能
正常工作 （自 2.0.0 起）。

<a id="预处理器"></a>

## 预处理器

（自 1.9.0 起）

`#if` / `#ifdef` / `#ifndef` / `#elif` / `#else` / `#endif`，外加 `#define` 和 `#undef`：在源文件被解析
**之前**、按行求值的指令。没被选中的分支永远到不了解析器，也永远不会变成节点。它不属于任何一套语法。

```c
#if      <expr>
#ifdef   <NAME>        // 完全等于 #if  defined(NAME)
#ifndef  <NAME>        // 完全等于 #if !defined(NAME)
[#elif   <expr>]…
[#else]
#endif

#define  <NAME> [<value>]
#undef   <NAME>
```

<a id="它是干什么的"></a>

### 它是干什么的

`#if` 做的是 [静态开关](properties.md) 做不到的那件事：裁剪**声明层**。静态开关是 `Graph` 里的
一个节点，只能在两个值之间选；当两次构建的差别是一行 `ShadingModel`、一组不同的 `Outputs`、一个不同的
`import`、甚至一整个不该存在的 `ShaderFunction` 块时，只有 `#if` 能表达。

| | `#if` | `StaticSwitchParameter` / `StaticBool` |
| :-- | :-- | :-- |
| 求值时机 | 生成时，作用于源码文本 | 编译材质 shader 排列时 |
| 能选择的东西 | `Function` 函数体之外任意两段行 | `Graph` 体里的两个表达式 |
| 能碰到 `Settings`、`Outputs`、`Properties`、`import`、整个块 | **能** | 不能 |
| 由谁决定 | 项目的 define 表 —— 每次编译一个答案 | 材质实例 —— 每个实例可以不同 |
| 没选中的分支 | 哪里都不存在，也从不校验 | 作为节点留在资产里 |
| 改变答案的代价 | 重建每个读了这个 define 的源文件 | 在实例上勾一下 |

一句话：**`#if` 是项目范围、构建期的决定；静态开关是逐实例、给美术用的决定。** 由环境决定、差别在声明里
（Substrate 开没开、引擎版本、平台专属的 `Outputs`）→ `#if`；应当由调材质实例的人决定 → 静态开关。

<a id="识别"></a>

### 识别

每个物理行都会被检查，不满足下列规则的行原样交给解析器。

| # | 规则 |
| :-- | :-- |
| 1 | 这一行在所有 `Function` / `GraphFunction` 函数体之外 |
| 2 | 去空白后的行不以 `//` 开头 —— 所以 `// #if FOO` 就是注释 |
| 3 | 首个非空白字符是 `#`，`#` 后面允许空白 |
| 4 | 关键字是八个之一，**只认小写** |
| 5 | 操作数之后只能跟空白和 `//` 注释，否则是 `DSH1042`；`#define` 例外，它的值一直到行尾 |
| 6 | `#if` 可以嵌套，最多 64 层 |
| 7 | `#else` 结束这条链 —— 之后不能再有 `#elif` 或第二个 `#else` |

`Function` 之外的其他 `#` 行：`#Region` / `#EndRegion`（任意大小写）原样交给解析器；`.dss` 和 `.dsh` 里的
`#pragma`、`#include` 交给 2.0 解析器；其余一律是 `DSH1035` —— 包括 `#IF`、`#endfi` 这类拼错的写法，以及
`.dsm` / `.dsf` 声明层里的 `#include`（那里应该写 [`import`](import.md)）。拼错的 `#endfi` 不会
被静默吞掉，否则它下面的一切都会无条件生效。

**警告：**

**`Function` 和 `GraphFunction` 的函数体属于 shader 编译器。** 函数体是 HLSL，那里的 `#if` 针对的是 HLSL 自己
的预处理器和引擎的 define（`MATERIALBLENDING_SOLID`、`PIXELSHADER`…）。DreamShader 的预处理器不进入函数体：
里面的 `#` 行原样透传，不参与配对、不计入嵌套。反过来，**函数体里写不了 DreamShader 的 `#if`** —— 要在生成时
选择函数体，就把 `#if` 写在 `Function` 块外面：

```c
#if DS_SUBSTRATE
Function ApplyShading(in float3 C, out float3 R) { R = SubstratePath(C); }
#else
Function ApplyShading(in float3 C, out float3 R) { R = LegacyPath(C); }
#endif
```

`Graph` 体是 DreamShaderLang 而不是 HLSL，所以 `Graph` 里的 `#if` 由 DreamShader 处理。

<a id="条件表达式"></a>

### 条件表达式

支持 `defined(NAME)` / `defined NAME`、`!`、`&&`、`||`（短路）、`==` `!=` `<` `<=` `>` `>=`、`+ - * / %`
和括号。值只有两种：64 位有符号**整数**和**字符串**；条件为真当且仅当结果是非零整数。

| 表达式 | 求值为 |
| :-- | :-- |
| 整数字面量 | 它本身；十进制，或 `0x` 十六进制 |
| `"…"` 字面量 | 那个字符串 |
| `defined(X)` | `X` 已定义时为 `1`，否则为 `0` |
| **未定义**的标识符 | 整数 `0`，和 C 一样 |
| 定义为**空值**的标识符 | 整数 `1` —— 所以裸的 `#define FOO` 是个开关 |
| 值整个是整数字面量的标识符 | 那个整数（`-3` 也算） |
| 其他已定义的标识符 | 它的值，作为字符串 |

- 字符串之间的 `==` / `!=` 是逐字比较，**大小写敏感**。
- 字符串和整数比较、对字符串做算术或比大小、把字符串当真值（`#if DS_PLATFORM`）都是 `DSH1040`，从不静默当成
  真或假。版本门槛请用整数的 `DS_ENGINE_MAJOR` / `DS_ENGINE_MINOR`，不要比较 `DS_PLUGIN_VERSION` 字符串。
- `/` 和 `%` 向零截断；除以零是 `DSH1041`。
- `#define` 的值是名字之后到行尾的**文本**（先去掉行尾 `//` 注释），从不展开、从不求值：`#define PP_SUM 1 + 1`
  是字符串 `"1 + 1"`，不是 `2`。没有宏，也没有 `#include`。

```c
#if DS_ENGINE_MAJOR > 5 || (DS_ENGINE_MAJOR == 5 && DS_ENGINE_MINOR >= 7)
```

<a id="define-从哪来"></a>

### define 从哪来

五层，按顺序解析，后一层覆盖前一层 —— **只有 `Builtin` 永远不会输**。名字大小写敏感，必须符合
`[A-Za-z_][A-Za-z0-9_]*`。

| # | 层 | 来源 |
| --: | :-- | :-- |
| 1 | `Builtin` | 下面的 `DS_` 常量，只读 |
| 2 | `Settings` | 项目设置 *DreamPlugin ▸ Dream Shader ▸ Compiler ▸ Preprocessor Defines*（写进 `DefaultEngine.ini`，随项目入库） |
| 3 | `Registered` | C++ 的 `UE::DreamShader::RegisterDreamShaderDefine()` |
| 4 | `Provider` | C++ 的 `RegisterDreamShaderDefineProvider()`，每次解析时现取 |
| 5 | `CommandLine` | commandlet 的 `-Define=NAME=VALUE`（简写 `-D=`，可重复；不带值就是空值） |

| 内建常量 | 类型 | 值 |
| :-- | :-- | :-- |
| `DS_ENGINE_MAJOR` / `DS_ENGINE_MINOR` / `DS_ENGINE_PATCH` | 整数 | 引擎版本 |
| `DS_SUBSTRATE` | 整数 | 开了 Substrate 是 `1`，否则 `0`（取自只读 CVar `r.Substrate`） |
| `DS_PLATFORM` | 字符串 | ini 平台名，例如 `"Windows"` —— 编辑器和 cook 时一样 |
| `DS_PLUGIN_VERSION` | 字符串 | 插件版本，取不到时是 `"unknown"`，永远有定义 |

`DS_` 是**前缀**保留：源文件里 `#define` / `#undef` 一个 `DS_` 开头的名字是 `DSH1039`；项目设置和 `-Define=`
里的这种条目带警告丢弃，`RegisterDreamShaderDefine` 返回 `false`。这个判定大小写敏感，`ds_foo` 可以随便用。

<a id="define-只在本文件有效"></a>

### `#define` 只在本文件有效

`#define` **只对写它的那个文件可见**，传不到导入它的文件，也传不到之后导入的文件 —— 这不是 C。每个文件（包括
被导入的 header）都各自对着这次编译的 define 表单独预处理，然后才解析。正因为预处理先于解析，`#if` 才能包住
`import`：分支没选中，那条 `import` 根本不会被读取。

```c
#if DS_SUBSTRATE
import "Shared/SubstrateHelpers.dsh";
#else
import "Shared/LegacyHelpers.dsh";
#endif
```

要集中定义开关，请用为此准备的渠道：项目设置的 *Preprocessor Defines*、C++ 注册，或一次性构建用的 `-Define=`。

<a id="后果"></a>

### 后果

- **没选中的分支完全不检查。** 它不被词法分析、不被解析、不被类型检查。没人构建的分支会悄悄腐烂：分支要短，
  并且对你实际发布的每一种 define 组合各跑一次带 `-Force` 的 commandlet。
- **行数守恒。** 每个指令行和被裁掉的行都输出成空行，所以诊断仍然报在你写的那一行。
- **依赖图读原始文件。** 它不求值任何条件，一个文件的依赖是**所有分支的并集**：改一个只有死分支导入的 `.dsh`，
  依赖它的源文件照样会重建。
- **构建键只记录真正读到的 define**（没定义的记为 `<undef>`），改一个 define 只重建读过它的源文件。改 define 表
  也会让编辑器里的 Ephemeral 材质重建。
- **含指令的源文件不能 Adopt**（`DSH8149`）：资产里只有选中的那个分支，从它改写源文件会删掉其余分支。Revert
  和 Detach 不受影响。VirtualFunction 启动同步同样拒绝这种文件（`DSH9001`）。函数体里的 HLSL `#if` 和
  `#Region` 不算指令。
- **反编译永远不会输出指令** —— 条件编译是单向语法。
- **裁掉整个 `Shader` 块不会删掉它的资产**，只是不再构建；一个块都不剩的 `.dsm` 是 `DSH2254`。
- 预处理器失败会在解析之前终止编译，报为 `DSH8291`，消息里带预处理器自己的 `DSH103x` 码和出错指令的位置；
  它在第一个错误处停下。

```c
Shader(Name="Materials/M_Foo", Root="Game")
{
    Settings = {
        Domain = "Surface";
#if !DS_SUBSTRATE
        ShadingModel = "DefaultLit";
#endif
    }

    Properties { vec3 Tint = vec3(0.8, 0.2, 0.1); }

    Outputs = {
#if DS_SUBSTRATE
        Substrate Surface;
        Base.FrontMaterial = Surface;
#else
        vec3 BaseColor;
        Base.BaseColor = BaseColor;
#endif
    }

    Graph = {
#if DS_SUBSTRATE
        Surface = Substrate.Slab(DiffuseAlbedo = Tint, Roughness = 0.4);
#else
        BaseColor = Tint;
#endif
    }
}
```

<a id="示例"></a>

## 示例

```c
// 顶层声明之前的行注释。
Shader(Name="Materials/M_Comments")
{
    /* 跨多行的
       块注释。 */
    Settings = {
        Domain = "UI";        // 行尾注释
        ShadingModel = Unlit; // 引号可选
    }

    Properties {
        // 类型和名字之间可以用任意空白。
        ScalarParameter Rough = 0.5 [Group="Surface"; Slider(0, 1)];
        vec3            Tint  = vec3(1.0, 0.4, 0.1);
        float           Fudge = 1.0f;   // 读作 1.0
    }

    Outputs {
        vec3	Color;                  // 制表符也是空白
        Base.EmissiveColor = Color
    }                                   // 块内最后一个 ';' 可省略

    Graph {
        vec2 UV = UE.TexCoord(Index = 0);
        Color = vec3(Rough, Rough, UV.x) * Tint;
    }
}
```

<a id="诊断"></a>

## 诊断

每条诊断都带它所指构造的文件、行和列，打印成
`<file>(<line>,<column>): DSHnnnn: <message>` （自 2.0.0 起）；只有少数关于整个文件的诊断（比如无法解析的 import）报在第 1 行。1.x 只有以
`near index {Index}` 结尾的消息才带位置。

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH2101: Unexpected character '{0}' in source.` | 一个不属于任何 token 的字符：ASCII 之外的字符、$、@、反引号、全角标点、孤零零的 '，或者看不见的不换行空格。 | 删掉这个字符；看起来正常的字符多半是看不见的那种，重打这一行。 |
| `DSH2102: Unterminated block comment; expected a closing '*/'.` | 一个 /\* 之后没有 \*/。块注释不嵌套，所以注释掉一段本身含 /\* … \*/ 的代码会提前结束。 | 闭合注释。报告的位置是开头的 /\*，那才是该看的地方。 |
| `DSH2103: Unterminated string literal; expected a closing '"'.` | 一个双引号在本行结束前没有闭合。字符串永不跨行；以反斜杠结尾的路径也会走到这里。 | 闭合字符串；路径用正斜杠，或把反斜杠写成两个。 |
| `DSH2104: Unknown escape sequence '\{0}' in a string literal.` | 字符串里的反斜杠后面不是六种已知转义之一 —— 几乎总是单反斜杠的 Windows 路径。 | 把反斜杠写成两个，或者干脆用正斜杠。 |
| `DSH2105: Malformed number literal '{0}'.` | 看起来像数字但不是：1.2.3、1abc、1e、1.0u、1fu。 | 按语言的拼法写数字：1.0f、0x10、2u、.5、1e-3。 |
| `DSH2106: A '#' directive must be the first thing on its line.` | 一个 # 不是它所在行的第一个非空白字符。 | 把指令写在单独一行。 |
| `DSH2240: Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace, VirtualFunction or import, found {0}.` | 顶层出现了不是 1.x 块的东西 —— 包括大小写写错的块关键字。 | 检查块关键字的大小写，以及上方有没有多余的 token 或没闭合的块。 [详解](blocks.md) |
| `DSH2150: Unexpected end of file while parsing {0}.` | 文件在一个没写完的结构中间结束了，几乎总是前面有个没配对的 {、( 或 \[。 | 闭合消息里点名的结构；报告位置是文件末尾，不是出错的地方。 |
| `DSH2253: '{0}' is read as 'import'; write it in lower case.` | import 写成了别的大小写。 | 写小写的 import。 [详解](import.md) |
| `DSH3254: Expected a default value after '{0} =', found {1}.` | 属性的 = 后面没有前端能读成默认值的东西，或者默认值不是该类型能读的形式。 | 写成数字、构造器、true / false 或资产引用，或者去掉 =。 [详解](sections.md) |
| `DSH2222: Expected a decimal number in a 1.x Graph expression, found the hexadecimal literal '{0}', which 1.x read as 0 followed by a name; move this code to a .dss file.` | 1.x 的 Graph 表达式里出现了十六进制字面量，1.x 把它静默读成 0。 | 写十进制。 [详解](../graph/unsupported.md) |
| `DSH1035: {0}({1}): unknown preprocessor directive '#{2}'. {3}` | 函数体之外的 # 行既不是八个小写指令之一，也不是 #Region / #EndRegion —— #IF、#endfi、声明层的 #include 都在其中。 | 修正拼写或大小写；包含文件写 import；HLSL 指令放进 Function 函数体。 |
| `DSH1030: {0}({1}): this '#if' is never closed; the file ends with {2} conditional block(s) still open.` | 一条 #if 链在 #endif 之前文件就结束了。函数体里的指令不计数，所以不能在函数体里闭合外面的链。 | 在该结束的地方补上 #endif。 |
| `DSH1031: {0}({1}): '#endif' without a matching '#if'.` | 没有打开的链时出现了 #endif，常见原因是 #if 写在了 Function 函数体里。 | 删掉多余的 #endif，或让两端在花括号的同一侧。 |
| `DSH1032: {0}({1}): '#{2}' without a matching '#if'.` | 没有打开的链时出现了 #elif 或 #else。 | 先用 #if / #ifdef / #ifndef 打开链。 |
| `DSH1033: {0}({1}): '#{2}' after the '#else' on line {3}, which already closed this chain.` | #else 之后又出现了 #elif 或第二个 #else。 | 把多出来的分支挪到 #else 之前，或另起一条链。 |
| `DSH1034: {0}({1}): invalid '{2}' condition: {3}` | 条件表达式没写完：括号没闭合、运算符缺操作数。 | 补全表达式。 |
| `DSH1036: {0}({1}): '#{2}' requires a define name.` | #ifdef、#ifndef、#define 或 #undef 后面什么都没写。 | 写上 define 名。 |
| `DSH1037: {0}({1}): '#{2}' nesting is deeper than the limit of {3}.` | 条件链嵌套超过 64 层。 | 用 &amp;&amp; 合并内层条件，或拆分文件。 |
| `DSH1038: {0}({1}): '#{2}' needs a name made of letters, digits and underscores and not starting with a digit; got '{3}'.` | 指令后的名字不是标识符。 | 改成符合 \[A-Za-z\_\]\[A-Za-z0-9\_\]\* 的名字。 |
| `DSH1039: {0}({1}): '{3}' is a read-only built-in constant, so '#{2}' cannot change it. The 'DS_' prefix is reserved by DreamShader.` | 源文件试图 #define 或 #undef 一个 DS\_ 开头的名字。 | 换一个不带 DS\_ 前缀的名字；要模拟别的引擎或平台，定义你自己的开关。 |
| `DSH1040: {0}({1}): type mismatch in '{2}' condition: {3}` | 字符串和整数比较，或对字符串做算术、比大小、当真值。 | 同类比较：给 define 一个数值，或只用 == / != 比较两个字符串。 |
| `DSH1041: {0}({1}): the right operand of '{2}' in this '{3}' condition is zero.` | / 或 % 的除数为零 —— 未定义的名字读作 0 是常见原因。 | 定义这个名字，或用 defined(X) &amp;&amp; X != 0 &amp;&amp; … 护住。 |
| `DSH1042: {0}({1}): '{2}' is already complete before '{3}'. Nothing may follow a directive but a '//' comment.` | 指令已经完整，后面还有 token —— #if 1 2、#ifdef A B、#endif LABEL。 | 删掉多余的 token，或改成 // 注释。 |
| `DSH8291: '{0}' failed conditional compilation: {1}: {2}` | 预处理器拒绝了这个文件；消息里带它自己的 DSH103x 码。header 单独预处理，所以这条码指明是哪个文件。 | 看里面那个码。记住 #define 只在本文件有效。 |

数字被消费者拒绝时的其他码 —— `DSH3258`（`SortPriority`）、`DSH3275`（`Layout` 坐标）—— 见
[Section](sections.md) 和 [Layout 与 #Region](layout.md)。

<a id="继续阅读"></a>

## 继续阅读

- [顶层块](blocks.md) — 块关键字 —— 大小写敏感的那组 token。
  - [Section](sections.md) — 每个 section 的语句形式。
  - [类型与值](types.md) — 完整的类型 token 目录及其分上下文的有效性。
  - [不支持的写法](../graph/unsupported.md) — 1.x 的 Graph 表达式拒绝哪些写法。
