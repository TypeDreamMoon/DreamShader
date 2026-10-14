---
title: "GraphFunction"
description: "GraphFunction"
---

<a id="graphfunction"></a>

# GraphFunction

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **GraphFunction**

一个顶层块，其主体是 HLSL，但其中的 `UE.*` 调用会被提取出文本，构建为真实的
材质节点，并作为自动命名的输入引脚接入该调用的 Custom 节点。

| | |
| :-- | :-- |
| 声明于 | `.dsm`、`.dsf`、`.dsh` — 以及 [`Namespace`](namespace.md) 主体内部 |
| 种类 | 顶层块 |
| 生成 | 自身不生成任何内容。每个调用处生成一个 `UMaterialExpressionCustom` 节点，外加从其主体中提取出的材质表达式。 |
| 始于 | `1.3.1` |

旧版前端将 `GraphFunction` 读取为一个 `/// @custom` 函数，其中的 `UE.` 调用会被提取
（旧版规则 L8）；请参阅 [2.0 语言](../language-v2/index.md) 和
[`dsc migrate`](../tools/migrate.md)。

<a id="synopsis"></a>

## 概要

```c
GraphFunction [ ( <generate-attributes> ) ] [ <return-type> ] <name> ( [ <parameter-list> ] )
{
    <hlsl-with-UE-calls>
}
```

```c
<parameter-list> ::= <parameter> [ , <parameter> ] …
<parameter>      ::= [ { in | out } ] <type-token> <parameter-name>
```

关键字 `GraphFunction` 的匹配是**区分大小写**的。参数语法、`in` / `out`
限定符、“至少一个输出”规则、返回类型形式、类型标记、主体的
标识符规范化、开头的 `#include`、节点代码的形状，以及可选的
`(Generate = ..., Library = ..., Description = ...)` 头 *(2.1.2 之后)*（用于请求包从中生成
材质函数）都与 [`Function`](function.md) **完全相同** — 请阅读该页面了解所有
相关内容。本页仅记录不同之处。

<a id="how-a-graphfunction-differs-from-a-function"></a>

## GraphFunction 与 Function 的区别

| 方面 | `Function` | `GraphFunction` |
| :-- | :-- | :-- |
| `Inline` / `SelfContained` | 接受 | **不接受** — 见下文 |
| 主体中的 `UE.*` 调用 | 保留为字面 HLSL 文本，着色器编译器不认识它 | 构建为材质表达式并作为 Custom 节点输入传入 |
| 主体放置位置 | 调用节点的代码，以及任何调用它的节点的包装成员 | 调用节点的代码 |
| 可从另一个 `Function` 主体调用 | 可以 — 它被嵌入 | 仅当主体不提取任何 `UE.*` 调用时；否则为 [`DSH6327`](../diagnostics/DSH6xxx.md#dsh6327) |
| 递归 | [`DSH6260`](../diagnostics/DSH6xxx.md#dsh6260) | `DSH6260`，以及通过被提取调用实现的 [`DSH6330`](../diagnostics/DSH6xxx.md#dsh6330) |
| `Substrate` 参数和结果 | 从不（[`DSH6253`](../diagnostics/DSH6xxx.md#dsh6253)） | 从不（`DSH6253`） |

*(自 2.0.0 起)* 两者都将主体放入节点的代码中：不再有可供 `Function` 输出的共享 include，
因此 1.x 比较中的那一行已不复存在。

> [!WARNING]
> `GraphFunction SelfContained Foo(…)` 和 `GraphFunction Inline Foo(…)` 是
> [`DSH6307`](../diagnostics/DSH6xxx.md#dsh6307) *(自 2.0.0 起；1.x 跳过了修饰符检查，
> 将该词读取为返回类型)*。`GraphFunction SelfContained(…)` 声明一个名为
> `SelfContained` 的函数。`GraphFunction` 没有自包含模式。

<a id="ue-hoisting"></a>

## `UE.*` 提升

读取主体时，会对它进行扫描，每个 `UE.*` 调用都会被记录为一次被提取调用。构建
节点的代码时，调用的文本会被替换为生成的输入引脚名称；该引脚
携带的值就是该调用求值得到的材质表达式。

仅当以下**所有**条件按顺序满足时，调用才会被提取：

| 步骤 | 条件 |
| :-- | :-- |
| 1 | `U` 之前的字符是标识符边界 — 不是 `[A-Za-z0-9_]` 或文本开头 |
| 2 | 接下来的三个字符是 `U`/`u`、`E`/`e`、`.` — 在 1.x 主体中前缀的匹配**不区分大小写**，因此 `UE.`、`ue.`、`Ue.` 和 `uE.` 都会触发 |
| 3 | `.` 之后的字符开始一个标识符：`[A-Za-z_]`，然后是 `[A-Za-z0-9_]` |
| 4 | 在可选空白之后，下一个字符是 `(` |
| 5 | 存在匹配的 `)`（搜索会跳过 `"…"` 字符串） |

如果步骤 1–4 失败，文本会**原样且静默地**复制过去。如果步骤 5 失败，主体会
[`DSH6314`](../diagnostics/DSH6xxx.md#dsh6314)。

扫描会跳过 `//` 注释、`/* */` 注释、`"…"` 字符串和 `'…'` 字符字面量，因此写在注释中的
`UE.Time()` 不会被提取。

提取出的文本 — 包括其括号在内的整个 `UE.Foo(…)` — 会被解析为一个
[Graph 表达式](../graph/expressions.md)，因此任何 [`UE.*` 内置函数](../builtins/ue.md)，包括
通用的 [`UE.Expression(…)`](../builtins/ue-expression.md)，都可用。*(自 2.0.0 起)* 它
绑定在**函数自身的作用域**中：其参数（在每次调用时被该调用的
实参替换），以及文件的文件作用域声明。只有调用者才拥有的名称是
[`DSH6325`](../diagnostics/DSH6xxx.md#dsh6325) — 1.x 在调用者的 `Graph`
作用域中求值被提取调用；请改为将值作为参数传入。没有人拥有的名称，包括主体自身声明的
变量，是 [`DSH6326`](../diagnostics/DSH6xxx.md#dsh6326)。

> [!NOTE]
> 提取恰好消耗 `UE.Name( … )` 直到匹配的 `)` — 仅此而已。尾随的 swizzle
> 或成员访问会作为应用于该引脚的 HLSL 文本保留下来，因此 `UE.CameraVector().xy` 会变为
> `_ds_<Fn>_UE0 .xy`，并由着色器编译器求值，而非由图表构建器求值。该引脚
> 以其完整组件数携带节点的默认输出。引脚名称会替换调用的文本，
> 并填充空格至调用的长度（同时保留调用内的任何换行），因此主体中
> 之后的每一列仍与源匹配。

> [!WARNING]
> 只有 `UE.` 会被提取。`Substrate.*` 调用**不会**被提取，而是以
> 文本形式到达着色器编译器，在那里它不是有效的 HLSL；*(自 2.0.0 起)* 前端会以警告
> [`DSH6316`](../diagnostics/DSH6xxx.md#dsh6316) 指出这一点。Substrate 值根本无法跨越 Custom 节点边界。
> 请在 [`ShaderFunction`](shader-function.md) 中或直接在
> `Shader` 的 [`Graph`](../graph/index.md) 中构建 Substrate 材质图表。

<a id="values-the-hoist-refuses"></a>

### 提升拒绝的值

被提取的调用必须产生 Custom 节点输入所能携带的内容：一个数字、一个布尔值，或 *(自
2.0.0 起)* 一个纹理对象。`MaterialAttributes` 值或 Substrate 材质是
[`DSH6329`](../diagnostics/DSH6xxx.md#dsh6329)。具有多个输出的节点会向该引脚馈送其默认
输出。

<a id="generated-input-pins"></a>

## 生成的输入引脚

每个被提取的调用都会向 Custom 节点添加一个输入。引脚名称会先被派生，然后去重：

| 步骤 | 规则 |
| :-- | :-- |
| 1 | 基础名称为 `__ds_<FunctionName>_UE<N>`，其中 `N` 从 0 开始按源顺序计数主体中被提取的调用 — 每个调用处使用相同的名称 |
| 2 | 整个基础名称会进行标识符净化：非 `[A-Za-z0-9_]` → `_`，然后**连续的下划线串会被合并为一个** — 这会将开头的 `__` 变为单个 `_` |
| 3 | 当名称与已声明的 `in` 参数或更早的被提取调用冲突（以不区分大小写的方式比较）时，会追加 `_1`、`_2`、… |

因此，一个主体中包含一个 `UE.Time()` 调用的 `GraphFunction WindPulse` 会生成一个字面命名为以下内容的
引脚：

```text
_ds_WindPulse_UE0
```

而带命名空间的 `Common::Pulse` 会生成 `_ds_Common_Pulse_UE0`。

> [!NOTE]
> 可见引脚名称带有**单个**前导下划线。`__ds_` 仅出现在预清理的
> 基础名称中。这些引脚名称在生成的 Custom 节点上以及任何提及它们的
> 着色器编译器错误中都是用户可见的。

声明的 `in` 参数按声明顺序、以其声明名称占据前几个引脚。
提升的引脚紧随其后。

<a id="generated-custom-node-code"></a>

## 生成的 Custom 节点代码

*(自 2.0.0 起)* 该代码具有每个 `Function` 节点都有的形态——参见
[`Function` § 生成的 HLSL](function.md#generated-hlsl)——其中每个提升的调用都被替换为其
引脚名称：

```hlsl
<Out0Type> <Out0> = (<Out0Type>)0;      // a function without a return type: its first `out`
// Begin DreamShader source: <file>
// DreamShader custom: <Name> line <N>
<body, with every lifted UE.* call replaced by its pin name>
// End DreamShader source: <file>
return <Out0>;                         // only when the body has no top-level return
```

| 元素 | 规则 |
| :-- | :-- |
| 第一个输出 | 返回的值，或第一个 `out` 的局部变量，在主体之前声明并在主体之后返回 |
| 其他 `out` 参数 | 节点的附加输出，以**声明的**参数命名 *(自 2.0.0 起；1.x 以调用者的变量命名)*。引擎声明并对它们进行零初始化；主体直接赋值，因此没有回写行。 |
| 主体 | 逐字位于源标记之间 |
| 返回 | 仅当主体没有顶层 `return` 时追加 |

因为主体就是节点的代码，`return expr;` 可在任意大括号深度返回节点的第一个输出，
而附加输出保留主体在其之前所赋的值。参见
[`Function` § 返回降低](function.md#return-lowering)。

主体中的普通 `Function` 调用会像在 `Function` 节点中一样原样嵌入。

<a id="restrictions"></a>

## 限制

| 限制 | 后果 |
| :-- | :-- |
| 不允许 `Inline` / `SelfContained` | `DSH6307` |
| 不允许递归，无论直接还是间接 | 对于相互调用的主体为 `DSH6260`；当提升调用的参数再次到达该函数时为 `DSH6330` |
| 当它提升调用时只能从 `Graph` 块调用 | 另一个主体调用它会是 `DSH6327` |
| 输出携带 `Float1`…`Float4` 或 `MaterialAttributes` | 纹理或采样器输出是 [`DSH6254`](../diagnostics/DSH6xxx.md#dsh6254) |
| 不允许 `Substrate` 输入或结果 | `DSH6253` |
| 空主体 | 无诊断；输出为 0 |

<a id="argument-validation"></a>

### 参数验证

调用形式及其参数规则与 `Function` 的相同：参见
[`Function` § 调用函数](function.md#calling-a-function)。*(自 2.0.0 起)* 接受具名参数，
对多输出函数的值调用读取其第一个输出，并且 1.x 的合成
值目标 `__ds_<Name>_value<N>` 不再存在。

<a id="notes"></a>

## 说明

- 一个*仅*包含 `GraphFunction` 块的 `.dsm` 或 `.dsf` 不会生成任何资产。
- 名称修饰拼写 `DreamShaderFn_WindPulse(…)` 不再在 `Graph` 块中命名该函数
  *(自 2.0.0 起)*：[`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208)。
- `Function` 和 `GraphFunction` 不能共享名称：[`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210)
  *(自 2.0.0 起；1.x 将调用报告为有歧义)*。
- 在 UE 5.3 上，生成的 Custom 节点在材质图中显示其代码；从 UE 5.4 起，
  `ShowCode` 被设置为 `false`。
- 每个提升的调用都会单独降低。一个主体中两个文本相同的调用会产生两个引脚，
  尽管底层材质表达式仍可能通过
  [节点重用](../graph/node-reuse.md)共享。

<a id="diagnostics"></a>

## 诊断

`Function` 的声明、绑定、代码和调用诊断均原样适用——参见
[`Function` § 诊断](function.md#diagnostics)。以下是 `GraphFunction` 新增的诊断。每个
都携带该构造的行和列；代码页面有其消息。

<a id="parse-time"></a>

### 解析时

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH6307` | `GraphFunction` 之后的 `SelfContained` 或 `Inline` |
| `DSH6314` | 主体中的 `UE.*` 调用从未闭合 |
| `DSH6316` | 主体中的 `Substrate.*` 调用未被提升（警告） |

其他解析时代码（`DSH6300`–`DSH6308`、`DSH6319`）是 `Function` 的；参见
[`Function` § 解析时](function.md#parse-time)。

<a id="binding"></a>

### 绑定

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH6325` | 提升的调用读取了只有调用者才有的名称 |
| `DSH6326` | 提升的调用读取的名称既不是参数，也未在文件作用域声明 |
| `DSH6329` | 提升的调用生成了 `MaterialAttributes` 或 Substrate 值 |
| `DSH6330` | 函数通过提升的调用到达自身 |

<a id="custom-node-code"></a>

### Custom 节点代码

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH6327` | 另一个主体调用了提升调用的函数 |
| [`DSH6328`](../diagnostics/DSH6xxx.md#dsh6328) | 提升调用所记录的位置不在主体中（内部错误） |

完整的跨阶段列表位于[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
// DShader/Lib/Motion.dsh

GraphFunction WindPulse(in float2 uv, out float pulse)
{
    float t = UE.Time();
    pulse = sin(uv.x * 8.0 + t);
}

GraphFunction float Parallax(in float2 uv, in float depth)
{
    float2 view = UE.CameraVector().xy;
    return uv.x + view.x * depth;
}
```

从 `Shader` 中使用：

```c
import "Lib/Motion.dsh"

Shader(Name="Materials/M_Wind")
{
    Properties = { float Depth = 0.25; }
    Outputs    = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = {
        vec2  UV = UE.TexCoord(Index = 0);
        float Pulse;
        WindPulse(UV, Pulse);
        float Shift = Parallax(UV, Depth);
        Color = vec3(Pulse, Shift, 0.0);
    }
}
```

为 `WindPulse` 调用生成的材质：

```text
UMaterialExpressionTextureCoordinate       →  Custom pin "uv"
UMaterialExpressionTime                    →  Custom pin "_ds_WindPulse_UE0"
UMaterialExpressionCustom  Description="WindPulse"  OutputType=CMOT_Float1
```

其中 `Code`：

```hlsl
float pulse = (float)0;
// Begin DreamShader source: DShader/Lib/Motion.dsh
// DreamShader custom: WindPulse line 4

    float t = _ds_WindPulse_UE0;
    pulse = sin(uv.x * 8.0 + t);
// End DreamShader source: DShader/Lib/Motion.dsh
return pulse;
```

<a id="see-also"></a>

## 另见

- [Function](function.md) —— 共享的声明语法、参数、返回类型、节点代码和主体规范化
- [Namespace](namespace.md) —— `Ns::Fn` 限定和扁平化名称
- [调用函数](../graph/calls.md) —— 值调用与语句调用
- [2.0 语言](../language-v2/index.md) —— `/// @custom` 主体及其提升的 `UE.` 调用
- [UE 内置](../builtins/ue.md) —— 提升器可求值的每个 `UE.*` 调用
- [UE.Expression](../builtins/ue-expression.md) —— 通用反射节点内置
- [Substrate 内置](../builtins/substrate.md) —— 为何 `Substrate.*` 无法提升
- [Graph](../graph/index.md) —— `Graph` 块的语句和表达式语言
- [节点重用](../graph/node-reuse.md) —— 两个相同的提升表达式何时共享一个节点
- [类型令牌](types.md) —— 完整的令牌目录
- [ShaderFunction](shader-function.md) —— 当需要真实节点图或 Substrate 时的替代方案
- [诊断索引](../diagnostics/index.md) —— 每个代码，按阶段
