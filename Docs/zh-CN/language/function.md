---
title: "Function"
description: "Function"
---

<a id="function"></a>

# Function

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Function**

一个顶层块，用于声明可复用的 HLSL 辅助函数：其主体为逐字保留的 HLSL 文本，
每一处调用位置都会成为一个 `UMaterialExpressionCustom` 节点，其代码即为该主体。

| | |
| :-- | :-- |
| 声明位置 | `.dsm`、`.dsf`、`.dsh` —— 以及 [`Namespace`](namespace.md) 主体内部 |
| 类型 | 顶层块 |
| 生成 | 不生成资产，也不生成文件。每个调用位置是一个 Custom 节点；其代码包含主体以及该主体调用的每个其他 `Function` *（自 2.0.0 起：不再写入 `.ush` 包含）* |
| 多重性 | 每个文件可有任意数量；一个名称声明一个事物 |

旧版前端将 `Function` 读取为 [2.0 语言](../language-v2/index.md) 的 `/// @custom` 函数，
无论是否带 `SelfContained` 修饰符都一样；[`dsc migrate`](../tools/migrate.md) 就按这种方式写入。

<a id="synopsis"></a>

## Synopsis

```c
Function [ ( <generate-attributes> ) ] [ { Inline | SelfContained } ] [ <return-type> ] <name> ( [ <parameter-list> ] )
{
    <hlsl>
}
```

```c
<parameter-list>      ::= <parameter> [ , <parameter> ] …
<parameter>           ::= [ { in | out } ] <type-token> <parameter-name>
<generate-attributes> ::= <key> = "<value>" [ , <key> = "<value>" ] …      // Generate, Library, Description
```

关键字 `Function` 以**区分大小写**的方式匹配。`function`、`FUNCTION` 和 `Functions` 并不是
该关键字：文件会报告 [`DSH2240`](../diagnostics/DSH2xxx.md#dsh2240)，或者当后续内容读作 2.0 声明时，
报告 [`DSH2248`](../diagnostics/DSH2xxx.md#dsh2248)。参见
[Lexical elements](lexical.md#case-sensitivity)。

声明中的其他所有内容 —— `Inline` / `SelfContained` 修饰符、`in` / `out`
限定符，以及每个类型标记 —— 都以**不区分大小写**的方式匹配。

没有 `Settings`，也没有任何类型的节。参数列表之后的 `{ … }` 是原始 HLSL，
逐字捕获：只读取其中的花括号，字符串或注释内的花括号不计入。

可选的 `( … )` 头部 *（2.1.2 之后）* 要求包根据该函数生成一个材质函数 —— `Function(Generate = "MF_BL_Checker", Library = "Blender|Texture") Checker(...)`。它
仅在包的头部中有意义：参见 [Packages ▸ Asset headers](../../tools/packages.md#asset-headers)。
`Generate = ""` 使一个函数退出其 `Namespace` 的模板。另一个键是警告
[`DSH6331`](../diagnostics/DSH6xxx.md#dsh6331)，会被忽略；在包之外，该头部是
[`DSH7234`](../diagnostics/DSH7xxx.md#dsh7234)，不生成任何内容。1.x 在此块上没有头部，因此
之前能读取的文本没有读取方式发生变化。

<a id="declaration-order"></a>

## Declaration order

解析器从左到右读取标识符，并通过前瞻进行消歧：

| 形式 | 读取为 |
| :-- | :-- |
| `Function Name(…)` | 名称 |
| `Function Type Name(…)` | 返回类型，然后是名称 |
| `Function SelfContained Name(…)` | 修饰符，然后是名称 |
| `Function SelfContained Type Name(…)` | 修饰符、返回类型，然后是名称 |
| `Function Type SelfContained Name(…)` | 返回类型 `Type`、名称 `SelfContained`，然后本应是 `(` 的位置出现一个多余的 `Name` —— [`DSH6319`](../diagnostics/DSH6xxx.md#dsh6319) |

规则是机械式的：在一个标识符之后，`(` 使它成为名称；另一个标识符使它成为
*返回类型*，而下一个标识符成为名称。因此修饰符固定在第一位。

<a id="modifiers"></a>

## Modifiers

| 修饰符 | 可接受于 | 效果 |
| :-- | :-- | :-- |
| `SelfContained` | 仅 `Function` | 无：每个 `Function` 节点都嵌入其主体调用的函数，这正是 1.x 中该修饰符所要求的 —— 参见[下文](#inline--selfcontained-mode) |
| `Inline` | 仅 `Function` | `SelfContained` 的旧拼写：效果相同，另加一个警告（[`DSH6306`](../diagnostics/DSH6xxx.md#dsh6306)） |

两种拼写均不区分大小写。在 [`GraphFunction`](graph-function.md) 上，任意一种都是
[`DSH6307`](../diagnostics/DSH6xxx.md#dsh6307) *（自 2.0.0 起；1.x 将该词读取为返回类型）*。

> [!WARNING]
> 函数**名称**不能是 `Inline` 或 `SelfContained`。`Function SelfContained(in float a,
> out float b) { … }` takes the modifier branch, then finds `(` 在本应是名称的位置：
> [`DSH6300`](../diagnostics/DSH6xxx.md#dsh6300)。

<a id="parameters"></a>

## Parameters

参数是直到下一个 `,` 或 `)` 之前的单词。单词是标识符或关键字，因此任何
空白 —— 包括制表符和换行符 —— 都只是分隔它们。接受两个或三个单词：

| 单词数 | 形式 | 限定符 |
| :-- | :-- | :-- |
| 2 | `<type> <name>` | 默认为 `in` |
| 3 | `<qualifier> <type> <name>` | 按书写 |
| 少于 2、多于 3，或任何不是单词的内容（`[2]`、`= 1.0`） | — | `DSH6301` |

| 规则 | 行为 |
| :-- | :-- |
| 限定符 | 仅 `in` 和 `out`，以小写比较，因此 `In`、`OUT`、`iN` 都可用。 |
| `inout` | 在 1.x 函数中**不存在**：[`DSH6302`](../diagnostics/DSH6xxx.md#dsh6302)。 |
| 空参数 | 单个 `,` 会被跳过，因此 `)` 前的尾随逗号可被容忍。 |
| 顺序 | 参数保持其声明顺序。`in` 参数成为节点的输入引脚；`out` 参数成为其输出。两者可以交错。 |
| 保留名称 | 名为 `__return`（忽略大小写）的参数是 [`DSH6303`](../diagnostics/DSH6xxx.md#dsh6303)。 |
| 重复名称 | [`DSH4214`](../diagnostics/DSH4xxx.md#dsh4214)。 |
| 不可用 | `opt`、默认值和 `[ … ]` 元数据块**不**属于此语法（`DSH6301`）。它们属于 `ShaderFunction` 和 `VirtualFunction` 的 [`Inputs` / `Outputs` 节](inputs-outputs.md)。 |

<a id="at-least-one-output"></a>

### At least one output

`Function` 必须产生某些东西：**要么**至少一个 `out` 参数**要么**一个返回类型。
两者皆无则是 [`DSH6305`](../diagnostics/DSH6xxx.md#dsh6305)。

<a id="return-type"></a>

## Return type

| 形式 | 合法 | 结果 |
| :-- | :-- | :-- |
| `Function Name(in T a, out U r)` | 是 | 一个输出，`r` |
| `Function Name(in T a, out U r1, out V r2)` | 是 | `r1` 是节点的第一个输出，`r2` 是附加输出 |
| `Function U Name(in T a) { return expr; }` | 是 | 返回的值是节点的输出 |
| `Function U Name(in T a, out V r)` | **否** | [`DSH6304`](../diagnostics/DSH6xxx.md#dsh6304) |
| `Function Name(in T a)` | **否** | `DSH6305` |
| `Function U Name(…) { return; }` | **否** | 返回类型之下的顶层裸 `return;`：[`DSH6308`](../diagnostics/DSH6xxx.md#dsh6308) |
| `Function Name(…, out U r) { … return; … }` | **否** | 在无返回类型的函数中任意深度的 `return;`：[`DSH6256`](../diagnostics/DSH6xxx.md#dsh6256) *（自 2.0.0 起）* |

因此：**返回类型意味着恰好一个输出，并禁止每个显式 `out`。** 多个输出
需要 `out` 参数且没有返回类型。

<a id="return-lowering"></a>

### `return` lowering

*（自 2.0.0 起）* `return` 不再被重写。1.x 将每个顶层 `return` 替换为
对合成的 `__return` 结果的赋值；2.0 保留主体中的 `return` 语句原样，
因为主体现在*就是*返回节点第一个输出的 HLSL 函数的主体 —— 无论是
Custom 节点自身的，还是当另一个主体嵌入它时的辅助函数的。因此 `return expr;` 在
任意花括号深度返回该值；1.x 中绕过 `__return` 的嵌套情形现在行为与顶层
情形相同。

仍然检查的内容：返回类型（`DSH6308`）下的顶层裸 `return;`、函数中任意位置出现且没有该返回类型的 `return;`（`DSH6256`），以及函数体完全不包含 `return` 的返回类型——这是警告，表示输出为 0
([`DSH6257`](../diagnostics/DSH6xxx.md#dsh6257))。只包含一个裸表达式的函数体会被赋予其
`return`。名称 `__return` 保持保留（`DSH6303`）。

<a id="type-tokens"></a>

## 类型标记

15 个 GLSL 别名（`vec2`…`vec4`、`ivec*`、`uvec*`、`bvec*`、`mat2`…`mat4`）在解析时会被规范化为其
HLSL 拼写，作用范围包括返回类型和每个参数类型；以其他大小写拼写的标量或向量
（`Float3`）会被读取为其小写拼写。不命名任何类型的标记在声明处会被视为
[`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201) *（自 2.0.0 起；1.x 仅在调用该函数时才失败）*。完整别名列表见[类型标记](types.md)。

| 标记 | 作为 `in` 有效 | 作为 `out` / 返回值有效 |
| :-- | :-- | :-- |
| `float`、`half`、`int`、`uint`、`bool` 及其向量 `…2`–`…4`，以及 GLSL 别名 | 是 | 是——节点输出为 `Float1`…`Float4` |
| `MaterialAttributes` | **否**——[`DSH6210`](../diagnostics/DSH6xxx.md#dsh6210)，以及节点处的 [`DSH6252`](../diagnostics/DSH6xxx.md#dsh6252) | 是 |
| `StaticBool`、`StaticBoolParameter` | 是——读取为 `bool` | 是——读取为 `bool` |
| `Texture2D`、`TextureCube`、`Texture2DArray`、`Texture3D`、`VolumeTexture` | 是 | **否**——[`DSH6254`](../diagnostics/DSH6xxx.md#dsh6254) |
| `SamplerState` | 采样器 *（自 2.0.0 起；1.x 将其读取为 Texture2D）* | **否**——`DSH6254` |
| `Substrate` | **否**——[`DSH6253`](../diagnostics/DSH6xxx.md#dsh6253) | **否**——`DSH6253` |
| 矩阵（`float3x3`、`mat3`……） | 不能跨节点引脚：图调用处 [`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361) | 同上 |

生成的 HLSL 以 2.0 方式拼写每种类型：`vec3` 为 `float3`，`MaterialAttributes` 为
`FMaterialAttributes`，`VolumeTexture` 为 `Texture3D`。

> [!NOTE]
> `Substrate` 永远不能在 `Function` 或 [`GraphFunction`](graph-function.md) 上使用：Custom 节点
> 在任何引擎版本中都没有 Substrate 引脚。请在
> [`ShaderFunction`](shader-function.md) 或 `Shader` 的 `Graph` 中构建 Substrate 部分。参见
> [Substrate 内置项](../builtins/substrate.md)。

<a id="texture-parameters-and-samplers"></a>

### 纹理参数与采样器

有五个标记是*纹理参数*：`Texture2D`、`TextureCube`、`Texture2DArray`、`Texture3D`、
`VolumeTexture`（不区分大小写）。

| 位置 | 发生什么 |
| :-- | :-- |
| 在节点上 | 纹理 `in` 参数是一个引脚，携带纹理对象；引擎会在其旁边声明 `SamplerState <Name>Sampler`。请在函数体中使用该名称。 |
| 在嵌入的辅助函数中 | 其 HLSL 签名会在每个纹理参数之后立即增加 `SamplerState <Name>Sampler`，并且从另一个函数体对它的每次调用都会在纹理实参之后传递 `<argument>Sampler`。 |
| 该调用 | 必须传递每个实参（[`DSH6262`](../diagnostics/DSH6xxx.md#dsh6262)），并且其纹理实参必须是纯名称（[`DSH6263`](../diagnostics/DSH6xxx.md#dsh6263)）。 |
| 名为 `<Texture>Sampler` 的参数 | 与配对名称冲突：[`DSH6255`](../diagnostics/DSH6xxx.md#dsh6255)。 |

`SamplerState` **不是**纹理参数，不会获得伴随项。

*（自 2.0.0 起）* 纹理参数上的 `<Texture>.Sample(UV)`——以及 `SampleLevel`、`SampleBias`、
`SampleGrad`（比 HLSL 自身形式少一个实参）——会被重写为引擎的
`Texture2DSample(<Texture>, <Texture>Sampler, UV)` 系列。原生 HLSL 拼写保持原样。

<a id="body-normalisation"></a>

## 函数体规范化

每个 `Function` 和 `GraphFunction` 函数体——以及语言中其他任何内容——都会在存储前经过
标识符级重写。该扫描会跳过注释和 `"…"` 字符串。会发生两件事：

1. **别名替换。** 完整标识符会以*不区分大小写*的方式与 18 个条目匹配：
   15 个 GLSL 类型别名，加上 `mix` → `lerp`、`fract` → `frac`、`mod` → `fmod`。
2. **限定名称扁平化。** 形如 `A::B` 的标记（可重复，`::` 周围无空格）会被
   替换为整个标记经过标识符净化后的拼写——`Common::Remap01` 变为
   `Common_Remap01`。

> [!WARNING]
> 由于别名匹配不区分大小写，并适用于函数体中任意位置的完整标识符，
> 你自己名为 `Mix`、`Mod`、`Fract`、`Vec3`、`Mat4` 的辅助函数——或*变量*使用这些
> 名称之一——会在生成的 HLSL 中被静默重命名。不会有任何诊断：针对 GLSL 拼写的 `DSH5277` 警告
> 适用于 `Graph` 文本，而不是函数体。请重命名该标识符，或采用不会
> 冲突的拼写（`MixColor`、`ModValue`）。

*（自 2.0.0 起）* 扁平化后的 `Common_Remap01` 正是命名空间函数的名称，因此函数体内的
`Ns::Fn(…)` 调用现在可以到达该函数。参见
[命名空间](namespace.md#calling-a-namespaced-function)。

<a id="includes"></a>

## 包含

写在函数体**顶部**的 `#include` 指令——它们之前只能有空白和注释——不属于该函数。它们会从函数体中被清空（用空格覆盖，
因此后续每一行和每一列仍与源匹配）并被*提升*到
每个代码包含该函数体的 Custom 节点的包含列表（`IncludeFilePaths`）上：该函数自身的节点，以及任何其他嵌入它的 `Function` 的
节点，其中被嵌入函数的包含项优先。
*（自 2.0.0 起：不再有可提升到的生成包含项。）*

```c
Function float3 SampleWind(in float3 WorldPos)
{
    #include "/Plugin/DreamWind/Shared/DreamWindShared.h"   // hoisted
    FDreamWindSample S = DW_SampleAnalytic(DW_UnpackParams(P0, P1, P2), WorldPos);
    return S.Velocity;
}
```

`#include "…"` 和 `#include <…>` 均被接受；路径按书写原样存储，移除分隔符，并且必须是引擎可以解析的虚拟着色器路径（`/Engine/…`、`/Plugin/<Name>/…`）。
引擎会将节点的包含项放在文件作用域，因此定义函数的头文件可用。单个节点上的重复项会合并为一个条目。空路径为
[`DSH6258`](../diagnostics/DSH6xxx.md#dsh6258)；未终止的路径会留在函数体中，交由
着色器编译器处理。指令之前的注释会留在函数体中。

扫描会在遇到第一个不是前导 `#include` 的标记时停止：位于语句之后的指令**不会**被提升——它会落在生成的函数内部，这仅对由宏组成的头文件有效。参见 [DreamShaderBuiltins.ush](../builtins/hlsl-library.md#notes)。

<a id="generated-hlsl"></a>

## 生成的 HLSL

*（自 2.0.0 起）* 调用点的 Custom 节点会将其所有 HLSL 放在自己的 `Code` 中。形式如下：

```hlsl
struct generated_wrapper_<Name>_<CRC32 %08X>        // only when the body calls other Functions
{
	<Type0> DreamShaderFn_<Callee>(<params>)          // one member per embedded Function,
	{                                                 // a callee before its caller
		<Out1> = (<Type1>)0;                          // every `out` but the first
		<Type0> <Out0> = (<Type0>)0;                  // a function without a return type
		// Begin DreamShader source: <file>
		// DreamShader custom: <Callee> line <N>
		<callee body, verbatim>
		// End DreamShader source: <file>
		return <Out0>;
	}
};
generated_wrapper_<Name>_<CRC32 %08X> __ds_wrapper_<CRC32 %08X>;

<Type0> <Out0> = (<Type0>)0;                          // a function without a return type
// Begin DreamShader source: <file>
// DreamShader custom: <Name> line <N>
<body, verbatim; calls to embedded functions go through __ds_wrapper_<CRC32 %08X>>
// End DreamShader source: <file>
return <Out0>;                                       // only when the body has no top-level return
```

| 元素 | 规则 |
| :-- | :-- |
| 符号名称 | `DreamShaderFn_` + 函数名，其中所有非 `[A-Za-z0-9_]` 字符替换为 `_`，并且连续的多个下划线合并为一个。带命名空间的 `Common::ApplyTint` 就是函数 `Common_ApplyTint`，因此 `DreamShaderFn_Common_ApplyTint`。 |
| 为什么需要前缀 | 当同级的成员调用它时，未加前缀的 `Luminance(float3)` 会遮蔽来自 `/Engine/Private/Common.ush` 的引擎内建函数。 |
| 辅助函数的返回值 | 它的返回类型；对于没有返回类型的函数，是它的第一个 `out` 参数，这就是为什么一个函数体可以把具有多个输出的 `Function` 当作值来调用。 |
| 辅助函数参数 | 声明顺序；在每个纹理参数之后添加一个 `SamplerState <Name>Sampler`；除第一个外的每个 `out` 都作为 `out <Type> <Name>`，零初始化。 |
| 节点自身的函数体 | 不包裹在 DreamShader 的函数中：它就是 Custom 节点自身的代码。它的第一个输出是返回的值，或者对于第一个 `out` 是一个局部变量；其他每个 `out` 都是额外输出，由引擎声明并零初始化。 |
| 标记 | 每个函数体前后的 `Begin` / `custom` / `End` 注释行，让着色器编译错误可以映射回其源代码行；任何重写都不会在函数体内部添加或删除行。 |
| 返回 | 只有当函数体没有顶层 `return` 时，才会追加 `return 0.0;`（或第一个 `out`）；如果函数体只是一个裸表达式，则将其包裹在 `return …;` 中。 |

节点标题（它的 `Description`）就是函数名 — 对于
[`Namespace`](namespace.md) 的成员为 `Ns::Fn`。[生成的 HLSL](../generation/generated-hlsl.md) 页面描述了
1.x 所编写的 `.ush` include。

<a id="inline--selfcontained-mode"></a>

## `Inline` / `SelfContained` 模式

*(自 2.0.0 起)* 每个调用点的节点都以 `generated_wrapper_*` 结构体的成员形式，包含函数自身的函数体以及该
函数体传递性调用的其他每个 `Function` — HLSL 没有嵌套
函数，而 Custom 节点的代码就是一个函数体。1.x 仅对 `SelfContained`
函数这样做，否则引用其共享 include；2.0 没有共享 include，所以每个函数都会得到
`SelfContained` 所要求的内容，并且该修饰符会被读取但不会改变任何东西。*(2.0.0 – 2.1.0
将其读取为 `/// @custom selfcontained`，它不嵌入任何内容，并将其做出的每个调用留给
着色器编译器。)*

| 方面 | 每个 `Function`，无论是否带有 `Inline` / `SelfContained` |
| :-- | :-- |
| 函数体所在的位詈 | 在每个调用它的节点的代码中，并作为包装器成员出现在每个其函数体调用它的节点中 |
| 它的函数体调用的其他 `Function` | 嵌入，被调用者在其调用者之前 |
| `IncludeFilePaths` | 函数体以及每个嵌入函数的前导 include |
| 从节点函数体到嵌入函数的调用 | `__ds_wrapper_<CRC>.DreamShaderFn_<Name>(…)` |
| 包装器内部的同级调用 | 普通的 `DreamShaderFn_*`，没有 `this.` 限定符 |
| 递归 | [`DSH6260`](../diagnostics/DSH6xxx.md#dsh6260) |

两个 CRC32 值都对调用函数的名称进行哈希。一个 `.dss` 如果想要函数体完全按
原样采用，不嵌入任何内容，可以写 `/// @custom selfcontained`；1.x 的 `Function` 没有
这样的写法。从函数体到不是 `Function` 的内容的调用 — 例如 `ShaderFunction` 或 `VirtualFunction` — 会
[`DSH6261`](../diagnostics/DSH6xxx.md#dsh6261)；对于带有提升
`UE.*` 调用的 [`GraphFunction`](graph-function.md)，则
[`DSH6327`](../diagnostics/DSH6xxx.md#dsh6327)。一个仅在大小写上匹配函数的调用会给出警告
([`DSH6259`](../diagnostics/DSH6xxx.md#dsh6259)) 并留给着色器编译器处理。

<a id="calling-a-function"></a>

## 调用函数

两种调用形式都位于 [`Graph`](../graph/index.md) 块中，并在
[调用函数](../graph/calls.md) 中有详细说明。总结如下：

| 形式 | 规则 |
| :-- | :-- |
| `x = Fn(a, b);` — 值调用 *(自 1.3.1 起)* | 传递输入；其值是函数的第一个输出 — 返回值，或第一个 `out`（旧规则 L3b）。*(自 2.0.0 起)* 也适用于有多个输出的函数，其其他输出保持未连接；`Fn(a, b).Out` 或 `Fn(a, b, Output = "Out")` 用于选择另一个输出。 |
| `Fn(a, b, OutX, OutY);` — 语句调用 | 先输入，然后按声明顺序为每个输出提供一个接收者（旧规则 L5）。接收者必须是输出类型的变量（[`DSH4239`](../diagnostics/DSH4xxx.md#dsh4239)）（[`DSH4218`](../diagnostics/DSH4xxx.md#dsh4218)）；未声明的变量会在那里声明（[`DSH5283`](../diagnostics/DSH5xxx.md#dsh5283)）。 |

参数过多会触发 [`DSH4224`](../diagnostics/DSH4xxx.md#dsh4224)；漏掉某个输入会触发
[`DSH4217`](../diagnostics/DSH4xxx.md#dsh4217)。*(自 2.0.0 起)* 命名参数用于指定参数名
（对于不存在的名称则为 [`DSH4216`](../diagnostics/DSH4xxx.md#dsh4216)），使用命名参数的调用
按名称和声明顺序匹配，而不是按先输入后接收者的顺序。改写后的
拼写 `DreamShaderFn_Luma(c)` 不再用于命名函数 *(自 2.0.0 起)*：
[`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208)。

<a id="notes"></a>

## 说明

- **一个名称只声明一个事物。** 同名两个函数 — 无论是在一个文件中，还是在一个文件与它所
  导入的头文件中 — 都会触发 [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210)；`A::B` 与 `A_B` 也是
  同一个名称，同理。与内建函数同名的函数（`lerp`、`saturate`……）会触发
  [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206)。
- **名称区分大小写** *(自 2.0.0 起)*。`Luma` 与 `luma` 是两个函数。一个 `Graph` 调用
  如果仅在大小写上匹配某个函数，且匹配是唯一的，仍会解析成功，并给出
  [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275)。
- **头文件会被单独解析。** 被导入的 `.dsh` 不会被粘贴到文件中；它的函数会被声明到其中。
  参见 [import](import.md)。
- 仅包含 `Function` 块的 `.dsm` 或 `.dsf` 会被接受，并且不产生任何内容。
- 在 UE 5.3 上，生成的 Custom 节点会在材质图中显示其代码；从 UE 5.4 起，
  每个生成的 Custom 节点上 `ShowCode` 都被设置为 `false`。
- 旧式分节风格函数体 `Function Name { Inputs = { … } Code = { … } }` **不是**受支持的
  形式。如果 `{` 中原本应是参数列表的 `(` 的位置为空，则会触发 `DSH6300`。

<a id="diagnostics"></a>

## 诊断

每条诊断都带有其所属构造的行号和列号。各表格说明了每个代码何时会被触发；代码页面中给出了
对应的消息。

<a id="parse-time"></a>

### 解析时

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH6300` | `Function`、修饰符或返回类型之后没有名称 — 包括 `Function SelfContained(` 以及分节风格的 `Function Name {` |
| `DSH6306` | 使用了 `Inline`（警告） |
| `DSH6319` | 名称之后没有 `(`，或参数列表之后没有 `{` |
| `DSH6301` | 某个参数不是 `[in\|out] Type Name` |
| `DSH6302` | 使用了除 `in` / `out` 之外的限定符，包括 `inout` |
| `DSH6303` | 某个参数被命名为 `__return` |
| `DSH6304` | 同时存在返回类型和 `out` 参数 |
| `DSH6305` | 既没有返回类型，也没有 `out` 参数 |
| `DSH6308` | 在返回类型下出现了顶层的裸 `return;` |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | 参数列表或函数体从未闭合 |

<a id="binding"></a>

### 绑定

| 代码 | 引发条件 |
| :-- | :-- |
| `DSH4201` | 类型标记未指向任何类型 |
| `DSH4210` | 名称被声明了两次 |
| `DSH4214` | 两个参数共用同一名称 |
| `DSH6206` | 该名称属于某个内置项 |
| `DSH6210` | 一个 `MaterialAttributes` 输入 |

<a id="custom-node-code"></a>

### 自定义节点代码

| 代码 | 引发条件 |
| :-- | :-- |
| `DSH6252` | 节点自身函数上的一个 `MaterialAttributes` 输入 |
| `DSH6253` | 签名中的 `Substrate` |
| `DSH6254` | 返回了纹理或采样器，或通过一个 `out` 交回 |
| `DSH6255` | 两个参数最终共用同一个 HLSL 名称——通常是 `<Texture>Sampler` |
| `DSH6256` | 在一个没有返回类型的函数中出现一个 `return;` |
| `DSH6257` | 有返回类型，但函数体中没有 `return`（警告） |
| `DSH6258` | 一个路径为空的前置 `#include` |
| `DSH6259` | 函数体中的调用仅大小写匹配某个函数（警告） |
| `DSH6260` | 函数体互相循环调用的函数 |
| `DSH6261` | 函数体调用的函数不是 `Function` |
| `DSH6262` | 对一个接收纹理的函数的调用传入了错误数量的参数 |
| `DSH6263` | 此类调用的某个纹理参数不是普通名称 |
| `DSH6265` | 对任何其他函数的调用所传参数数量与其 HLSL 接受的数量不同 |
| `DSH6266` | 函数体读取了对某个 HLSL 不返回任何值的函数的调用结果 |
| `DSH6327` | 函数体调用了一个带有被提升的 `UE.*` 调用的 `GraphFunction` |

<a id="call-time"></a>

### 调用时

| 代码 | 引发条件 |
| :-- | :-- |
| `DSH4208` | 被调用的名称未声明任何内容 |
| `DSH5275` | 被调用的名称仅大小写匹配（警告） |
| `DSH4216` | 某个命名参数未指向任何参数 |
| `DSH4217` | 未传入某个输入 |
| `DSH4224` | 参数过多 |
| `DSH4239` | 接收者不是变量 |
| `DSH4218` | 接收者的类型与其输出不匹配 |
| `DSH5283` | 声明了未声明的接收者（信息） |
| `DSH4361` | 矩阵跨越了节点引脚 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 发射器无法创建自定义节点 |

消息中的 `{Name}` 是函数的名称；对于 [`Namespace`](namespace.md) 的成员，请参阅
[该页面](namespace.md#notes)。

完整的跨阶段列表见[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
// DShader/Lib/Color.dsh

Function float Luma(in vec3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

Function SelfContained Remap01(in float value, out float result)
{
    result = saturate(value * 0.5 + 0.5);
}

Function SampleTinted(in Texture2D tex, in vec2 uv, in vec3 tint, out vec3 rgb, out float alpha)
{
    float4 texel = Texture2DSample(tex, texSampler, uv);
    rgb   = texel.rgb * tint;
    alpha = texel.a;
}
```

从一个 `Shader` 中使用：

```c
import "Lib/Color.dsh"

Shader(Name="Materials/M_Tinted")
{
    Properties = {
        Texture2D BaseTex = Path(Game, "Textures/T_Base");
        vec3      Tint    = vec3(1.0, 0.6, 0.2);
    }
    Outputs = {
        vec3  Color;
        float Alpha;
        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }
    Graph = {
        vec2  UV = UE.TexCoord(Index = 0);
        vec3  Rgb;
        float A;
        SampleTinted(BaseTex, UV, Tint, Rgb, A);

        float Key;
        Remap01(Luma(Rgb), Key);

        Color = Rgb * Key;
        Alpha = A;
    }
}
```

每次调用都是一个自定义节点。`Luma` 节点的代码——注意函数体自身的 `return`：

```hlsl
// Begin DreamShader source: DShader/Lib/Color.dsh
// DreamShader custom: Luma line 4

    return dot(color, float3(0.299, 0.587, 0.114));
// End DreamShader source: DShader/Lib/Color.dsh
```

`SampleTinted` 节点：引脚 `tex`、`uv`、`tint`；输出 `rgb`（`Float3`）以及附加输出
`alpha`（`Float1`）。其代码声明并返回第一个 `out`，并读取引擎在
`tex` 旁声明的采样器：

```hlsl
float3 rgb = (float3)0;
// Begin DreamShader source: DShader/Lib/Color.dsh
// DreamShader custom: SampleTinted line 14

    float4 texel = Texture2DSample(tex, texSampler, uv);
    rgb   = texel.rgb * tint;
    alpha = texel.a;
// End DreamShader source: DShader/Lib/Color.dsh
return rgb;
```

<a id="see-also"></a>

## 另请参阅

- [GraphFunction](graph-function.md) —— 其 `UE.*` 调用会变为真实材质节点的变体
- [命名空间](namespace.md) —— `Ns::Fn` 限定与扁平化后的 `Ns_Fn` 名称
- [调用函数](../graph/calls.md) —— 值调用与语句调用、参数规则
- [2.0 语言](../language-v2/index.md) —— `/// @custom`，一个 `Function` 被读取时所依据的内容
- [生成的 HLSL](../generation/generated-hlsl.md) —— 1.x 为 `Function` 代码块编写的 `.ush` 包含文件
- [类型标记](types.md) —— 完整的标记目录与 GLSL 别名表
- [输入 / 输出 / 结果](inputs-outputs.md) —— *section* 参数语法，含 `opt` 与元数据
- [ShaderFunction](shader-function.md) —— 生成真实 `UMaterialFunction` 资产的代码块
- [源文件](source-files.md) —— `.dsm` / `.dsh` / `.dsf` 中哪一个可以容纳 `Function`
- [词法元素](lexical.md) —— 注释、标识符以及大小写敏感性矩阵
- [HLSL 库](../builtins/hlsl-library.md) —— `Function` 函数体内可用的辅助工具
- [Substrate 内置项](../builtins/substrate.md) —— 为什么 `Substrate` 属于 `GraphFunction`/`ShaderFunction` 关注点
- [诊断索引](../diagnostics/index.md) —— 按阶段划分的每一种代码
