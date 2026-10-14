---
title: "DSH2xxx —— 词法分析器与语法"
description: "DSH2xxx —— 词法分析器与语法"
---

<a id="dsh2xxx-----lexer-and-syntax"></a>

# DSH2xxx —— 词法分析器与语法

> 生成标记之间的内容由 `.skill/gen-diagnostics.ps1` 编写。
> 标记下方的所有内容均为手动编写，并可在重新生成后保留。

<a id="dsh2101"></a>

## DSH2101

<!-- generated:begin DSH2101 -->
**严重性** 错误

**消息**

```
Unexpected character '{0}' in source.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:780`
<!-- generated:end DSH2101 -->

**原因。** 一个不属于任何 DreamShaderLang 标记的字符：一个孤立的 `@`、`$`、一个反引号、一个从文档中粘贴过来的全角标点符号，或浏览器复制时留下的不间断空格。词法分析器刻意仅支持 ASCII —— 一个西里尔字母 `а` 或一个全角 `Ａ` *不*是标识符字符，因为由这类字符构建的符号在编辑器中看起来正常，但在下游却无法匹配任何内容。

**修复。** 删除该字符。当消息显示一个看起来普通的字符时，它几乎总是不可见的那种：重新输入该行而不是编辑它，或在编辑器中开启“显示空白字符”。`@` 位于 `///` 块中是指令，没有问题 —— 此错误仅在代码上触发。

**注意。** 该字符仍会作为 `Unknown` 标记发出，因此解析器也会在该位置报告它所期望的内容；两条消息描述的是同一个拼写错误。

**参见** [DreamShaderLang 2.0](../language-v2/index.md)

<a id="dsh2102"></a>

## DSH2102

<!-- generated:begin DSH2102 -->
**严重性** 错误

**消息**

```
Unterminated block comment; expected a closing '*/'.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:353`
<!-- generated:end DSH2102 -->

**原因。** 一个 `/*` 后面没有 `*/`。块注释不会嵌套：第一个 `*/` 会关闭*最外层*注释，因此一个本身包含 `/* ... */` 的被注释区域会提前结束，该区域的尾部会变成代码 —— 而且更常见的是，之后的某个 `/*` 会一直延伸到文件末尾并落在这里。

**修复。** 关闭注释。报告的位置是**起始** `/*`，而不是文件末尾：那才是应该查看的位置，因为文件末尾无法说明哪个注释被遗留为未关闭。要注释掉一个本身已包含块注释的区域，请使用不含 `///` 的 `//` 行注释，或 `#if 0` … `#endif`，这些由预处理器处理。

**参见** [DreamShaderLang 2.0](../language-v2/index.md)

<a id="dsh2103"></a>

## DSH2103

<!-- generated:begin DSH2103 -->
**严重性** 错误

**消息**

```
Unterminated string literal; expected a closing '"'.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:507`
<!-- generated:end DSH2103 -->

**原因。** 一个 `"` 在其行结束前没有关闭的 `"`。DreamShaderLang 字符串永远不会跨行：这是一个刻意的限制，正是它防止一个缺失的引号吞掉文件的其余部分。有两种常见情形会在此出现 —— 真正遗忘的引号，以及以反斜杠结尾的路径（`"C:\Assets\"`），其中 `\"` 被读取为转义引号，字面量便继续延伸下去。

**修复。** 关闭字符串。在路径中，请使用正斜杠（`"/Game/Materials/M_A"`）或将反斜杠加倍（`"C:\\Assets\\"`）；DreamShaderLang 资源路径在所有地方都以 `/` 分隔。

**注意。** 直到行尾的文本仍会作为一个字符串标记发出，因此其周围的语句仍会被解析，文件中任何进一步的错误仍会被报告。

**参见** [DreamShaderLang 2.0](../language-v2/index.md)

<a id="dsh2104"></a>

## DSH2104

<!-- generated:begin DSH2104 -->
**严重性** 错误

**消息**

```
Unknown escape sequence '\{0}' in a string literal.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:487`
<!-- generated:end DSH2104 -->

**原因。** 字符串内的一个反斜杠后面跟着的字符不属于该语言所知的六种转义之一：`\\`、`\"`、`\n`、`\r`、`\t`、`\0`。几乎总是用单个反斜杠书写的 Windows 路径 —— `"D:\new\textures"` 包含 `\n` 和 `\t`（它们会静默地变成换行符和制表符）以及 `\x`，后者会落在这里。

**修复。** 将每个反斜杠加倍，或者 —— 更好的做法 —— 用正斜杠书写路径。这里没有 `\x41`、`\u0041` 或八进制转义：需要六种转义之外字符的字符串应将该字符按字面携带（源文件按 UTF-8 读取）。

**注意。** 反斜杠在解析后的值中会被**保留**，因此 `"\q"` 产生两个字符 `\q`。没有任何内容被静默丢失，经过打印器往返的源文件会原样返回。

**参见** [DreamShaderLang 2.0](../language-v2/index.md)

<a id="dsh2105"></a>

## DSH2105

<!-- generated:begin DSH2105 -->
**严重性** 错误

**消息**

```
Malformed number literal '{0}'.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:698`
<!-- generated:end DSH2105 -->

**原因。** 某个看起来像数字但不是数字的东西。会在此出现的形式：

| 书写形式 | 原因 |
| :-- | :-- |
| `1.2.3` | 两个小数点 |
| `0x` | 有十六进制前缀但没有数字 |
| `1e`、`1e+` | 有指数但没有数字 |
| `1fu`、`1uf` | 同一个字面量上同时有浮点后缀和整数后缀 |
| `1.0u`、`1e3l` | 带有小数部分或指数的值上使用了整数后缀 |
| `0x1h`、`0x2f` … `h`/`f`（十六进制之后） | 十六进制字面量上使用了浮点后缀 —— 十六进制始终是整数 |
| `2abc`、`1px` | 字母粘在数字末尾 |

最后一行最令人意外：`2abc` 是**一个**格式错误的数字标记，而不是 `2` 后跟标识符 `abc`。将其拆分会给解析器一个看起来合理的表达式，并掩盖真正的错误。

**修复。** 按该语言所拼写的方式书写字面量：十进制整数带可选的 `u`/`U`（无符号）或 `l`/`L`（长整型）后缀，十六进制写作 `0x…`（仅为整数），浮点数带 `.`、指数或 `f`/`F`/`h`/`H` 后缀。`1.0f`、`0x10`、`2u`、`.5`、`1e-3` 和 `2.0h` 都是有效的。当意图是成员访问或 swizzle 时，请将数字放在点前面：`x.5` 是成员访问，`2.5` 是浮点数。

**注意。** 一个格式错误的字面量仍会产生恰好一个标记，其值会被尽可能解析（`1e` 读作 1），因此周围的语句仍会被解析，文件的其余部分仍会报告各自的错误。

**参见** [DreamShaderLang 2.0](../language-v2/index.md)

<a id="dsh2106"></a>

## DSH2106

<!-- generated:begin DSH2106 -->
**严重性** 错误

**消息**

```
A '#' directive must be the first thing on its line.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLexer.cpp:382`
<!-- generated:end DSH2106 -->

**原因。** 一个 `#` 不是其物理行上第一个非空白字符 —— `int a = 1;
#pragma x`, or a `#` used as an operator (DreamShaderLang has no `#`, `##` 或 stringize 运算符）。前导空格和制表符没问题；该行上的其他任何内容都不行，这包括注释：`/* c */ #pragma x` **不是**指令行，因为注释是词法分析器已经看到过的东西。

**修复。** 将指令放在单独的一行。如果 `#` 本意是运算符，那么它不是 —— 请使用所意图内容的 DreamShaderLang 拼写（材质设置为 `#pragma material(...)`，包含为 `import "..."` 或 `#include "..."`）。

**注意。** `#` 会作为 `Unknown` 词法单元输出，并且词法分析会在同一行继续，因此其后的单词仍会被正常词法分析，该行上的任何第二个错误仍然会被报告。

**参见** [DreamShaderLang 2.0](../language-v2/index.md)

<a id="dsh2150"></a>

## DSH2150

<!-- generated:begin DSH2150 -->
**严重性** 错误

**消息**

```
Unexpected end of file while parsing {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParser.cpp:246`
<!-- generated:end DSH2150 -->

**原因。** 文件在某个尚未完成的结构中间结束：一个表达式、一个参数列表、一个初始化列表、一条语句或一个块。几乎总是文件中较早处存在不配对的 `{`、`(` 或 `[` —— 解析器一直在寻找闭合符号，但文本已经用完了。

**修复。** 闭合消息中所指名的结构。当消息说“一个块”时，请查找其 `}` 缺失的函数；报告的位置是文件末尾，而不是错误所在的位置。

<a id="dsh2151"></a>

## DSH2151

<!-- generated:begin DSH2151 -->
**严重性** 错误

**消息**

```
Expected an expression, found {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:544`、`Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:631`
<!-- generated:end DSH2151 -->

**原因。** 一个不能作为值开头的词法单元出现在了需要值的位置：一个多余的 `,` 或 `;`，一个用于开始语句的关键字（`if`、`return`），一个右括号，或是一行在较早处丢失了其 `;` 后残留的部分。

**修复。** 补上缺失的操作数，或删除多余的词法单元。当所指名的词法单元是关键字时，通常前一条语句是丢失了其 `;` 的语句。

<a id="dsh2152"></a>

## DSH2152

<!-- generated:begin DSH2152 -->
**严重性** 错误

**消息**

```
Expected ')' to close a cast, found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:253`、`Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:522`、`Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:620`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:159`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:259`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:294`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:341`
<!-- generated:end DSH2152 -->

**原因。** 一个不配对的 `(`。解析器读取了一个完整的表达式，然后遇到了并非其所应匹配的 `)` 的其他内容。

**修复。** 添加 `)`。当消息指名一个参数列表时，请检查是否在两个参数之间缺少 `,` —— `f(a b)` 会在 `b` 处报告此代码。

<a id="dsh2153"></a>

## DSH2153

<!-- generated:begin DSH2153 -->
**严重性** 错误

**消息**

```
Expected ']' to close an index, found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:325`
<!-- generated:end DSH2153 -->

**原因。** 某个索引表达式 `a[…` 缺少 `]`，或者某个索引因为开始了一个作者并非本意的表达式而吞掉了 `]`（`a[i, j]` —— 逗号运算符不属于 DreamShaderLang，因此 `,` 会结束索引）。

**修复。** 添加 `]`。多维索引写作 `a[i][j]`，而绝不是 `a[i, j]`。

<a id="dsh2154"></a>

## DSH2154

<!-- generated:begin DSH2154 -->
**严重性** 错误

**消息**

```
Expected ';' after the 'for' initializer, found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:223`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:244`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:346`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:372`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:387`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:401`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:415`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:464`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:600`
<!-- generated:end DSH2154 -->

**原因。** 一条没有以 `;` 结束的语句。与 1.x 不同，2.0 语法没有以换行结尾的语句形式：每条语句都以 `;` 或一个 `}` 结束。

**修复。** 添加 `;`。报告的位置是下一行的第一个词法单元，因此错误位于消息所指向行的上一行。

**恢复。** 丢失了 `;` 的语句会与其后直到下一个 `;` 的词法单元一起被丢弃，并且该块会继续解析 —— 一个缺失的分号只会导致一条语句的代价，而不是函数其余部分的代价。

<a id="dsh2155"></a>

## DSH2155

<!-- generated:begin DSH2155 -->
**严重性** 错误

**消息**

```
Expected '}' to close an initializer list, found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:672`
<!-- generated:end DSH2155 -->

**原因。** 一个缺少 `}` 的 `{ … }` 初始化器，或者一个并非表达式的元素（例如花括号内的一个 `;`）。

**修复。** 闭合该列表。元素之间用 `,` 分隔，并且允许在 `}` 之前有一个尾随的 `,`；嵌套列表写作 `{ { 1, 2 }, { 3, 4 } }`。

<a id="dsh2157"></a>

## DSH2157

<!-- generated:begin DSH2157 -->
**严重性** 错误

**消息**

```
Expected '(' after 'if', found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:148`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:193`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:283`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:330`
<!-- generated:end DSH2157 -->

**原因。** 某个控制流关键字缺少其带括号的头部。在 DreamShaderLang 中，与 HLSL 一样，`if`、`for`、`while` 和 `do … while` 的条件始终是带括号的。

**修复。** 写作 `if (cond)`、`for (init; cond; step)`、`while (cond)`、`do … while (cond);`。

<a id="dsh2158"></a>

## DSH2158

<!-- generated:begin DSH2158 -->
**严重性** 错误

**消息**

```
A positional argument cannot follow a named argument; give this argument a name too.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:603`
<!-- generated:end DSH2158 -->

**原因。** 一次调用以错误的顺序混合了两种参数形式 —— `Fn(A = 1, x)`。命名参数按名称绑定引脚，因此一旦某个参数被命名，后续参数的位置就不再承载读者可以信任的信息。

**修复。** 为其余参数命名（`Fn(A = 1, B = x)`），或将已命名的参数移到末尾（`Fn(x, A = 1)`，这是合法的）。一个确实是赋值的参数要写在括号中：`Fn((a = b), c)`。

<a id="dsh2159"></a>

## DSH2159

<!-- generated:begin DSH2159 -->
**严重性** 错误

**消息**

```
Expected ':' to complete the conditional operator, found {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:153`
<!-- generated:end DSH2159 -->

**原因。** 一个缺少其 `:` 的 `?`。通常是某个 `:` 被写成了其他类型，或者是某个条件表达式的中间操作数越过了 `:`，因为其中的某个括号不配对。

**修复。** 写作 `cond ? a : b`。该运算符是右结合的，因此一条链不需要括号：`a ? b : c ? d : e` 表示 `a ? b : (c ? d : e)`。

<a id="dsh2160"></a>

## DSH2160

<!-- generated:begin DSH2160 -->
**严重性** 错误

**消息**

```
Unsupported statement: the preprocessor line '#{0}' cannot appear inside a function body; mark the function /// @custom to hand its body to the shader compiler.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:134`、`Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:442`
<!-- generated:end DSH2160 -->

**消息（语句）** `Unsupported statement '{0}': DreamShaderLang 2.0 has no switch statement,
write if / else if instead.`

**消息（指令）** `Unsupported statement: the preprocessor line '#{0}' cannot appear inside a
function body; mark the function /// @custom to hand its body to the shader compiler.`

**原因。** 在语句层级出现 `switch`、`case` 或 `default`，或者在已解析的函数体内出现 `#` 行。两者都不属于该语言：2.0 会将函数体降级为材质图表，而图表没有跳转表；`#` 行本应在解析器看到它之前就被预处理器消费掉。

**修复。** 将 `switch` 改写为 `if` / `else if` 链。对于 `#` 行，要么将其移到文件作用域（`#pragma`、`#include` 是声明，而不是语句），要么在函数体确实是 HLSL 时，将函数标记为 `/// @custom` —— 此时其函数体会被原样捕获并原封不动地交给着色器编译器。

<a id="dsh2161"></a>

## DSH2161

<!-- generated:begin DSH2161 -->
**严重性** 错误

**消息**

```
Expected a member or swizzle name after '.', found {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:295`
<!-- generated:end DSH2161 -->

**原因。** 一个 `.` 后面没有跟标识符：一个数字（`v.1`）、一个关键字，或一个本应是浮点字面量一部分的 `.`（`1 .5`）。

**修复。** 写出成员名或 swizzle 名称。对解析器而言 swizzle 就是普通的成员访问，所以 `v.xy`、`v.rgb` 和 `Struct.Field` 都采用相同的形式。

<a id="dsh2162"></a>

## DSH2162

<!-- generated:begin DSH2162 -->
**严重性** 错误

**消息**

```
An initializer list is only allowed as a variable initializer, not as a general expression.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserExpressions.cpp:536`
<!-- generated:end DSH2162 -->

**原因。** 一个 `{ … }` 被用作值——作为调用实参、在 `return` 中，或位于运算符右侧。在 HLSL 中，如同在 C 中一样，花括号列表是一种初始化器的语法形式，而不是带类型的表达式。

**修复。** 声明一个变量并传入它，或使用构造函数：用 `float3(1, 2, 3)` 代替 `{ 1, 2, 3 }`。

<a id="dsh2163"></a>

## DSH2163

<!-- generated:begin DSH2163 -->
**严重性** 错误

**消息**

```
Expected a variable name, found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:572`
<!-- generated:end DSH2163 -->

**原因。** 一个声明，其声明符没有名称：重复的 `,`（`float a,, b;`）、位于 `=` 之前的 `,`（`float a, = 1;`），或一个类型拼写实际上本意是一个变量并遮蔽了该名称。

**修复。** 写出名称。声明的每个声明符都是 `Name`、`Name[dims]`、`Name = init` 或 `Name[dims] = init`。

<a id="dsh2164"></a>

## DSH2164

<!-- generated:begin DSH2164 -->
**严重性** 错误

**消息**

```
Expected 'while' after the body of a 'do' statement, found {0}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:324`
<!-- generated:end DSH2164 -->

**原因。** 一个 `do`，其 `while` 缺失或拼写错误。

**修复。** 写出 `do { … } while (cond);`——末尾的 `;` 是该语句的一部分，其缺失是 DSH2154，而不是此代码。

<a id="dsh2165"></a>

## DSH2165

<!-- generated:begin DSH2165 -->
**严重性** 错误

**消息**

```
Expected '{' to open a block, found {1}.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangParserStatements.cpp:479`
<!-- generated:end DSH2165 -->

**原因。** 需要一个块，却发现了其他内容。只有在调用方未先检查就请求一个块时才会遇到这种情况——函数体缺失 `{` 时会报告此错误。

**修复。** 用 `{` 打开该块。`extern` 函数完全没有函数体，以 `;` 结束；`/// @custom` 函数仍然需要其花括号，解析器只是不读取其中的内容。

<a id="dsh2200"></a>

## DSH2200

<!-- generated:begin DSH2200 -->
**严重性** 错误

**消息**

```
(built at runtime)
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:550`
<!-- generated:end DSH2200 -->

**原因。** 一个 1.x Graph 表达式在 `if` 条件之外使用了比较、逻辑运算符或 `?:`。1.x 表达式读取器认识 `+ - * /`、调用和成员；遇到其他任何内容时它会停止并沿用目前已读取的内容，因此 `a < b ? x : y` 会静默地变成 `a`。旧版前端会拒绝该文本，而不是重现这种行为。

**修复。** 将选择放入一个 `if` / `else`（1.x 会在那里进行比较），或使用诸如 `UE.If(...)` 之类的节点；或将代码移到 `.dss` 文件中，在那里这些运算符是存在的。

<a id="dsh2201"></a>

## DSH2201

<!-- generated:begin DSH2201 -->
**严重性** 错误

**消息**

```
(built at runtime)
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:546`
<!-- generated:end DSH2201 -->

**原因。** 一个 1.x Graph 表达式使用了 `%`、移位或按位运算符。1.x 没有这些：它在运算符处停止读取，并静默地丢弃表达式的其余部分。

**修复。** 对于取余使用 `fmod(a, b)`；对于任何按位运算，将代码移入 `Function`（HLSL）或移入 `.dss` 文件。

<a id="dsh2202"></a>

## DSH2202

<!-- generated:begin DSH2202 -->
**严重性** 错误

**消息**

```
Expected no '[ ]' in a 1.x Graph expression, found '{0}', where 1.x silently dropped the index and everything after it; use a swizzle such as '.r' or move this code to a .dss file.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:392`
<!-- generated:end DSH2202 -->

**原因。** 一个 1.x Graph 表达式使用 `[ ]` 对值进行索引。1.x 会一声不响地丢弃索引及其后的所有内容，因此图表从未做到文本所表达的事。

**修复。** 使用 swizzle（`.r`、`.xy`）获取某个分量；要按编号读取节点的输出，请在调用中写 `OutputIndex = k`。真正的索引需要使用 `.dss` 文件。

<a id="dsh2203"></a>

## DSH2203

<!-- generated:begin DSH2203 -->
**严重性** 错误

**消息**

```
Expected a constructor such as 'float3(x)' in a 1.x Graph expression, found the cast '({0})', which 1.x never had; move this code to a .dss file.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:502`
<!-- generated:end DSH2203 -->

**原因。** 一个 C 风格的类型转换（如 `(float3)x`）出现在 1.x Graph 表达式中。1.x 只有构造函数。

**修复。** 写出构造函数 `float3(x)`，或将代码移到 `.dss` 文件。

<a id="dsh2204"></a>

## DSH2204

<!-- generated:begin DSH2204 -->
**严重性** 错误

**消息**

```
Expected an assignment only as a whole 1.x Graph statement, found one inside an expression; move this code to a .dss file.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:513`
<!-- generated:end DSH2204 -->

**原因。** 在 1.x Graph 表达式内部将赋值用作值（`a = (b = c)`，或赋值作为调用实参）。在 1.x 中，赋值是一整条语句。

**修复。** 将其拆分为两条语句，或将代码移到 `.dss` 文件。

<a id="dsh2205"></a>

## DSH2205

<!-- generated:begin DSH2205 -->
**严重性** 错误

**消息**

```
Expected '=' in a 1.x Graph assignment, found '{0}', which 1.x read as the declaration of a variable named '{1}'; write 'x = x + y' or move this code to a .dss file.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:316`
<!-- generated:end DSH2205 -->

**原因。** 一个复合赋值（`+=`、`*=`……）出现在 1.x Graph 主体中。1.x 不知道这些运算符：它会把 `x += y` 读作变量声明，将 `x` 作为其类型，并在很久之后才失败，或者根本不失败。

**修复。** 将其展开写出：`x = x + y;`。复合赋值在 `.dss` 文件中存在。

<a id="dsh2206"></a>

## DSH2206

<!-- generated:begin DSH2206 -->
**严重性** 错误

**消息**

```
Expected a variable or 'variable.member' on the left of a 1.x Graph assignment, found '{0}'.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:341`
<!-- generated:end DSH2206 -->

**原因。** 1.x Graph 赋值的左侧既不是变量也不是 `variable.member`。1.x 可以赋值给局部变量、输出，或 `Base` / 材质变量的某一个成员，除此之外不能赋值给其他任何内容（不能赋值给 swizzle 的 swizzle、索引或调用结果）。

**修复。** 赋值给一个普通变量，并在右侧用构造函数组合各部分。

<a id="dsh2207"></a>

## DSH2207

<!-- generated:begin DSH2207 -->
**严重性** 错误

**消息**

```
Expected no '++' or '--' in a 1.x Graph expression, found '{0}'; move this code to a .dss file.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:444`
<!-- generated:end DSH2207 -->

**原因。** `++` 或 `--` 出现在 1.x Graph 表达式中。1.x 没有此类运算符，也没有使用它们的循环。

**修复。** 写出 `x = x + 1.0;`，或将代码移到 `.dss` 文件。

<a id="dsh2208"></a>

## DSH2208

<!-- generated:begin DSH2208 -->
**严重性** 错误

**消息**

```
Expected a declaration, an assignment, a call or 'if' in a 1.x Graph body, found '{0}', which 1.x did not have; move this code to a .dss file.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:227`
<!-- generated:end DSH2208 -->

**原因。** 1.x 从未有过的语句开启了 Graph 主体的这一行：`for`、`while`、`do`、`return`、`break`、`continue` 或 `discard`。1.x Graph 主体是声明、赋值、调用以及 `if` / `else`。（`switch` 会更早一步被解析器以 `DSH2160` 拒绝。）

**修复。** 将循环和提前退出移入 `Function`（HLSL），或迁移该文件并在 `.dss` 中编写它们。

<a id="dsh2209"></a>

## DSH2209

<!-- generated:begin DSH2209 -->
**严重性** 错误

**消息**

```
Expected braces around the body of a 1.x Graph 'if', found a single statement.
```

**引发者** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:256`、`Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:281`
<!-- generated:end DSH2209 -->

**原因。** 1.x Graph `if` 或 `else` 的主体是一个不带大括号的单条语句，而 1.x
要求使用大括号。

**修复。** 在主体两侧加上 `{ }`，即使它只有一条语句。

<a id="dsh2210"></a>

## DSH2210

<!-- generated:begin DSH2210 -->
**严重性** 错误

**消息**

```
Expected at most one comparison in a 1.x Graph 'if' condition, found '{0}' as well, where 1.x silently dropped the rest of the condition; move this code to a .dss file.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:298`
<!-- generated:end DSH2210 -->

**原因。** 1.x Graph `if` 条件包含多个比较（`a > b && c < d`）。1.x 会读取
第一个比较并静默丢弃其余部分，因此分支仅基于条件的一半
执行。

**修复。** 嵌套两个 `if`，或先计算组合掩码再进行比较；`&&` 和 `||` 可在
`.dss` 文件中使用。

<a id="dsh2211"></a>

## DSH2211

<!-- generated:begin DSH2211 -->
**严重性** 错误

**消息**

```
Expected a call or an assignment as a 1.x Graph statement, found an expression whose value is never used.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:150`
<!-- generated:end DSH2211 -->

**原因。** 1.x Graph 语句是一个值无处可去的表达式，例如 `a + b;`。它会
构建无人读取的节点，这几乎总是丢失的赋值。

**修复。** 将该值赋给变量或输出，或删除该行。

<a id="dsh2212"></a>

## DSH2212

<!-- generated:begin DSH2212 -->
**严重性** 错误

**消息**

```
Expected no storage keyword on a 1.x Graph variable, found '{0}'; move this code to a .dss file.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:96`
<!-- generated:end DSH2212 -->

**原因。** 存储关键字（`static`、`const`、`uniform`）出现在 1.x Graph
主体内的变量上。1.x 局部变量没有这些关键字；常量和参数位于 `Properties` 中。

**修复。** 移除该关键字，或在 `Properties` 中声明该值。`.dss` 文件有 `const` 局部变量。

<a id="dsh2213"></a>

## DSH2213

<!-- generated:begin DSH2213 -->
**严重性** 错误

**消息**

```
Expected a single value in a 1.x Graph declaration, found the array declarator '{0}'; move this code to a .dss file.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:108`
<!-- generated:end DSH2213 -->

**原因。** 1.x Graph 变量被声明为数组。1.x 图没有数组值。

**修复。** 使用单独的变量，或将代码移到 `.dss` 文件或 `Function` 中。

<a id="dsh2214"></a>

## DSH2214

<!-- generated:begin DSH2214 -->
**严重性** 错误

**消息**

```
Expected an expression as a 1.x Graph initializer, found an initializer list; use a constructor such as 'float3(a, b, c)'.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:522`
<!-- generated:end DSH2214 -->

**原因。** 带大括号的列表出现在 1.x 不会读取它的位置：另一个列表内部。1.x 在声明中接受
`T x = {a, b, c};` 作为构造函数 `T(a, b, c)`，并接受 `T x = {};` 作为零，
此时类型就写在旁边；而嵌套在该列表中的列表没有类型可供构造。（作为赋值
值或作为参数的列表会更早被解析器拒绝，报为 `DSH2162`。）

**修复。** 写入构造函数：`float3(a, b, c)`。

<a id="dsh2215"></a>

## DSH2215

<!-- generated:begin DSH2215 -->
**严重性** 错误

**消息**

```
Expected an initializer on the 1.x Graph variable '{0}' of type '{1}', found none, and 1.x had no zero value for that type.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:1165`
<!-- generated:end DSH2215 -->

**原因。** 1.x Graph 变量没有初始化器，且其类型在 1.x 中没有零值：纹理、
Substrate 值或采样器。数字以零开始，`MaterialAttributes` 以空集开始。

**修复。** 为该变量提供初始化器。

<a id="dsh2216"></a>

## DSH2216

<!-- generated:begin DSH2216 -->
**严重性** 错误

**消息**

```
Expected a name after '#Region', found none.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:1023`
<!-- generated:end DSH2216 -->

**原因。** `#Region` 后面没有名称。该名称是该区域在图中
变成的注释框的标题。

**修复。** 写入 `#Region "Name"`。

<a id="dsh2217"></a>

## DSH2217

<!-- generated:begin DSH2217 -->
**严重性** 错误

**消息**

```
Expected a '#Region' before this '#EndRegion', found none open.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:171`
<!-- generated:end DSH2217 -->

**原因。** 在此 Graph 主体中，`#EndRegion` 之前没有打开的 `#Region`。

**修复。** 移除它，或添加它本应关闭的 `#Region "Name"`。

<a id="dsh2218"></a>

## DSH2218

<!-- generated:begin DSH2218 -->
**严重性** 错误

**消息**

```
Expected '#EndRegion' to close the region '{0}' before the end of the Graph body, found none.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:65`
<!-- generated:end DSH2218 -->

**原因。** 在 Graph 主体结束处，`#Region` 仍处于打开状态。

**修复。** 在主体 `}` 之前用 `#EndRegion` 关闭它。

<a id="dsh2219"></a>

## DSH2219

<!-- generated:begin DSH2219 -->
**严重性** 错误

**消息**

```
Expected '#Region' or '#EndRegion' as the only '#' line in a 1.x Graph body, found '#{0}'.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:1007`
<!-- generated:end DSH2219 -->

**原因。** 在 1.x Graph 主体内出现了 `#` / `#EndRegion` 之外的 `#Region` 行。
条件编译（`#if` ...）在解析器运行之前就已解决。在 `.dsm` / `.dsf` 中，
预处理器会在解析器看到之前拒绝它不认识的指令（`DSH1035`）；此代码
正是 `.dsh` 中的 1.x Graph 主体所得到的，其中 `#pragma` 和 `#include` 能通过预处理器，
因为该头文件也可能包含 2.0 声明。

**修复。** 将 `import` 行移到文件顶部；`#pragma` 属于 `.dss` 文件。

<a id="dsh2220"></a>

## DSH2220

<!-- generated:begin DSH2220 -->
**严重性** 错误

**消息**

```
Expected no bare block in a 1.x Graph body, found one; 1.x has blocks only after 'if' and 'else'.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:191`
<!-- generated:end DSH2220 -->

**原因。** 1.x Graph 主体中出现了裸 `{ ... }` 块。1.x 仅在 `if` 和
`else` 之后才有块，且其变量都处于同一作用域，因此单独的块毫无意义。

**修复。** 移除外层大括号。

<a id="dsh2222"></a>

## DSH2222

<!-- generated:begin DSH2222 -->
**严重性** 错误

**消息**

```
Expected a decimal number in a 1.x Graph expression, found the hexadecimal literal '{0}', which 1.x read as 0 followed by a name; move this code to a .dss file.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyStatements.cpp:364`
<!-- generated:end DSH2222 -->

**原因。** 1.x Graph 表达式中出现了十六进制字面量。1.x 数字读取器在 `0` 之后
停止，并将 `x1F` 作为名称读取，因此该值被静默地视为 0。

**修复。** 以十进制写入该数字。

<a id="dsh2240"></a>

## DSH2240

<!-- generated:begin DSH2240 -->
**严重性** 错误

**消息**

```
Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace, VirtualFunction or import, found {0}.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:506`
<!-- generated:end DSH2240 -->

**原因。** 在 `.dsm` / `.dsf` 文件顶部，或 `.dsh` 的 1.x 部分顶部，
有 1.x 块之外的内容开启了声明。

**修复。** 1.x 文件是 `Shader`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend`、
`Function`、`GraphFunction`、`Namespace`、`VirtualFunction` 块和 `import` 行的列表。检查
此行上方是否有游离的标记或未关闭的块。

<a id="dsh2241"></a>

## DSH2241

<!-- generated:begin DSH2241 -->
**严重性** 错误

**消息**

```
Expected '(' with the block's attributes, such as '(Name = "M_Example")', found {0}.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:555`
<!-- generated:end DSH2241 -->

**原因。** 块词后面没有其属性列表。每个 1.x 块至少在此处写明其
名称。

**修复。** 写入 `Shader(Name = "M_Example")`，并在其后用 `{` 打开主体。

<a id="dsh2242"></a>

## DSH2242

<!-- generated:begin DSH2242 -->
**严重性** 错误

**消息**

```
Expected a 'Name = "..."' attribute on '{0}', found none.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:776`
<!-- generated:end DSH2242 -->

**原因。** 该块有属性列表，但其中没有 `Name`。该名称是该块
构建的资产（或对于 VirtualFunction，是名称调用所使用的）。

**修复。** 添加 `Name = "..."`。

<a id="dsh2243"></a>

## DSH2243

<!-- generated:begin DSH2243 -->
**严重性** 错误

**消息**

```
Expected an attribute name such as 'Name', found {0}.
```

**由以下项引发** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:574`、`Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:583`、`Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:594`、`Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:645`
<!-- generated:end DSH2243 -->

**原因。** 块的属性列表不是以逗号分隔的 `Key = value` 对：缺少键、缺少 `=`，或值不是字符串、单词或数字。紧接 `)` 之前的逗号没有问题，与 1.x 中一样；但连续两个逗号则不行。

**修复。** 将每个属性写为 `Key = "value"`。

<a id="dsh2244"></a>

## DSH2244

<!-- generated:begin DSH2244 -->
**严重性** 警告

**消息**

```
The attribute '{0}' is written twice; the later value wins, as it did in 1.x.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:622`
<!-- generated:end DSH2244 -->

**原因。** 在块的属性列表中，某个属性被写了两次。1.x 保留后出现的值，此前端也是如此；之所以给出警告，是因为第一个值是死文本。

**修复。** 删除其中一个。

<a id="dsh2245"></a>

## DSH2245

<!-- generated:begin DSH2245 -->
**严重性** 错误

**消息**

```
Expected a Shader section (Properties, Settings, Outputs, Graph or Layout), found '{0}'.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2659`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:996`
<!-- generated:end DSH2245 -->

**原因。** 一个不是节的单词在块内开启了一行。Shader 有 `Properties`、`Settings`、`Outputs`、`Graph` 和 `Layout`；ShaderFunction、layer 或 blend 还有 `Inputs`，并对 `Outputs` 采用 `Results`；VirtualFunction 有 `Options`、`Inputs` 和 `Outputs`。`Code` 是 `DSH2246`。

**修复。** 检查拼写——与 1.x 一样，节名称匹配时忽略大小写，因此错的是字母——并检查它前面的节是否已关闭。

<a id="dsh2246"></a>

## DSH2246

<!-- generated:begin DSH2246 -->
**严重性** 错误

**消息**

```
Expected 'Graph' as the body section of '{0}', found 'Code', which 1.x accepted only inside a Function.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:988`
<!-- generated:end DSH2246 -->

**原因。** Shader 或 ShaderFunction 块包含 `Code` 节。1.x 仅在 `Function` 内读取 `Code`；在资产块中，主体是 `Graph`。

**修复。** 将节重命名为 `Graph`，或将 HLSL 移入 `Function` 并调用它。

<a id="dsh2247"></a>

## DSH2247

<!-- generated:begin DSH2247 -->
**严重性** 错误

**消息**

```
Expected no body in the VirtualFunction '{0}', which declares an existing asset, found the section '{1}'.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2651`
<!-- generated:end DSH2247 -->

**原因。** VirtualFunction 包含 `Graph` 或 `Code` 节。VirtualFunction 声明已存在资产的接口；它不构建任何内容。

**修复。** 移除主体；如果源代码旨在构建该资产，则将该块改为 `ShaderFunction`。

<a id="dsh2248"></a>

## DSH2248

<!-- generated:begin DSH2248 -->
**严重性** 错误

**消息**

```
Expected a 1.x block in a '.{0}' file, found {1}, which is 2.0 syntax; 2.0 declarations belong in a .dss file or a .dsh header.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:497`
<!-- generated:end DSH2248 -->

**原因。** 2.0 语法（`uniform`、`export` 函数、`#pragma`）出现在 `.dsm` 或 `.dsf` 文件中，而这类文件仅由 1.x 前端读取。

**修复。** 将 2.0 声明放入 `.dss` 文件，或放入 `.dsh` 头文件（它同时接受两种方言）。`dsc migrate` 会重写整个 1.x 文件。

<a id="dsh2249"></a>

## DSH2249

<!-- generated:begin DSH2249 -->
**严重性** 错误

**消息**

```
Expected only Function, GraphFunction, Namespace and VirtualFunction blocks in a '.dsh' header, found the asset block '{0}', which belongs in a .dsm or .dsf file.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:520`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:722`
<!-- generated:end DSH2249 -->

**原因。** `.dsh` 头文件中包含一个构建资产的块（`Shader`、`ShaderFunction`、layer）。头文件会被包含到其他文件中；其中的资产块会按包含它的每个文件各构建一次。

**修复。** 将该块移入它自己的 `.dsm` / `.dsf`，并在头文件中保留函数和 VirtualFunction。

<a id="dsh2250"></a>

## DSH2250

<!-- generated:begin DSH2250 -->
**严重性** 错误

**消息**

```
Expected one Shader block in a file, found a second one.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:750`
<!-- generated:end DSH2250 -->

**原因。** 一个文件包含两个 `Shader` 块。材质源就是一个材质。

**修复。** 让每个材质拥有自己的文件。

<a id="dsh2251"></a>

## DSH2251

<!-- generated:begin DSH2251 -->
**严重性** 警告

**消息**

```
'{0}' is the old spelling of '{1}'; it still reads the same.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:740`
<!-- generated:end DSH2251 -->

**原因。** 使用了较早 1.x 版本的块单词（以 `MaterialLayer` 表示 `ShaderLayer`，以 `MaterialLayerBlend` 表示 `ShaderLayerBlend`）。其读取结果相同。

**修复。** 无需更改；`dsc migrate` 会写出当前形式。

<a id="dsh2252"></a>

## DSH2252

<!-- generated:begin DSH2252 -->
**严重性** 错误

**消息**

```
Expected a double-quoted path after 'import', found {0}.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:380`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:392`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:409`
<!-- generated:end DSH2252 -->

**原因。** 前端不读取 `import` 行。有三种情况：`import` 后面不是带引号的路径（`"..."`，或同一行的 `'...'`，1.x 文件和 `.dsh` 仍接受这种写法）；该路径指向 `.dsf` 或 `.dsm`——材质或函数文件会自行编译，绝不被包含；或者该路径是根限定路径（`Project:Shared/Common.dsh`、`Plugin.X:...`），即 1.6.0 形式，`import` 不再接受。

**修复。** 使用相对于此文件或其自身源根的路径编写 `import "Shared/Common.dsh";`。位于另一根中的头文件可通过以 `/` 为锚的路径访问，`import
"/Shared/Common.dsh";`，它会先查找此文件的根，然后依次查找其他每个根——也可通过复制它或通过包来访问。参见 [import](../language/import.md#resolution)。

<a id="dsh2253"></a>

## DSH2253

<!-- generated:begin DSH2253 -->
**严重性** 警告

**消息**

```
'{0}' is read as 'import'; write it in lower case.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:371`
<!-- generated:end DSH2253 -->

**原因。** 对 `import` 使用了 `Import`（或其他大小写形式）。1.x 对该词的匹配较为宽松。

**修复。** 以小写形式书写 `import`。

<a id="dsh2254"></a>

## DSH2254

<!-- generated:begin DSH2254 -->
**严重性** 错误

**消息**

```
Expected a top-level Shader, ShaderFunction, ShaderLayer, ShaderLayerBlend, Function, GraphFunction, Namespace or VirtualFunction block in this 1.x file, found none.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:309`
<!-- generated:end DSH2254 -->

**原因。** 该文件完全没有块：文件为空、其中所有内容都被注释掉，或某个 `#if` 将其移除。

**修复。** `.dsm` 需要 `Shader`，`.dsf` 需要 `ShaderFunction`、`ShaderLayer` 或 `ShaderLayerBlend`。如果它是残留文件，请删除该文件。

<a id="dsh2255"></a>

## DSH2255

<!-- generated:begin DSH2255 -->
**严重性** 错误

**消息**

```
Expected a Graph section in the Shader '{0}', found none.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1042`
<!-- generated:end DSH2255 -->

**原因。** Shader 没有 `Graph` 节，其 `Outputs` 节中也没有任何内容，因此没有可构建的内容。没有 `Graph` 时，`Outputs` 语句必须自行计算值：已初始化的声明，或右侧为表达式的绑定。

**修复。** 添加 `Graph = { ... }`。

<a id="dsh2256"></a>

## DSH2256

<!-- generated:begin DSH2256 -->
**严重性** 警告

**消息**

```
The Shader '{0}' has no Outputs section, so nothing its Graph computes reaches the material.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:1049`
<!-- generated:end DSH2256 -->

**原因。** Shader 有 Graph 但没有 `Outputs` 节，因此没有任何值到达材质属性。材质可以构建，但没有任何内容接入它。

**修复。** 添加 `Outputs = { ... }`，其中至少包含一个 `Base.<Attribute> = <variable>;` 绑定。

<a id="dsh2257"></a>

## DSH2257

<!-- generated:begin DSH2257 -->
**严重性** 错误

**消息**

```
Expected '`{' to open the '{0}' block, found {1}.
```

**触发者** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2455`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2569`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2608`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:2625`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:809`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:882`, `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:899`
<!-- generated:end DSH2257 -->

**原因。** 在必须打开块的地方没有打开块：`Shader`、
`ShaderFunction`、`Namespace` 或 `VirtualFunction` 的属性列表后面没有 `{`，块内缺少节名称
（`Properties`、`Graph`、`Inputs`、……），或者节名称后面没有
`{`。

**修复。** 在同一行或下一行用 `{` 打开主体，并以各自名称开始每一节。

<a id="dsh2258"></a>

## DSH2258

<!-- generated:begin DSH2258 -->
**严重性** 警告

**消息**

```
The section '{0}' is written twice; the later one wins, as it did in 1.x.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:962`、`Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:976`
<!-- generated:end DSH2258 -->

**原因。** 一个节在块中被写了两次。1.x 会保留后一个并静默丢弃前一个；
此前端行为相同，但会说明这一点。

**修复。** 将两个节合并为一个。

<a id="dsh2259"></a>

## DSH2259

<!-- generated:begin DSH2259 -->
**严重性** 错误

**消息**

```
Expected ShaderFunction, ShaderLayer and ShaderLayerBlend blocks in a '.dsf' file, found a Shader block, which belongs in a .dsm file.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:758`
<!-- generated:end DSH2259 -->

**原因。** 一个 `.dsf` 中包含了 `Shader` 块。`.dsf` 构建函数资产——`ShaderFunction`、
`ShaderLayer`、`ShaderLayerBlend`——而材质应放在 `.dsm` 中。1.x 在生成文件时
也拒绝它。该块被跳过；文件的其余部分仍会被读取。

**修复。** 将 `Shader` 块移入 `.dsm`。`.dsm` 可以在其 `Shader` 旁保留 `ShaderFunction` 和层块，
因此将整个文件移到 `.dsm` 也可以。

<a id="dsh2260"></a>

## DSH2260

<!-- generated:begin DSH2260 -->
**严重性** 警告

**消息**

```
'{0}' is not a value of NoGeneration, which is true or false; the block is built as if it were false.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangLegacyParser.cpp:798`
<!-- generated:end DSH2260 -->

**原因。** 1.x 块的 `NoGeneration =` *（2.1.2 之后）* 所包含的不是 `true`、`false`、`1` 或 `0`。该块会按其值为 `false` 来构建。参见 [NoGeneration](../generation/no-generation.md)。

**修复。** 写入 `NoGeneration = true`，或删除该键。

<a id="dsh2300"></a>

## DSH2300

<!-- generated:begin DSH2300 -->
**严重性** 错误

**消息**

```
Expected '`{' to open the block of pass '{0}', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:240`
<!-- generated:end DSH2300 -->

**原因。** pass 的开头 `pass <Name> : <kind>` 后面没有打开其块的 `{`。pass 没有参数列表——它的设置是花括号内的语句，
不同于缓冲区的 `(Key = Value)` 列表——因此 `pass Blur : compute(Injection = AfterOpaque)` 就是此错误，
出现在 `(`。其他形式：在类别之后出现 `;` 或第二个词（`pass Blur : compute fullscreen`），以及
`{` 完全缺失，此时消息会指出块中第一个设置。

**修复。** 在类别之后立即打开块，可在同一行或下一行，并将所有设置
放入其中：

```hlsl
pass Blur : compute
{
    Injection = AfterOpaque;
    Shader    = "BoxBlur.usf";
    Entry     = BlurCS;
    write Result = Blurred;
}
```

没有 `{` 时，块中的行会被当作文件作用域声明读取，其中的 `}` 会被当作游离的
声明，因此会引发更多错误（DSH3205、对于 `Shader =` 行的 DSH3222、DSH3200）；它们源于此错误
并会随之消失。

<a id="dsh2301"></a>

## DSH2301

<!-- generated:begin DSH2301 -->
**严重性** 错误

**消息**

```
Expected a setting ('Key = Value;'), a 'read', 'write' or 'param' line or an 'hlsl' block in a pass block, found {0}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:334`
<!-- generated:end DSH2301 -->

**原因。** pass 块中的语句以任何语句都不会以其开头的内容开头。pass 包含设置（`Key = Value;`）以及 `read`、`write` 和 `param` 行，
它们全都以名称开头；而此处是关键字、数字、字符串或标点符号。典型形式：HLSL 习惯用法
例如 `in Source = Mask;` 或 `out Result = Blurred;`（`in` 和 `out` 是关键字）、写在 pass 内的 `uniform`
或 `static const` 声明、没有键的值（`"M_Mask";`），以及
重复的 `;`——pass 块没有空语句。`uniform` 行也可能表示其上方 pass 的 `}`
缺失。

**修复。** 将该行写为设置或绑定——`read Source = Mask;`、
`write Result = Blurred;`、`Material = "M_Mask";`——并删除多余的 `;`。声明应放在文件
作用域，位于 pass 之上，而 pass 通过 `param` 获取它们的值：

```hlsl
uniform float BlurRadius = 3.0;

pass Blur : compute
{
    ...
    param Radius = BlurRadius;
}
```

<a id="dsh2302"></a>

## DSH2302

<!-- generated:begin DSH2302 -->
**严重性** 错误

**消息**

```
Expected '=' after the key '{0}', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:447`
<!-- generated:end DSH2302 -->

**原因。** pass 语句以不是绑定词的名词开头，因此它被当作设置 `Key = Value;` 读取，而 `=` 不存在。
常见形式：

- 缺失或拼错的 `=`：`Injection AfterOpaque;`、`Injection: AfterOpaque;`、
  `Material("M_Mask");`；
- 大小写错误的绑定词：`Read Mask;`、`Write SceneColor;`——`read`、`write` 和
  `param` 为小写，设置键以大写开头；
- 没有绑定词的缓冲区：`Mask;` 而非 `read Mask;`；
- 块内的声明：`float Radius = 2;`。

`read`、`write` 和 `param` 本身始终会开始一个绑定，无论其后是什么，因此后面没有名称的绑定词
（`read;`、`read "Mask";`、`param = 3;`）属于 DSH2304 或 DSH2306，而不是此错误。

**修复。** 为设置提供其 `=`，为绑定提供其小写词和名称。pass 需要从管线获取的值，应是文件作用域的
`uniform` 或 `static const`，并通过
`param Radius = BlurRadius;` 传入。

<a id="dsh2303"></a>

## DSH2303

<!-- generated:begin DSH2303 -->
**严重性** 错误

**消息**

```
Expected ';' at the end of the pass statement, found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:458`
<!-- generated:end DSH2303 -->

**原因。** 完整的 pass 语句后面没有 `;`。最常见的情况是行尾缺少 `;`，
消息会指向下一行的第一个标记——而错误在上一行。其他形式：一条语句中有两个绑定（`read A, B;`）、
缓冲区后面有其他内容（`read Source = Mask[0];`、`read Mask.Previous.r;`），以及两个设置用逗号连接
（`Shader = "Blur.usf", Entry = BlurCS;`）。

**修复。** 以 `;` 结束每个设置和绑定，并且每条语句只绑定一个缓冲区：
`read A; read B;`，而不是 `read A, B;`。通道在着色器或材质中选择，而不是在
绑定中选择。当 `;` 缺失时，恢复过程还会跳过下一条语句，因此那条语句中的错误
只有在修复此问题后才会显示。

<a id="dsh2304"></a>

## DSH2304

<!-- generated:begin DSH2304 -->
**严重性** 错误

**消息**

```
Expected a buffer after '{0}', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:374`、`Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:384`
<!-- generated:end DSH2304 -->

**原因。** `read` 或 `write` 绑定没有应在其位置的缓冲区。语句开头的 `read` 和 `write`
始终会开始一个绑定，无论其后是什么，因此后面没有名称的词
——`read;`、`write;`、`read "Mask";`——就是此错误，如 `Expected a buffer after 'read', ...`。
指明其槽位和 `=` 之后没有缓冲区的绑定也是如此——`read Source = "Mask";`、
`read Source = ;`、`write Result = (Blurred);`——如 `Expected a buffer after 'read Source =', ...`。
缓冲区是一个裸名称——此管线中的缓冲区或内置纹理，例如 `SceneColor`——
绝不是字符串或表达式。

**修复。** 命名缓冲区：`read Source = Mask;`、`write Result = Blurred;`。当 pass 内的名称就是
缓冲区自身的名称时，将其省略：`read Mask;`。

<a id="dsh2305"></a>

## DSH2305

<!-- generated:begin DSH2305 -->
**严重级别** 错误

**消息**

```
'{0}.{1}': the one thing a buffer has after '.' is 'Previous', last frame's contents of a 'History = true' buffer.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:402`、`Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:408`
<!-- generated:end DSH2305 -->

**原因。** 一个 `read` 或 `write` 绑定在其缓冲区之后有一个 `.`，而其后内容并非完全如此拼写的 `Previous`：`read Mask.previous;`、`read Blurred.r;`（一个通道）、`read Wind.Last;`。缓冲区没有成员，也没有 swizzle；`.Previous`——上一帧 `History = true` 缓冲区的内容——是绑定在其后唯一可以命名的东西。其后没有名称的 `.` 也属于此代码，即 `Expected 'Previous' after '.', found ...`。

**修复。** 绑定缓冲区本身，并在采样处获取通道：`read Blurred;`，然后在着色器或材质中 `.r`。对于上一帧的内容，书写 `read Prev = Wind.Previous;` 并用 `History = true` 声明 `Wind`（否则为 DSH4415）。只有 `read` 可以命名 `.Previous`；其 `write` 为 DSH7318。

<a id="dsh2306"></a>

## DSH2306

<!-- generated:begin DSH2306 -->
**严重级别** 错误

**消息**

```
Expected a parameter name after 'param', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:426`
<!-- generated:end DSH2306 -->

**原因。** `param` 之后并未跟随 pass 内部值的名称——即材质参数或它所设置的 HLSL 名称。语句开头的 `param` 无论其后是什么，总是开始一个绑定，因此省略名称（`param = 3;`）、该词之后没有任何内容（`param;`）以及带引号的名称（`param "Radius" = 3;`）都属于此错误；它们都不会被读作设置。

**修复。** 书写 `param <Name> = <expression>;`，如 `param Radius = OutlineWidth * 2;`。该名称属于 pass：pass 材质的一个参数，或其 `.usf` 所读取的宏。

<a id="dsh2307"></a>

## DSH2307

<!-- generated:begin DSH2307 -->
**严重级别** 错误

**消息**

```
Expected '=' after 'param {0}', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:430`
<!-- generated:end DSH2307 -->

**原因。** 一个 `param` 绑定命名了其目标，之后却没有 `=`：`param Radius OutlineWidth;`、`param Radius: OutlineWidth;`、`param Radius(OutlineWidth);` 或 `param Radius == OutlineWidth;`。

**修复。** 书写 `param <Name> = <expression>;`。该表达式在每个视图的 CPU 上计算一次：一个 `uniform`、一个 `static const`、一个字面量或 `DreamPassWeight`，乘以和加上常量，如 `param Radius = OutlineWidth * 2 + 1;`。

<a id="dsh2308"></a>

## DSH2308

<!-- generated:begin DSH2308 -->
**严重级别** 错误

**消息**

```
Expected a key such as 'Scale' in the arguments of buffer '{0}', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:147`
<!-- generated:end DSH2308 -->

**原因。** 缓冲区的参数列表在参数起始处出现了键以外的内容：没有键的值（清除值为 `R8(0)`）、带引号的键（`R8("Clear" = 0)`）、尾随逗号（`R8(Clear = 0, )`），或未写构造函数的向量，其第二个数字随后出现在本应是键的位置（`Size = 256, 256`）。

**修复。** 为每个参数命名，并为向量提供其构造函数：

```hlsl
buffer Wind : RG16F(Size = int2(256, 256), History = true, Clear = 0);
```

键为 `Scale`、`Size`、`Resolution`、`Clear`、`Mips`、`History` 和 `Export`。

<a id="dsh2309"></a>

## DSH2309

<!-- generated:begin DSH2309 -->
**严重级别** 错误

**消息**

```
Expected '=' after '{0}', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:151`
<!-- generated:end DSH2309 -->

**原因。** 缓冲区参数列表中的一个键之后并未跟随 `=`：单独的键，仿佛它是标志（`R8(History)`、`RGBA16F(Export, Scale = 0.5)`），或在 `=` 出现的位置放了 `:`（`R8(Clear: 0)`）。

**修复。** 为每个键提供其值：`History = true`、`Export = true`、`Clear = 0`。缓冲区没有裸标志。

<a id="dsh2310"></a>

## DSH2310

<!-- generated:begin DSH2310 -->
**严重级别** 错误

**消息**

```
Expected ',' or ')' in the arguments of buffer '{0}', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:171`
<!-- generated:end DSH2310 -->

**原因。** 缓冲区参数列表中的一个 `Key = Value` 之后既不是 `,`，也不是 `)`：两个参数之间没有逗号（`R8(Clear = 0 Scale = 0.5)`）、将 `;` 用作分隔符，或列表从未闭合（`R8(Clear = 0;`）。

**修复。** 用 `,` 分隔参数，并在 `;` 之前闭合列表：`buffer Mask : R8(Clear = 0, Scale = 0.5);`。

<a id="dsh2311"></a>

## DSH2311

<!-- generated:begin DSH2311 -->
**严重级别** 错误

**消息**

```
Expected ';' after the declaration of buffer '{0}', found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:184`
<!-- generated:end DSH2311 -->

**原因。** 缓冲区声明在其格式或参数列表之后并未以 `;` 结束。通常是缺少 `;`，而消息指向下一个声明；其他情形包括参数没有其括号（`buffer Mask : R8 Clear = 0;`）、一个声明中有两个缓冲区（`buffer Mask : R8, Edge : R8;`），以及参数列表处用了大括号（`buffer Mask : R8 { Clear = 0; }`）。

**修复。** 每个声明一个缓冲区，其参数放在括号中，末尾加 `;`：

```hlsl
buffer Mask : R8(Clear = 0);
buffer Edge : R8;
```

<a id="dsh2312"></a>

## DSH2312

<!-- generated:begin DSH2312 -->
**严重级别** 错误

**消息**

```
The line '#{0}' cannot appear inside a pass block; a pass holds settings, 'read', 'write' and 'param' lines and an 'hlsl' block, where HLSL's own '#' lines go.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:322`
<!-- generated:end DSH2312 -->

**原因。** 一个 `#pragma` 或 `#include` 行位于 pass 的大括号内。预处理器自身的行——`#if`、`#ifdef`、`#else`、`#endif`、`#define`——在解析器运行之前就已解析，可以位于此处，但 `#pragma` 和 `#include` 是文件作用域声明，会按书写原样到达解析器：围绕一组设置的 `#pragma region`、误入第一个 pass 的 `#pragma pipeline(...)`、本应属于 pass 着色器的 `#include`。该错误只影响那一行：其后的语句照常解析，因此其中的错误会在同一次运行中报告。

**修复。** 将 `#pragma pipeline(...)` 移到文件作用域，放在 pass 之上。去掉 `#pragma region`：`.dsp` 在文件作用域也会忽略它（DSH3315），而 `//` 注释同样可以分组行。`#include` 属于 pass 所运行的 HLSL：其 `hlsl` 块，或其 `.usf`。在 `hlsl` 块内，每个 `#` 行都属于 HLSL，按书写原样传给着色器编译器。

<a id="dsh2313"></a>

## DSH2313

<!-- generated:begin DSH2313 -->
**严重级别** 错误

**消息**

```
Expected '{' after 'hlsl' to open a block of HLSL, found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserDeclarations.cpp:996`
<!-- generated:end DSH2313 -->

**原因。** 在 `.dsp` 中，`hlsl` 开启一个 HLSL 块——pass 的代码，或文件的共享代码——而其后紧接的内容不是 `{`。在 `.dsp` 中，无论声明或 pass 语句从何处开始，`hlsl` 就是那个词本身，别无其他，因此它在那里不是名称。

**修复。** 写出该块：`hlsl { ... }`，`{` 在同一行或下一行。在 `.dsp` 之外，`hlsl` 是普通名称。

<a id="dsh2314"></a>

## DSH2314

<!-- generated:begin DSH2314 -->
**严重级别** 错误

**消息**

```
Expected '`}' to close pass '{0}' before the next declaration, found {1}.
```

**引发自** `Source/DreamShaderLang/Private/Lang/LangParserPipeline.cpp:269`
<!-- generated:end DSH2314 -->

**原因。** 一个 pass 块没有闭合，而 `buffer` 或 `pass` 声明在其内部开始了一行。解析器在此处结束未闭合的 pass，并保留目前读取的语句，因此下面的声明仍能解析。当后面跟着其他内容时，同样的缺失 `}` 会有不同解读：在文件末尾是 DSH2150，在 `uniform` 或 `static const` 行之前是 DSH2301，在 `#pragma` 行之前是 DSH2312。

**修复。** 在消息所指向的行之上添加闭合该 pass 的 `}`。
