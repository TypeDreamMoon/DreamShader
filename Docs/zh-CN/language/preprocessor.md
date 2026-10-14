---
title: "预处理器"
description: "预处理器"
---

<a id="preprocessor"></a>

# 预处理器

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **预处理器**

`#if` / `#ifdef` / `#ifndef` / `#elif` / `#else` / `#endif`，外加 `#define` 和 `#undef`：
在源码被解析之前、在生成阶段求值的面向行的指令。未被采用的分支永远不会到达解析器，也永远不会成为节点。

| | |
| :-- | :-- |
| 声明于 | `.dsm`, `.dsf`, `.dsh` |
| 种类 | 指令 |
| 处理者 | 预处理器，逐文件处理，在文件被解析**之前**——头文件在被导入时也单独处理 |
| 识别方式 | 逐行识别，基于每个文件自身的文本，在 `Function` 主体之外 |
| 大小写规则 | 八个关键字**仅匹配小写** |
| 起始版本 | `1.9.0` |

<a id="synopsis"></a>

## 概要

```c
#if      <expr>
#ifdef   <NAME>        // exactly #if  defined(NAME)
#ifndef  <NAME>        // exactly #if !defined(NAME)
[#elif   <expr>]…
[#else]
#endif

#define  <NAME> [<value>]
#undef   <NAME>
```

一条指令占据整行。允许有前导空白，允许在 `#` 之后有空白，允许有一个尾随的 `//` 注释。该行上的其他所有内容都属于这条指令。

`#ifdef` 和 `#ifndef` 只是语法糖——它们在其他任何处理发生之前就被脱糖为 `#if defined(NAME)` 和 `#if !defined(NAME)`，此后行为完全相同，包括它们对[构建键](#rebuilds)的贡献。之所以存在它们，是因为拼写决策是“匹配 HLSL”，而 HLSL 作者会凭肌肉记忆写出它们。

<a id="what-this-is-for"></a>

## 用途

`#if` 做了[静态开关](../../parameters/parameter-nodes.md)无法做的一件事：切开**声明**层。

静态开关是一个节点。它存在于 `Graph = { … }` 内部，并在两个值之间进行选择。当材质两个构建之间的差异不是一个值，而是一个*声明*时——一个不同的 `ShadingModel`、一组不同的 `Outputs`、一个不同的 `import`、一整块根本不该存在的 `ShaderFunction` 块——静态开关就无处立足。在 Substrate 下，材质可能需要上述全部四项，而任何开关节点的排列都无法表达其中任何一项。

`#if` 是文本，所以它可以切开其中任何内容。

<a id="choosing-between-if-and-a-static-switch"></a>

### 在 `#if` 和静态开关之间选择

| | `#if` | `StaticSwitchParameter` / `StaticBool` |
| :-- | :-- | :-- |
| 求值时机 | 在生成阶段，基于源码文本 | 在材质的着色器排列被编译时 |
| 可在什么之间选择 | HLSL 主体之外任意位置的两段行 | `Graph` 主体内的两个表达式 |
| 可触及 `Settings`、`Outputs`、`Properties`、`import`、整个代码块 | **是** | 否 |
| 由谁选择 | 项目的定义表——每次编译一个答案 | 材质实例——每个实例一个不同的答案 |
| 未采用分支的结果 | 不存在；它不在资产中，因此永远不会被验证 | 在资产中，作为节点，被编译进各自的排列 |
| 更改答案的代价 | 重新构建每个读取该定义的文件 | 在实例上勾选一个复选框 |
| 美术师可见性 | 否——资产只显示被采用的分支 | 是——作为参数可见，另一个分支仍留在图表中 |

把这张表读成一条规则：**`#if` 是项目范围、构建时的决策；静态开关是每个实例、面向美术师的决策。**

- 由环境决定，且差异在于声明 → `#if`。Substrate 开或关、引擎版本门控、平台特定的 `Outputs` 块。
- 应由某人调整材质实例来决定 → 静态开关。始终如此。`#if` 无法从实例翻转，而项目范围地翻转它会重新构建每个读取它的资产。
- 差异是 `Graph` 内的一个表达式，且所有使用方都会选择同一分支 → 两者都可行；优先选择静态开关。它不需要注册定义，并且保留一个仍可被要求执行另一件事的资产。

> [!WARNING]
> 不要为了压低排列数量而使用 `#if`。那是静态开关的 `StaticSwitchParameter` 在实例层面已经做的事；用 `#if` 替换它会用一个排列换来一次重新构建，并夺走使用材质者的选择权。

<a id="directives"></a>

## 指令

<a id="recognition"></a>

### 识别

每个物理行都会被检测。未通过任何一条规则的行不是指令，会原样传递给解析器。

| # | 规则 |
| :-- | :-- |
| 1 | 该行必须在每个 `Function` / `GraphFunction` 主体之外——见[下文](#function-and-graphfunction-bodies-are-the-shader-compilers) |
| 2 | 修剪后的行不得以 `//` 开头 |
| 3 | 第一个非空白字符必须是 `#` |
| 4 | `#` 之后可以有空白——`#  if FOO` 与 `#if FOO` 是同一条指令 |
| 5 | 关键字必须是 `if` `ifdef` `ifndef` `elif` `else` `endif` `define` `undef` 之一，并以**小写**拼写 |
| 6 | `#if` / `#elif` 需要一个表达式；`#ifdef` / `#ifndef` / `#undef` 需要一个名称；`#define` 接受一个名称外加一个可选值直到行尾；`#else` / `#endif` 不接受任何内容 |
| 7 | 在操作数之后，只能有空白和一个 `//` 注释——其他任何内容都是 `DSH1042`。`#define` 豁免，因为它没有操作数末尾可检查 |
| 8 | `#if` 块可嵌套，深度为 64 |
| 9 | `#else` 结束整条链——其后不得再有 `#elif`，也不得有第二个 `#else` |

因此，把一条指令注释掉的效果与它看起来完全一致：根据规则 2，`// #if FOO` 是普通注释行。

规则 7 中的尾随注释剥离是引号感知的，因此字符串字面量中的 `//` 不会提前结束指令：`#if DS_HOST == "http://build"` 会与整个 URL 进行比较。

> [!WARNING]
> **规则 7 无条件适用于 `#else` 和 `#endif`**，无论它们是否位于被切掉的分支内——它们属于*链*，而非分支，正如 `DSH1032` / `DSH1033` 配对检查所做的那样。它要纠正的习惯来自 C：在 C 中，长链会在底部加标签：
>
> ```c
> #endif MOONTOON_LEGACY     // DSH1042 — MOONTOON_LEGACY 是多余标记
> #endif // MOONTOON_LEGACY  // 正确
> ```
>
> `#define` 是唯一一条完全不做尾随检查的指令，因为它的值一直延伸到行尾。参见 [`#define` 值是文本](#define-values-are-text)。

<a id="what-is-not-a-directive"></a>

### 什么*不是*指令

在此功能存在之前，DreamShaderLang 已有两个其他的 `#` 构造，两者都不得被破坏。预处理器认领哪些 `#` 行，取决于**该行所在的位置**，然后取决于 **`#` 之后的单词**——与一个封闭列表进行匹配，不会仅凭形状就放行任何内容：

| 位置 | 在 `#` 之后 | 处理方式 |
| :-- | :-- | :-- |
| 在 `Function` / `GraphFunction` 体内（在 `.dss` 或 `.dsh` 中，则是 `/// @custom` 函数体内） | 任意内容 | **原样传递** — 不被识别、不配对、不计入嵌套深度，也永远不会出现在已触及集合中 |
| 外部 | `region` 或 `endregion`，无论**任何**大小写 | **传递**给解析器 |
| 外部，位于 `.dss` 或 `.dsh` 中 | `pragma` 或 `include` | **传递**给 2.0 解析器，它们属于该解析器的行 |
| 外部，位于 `.dsm` 或 `.dsf` 中 | `pragma nogeneration`、`pragma disable_warning`、`pragma disable_error` | *（2.1.2 之后）* **传递**给旧版解析器，该解析器在顶层接受这三者；参见 [NoGeneration](../generation/no-generation.md) 和 [禁用诊断](../language-v2/disable-diagnostics.md) |
| 外部 | 八个关键字之一，**小写** | 一条指令 |
| 外部 | 其他任何内容 | `DSH1035` |

区域行明确列出了这两个词，而不是笼统地放行某种更宽泛的形式，
而这正是两种语法之间互操作的全部内容：`#Region` 保留了它一直以来的
大小写不敏感匹配，除此之外没有任何东西能因此获得豁免。

最后一行正是这样做的目的。在声明层级，`#include` 确实是一个错误 ——
那里的正确拼写是 [`import`](import.md) —— 而像 `#ifdefined` 或 `#endfi` 这样的拼写错误绝不能
悄然通过，因为被吞掉的 `#endfi` 会让其下所有内容无条件启用。**大小写错误的指令之所以被报告，
出于同样的原因**：`#IF FOO` 和 `#Endif` 是 `DSH1035`，而不是
悄无声息的空行。

`DSH1035` 会根据你写的内容选择建议：

| 书写内容 | 建议 |
| :-- | :-- |
| 关键字的近似拼写 —— `#ifdefined`、`#endfi`、`#IF` | 正确的小写拼写 |
| `#include` | [`import "…"`](import.md)，即 DreamShaderLang 的拼写 |
| 其他任何内容 | 通用规则 —— 八个关键字，小写 |

`#Region` 是另一个戴着相似帽子的东西，两者从不会冲突：

| | `#if` 家族 | `#Region` / `#EndRegion` |
| :-- | :-- | :-- |
| 拼写 | 仅小写 —— `#IF` 是 `DSH1035` | **大小写不敏感**匹配，因此 `#Region` 和 `#REGION` 相同 |
| 读取者 | 预处理器，在解析之前 | 解析器，作为 `Graph` 体的一个标记 |
| 有效范围 | 在 `Function` / `GraphFunction` 体之外的任何行 | 仅在 `Graph = { … }` 内部 |
| 效果 | 从解析器看到的文本中移除行 | 命名一段语句，变成一个注释框 |
| 空行规则 | 指令行会作为空行输出 | 原样传递 |
| 对 [Adopt](#a-source-with-directives-cannot-be-adopted) 而言是否算作指令 | 是 | **否** |

<a id="function-and-graphfunction-bodies-are-the-shader-compilers"></a>

### `Function` 和 `GraphFunction` 体属于着色器编译器

`Function` 或 `GraphFunction` 体是原始 HLSL，而 HLSL 有自己的预处理器。写在那里的指令
针对的是**那个**预处理器，带的是着色器编译器的定义 —— `PIXELSHADER`、
`MATERIALBLENDING_MASKED`、引擎自身环境 —— 这些 DreamShader 一概不知道，也不应去猜测。

```c
// The engine exposes blend mode only as a shader define, so this has to be a Custom node.
Function MoonToonBlendModeSwitch(in float3 Opaque, in float3 Masked, out float3 Result)
{
#if MATERIALBLENDING_SOLID
    Result = Opaque;
#elif MATERIALBLENDING_MASKED
    Result = Masked;
#else
    Result = 0;
#endif
}
```

**DreamShader 的预处理器不会进入那些体。** `Function` 签名的 `{` 与其匹配的
`}` 之间的每一行都会原样传递，`#` 行也不例外：它们不被识别为指令，
不与外部任何东西配对，不计入嵌套深度，也不对已触及集合有任何贡献。上述这些行会原封不动地到达生成的
`.ush`，并在材质的着色器编译时被解析。

> [!IMPORTANT]
> 这条边界区分了可正常工作的函数和悄无声息出错的函数。对照 DreamShader 的定义表求值，
> `MATERIALBLENDING_SOLID` 根本就是未定义的，于是它读取到 `0`，每个 `#elif` 都读取到 `0`，
> `#else` 分支无条件胜出，函数干净地编译通过，却为每种混合模式返回错误的值。没有诊断会发现它，
> 因为并没有出错 —— 是错的预处理器作出了回答。
>
> 这不是假设。在整个源码树中，**每一个**位于 `.dsm`、
> `.dsf` 或 `.dsh` 中的 HLSL `#` 指令 —— 九个文件、六条 `#include` 和三条 `#if` / `#elif` / `#else` / `#endif`
> 链 —— 都位于 `Function` 体内，无一例外。正是不透明区域让它们正常工作。
>
> 这条规则反过来讲也值得说明：**在 `Function` 体内，你无法编写
> DreamShader `#if`。** 那里的条件 HLSL 是在*着色器*编译期生效的。要在生成期
> 于两个函数体之间选择，请把 `#if` 放在 `Function` 块本身周围，
> 即 DreamShader 能看到的地方：
>
> ```c
> #if DS_SUBSTRATE
> Function ApplyShading(in float3 C, out float3 R) { R = SubstratePath(C); }
> #else
> Function ApplyShading(in float3 C, out float3 R) { R = LegacyPath(C); }
> #endif
> ```

体的查找方式是：取闭合 `Function` / `GraphFunction` 签名的那个 `{` —— 它可能
跨越多行，也可能带有 `SelfContained` 或 `Inline` —— 然后扫描到其匹配的 `}`，
忽略注释和字符串字面量中的花括号。`GraphFunction` 以同样的方式被视为不透明，
因为它同样可以携带裸 HLSL，而且 DreamShader `#if` 在 HLSL 体中从来就没有用处：
HLSL 自己的预处理器已经在那里了。

<a id="conditional-expressions"></a>

## 条件表达式

<a id="grammar"></a>

### 语法

```c
expr    := or
or      := and     ( '||' and )*
and     := equ     ( '&&' equ )*
equ     := rel     ( ('==' | '!=') rel )*
rel     := add     ( ('<' | '<=' | '>' | '>=') add )*
add     := mul     ( ('+' | '-') mul )*
mul     := unary   ( ('*' | '/' | '%') unary )*
unary   := ('!' | '-' | '+')? primary
primary := '(' expr ')'
         | 'defined' ( '(' IDENT ')' | IDENT )
         | IDENT
         | INT
         | STRING
```

`&&` 和 `||` 会**短路**：当左侧已经决定结果时，右侧就不会被求值，因此只有右侧才会读取的定义
根本不会被读取。

<a id="values"></a>

### 值

值有两种：64 位有符号**整数**和**字符串**。没有浮点数，
也没有布尔值 —— 当条件求值为非零整数时即为真。

| 表达式 | 求值为 |
| :-- | :-- |
| 整数字面量 | 其本身；十进制，或 `0x` / `0X` 十六进制 |
| 带引号的 `"…"` 字面量 | 该字符串 |
| `defined(X)` 或 `defined X` | 当 `X` 已定义时为 `1`，未定义时为 `0` —— 其值不被查看 |
| **未定义**的标识符 | 整数 `0`，与 C 中一样 |
| 定义为**空**值的标识符 | 整数 `1` |
| 整个值就是一个整数字面量的标识符 | 该整数 |
| 其他任何已定义的标识符 | 其值，作为字符串 |

最后四行是**按这个顺序**尝试的，而空值行必须放在解析之前：空字符串无法解析为整数，所以若先测试它，就会让每个裸 `#define FOO` 标记都变成*字符串*——而 `#if` 中的字符串会报错，这就封死了最常见的那种写法。带符号字面量也算，因此 `#define FOO -3` 就是整数 `-3`，尽管语法里的 `INT` 是无符号的，而且 `-` 名义上是一元运算符。

| 运算 | 规则 |
| :-- | :-- |
| 两个整数之间的 `==` 和 `!=` | 数值 |
| 两个字符串之间的 `==` 和 `!=` | 字面量，且**区分大小写**——在 Windows 上 `DS_PLATFORM == "windows"` 为 false |
| 字符串与整数之间的 `==` 和 `!=` | `DSH1040`，而不是静默的 `false` |
| `<` `<=` `>` `>=`、算术、`!`、`&&`、`\|\|`，以及裸布尔测试 | 仅限整数——字符串操作数是 `DSH1040` |
| `/` 和 `%` | 像 C 和 HLSL 那样向零截断：`-7 / 2` 为 `-3`，`-7 % 2` 为 `-1` |
| `/` 和 `%` 除以零 | `DSH1041` |

> [!WARNING]
> **字符串永远不会被强制转换为真值。** `#if DS_PLATFORM` 是 `DSH1040`，而不是“因为它非空所以为
> true”。`!DS_PLATFORM` 也是如此，`DS_PLATFORM && X` 以及将
> 字符串与数字比较也是如此。静默地把字符串当作 true 或 false，正是平台门控最终在
> 所有地方都触发的根源，所以这里直接拒绝——请写 `DS_PLATFORM == "Windows"`。

> [!NOTE]
> `DS_PLUGIN_VERSION` 和 `DS_PLATFORM` 是字符串，所以 `DS_PLUGIN_VERSION >= "1.9.0"` 就是 `DSH1040`，
> 而不是版本测试。字符串只能与 `==` / `!=` 比较，不能与其他任何东西比较；
> 若要做版本门控，请使用整数形式的引擎定义。

<a id="examples"></a>

### 示例

```c
#if DS_SUBSTRATE
#elif defined(MOONTOON_LEGACY_TOON)
#elif DS_PLATFORM == "Windows"
#else
#endif
```

引擎版本门控需要版本的两个部分，否则它会把 `6.0` 读作比 `5.7` 更旧：

```c
#if DS_ENGINE_MAJOR > 5 || (DS_ENGINE_MAJOR == 5 && DS_ENGINE_MINOR >= 7)
```

<a id="define-values-are-text"></a>

### `#define` 值是文本

一个 `#define` 的值是**名称之后、直到行尾的一切内容**。它被存储为文本，
永远不会被分词、永远不会被展开、也永远不会被求值：

```c
#define PP_SUM  1 + 1      // the five-character string "1 + 1", NOT the integer 2
#define PP_C    5 // five  // the integer 5 — the trailing comment is stripped first
#define PP_NOTE two words  // the string "two words"
#define PP_MARK            // empty: a marker, which reads as the integer 1
```

因此 `#if PP_SUM == 2` 是 `DSH1040`——字符串对整数——而不是 `true`。这是 DreamShader 的
`#define` 有意比 C 更弱的一处：这里没有宏，只有供 `#if` 测试的
具名值。

值一直延伸到行尾这一点带来两个后果：

- **`#define` 是唯一没有尾部令牌检查的指令。** `#define A B C` 是合法的，
  其值为 `B C`。其他所有指令在遇到多余令牌时都会报告 `DSH1042`——包括 `#undef A B`，
  它可没有这样的借口。
- **`//` 的剥离发生在取值之前**，所以注释永远不会落入值中。它是
  感知引号的，因此 `#define PP_URL "http://x"` 会保留其完整字符串。

完全没有名称的 `#define` 或 `#undef` 是 `DSH1038`，即名称诊断——`DSH1036` 是保留给
`#if` 系列缺失操作数的情况。

<a id="where-defines-come-from"></a>

## 定义从哪里来

五个层级，按以下顺序解析。较后的层级会覆盖较前的层级——**但 `Builtin` 除外，
它永远不会被覆盖。**

| # | 层级 | 设置来源 | 胜过 |
| --: | :-- | :-- | :-- |
| 1 | `Builtin` | 下面的 `DS_` 环境事实 | 一切——此层级是只读的 |
| 2 | `Settings` | [项目设置](../settings/project.md)中的*预处理器定义* | 无——最低层级 |
| 3 | `Registered` | 来自 C++ 的 `RegisterDreamShaderDefine()` | `Settings` |
| 4 | `Provider` | 一个定义提供者委托，在解析时重新拉取 | `Registered` |
| 5 | `CommandLine` | [命令小程序](../tools/commandlet.md)上的 `-Define=NAME=VALUE` | 除 `Builtin` 之外的一切 |

名称**区分大小写**，与 C 和 HLSL 一致：`Foo` 和 `FOO` 是两个不同的定义。名称必须匹配
`[A-Za-z_][A-Za-z0-9_]*`。

<a id="the-builtin-ds_-constants"></a>

### 内置的 `DS_` 常量

| 名称 | 类型 | 值 | 取自 |
| :-- | :-- | :-- | :-- |
| `DS_ENGINE_MAJOR` | 整数 | 引擎主版本号——`5` | `DREAMSHADER_UE_MAJOR` |
| `DS_ENGINE_MINOR` | 整数 | 引擎次版本号——`3` … `8` | `DREAMSHADER_UE_MINOR` |
| `DS_ENGINE_PATCH` | 整数 | 引擎热修复版本号 | `DREAMSHADER_UE_PATCH` |
| `DS_SUBSTRATE` | 整数 | 启用 Substrate 时为 `1`，否则为 `0` | `r.Substrate` CVar |
| `DS_PLATFORM` | 字符串 | ini 平台名称，例如 `"Windows"` | `FPlatformProperties::IniPlatformName()` |
| `DS_PLUGIN_VERSION` | 字符串 | 插件的 `VersionName`，例如 `"1.9.0"` | 插件描述符；不可用时为 `"unknown"` |

其中三个来源是经过选择而非显而易见的，每一种选择都在行为中可见：

- **引擎版本来自 DreamShader 自己的宏，而不是 `ENGINE_MAJOR_VERSION`。** 它们
  与插件 C++ 的 `DREAMSHADER_UE_VERSION_AT_LEAST` 保护所读取的是同一批宏，因此一个为了
  兼容性测试而覆盖它们的分支只会得到一个答案，而不是两个相互矛盾的答案。
- **`DS_PLATFORM` 是*ini* 平台名称，绝不是 `PlatformName()`。** 后者会合并
  编辑器/服务器/客户端，在编辑器中给出 `WindowsEditor`，而在烘焙期间给出 `Windows`——这样同一个
  来源会在烘焙的两端以两种不同方式预处理，编辑器展示的资产将不会是最终发布的
  资产。
- **`DS_PLUGIN_VERSION` 始终有定义**，会回退到 `"unknown"` 而不是变成未定义。
  否则 `defined(DS_PLUGIN_VERSION)` 可能因与
  源码毫无关系的原因而翻转，并连带改变构建键。

这张表中的一切都是**运行进程的不变量**，这是要求而非巧合。定义在生成时求值一次，
其效果随后被烘焙进保存的资产；一个会在会话中途改变的值会让构建不可复现，
并使资产记录的构建键成为谎言。`r.Substrate` 之所以合格，仅仅因为它是只读 CVar，在
启动时固定。

> [!WARNING]
> **`DS_` 前缀是作为前缀保留的，而不是作为一个列表保留。** 任何内容都不得定义、重定义或
> 取消定义以 `DS_` 开头的名称，无论 DreamShader 当前是否提供该名称。
> 拒绝的报告方式取决于尝试来自何处，因为只有其中一种情况
> 有文件和行号可供指向：
>
> | 尝试来自 | 结果 |
> | :-- | :-- |
> | 源文件中的 `#define DS_FOO` 或 `#undef DS_FOO` | 编译失败并报 `DSH1039` |
> | `RegisterDreamShaderDefine("DS_FOO", …)` | 返回 `false`，记录错误，表不变 |
> | *预处理器定义*，或 `-Define=DS_FOO=…` | 该条目被丢弃并发出警告；编译继续 |
>
> 采用前缀规则而非固定列表，是为了防止未来的内置项被某些项目已在该名称下注册的
> 定义悄悄覆盖。
>
> 前缀测试本身**区分大小写**，所以 `ds_foo` 和 `Ds_Foo` 是普通名称，可以
> 自由定义——它们不会与内置项冲突，因此没有什么需要保护。该检查
> 必须区分大小写也并非可有可无：若非如此，`ds_substrate` 就会通过保留
> 测试，然后落入 `DS_SUBSTRATE` 的槽位，从后门覆盖一个只读内置项。

<a id="project-settings"></a>

### 项目设置

*项目设置 ▸ DreamPlugin ▸ Dream Shader ▸ Compiler ▸ **预处理器定义*** 是一个
`TMap<FString, FString>`，写入项目的 `DefaultEngine.ini`。它是签入层级：
描述*此项目*的开关，由克隆该项目的所有人共享。

```ini
[/Script/DreamShader.DreamShaderSettings]
+PreprocessorDefines=(("MOONTOON_LEGACY_TOON", "1"))
+PreprocessorDefines=(("TOON_QUALITY", "2"))
```

编辑该映射会递增定义修订号，从而使 Ephemeral 材质失效——参见
[重建](#rebuilds)。

<a id="from-c"></a>

### 从 C++

两个入口点，都是 `UE::DreamShader` 中的自由函数，声明于
`Public/DreamShaderDefineResolution.h`（`DreamShader` 模块——表类型本身位于
`DreamShaderLang`，但构建一个是引擎的工作）。

**直接注册**是最简单的一种。`SourceTag` 标识贡献者，因此整个
集合可以在关闭时撤回：

```cpp
#include "DreamShaderDefineResolution.h"

void FMoonToonModule::StartupModule()
{
    using namespace UE::DreamShader;

    RegisterDreamShaderDefine(TEXT("MOONTOON"),         TEXT("1"), TEXT("MoonToon"));
    RegisterDreamShaderDefine(TEXT("MOONTOON_VERSION"), TEXT("3"), TEXT("MoonToon"));
}

void FMoonToonModule::ShutdownModule()
{
    UE::DreamShader::UnregisterDreamShaderDefinesFrom(TEXT("MoonToon"));
}
```

`RegisterDreamShaderDefine` 对于无效或保留名称返回 `false`，并且不触碰表。
从同一标签注册同一名称两次会覆盖。

**提供程序**是默认应该使用的方式。直接注册必须发生在
首次编译之前，这使其成为模块加载顺序的问题；提供程序在
表被解析的那一刻被拉取，因此加载顺序不再重要，并且值可能依赖于在 `StartupModule` 时
尚未就绪的状态：

```cpp
#include "DreamShaderDefineResolution.h"

FDelegateHandle Handle = UE::DreamShader::RegisterDreamShaderDefineProvider(
    UE::DreamShader::FDreamShaderDefineProviderDelegate::CreateLambda(
        [](UE::DreamShader::FDreamShaderDefineTable& InOutTable)
        {
            const int32 Quality = GetMoonToonQualityLevel();
            InOutTable.Set(
                TEXT("TOON_QUALITY"),
                FString::FromInt(Quality),
                UE::DreamShader::EDreamShaderDefineSource::Provider,
                TEXT("MoonToon"));
        }));

// ... at shutdown
UE::DreamShader::UnregisterDreamShaderDefineProvider(Handle);
```

提供程序计算出的任何内容仍然必须遵守不变性规则：每次编译询问一次，
其答案被烘焙到资产中。

还有两个函数值得了解。`ResolveDreamShaderDefines()` 返回一次编译
将看到的表，所有五个层级已经合并——对于记录构建实际使用了什么很有用。
`GetDreamShaderDefineRevision()` 会在每个层级的每次变更时递增；任何根据定义集缓存编译
输出的内容都会比较它并在其变化时失效。

> [!NOTE]
> **对于任何以定义名称为键的内容，请使用 `FDreamShaderDefineMap` / `FDreamShaderDefineValueMap`，而不是普通的 `TMap<FString, …>`。**
> 两个别名都在其键函数中固定了 `ESearchCase::CaseSensitive` 键匹配，因为默认 `TMap<FString, …>` 并不保证关于大小写的唯一答案：
> `FString` 没有类内 `operator==`，`Equals` 在此引擎树中已将其默认值改为区分大小写，
> 而 `GetTypeHash(FString)` 仍然是大小写*不敏感*的 `Strihash`。将其
> 留给容器会使大小写敏感性——一条语言规则——变成引擎版本细节，因此
> 针对 5.6、5.7 和 5.8 构建的插件将拥有三种不同的语言。
>
> `EDreamShaderDefineSource::SourceFile`，即源文件中 `#define` 的枚举项，列在
> 五个层级之后，**不参与排序**。这些定义只存在于预处理器返回时丢弃的
> 表私有副本中，在那里它们只是在那个文件的剩余部分中覆盖
> 注入层级所说的任何内容。对保留名称的 `#define` 仍然
> 被 `Set()` 拒绝，这正是引发 `DSH1039` 的原因。

<a id="command-line"></a>

### 命令行

[commandlet](../tools/commandlet.md) 接受 `-Define=NAME=VALUE`，或短选项 `-D=NAME=VALUE`。
**该选项可重复**——每次出现都会添加一个名称——并且它是最明确的层级，因此
其优先级高于除内置项之外的所有内容。

```powershell
-run=DreamShader compile -All -Force `
  -Define=MOONTOON_LEGACY_TOON=1 `
  -Define=TOON_PROFILE=cinematic `
  -D=CI_BUILD
```

`-Define=CI_BUILD` 不带 `=VALUE` 会将名称定义为空值——`defined(CI_BUILD)` 为 `1`
且 `#if CI_BUILD` 为 true。参见[值](#values)。

<a id="define-is-file-local"></a>

## `#define` 是文件局部的

一个 `#define` **仅在写入它的文件中**可见。它不会到达导入
该文件的文件，也不会到达在其之后导入的文件。这不是 C。

```c
// Shared/Common.dsh
#define TOON_FANCY 1
```

```c
// Materials/M_Toon.dsm
import "Shared/Common.dsh";

#if TOON_FANCY        // ← 0. The header's #define is not visible here.
```

原因在于工作顺序。每个文件在被解析之前，都会**独自**根据
编译的定义表进行预处理：先是导入文件，当读取其 `import` 时再处理头文件
*（自 2.0.0 起；在 1.9.x 及之前，头文件的文本当时被内联）*。这正是让
`#if` 能够包裹 `import` 的原因——当其分支被剪除时，`import` 根本不会被读取：

```c
#if DS_SUBSTRATE
import "Shared/SubstrateHelpers.dsh";
#else
import "Shared/LegacyHelpers.dsh";
#endif
```

当某个头文件的声明到达导入文件时，它自己的 `#define` 已经消失，并且没有任何内容
被粘贴到该文件的文本中供后续的 `#if` 查看。让 `#define` 像 C 那样行事将意味着
按包含顺序对每个文件进行一次预处理器遍历，以及 C 的依赖于包含顺序的宏状态，
而后者是数十年来 bug 的来源，并不令人怀念。

**要集中定义一个开关，请使用为此构建的通道**：项目设置中的*预处理器定义*，
从 C++ 使用的 `RegisterDreamShaderDefine`，或用于一次性构建的 `-Define=`。源文件中的 `#define`
用于该文件内的局部缩写，仅此而已。

> [!NOTE]
> 未采用的分支中的 `#define` 或 `#undef` 什么都不做，正如该分支的其余部分
> 什么都不做一样。指令行会与其余所有内容一起被剪除。

<a id="consequences"></a>

## 后果

前四项改变的是你的工作方式，而不仅仅是能编译什么。在编写第一个
`#if` 之前请阅读它们；后两项解释了为什么工具链的其余部分在剪除后仍能继续工作。

<a id="an-inactive-branch-is-not-checked-at-all"></a>

### 非活动分支完全不会被检查

文本在解析器看到它之前就被剪除，因此未采用的分支从不进行词法分析、从不
解析、从不进行类型检查，也从不生成。它可以包含任何内容——无效语法、
调用一个不存在的函数、一个在两个引擎版本前已被移除的 `ShadingModel`——
而编译仍然通过。

这是 C 的交易，也是剪除声明层的代价：让 `#if` 能够移除一行 `Settings` 的同一机制，
也让它无法检查该行。

实际后果是**当前没人构建的分支会悄悄腐烂。** 两个习惯
有帮助：

- 保持分支简短。围绕三行 `Settings` 的 `#if` 在其损坏时会被注意到；
  围绕两百行的 `#if` 则不会。
- 编译你实际发布的每一种组合。针对每个定义集运行一次 commandlet，配合 `-Force`，是
  一个恰好能捕获此类问题的 CI 门禁——参见[命令行](#command-line)。

<a id="a-source-with-directives-cannot-be-adopted"></a>

### 包含指令的源文件无法被采用

[**采用到源文件中**](../generation/divergence.md#adopt-into-source) 会从资产当前内容重写源文件。资产只保存剪切后的*结果*——即实际走到的分支，没有任何记录表明曾经存在分支。因此，用它重写源文件会删除文件中的所有指令，并静默地将一个条件材质替换为磁盘上碰巧存在的那个构建。

所以，只要源文件中包含任何指令——无论是否走到、是否有效，且包括其唯一指令是 `#define` 的文件——Adopt 就会以 `DSH8149` 拒绝执行。

有两样东西**不算**指令，这两者实践上都很重要。`#Region` 不是预处理器指令。`Function` 主体内的 `#if` 也不算：门禁应用与预处理器本身相同的不透明区域规则，因此其 HLSL 主体根据 `MATERIALBLENDING_SOLID` 或 `PIXELSHADER` 进行分支的材质函数仍然可以被采用。这些文件不包含 DreamShader 条件，拒绝它们会让那些从未使用该功能的源文件为此付出代价。

另外两种解决分歧的方式不受影响，因为它们都不会写入源文件：

| 操作 | 源文件中包含指令时 |
| :-- | :-- |
| **还原到源文件** | 可用——丢弃手工编辑并从头根据源文件重建，包括所有条件 |
| **采用到源文件中** | **被拒绝**，`DSH8149` |
| **从 DreamShader 分离** | 可用——移除标记并停止管理该资产 |
| **VirtualFunction 同步** | **被拒绝**，`DSH9001`——见下文 |

要将手工编辑带回条件源文件，请使用 *Export DSM* 并手动将更改合并到正确的分支中。

<a id="virtualfunction-sync-refuses-the-same-sources"></a>

### VirtualFunction 同步会拒绝相同的源文件

[启动同步服务](../../tools/virtual-function-tools.md#startup-sync-service) 会就地重写 `VirtualFunction` 声明，并且会以 `DSH9001` 拒绝包含指令的源文件。其原因比 Adopt 的更尖锐，风险也更大。

同步会使用记录的**字节**偏移量将新文本拼接到声明上。行数守恒只能保证行对齐，仅此而已：被剪切的行会变成*更短的*空行，因此第一个剪切点之后的每个字节偏移量都会指向别处。契约中没有可用于纠正的偏移量映射，而如果预处理后的字符串曾经到达文件写入器，它将**永久**压平文件中的每一个死分支。

比起 Adopt，它更糟的一点是没人要求它这样做：Adopt 是用户点击的菜单项，而同步在桥接启动时无人值守地运行，并遍历项目中每个可写的源文件。

> [!NOTE]
> 只有**写入**路径会拒绝。同一扫描器的只读调用方——上下文菜单、*打开 Virtual Function*、复制调用示例——会继续读取原始文本，因此它们看到的是所有分支的并集。这与依赖图遵循相同的规则，原因也相同。
>
> 拒绝还首先通过 `VirtualFunction` 子字符串测试来缩小范围，因此同步永远不会触及的条件材质不会在每次编辑器启动时都产生错误。

<a id="the-decompiler-never-emits-a-directive"></a>

### 反编译器永远不会输出指令

反编译读取资产，而资产保存一个图——即实际走到的分支。其中没有任何内容可以用来重建 `#if`。

**条件是单向语法。** `.dsm` → 资产保留它；资产 → `.dsm` 则不会。经过[反编译器](../tools/decompiler.md) 的往返始终返回一个条件已被压平移除的源文件，这不过是换种方式表达了 `DSH8149` 所说的内容。

<a id="line-numbers-and-what-the-dependency-graph-sees"></a>

### 行号，以及依赖图看到的内容

两个不变量使工具链的其余部分能够在剪切后继续正常工作。

**输出的行数与输入完全相同。** 每一行指令和每一行被剪切的行都作为*空*行输出，而不是被移除。每个诊断都携带其构造在自身文件中的行和列——头文件中的诊断在头文件中——编辑器的错误标记会读取它们（请参阅 [`import`](import.md#source-line-mapping)），因此即使源文件有一百行被剪切，仍然会报告在您编写它们的行上。

**依赖图读取原始文件，而不是预处理后的文件。** 它会扫描 `import` 行而不评估任何条件，因此文件的依赖是**所有分支的并集**。编辑一个只有死分支导入的 `.dsh` 仍然会将其依赖项标记为需要重建。

这是有意为之，而且方向保守：比严格必要更频繁地重建只会耗费时间，而未能重建会产生与源文件不匹配的资产。

<a id="rebuilds"></a>

### 重建

[构建键](../generation/caching.md#it-is-a-build-key-not-just-a-source-hash) 纳入了源文件**实际读取**的宏定义，而不是整张定义表。更改一个宏定义会重建读取它的源文件，并跳过不读取它的源文件。

| 读取了什么 | 记录为 |
| :-- | :-- |
| 一个已定义的宏定义 | 它的值 |
| 一个在**未**定义时被读取的宏定义 | 哨兵值 `<undef>` |
| 一个只有短路操作数才会读取的宏定义 | 无——它未被读取 |
| 一个在被剪切分支内被提及的宏定义 | 无——见下文 |

哨兵值是最容易出错且代价高昂的部分：没有它，映射中不存在的名字就是其*后续定义*不会改变哈希的名字，而一个源文件在资产不重建的情况下开始走不同分支就是静默数据丢失。

尽管如此，省略被剪切分支内的宏定义仍然是可靠的。预处理是确定性的，第 *k* 个被评估条件的位置只取决于它之前 *k*−1 个条件的结果，所以如果每个记录的值都未改变，输出就是逐字节相同的。死分支内的宏定义不会产生影响，直到杀死该分支的条件发生变化——而那个条件**确实**被记录了。

> [!NOTE]
> 检验标准是**谁决定了这次读取**。同一个文件中较早的 `#define` 定义过的名字，或者那里的 `#undef` 移除过的名字，完全由源文本回答——而源文本已经在哈希中——所以不会被记录。其他每一次读取都由注入表回答并被记录，当表为空时记录哨兵值。后半部分是最容易自我说服的部分：`#if defined(FOO)` 而没人定义 `FOO` 之所以为假，*是因为表中没有 `FOO`*，所以跳过它就意味着以后注入 `FOO` 不会改变任何哈希、不会重建任何东西，并静默地走另一个分支。

更改定义表还会提升 ThinCustom Ephemeral 材质所依据的修订版本，因此编辑器会重建它们，而不是在屏幕上留下在先前宏定义集下生成的材质。否则，编辑器显示的内容和命令行程序写入磁盘的内容可能会在没有可见原因的情况下不一致。

<a id="notes"></a>

## 注释

- **指令是通过逐行扫描发现的，因此块注释不会隐藏指令。** `#if` 内的 `/* … */` 仍然是一条指令。可通过在每行使用 `//` 来注释掉指令。`import` 则不同
  *(自 2.0.0 起)*：它是一个词元，因此 `/* … */` 内的一个会被注释掉。请参阅
  [`import`](import.md#recognition) 和 [词法元素](lexical.md#comments)。
- **`#if` 可在 `Graph` 体内工作。** `Graph` 体是 DreamShaderLang，而非 HLSL，因此其
  条件指令是 DreamShader 的。`Function` 和 `GraphFunction` 体则是例外，而且是
  唯一的例外——请参阅[上文](#function-and-graphfunction-bodies-are-the-shader-compilers)。
- **切除整个 `Shader` 块不会删除其资产。** 文件只是不再构建
  它，而先前生成的资产会留在磁盘上。完全没有块的 `.dsm` 是
  [`DSH2254`](../diagnostics/DSH2xxx.md#dsh2254)。静默删除资产远比
  留下一个要危险得多；请自行删除它，或者只保护范围更小的内容而不是整个块。
- **通过 `defined(X)` 读取的定义会像任何其他读取一样被记录。** 因此稍后定义 `X`
  会重建那些查询过它的源文件，包括那些只查询过它
  是否存在的源文件。
- **DreamShader 的预处理器没有 `#include`，也没有宏展开。** `#define` 的值是一个
  供 `#if` 测试的值；它绝不会被替换到源文本中，而且没有
  类函数宏。请使用 [`import`](import.md) 来引入另一个文件——在
  `.dsm` / `.dsf` 的声明层级写入 `#include` 是 `DSH1035`，并且消息会这样说明。1.x 文件确实拥有的 `#include`
  是 `Function` 体内的 HLSL 指令，而预处理器在其处根本不会查看；它会被提升到生成的 `.ush` 中，并由着色器编译器解析。请参阅
  [函数 ▸ 包含](function.md#includes)。
- **没有指令的源文件会逐字节返回。** 行尾符不会被改动，也不会添加
  末尾换行符，因此将文件送入预处理器绝不会成为其
  显示为已修改的原因。
- **嵌套层级在跳过的分支中也会被计数。** 嵌套在假 `#if` 下的 `#if` 仍会添加一个
  层级，因为扫描器必须配对它并未求值的指令。六十四层是
  合法的；第六十五层是 `DSH1037`。
- **解析器从不会看到指令。** 当声明语法运行时，每条指令
  行都是空行，这就是为什么这八个关键字没有一个出现在
  [关键字索引](keywords.md) 中作为解析器知道的内容。
- **`.dsh` 内容规则会应用于被切除的文本。** 解析器逐
  个声明地决定它 *(自 2.0.0 起；1.x 会在文本中搜索 `Shader(`)*，在指令被
  解析之后，因此被切除分支内的 `Shader` 块不再存在于那里以供拒绝——而
  被*采用*分支内的一个则是 [`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249)。因此，一个头文件
  可以携带位于永远不会为真的 `#if` 之后的 `Shader` 块，这是合法的，而且几乎
  肯定是个错误。请参阅[源文件](source-files.md#how-the-restriction-is-enforced)。

<a id="diagnostics"></a>

## 诊断

*(自 2.0.0 起)* 预处理器故障会在文件被解析之前停止编译，并报告
为 [`DSH8291`](../diagnostics/DSH8xxx.md#dsh8291)，其消息会指明文件——当失败的
是头文件时，则指明该头文件——并在下面携带预处理器自身的代码，以及
违规指令的文件和行号。预处理器会在其第一个错误处停止。

| 代码 | 原因 |
| :-- | :-- |
| `DSH1030` | 到达文件末尾时仍有一个 `#if` 未闭合 |
| `DSH1031` | `#endif` 没有匹配的 `#if` |
| `DSH1032` | `#elif` 或 `#else` 没有匹配的 `#if` |
| `DSH1033` | `#elif` 或在 `#else` 之后又出现第二个 `#else` |
| `DSH1034` | 条件表达式**不完整或格式错误** |
| `DSH1035` | `Function` 体之外的 `#` 行，它既不是八个全小写关键字之一，也不是 `#Region` / `#EndRegion`——在 `.dsm` / `.dsf` `#include` 和 `#pragma` 中也是如此，但三个文件级 pragma 除外 *(2.1.2 之后)*——还有 `#endfi`，以及大小写拼写错误的 `#IF` |
| `DSH1036` | `#if` 或 `#elif` 后面没有表达式 |
| `DSH1037` | 嵌套深度超过 64 层——第 65 个 `#if` 就是失败的那一个 |
| `DSH1038` | `#define` 或 `#undef` 缺少名称，或者名称不是 `[A-Za-z_][A-Za-z0-9_]*` |
| `DSH1039` | 试图定义或取消定义保留的 `DS_` 名称 |
| `DSH1040` | 在需要整数的地方使用了字符串：与数字比较、算术、`!`、`&&`、`\|\|`，或裸布尔测试 |
| `DSH1041` | 除以零或对零取模 |
| `DSH1042` | 表达式或名称**已完整解析**，但后面仍有词元 |
| `DSH8149` | 对包含指令的文件执行 **Adopt Into Source**——请参阅[上文](#a-source-with-directives-cannot-be-adopted) |
| `DSH9001` | 对包含指令的文件执行 **VirtualFunction sync**——请参阅[上文](#virtualfunction-sync-refuses-the-same-sources) |

`DSH1034` 和 `DSH1042` 依据一个机械问题来区分——*表达式是否结束了？*——不涉及
任何判断：

| 写法 | 代码 | 原因 |
| :-- | :-- | :-- |
| `#if (1` | `DSH1034` | 括号永远不会闭合 |
| `#if 1 &&` | `DSH1034` | `&&` 没有右操作数 |
| `#if 1 &&)` | `DSH1034` | `)` 不是有效操作数，因此表达式在那里未完成地停止 |
| `#if 1 2` | `DSH1042` | `1` 已经是一个完整表达式 |
| `#if 1)` | `DSH1042` | 同样——`)` 是多余的 |
| `#ifdef A B` | `DSH1042` | `A` 是有效名称；`B` 是多余的 |
| `#undef A B` | `DSH1042` | 相同 |
| `#else junk` | `DSH1042` | `#else` 完全不接受操作数 |
| `#define A B C` | *无* | `#define` 取值直到行尾 |

这种区分依据的是**读者看到的内容**，而不是哪个组件抛出了错误。`#ifdef A B` 会脱糖为
`#if defined(A) B`，因此任何会给糖形式和脱糖形式赋予不同代码的规则都会是
不连贯的。无论多余词元是 `B`、`2` 还是 `)`，它都是一个错误、一个修复方法：删除
它，或者将其注释掉。

完整文本和逐代码说明见 [DSH1xxx](../diagnostics/DSH1xxx.md)、
[DSH8xxx](../diagnostics/DSH8xxx.md) 和 [DSH9xxx](../diagnostics/README.md)。

<a id="example"></a>

## 示例

Substrate 案例，这正是该功能存在的原因。三个差异中有两个位于
声明层，任何切换节点都无法触及：`ShadingModel` 行是一个 `Settings` 键，而
这两个分支并未声明相同的 `Outputs`——不是相同的*名称*，甚至不是相同的
*类型*。一个 `Substrate` 值和一个 `vec3` 不是一个切换节点的两个输入；它们是两个
不同的材质。

```c
Shader(Name="Materials/M_Foo", Root="Game")
{
    Settings = {
        Domain = "Surface";
#if !DS_SUBSTRATE
        ShadingModel = "DefaultLit";
#endif
    }

    Properties {
        vec3 Tint = vec3(0.8, 0.2, 0.1);
    }

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

Substrate 分支完全不写入 `ShadingModel`。1.x 要求这样做：绑定 `Base.FrontMaterial`
强制设置着色模型，而那里的显式设置只能表示 `"Substrate"`。*（自
2.0.0 起）* 该绑定 [不设置任何着色模型](../builtins/substrate.md#binding-a-substrate-value-to-basefrontmaterial)
也不拒绝任何着色模型，因此现在对*另一个*分支的行进行保护只是将 `DefaultLit` 排除在
Substrate 构建之外。

当 `DS_SUBSTRATE` 位于 `1` 时，到达解析器的文本为：

```c
Shader(Name="Materials/M_Foo", Root="Game")
{
    Settings = {
        Domain = "Surface";



    }

    Properties {
        vec3 Tint = vec3(0.8, 0.2, 0.1);
    }

    Outputs = {

        Substrate Surface;
        Base.FrontMaterial = Surface;




    }

    Graph = {

        Surface = Substrate.Slab(DiffuseAlbedo = Tint, Roughness = 0.4);



    }
}
```

三十一行输入，三十一行输出。`Graph` 块内部的错误仍会报告在
作者撰写它所在的行上：`Substrate.Slab` 调用在两份文本中都是第 26 行。

<a id="see-also"></a>

## 另请参阅

- [布局](layout.md#region--endregion) — `#Region` / `#EndRegion`，另一种 `#` 语法
- [`import`](import.md) — `#if` 可以包裹的内容，以及空行所保护的行映射
- [函数](function.md#includes) — HLSL 主体，以及 `#include` 提升
- [词法元素](lexical.md) — 注释，以及块注释为何不会隐藏指令
- [源文件](source-files.md) — 指令可能出现的三种文件类型
- [项目设置](../settings/project.md) — *预处理器定义*
- [命令行工具](../tools/commandlet.md) — `-Define=NAME=VALUE`
- [缓存](../generation/caching.md) — 受影响的定义所并入的构建键
- [分歧](../generation/divergence.md) — 采用、还原和分离
- [VirtualFunction 工具](../../tools/virtual-function-tools.md) — 以 `DSH9001` 拒绝的启动同步
- [反编译](../tools/decompiler.md) — 无法往返转换的方向
- [参数节点](../../parameters/parameter-nodes.md) — `StaticSwitchParameter`，另一种分支方式
- [诊断索引](../diagnostics/index.md) — 按阶段列出的每条消息
