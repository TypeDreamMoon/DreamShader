---
title: "构造函数"
description: "构造函数"
---

<a id="constructors"></a>

# 构造函数

> [DreamShader](../index.md) » [图](index.md) » **构造函数**

以标量或向量类型命名的调用语法表达式，用其参数构建该宽度的值。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf` — 在 `Graph { … }` 主体内、`Outputs` 绑定表达式中，或 `Outputs` 声明默认值中 |
| 种类 | 表达式，调用形式 |
| 生成 | `UMaterialExpressionAppendVector`（第一个参数之后的每个参数一个），或者当所有参数都为常量时生成一个 `UMaterialExpressionConstant` / `Constant2Vector` / `Constant3Vector` / `Constant4Vector`；在直通和广播情形下不生成任何内容 |

<a id="synopsis"></a>

## 概要

```c
<constructor-call> := <constructor-name> ( <expression> [ , <expression> ] … )
```

命名参数会被拒绝。[花括号初始化器](declarations.md#brace-initializers) 正是
构造函数调用：`vec3 rgb = {r, g, b};` 会被重写为 `vec3(r, g, b)` 并遵守本页的所有规则。

解析器将构造函数名称读取为**类型** *(自 2.0.0 起)*：该调用不会进入函数查找，
绑定器会像检查 `.dss` 构造函数那样检查它。

<a id="constructor-names"></a>

## 构造函数名称

**34 个名称**，全部**不区分大小写**地匹配（`FLOAT3(…)` 也可用）。分量数是名称末尾的
数字；没有数字，或数字为 `1` 的名称是标量。

| 名称 | 分量 | 种类 | 家族 |
| :-- | :-- | :-- | :-- |
| `float` | 1 | float | HLSL float |
| `float1` | 1 | float | HLSL float |
| `float2` | 2 | float | HLSL float |
| `float3` | 3 | float | HLSL float |
| `float4` | 4 | float | HLSL float |
| `half` | 1 | half | HLSL half — 行为与 `float` 相同 |
| `half1` | 1 | half | HLSL half |
| `half2` | 2 | half | HLSL half |
| `half3` | 3 | half | HLSL half |
| `half4` | 4 | half | HLSL half |
| `vec2` | 2 | float | GLSL float 向量 |
| `vec3` | 3 | float | GLSL float 向量 |
| `vec4` | 4 | float | GLSL float 向量 |
| `int` | 1 | **int** | HLSL 有符号整数 |
| `int2` | 2 | **int** | HLSL 有符号整数 |
| `int3` | 3 | **int** | HLSL 有符号整数 |
| `int4` | 4 | **int** | HLSL 有符号整数 |
| `ivec2` | 2 | **int** | GLSL 有符号整数向量 |
| `ivec3` | 3 | **int** | GLSL 有符号整数向量 |
| `ivec4` | 4 | **int** | GLSL 有符号整数向量 |
| `uint` | 1 | **uint** | HLSL 无符号整数 |
| `uint2` | 2 | **uint** | HLSL 无符号整数 |
| `uint3` | 3 | **uint** | HLSL 无符号整数 |
| `uint4` | 4 | **uint** | HLSL 无符号整数 |
| `uvec2` | 2 | **uint** | GLSL 无符号整数向量 |
| `uvec3` | 3 | **uint** | GLSL 无符号整数向量 |
| `uvec4` | 4 | **uint** | GLSL 无符号整数向量 |
| `bool` | 1 | bool | HLSL 布尔 |
| `bool2` | 2 | bool | HLSL 布尔 |
| `bool3` | 3 | bool | HLSL 布尔 |
| `bool4` | 4 | bool | HLSL 布尔 |
| `bvec2` | 2 | bool | GLSL 布尔向量 |
| `bvec3` | 3 | bool | GLSL 布尔向量 |
| `bvec4` | 4 | bool | GLSL 布尔向量 |

*(自 2.0.0 起)* 解析器接受 HLSL 类型语法中的每种拼写，不区分大小写，因此还有少数
名称也是构造函数：

| 名称 | 分量 | 备注 |
| :-- | :-- | :-- |
| `int1`、`uint1`、`bool1` | 1 | HLSL 中标量的拼写，正如 `float1` 一样 |
| `double`、`double1` … `double4` | 1–4 | 种类为 double；在图中像每个数字一样是 float |
| `StaticBool` | 1 | 读取为 `bool`，与在声明中一样 |
| `float2x2` … `float4x4`、`mat2`、`mat3`、`mat4`（以及其他家族的 `NxM`） | — | 矩阵，图没有对应的值：[`DSH4361`](../diagnostics/DSH4xxx.md#dsh4361) |

<a id="names-that-do-not-exist"></a>

### 不存在的名称

| 不存在 | 原因 |
| :-- | :-- |
| `vec1`、`ivec1`、`uvec1`、`bvec1` | GLSL 家族从 2 开始 |
| `hvec2`、`hvec3`、`hvec4` | 没有 GLSL half 家族 |
| `dvec2`、`dvec3`、`dvec4` | 没有 GLSL double 家族 |
| `Scalar`、`Vector`、`Color` | 已移除的类型别名；见 [类型](../language/types.md) |

上述每个名称都作为普通标识符进行词法分析，并且由于它不是类型，会落到
函数查找 — [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208)，除非声明了该名称的函数。

`MaterialAttributes`、`Substrate`、纹理类型和 `SamplerState` 也是类型名称，但无法
构造它们的任何值：`Texture2D(…)` 是 [`DSH4223`](../diagnostics/DSH4xxx.md#dsh4223)。

> [!WARNING]
> **构造函数名称会遮蔽一切。** 构造函数名称在解析调用时就被读取为类型 — 早于对 `UE.*`
> 内置函数、`Substrate.*` 内置函数、数学内置函数、参数以及所有用户定义的查找。一个名为
> `float3`、`int`、`bool4`、`vec2`（或表中任何其他名称）的
> `Function`、`GraphFunction`、`ShaderFunction` 或 `VirtualFunction` 可以在没有诊断的情况下声明，
> 但永远无法被调用 — 每个调用点都会构建一个构造函数。见
> [名称解析](name-resolution.md)。

<a id="integer-constructors"></a>

### 整数构造函数

整数名称为 `int`、`int1..4`、`ivec2..4`、`uint`、`uint1..4` 和 `uvec2..4`。对编译器而言，它们的结果
是 `int` 或 `uint` 值。

该种类只做一件事：当**两个**操作数都是整数构造函数调用时，`/`
会被拒绝（[`DSH4243`](../diagnostics/DSH4xxx.md#dsh4243)），与 1.x 中一样。它不执行截断、不舍入，
也不做范围限制 — 整数构造函数生成的浮点值图与其 `float`
对应物相同，而 `int(7.9)` 为 7.9。见 [整数除法](expressions.md#integer-division)。

该种类来自构造函数自身的名称，因此用非整数构造函数包裹会清除它：
`float(int(7))` 是 float。

<a id="argument-rules"></a>

## 参数规则

先检查调用及其参数；每个失败的检查都会被报告。

| # | 条件 | 行为 |
| :-- | :-- | :-- |
| 1 | 任何参数被命名（`float3(x = 1.0)`） | [`DSH4225`](../diagnostics/DSH4xxx.md#dsh4225) |
| 2 | 完全没有参数（`float3()`） | [`DSH4221`](../diagnostics/DSH4xxx.md#dsh4221) |
| 3 | 某个参数不是数字或 bool — 纹理对象、`MaterialAttributes` 或 `Substrate` 值 — 或者是矩阵 | [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226) |

然后，令 `N` 为构造函数的分量数：

| # | 情况 | 行为 | 创建的节点 |
| :-- | :-- | :-- | :-- |
| 4 | 恰好一个参数，且带有 `N` 个分量（对于 `N == 1`，则为标量） | 该参数被**原样**返回；只有它的种类发生变化 | 无 |
| 5 | `N > 1`，恰好一个参数，且该参数为标量 | **广播**：标量被连接到读取该值的位置，材质节点复制它 *(自 2.0.0 起；1.x 构建了一条 `AppendVector` 链)* | 无；当标量为常量时创建一个 `ConstantNVector` |
| 6 | 其他情况 | 各参数的分量数之和必须**恰好**等于 `N` | 第一个参数之后的每个参数各一个 `AppendVector` |
| 7 | 总和不等于 `N` | [`DSH4222`](../diagnostics/DSH4xxx.md#dsh4222) | — |

<a id="component-packing"></a>

### 分量打包

规则 6 是通道从左到右的简单串联——只要总数精确，混合宽度的参数就是合法的。

| 调用 | 总数 | 有效 | 结果 |
| :-- | :-- | :-- | :-- |
| `float4(rgb, 1.0)` | 3 + 1 | 是 | `float4` |
| `float4(uv, uv)` | 2 + 2 | 是 | `float4` |
| `float3(x, yz)` | 1 + 2 | 是 | `float3` |
| `float4(x, y, z, w)` | 1×4 | 是 | `float4` |
| `vec3(0.5)` | 标量广播（规则 5） | 是 | `(0.5, 0.5, 0.5)` |
| `float3(rgba)` | 4 | **否** | `DSH4222` |
| `float3(uv)` | 2 | **否** | `DSH4222` |
| `float(rgb)` | 3 | **否** | `DSH4222` |
| `float3()` | 0 | **否** | `DSH4221` |

> [!NOTE]
> **构造函数从不收窄**，在 1.x 源码中也是如此。`float3(rgba)` 是错误，而不是截断；请编写 `rgba.rgb`。收窄只发生在转换位置——参见
> [转换](conversions.md#narrowing)。加宽只从标量发生（规则 5）；
> `float3(uv)` 不会补零。

<a id="constant-folding"></a>

## 常量折叠

当每个参数都是常量时，整个调用是一个常量节点，而不是 `N` 个 `Constant`
节点加上 `AppendVector` 个节点：一个持有这些值的 `Constant` / `Constant2Vector` / `Constant3Vector` /
`Constant4Vector`。常量是数字字面量、其取负形式、一个 `const`
属性、持有常量的变量，或对常量的任何算术运算：折叠过程会折叠
操作数全为常量的运算符、构造函数和 swizzle *(自 2.0.0 起)*。

| 编写 | 折叠为 |
| :-- | :-- |
| `vec2(0.5, 1.0)` | `Constant2Vector(0.5, 1.0)` |
| `vec3(0.5)` | `Constant3Vector(0.5, 0.5, 0.5)` — 广播也会折叠 |
| `float4(1.0, 2.0, 3.0, -1.0)` | `Constant4Vector(1, 2, 3, -1)` |
| `int3(1, 2, 3)` | `Constant3Vector(1, 2, 3)` *(自 2.0.0 起)*；对编译器而言该值仍为 `int3` |
| `vec3(K, 0.0, 0.0)` | 当 `K` 是参数时不折叠——两个 `AppendVector` 节点 |

每个值都具有其类型的宽度；折叠后的构造函数没有自己的宽度，不会
覆盖声明 *(自 2.0.0 起)*。参见
[转换](conversions.md#authoritative-component-counts)。

相等的常量是同一个节点：写入五次的 `vec3(0.5)` 只产生一个 `Constant3Vector`。参见
[节点复用](node-reuse.md)。

<a id="notes"></a>

## 说明

- 规则 4 **不创建任何节点**。`float3(SomeFloat3)` 是空操作包装；`float3(SomeInt3)` 只
  将种类改为 float。
- 构造函数调用是普通后缀表达式，可以进行 swizzle：`vec4(uv, 0, 1).xy`。
- 空主体的花括号初始化器（`float3 v = {};`）根本不会到达构造函数——它
  产生该类型的零值。参见 [声明](declarations.md)。
- 不支持嵌套花括号初始化器：`float4 m = {{1,2},{3,4}};` 会被重写为
  `float4({1,2},{3,4})`，而作为参数的花括号列表是
  [`DSH2214`](../diagnostics/DSH2xxx.md#dsh2214)。
- 构造函数上的输出选择器（`Output=`、`OutputIndex=`）
  [`DSH5253`](../diagnostics/DSH5xxx.md#dsh5253)。

<a id="diagnostics"></a>

## 诊断

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH4225` | 任何 `name = value` 参数。 |
| `DSH4221` | 没有参数的构造函数。 |
| `DSH4226` | 参数为纹理对象、`MaterialAttributes` 或 `Substrate` 值，或矩阵。 |
| `DSH4222` | 参数宽度之和不等于构造函数的宽度，且调用并非单个标量。 |
| `DSH4223` | 没有构造函数的类型：`MaterialAttributes`、`Substrate`、纹理类型、`SamplerState`。 |
| `DSH4361` | 矩阵构造函数：图中没有矩阵值。 |
| `DSH2214` | 嵌套在花括号初始化器中的花括号列表。 |
| `DSH5253` | 构造函数上的输出选择器。 |
| `DSH4208` | 拼写错误的构造函数（`vec1`、`float5`）会落入函数查找。 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 发射器无法创建 `AppendVector` 或常量节点。 |

<a id="example"></a>

## 示例

```c
Shader(Name="Docs/M_Constructors", Root="Game")
{
    Properties { ScalarParameter K = 0.5; }
    Settings   { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }

    Graph {
        float  a    = frac(K * 3.0);
        vec3   grey = vec3(a);              // rule 5: scalar broadcast, no node
        vec3   tint = vec3(0.2, 0.4, 0.8);  // constant-folded to Constant3Vector
        vec2   uv   = float2(a, 1.0);       // rule 6: 1 + 1 packing
        Color = tint * grey + float3(uv, 0.0);   // rule 6: 2 + 1 packing
    }
}
```

生成的节点：

```text
ScalarParameter K
Constant(3.0), Multiply, Frac                      -> a
(no node)                                          -> vec3(a): `a` is wired where grey is read
Constant3Vector(0.2, 0.4, 0.8)                     -> tint               (folded, 1 node)
Multiply                                           -> tint * grey
Constant(1.0), AppendVector                        -> float2(a, 1.0)     (1 node)
Constant(0.0), AppendVector                        -> float3(uv, 0.0)
Add                                                -> Base.EmissiveColor
```

<a id="see-also"></a>

## 另请参阅

- [表达式](expressions.md) — 运算符、优先级和整数除法规则
- [字面量](literals.md) — 什么算作可折叠的常量
- [转换](conversions.md) — 收窄、加宽和值的宽度
- [Swizzle](swizzle.md) — 收窄值的正确方式
- [声明](declarations.md) — 花括号初始化器和声明的类型标记
- [名称解析](name-resolution.md) — 构造函数位于其顶端的调用解析顺序
- [类型](../language/types.md) — 完整的类型标记目录和已移除的别名
- [`UE.TransformVector` / `UE.TransformPosition`](../builtins/transform.md) — 唯一的类矩阵操作
- [节点复用](node-reuse.md) — 常量去重
