---
title: 运算符与转换
description: 四个运算符及其优先级、字面量、名字查找、构造器、swizzle、把值适配到目标类型的转换规则，以及相同节点的合并。
---

Graph 语言产生值的那一半：字面量、标识符、调用、成员访问和四个算术运算符，最终降级成
`UMaterialExpression` 节点。本页内容同样适用于 `Outputs` 绑定表达式和 `Outputs` 声明默认值——它们就是
同一套文法下的单条表达式。

<a id="语法概要"></a>

## 语法概要

```c
<expression>     := <additive>
<additive>       := <multiplicative> [ { + | - } <multiplicative> ] …
<multiplicative> := <unary> [ { * | / } <unary> ] …
<unary>          := { + | - } <unary> | <postfix>
<postfix>        := <primary> [ <postfix-op> ] …
<postfix-op>     := . <identifier> | :: <identifier> | ( [ <argument> [ , <argument> ] … ] )
<primary>        := <identifier> | <number-literal> | <string-literal> | ( <additive> )
<argument>       := [ <identifier> = ] <additive>
```

这是 1.x 的文法。legacy 前端用 2.0 文法（它的超集）解析表达式，再把上面文法之外的一切带码拒绝
（自 2.0.0 起）——见[不存在的运算符](#不存在的运算符)。

<a id="优先级与结合性"></a>

## 优先级与结合性

绑定最紧的排在最前。

| 层级 | 运算符 | 元数 | 结合性 | 说明 |
| :-- | :-- | :-- | :-- | :-- |
| 1 | `f(…)` 调用、`.member`、`::name` | 后缀 | 左 | 可自由串联：`A::F(x).rgb.b` |
| 2 | `+` `-` | 一元前缀 | 右 | 可递归：`- -x` 合法；不带空格的 `--x` 是自减运算符，`DSH2207` |
| 3 | `*` `/` | 二元 | 左 | `a / b / c` 是 `(a / b) / c` |
| 4 | `+` `-` | 二元 | 左 | `a - b - c` 是 `(a - b) - c` |
| —— | `( … )` 分组 | —— | —— | 覆盖上面各层；不产生节点 |

<a id="不存在的运算符"></a>

### 不存在的运算符

1.x 表达式文法里没有别的运算符。下面每一种都能解析，随后被拒绝：

| 类别 | 写法 | 码 |
| :-- | :-- | :-- |
| 取模 | `%` —— 请用 `fmod` 内置 | `DSH2201` |
| 比较 | `==` `!=` `<` `>` `<=` `>=` —— **只**在 `if` 条件里合法 | `DSH2200` |
| 逻辑 | `&&` `\|\|` `!` | `DSH2200` |
| 位运算 / 移位 | `&` `\|` `^` `~` `<<` `>>` | `DSH2201` |
| 条件 | `? :` | `DSH2200` |
| 自增 / 自减 | `++` `--` | `DSH2207` |
| 复合赋值 | `+=` `-=` `*=` `/=` 等 | `DSH2205` |
| 表达式内赋值 | `=`，命名调用参数除外 | `DSH2204` |
| 下标 | `[ ]` —— 请用 [swizzle](#swizzle) | `DSH2202` |
| 强制转换 | `(float3)x` —— 请用构造器 | `DSH2203` |
| 逗号运算符 | `,`，调用参数表除外 | 没有自己的码：语句在那里结束，后面的内容通常是 `DSH2154` |
| 矩阵 | 不存在 | 矩阵值是 `DSH4361` |

**警告：**

**1.x 会静默截断这些写法。** 它的分词器把每个不认识的字符（`%`、`!`、`<`、`>`、`&`、`|`、`^`、`~`、`?`、
`[` ……）都映射成输入结束，于是解析器保留它前面的表达式、无声地丢掉其余部分。（自 2.0.0 起） 起整条
表达式都会被读完，运算符被拒绝：

| 写法 | 1.x 构建出的 | 现在的码 |
| :-- | :-- | :-- |
| `a % b` | `a` | `DSH2201` |
| `a && b` | `a` | `DSH2200` |
| `a ? b : c` | `a` | `DSH2200` |
| `v[0]` | `v` | `DSH2202` |
| `a << 2` | `a` | `DSH2201` |

根本不是记号的字符（`@`、`$`、反引号）是 `DSH2101`。不再需要靠给可疑表达式加括号来让截断暴露出来。完整
清单见[不支持的写法](unsupported.md)。

<a id="操作数规则"></a>

## 操作数规则

绑定器对 `+ - * /` 的两个操作数做这些检查：

| # | 检查 | 失败时 |
| --: | :-- | :-- |
| 1 | 两个操作数都是数或 bool——不是纹理、采样器或 `MaterialAttributes` 值 | `DSH4226` |
| 2 | `Substrate` 操作数：两个 `Substrate` 值之间的 `+` 和乘以标量的 `*` 分别构建 `Substrate.Add` / `Substrate.Weight` （自 2.0.0 起）（1.x 两者都拒绝）；其他运算符 | `DSH5293` |
| 3 | 宽度一致：结果与最宽的操作数一样宽，标量操作数广播到该宽度，更窄的向量不会 | `DSH4226` |
| 4 | 对 `/`：两个操作数不能都是整数构造器调用 | `DSH4243`——见[整数除法](#整数除法) |

运算符从不收窄操作数：`float3 * float4` 是 `DSH4226`，这样通道永远不会被静默丢掉。（自 2.0.0 起） 起它也
不再加宽操作数：1.x 在某些情况下会把一个操作数加宽到"权威"值的宽度；现在操作数都有类型，标量广播是两种
宽度相遇的唯一方式。

这里没有单独的标量提升步骤：标量操作数原样传给材质节点，由 Unreal 自行复制，所以 `A` 是 `vec3`、`K` 是
`float` 时 `A * K` 只产生一个 `Multiply` 节点。

<a id="二元运算符的结果"></a>

### 二元运算符的结果

| 属性 | 取值 |
| :-- | :-- |
| 节点 | `Add` / `Subtract` / `Multiply` / `Divide` |
| 宽度 | 最宽操作数的宽度 |
| 种类 | 按 HLSL 规则提升——`int` 与 `float` 得到 `float`；在图里每个值都是浮点 |
| 全是常量的操作数 | 由常量折叠 pass 合成一个 `Constant` 节点 （自 2.0.0 起） |

<a id="一元运算符"></a>

## 一元运算符

| 形式 | 降级为 | 节点开销 | 结果 |
| :-- | :-- | :-- | :-- |
| `+x` | 恒等 | 无 | `x` 原样返回 |
| `-x` | `Multiply(x, Constant(-1))` | 一个 `Multiply` 和一个值为 `-1` 的 `Constant` | `x` 的类型 |

`!x` 是 `DSH2200`，`~x` 是 `DSH2201`，`++x` 和 `--x` 是 `DSH2207`。

被取负的常量会被折叠：`-2.0` 变成一个值为 `-2` 的 `Constant` （自 2.0.0 起），常量上的任何算术也一样。
见[常量折叠](#常量折叠)。

<a id="整数除法"></a>

## 整数除法

`/` 是唯一带额外类型规则的运算符：当**两个**操作数都是整数构造器调用（`int`、`int2..4`、`ivec2..4`、
`uint`、`uint2..4`、`uvec2..4`）时报 `DSH4243`。材质图没有整数除法，所以编译器拒绝在 HLSL 的截断结果和
图的小数结果之间替你选一个。

其余所有 `/` 都是图的浮点除法，和 1.x 一样——1.x 把每个数字字面量都当作浮点——哪怕两边对编译器都是整数：
整数字面量（`7`）、`int` / `uint` 变量或属性（legacy 规则 L27）。[`dsc migrate`](../tools/migrate.md)
会把这样的除法写成 `float(7) / 2`，因为 `.dss` 拒绝整数之间的每一个 `/`。

| 表达式 | 结果 |
| :-- | :-- |
| `int(7) / int(2)` | `DSH4243` |
| `int(7) / 2` | `3.5`（2.0.0 – 2.1.0 报 `DSH4243`） |
| `7 / 2` | `3.5`（2.0.0 – 2.1.0 报 `DSH4243`；2.1.1 起恢复） |
| `7.0 / 2` | `3.5` |
| `float(int(7)) / int(2)` | `3.5`——有一个操作数是浮点 |
| `int a = 7; int b = 2; a / b` | `3.5`（2.0.0 – 2.1.0 报 `DSH4243`） |

**提示：**

`int`、`uint`、`bool` 和 `half` 对编译器是各自的种类，这条规则就是这种区别存在的用处。在生成图里它们都是
浮点——没有整数运算，也没有截断。

<a id="字面量"></a>

## 字面量

（自 2.0.0 起） 起 1.x `Graph` 块体由 DreamShaderLang 2.0 词法器读取：下面是 2.0 的记号规则，1.x 读法
与之不同的少数几处会带码拒绝，并在信息里说明 1.x 当时做了什么。

<a id="数值字面量"></a>

### 数值字面量

```c
<number-literal> := { <digit> | . } … [ { e | E } [ { + | - } ] <digit> … ] [ <suffix> … ]
<suffix>         := { f | F | h | H | u | U | l | L }
```

没有小数部分、没有指数、没有浮点后缀的字面量对编译器是**整数**——`int`，带 `u` 时是 `uint`
（自 2.0.0 起）；其余都是 `float`（带 `h` 时是 `half`）。在生成图里所有字面量都一样：一个 1 分量浮点
`Constant`。

| 写法 | 种类 | 值 | 结果 |
| :-- | :-- | :-- | :-- |
| `1` | `int` | 1.0 | `Constant`，1 分量 |
| `1.0` | `float` | 1.0 | 与 `1` **同一个** `Constant` 节点 |
| `.5` | `float` | 0.5 | `Constant`——允许前导 `.` |
| `1.` | `float` | 1.0 | `Constant`——允许末尾 `.` |
| `1e-3` | `float` | 0.001 | `Constant` |
| `2.5E+2` | `float` | 250.0 | `Constant` |
| `0.55f` | `float` | 0.55 | `Constant` |
| `0.5h` | `half` | 0.5 | `Constant` |
| `3u`、`3ul` | `uint` | 3.0 | `Constant`——无符号整数 （自 2.0.0 起） |
| `0.5.5` | —— | —— | `DSH2105`（1.x 静默读成 0.5） |
| `0x10` | —— | —— | `DSH2222`——十六进制字面量，1.x 从没把它读成十六进制 |
| `1.0fx` | —— | —— | `DSH2105`——字母属于这个数 |

词法细节：

- 一个数是一个记号：数字、至多一个 `.`、至多一个指数，然后是后缀字母。仍然粘在它后面的东西——又一个 `.`、
  字母、后缀之后的数字——都属于同一个记号，使它成为 `DSH2105`（1.x 保留最长的合法前缀，丢掉其余部分）。
  所以 `0.5.5`、`0..5`、`1abc`、`1.0fx` 是错误，不是数。
- 指数必须有数字：`1e`、`1e+` 是 `DSH2105`。
- 浮点后缀字母与整数后缀字母并用（`1fu`）、带小数或指数的数字上用整数后缀（`1.0u`、`1e3l`）都是
  `DSH2105`。
- 没有二进制、八进制写法，也没有数字分隔符。
- 前导 `-` 不属于字面量，它是一元负号；被取负的字面量会折叠成一个 `Constant`。

<a id="字符串字面量"></a>

### 字符串字面量

字符串字面量在分词时解码转义，且**永远不会求值成一个值**。它只在参数处理器读取字面*文本*的地方合法：设置
节点属性的 `UE.*` / `Substrate.*` 命名参数、`Output=` / `OutputName=` 选择器，或 `Path(…)` 资产引用。其他
需要值的地方都是 `DSH4202`。

| 转义 | 产生 |
| :-- | :-- |
| `\n` | 换行 |
| `\r` | 回车 |
| `\t` | 制表符 |
| `\"` | `"` |
| `\\` | `\` |
| `\0` | 无——该字符被丢弃 |
| `\` + 任意其他字符 | `DSH2104` （自 2.0.0 起）（1.x 丢掉反斜杠） |

```c
vec4 Scene = UE.SceneTexture(Id = "PostProcessInput0");   // 可以
float x    = "0.5";                                       // DSH4202
```

字符串不能跨行。到行尾还没有闭合 `"` 的是 `DSH2103` （自 2.0.0 起）（1.x 会一直读到输入结束），文件
其余部分照常读取并各自报错。

<a id="布尔字面量"></a>

### 布尔字面量

`true` 和 `false` （自 1.5.0 起） 在解析这一行时就被读成字面量，不区分大小写，排在任何名字查找之前
（自 2.0.0 起）（1.x 排在变量和属性之后）。它们是 1 分量 `bool`，节点是值为 1 或 0 的 `Constant`
（自 2.0.0 起）（1.x 构建的是 `StaticBool`）；相等的常量是一个节点。

**警告：**

因为 `true` / `false` 在任何名字**之前**被读取，一个叫 `True` 或 `False`（任意大小写）的 Graph 变量或
已声明属性无法被读取：每次使用这个名字都是字面量。请避免使用这些名字。

<a id="标识符解析"></a>

## 标识符解析

（自 2.0.0 起） 起名字分两个阶段查找：legacy 前端在解析时读取少数 1.x 拼法（布尔字面量、类型名、
`N::F`、1.x 调用简写、成为节点的属性），其余由 2.0 绑定器解析——与 `.dss` 相同。绑定器**区分大小写**；
只在另一种大小写下匹配的名字由 legacy 规则 L19 照用，并给出警告。

| 顺序 | 阶段 | 表面 | 大小写 | 结果 |
| --: | :-- | :-- | :-- | :-- |
| 1 | 解析 | 布尔字面量 `true` / `false` | 不敏感 | 1 分量 bool 值 |
| 2 | 解析 | 类型名——`float3`、`vec3`、`half`、`Texture2D` …… | 不敏感 | 类型不是值：`DSH4204` |
| 3 | 解析 | 类型为参数节点的属性（`TextureSampleParameter2D`、`ChannelMaskParameter` ……），`StaticSwitchParameter` 除外 | 不敏感 | 该属性的节点，在这次读取处写出 |
| 4 | 绑定 | Graph 变量——块体的声明、`Shader` 的 `Outputs` 声明——由内向外 | 敏感 | 该变量 |
| 5 | 绑定 | 函数的参数：`ShaderFunction` 的输入或输出，或 `Base`（`Shader` 构建的材质） | 敏感 | 该参数 |
| 6 | 绑定 | 文件作用域的声明：标量、向量、纹理或静态布尔属性 | 敏感 | 材质参数 |
| 7 | 绑定 | 4–6 中忽略大小写时**恰好一个**匹配 | 不敏感 | 采用它，并给出警告 `DSH5275`（legacy 规则 L19） |
| 8 | —— | 无匹配 | —— | `DSH4200`；单独的 `UE` 或 `Substrate` 是 `DSH4203` |

步骤 1–3 在任何变量已知之前运行，所以它们胜过同名变量。步骤 7 要求唯一匹配：忽略大小写时有两个声明都匹配，
就一个也不选，报 `DSH4200`。与标量、向量、纹理或静态布尔属性同名的 Graph 变量从它声明起胜出，声明处给出
警告 `DSH4251`。

**提示：**

`StaticSwitchParameter` 属性刻意**不**作为裸标识符解析——它需要两个分支。按名字读取会报 `DSH4200`，请改用
调用形式。见[调用](calls.md#staticswitchparameter)。

另外，名字写成与声明不同的大小写时**就是**那个声明：`float3 Color = …;` 后面跟 `color = …;` 写的是
`Color` （自 2.0.0 起）（1.x 会另建一个叫 `color` 的条目）。

调用名的查找顺序见[调用](calls.md#分发顺序)。

<a id="构造器"></a>

## 构造器

```c
<constructor-call> := <constructor-name> ( <expression> [ , <expression> ] … )
```

解析器把构造器名读成**类型** （自 2.0.0 起）：调用根本不会走到函数查找，绑定器像检查 `.dss` 构造器
一样检查它。名字都不区分大小写（`FLOAT3(…)` 也行）。分量数是名字末尾的数字；没有数字或数字为 `1` 的是标量。

| 家族 | 名字 | 种类 |
| :-- | :-- | :-- |
| HLSL 浮点 | `float` `float1..4` | float |
| HLSL half | `half` `half1..4` | half——行为与 `float` 相同 |
| GLSL 浮点向量 | `vec2..4` | float |
| 有符号整数 | `int` `int1..4` `ivec2..4` | **int** |
| 无符号整数 | `uint` `uint1..4` `uvec2..4` | **uint** |
| 布尔 | `bool` `bool1..4` `bvec2..4` | bool |
| double | `double` `double1..4` | double；在图里和所有数一样是浮点 |
| `StaticBool` | `StaticBool` | 读作 `bool`，与声明中一样 |

（自 2.0.0 起） 起解析器接受 HLSL 类型文法里的每一种拼法，所以 `int1`、`uint1`、`bool1`、`double*` 和
`StaticBool` 也是构造器。矩阵拼法（`float2x2` … `float4x4`、`mat2`、`mat3`、`mat4`）是类型，但图里没有矩阵
值：`DSH4361`。

不存在的名字：`vec1`、`ivec1`、`uvec1`、`bvec1`（GLSL 家族从 2 开始）；`hvec2..4`、`dvec2..4`；已移除的别名
`Scalar`、`Vector`、`Color`。它们都按普通标识符词法处理，落到函数查找——除非声明了同名函数，否则报
`DSH4208`。`MaterialAttributes`、`Substrate`、纹理类型和 `SamplerState` 也是类型名，但不能构造它们的值：
`Texture2D(…)` 是 `DSH4223`。

**警告：**

**构造器名字会遮蔽一切。** 构造器名在解析调用时就被读成类型——排在 `UE.*`、`Substrate.*`、数学内置、参数
和所有用户声明之前。一个叫 `float3`、`int`、`bool4` 或 `vec2` 的 `Function`、`GraphFunction`、
`ShaderFunction` 或 `VirtualFunction` 可以毫无诊断地声明出来，却**永远无法被调用**——每个调用点构建的都是
构造器。请改名。

整数构造器的种类只做一件事：两个操作数都是整数构造器调用时，`/` 被拒绝（`DSH4243`），与 1.x 一样。它不截断、
不舍入、不钳制范围——`int(7.9)` 就是 7.9。种类来自构造器自己的名字，所以外面再包一层非整数构造器就清掉了：
`float(int(7))` 是浮点。

<a id="参数规则"></a>

### 参数规则

先检查调用及其参数，每一项失败都会报告：

| # | 条件 | 行为 |
| --: | :-- | :-- |
| 1 | 任一参数是命名参数（`float3(x = 1.0)`） | `DSH4225` |
| 2 | 一个参数都没有（`float3()`） | `DSH4221` |
| 3 | 参数不是数或 bool——纹理、`MaterialAttributes`、`Substrate` 值——或是矩阵 | `DSH4226` |

然后，记构造器分量数为 `N`：

| # | 情况 | 行为 | 节点 |
| --: | :-- | :-- | :-- |
| 4 | 恰好一个参数，且它有 `N` 个分量（`N == 1` 时为标量） | 参数**原样**返回，只改变种类 | 无 |
| 5 | `N > 1`，恰好一个参数，且是标量 | **广播**：在读取值的地方接上这个标量，由材质节点复制 （自 2.0.0 起）（1.x 构建 `AppendVector` 链） | 无；标量是常量时为一个 `ConstantNVector` |
| 6 | 其他 | 各参数分量数之和必须**正好**等于 `N` | 第一个之后每个参数一个 `AppendVector` |
| 7 | 和 ≠ `N` | `DSH4222` | —— |

| 调用 | 总数 | 合法 | 结果 |
| :-- | :-- | :-- | :-- |
| `float4(rgb, 1.0)` | 3 + 1 | 是 | `float4` |
| `float4(uv, uv)` | 2 + 2 | 是 | `float4` |
| `float3(x, yz)` | 1 + 2 | 是 | `float3` |
| `vec3(0.5)` | 标量广播 | 是 | `(0.5, 0.5, 0.5)` |
| `float3(rgba)` | 4 | **否** | `DSH4222` |
| `float3(uv)` | 2 | **否** | `DSH4222` |
| `float(rgb)` | 3 | **否** | `DSH4222` |
| `float3()` | 0 | **否** | `DSH4221` |

**提示：**

构造器**从不收窄**，也从不补零——在 1.x 源文件里也一样。`float3(rgba)` 是错误而不是截断，请写 `rgba.rgb`；
`float3(uv)` 也是错误，请写 `float3(uv, 0.0)`。加宽只发生在标量上，收窄只发生在[转换](#转换)一节列出的
位置。构造器上的输出选择器（`Output=`、`OutputIndex=`）是 `DSH5253`。

<a id="常量折叠"></a>

### 常量折叠

当每个参数都是常量时，整个调用是一个常量节点：`Constant` / `Constant2Vector` / `Constant3Vector` /
`Constant4Vector`。常量是数值字面量、被取负的字面量、`const` 属性、持有常量的变量，或常量上的任何算术：
折叠 pass 会折叠操作数全是常量的运算符、构造器、swizzle 和内置 （自 2.0.0 起）。

| 写法 | 折叠为 |
| :-- | :-- |
| `vec2(0.5, 1.0)` | `Constant2Vector(0.5, 1.0)` |
| `vec3(0.5)` | `Constant3Vector(0.5, 0.5, 0.5)`——广播也会折叠 |
| `float4(1.0, 2.0, 3.0, -1.0)` | `Constant4Vector(1, 2, 3, -1)` |
| `int3(1, 2, 3)` | `Constant3Vector(1, 2, 3)` （自 2.0.0 起）；对编译器它仍是 `int3` |
| `vec3(K, 0.0, 0.0)` | `K` 是参数时不折叠——两个 `AppendVector` |

折叠出来的值与图里同一个节点算出的值一致：常量 `sin` / `cos` 按周期计算，和 `Period = 1` 的 Sine /
Cosine 节点一样，所以 `sin(0.25)` 与 `T = 0.25` 时的 `sin(T)` 都是 1 （自 2.1.2 起）；`round` 把正好一半的
值舍入到偶数 （自 2.1.2 起）。见[数学内置](../builtins/math.md)。

相等的常量是一个节点：`vec3(0.5)` 写五次也只有一个 `Constant3Vector`。

<a id="swizzle"></a>

## Swizzle

```c
<swizzle>  := <expression> . <channels>
<channels> := <channel> [ <channel> ] [ <channel> ] [ <channel> ]
<channel>  := { x | y | z | w | r | g | b | a }
```

| 字符 | 通道索引 |
| :-- | --: |
| `x`、`X`、`r`、`R` | 0 |
| `y`、`Y`、`g`、`G` | 1 |
| `z`、`Z`、`b`、`B` | 2 |
| `w`、`W`、`a`、`A` | 3 |

只存在两套字符集：`xyzw` 和 `rgba`。没有 `stpq`，也没有 `uv`——`.uv` 是 `DSH4230`。通道字符不区分大小写。
**在一个 swizzle 里混用两套字符集是 `DSH4230`** （自 2.0.0 起）（1.x 把 `.xg` 当作 `.xy`）。

<a id="长度与边界"></a>

### 长度与边界

| 规则 | 违反时 |
| :-- | :-- |
| 一到四个通道字符 | `DSH4230` |
| 每个字符都必须是通道字母 | `DSH4230` |
| 每个通道索引都必须**小于**基值的分量数 | `DSH4230` |
| 一个 swizzle 只用一套字符 | `DSH4230` |

| 基值 | Swizzle | 结果 |
| :-- | :-- | :-- |
| `float4` | `.rgb` | `float3` |
| `float3` | `.a` | 错误——通道 3 不小于 3 |
| `float2` | `.xyz` | 错误 |
| `float` | `.x`、`.r` | 基值本身 |
| `float` | `.xx`、`.rrr` | 复制成 `float2` / `float3` |
| `float` | `.y`、`.g`、`.a` …… | **错误** |

**警告：**

**标量不会通过越界通道广播。** 对标量参数写 `Roughness.yz` 是 `DSH4230`，而不是双分量广播。要广播标量，
请重复通道 0（`Roughness.xx`）、使用构造器（`float2(Roughness)`），或者依赖赋值处的标量加宽。

<a id="降级"></a>

### 降级

（自 2.0.0 起） 起 swizzle 是一个节点：生成器不再把通道掩码放在连线上。四种情况，按此顺序尝试：

| # | 适用条件 | 产生的节点 | 结果 |
| --: | :-- | :-- | :-- |
| 1 | 源节点正好有一个发布所选通道的输出引脚——纹理采样的 `.rgb` 或 `.a`、向量参数的 `.rgb` 或 `.r` | **无** | 读那个引脚 |
| 2 | 通道索引**严格递增且不重复** | 一个 `ComponentMask` | 所选通道 |
| 3 | 基值 2–4 分量，且有重排或重复 | 每个不同通道一个单通道掩码（或按情况 1 用引脚），再加 N−1 个 `AppendVector` | 拼接结果 |
| 4 | 基值 1 分量且所有通道都在范围内 | N−1 个 `AppendVector`（单字符时无） | 标量复制 N 次 |

恒等掩码——按顺序取值的全部通道：`float` 上的 `.x`、`float3` 上的 `.rgb`——在 `.dss` 里不产生节点；在 1.x
源文件里会保留成一个 `ComponentMask`，因为 1.x 无论值声明成多宽都会给连线加掩码。

| Swizzle | 通道 | 情况 | 节点开销 |
| :-- | :-- | :-- | :-- |
| `.r` / `.x` | 0 | 2 | 1 个 `ComponentMask` |
| `.rg` / `.xy` | 0,1 | 2 | 1 个 `ComponentMask` |
| `.ga` | 1,3 | 2——递增，允许跳跃 | 1 个 `ComponentMask` |
| `.gr` | 1,0 | 3 | 2 个 `ComponentMask`、1 个 `AppendVector` |
| `.bgr` | 2,1,0 | 3 （自 1.3.3 起） | 3 个 `ComponentMask`、2 个 `AppendVector` |
| `.xxx` | 0,0,0 | 3 | 1 个 `ComponentMask`、2 个 `AppendVector` |
| `float` 上的 `.rr` | 0,0 | 4 | 1 个 `AppendVector` |

源节点有对应引脚时，情况 1 会把表中的 `ComponentMask` 换成引脚。不管 swizzle 写了多少次，每个节点只算一次：
相等的节点会被合并。

**提示：**

**为什么不再用连线上的内联 mask。** 材质图编辑器只信任它能解析回某个已命名引脚的连线掩码；1.8.0 曾为此在
`SceneTexture` 这类节点上把 `CustomStencil.r` 静默改成 `InvSize`。（自 2.0.0 起） 起掩码一律是真节点或
真引脚，编辑器的 Apply / Save 不会再改写任何一条连线。

<a id="复合-swizzle"></a>

### 复合 swizzle

对已经 swizzle 过的值再做 swizzle，会经由已有掩码重新映射，所以编号永远相对于被 swizzle 的那个值。

| 写法 | 含义 |
| :-- | :-- |
| `v.rgb.b` | `v` 的通道 2 |
| `v.ga.r` | `v` 的通道 1——`.ga` 选出 1,3，再由 `.r` 取第 0 项 |
| `v.ga.g` | `v` 的通道 3 |
| `v.bgr.r` | `v` 的通道 2 |

只要结果仍然递增，掩码的掩码会折叠成一个掩码，所以 `v.rgb.rg` 是 `v` 上的一个 `ComponentMask`（或一个引脚）。
选取外层列表里不存在的项，是相对*掩码后*宽度的越界：`v.ga.b` 是 `DSH4230`。常量的 swizzle 会被折叠：
`vec4(1.0, 2.0, 3.0, 4.0).yx` 是一个 `Constant2Vector(2, 1)`。

因为 `.` 是普通后缀运算符，任何调用结果都可以直接 swizzle：

```c
float u  = UE.TexCoord().x;
vec3  c  = SampleTexture2D(Albedo, uv).rgb;
vec2  yx = vec4(1.0, 2.0, 3.0, 4.0).yx;
float m  = MyFunction(A, B).r;
```

在 `Function`、`GraphFunction`、`ShaderFunction` 或 `VirtualFunction` 的调用上，与函数某个输出同名的成员
选中那个输出；函数没有返回值时**任何**成员都被当作输出名（legacy 规则 L3b），不是输出名就是 `DSH5280`。见
[调用](calls.md#输出选择)。多输出 `UE.*` 节点上的成员指名一个输出；只有当这些输出是同一个值的通道
视图时（如纹理采样）才允许 swizzle。

<a id="不能-swizzle-的基值"></a>

### 不能 swizzle 的基值

| 基值 | 行为 |
| :-- | :-- |
| `MaterialAttributes` | 不是 swizzle——成员名按材质属性读取。见[语句与声明](statements.md#materialattributes-成员写入) |
| 纹理对象 | `DSH4206`：纹理的成员是采样方法，必须调用 |
| `Substrate` | `DSH4207`：该值没有这个成员 |

<a id="转换"></a>

## 转换

转换不是作者写出来的。它在值遇到有类型的位置时自动应用。（自 2.0.0 起） 起每个值都有**类型**：1–4 分量、
一种种类（`float`、`half`、`double`、`int`、`uint`、`bool`）的数，一种纹理类型的纹理，`SamplerState`、
`MaterialAttributes` 或 `Substrate`。1.x 源文件可以做、`.dss` 不可以做的事——把更宽的值截短——是 legacy
规则 L22，并会明说。

<a id="应用位置"></a>

### 应用位置

| # | 位置 | 类型来自 | 放不进时的码 |
| --: | :-- | :-- | :-- |
| 1 | 带初始值的声明——`float3 c = A;` | 声明的类型 | `DSH4228` |
| 2 | 赋给 Graph 变量——`c = A;` | 变量的声明类型 | `DSH4228` |
| 3 | 赋给匹配某个 `Outputs` 声明的名字——`Color = A;` | 输出的声明类型 | `DSH4228` |
| 4 | `MaterialAttributes` 成员写入 | 属性自身的类型 | `DSH4228` |
| 5 | 函数的输入参数 | 输入的声明类型 | `DSH4226` |
| 6 | `UE.*` / `Substrate.*` 引脚、纹理采样的坐标 | 引擎目录里该引脚的类型 | `DSH5214` |
| 7 | `if` 条件 | 一个真假值 | `DSH4260` |
| 8 | `Shader` 的 `Outputs` 绑定——`Base.EmissiveColor = Color;` | 材质属性的类型 | `DSH4228`，且从不截短 |
| 9 | `+ - * /` 的操作数 | 最宽的操作数：标量广播过去，其余宽度不变 | `DSH4226` |

赋给一个任何大小写下都不匹配任何声明的名字，会以值自身的类型声明它（legacy 规则 L26，提示 `DSH5292`），
没有要转换到的东西。`if` 在两个分支里赋值同一个变量时，合并的是两个同为该变量声明类型的值，合并本身不做
转换。

<a id="规则顺序"></a>

### 规则顺序

| # | 值 → 位置 | 行为 | 节点 |
| --: | :-- | :-- | :-- |
| 1 | 类型相同 | **原样**通过 | 无 |
| 2 | 宽度相同、种类不同的数（`int` → `float`、`float` → `bool` ……） | 种类改变；图里的值不变 | 无 |
| 3 | **从标量加宽**——标量放进更宽的数 | 广播：在读取处接上标量，由材质节点复制 （自 2.0.0 起） | 无；标量是常量时一个 `ConstantNVector` |
| 4 | 更窄的向量放进节点引脚（2 分量进 `float3` 引脚 ……） | 原样接上：节点怎么处理更少的分量是节点的事 | 无 |
| 5 | **收窄**（1.x 源文件）——更宽的向量放进更窄的位置 | 取前导分量（`r`、`rg`、`rgb`），给出提示 `DSH5289` （自 2.0.0 起） | 一个 `ComponentMask`，或源节点上正好对应这些通道的输出引脚；引脚处无 |
| 6 | 多输出节点放进数 | 默认输出类型合适时取默认输出；在 1.x 源文件里，目录没给第一个输出宽度的节点读作第一个输出，**直接传给节点引脚的任何节点**也读作第一个输出（提示 `DSH5287`；2.0.0 – 2.1.0 拒绝后一种情况 （自 2.1.1 起））；否则 `DSH5201` | 无 |
| 7 | 其他 | 以该位置的码拒绝 | —— |

规则 6 的后半句意味着 `UE.TransformPosition(UE.WorldPosition(), …)` 这样的写法又能构建了：
`UE.WorldPosition()` 接到引脚上时取它的第一个输出 `XYZ`；`dsc migrate` 会写成 `UE.WorldPosition().XYZ`。

纵向是输入宽度，横向是期望宽度（1.x 源文件）：

| 输入 \ 期望 | 1 | 2 | 3 | 4 |
| :-- | :-- | :-- | :-- | :-- |
| **1** | 不变 | 广播，无节点 | 广播，无节点 | 广播，无节点 |
| **2** | `.r`，`DSH5289` | 不变 | **错误** | **错误** |
| **3** | `.r`，`DSH5289` | `.rg`，`DSH5289` | 不变 | **错误** |
| **4** | `.r`，`DSH5289` | `.rg`，`DSH5289` | `.rgb`，`DSH5289` | 不变 |

在节点引脚处，表中的两种错误会被接受（规则 4），截短也不产生节点。

跨种类的情况（"报错"即该位置的码）：

| 期望 \ 输入 | 数 | MaterialAttributes | Substrate | 纹理 |
| :-- | :-- | :-- | :-- | :-- |
| **MaterialAttributes** | 报错 | 通过 | 报错 | 报错 |
| **Substrate** | 报错 | 报错 | 通过 | 报错 |
| **纹理（T）** | 报错 | 报错 | 报错 | 纹理类型为 `T` 时通过；接受任意纹理的引脚接受任意纹理 |
| **数（N）** | 见上表 | 报错 | 报错 | 报错 |

纹理只转换到它自己的纹理类型：`Texture2D`、`TextureCube`、`Texture2DArray`、`Texture3D`、`VolumeTexture` 是
五种类型，`SamplerState` 是独立的类型而不是纹理 （自 2.0.0 起）。`TextureCube` 永远不会转换成
`Texture2D`。

<a id="收窄"></a>

### 收窄

在 1.x 源文件里，比目标宽的值保留它的前导分量（legacy 规则 L22）。截短不再是静默的 （自 2.0.0 起）：每一次
都给出提示 `DSH5289`，所以 `dsc check` 能把它们列出来，`dsc migrate` 会写出 swizzle。

```c
vec4   Src = vec4(0.1, 0.2, 0.3, 0.4);
float3 Rgb = Src;      // Src.rgb —— DSH5289；A 通道被丢弃
float  R   = Src;      // Src.r   —— DSH5289
```

显式写出 swizzle（`float3 Rgb = Src.rgb;`）能表明意图，构建的节点相同，提示也随之消失。

收窄**只**在上面的位置 1–7 应用。它被刻意排除在以下位置之外：二元运算符的操作数（`DSH4226`）、构造器参数
（`DSH4222`）、swizzle 边界（`DSH4230`）、`Shader` 的 `Outputs` 绑定如 `Base.Opacity = Color4;`（`DSH4228`，
1.x 在这里也不截短），以及 `.dss` 源文件。

<a id="加宽"></a>

### 加宽

**唯一**的加宽规则是标量广播：1 分量值进入更宽的位置时不产生节点，标量本身接到读取它的地方，由材质节点复制
（自 2.0.0 起）（1.x 构建 `AppendVector` 链）。常量标量会折叠成一个 `ConstantNVector`。

**没有**补零，也**没有**部分加宽。`float3 v = SomeFloat2;` 是 `DSH4228`；请用构造器说明多出来的通道装什么：
`float3 v = float3(SomeFloat2, 0.0);`。唯一接受更窄向量的地方是节点引脚（规则 4）。

<a id="floatint-与-bool"></a>

### float、int 与 bool

编译器知道每个数的种类 （自 2.0.0 起）；生成图不知道，因为材质图里每个值都是浮点。所以种类变化不会改变
图里的任何东西。

| Token 家族 | 对转换意味着什么 |
| :-- | :-- |
| `float`、`float1..4`、`half`、`half1..4`、`vec2..4` | 种类为 `float` 或 `half` 的 1 / 2 / 3 / 4 分量 |
| `int`、`int2..4`、`ivec2..4` | 种类 `int`——不截断、不舍入 |
| `uint`、`uint2..4`、`uvec2..4` | 种类 `uint`——不钳制范围、不处理符号 |
| `bool`、`bool2..4`、`bvec2..4` | 种类 `bool`——不归一化到 0 或 1 |
| `StaticBool`、`StaticBoolParameter` | 读作 `bool`，1 分量 |

- `int x = 7.9;` 存的就是 7.9；想截断请用 `floor(…)` 或 `trunc(…)`。
- `bool b = 0.5;` 存的就是 0.5；没有到 0 或 1 的转换，想要 0/1 请比较。
- 显式转换也遵循同一条规则：`int(0.5)`、`bool(0.5)` 保留 0.5，常量求值时也一样 （自 2.1.2 起）——之前的
  版本在折叠常量时会截断，可能翻转一个常量条件。`Function` 函数体里的转换是 HLSL 转换。
- 把 `float4` 赋给 `int3` 的收窄与 `float4` → `float3` 完全一样（`DSH5289`）。
- 种类只在一处要紧：两个整数构造器调用之间的 `/` 是 `DSH4243`，见[整数除法](#整数除法)。

<a id="权威分量数"></a>

### 权威分量数

（自 2.0.0 起） 起**不存在了**。1.x 让一些值——常量折叠的构造器、`dot`、一张已知宽度表里的 `UE.*`
内置——带有能覆盖声明、能左右运算符的"权威"宽度。现在每个值都有其类型的宽度：

| 值 | 宽度 |
| :-- | :-- |
| 变量 | 声明的宽度——始终生效 |
| 字面量 | 1 |
| 构造器，折叠与否 | 其类型的宽度 |
| 内置 | 操作数中最宽的，或内置的返回宽度（`dot`、`length`、`distance`：1） |
| `UE.*` / `Substrate.*` 节点输出 | 引擎目录给该输出的类型；目录没给宽度的输出跟随节点最宽的输入，输入都不比标量宽时跟随值去往的位置 |
| swizzle | 它的通道数 |

所以 `float2 dir = UE.CameraVectorWS();` 是 `float2`——前两个分量，`DSH5289`——而 1.x 存的是三个分量。
两个宽度不同的操作数只能经由标量广播相遇，其余都是 `DSH4226`。

<a id="节点复用"></a>

## 节点复用

（自 2.0.0 起） 起复用是在整个资产构建完之后，跑在编译器中间图上的一道 pass，而不是 1.x 生成器边建节点
边查的缓存。规则还是那一条——类相同、参数相同的两个节点是一个节点——但它作用于**每一个**节点。

pass 作用于**一个**生成资产的图：一个 `Shader`，或一个 `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend`。
同一个 `.dsm` 里的材质和两个函数资产是三张独立的图，彼此不共享。

| 顺序 | Pass | 做什么 |
| --: | :-- | :-- |
| 1 | 剪枝 | 合并之前先丢掉没人读的节点 |
| 2 | 折叠 | 操作数全是常量的运算符、内置、构造器或 swizzle 变成一个常量 |
| 3 | swizzle | 掩码的掩码在保持递增时合成一个；恒等掩码不产生节点（1.x 源文件除外） |
| 4 | 去重 | 合并相等的节点 |
| 5 | 剪枝 | 丢掉任何输出都到达不了的东西；没人读的参数给出提示 `DSH4390` |

2–4 会重复，最多四轮，直到某一轮什么都没改变。这道 pass **不**按块划分作用域：then 分支里写的子表达式，在
else 分支里或 `if` 之后再写一遍，是同一个节点。

两个节点在以下各部分全都相等时是一个节点：运算（运算符、内置、常量、参数、反射的 `UE.*` / `Substrate.*`
节点、Custom 节点、函数调用）、类、输出（个数、名字、类型——bool 常量与 float 常量不同）、属性（数值按 9 位
有效数字比较）、按顺序的操作数（所以 `a * b` 和 `b * a` 是两个节点）、按集合比较的输入引脚（所以参数写的
顺序无关）。比较的是**节点**而不是文本：变量代表它持有的节点，所以在两次文本相同的表达式之间重新赋值会得到
两个节点，而 `F_Tint(a, b)` 与 `F_Tint(Color = a, Tint = b)`、`1` 与 `1.0` 是同一个节点。

1.x 从不共享、现在相等时会合并的东西：

| 写法 | 1.x | 现在 |
| :-- | :-- | :-- |
| `Function` 和 `GraphFunction` 调用 | 每个调用点一个新 `Custom` 节点 | 每种不同的参数表一个节点 |
| Custom 类的 `UE.Expression` | 从不共享 | 代码和输入相等时共享 |
| `UE.TexCoord`、`UE.Time`、`UE.Panner`、`UE.TransformVector`、`UE.TransformPosition` 和无参状态读取 | 每个调用点一个节点 | 每种不同的参数表一个节点 |
| `UE.CollectionParam` / `UE.CollectionParameter` | 每个调用点一个节点 | 每个集合加参数一个节点 |
| `MakeMaterialAttributes`、`SetMaterialAttributes` 和属性读取 | 每次声明、写入、读取一个节点 | 相等时共享 |
| `If` | 每个 `if` 每个被合并的变量一个 | 条件和两个值都相等时共享 |
| `StaticSwitchParameter` 调用 | 从不共享 | 每对不同的分支一个节点 |
| 参数与属性节点 | 每个作用域一次，`if` 之后再一次 | 每个参数加参数表一个节点 |

从不合并的：语句性质的节点（材质输出、函数输出、`UE.DreamPassOutput` 这类自定义输出节点）、函数输入
（`FunctionInput`）、在任何一个属性、引脚或操作数上不同的两个节点（`UE.TexCoord(Index = 0)` 与
`UE.TexCoord(Index = 1)`；参数的引脚调用与它的裸读取）。

**提示：**

因此：一个资产里同一个数值的所有出现都是**一个** `Constant`（`1` 和 `1.0`、`0.5` 和 `.5`）；同一个 `-x` 写
两次是一个 `Multiply`，而两个不同的取负各有自己的 `Constant(-1)`（1.x 共享一个）；`UE.TexCoord(Index = 0)`
写五次是**一个** `TextureCoordinate`（1.x 是五个）；在分支内部首次读取、在 `if` 之后再次读取的参数是一个节点
（1.x 会留下重复节点）。节点布局在合并后的图上计算，所以被合并的节点只有一个位置，不留空位。

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH2151: Expected an expression, found {0}.` | 该出现操作数的地方出现了别的东西，例如 x = ; 或 a \* )。 |  |
| `DSH2152: Expected ')' to close an argument list, found {1}.` | 调用或括号表达式缺少 )；也常见于两个参数之间缺了 ,，例如 f(a b)。 | 补上 ) 或 ,。 |
| `DSH2161: Expected a member or swizzle name after '.', found {0}.` | . 后面不是名字。 |  |
| `DSH5260: Expected a name after '::', found {0}.` | :: 后面不是名字。 | 写成 N::F(...)。 |
| `DSH2101: Unexpected character '{0}' in source.` | 根本不是记号的字符：@、$、反引号、从文档里粘进来的全角标点或不换行空格。 | 删掉这个字符；看起来普通的往往是不可见字符，重打这一行。 |
| `DSH2105: Malformed number literal '{0}'.` | 看起来像数又不是数：第二个 .、没有数字的指数、浮点与整数后缀并用、带小数的数用整数后缀、末尾粘着字母。 |  [详解](#数值字面量) |
| `DSH2222: Expected a decimal number in a 1.x Graph expression, found the hexadecimal literal '{0}', which 1.x read as 0 followed by a name; move this code to a .dss file.` | 1.x Graph 里的十六进制字面量；1.x 实际读出来的值是 0。 | 改写成十进制。 |
| `DSH2103: Unterminated string literal; expected a closing '"'.` | 字符串到行尾还没有闭合的引号——常见于以反斜杠结尾的路径。 | 闭合字符串；路径用正斜杠。 |
| `DSH2104: Unknown escape sequence '\{0}' in a string literal.` | 字符串里的反斜杠后面不是六种已知转义之一。 | 反斜杠写两遍，或改用正斜杠。 |
| `DSH4202: A string has no value in an expression; it is only ever the value of a reflected property, as in 'UE.Expression(Class = "...")'.` | 字符串字面量被当作值求值。 |  |
| `DSH4226: {0} expects {1}, and this is {2}.` | 操作数不是数，或两个操作数宽度不一致且都不是标量；也用于构造器参数和函数输入。 | 显式 swizzle 较宽的一方，或用构造器构建较窄的一方。标量可以广播到任意宽度。 |
| `DSH4243: Both sides of this '/' are integers, and the material graph has no integer division; write 'float(a) / b' for the fraction, or 'floor(float(a) / b)' for the whole part.` | / 的两个操作数都是整数构造器调用，如 int(7) / int(2)。整数字面量和 int 变量之间的 / 是浮点除法（legacy 规则 L27）。 |  [详解](#整数除法) |
| `DSH5293: A Substrate value has no compound assignment; write 'S = S + T' (Substrate.Add) or 'S = S * w' (Substrate.Weight).` | Substrate 值没有的运算符：只有 A + B、乘以标量和 lerp。 |  [详解](../builtins/substrate.md) |
| `DSH4204: '{0}' is a type, not a value; write '{0}(...)' to construct one.` | 类型名出现在需要值的地方——包括名字恰好是类型拼法的变量，例如 Half、Float3。 | 构造一个值，或给变量换名字。 |
| `DSH4203: '{0}' is a namespace, not a value; write '{0}.SomeNode(...)'.` | 把 UE 或 Substrate 当作值读取。 |  |
| `DSH4225: A constructor takes its components in order; named arguments belong on 'UE.' nodes and on function calls.` | 构造器调用里出现了 name = value 参数。 |  |
| `DSH4221: {0}() has no components; write the value, as in 'float3(0.0)'.` | 没有参数的构造器。 |  |
| `DSH4222: '{0}' has {1} fields and this list has {2}.` | 各参数宽度之和不等于构造器宽度，且调用不是单个标量。 | 数一下分量：float4(v3, w) 是 3 + 1；float3(v4) 请写 v4.xyz。 |
| `DSH4223: {0} expects {1}, and this is {2}.` | 没有构造器的类型：MaterialAttributes、Substrate、纹理类型、SamplerState。 |  |
| `DSH4361: This expression is {0} and the material graph has no matrices; compute it inside the custom body instead.` | 矩阵构造器或矩阵值。 | 坐标空间变换用 UE.TransformVector / UE.TransformPosition；矩阵运算放进 Function 函数体。 [详解](../builtins/ue.md) |
| `DSH4230: '.{0}' is not a swizzle; a swizzle is one to four of 'xyzw' or 'rgba'.` | 不是通道字母的字符、超过四个字符、通道越过基值宽度，或混用两套字符集。 | 只用一套字符并保持在范围内。 |
| `DSH4206: '{0}' on a texture is a call: write 'Tex.{0}(UV)'.` | 对纹理对象写了成员访问。 |  |
| `DSH4207: A value of type {0} has no member '{1}'.` | 对没有这种成员的值写了成员访问，例如 Substrate 值。 |  |
| `DSH4228: {0} expects {1}, and this is {2}.` | 值放不进它要存进的变量、输出或属性——例如把 2 分量加宽到 3 分量。 | 用构造器：float3(v, 0.0)。 |
| `DSH5289: {0} expects {1}, and this is {2}: its leading components are taken, which is what 1.x did; a '.dss' writes the swizzle.` | 提示（legacy 规则 L22）：更宽的值被截成前导分量。 | 不必改；显式写出 swizzle 可消除提示。 |
| `DSH5287: '{0}' has more than one output and is read as its first, '{1}', which is what 1.x did; a '.dss' names the output.` | 提示（legacy 规则 L3c）：多输出节点被当作值使用而没说取哪个输出，按 1.x 取第一个。 | 不必改；dsc migrate 会写出输出名（.XYZ、.RGB）。 |
| `DSH5201: {0} expects {1}, and this node has more than one output; name the one you mean.` | 多输出节点被当作值使用而没有指名输出，且它的默认输出放不进去；或成员指名的输出不存在。 | 指名一个输出，如 UE.VertexColor().A。 [详解](../builtins/ue-expression.md) |
| `DSH4200: '{0}' is not declared; did you mean '{1}'? Names are case-sensitive.` | 既不是 Graph 变量、参数也不是文件作用域声明的名字，或忽略大小写时匹配到多个——包括按名字读取 StaticSwitchParameter。 |  [详解](calls.md#staticswitchparameter) |
| `DSH5275: '{0}' matches '{1}' only in case; a 1.x source is read ignoring case, so this is '{1}', and a '.dss' needs the exact spelling.` | 警告（legacy 规则 L19）：名字只在忽略大小写时匹配到它的声明。 | 按声明的拼写写名字。 |
| `DSH2211: Expected a call or an assignment as a 1.x Graph statement, found an expression whose value is never used.` | 既不是调用也不是赋值的表达式语句，例如 a + b;。 | 把值赋给变量或输出，或删掉这一行。 |

<a id="示例"></a>

## 示例

```c
Shader(Name="Docs/M_Expressions")
{
    Properties {
        vec3 A = vec3(1.0, 0.5, 0.2);
        vec3 B = vec3(0.1, 0.2, 0.3);
        float K = 2.0;
    }
    Settings { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs { vec3 Color; Base.EmissiveColor = Color; }
    Graph {
        vec3 Sum    = A + B;
        vec3 Diff   = A - B;
        vec3 Scaled = A * K;
        vec3 Ratio  = A / K;
        vec3 Neg    = -Scaled;
        Color = (Sum + Diff - Scaled + Ratio) * 0.25 + Neg;
    }
}
```

```text
VectorParameter A, VectorParameter B, ScalarParameter K   (property nodes)
Add(A, B)                                        -> Sum
Subtract(A, B)                                   -> Diff
Multiply(A, K)                                   -> Scaled
Divide(A, K)                                     -> Ratio
Multiply(Scaled, Constant(-1))                   -> Neg
Add / Subtract / Add chain, Multiply(..., 0.25), Add(..., Neg)
```

<a id="参见"></a>

## 参见

- [语句与声明](statements.md) —— 表达式出现在哪些语句形式里
- [调用](calls.md) —— 调用语法、命名参数、输出选择
- [数学内置](../builtins/math.md) —— `fmod`、`pow`、`min`、`max`、`lerp`、`saturate`
- [UE.* 节点](../builtins/ue.md) —— 各内置的输出类型
- [不支持的写法](unsupported.md) —— 所有被拒绝的写法及其码
- [迁移到 .dss](../tools/migrate.md) —— 本页每条 legacy 规则在 `.dss` 里的写法
