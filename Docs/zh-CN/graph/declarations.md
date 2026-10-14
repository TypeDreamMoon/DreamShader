---
title: "声明"
description: "声明"
---

<a id="declarations"></a>

# 声明

> [DreamShader](../index.md) » [图](index.md) » **声明**

将一个新名称引入 `Graph` 主体并将其绑定到带类型的值的语句。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` — 在 `Graph` 块内 |
| 种类 | 语句 |
| 生成 | 初始化器的节点；没有初始化器的声明会被赋予其类型的零值 |

<a id="synopsis"></a>

## 概要

```c
<type> <name> [ = { <expression> | <brace-initializer> } ]
     [ , <name> [ = { <expression> | <brace-initializer> } ] ] … ;

brace-initializer := { [ <expression> [ , <expression> ] … ] }
```

当语句以连续两个名称开头时，它就是声明——一个类型标记和变量的名称*（自 2.0.0 起；1.x 在其最后一个顶层空白处拆分文本）*。两者都是单个标记。

> [!NOTE]
> 名称是一个标识符。`float3 A.B = x;` 在 `.` 处是 [`DSH2154`](../diagnostics/DSH2xxx.md#dsh2154)
> *（自 2.0.0 起）*；1.x 声明了一个字面名为 `A.B` 的变量。成员写入前面没有类型：`Attrs.BaseColor = x;`。

<a id="accepted-type-tokens"></a>

## 可接受的类型标记

| 标记 | 分量数 | 备注 |
| :-- | --: | :-- |
| `float` `float1` `half` `half1` `int` `uint` `bool` | 1 | |
| `float2` `half2` `vec2` `int2` `uint2` `bool2` `ivec2` `uvec2` `bvec2` | 2 | |
| `float3` `half3` `vec3` `int3` `uint3` `bool3` `ivec3` `uvec3` `bvec3` | 3 | |
| `float4` `half4` `vec4` `int4` `uint4` `bool4` `ivec4` `uvec4` `bvec4` | 4 | |
| `MaterialAttributes` | 0 | 见 [MaterialAttributes](material-attributes.md) |
| `Substrate` | 0 | **UE 5.4+**；需要初始化器 |
| `StaticBool` `StaticBoolParameter` | 1 | 一个 `bool` 变量*（自 1.6.0 起）* |
| `Texture2D` | 0 | 需要初始化器 |
| `TextureCube` | 0 | 需要初始化器 |
| `Texture2DArray` | 0 | 需要初始化器 |
| `Texture3D` `VolumeTexture` | 0 | 需要初始化器 |
| `SamplerState` | 0 | 它自身的采样器，不再是 `Texture2D`*（自 2.0.0 起）*；需要初始化器 |

每一行都按大小写不敏感进行匹配（`Float3`、`VEC3`、`materialattributes`）。类型标记是一个标记*（自 2.0.0 起）*：`float 3` 和 `Material Attributes` 不再被读取为类型。`vec*`、`ivec*`、`uvec*` 和 `bvec*` 读取为 `float*`、`int*`、`uint*` 和 `bool*`。

`int`、`uint`、`bool` 和 `half` 对编译器来说是自成一类的种类——两个整数构造函数之间的 `/` 是 [`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243)，条件是一个 `bool`——并且在生成的图中它们每一个都是浮点数。见[类型标记](../language/types.md)。矩阵拼写（`mat3`、`float3x3`）可以解析，但图中没有矩阵值：[`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361)。

<a id="uninitialized-declarations"></a>

## 未初始化的声明

`<type> <name>;` 由旧版前端赋予初始化器，因为 1.x 将未设置的变量读取为零：

| 声明的类型 | 初始化器 |
| :-- | :-- |
| 标量（1 个分量） | `0`（`bool` 则为 `false`） |
| 向量（2–4 个分量） | `floatN(0, …)`——每个分量一个零，使用声明的种类 |
| `MaterialAttributes` | 一个空属性集（`MakeMaterialAttributes`） |
| `Substrate` | 无——[`DSH2215`](../diagnostics/DSH2xxx.md#dsh2215) |
| 任何纹理类型、`SamplerState` | 无——`DSH2215` |
| 无法解析的标记 | [`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201) |

```c
float3 c;              // float3(0, 0, 0)
MaterialAttributes A;  // an empty attribute set, ready for member writes
Texture2D T;           // DSH2215: requires an initializer
```

<a id="declarations-with-an-initializer"></a>

## 带初始化器的声明

初始化器先被求值，然后转换为声明的类型：

| 顺序 | 步骤 |
| --: | :-- |
| 1 | 类型标记必须能够解析，否则 `DSH4201` |
| 2 | 标量会扩展到向量类型的每一个分量 |
| 3 | 更宽的向量给出其**前导**分量——旧版规则 L22，由信息 [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) 说明 |
| 4 | 其他任何不匹配的情况——更窄的向量、纹理转为数字——都是 [`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228) |

第 4 步意味着 2 分量值永远不会加宽为 3。见[转换](conversions.md)。

<a id="authoritative-widths-override-the-declared-width"></a>

## 权威宽度覆盖声明的宽度

*（自 2.0.0 起）* 它们不再这样做了：声明的类型就是变量的类型。1.x 让某些值（`UE.TexCoord`、`UE.CameraVectorWS`、常量折叠的构造函数、`dot` 等）无论声明如何都保留自己的宽度，因此 `float2 dir = UE.CameraVectorWS();` 在没有任何消息的情况下生成了三分的 `dir`。现在 `dir` 是一个 `float2`：值的前导两个分量，当值被类型化为更宽时由 `DSH5289` 说明。

```c
Graph = {
    float3 c = Tint;            // Tint is a float4 property: DSH5289, c = Tint.rgb
    float2 dir = UE.CameraVectorWS();   // dir is two components
}
```

依赖该覆盖的 1.x 源码构建出的图与通过 1.9.x 构建的不同，而且没有任何报告：[`dsc migrate`](../tools/migrate.md) 比较 2.0 编译器从 1.x 文件构建的内容与从 `.dss` 构建的内容，两者一致。请重新构建此类材质并检查它。

二元运算符从不收窄操作数：两个宽度不同且都不是标量的操作数是 [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226)。赋值、声明、函数输入和引脚取前导分量（`DSH5289`）。见[转换](conversions.md)。

<a id="comma-declarators"></a>

## 逗号声明符

```c
float a = 1, b, c = 3;
```

每个声明符声明一个变量。规则：

| 规则 | 行为 |
| :-- | :-- |
| 识别 | 声明符后面跟着 `,` 和另一个名称的声明 |
| 共享类型 | 每个声明符都拥有**第一个**的类型；`,` 之后的类型标记会被读取为下一个声明符的名称，其后面的内容是 `DSH2154` |
| 声明符名称 | 必须是标识符——否则为 [`DSH2163`](../diagnostics/DSH2xxx.md#dsh2163) |
| 初始化器 | 每个声明符可以带有自己的 `= <expression>` 或 `= { … }`，或者不带 |
| 源位置 | 每个声明符报告自己的行和列*（自 2.0.0 起）* |

`float a = 1, b, c = 3;` 声明三个 1 分量值：`a` 初始化为 `1`，`b` 初始化为 `0`，`c` 初始化为 `3`。

<a id="brace-initializers"></a>

## 花括号初始化器

作为声明初始化器的花括号列表会被重写为声明类型的构造函数调用：`T x = { a, b };` 是 `T x = T(a, b);`。因此花括号初始化器**正是**构造函数调用，并继承[构造函数](constructors.md)的每一条规则：仅限位置参数（[`DSH4225`](../diagnostics/DSH4xxx.md#dsh4225)）、单个标量扩展到每一个分量，以及多个参数必须刚好加起来等于类型的宽度（[`DSH4222`](../diagnostics/DSH4xxx.md#dsh4222)）。

`{}` 被特殊处理：它完全不是初始化器，因此变量得到其类型的零值。

目标类型始终是声明的类型标记。其他任何地方的花括号列表——赋值的右侧或成员写入、作为参数——都是 [`DSH2162`](../diagnostics/DSH2xxx.md#dsh2162)*（自 2.0.0 起）*。纹理、`Substrate` 或 `MaterialAttributes` 类型没有构造函数：`Texture2D t = {x};` 是 [`DSH4223`](../diagnostics/DSH4xxx.md#dsh4223)。

> [!WARNING]
> 嵌套大括号不起作用。`float4 m = {{1,2},{3,4}};` 是
> [`DSH2214`](../diagnostics/DSH2xxx.md#dsh2214)；请写作 `float4(float2(1, 2), float2(3, 4))`。

```c
Graph = {
    float r = 1.0;
    float g = 0.5;
    float b = 0.2;
    vec3 rgb  = {r, g, b};      // -> vec3(r, g, b)
    vec4 rgba = {rgb, 1.0};     // -> vec4(rgb, 1.0), mixed-width packing
    Color = rgba.rgb;
}
```

<a id="redeclaration"></a>

## 重复声明

在同一代码块中，名称已被声明的声明属于
[`DSH4220`](../diagnostics/DSH4xxx.md#dsh4220)。在 `if` 或 `else` 主体中声明一个外围主体
已声明的名称也是如此，而在 `Shader` 中声明名为 `Base` 的声明
也是重复声明，因为该名称就是材质本身。

该检查会**精确**比较名称 *(自 2.0.0 起)*：`float a = 1; float A = 2;` 声明两个
变量，而 1.x 会报告重复声明。读取 `a` 或 `A` 会找到其自身的变量；在
既不精确匹配任何一者的分支中读取时，会找到唯一的大小写不敏感匹配，并给出警告
[`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275)。

对现有名称赋值不是重复声明——省略类型标记即可重新赋值。参见
[语句](statements.md#assignment)。

<a id="scope"></a>

## 作用域

每个 `if` 和 `else` 主体都是一个拥有自身作用域的代码块 *(自 2.0.0 起)*；1.x 中整个主体
只有一个值映射。

| 情形 | 语句之后可见性 |
| :-- | :-- |
| 主体级别的声明 | 在该主体的其余部分可见。在 `Shader` 中，该主体还包含 `Outputs` 声明，位于 `Graph` 语句之前 |
| 在 `if` 或 `else` 主体内部的声明 | 仅在该主体结束前可见 |
| 分支内的属性读取 | 属性是文件作用域输入，绝不是分支值 |

| 情形 | 结果 |
| :-- | :-- |
| 在 `if` **之前**声明并在某分支中赋值的变量 | 其两个值会合并为一个条件节点，变量在 `if` 之后保留合并值——参见 [if / else](if.md) |
| 在分支**内部**声明的变量 | 在分支结束时被丢弃；不会合并任何内容，`if` 之后的读取是 [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200) |
| 分支声明了外围主体已声明的名称 | `DSH4220` |

> [!WARNING]
> 1.x 会合并在两个分支中都声明的名称，并允许在 `if` 之后读取它。该名称现在
> 是分支局部的。请在 `if` **之前**声明变量，并在每个分支中赋值：
>
> ```c
> float3 Blend = float3(0.0, 0.0, 0.0);
> if (Mask > 0.5) { Blend = float3(1.0, 0.0, 0.0); }
> else            { Blend = float3(0.25, 0.25, 0.25); }
> Color = Blend;
> ```

分支局部的名称可以在 `if` 之后再次声明：其作用域已经结束。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH4201` | 类型标记无法解析 |
| `DSH2215` | 纹理、`SamplerState` 或 `Substrate` 变量没有初始值设定项 |
| `DSH4220` | 该名称已在此代码块中声明、由外围主体声明，或在 `Shader` 中是 `Base` |
| `DSH4228` | 初始值设定项与声明的类型不匹配 |
| `DSH5289` | *信息：* 较宽的初始值设定项被截取为其前导分量 |
| `DSH2154` | 声明未以 `;` 结尾，或其名称不是一个标识符 |
| `DSH2163` | `,` 之后的声明符不是名称 |
| `DSH2162` | 声明初始值设定项之外的大括号列表 |
| `DSH2214` | 大括号初始值设定项内部的大括号列表 |
| `DSH4222` | 大括号初始值设定项的各分量加起来不等于类型宽度 |
| `DSH4223` | 对没有构造函数的类型使用大括号初始值设定项 |
| [`DSH2212`](../diagnostics/DSH2xxx.md#dsh2212) | 对 Graph 变量使用 `static` 或 `const` |
| [`DSH2213`](../diagnostics/DSH2xxx.md#dsh2213) | 数组声明符，`float w[4];` |
| [`DSH5294`](../diagnostics/DSH5xxx.md#dsh5294) | 在不具备该节点的引擎上使用 `Substrate` 节点 |

完整的跨阶段列表位于[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
Shader(Name="DreamShaderTests/Corpus/M_Declarations")
{
    Properties = {
        vec4  Src = vec4(0.1, 0.2, 0.3, 0.4);
        float K   = 2.0;
    }
    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs  = { vec3 Color; Base.EmissiveColor = Color; }

    Graph = {
        float a = 1.0, b = 0.5, c;      // comma declarators; 'c' is 0
        vec3  rgb  = Src.rgb;           // swizzle of a property
        vec4  full = {rgb, a};          // brace initializer -> vec4(rgb, a)
        vec3  lit;                      // float3(0, 0, 0)

        if (K > 1.0) {
            lit = full.rgb * b;         // assignment, not declaration: 'lit' exists before the if,
        } else {                        // so both branches change the same variable and merge
            lit = full.rgb * c;
        }

        Color = lit;
    }
}
```

<a id="see-also"></a>

## 另请参阅

- [语句](statements.md) — 每种语句形式以及分类顺序
- [类型标记](../language/types.md) — 完整的按上下文有效性矩阵
- [构造函数](constructors.md) — 大括号初始值设定项继承的规则
- [转换](conversions.md) — 窄化、广播，以及各自的适用位置
- [表达式](expressions.md) — `=` 右侧可以出现的内容
- [if / else](if.md) — 分支执行与合并
- [名称解析](name-resolution.md) — 精确查找与大小写不敏感查找
- [MaterialAttributes](material-attributes.md) — 声明并写入属性值
- [Swizzles](swizzle.md) — swizzle 如何影响所声明值的宽度
- [节点复用](node-reuse.md) — 为什么两个相同的初始值设定项共享一个节点
- [不支持的构造](unsupported.md) — `return`、`for`、`+=` 及其代码
- [输出绑定](../language/output-bindings.md) — `Outputs` 声明及其初始值设定项
- [诊断索引](../diagnostics/index.md) — 按阶段列出的所有代码
