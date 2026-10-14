---
title: 不支持的写法
description: 循环、return、三元、%、比较与下标——Graph 块不实现的每一种 HLSL / GLSL 写法、编译器为它报的码，以及应该改写成什么。
---

`Graph` 块是节点图构建器，不是着色器编译器。它只有固定的几种语句形式、四个算术运算符，除 `if` 之外没有任何
控制流。其余一切都会带诊断码被拒绝。（自 2.0.0 起） 起，不会再有任何写法被编译成比你写下来的更少的东西：
1.x 生成器遇到第一个不认识的运算符就静默丢掉表达式的其余部分（`a % b` 构建出 `a`），而现在每一种这样的写法
都是错误，信息里还会说明 1.x 当时做了什么。

如果你是从 HLSL 或 GLSL 过来的，这一页应该最先读。想写循环、`?:`、`&&` 或位运算，请把代码搬进
[`Function`](../language/functions.md)，或用 [`dsc migrate`](../tools/migrate.md) 把文件迁到 `.dss`——
那里有这些写法。

<a id="一种写法是怎样被拒绝的"></a>

## 一种写法是怎样被拒绝的

legacy 前端用 2.0 的语句与表达式解析器读取 `Graph` 块体，它的文法是 1.x 文法的超集；然后再拒绝 1.x 没有的
东西：

| 阶段 | 拒绝什么 | 码 |
| :-- | :-- | :-- |
| 词法器 | 根本不是记号的字符（`@`、`$`、反引号）、写坏的数（`0.5.5`、`1.0fx`） | `DSH2101`、`DSH2105` |
| 解析器 | 不是 2.0 文法里语句或表达式的文本：缺了 `;`、`)` 或名字，`switch` | `DSH2150`–`DSH2165` |
| legacy 限制检查 | 2.0 文法有、1.x 没有的写法：运算符、`[ ]`、强制转换、循环、`return`、复合赋值 | `DSH2200`–`DSH2222` |
| 绑定器 | 能解析但没有意义的写法：字符串值、整数除法、宽度不匹配 | `DSH42xx` |

每条诊断都带该写法的行列。一个阶段会报告它的全部错误，有错误的阶段会在下一阶段之前停止构建。

<a id="1x-会截断的运算符"></a>

## 1.x 会截断的运算符

下面每一种 1.x 都会接受，然后只构建运算符前面的部分，没有任何信息。（自 2.0.0 起） 起每一种都是错误。

| 写法 | 码 | 应改写为 |
| :-- | :-- | :-- |
| `a % b` | `DSH2201` | `fmod(a, b)` |
| `a && b`、`a \|\| b`、`!a` | `DSH2200` | 嵌套 `if` 语句，或对 0/1 掩码用 `max` / `min` |
| `a & b`、`a \| b`、`a ^ b`、`~a` | `DSH2201` | 带 HLSL 函数体的 `Function` |
| `a << 2`、`a >> 2` | `DSH2201` | `a * 4.0`、`a / 4.0`，或一个 `Function` |
| `if` 条件之外的 `a < b`、`a > b`、`a <= b`、`a >= b`、`a == b`、`a != b` | `DSH2200` | [`if` 条件](if.md#条件) |
| `a ? b : c` | `DSH2200` | `if` / `else`、`lerp(c, b, mask)`，或 `StaticSwitchParameter` 调用 |
| `v[0]` | `DSH2202` | [swizzle](expressions.md#swizzle)：`v.x` |
| `if` 条件里的第二个比较、`&&` 或 `\|\|` | `DSH2210` | 嵌套 `if` 语句 |

```c
// 被拒绝（DSH2201）；1.x 构建出的是 UE.Time()。
float Wave = UE.Time() % 1.0;

// 正确写法：
float Wave = fmod(UE.Time(), 1.0);
```

```c
// 被拒绝（DSH2210）；1.x 构建出的是 if (Color.r > 0.5)。
if (Color.r > 0.5 && Color.g > 0.5) { Out = One; } else { Out = Zero; }

// 正确写法：
if (Color.r > 0.5) {
    if (Color.g > 0.5) { Out = One; } else { Out = Zero; }
} else {
    Out = Zero;
}
```

限制检查对一个被拒绝的写法只报一次、不再看它内部，所以 `a % b % c` 是一条诊断，`a % b + c % d` 是两条。

**提示：**

依赖过截断的 1.x 源文件，从 2.0.0 起会报错而不是悄悄构建出一半的逻辑。这正是这些码的用处：以前"生成材质
看起来丢了一半逻辑"的那类问题，现在在构建时就会指到具体的行列上。

<a id="语句层面的写法"></a>

## 语句层面的写法

| 写法 | 码 | 应改写为 |
| :-- | :-- | :-- |
| `return Color;`、`return;` | `DSH2208` | 给声明的输出变量赋值：`Color = …;` |
| `for (int i = 0; i < 3; i = i + 1) {}` | `DSH2208` | 手工展开，或把循环搬进函数体是真 HLSL 的 [`Function`](../language/functions.md) |
| `while (t) {}`、`do {} while (t);` | `DSH2208` | 同上 |
| `break;`、`continue;`、`discard;` | `DSH2208` | —— |
| `switch (Mode) {}` | `DSH2160` | 嵌套 [`if`](if.md) 语句，或 [`StaticSwitchParameter` 调用](calls.md#staticswitchparameter) |
| `float Foo(float x) { return x; }` | `DSH2154`，在本该放声明 `;` 的 `(` 处 | 在顶层声明成 `Function` 或 `GraphFunction` 再调用 |
| `struct S { float a; }` | `DSH2151`：`struct` 不能开始一个表达式 | 用 `MaterialAttributes`，或把结构体放进 `Function` 函数体 |
| 前面没有 `if` / `else` 的 `{ … }` | `DSH2220` | 去掉花括号 |
| 变量上的 `static` / `const` | `DSH2212` | `const` 属性 |
| `float w[4];` | `DSH2213` | 多个独立变量 |
| `a + b;`——没人接收的值 | `DSH2211` | 把它赋给变量 |
| `#include "X.ush"`、`#pragma …` | `DSH1035`：`.dsm` / `.dsf` 的预处理器不认识这样的行 | 文件级 [`import`](../language/import.md)，或一个使用该 include 的 `Function` |
| `#define K 2` | Graph 里没有诊断：这一行属于[预处理器](../language/lexical.md#预处理器)，它在 `#if` 条件里使用 define | `const` 属性 |

`Graph` 块体按语句读取，而不是按文本在 `;` 处切分，所以带花括号的写法后面的代码不会再被吞进同一条语句
（自 2.0.0 起）。

<a id="表达式层面的写法"></a>

## 表达式层面的写法

| 写法 | 码 |
| :-- | :-- |
| `x = a == b;` | `DSH2200` |
| `a == b;`（*作为语句*） | `DSH2211`——比较的值无处可去 |
| `x = a++;`、`a++;`、`--a;` | `DSH2207` |
| `x = (float)a;` | `DSH2203`——没有 C 风格强制转换，请写 `float(a)` |
| `x = (y = z);` | `DSH2204` |
| `x = 0x10;` | `DSH2222`——没有十六进制字面量；1.x 把 `0x10` 读成 `0` |
| `x = 1.0fx;`、`x = 0.5.5;` | `DSH2105` |
| `x = "text";` | `DSH4202` |
| `x = {1, 2, 3};`、`Attrs.BaseColor = {1, 0, 0};` | `DSH2162`——花括号列表只作为声明的初始值读取 |
| `float4 v = {{1,2},{3,4}};` | `DSH2214`——花括号初始值不能嵌套 |
| `f(a b)` | `DSH2152`——缺少 `,` |
| `if (a) Color = X;` | `DSH2209`——花括号是必需的 |
| `@`、`$`、反引号 | `DSH2101` |

<a id="复合赋值与自增自减"></a>

## 复合赋值与自增自减

`+= -= *= /= %=`、位运算的复合形式、`++` 和 `--` 都不是 1.x 运算符。

| 写法 | 码 |
| :-- | :-- |
| `a += b;`、`a -= b;`、`a *= b;`、`a /= b;`、`a %= b;` | `DSH2205` |
| `a++;`、`a--;`、`++a;`、`--a;` | `DSH2207` |

（自 2.0.0 起） 起空白不再有影响。1.x 把 `a += b;` 读成类型为 `a`、名为 `+` 的变量声明，把 `a+=b;` 读成对
一个名叫 `a+` 的全新变量的赋值，`a` 保持不变且没有任何信息；`x = --a;` 被读成 `-(-a)`。现在这些都是错误。
请写展开形式：`a = a + b;`。

```c
// 被拒绝（DSH2205）。
Sum+=Tint;

// 正确写法：
Sum = Sum + Tint;
```

而且这里本来就没有可以自增的存储——Graph 变量是绑定到某个节点上的名字，不是内存单元。

<a id="大小写写错的关键字"></a>

## 大小写写错的关键字

`if` 和 `else` 是关键字，**区分大小写**，`for`、`while`、`return`、`struct` 等 HLSL 关键字也一样。大小写写错的
拼法是一个名字。

| 写法 | 结果 |
| :-- | :-- |
| `If (x) {}`、`IF (x) {}` | `If (x)` 是对一个叫 `If` 的函数的调用，本该放这条语句 `;` 的位置出现了 `{`：`DSH2154` |
| `if (x) {} Else {}`、`ELSE {}` | `if` 能解析；`Else` 是一个名字，它的 `{` 是 `DSH2154` |

类型 token、构造器名和 `true` / `false` 不区分大小写。其他名字先精确查找，再不区分大小写，后者带警告
（`DSH5275`；引擎名为 `DSH5276`）。另一个区分大小写的名字是 `SampleTexture2D`。

<a id="根本不存在的特性"></a>

## 根本不存在的特性

| 特性 | 状态 | 替代方案 |
| :-- | :-- | :-- |
| 矩阵类型（`float3x3`、`float4x4`、`mat2`、`mat3`、`mat4`） | 图里没有矩阵值：`DSH4361` | `UE.TransformVector` / `UE.TransformPosition`，或把矩阵局部变量放在 `Function` 函数体**内部** |
| 数组与下标 | `DSH2213`、`DSH2202` | 通道用 [swizzle](expressions.md#swizzle)；层用 `Texture2DArray` 采样 |
| `inout` 参数 | 1.x 函数只有 `in` 和 `out`：`DSH6302` | 传一个 `in` 加一个 `out` |
| 在 `Graph` 内声明函数 | `DSH2154` | 顶层的 [`Function` / `GraphFunction`](../language/functions.md) |
| 整数运算 | 没有——`int`、`uint`、`bool` 和 `half` 在图里都是浮点；两个整数构造器之间的 `/` 是 `DSH4243` | 见[整数除法](expressions.md#整数除法) |
| 十六进制、八进制、二进制字面量 | 十六进制是 `DSH2222` | 十进制字面量 |
| 字符串值 | `UE.*` 命名参数之外是 `DSH4202` | —— |
| 三元条件 | `DSH2200` | `if` / `else`、`lerp`、`StaticSwitchParameter` |
| 逗号运算符 | 没有 | 拆成多条语句 |
| 表达式内赋值 | `DSH2204` | 拆成多条语句 |
| 嵌套花括号初始值 | `DSH2214` | 用构造器调用：`float4(float2(1,2), float2(3,4))` |
| 预处理指令 | 只有条件编译那一组和 `#Region` / `#EndRegion` | `import`、`const` 属性 |
| `Shader` 里的 `Code = { … }` | **已移除**——`DSH2246` | `Graph = { … }` |

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH2200: Expected only '+', '-', '*', '/', calls and members in a 1.x Graph expression, found '{0}', where 1.x silently dropped the rest of the expression; move this code to a .dss file.` | if 条件之外的比较、逻辑运算符（&amp;&amp;、\|\|、!）或 ?:。1.x 会在这里停止读取，静默使用已读到的部分。 | 把选择放进 if / else，或用 UE.If(...) 这样的节点；或把代码迁到 .dss。 |
| `DSH2201: Expected only '+', '-', '*', '/', calls and members in a 1.x Graph expression, found '{0}', where 1.x silently dropped the rest of the expression; move this code to a .dss file.` | %、移位或位运算符。 | 取余用 fmod(a, b)；位运算搬进 Function 或 .dss。 [详解](../builtins/math.md) |
| `DSH2202: Expected no '[ ]' in a 1.x Graph expression, found '{0}', where 1.x silently dropped the index and everything after it; use a swizzle such as '.r' or move this code to a .dss file.` | 用 \[ \] 下标。 | 用 swizzle 取分量；按序号读节点输出写 OutputIndex = k。 |
| `DSH2203: Expected a constructor such as 'float3(x)' in a 1.x Graph expression, found the cast '({0})', which 1.x never had; move this code to a .dss file.` | C 风格强制转换，例如 (float3)x。 | 写构造器：float3(x)。 |
| `DSH2204: Expected an assignment only as a whole 1.x Graph statement, found one inside an expression; move this code to a .dss file.` | 赋值被当作表达式里的值。 | 拆成两条语句。 |
| `DSH2205: Expected '=' in a 1.x Graph assignment, found '{0}', which 1.x read as the declaration of a variable named '{1}'; write 'x = x + y' or move this code to a .dss file.` | 复合赋值（+=、\*= ……），无论有没有空格。 | 写展开形式：x = x + y; |
| `DSH2207: Expected no '++' or '--' in a 1.x Graph expression, found '{0}'; move this code to a .dss file.` | ++ 或 --，包括前缀形式。 | 写 x = x + 1.0; |
| `DSH2208: Expected a declaration, an assignment, a call or 'if' in a 1.x Graph body, found '{0}', which 1.x did not have; move this code to a .dss file.` | for、while、do、return、break、continue 或 discard。 | 循环和提前退出搬进 Function（HLSL），或迁到 .dss。 [详解](../language/functions.md) |
| `DSH2209: Expected braces around the body of a 1.x Graph 'if', found a single statement.` | if 或 else 块体不带花括号。 | 给块体加上 { }。 |
| `DSH2210: Expected at most one comparison in a 1.x Graph 'if' condition, found '{0}' as well, where 1.x silently dropped the rest of the condition; move this code to a .dss file.` | if 条件里多于一个比较，或用了 &amp;&amp; / \|\|。 | 改写成嵌套 if。 [详解](if.md) |
| `DSH2211: Expected a call or an assignment as a 1.x Graph statement, found an expression whose value is never used.` | 值无人接收的表达式语句，例如 a + b; 或 a == b;。 | 把值赋给变量或输出，或删掉这一行。 |
| `DSH2220: Expected no bare block in a 1.x Graph body, found one; 1.x has blocks only after 'if' and 'else'.` | 前面没有 if / else 的 { … }。 | 去掉花括号。 |
| `DSH2222: Expected a decimal number in a 1.x Graph expression, found the hexadecimal literal '{0}', which 1.x read as 0 followed by a name; move this code to a .dss file.` | 十六进制字面量。 | 改写成十进制。 |
| `DSH2160: Unsupported statement: the preprocessor line '#{0}' cannot appear inside a function body; mark the function /// @custom to hand its body to the shader compiler.` | 语句开头是 switch、case 或 default（也包括函数体里的 # 行）。 | 把 switch 改写成 if / else if 链，或用 StaticSwitchParameter。 |
| `DSH2154: Expected ';' after an expression statement, found {1}.` | 语句没有以 ; 结束：大小写写错的 If、Graph 里的函数定义、末尾缺 ; 的最后一条语句。 | 补上 ;，或修正关键字的大小写。 |
| `DSH2151: Expected an expression, found {0}.` | 该出现表达式的地方出现了别的东西：struct、= 5;、if ()。 |  |
| `DSH2152: Expected ')' to close an argument list, found {1}.` | 缺少 )——常见于两个参数之间缺了 ,。 |  |
| `DSH2162: An initializer list is only allowed as a variable initializer, not as a general expression.` | 声明之外把花括号列表当作值。 | 写构造器。 |
| `DSH2214: Expected an expression as a 1.x Graph initializer, found an initializer list; use a constructor such as 'float3(a, b, c)'.` | 花括号初始值里嵌套了花括号列表。 |  |
| `DSH2212: Expected no storage keyword on a 1.x Graph variable, found '{0}'; move this code to a .dss file.` | Graph 变量上的 static / const。 |  |
| `DSH2213: Expected a single value in a 1.x Graph declaration, found the array declarator '{0}'; move this code to a .dss file.` | 数组声明子。 |  |
| `DSH2101: Unexpected character '{0}' in source.` | 不属于任何记号的字符。 |  |
| `DSH2105: Malformed number literal '{0}'.` | 写坏的数：0.5.5、1.0fx、1e。 |  |
| `DSH1035: {0}({1}): unknown preprocessor directive '#{2}'. {3}` | 条件编译那一组和 #Region / #EndRegion 之外的 # 行——#include、#pragma。 | #include 改写成 import；HLSL 指令放进 Function 函数体。 |
| `DSH2246: Expected 'Graph' as the body section of '{0}', found 'Code', which 1.x accepted only inside a Function.` | Shader 或函数类块里的 Code section。 | 把 section 改名为 Graph。 |
| `DSH4202: A string has no value in an expression; it is only ever the value of a reflected property, as in 'UE.Expression(Class = "...")'.` | 字符串被当作值。 |  |
| `DSH4243: Both sides of this '/' are integers, and the material graph has no integer division; write 'float(a) / b' for the fraction, or 'floor(float(a) / b)' for the whole part.` | / 的两个操作数都是整数构造器调用。 |  [详解](expressions.md#整数除法) |
| `DSH4361: This expression is {0} and the material graph has no matrices; compute it inside the custom body instead.` | 矩阵值。 |  |
| `DSH6302: Expected 'in' or 'out' before the parameter '{0}' of '{1}', found '{2}'; a 1.x function has no 'inout'.` | 1.x 函数参数上的 inout。 | 拆成一个 in 和一个 out。 |

跨阶段的完整清单见[错误速查](../diagnostics/index.md)。

<a id="完整示例"></a>

## 完整示例

一段 GLSL 风格的尝试，以及它的 DreamShaderLang 等价写法：

```c
// 被拒绝。
Graph {
    vec2  uv    = UE.TexCoord(Index = 0);
    float t     = UE.Time() % 4.0;                 // DSH2201
    float mask  = uv.x > 0.5 && uv.y > 0.5;        // DSH2200
    vec3  col   = mask ? Hot : Cold;               // DSH2200
    col        *= Gain;                            // DSH2205
    Color = col;
}
```

四行都会报错，一次构建全部报出来。（1.x 下前三行能编译，材质是错的，只有最后一行报错。）

```c
// 正确写法。
Graph {
    vec2  uv = UE.TexCoord(Index = 0);
    float t  = fmod(UE.Time(), 4.0);

    vec3 col;
    if (uv.x > 0.5) {
        if (uv.y > 0.5) { col = Hot; } else { col = Cold; }
    } else {
        col = Cold;
    }

    Color = col * Gain;
}
```

<a id="什么时候该用-function"></a>

## 什么时候该用 `Function`

任何真正命令式的东西——循环、位掩码、`switch`——都属于 `Function`，它的函数体是真 HLSL，会被编译进一个
`Custom` 节点：

```c
Function float Ring(in float2 uv, in float count)
{
    float acc = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        acc += sin(uv.x * count * (i + 1));
    }
    return acc * 0.25;
}
```

```c
Graph {
    float r = Ring(uv, 8.0);
}
```

如果这个辅助函数还需要读取图节点，请改声明成 `GraphFunction`——它的函数体仍然是 HLSL，但里面的 `UE.*`
调用会被提升成 Custom 节点的输入引脚。见[函数](../language/functions.md)。整个文件都需要这些写法时，用
[`dsc migrate`](../tools/migrate.md) 把它迁到 `.dss`。

<a id="参见"></a>

## 参见

- [语句与声明](statements.md) —— *确实*被支持的语句形式
- [运算符与转换](expressions.md) —— 存在的那四个运算符
- [if / else](if.md) —— 唯一的控制流及其条件运算符
- [数学内置](../builtins/math.md) —— `fmod`、`min`、`max`、`clamp`、`lerp`
- [当前限制](../diagnostics/limitations.md) —— 编译器整体上还做不到的事
