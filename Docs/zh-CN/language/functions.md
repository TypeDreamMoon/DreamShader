---
title: 函数
description: Function、GraphFunction 与 Namespace —— 参数、返回类型、被调函数的内嵌、UE.* 提升，以及调用如何解析。
---

`Function` 和 `GraphFunction` 把可复用逻辑从裸的 `Graph` 语句里挪出来。两者都用 C 风格签名加一段 HLSL
函数体，每个调用点都变成一个 `Custom` 节点，节点代码就是这段函数体；区别在于函数体里的 `UE.*` 调用怎么处理。
legacy 前端把两者都读成 2.0 的 `/// @custom` 函数 （自 2.0.0 起），`dsc migrate` 也这样写。

| | `Function` | `GraphFunction` |
| :-- | :-- | :-- |
| 函数体去向 | 调用它的节点的代码；被别的函数体调用时，作为包装结构体的成员内嵌进那个节点 | 调用它的节点的代码 |
| 函数体里的 `UE.*` 调用 | 原样留作 HLSL 文本，shader 编译器不认识 | 构建成材质表达式，作为 `Custom` 节点输入传入 |
| `Inline` / `SelfContained` | 接受，但不改变任何东西 | `DSH6307` |
| 可被另一个 `Function` 函数体调用 | 可以 —— 会被内嵌 | 只有函数体没提升任何 `UE.*` 调用时才行，否则 `DSH6327` |
| 递归 | `DSH6260` | `DSH6260`；通过提升出的调用回到自己是 `DSH6330` |

不再写共享的 `.ush` include：每个节点的 `Code` 自己带着它需要的全部 HLSL （自 2.0.0 起）。`Namespace` 把
这两种块归到 `Ns::` 前缀之下。

<a id="语法概览"></a>

## 语法概览

```c
Function      [ { Inline | SelfContained } ] [ <return-type> ] <name> ( [ <parameter-list> ] ) { <hlsl> }
GraphFunction                                [ <return-type> ] <name> ( [ <parameter-list> ] ) { <hlsl> }

<parameter-list> := <parameter> [ , <parameter> ] …
<parameter>      := [ { in | out } ] <type-token> <parameter-name>
```

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

它没有 `(Key = Value)` 头部、没有 `Settings`、也没有任何 section。参数列表之后的 `{ … }` 是原始 HLSL，原样
保留：只读它的花括号，字符串和注释里的花括号不算。

关键字 `Function` 和 `GraphFunction` **大小写敏感**（`function` 是 `DSH2240`）；修饰符、`in` / `out` 限定符和
所有类型 token 不敏感。

<a id="声明顺序"></a>

## 声明顺序

解析器从左到右读标识符，靠前瞻消歧：读完一个标识符后，下一个是 `(` 它就是名字；下一个还是标识符，它就是
*返回类型*，再下一个是名字。

| 形式 | 读作 |
| :-- | :-- |
| `Function Name(…)` | 名字 |
| `Function Type Name(…)` | 返回类型，然后名字 |
| `Function SelfContained Name(…)` | 修饰符，然后名字 |
| `Function SelfContained Type Name(…)` | 修饰符、返回类型、名字 |
| `Function Type SelfContained Name(…)` | 返回类型 `Type`、名字 `SelfContained`，然后在 `(` 的位置多出一个 `Name` —— `DSH6319` |

所以修饰符的位置是固定的：紧跟在关键字**之后**的**第一**位。

**警告：**

函数不能**取名**为 `Inline` 或 `SelfContained`。`Function SelfContained(in float a, out float b) { … }`
会走修饰符分支，然后在本该是名字的位置发现 `(`：`DSH6300`。旧的 section 式函数体
`Function Name { Inputs = { … } Code = { … } }` 也是 `DSH6300`。

<a id="参数"></a>

## 参数

一个参数是到下一个 `,` 或 `)` 为止的那些单词，任意空白（包括制表符和换行）都只是分隔。接受 2 或 3 个单词：

| 单词数 | 形式 | 限定符 |
| :-- | :-- | :-- |
| 2 | `<type> <name>` | 默认为 `in` |
| 3 | `<qualifier> <type> <name>` | 按写的来 |
| 少于 2、多于 3，或者含非单词的东西（`[2]`、`= 1.0`） | — | `DSH6301` |

| 规则 | 行为 |
| :-- | :-- |
| 限定符 | 只有 `in` 和 `out`，比较前会小写化，所以 `In`、`OUT`、`iN` 都可以。 |
| `inout` | 1.x 函数里**不存在**：`DSH6302`。拆成一个 `in` 参数和一个 `out` 参数；`.dss` 函数可以用 `inout`。 |
| 空参数 | 单独的 `,` 被跳过，所以 `)` 前的尾随逗号可以接受。 |
| 顺序 | 参数保持声明顺序。`in` 参数是节点的输入引脚，`out` 参数是它的输出。两者可以交错。 |
| 保留名 | 名为 `__return`（忽略大小写）的参数是 `DSH6303`。 |
| 重名 | `DSH4214`。 |
| 不可用 | `opt`、默认值和 `[ … ]` 元数据**不属于**这套语法（`DSH6301`），它们属于 [`Inputs` / `Outputs` section](sections.md)。 |

<a id="至少一个输出"></a>

### 至少一个输出

函数必须产出点什么：**要么**至少有一个 `out` 参数，**要么**有返回类型。两者都没有是 `DSH6305`。

<a id="返回类型"></a>

## 返回类型

| 形式 | 合法 | 结果 |
| :-- | :-- | :-- |
| `Function Name(in T a, out U r)` | 是 | 一个输出 `r` |
| `Function Name(in T a, out U r1, out V r2)` | 是 | `r1` 是节点的第一个输出，`r2` 是附加输出 |
| `Function U Name(in T a) { return expr; }` | 是 | 返回的值就是节点的输出 |
| `Function U Name(in T a, out V r)` | **否** | `DSH6304` |
| `Function Name(in T a)` | **否** | `DSH6305` |
| `Function U Name(…) { return; }` | **否** | 有返回类型时顶层的裸 `return;`：`DSH6308` |
| `Function Name(…, out U r) { … return; … }` | **否** | 没有返回类型的函数里任何深度的 `return;`：`DSH6256` （自 2.0.0 起） |

**有返回类型就意味着恰好一个输出，并且禁止任何显式 `out`。** 多输出必须用 `out` 参数且不写返回类型。

<a id="return-不再改写"></a>

### `return` 不再改写

`return` 原样保留 （自 2.0.0 起）。1.x 把每个顶层 `return` 改写成对合成结果 `__return` 的赋值；现在函数体
本身*就是*返回节点第一个输出的那个 HLSL 函数的函数体 —— `Custom` 节点自己的，或者被内嵌时 helper 的。所以
`return expr;` 在任何花括号深度都返回值，1.x 里嵌在 `if` 中绕过 `__return` 的情况现在和顶层的一样。

仍然检查的有：有返回类型时顶层的裸 `return;`（`DSH6308`）、没有返回类型时任何位置的 `return;`（`DSH6256`），
以及声明了返回类型但函数体里一个 `return` 都没有 —— 这是警告 `DSH6257`，输出会是 0。整个函数体只是一个裸表达式
时会自动补上 `return`。`__return` 这个名字仍然保留。

<a id="接受的参数类型"></a>

## 接受的参数类型

15 个 GLSL 别名在解析时规范化成 HLSL 拼法，换了大小写的标量 / 向量（`Float3`）读成小写拼法。不是类型的 token
在声明处就是 `DSH4201` （自 2.0.0 起）（1.x 要等到函数被调用才失败）。

| Token | 可作 `in` | 可作 `out` / 返回 |
| :-- | :-- | :-- |
| `float`、`half`、`int`、`uint`、`bool`，它们的 `…2`–`…4` 向量和 GLSL 别名 | 可以 | 可以 —— 节点输出是 `Float1`…`Float4` |
| `MaterialAttributes` | **不可以** —— `DSH6210` | 可以 |
| `StaticBool`、`StaticBoolParameter` | 可以 —— 读作 `bool` | 可以 —— 读作 `bool` |
| `Texture2D`、`TextureCube`、`Texture2DArray`、`Texture3D`、`VolumeTexture` | 可以 | **不可以** —— `DSH6254` |
| `SamplerState` | 一个采样器 （自 2.0.0 起）（1.x 读成 `Texture2D`） | **不可以** —— `DSH6254` |
| `Substrate` | **不可以** —— `DSH6253` | **不可以** —— `DSH6253` |
| 矩阵（`float3x3`、`mat3`…） | 不能跨节点引脚：`Graph` 调用时 `DSH4361` | 同左 |

生成的 HLSL 用 2.0 拼法写每个类型：`vec3` 是 `float3`，`MaterialAttributes` 是 `FMaterialAttributes`，
`VolumeTexture` 是 `Texture3D`。

**警告：**

`Substrate` 在 `Function` 和 `GraphFunction` 上永远不可用：任何引擎版本的 `Custom` 节点都没有 Substrate 引脚。
请在 `ShaderFunction` 或 `Shader` 的 `Graph` 里构建 Substrate 部分。

<a id="纹理参数与采样器"></a>

### 纹理参数与采样器

`Texture2D`、`TextureCube`、`Texture2DArray`、`Texture3D`、`VolumeTexture` 是*纹理参数*：在节点上它是一个承载
纹理对象的引脚，引擎会在旁边声明 `SamplerState <Name>Sampler`，在函数体里用这个名字采样。被内嵌成 helper 时，
它的 HLSL 签名会在每个纹理参数后面多出 `SamplerState <Name>Sampler`，别的函数体调用它时也会在纹理实参后传
`<argument>Sampler` —— 这种调用必须传满所有实参（`DSH6262`），纹理实参必须是一个纯名字（`DSH6263`）。参数自己
叫 `<Texture>Sampler` 会和配对的名字撞上（`DSH6255`）。`SamplerState` 参数**不是**纹理参数，没有伴随参数。

纹理参数上的 `<Texture>.Sample(UV)` —— 以及比 HLSL 原形少一个参数的 `SampleLevel`、`SampleBias`、`SampleGrad`
—— 会被改写成引擎的 `Texture2DSample(<Texture>, <Texture>Sampler, UV)` 一族 （自 2.0.0 起）；原生 HLSL 写法
保持原样。

```c
Function SampleTinted(in Texture2D tex, in vec2 uv, in vec3 tint, out vec3 rgb, out float alpha)
{
    float4 texel = Texture2DSample(tex, texSampler, uv);
    rgb   = texel.rgb * tint;
    alpha = texel.a;
}
```

<a id="函数体规范化"></a>

## 函数体规范化

每个 `Function` 和 `GraphFunction` 的函数体 —— 语言里也只有它们 —— 在存储之前会过一遍标识符级别的改写。
扫描跳过注释和字符串，做两件事：替换 18 个别名（15 个 GLSL 类型拼法，加上 `mix` → `lerp`、`fract` → `frac`、
`mod` → `fmod`），以及把 `A::B`（`::` 两边不能有空格）压平成净化后的标识符 `A_B`。

**警告：**

因为别名匹配忽略大小写、且作用于函数体内任意位置的整个标识符，你自己写的名为 `Mix`、`Mod`、`Fract`、
`Vec3` 或 `Mat4` 的 helper 或*变量*会在产出的 HLSL 里被静默改名。没有任何诊断 —— GLSL 拼法的 `DSH5277` 警告只针对
`Graph` 文本。请改名，或者换一个不会撞上的拼法（`MixColor`、`ModValue`）。

压平后的 `Common_Remap01` 正是命名空间函数的名字，所以函数体里的 `Ns::Fn(…)` 调用现在能找到那个函数
（自 2.0.0 起）。

<a id="顶部-include-的提升"></a>

## 顶部 `#include` 的提升

（自 1.8.0 起）

写在函数体**顶部**的 `#include` 指令（前面只允许空白和注释）不属于函数本身。它们从函数体里被抹成空格（之后
每一行、每一列仍然和源码对得上），并被*提升*到每个代码里含这段函数体的 `Custom` 节点的 include 列表
（`IncludeFilePaths`）上：函数自己的节点，以及内嵌它的其他 `Function` 的节点，被内嵌函数的 include 排在前面
（自 2.0.0 起）。

```c
Function float3 SampleWind(in float3 WorldPos)
{
    #include "/Plugin/DreamWind/Shared/DreamWindShared.h"   // 被提升
    FDreamWindSample S = DW_SampleAnalytic(DW_UnpackParams(P0, P1, P2), WorldPos);
    return S.Velocity;
}
```

| 情况 | 处理 |
| :-- | :-- |
| `#include "…"` 或 `#include <…>` | 路径去掉定界符后原样保存，必须是引擎能解析的虚拟 shader 路径（`/Engine/…`、`/Plugin/<Name>/…`） |
| 同一个节点上的重复 | 合并成一条 |
| 空路径 | `DSH6258` |
| 第一条语句**之后**的 `#include` | 不提升，留在生成的函数里 —— 只对纯宏头文件有效 |

引擎把节点的 include 放在文件作用域，所以定义了函数的头文件也能用。这正是让一个 C++ 与 HLSL 共用的头文件能被
直接消费、而不必把它的函数抄进 DreamShaderLang 的东西。

<a id="生成的-hlsl"></a>

## 生成的 HLSL

调用点的 `Custom` 节点把它的全部 HLSL 放在自己的 `Code` 里 （自 2.0.0 起）：

```hlsl
struct generated_wrapper_<Name>_<CRC32>          // 只在函数体调用了其他 Function 时出现
{
	<Type0> DreamShaderFn_<Callee>(<params>)     // 每个被内嵌的 Function 一个成员，
	{                                            // 被调方排在调用方之前
		// Begin DreamShader source: <file>
		// DreamShader custom: <Callee> line <N>
		<callee body, verbatim>
		// End DreamShader source: <file>
		return <Out0>;
	}
};
generated_wrapper_<Name>_<CRC32> __ds_wrapper_<CRC32>;

<Type0> <Out0> = (<Type0>)0;                     // 没有返回类型时的第一个 out
// Begin DreamShader source: <file>
// DreamShader custom: <Name> line <N>
<body, verbatim；对被内嵌函数的调用经过 __ds_wrapper_<CRC32>>
// End DreamShader source: <file>
return <Out0>;                                   // 只在函数体没有顶层 return 时追加
```

| 元素 | 规则 |
| :-- | :-- |
| 符号名 | `DreamShaderFn_` 加上函数名，非 `[A-Za-z0-9_]` 字符换成 `_`、连续下划线塌缩。命名空间成员 `Common::ApplyTint` 就是函数 `Common_ApplyTint`，所以是 `DreamShaderFn_Common_ApplyTint`。 |
| 为什么要前缀 | 不带前缀的 `Luminance(float3)` 会在兄弟成员调用它时遮蔽 `Common.ush` 里的引擎内建。 |
| helper 的返回值 | 它的返回类型；没有返回类型时是第一个 `out` 参数 —— 所以函数体里可以把多输出的 `Function` 当值调用。 |
| 节点自己的函数体 | 不包在 DreamShader 的函数里，它就是 `Custom` 节点的代码。第一个输出是返回值，或第一个 `out` 的局部变量；其余 `out` 是附加输出，由引擎声明并清零。 |
| 标记注释 | 每段函数体周围的 `Begin` / `custom` / `End` 注释行让 shader 编译错误能映射回源码行；任何改写都不会在函数体里增删行。 |

节点的标题（`Description`）是函数名 —— 命名空间成员是 `Ns::Fn`。

<a id="inline--selfcontained"></a>

## `Inline` / `SelfContained`

每个调用点的节点都带着函数自己的函数体，以及这个函数体（传递地）调用的每个其他 `Function`，后者作为
`generated_wrapper_*` 结构体的成员 —— HLSL 没有嵌套函数，而 `Custom` 节点的代码是一个函数体。1.x 只对
`SelfContained` 函数这样做，其他函数引用共享的 include；2.0 没有共享 include，所以每个函数都得到了
`SelfContained` 当初要求的东西，这个修饰符读进来后什么都不改变 （自 2.1.1 起）。（2.0.0 – 2.1.0 把它读成
`/// @custom selfcontained`，那表示什么都不内嵌，于是这种函数体里的每个调用都留给 shader 编译器，然后失败。）

| 方面 | 每个 `Function`，带不带 `Inline` / `SelfContained` 都一样 |
| :-- | :-- |
| 函数体所在位置 | 每个调用它的节点的代码里，以及每个函数体调用它的节点的包装成员里 |
| 函数体调用的其他 `Function` | 内嵌，被调方排在调用方之前 |
| `IncludeFilePaths` | 函数体及每个被内嵌函数的顶部 include |
| 节点函数体对被内嵌函数的调用 | `__ds_wrapper_<CRC>.DreamShaderFn_<Name>(…)` |
| 递归 | `DSH6260` |

`Inline` 是 `SelfContained` 的旧拼法，读法相同，但给出警告 `DSH6306`。函数体调用不是 `Function` 的东西 ——
`ShaderFunction` 或 `VirtualFunction` —— 是 `DSH6261`；调用提升了 `UE.*` 的 `GraphFunction` 是 `DSH6327`；只差大小写
的调用是警告 `DSH6259`，留给 shader 编译器。

<a id="graphfunction-与-ue-提升"></a>

## `GraphFunction` 与 `UE.*` 提升

（自 1.3.1 起）

读函数体时会扫描一遍，每个 `UE.*` 调用都记作一次提升；组装节点代码时，调用文本被替换成一个生成输入引脚的
名字，那个引脚承载这次调用求值得到的材质表达式。

只有下列条件按顺序全部满足时才会被提升：

| 步骤 | 条件 |
| :-- | :-- |
| 1 | `U` 前面的字符是标识符边界 |
| 2 | 接下来三个字符是 `U`、`E`、`.` —— 在 1.x 函数体里**忽略大小写**匹配，所以 `ue.`、`Ue.`、`uE.` 都会触发 |
| 3 | `.` 后面的字符能开始一个标识符 |
| 4 | 跳过可选空白后，下一个字符是 `(` |
| 5 | 存在配对的 `)` —— 搜索跳过字符串 |

第 1–4 步不满足时，文本原样静默透传。第 5 步失败是 `DSH6314`。扫描跳过注释、字符串和字符字面量，所以写在
注释里的 `UE.Time()` 不会被提升。

提取出的文本按 Graph 表达式解析，所以任何 [`UE.*` 内置](../builtins/ue.md)（包括
[`UE.Expression`](../builtins/ue-expression.md)）都可用。它在**函数自己的作用域**里绑定 （自 2.0.0 起）：
函数的参数（每次调用时换成那次调用的实参），以及文件作用域的声明。只有调用方才有的名字是 `DSH6325` —— 1.x 在
调用方的 `Graph` 作用域里求值提升的调用；请把值作为参数传进来。谁都没有的名字（包括函数体自己声明的变量）是
`DSH6326`。

<a id="生成的引脚名"></a>

### 生成的引脚名

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 基础名是 `__ds_<FunctionName>_UE<N>`，`N` 按源码顺序给函数体里的提升从 0 计数 —— 每个调用点都是同样的名字 |
| 2 | 整个名字经过标识符净化，连续下划线塌缩，开头的 `__` 变成单个 `_` |
| 3 | 和已声明的 `in` 参数或前面的提升冲突（忽略大小写）时，追加 `_1`、`_2`… |

所以函数体里含一个 `UE.Time()` 调用的 `GraphFunction WindPulse` 会产出一个字面名为：

```text
_ds_WindPulse_UE0
```

的引脚，而带命名空间的 `Common::Pulse` 产出 `_ds_Common_Pulse_UE0`。声明的 `in` 参数按声明顺序占据前面的
引脚，提升出的引脚排在后面。这些名字在生成的节点上、以及任何提到它们的 shader 编译错误里都是用户可见的。

**提示：**

提升恰好消费 `UE.Name( … )` 到配对的 `)` 为止，一个字符都不多。尾随的 swizzle 或成员访问会留在原地，作为作用于
引脚的 HLSL，所以 `UE.CameraVector().xy` 变成 `_ds_<Fn>_UE0 .xy`，由 shader 编译器求值；引脚承载节点默认输出的
完整分量。引脚名用空格补齐到原调用的长度，所以函数体后面的每一列仍然和源码对得上。

**警告：**

**只有 `UE.` 会被提升。** `Substrate.*` 调用不被提升，作为文本到达 shader 编译器，而那不是合法 HLSL；前端会给出
警告 `DSH6316` （自 2.0.0 起）。Substrate 值根本不能跨越 `Custom` 节点边界 —— 请在 `ShaderFunction` 或
`Shader` 的 `Graph` 里构建 Substrate 图。

被提升的调用必须产出 `Custom` 节点输入能承载的东西：数字、布尔，或者纹理对象 （自 2.0.0 起）。
`MaterialAttributes` 值或 Substrate 材质是 `DSH6329`。有多个输出的节点把默认输出送进引脚。

```c
GraphFunction WindPulse(in float2 uv, out float pulse)
{
    float t = UE.Time();
    pulse = sin(uv.x * 8.0 + t);
}
```

一次调用会生成：

```text
UMaterialExpressionTextureCoordinate   →  Custom pin "uv"
UMaterialExpressionTime                →  Custom pin "_ds_WindPulse_UE0"
UMaterialExpressionCustom  Description="WindPulse"  OutputType=CMOT_Float1
```

```hlsl
float pulse = (float)0;
// Begin DreamShader source: DShader/Lib/Motion.dsh
// DreamShader custom: WindPulse line 4

    float t = _ds_WindPulse_UE0;
    pulse = sin(uv.x * 8.0 + t);
// End DreamShader source: DShader/Lib/Motion.dsh
return pulse;
```

附加的 `out` 参数是节点的附加输出，以**声明的**参数命名 （自 2.0.0 起）（1.x 以调用方的变量命名），函数体
直接给它们赋值。函数体里对普通 `Function` 的调用和在 `Function` 节点里一样被内嵌。

<a id="namespace"></a>

## `Namespace`

`Namespace` 给它包含的 `Function` 和 `GraphFunction` 声明的名字加前缀。它不是一个实体：不会为它存储任何对象，
它也**不创建作用域**。成员的名字是压平后的标识符 （自 2.0.0 起）：

```text
Namespace(Name = "Common") { Function ApplyTint(…) }   →   Common::ApplyTint   →   函数 Common_ApplyTint
```

名字查找、绑定器诊断和 helper 符号 `DreamShaderFn_Common_ApplyTint` 看到的都是 `Common_ApplyTint`；限定拼法只留在
一个地方：成员 `Custom` 节点的标题（`Description`）是 `Common::ApplyTint`，和 1.x 一样。

| 规则 | 行为 |
| :-- | :-- |
| `Name` | 必须是合法标识符（`DSH6309`）。`Namespace(Name="A::B")` 是错误 —— **没有多段声明形式**。 |
| 函数体 | 只能是 `Function` 和 `GraphFunction`。嵌套命名空间、section、资产块和 `import` 都是 `DSH6310`。空的函数体可以接受。 |
| 重复打开 | 允许且不作检查。同一文件或跨导入 header 的两个 `Namespace(Name="Common")` 块都会加 `Common::` 前缀。 |
| 查找 | 限定名，或它压平后的拼法 `Common_ApplyTint`。没有 `using`，也没有不限定的回退：裸的 `ApplyTint(…)` 是 `DSH4208`。 |
| 大小写 | 名字大小写敏感 （自 2.0.0 起）。`common::applytint(…)` 只与一个函数匹配时仍能解析，并给出警告 `DSH5275`。 |
| 缺名字 | `Common::` 后面什么都没有是 `DSH5260`。 |

```c
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }

    Function float Remap01(in float value) {
        return saturate(value * 0.5 + 0.5);
    }

    GraphFunction Pulse(in float speed, out float value) {
        value = sin(UE.Time() * speed);
    }
}
```

```c
Graph = {
    vec3 Tinted;
    Common::ApplyTint(BaseColor, Tint, Tinted);   // 语句式调用
    float K = Common::Remap01(Tinted.r);          // 值式调用
}
```

**警告：**

因为 `::` 和 `_` 压平后是同一个东西，`Namespace(Name="Common")` 的成员 `ApplyTint` 和顶层的
`Function Common_ApplyTint` 声明的是同一个名字：`DSH4210`。

**在另一个 `Function` 或 `GraphFunction` 函数体里**写 `Ns::Fn(…)` 现在能调用到 （自 2.0.0 起）：函数体规范化把
`Common::ApplyTint` 改写成 `Common_ApplyTint`，那正是函数自己的名字，于是这个调用像其他 `Function` 调用一样被内嵌。
1.x 里它会作为未定义的 `Common_ApplyTint` 到达 shader 编译器然后失败。`::` 两边不能有空格。

<a id="调用"></a>

## 调用

两种调用形式都写在 `Graph` 块里。

| 形式 | 规则 |
| :-- | :-- |
| `x = Fn(a, b);` —— **值式调用** （自 1.3.1 起） | 传入输入；值是函数的第一个输出 —— 返回值，或第一个 `out`。多输出的函数也可以这样调用 （自 2.0.0 起），其他输出不连线；`Fn(a, b).Out` 或 `Fn(a, b, Output = "Out")` 选别的输出。 |
| `Fn(a, b, OutX, OutY);` —— 语句式调用 | 先全部输入，再按声明顺序每个输出一个接收者。接收者必须是变量（`DSH4239`），类型和输出一致（`DSH4218`）；没人声明过的接收者就地声明，给出 info `DSH5283`。 |

实参太多是 `DSH4224`，漏了输入是 `DSH4217`。命名参数现在可以用 （自 2.0.0 起）：它命名一个参数（没有这个名字是
`DSH4216`），用了命名参数的调用按名字和声明顺序匹配，而不是"先输入后接收者"。修饰过的拼法
`DreamShaderFn_Luma(c)` 不再指向这个函数 （自 2.0.0 起）：`DSH4208`。

完整的参数规则、`default`、输出选择器见 [调用](../graph/calls.md)。

<a id="说明"></a>

## 说明

- **一个名字只声明一样东西。** 同名的两个函数 —— 同一文件里，或文件和它导入的 header 之间 —— 是 `DSH4210`；
  `A::B` 和 `A_B` 是同一个名字，也算。`Function` 和 `GraphFunction` 同名同样是 `DSH4210` （自 2.0.0 起）（1.x
  把调用报成有歧义）。名字和内置函数（`lerp`、`saturate`…）相同是 `DSH6206`。没有重载。
- **名字大小写敏感** （自 2.0.0 起）。`Luma` 和 `luma` 是两个函数；`Graph` 调用只差大小写且唯一匹配时仍能解析，
  给出警告 `DSH5275`。
- **header 单独解析。** 导入的 `.dsh` 不会被粘贴进文件，它的函数被登记进来。见
  [import 与命名空间](import.md)。
- 只含 `Function` / `GraphFunction` 块的 `.dsm` 或 `.dsf` 能编译成功且不产出任何东西。
- UE 5.3 上生成的 `Custom` 节点会在材质图里显示代码；UE 5.4 起 `ShowCode` 被设为 `false`。

<a id="示例"></a>

## 示例

```c
// DShader/Lib/Color.dsh

Function float Luma(in vec3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

Function Remap01(in float value, out float result)
{
    result = saturate(value * 0.5 + 0.5);
}
```

```c
import "Lib/Color.dsh";

Shader(Name="Materials/M_Tinted")
{
    Properties = {
        TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_Base");
        vec3                     Tint    = vec3(1.0, 0.6, 0.2);
    }
    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }
    Graph = {
        vec2 UV  = UE.TexCoord(Index = 0);
        vec4 Tex = BaseTex(Coordinates = UV);

        float Key;
        Remap01(Luma(Tex.rgb), Key);      // 语句式调用，内层是值式调用

        Color = Tex.rgb * Key;
    }
}
```

每个调用都是一个 `Custom` 节点。`Luma` 节点的代码 —— 注意函数体自己的 `return`：

```hlsl
// Begin DreamShader source: DShader/Lib/Color.dsh
// DreamShader custom: Luma line 4

    return dot(color, float3(0.299, 0.587, 0.114));
// End DreamShader source: DShader/Lib/Color.dsh
```

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH6300: Expected a function name after '{0}', found '('.` | Function / GraphFunction（及可选的修饰符和返回类型）后面在名字的位置出现了 ( —— Function SelfContained( 和 section 式的 Function Name { 也会走到这里。 | 写成 Function float3 MyFunction(in float3 value) { ... }。 |
| `DSH6319: Expected '(' to open the parameter list of '{0}', found {1}.` | 名字后面没有 (，或者参数列表后面没有打开函数体的 {。 | 写出参数列表（可以为空）和函数体：Function float Pi() { return 3.14159; }。 |
| `DSH6301: Expected '[in\|out] Type Name' in the parameter list of '{0}', found '{1}'.` | 参数不是 \[in\|out\] Type Name。1.x 参数没有默认值也没有元数据。 | 每个参数写成 in float3 color 或 out float result。 |
| `DSH6302: Expected 'in' or 'out' before the parameter '{0}' of '{1}', found '{2}'; a 1.x function has no 'inout'.` | 参数带了 inout，或在 in / out 的位置写了别的词。 | 拆成一个 in 参数和一个 out 参数。 |
| `DSH6303: Expected a parameter name other than '__return', which 1.x reserved, in '{0}'.` | 参数名是 \_\_return。 | 给参数改名。 |
| `DSH6304: Expected either a return type or 'out' parameters on '{0}', found both.` | 同时写了返回类型和 out 参数。 | 返回第一个结果、不留 out 参数；或者去掉返回类型、每个结果都用 out。 |
| `DSH6305: Expected '{0}' to return a value or to have at least one 'out' parameter, found neither.` | 既没有返回类型也没有 out 参数，节点没有值给图读取。 | 加一个返回类型或一个 out 参数。 |
| `DSH6306: 'Inline' is the old spelling of 'SelfContained', and both read the same: the function's node embeds the functions it calls.` | 使用了 Function Inline（警告）。两种拼法读法相同，和不写修饰符一样。 | 去掉修饰符，或改写成 SelfContained；两者都不改变任何东西。 |
| `DSH6307: Expected a return type or a name after 'GraphFunction', found the modifier '{0}', which only a Function takes.` | GraphFunction 后面跟了 SelfContained 或 Inline。 | 删掉修饰符。 |
| `DSH6308: Expected a value after 'return' in '{0}', which returns '{1}', found a bare 'return;'.` | 有返回类型的函数里出现了顶层的裸 return;。 | 每条路径都返回一个值。 |
| `DSH6256: '{0}' returns void but its body uses 'return;'; a custom node always returns its first output, so give the function a return type or restructure the body.` | 没有返回类型的函数在任何深度用了裸 return;。 | 给函数一个返回类型并返回值，或者把提前退出改成 if。 |
| `DSH6257: '{0}' declares a return type but its body never returns a value; the node's first output will be 0.` | 声明了返回类型，函数体里一个 return 都没有（警告）。 | 返回签名承诺的值。 |
| `DSH6258: '{0}' has an '#include' with an empty path; write the virtual shader path the header lives at, for example "/Engine/Private/Common.ush".` | 函数体顶部的 #include 路径为空。 | 写头文件所在的虚拟 shader 路径。 |
| `DSH6260: The '@custom' functions {0} call each other in a cycle; HLSL has no recursion, so their bodies cannot be embedded in a custom node.` | 函数体之间互相调用成环。 | 把共享部分抽成第三个不调用别人的函数。 |
| `DSH6261: '{0}' is not a '@custom' function and cannot be called from the HLSL body of '{1}'; a custom node sees no graph values, so mark '{0}' '@custom' as well or move the call out of the body.` | 函数体调用了不是 Function 的东西 —— ShaderFunction 或 VirtualFunction。 | 在 Graph 里调用它，把结果作为参数传进来。 |
| `DSH6262: '{0}' takes {1} argument(s) but this call passes {2}; a call that carries a texture cannot be matched up by position otherwise.` | 调用带纹理参数的函数时实参数目不对。 | 每个参数传一个实参。 |
| `DSH6263: The texture argument for '{0}' of '{1}' has to be a plain texture name, because the sampler that goes with it is named after it.` | 纹理位置上的实参不是纯名字。 | 先把纹理赋给一个名字再传。 |
| `DSH6255: '{0}' declares '{1}' twice in the HLSL it generates; a texture parameter also claims '{1}Sampler', which the engine declares alongside it.` | 两个参数在生成的 HLSL 里同名 —— 通常是参数叫 &lt;Texture&gt;Sampler。 | 给其中一个改名。 |
| `DSH6327: '{0}' takes the 'UE.' calls lifted out of its body as inputs of its own custom node, so the HLSL body of '{1}' cannot call it; call it from a graph body instead.` | 一个函数体调用了提升过 UE.\* 的 GraphFunction。 | 在 Graph 里调用它并把结果传进来，或者把 UE. 调用挪成参数。 |
| `DSH6259: '{0}' differs from the function '{1}' only in case; HLSL is case-sensitive, so this call is left for the shader compiler. Did you mean '{1}'?` | 函数体里的调用和某个函数只差大小写（警告）。 | 按声明的拼法写。 |
| `DSH6314: Expected a ')' to close the 'UE.' call in the body of '{0}', found the end of the body.` | 函数体里的 UE.\*( 没有配对的 )。 | 闭合这个调用。 |
| `DSH6316: A 'Substrate.' call in the body of '{0}' is not lifted into a node, because no custom node input carries a Substrate value; it reaches the shader compiler as text.` | 函数体里调用了 Substrate.\*（警告）。 | 在 Graph 里构建 Substrate 节点，HLSL 里只留数字。 [详解](../builtins/substrate.md) |
| `DSH6325: '{0}' lifts a 'UE.' call out of its body that reads '{1}', which 1.x took from the caller's scope and 2.0 does not; pass '{1}' to '{0}' as a parameter.` | 被提升的调用读了调用方的变量。 | 为这个值加一个参数，在调用处传进来。 |
| `DSH6326: '{0}' lifts a 'UE.' call out of its body that reads '{1}', which is neither a parameter of '{0}' nor declared at file scope.` | 被提升的调用读的名字既不是参数也不在文件作用域 —— 包括函数体自己声明的变量。 | 把值作为参数传进来。 |
| `DSH6329: '{0}' carries 'UE.' calls lifted out of its body, and only a '@custom' function with a verbatim body lifts calls into its node's inputs.` | 被提升的调用产出 MaterialAttributes 或 Substrate 值，Custom 输入承载不了。 | 在 Graph 里构建这个节点。 |
| `DSH6330: '{0}' reaches itself through the 'UE.' calls lifted out of its body, and each call makes a new custom node, so the graph would never end.` | 被提升调用的实参又调用了这个函数。 | 在 Graph 里算出内层值再传进来。 |
| `DSH6210: '{0}' is '@custom', so '{1}' becomes an input pin of a Custom node, and a Custom node cannot take a material; pass the fields it needs instead.` | MaterialAttributes 作为输入。 | 传函数体需要的字段；MaterialAttributes 可以作为结果。 [详解](types.md) |
| `DSH6253: '{0}' uses Substrate on '{1}'; a custom node has no Substrate pins, so build that part of the material out of reflected Substrate nodes.` | 签名里出现 Substrate。 | 在 ShaderFunction 或 Shader 的 Graph 里构建 Substrate 部分。 |
| `DSH6254: '{0}' returns a texture through '{1}'; a custom node output carries float1..4 or material attributes, never a texture object.` | 返回纹理或采样器，或通过 out 交出一个。 | 在函数体里采样，返回采样结果。 |
| `DSH6206: '{0}' is a builtin operation and cannot be redeclared; rename the function.` | 函数名和内置操作相同（dot、lerp、saturate…）。 | 给函数改名。 |
| `DSH4210: '{0}' is already declared in this file; one name declares one thing.` | 同名的两个声明 —— 包括 Ns::Fn 撞上顶层的 Ns\_Fn，以及 Function 和 GraphFunction 同名。 | 给其中一个改名。 |
| `DSH4208: A value of type {0} has no method '{1}'.` | 调用的名字什么都没声明 —— 常见原因是不限定地调用了命名空间成员，或写了 DreamShaderFn\_ 前缀的拼法。 | 改写成 Ns::Fn(…)，名字区分大小写。 |
| `DSH5260: Expected a name after '::', found {0}.` | 限定调用的 :: 后面什么都没有。 | 写成 N::F(...)。 |
| `DSH4216: '{0}' takes its arguments in order and has no argument called '{1}'.` | 命名参数指向一个不存在的参数。 | 改正名字，或按位置传。 [详解](../graph/calls.md) |
| `DSH4217: '{0}' is missing its '{1}' argument.` | 调用漏了一个输入。 | 把它传进去。 |
| `DSH4224: '{0}' takes at most {1} arguments.` | 实参太多。 | 对照声明。 |
| `DSH4239: '{0}' is an out parameter of '{1}', so its argument has to be a variable.` | 接收者不是变量。 | 声明一个参数类型的局部变量再传。 |
| `DSH4218: '{0}' writes back {1}, and this variable is {2}; an out argument has to match exactly.` | 接收者变量的类型和输出不一致。 | 用参数的确切类型声明变量，调用后再转换。 |
| `DSH5283: '{0}' is not declared, and as in 1.x it is declared here as a local of type {1} receiving '{2}' of '{3}'.` | 语句式调用的接收者没有声明过，和 1.x 一样就地声明（info）。 | 不必改；dsc migrate 会把声明写出来。 |
| `DSH6310: Expected only Function and GraphFunction blocks inside the namespace '{0}', found {1}.` | Namespace 体内出现了其他任何东西，包括嵌套的 Namespace 和 import。 | 把那个块挪出命名空间。 |

<a id="该选哪一种"></a>

## 该选哪一种

| 场景 | 用 |
| :-- | :-- |
| 复用的纯数学 | `Function`，放在 `.dsh` |
| 复用的、需要真实材质节点的逻辑 | `GraphFunction` |
| 供其他材质调用的可复用资产 | `ShaderFunction`，放在 `.dsf` |
| 已经存在的 `UMaterialFunction` | `VirtualFunction` |
| `MaterialAttributes` 输入或 Substrate 传递 | `ShaderFunction` —— 绝不用 `Function` |
| 公共库 API | 放进 `Namespace` 的稳定名字，作为 package 发布 |

<a id="继续阅读"></a>

## 继续阅读

- [调用](../graph/calls.md) — 全部参数形式、default、输出选择器。
  - [import 与命名空间](import.md) — 把 helper header 送进调用它的文件。
  - [UE.* 节点](../builtins/ue.md) — GraphFunction 函数体能提升的一切。
  - [类型与值](types.md) — 签名里的类型 token 及其 HLSL 拼写。
