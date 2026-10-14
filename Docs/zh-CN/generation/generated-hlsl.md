---
title: "生成的 HLSL"
description: "生成的 HLSL"
---

<a id="generated-hlsl"></a>

# 生成的 HLSL

> [DreamShader](../index.md) » [生成](index.md) » **生成的 HLSL**

编译为源的 `Function` 块写入的 HLSL。*(自 2.0.0 起)* 它被写入每次调用所生成的
Custom 节点中；不会生成 `.ush` include。

| | |
| :-- | :-- |
| 写入时机 | 一个 `Graph` 调用 [`Function`](../language/function.md) 时——每次调用一个 Custom 节点 |
| 写入位置 | Custom 节点的 `Code`；其 `IncludeFilePaths` 仅携带从函数体中提升出来的 `#include` 行 |
| 包含内容 | 被调用函数的函数体本身，以及该函数体调用的每个 `Function`，嵌入在一个包装结构体中 |
| 1.x 及之前 | 每个源一个 `/DreamShaderGenerated/<basename>_<hash>.ush`，包含每个 `Function`，由各节点分别 include |

1.x 的 `Function` 被旧版前端读取为带有逐字函数体的 `/// @custom` 函数，即 Custom 节点的 2.0 形式——参见 [`/// @custom` 函数体](../language-v2/index.md#-custom-bodies)。一个
[`GraphFunction`](../language/graph-function.md) 也是如此，其 `UE.*` 调用被提升为
连接到额外输入的图表节点。

<a id="synopsis"></a>

## 概要

```text
<node code>     := [ <wrapper> ] <body block>
<wrapper>       := struct <wrapper type> { <helper definition> … }; <wrapper type> <wrapper var>;
<wrapper type>  := generated_wrapper_ <SanitizeIdentifier(function)> _ <CRC32(function) as %08X>
<wrapper var>   := __ds_wrapper_ <CRC32(function) as %08X>
<helper symbol> := DreamShaderFn_ <SanitizeIdentifier(helper name)>
<body block>    := [ <first out> declared ] <begin marker> <custom marker> <body> <end marker> [ return … ]
```

`function` 是构建该节点所用的 `Function` 的名称。仅当其函数体调用另一个 `Function` 时，才会写入该包装结构体。

<a id="location"></a>

## 位置

*(自 2.0.0 起)* 无需定位文件：代码存在于节点中，位于资产内，并派生自 [构建键](caching.md) 所哈希的源，因此未更改的源会像其他所有内容一样被跳过。

模块启动时，虚拟目录 `/DreamShaderGenerated` 仍映射到 **生成着色器目录**
项目设置（类别 `Paths`，默认 `Intermediate/DreamShader/GeneratedShaders`），并且 *工具 ▸ DreamShader ▸ 清理生成的着色器* 仍会删除其下的每个 `*.ush`——
递归地、一次一个文件，从不删除目录本身——并排队进行一次完整强制重新编译。不再有任何
编译写入该处；在那里发现的文件是由 1.x 构建写入的。由 1.x 构建保存的材质可能仍包含引用其中一个的 Custom 节点；重新构建该材质会将其替换为
携带自身代码的节点。

<a id="node-code-layout"></a>

## 节点代码布局

```hlsl
struct generated_wrapper_<Name>_<HASH>             // only when the body calls other Functions
{
	<helper definition>

	<helper definition>

};
generated_wrapper_<Name>_<HASH> __ds_wrapper_<HASH>;

<T> <first out> = (<T>)0;                           // only when the first result is an `out` parameter
// Begin DreamShader source: <project-relative path of the file that declares the function>
// DreamShader custom: <Name> line <N>
<body, verbatim>
// End DreamShader source: <same path>
return <first out>;                                 // only when the body needs one, see below
```

| 元素 | 规则 |
| :-- | :-- |
| 函数体 | 花括号之间的文本，在 1.x [函数体规范化](../language/function.md#body-normalisation) 之后。它从不重新缩进，也没有任何重写会添加或删除一行，因此着色器编译错误可映射回其源代码行；`<N>` 是函数体左花括号所在行 |
| 路径 | 相对于项目，因此代码——连同着色器键——在每台机器上都相同。在导入的 `.dsh` 中声明的函数会命名该头文件 |
| 首个结果 | 声明的返回类型，或者对于没有返回类型的函数，其首个 `out` 参数（旧版规则 L9）：该参数是节点的主输出，其他 `out` 参数是附加输出 |
| 最终 `return` | 当函数体自身没有顶层 `return` 或函数不返回任何内容时添加：`return <first out>;`，否则为 `return 0.0;`。函数体从不返回的返回类型属于 [`DSH6257`](../diagnostics/DSH6xxx.md#dsh6257) |
| 输入 | 每个 `in` 参数都是节点输入；纹理输入会在其旁边获得引擎的 `<name>Sampler` |

<a id="function-name-mangling"></a>

## 函数名称改编

每个嵌入的辅助函数都会加前缀。没有它，一个 `Function Luminance(float3)` 会重新定义
引擎自有的 `Luminance`（来自 `/Engine/Private/Common.ush`），并导致着色器编译失败，报错
`redefinition of 'Luminance'`。

```text
DreamShaderFn_ + SanitizeIdentifier(<Function name>)
```

`SanitizeIdentifier` 按顺序应用这些规则：

| # | 规则 |
| :-- | :-- |
| 1 | `[A-Za-z0-9_]` 之外的每个字符都变为 `_` |
| 2 | 结果为空时变为字面量 `DreamShaderSymbol` |
| 3 | 结果全为下划线时变为字面量 `DreamShaderSymbol` |
| 4 | 首个字符不是 `[A-Za-z_]` 时，前置一个 `_` |
| 5 | 连续下划线的连续串合并为一个 |

| DSL 名称 | 函数名称 | 发出的符号 |
| :-- | :-- | :-- |
| `Luma` | `Luma` | `DreamShaderFn_Luma` |
| `Common::ApplyTint` | `Common_ApplyTint` | `DreamShaderFn_Common_ApplyTint` |
| `Common::Remap01` | `Common_Remap01` | `DreamShaderFn_Common_Remap01` |

`Namespace(Name="Common")` 内的 `Function` 是函数 `Common_ApplyTint`，因此顶层的
`Function Common_ApplyTint` 是同一名称被声明了两次：
[`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210)。

<a id="per-function-definition-shape"></a>

## 逐函数定义形式

作为包装结构体成员的嵌入辅助函数：

```hlsl
	<RetType> DreamShaderFn_<Name>(<parameters>)
	{
		<out parameter> = (<T>)0;                   // every `out` parameter but the first result
		<T> <first out> = (<T>)0;                   // when the first result is an `out` parameter
// Begin DreamShader source: <path>
// DreamShader custom: <Name> line <N>
<body, verbatim>
// End DreamShader source: <path>
		return <first out>;                         // when the first result is an `out` parameter
	}
```

| 元素 | 规则 |
| :-- | :-- |
| `RetType` | 声明的返回类型，或者当没有返回类型时，首个 `out` 参数的类型 |
| 参数 | 按声明顺序的每个参数，但那个首个 `out` 除外；`out` 参数以 `out <type> <name>` 形式表示 *(自 2.0.0 起；1.x 将输入放在最前)* |
| 采样器配对 | 紧跟在每个纹理类型参数之后，一个额外的 `SamplerState <name>Sampler` |
| 其他 `out` 参数 | 首先零初始化：HLSL `out` 到达时未初始化 |
| 函数体 | 与节点自身代码中一样：位于标记之间，逐字保留 |

为采样器配对而进行的纹理类型参数比较，针对以下五个 token，不区分大小写：

| `Texture2D` | `TextureCube` | `Texture2DArray` | `Texture3D` | `VolumeTexture` |
| :-- | :-- | :-- | :-- | :-- |

`SamplerState` **不是**其中之一；声明为 `SamplerState` 的参数不会获得配套
参数。一个纹理参数 `T` 位于名为 `TSampler` 的参数旁边属于
[`DSH6255`](../diagnostics/DSH6xxx.md#dsh6255)。

参数以其声明类型的 2.0 拼写书写：

| 声明的 token | 发出的 HLSL 类型 |
| :-- | :-- |
| `vec2`…`vec4`、`ivec*`、`uvec*`、`bvec*`、`mat2`…`mat4` | `float2`…`float4`、`int*`、`uint*`、`bool*`、`float2x2`…`float4x4` |
| 其他大小写形式的标量、向量或矩阵类型（`Float3`） | 小写 |
| `VolumeTexture` | `Texture3D` |
| 其他所有 | 按声明原样 |

<a id="includes-on-the-node"></a>

## 节点上的 Include

函数体开头的 `#include "…"` 行会被原地置空——用空格覆盖，使它们之后的每个位置仍与源匹配——并改为添加到节点的 `IncludeFilePaths`
中：先嵌入辅助函数的，按它们被嵌入的顺序，然后是函数自身的，每个
路径各一次。路径为空的 `#include` 属于 [`DSH6258`](../diagnostics/DSH6xxx.md#dsh6258)。参见
[函数 ▸ Include](../language/function.md#includes)。

<a id="self-contained-functions"></a>

## 自包含函数

节点嵌入其函数体所调用 `Function` 的**传递闭包**：每个被调用者一次，
依赖项在其依赖者之前。递归没有 HLSL 形式；它们之间的循环属于
[`DSH6260`](../diagnostics/DSH6xxx.md#dsh6260)。

| 元素 | 形式 |
| :-- | :-- |
| 结构体类型 | `generated_wrapper_<sanitized name>_<hash>` |
| 实例变量 | `__ds_wrapper_<hash>` |
| 名称 | 构建该节点所用的 `Function` |
| 哈希 | **原始、未净化**名称的 CRC32，大写十六进制 |
| 节点自身代码中的调用点 | `__ds_wrapper_<hash>.DreamShaderFn_<Name>(…)` |
| 嵌入成员之间的调用点 | 普通的 `DreamShaderFn_<Name>(…)`，无限定符 |
| 成员分隔 | 一个制表符的缩进，成员之间一个空行 |

调用点按**名称**重写：参数列表保持原样，只是在每个纹理参数之后插入一个 `<texture>Sampler` 参数。该调用必须准确传入被调用方 HLSL 所接收的参数——HLSL 没有默认参数，这里的函数也没有重载（当它携带纹理时为 [`DSH6262`](../diagnostics/DSH6xxx.md#dsh6262)，否则为 [`DSH6265`](../diagnostics/DSH6xxx.md#dsh6265)）——并且每个纹理参数都必须是普通名称（[`DSH6263`](../diagnostics/DSH6xxx.md#dsh6263)）。辅助函数返回其第一个结果，因此函数体会将其作为值来调用——`float r = Remap01(x);`——而同时将该结果作为参数传入的调用，`Remap01(x, r);`，会按照 1.x 的方式重塑：`r = Remap01(x);`。*（2.0.0 – 2.1.2 会将其保持原样，对着色器编译器来说多了一个参数。）* 不返回任何值的函数——即一个被声明为 `void` 的 `/// @custom`，而非 1.x 的函数——其值不能被读取（[`DSH6266`](../diagnostics/DSH6xxx.md#dsh6266)）。

函数体内的命名空间限定调用可以正常工作*（自 2.0.0 起）*：函数体规范化会将 `Common::Remap01(` 展平为 `Common_Remap01(`，即该命名空间函数自身的名称，因此它会像其他任何调用一样被找到并嵌入。在 1.x 中，它会调用一个未定义的 `Common_Remap01`，导致着色器编译失败。

`SelfContained`（及其旧拼写 `Inline`，[`DSH6306`](../diagnostics/DSH6xxx.md#dsh6306)）会请求 1.x 嵌入闭包；现在每个节点都会在无需请求的情况下嵌入它，因此该修饰符不会改变任何东西，该函数就是一个普通的 `/// @custom`。*（2.0.0 – 2.1.0 会将其读取为 `/// @custom selfcontained`，它不会嵌入任何内容：它所调用的 `Function` 会被留给着色器编译器，[`DSH6264`](../diagnostics/DSH6xxx.md#dsh6264)。）*

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH4210` | 文件中及其导入的头文件中存在两个同名的 `Function`——包括一个命名空间的和一个顶级的、展平后同名的 |
| `DSH6260` | 节点嵌入的 `Function` 彼此以循环方式调用 |
| [`DSH6259`](../diagnostics/DSH6xxx.md#dsh6259) | *（警告）* 函数体调用的名称与某个 `Function` 仅大小写不同；该调用保持原样 |
| [`DSH6261`](../diagnostics/DSH6xxx.md#dsh6261) | 函数体调用的函数不是 `Function` 或 `GraphFunction`，例如 `ShaderFunction` |
| [`DSH6327`](../diagnostics/DSH6xxx.md#dsh6327) | 函数体调用了一个 `GraphFunction`，其 `UE.*` 调用已被提升为其自身节点的输入 |
| `DSH6262`, `DSH6263` | 传入纹理的调用无法与被调用方的参数匹配 |
| [`DSH6265`](../diagnostics/DSH6xxx.md#dsh6265) | 调用传入的参数数量与被调用方 HLSL 所接收的不符 |
| [`DSH6266`](../diagnostics/DSH6xxx.md#dsh6266) | 函数体读取了对其 HLSL 不返回任何值的函数的调用结果 |
| `DSH6258` | 路径为空的 `#include` 行 |

函数体中对文件中不存在的函数名称的调用会留给着色器编译器。

<a id="example"></a>

## 示例

```c
// DShader/M_Ramp.dsm
Namespace(Name="Common")
{
    Function float3 ApplyTint(in float3 color, in float3 tint) { return color * tint; }
}

Function SelfContained Remap01(in float value, out float result) {
    result = saturate(value * 0.5 + 0.5);
}

Shader(Name="Materials/M_Ramp")
{
    Properties { ScalarParameter Input = 0.25; vec3 Tint = vec3(1.0, 0.4, 0.1); }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }
    Graph {
        float R;
        Remap01(Input, R);
        Color = Common::ApplyTint(vec3(R, R, R), Tint);
    }
}
```

`Remap01` 调用会变成一个 Custom 节点，其输入为 `value`，主输出为 `result`：

```hlsl
float result = (float)0;
// Begin DreamShader source: DShader/M_Ramp.dsm
// DreamShader custom: Remap01 line 7

    result = saturate(value * 0.5 + 0.5);
// End DreamShader source: DShader/M_Ramp.dsm
return result;
```

`Common::ApplyTint` 调用会变成一个 Custom 节点，其输入为 `color` 和 `tint`；其函数体返回：

```hlsl
// Begin DreamShader source: DShader/M_Ramp.dsm
// DreamShader custom: Common_ApplyTint line 4
 return color * tint;
// End DreamShader source: DShader/M_Ramp.dsm
```

这两个函数体都没有调用另一个 `Function`，因此两个节点都没有包装器，也都没有包含任何内容。一个确实调用了的函数会携带它（哈希值仅供说明）：

```c
Function float Luma(in float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }
Function float3 Desaturate(in float3 c, in float amount) { float l = Luma(c); return lerp(c, float3(l, l, l), amount); }
```

```hlsl
struct generated_wrapper_Desaturate_1A2B3C4D
{
	float DreamShaderFn_Luma(float3 c)
	{
// Begin DreamShader source: DShader/M_Gray.dsm
// DreamShader custom: Luma line 1
 return dot(c, float3(0.299, 0.587, 0.114));
// End DreamShader source: DShader/M_Gray.dsm
	}

};
generated_wrapper_Desaturate_1A2B3C4D __ds_wrapper_1A2B3C4D;

// Begin DreamShader source: DShader/M_Gray.dsm
// DreamShader custom: Desaturate line 2
 float l = __ds_wrapper_1A2B3C4D.DreamShaderFn_Luma(c); return lerp(c, float3(l, l, l), amount);
// End DreamShader source: DShader/M_Gray.dsm
```

<a id="see-also"></a>

## 另请参阅

- [Function](../language/function.md)——生成此代码的模块
- [GraphFunction](../language/graph-function.md)——一个 Custom 节点，其 `UE.*` 调用被提升到图中
- [Namespace](../language/namespace.md)——限定名称如何被展平
- [`/// @custom` 函数体](../language-v2/index.md#-custom-bodies)——每个 `Function` 都会变成的 2.0 形式
- [HLSL 库](../builtins/hlsl-library.md)——`Shaders/DreamShaderBuiltins.ush`，手写的配套内容
- [缓存](caching.md)——涵盖此代码的构建键
- [项目设置](../settings/project.md)——**Generated Shader Directory**
- [临时材质](ephemeral.md)——*Clean Generated Shaders* 及其他维护操作
- [生成](index.md)——代码在管线中的构建位置
- [诊断索引](../diagnostics/index.md)——每个代码
