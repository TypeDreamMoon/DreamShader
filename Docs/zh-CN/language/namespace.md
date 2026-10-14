---
title: "命名空间"
description: "命名空间"
---

<a id="namespace"></a>

# 命名空间

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **命名空间**

一个顶层块，它会用 `<Name>::` 作为其所包含的 `Function` 和 `GraphFunction` 声明名称的前缀。

| | |
| :-- | :-- |
| 声明于 | `.dsm`, `.dsf`, `.dsh` |
| 类型 | 顶层块 |
| 生成 | 不直接生成任何内容——其成员生成的正是它们在顶层会生成的内容 |
| 多重性 | 每个文件可出现任意次数；同一名称可以多次打开 |

<a id="synopsis"></a>

## 语法概要

```c
Namespace(Name = "<identifier>" [ , Generate = "<template>" ] [ , Library = "<category>" ] [ , Description = "<text>" ])
{
    { <function-declaration> | <graph-function-declaration> } …
}
```

关键字 `Namespace` 的匹配**区分大小写**。`namespace` 和 `NAMESPACE` 不是该关键字。

<a id="header-attributes"></a>

## 头部属性

| 属性 | 必需 | 值 | 作用 |
| :-- | :-- | :-- | :-- |
| **`Name`** | 是 | 字符串 | 添加到每个成员名称之前的限定符 |
| `Generate` | 否 | 字符串，`{Name}` 成员名称 | *(2.1.2 之后)* 在包的头部中，每个成员都会生成一个该名称的材质函数——见[包 ▸ 资源头部](../../tools/packages.md#asset-headers)。成员自身的 `Generate` 优先；对其设置 `Generate = ""` 可使其退出 |
| `Library` | 否 | 字符串 | *(2.1.2 之后)* 每个生成函数的函数库分类 |
| `Description` | 否 | 字符串 | *(2.1.2 之后)* 每个生成函数的描述 |

任何其他键都会被解析并静默忽略。属性键的匹配不区分大小写，因此 `Namespace(name="Common")` 也可用。值可以加引号或直接书写；裸值会延续到括号外的下一个 `,` 或 `)`。重复写入的键会触发警告（[`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244)），且后写的值生效。`)` 前的尾随逗号会被接受，与 1.x 中一样 *(2.0.0 – 2.1.0 曾以 [`DSH2243`](../diagnostics/DSH2xxx.md#dsh2243) 拒绝它)*。

`Name` 必须是标识符——一个字母或 `_`，随后是字母、数字和 `_`。名称缺失、为空或仅含空白，以及包含 `::`、`.`、`-`、空格或任何其他字符的名称，都会触发 [`DSH6309`](../diagnostics/DSH6xxx.md#dsh6309)。因此**不存在多段声明形式**：`Namespace(Name="A::B")` 是错误。

<a id="body-contents"></a>

## 主体内容

| 构造 | 是否接受 |
| :-- | :-- |
| [`Function`](function.md) | 是，任意数量 |
| [`GraphFunction`](graph-function.md) | 是，任意数量 |
| 嵌套的 `Namespace` | **否** |
| `Shader`、`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend`、`VirtualFunction` | **否** |
| 节（`Properties`、`Settings`、`Inputs`、`Outputs`、`Graph`、`Layout`……） | **否** |
| `import` | **否** *(自 2.0.0 起)*——导入只能位于文件作用域 |

成员可以按任意顺序出现任意次数；成员之间的杂散 `;` 会被跳过。两个关键字在主体内的匹配均区分大小写，与顶层完全一致。任何其他内容都会触发 [`DSH6310`](../diagnostics/DSH6xxx.md#dsh6310)，解析器会在下一个成员关键字或结束 `}` 处恢复。

<a id="no-nesting"></a>

### 不可嵌套

`Namespace` 只能从顶层关键字循环访问；主体只识别这两个函数关键字。因此嵌套的 `Namespace` 会触发 `DSH6310`。无法声明 `A::B::C`，并且重新打开也不会组合：

```c
Namespace(Name="A") { Namespace(Name="B") { Function f(out float r) { r = 0; } } }
// DSH6310 at the inner Namespace
```

<a id="name-flattening"></a>

## 名称扁平化

`Namespace` 不是实体。不会为它存储任何对象，它也不会创建作用域。它唯一的作用是影响成员的名称，该名称 *(自 2.0.0 起)* 是扁平化后的标识符：

```text
Namespace(Name = "Common") { Function ApplyTint(…) }   →   Common::ApplyTint   →   function Common_ApplyTint
```

扁平化即标识符清理器：每个非 `[A-Za-z0-9_]` 字符都会变为 `_`，连续的下划线串会合并为一个（`Common__ApplyTint` → `Common_ApplyTint`）。成员会以该名称落在顶层函数之间；名称查找、绑定器诊断以及生成的 HLSL 符号 `DreamShaderFn_Common_ApplyTint` 都会看到 `Common_ApplyTint`。限定写法只在一个地方保留：成员 Custom 节点的标题（`Description`）是 `Common::ApplyTint`，与 1.x 一致。

> [!WARNING]
> 由于 `::` 和 `_` 扁平化后是同一个东西，`Namespace(Name="Common") { Function ApplyTint … }` 和顶层 `Function Common_ApplyTint …` 声明的是同一个名称：[`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210)。

<a id="calling-a-namespaced-function"></a>

## 调用带命名空间的函数

在 [`Graph`](../graph/index.md) 块中，使用 `Ns::Fn(…)`。`::` 链会作为一个名称读取并以相同方式扁平化，因此该调用会到达 `Ns_Fn`。

```c
Graph = {
    vec3 Tinted;
    Common::ApplyTint(BaseColor, Tint, Tinted);   // statement call
    float K = Common::Remap01(Raw);               // value call
}
```

| 规则 | 行为 |
| :-- | :-- |
| 解析 | 限定名称，或其扁平化写法 `Common_ApplyTint`。没有 `using` 指令，也没有非限定回退：裸写 `ApplyTint(…)` 会触发 [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208)。 |
| 大小写 | *(自 2.0.0 起)* 名称区分大小写。`common::applytint(…)` 在匹配到唯一函数时仍可解析，并附带 [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275)。 |
| 修饰写法 | `DreamShaderFn_Common_ApplyTint(…)` 不再命名该函数 *(自 2.0.0 起)*：`DSH4208`。 |
| 名称缺失 | 后面没有任何内容的 `Common::` 会触发 [`DSH5260`](../diagnostics/DSH5xxx.md#dsh5260)。 |
| 调用形式 | 与非限定调用完全相同。见[调用函数](../graph/calls.md)。 |

**在包含该头部的 `.dss` 中**，`Ns::Fn(…)` 的读取方式相同 *(2.1.2 之后)*：调用会到达 `Ns_Fn`，`dsc fmt` 和语言服务会保留限定写法，未知成员会触发以 `Ns::Fn` 的原样写法命名的 [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208)。诸如 dreamshader-blender 之类的库以其自身 Graph 调用它的方式来调用：

```hlsl
#include "@typedreammoon/dreamshader-blender/Library/Noise.dsh"

export void M_Noise(inout material m)
{
    float2 uv = UE.TexCoord(Index = 0);
    float Fac;
    float3 Color;
    BL::TexNoise3D(float3(uv, 0.0), 5.0, 2.0, 0.5, 2.0, 0.0, 1.0, Fac, Color);
    m.EmissiveColor = Color;
}
```

`out` 参数是普通的 `.dss` 局部变量，在调用之前声明；1.x Graph 对接收者的隐式声明（[`DSH5283`](../diagnostics/DSH5xxx.md#dsh5283)）是一条旧规则。直到 2.1.2，`.dss` 只能以 `Ns_Fn(…)` 到达该成员，而 `Ns::Fn` 在第二个 `:` 处为 `DSH2154`。

**在另一个 `Function` 或 `GraphFunction` 主体内**，`Ns::Fn(…)` 调用可用 *(自 2.0.0 起)*。主体规范化会把 `Common::ApplyTint` 重写为 `Common_ApplyTint`，后者现在是该函数自身的名称，因此该调用会像任何其他 `Function` 调用一样被嵌入。在 1.x 中，它会以未定义的 `Common_ApplyTint` 到达着色器编译器并在那里失败。限定符的书写在 `::` 两侧不能有空格。

<a id="notes"></a>

## 说明

- **允许重新打开且不进行检查。** 两个 `Namespace(Name="Common")` 块——在同一文件中或跨导入的头文件——都加上 `Common::` 前缀。没有重复命名空间诊断；两个具有相同限定名称的成员会 `DSH4210`。
- **命名空间不创建查找作用域。** 成员通过其限定（或扁平化）名称调用同级成员。在块内部，裸同级名称不会被识别为对同级成员的调用，而是留给着色器编译器处理。
- **带空块的 `Namespace(Name="X") { }`** 会被接受且不声明任何内容 *（自 2.0.0 起；1.x 会因没有块而报文件错误）*。
- **头文件单独解析。** 导入头文件的命名空间被声明到导入文件中，其成员跨文件冲突的方式与在单个文件内完全一致。参见 [import](import.md)。
- 关于成员声明名称的解析器诊断会以书写原样命名该成员（`ApplyTint`）；关于提升的 `UE.*` 调用则以 `Common::ApplyTint` 命名；解析器之后的全部内容都以扁平化的 `Common_ApplyTint` 命名。

<a id="diagnostics"></a>

## 诊断

每个代码都携带构造的行和列；代码的页面包含消息。

<a id="parse-time"></a>

### 解析时

| 代码 | 触发条件 |
| :-- | :-- |
| [`DSH2241`](../diagnostics/DSH2xxx.md#dsh2241) | `Namespace` 之后没有 `(` |
| `DSH2243` | 格式错误的属性列表 |
| `DSH2244` | 属性被写入两次（警告） |
| `DSH6309` | 没有 `Name`、为空，或者不是标识符 |
| [`DSH2257`](../diagnostics/DSH2xxx.md#dsh2257) | 头文件之后没有 `{` |
| `DSH6310` | 块中除 `Function` / `GraphFunction` 之外的任何内容——包括嵌套的 `Namespace` |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | 块从未关闭 |
| [`DSH2103`](../diagnostics/DSH2xxx.md#dsh2103) | `Name` 值的 `"` 从未关闭 |

成员声明报告其自身的错误——参见 [`Function` § 诊断](function.md#diagnostics)。

<a id="binding-and-call-time"></a>

### 绑定和调用时

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH4210` | 两个成员扁平化为同一个名称，或者成员和顶层函数如此 |
| `DSH5260` | `::` 之后没有名称 |
| `DSH4208` | 被调用的名称不声明任何内容——例如对成员的非限定调用 |
| `DSH5275` | 被调用的名称仅在大小写上匹配成员（警告） |

完整的跨阶段列表位于[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
// DShader/Lib/Common.dsh

Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result)
    {
        result = color * tint;
    }

    Function float Remap01(in float value)
    {
        return saturate(value * 0.5 + 0.5);
    }

    GraphFunction Pulse(in float speed, out float value)
    {
        value = sin(UE.Time() * speed);
    }
}
```

```c
import "Lib/Common.dsh"

Shader(Name="Materials/M_Common")
{
    Properties = {
        vec3  Tint  = vec3(1.0, 0.6, 0.2);
        float Speed = 2.0;
    }
    Outputs = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = {
        vec3 Gray = vec3(0.5, 0.5, 0.5);

        vec3 Tinted;
        Common::ApplyTint(Gray, Tint, Tinted);

        float Key = Common::Remap01(Tinted.r);

        float P;
        Common::Pulse(Speed, P);

        Color = Tinted * Key * P;
    }
}
```

生成的名称：

```text
declaration                     function name        Custom node title     helper symbol when embedded
Namespace "Common" > ApplyTint  Common_ApplyTint     Common::ApplyTint     DreamShaderFn_Common_ApplyTint
Namespace "Common" > Remap01    Common_Remap01       Common::Remap01       DreamShaderFn_Common_Remap01
Namespace "Common" > Pulse      Common_Pulse         Common::Pulse         (none — it lifts a UE.* call, so it is never embedded)
```

<a id="see-also"></a>

## 另请参见

- [函数](function.md) —— 成员声明语法和 `DreamShaderFn_*` 辅助符号
- [GraphFunction](graph-function.md) —— 另一种合法的成员类型
- [调用函数](../graph/calls.md) —— 值调用与语句调用
- [名称解析](../graph/name-resolution.md) —— `Graph` 块使用的查找顺序
- [import](import.md) —— 头文件的声明如何到达导入文件
- [源文件](source-files.md) —— `.dsm` / `.dsh` / `.dsf` 中哪些可以包含 `Namespace`
- [词法元素](lexical.md) —— 标识符、`::` 以及大小写敏感矩阵
- [关键字](keywords.md) —— 完整的关键字索引
- [诊断索引](../diagnostics/index.md) —— 按阶段列出的每个代码
