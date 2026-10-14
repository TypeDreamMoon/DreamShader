---
title: "词法元素"
description: "词法元素"
---

<a id="lexical-elements"></a>

# 词法元素

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **词法元素**

DreamShaderLang 的字符级规则：空白、注释、标识符、大小写敏感性、字符串与数字字面量，以及 section 的正文如何被切分成语句。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf`、`.dsh` |
| 类型 | 词法结构 |
| 适用范围 | 整个文件 —— 块、section 和 `Graph` 正文一视同仁；`Function` / `GraphFunction` 正文是 HLSL |

*(自 2.0.0 起)* 一个词法器把每个源文件 —— `.dss` 和 1.x 一样 —— 读成同一条 token 流；1.x 语法再由 legacy 前端从这些 token 里读出来。直到 1.9.x，声明语法和 `Graph` 语法各有一个字符扫描器，规则也各不相同。对 `.dsm` / `.dsf` 有三个专门的选项：`///` 行是普通注释；同一行里的一对 `'…'` 是字符串（Content Browser 的 `Texture2D'…'` 外壳）；行首的 `#` 行是一个指令 token。

<a id="synopsis"></a>

## 语法概览

```c
<token> := <identifier> | <keyword> | <string-literal> | <number> | <punctuation>

<identifier>     := { <letter> | _ } { <letter> | <digit> | _ }…
<qualified-name> := <identifier> :: <identifier>
<string-literal> := " { <character> | \<escape> }… "
<punctuation>    := { | } | ( | ) | [ | ] | ; | = | , | . | : | # | the operators of a Graph expression
```

空白和注释可以出现在任意两个 token 之间，除此之外没有意义。

<a id="whitespace"></a>

## 空白

空格、制表符、垂直制表符、换页、回车和换行分隔 token *(自 2.0.0 起：只认 ASCII；其他 Unicode 空白，比如不换行空格，是
[`DSH2101`](../diagnostics/DSH2xxx.md#dsh2101))*。换行在语法里没有作用，只有
[预处理器](preprocessor.md)指令、`Graph` 体内的 `#Region` 指令
（[Layout](layout.md)）和诊断的行号关心它。

*(自 2.0.0 起)* 任意空白都能在任何 section 里分隔两个单词：类型和名字之间写制表符和写空格一样。

<a id="comments"></a>

## 注释

| 形式 | 规则 |
| :-- | :-- |
| `// …` | 行注释；一直到下一个换行为止，不含换行本身。在 1.x 文件里 `///` 是普通行注释 |
| `/* … */` | 块注释；在第一个 `*/` 处结束 |

- **块注释不嵌套。** `/* a /* b */ c */` 在第一个 `*/` 处结束；后面的 `c */` 又变回代码。
- **未闭合的块注释是错误** *(自 2.0.0 起)*：
  [`DSH2102`](../diagnostics/DSH2xxx.md#dsh2102)，报在开头的 `/*` 处。1.x 会接受它并吞掉文件剩余部分。
- 注释在每个位置都被同样识别，包括 `Graph` 体内，统计花括号、圆括号和方括号时也是，所以注释里的花括号或引号永远不会让块失衡。
- **注释能藏住 `import`** *(自 2.0.0 起)*：`import` 是一个 token，所以写在 `/* … */` 里的就是被注释掉了。见 [`import`](import.md#recognition)。**它藏不住预处理指令**：`#if` 一族是在理解注释之前按行扫描出来的，所以写在 `/* … */` 里的 `#if` 仍然是指令。首个非空白字符是 `//` 的行则一定不是。见 [预处理器](preprocessor.md#recognition)。

<a id="identifiers"></a>

## 标识符

| 位置 | 可用字符 |
| :-- | :-- |
| 首字符 | ASCII 字母或 `_` |
| 后续字符 | ASCII 字母、数字或 `_` |

- 没有长度上限。*(自 2.0.0 起)* 只有 ASCII 字母算字母；ASCII 之外的字母是 `DSH2101`。
- **块关键字、section 名和类型名不是保留的。** 叫 `Shader`、`Graph` 或 `float` 的属性、变量、参数、函数都可以；至于这个名字之后能不能解析，是它所处上下文的事。小写[保留字](keywords.md#reserved-words) —— `in`、`out`、`if`、`return`、`const`…… —— 则是保留的 *(自 2.0.0 起)*。
- 每个 section 里的声明名都必须是一个标识符 token *(自 2.0.0 起)*；1.x 只检查 `Properties` 的名字非空，所以 `Properties { float 1Bad = 0; }` 能解析。现在 `1Bad` 是一个畸形数字（[`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105)），这条声明是 [`DSH3250`](../diagnostics/DSH3xxx.md#dsh3250)。
- 带命名空间的名字用 `::`，例如 `Common::ApplyTint`。见 [命名空间](namespace.md)。

名字进入生成的 HLSL 时会被净化：`A–Z a–z 0–9 _` 之外的每个字符变成 `_`，开头是数字则加一个 `_` 前缀，连续的 `__` 塌缩成一个，结果为空或全是下划线时变成 `DreamShaderSymbol`。正是这次塌缩，让 `Common::ApplyTint` 和 `Common_ApplyTint` 在生成的代码里相撞。

<a id="case-sensitivity"></a>

## 大小写敏感性

**顶层块关键字和小写的保留字是大小写敏感的。** 1.x 语法里其余类关键字的东西都忽略大小写匹配，下表标出了例外。引擎定义的名字 —— 材质属性、节点类、引脚 —— 精确匹配；在 1.x 文件里只差大小写也能解析，但给出警告 [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276)；文件自己声明的名字同理，警告是 [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275) *(自 2.0.0 起，legacy 规则 L19)*。[预处理器](preprocessor.md)在语法之前运行，不属于任何一套语法，从头到尾都大小写敏感。

| 构造 | 大小写敏感 | 参考 |
| :-- | :-- | :-- |
| 顶层块关键字 —— `Shader`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend`、`MaterialLayer`、`MaterialLayerBlend`、`VirtualFunction`、`Namespace`、`Function`、`GraphFunction` | **是** | [关键字索引](keywords.md) |
| 保留字 —— `in`、`out`、`const`、`return`、`true`、`import`、… | **是**，作为保留字；`In`、`Const`、`True` 是普通单词，1.x 语法在需要关键字的地方仍会把它们读成关键字 | [关键字索引](keywords.md#reserved-words) |
| Section 名 —— `Properties`、`Settings`、`Outputs`、`Inputs`、`Results`、`Options`、`Graph`、`Code`、`Layout` | 否 | [关键字索引](keywords.md) |
| Header 属性键 —— `Name=`、`Root=`、`Asset=` | 否 | [Shader](shader.md) |
| `Settings` / `Options` 键、元数据键、`Expression( … )` 参数键、`Layout` 参数键 | 否 | [Settings](../../settings/index.md) |
| `UE.*` 参数名 | 否，`DSH5276` 除外 | [`UE.*` 目录](../builtins/ue.md) |
| 类型 token —— `float3`、`vec3`、`Texture2D`、`ScalarParameter`、… | 否 | [类型](types.md) |
| `Properties` 中的 `const` 前缀 | 否 | [属性](properties.md) |
| `Inputs` 中的 `opt` 前缀 | 否 | [输入 / 输出](inputs-outputs.md) |
| `Function` 之后的 `SelfContained` / `Inline` | 否 | [函数](function.md) |
| `in` / `out` 参数限定符 | 否 | [函数](function.md) |
| `true` / `false` | 否 | [类型](types.md) |
| `Path(` 纹理引用关键字 | 否 | [`Path(...)`](../parameters/path.md) |
| `Base.` 绑定前缀和 `Expression(` | 否 | [输出绑定](output-bindings.md) |
| `Base.` 属性名 | 否，`DSH5276` 除外 | [输出绑定](output-bindings.md#basetarget-catalogue) |
| `.Pin[` 引脚选择器 | 否 | [输出绑定](output-bindings.md) |
| `Group("…")` 属性作用域头 | 否 | [属性](properties.md) |
| `Slider(` 元数据简写 | 否 | [元数据块](../parameters/metadata.md) |
| `UE.` 内建前缀 | 在 `Properties` 中和 `GraphFunction` 体中为否；在 `Graph` 调用中为**是** *（自 2.0.0 起）* | [`UE.*` 目录](../builtins/ue.md) |
| `#Region` / `#EndRegion` | 否 | [布局](layout.md) |
| `Node(` / `Comment(` 布局调用 | 否 | [布局](layout.md) |
| `import` | 否，另一种情况会给出警告 [`DSH2253`](../diagnostics/DSH2xxx.md#dsh2253) | [`import`](import.md) |
| `#if` / `#ifdef` / `#ifndef` / `#elif` / `#else` / `#endif` / `#define` / `#undef` | **是 —— 只认小写**；`#IF` 是 `DSH1035`，不是静默无效 | [预处理器](preprocessor.md#what-is-not-a-directive) |
| define **名字**，以及保留的 `DS_` 前缀判定 | **是** —— `Foo` 和 `FOO` 是两个不同的 define，且 `ds_foo` 不算保留 | [预处理器](preprocessor.md#where-defines-come-from) |
| `#if` 里的字符串比较 | **是** —— `DS_PLATFORM == "windows"` 在 Windows 上为假 | [预处理器](preprocessor.md#values) |
| `default` 调用参数哨兵 | **是** —— 只认小写 *（自 2.0.0 起）* | [调用](../graph/calls.md) |
| `.dsm` / `.dsf` / `.dsh` 扩展名 | 否 | [源文件](source-files.md) |

所以 `shader(Name="X")` 和 `SHADER(Name="X")` 都是语法错误
（[`DSH2240`](../diagnostics/DSH2xxx.md#dsh2240)），而 `properties = { … }`、
`settings { domain = "ui"; }` 和 `Shader(name="X")` 都能被接受。

关键字是整个单词：`ShaderFunction` 是它自己的标识符，永远不会被读成 `Shader` 后面跟
别的东西，`ShaderLayerBlend` 也永远不会被读成 `ShaderLayer`。

<a id="string-literals"></a>

## 字符串字面量

字符串以 `"` 开始，到同一行里下一个未转义的 `"` 结束，由词法器完成
反转义。

| 转义 | 产生 |
| :-- | :-- |
| `\n` | 换行 |
| `\r` | 回车 |
| `\t` | 制表符 |
| `\"` | `"` |
| `\\` | `\` |
| `\0` | 什么都不产生 —— 字符串不能含 NUL |
| `\<any other character>` | [`DSH2104`](../diagnostics/DSH2xxx.md#dsh2104) *（自 2.0.0 起；1.x 会丢掉反斜杠）* |

- *（自 2.0.0 起）* 字符串永远不跨行：闭合 `"` 之前遇到换行就结束，未闭合的字符串是
  [`DSH2103`](../diagnostics/DSH2xxx.md#dsh2103)，在 section 里和在 `Graph` 体里都一样。
  1.x 允许字符串跨行，并且 `Graph` 体里未闭合的字符串会在输入末尾被静默闭合。
- 在 1.x 文件和 `.dsh` 里，同一行里的一对 `'` 也是字符串，没有转义 —— 也就是
  Content Browser 引用的外壳 `Texture2D'/Game/T.T'`。孤零零的 `'` 是 `DSH2101`。

任何可以写引号的地方 —— settings 值、元数据值、属性值、参数值 —— **引号都是可选的**：`Domain = UI;` 和
`Domain = "UI";` 等价。带引号的值就是字符串的文本，不带引号的值是它那些 token 的文本。

> [!NOTE]
> 带引号的 `Settings` 或 `Options` 值不会被去空白：`Domain = " UI ";` 存下来的是带空格的
> `` UI ``。

<a id="numeric-literals"></a>

## 数字字面量

<a id="declaration-grammar"></a>

### 声明语法

*（自 2.0.0 起）* 数字在任何地方都是 token，按 [下面](#graph-expression-grammar)的规则切分；
然后由 section 按消费者的要求读取这个 token 的文本：

| 消费者 | 接受什么 |
| :-- | :-- |
| 标量默认值 | 可选的正负号、数字、至多一个 `.`、可选的结尾 `f` / `F`；或者 `true` / `false` |
| 整数参数（`SortPriority`、`Layout` 坐标） | 可选的正负号和数字 |
| 布尔默认值 | 恰好 `true` 或 `false`，大小写不限 |
| 向量默认值 | `<anything>( <part> [, <part>]… )` —— 见下 |

不被接受的写法：默认值是 [`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254)，
`SortPriority` 是 [`DSH3258`](../diagnostics/DSH3xxx.md#dsh3258)，
`Layout` 坐标是 [`DSH3275`](../diagnostics/DSH3xxx.md#dsh3275)。

| 写法 | 读作 | 诊断 |
| :-- | :-- | :-- |
| `float Strength = 1.0f;` | `1.0` | 无 |
| `float Strength = 0.0f;` | `0.0` | 无 |
| `float Strength = 1abc;` | — | `DSH2105` 和 `DSH3254` *（自 2.0.0 起；1.x 读成 `1.0`）* |
| `float Strength = abc;` | — | `DSH3254` |
| `float Strength = 0x1F;` | — | `DSH3254` |
| `float Strength = 1e3;` | `1000.0` | 无 *（2.0.0 – 2.1.0：`DSH3254`）* |

向量字面量的形式和 1.x 一样故意写得很松。第一个 `(` 和**最后一个** `)` 界定
分量，而 **`(` 之前的文本被完全忽略** —— `float3(1,0,0)`、`vec3(1,0,0)`、
`(1,0,0)` 和 `Nonsense(1,0,0)` 解析结果完全一样。内部按 `,` 切分且
不跟踪嵌套，所以某个分量里再嵌一层调用就会切错。每个分量可以是数字（可带指数和结尾的 `f`），也可以是
`true` / `false`。

| 分量个数 | 结果 |
| :-- | :-- |
| 1 | `(a, a, a, 1)` —— 铺满 x、y、z |
| 2 | `(a, b, 0, 0)` |
| 3 | `(a, b, c, 1)` |
| 4 或更多 | 取前四个，多出来的被忽略 |

未填充时的默认值是 `(0, 0, 0, 1)`。

<a id="graph-expression-grammar"></a>

### Graph 表达式语法

| 元素 | 规则 |
| :-- | :-- |
| 起始 | 一个数字，**或者**紧跟数字的 `.` —— 所以 `.5` 合法 |
| 主体 | 数字，然后至多一个 `.` 和更多数字 |
| 指数 | `e` 或 `E`，可跟 `+` 或 `-`，然后是数字 |
| 后缀 | `f` `F` `h` `H` 表示浮点，`u` `U` `l` `L` 表示整数 |
| 十六进制 | `0x1F` 是一个十六进制整数 token；在 `Graph` 表达式里是 [`DSH2222`](../diagnostics/DSH2xxx.md#dsh2222) *(自 2.0.0 起；1.x 把它读成 `0` 后面跟名字 `x1F`)* |
| 粘在一起的其他东西 | `1.2.3`、`1abc`、`1e`、`1.0u`、`1fu` 是一个畸形 token：`DSH2105` |

后缀是 token 文本的一部分，所以 `0.55f` 就是数字 `0.55`。

*(自 2.0.0 起)* 2.0 语法的每个运算符都是 token：`%`、`<`、`!`、`&&`、`?`、`[`、`]` 不再
终止 `Graph` 表达式并丢掉后半截。1.x 的 `Graph` 没有的东西会被点名拒绝 —— [`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200) 到 `DSH2222`。见
[不支持的写法](../graph/unsupported.md)。

<a id="statement-separation"></a>

## 语句切分

section 的内容是一串以 `;` 分隔的语句，逐个 token 读取 *(自 2.0.0 起)*。

- `;` 只在 `()`、`[]` 和 `{}` 之外分隔语句。字符串是一个 token，所以字符串里的 `;` 只是文本。
- 空语句被跳过，所以多余的 `;;` 无害。
- section 闭合 `}` 之前最后一条语句的 `;` 是可选的。
- section 闭合 `}` 之后的 `;` 是可选的，section 名和它的块之间的 `=` 也是可选的 *(自 1.5.0 起)*。
- 一个值 —— 默认值、设置、绑定源、参数 —— 一直延续到括号之外结束它的 `;`、`,` 或 `)`，所以不带引号的属性值
  可以含圆括号和逗号：`Asset = Path(Game, "MaterialFunctions/F_X")` 是一个值。

*(自 2.0.0 起)* 1.x 用三种不同的切分器切语句，其中两种找的是字面空格，所以在 `Inputs`、`Outputs`、`Results` 和 `Shader` 的 `Outputs` 里写 `float3\tColor;` 会失败，
而且 `opt\tfloat X` 不会把输入标成可选。现在两者都能正常工作。

<a id="diagnostics"></a>

## 诊断

| 码 | 何时触发 |
| :-- | :-- |
| `DSH2101` | 一个不属于任何 token 的字符：ASCII 之外、`$`、`@`、孤零零的 `'`、…… |
| `DSH2102` | 一个没有 `*/` 的 `/*` |
| `DSH2103` | 一个在本行没有闭合 `"` 的字符串 |
| `DSH2104` | 字符串里的未知转义 |
| `DSH2105` | 一个畸形数字 |
| [`DSH2106`](../diagnostics/DSH2xxx.md#dsh2106) | 一个不是它所在行第一个东西的 `#` |
| `DSH2240` | 顶层出现了不是块关键字的东西 |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | 文件在一个块、section 或列表中间结束 |
| `DSH3254`、`DSH3258`、`DSH3275` | 一个默认值、排序优先级或布局坐标不是所需类型的数字 |

每条诊断都带它所指构造的文件、行和列，并打印成
`<file>(<line>,<column>): DSHnnnn: <message>` *(自 2.0.0 起)*；只有少数关于整个文件的诊断，
比如无法解析的 import，报在第 1 行。1.x 只有以
`near index {Index}` 结尾的消息才带位置。见 [`import`](import.md#source-line-mapping)。

<a id="example"></a>

## 示例

```c
// Line comment before the top-level declaration.
Shader(Name="DreamShaderTests/Corpus/M_Comments")
{
    /* Block comment
       spanning multiple lines. */
    Settings = {
        Domain = "UI";        // trailing line comment
        ShadingModel = Unlit; // quotes are optional
    }

    Properties {
        // Any whitespace may separate a type from its name.
        ScalarParameter Rough = 0.5 [Group="Surface"; Slider(0, 1)];
        vec3            Tint  = vec3(1.0, 0.4, 0.1);
        float           Fudge = 1.0f;   // reads as 1.0
    }

    Outputs {
        vec3	Color;                  // a tab is whitespace too
        Base.EmissiveColor = Color
    }                                   // the last ';' in a block is optional

    Graph {
        vec2 UV = UE.TexCoord(Index = 0);
        Color = vec3(Rough, Rough, UV.x) * Tint;
    }
}
```

<a id="see-also"></a>

## 继续阅读

- [关键字索引](keywords.md) —— 每个保留字，及其大小写规则
- [源文件](source-files.md) —— 这些 token 所在的文件种类
- [`import`](import.md) —— 自 2.0.0 起成为该语言的一个 token，以及位置从何而来
- [预处理器](preprocessor.md) —— 按行处理的指令，以及唯一一组大小写敏感的关键字
- [类型](types.md) —— 完整的类型 token 目录和 GLSL 别名
- [属性](properties.md) —— 带 `Group("…") { … }` 作用域的 section
- [输入 / 输出 / 结果](inputs-outputs.md) —— 带类型参数的 section
- [布局](layout.md) —— `#Region` / `#EndRegion`，唯一类似指令的语法
- [字面量](../graph/literals.md) —— `Graph` 块里的字面量形式
- [不支持的写法](../graph/unsupported.md) —— 1.x 的 `Graph` 表达式拒绝哪些写法
- [诊断索引](../diagnostics/index.md) —— 每个码
