---
title: "输入、输出与结果"
description: "输入、输出与结果"
---

<a id="inputs-outputs-and-results"></a>

# 输入、输出与结果

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **输入 / 输出 / 结果**

声明块的类型化参数的各个节 —— 生成的
`UMaterialFunction` 的输入和输出引脚，或 `Shader` 的已声明输出变量。

| | |
| :-- | :-- |
| 声明位置 | `.dsm`、`.dsf`、`.dsh` —— 位于 `ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend`、`VirtualFunction` 内；`Outputs` 也位于 `Shader` 内 |
| 种类 | 节 |
| 生成 | `UMaterialExpressionFunctionInput` / `UMaterialExpressionFunctionOutput`（材质函数块）；对 `Shader` 不直接生成任何内容 |

<a id="synopsis"></a>

## 概要

```c
Inputs  [=] { <parameter-declaration>… }
Outputs [=] { <parameter-declaration>… }
Results [=] { <parameter-declaration>… }
```

```c
parameter-declaration := [ opt ] <type> <name> [ = <default-expression> ] [ [ <metadata> ] ] ;
```

围绕 `<metadata>` 的最内层 `[ … ]` 对是 **DreamShaderLang 的字面标点**；外层
对是“可选”的元语法。`;` 可以在 `}` 之前的最后一个声明中省略。

在 `Shader` 内，`Outputs` 使用一种不同的语法，该语法还携带绑定语句：

```c
Outputs [=]
{
    <output-declaration>…
    <output-binding>…
}
```

```c
output-declaration := <type> <name> [ = <expression> ] ;
```

输出绑定在 [输出绑定](output-bindings.md) 中说明。

<a id="section-keywords-per-block"></a>

## 每个块的节关键字

| 块 | `Inputs` | `Outputs` | `Results` | `Properties` |
| :-- | :-- | :-- | :-- | :-- |
| `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` | 类型化参数 | 类型化参数 | **`Outputs` 的别名** | 参数节点 —— 见 [属性](properties.md) |
| `VirtualFunction` | 类型化参数 | 类型化参数 | **`Outputs` 的别名** | **`Inputs` 的别名** |
| `Shader` | 未知节（[`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245)） | 声明 + 绑定（自有语法） | 未知节（`DSH2245`） | 参数节点 |

`Results` 是纯粹的同义词：它追加到与 `Outputs` 相同的列表中。节名称
不区分大小写地匹配，可以以任意顺序出现，重复的节会**追加**到前一个节。
`{ … }` 块之前的 `=` 是可选的*（自 1.5.0 起）*。

> [!WARNING]
> 在 `VirtualFunction` 内，`Properties` 是 **`Inputs` 的别名**，因此使用
> 本页的语法，而不是 [`Properties`](properties.md) 声明语法。在那里将
> `Properties` 解读为“参数节点”是节语法中最常见的混淆来源。

<a id="declaration-members"></a>

## 声明成员

| 成员 | 必需 | 形式 | 描述 |
| :-- | :-- | :-- | :-- |
| `opt` | 否 | 关键字 | 在 Unreal 中将该输入标记为可选（`bUsePreviewValueAsDefault`）。*（自 1.2.3 起）* |
| **`<type>`** | 是 | 一个词 | 见 [接受的类型](#accepted-types)。 |
| **`<name>`** | 是 | 标识符 | 成为引脚名称。 |
| `= <default-expression>` | 否 | 表达式 | 输入的默认值，这也会使其变为可选*（自 2.0.0 起）*。对输出无意义 —— 见 [输出默认值](#output-defaults)。 |
| `[ <metadata> ]` | 否 | 块 | 描述和排序顺序。见 [各参数元数据](#per-parameter-metadata)。 |

<a id="parse-order"></a>

### 解析顺序

*（自 2.0.0 起）* 该节按词逐一读取声明：

| 步骤 | 操作 |
| :-- | :-- |
| 1 | 当后面还有两个词时，取前导的 `opt`（任意大小写） |
| 2 | 接下来的两个词是类型和名称 |
| 3 | `=` 开始默认值，该默认值一直延伸到 `;` 或 `[` |
| 4 | `[ … ]` 块是元数据 |
| 5 | `;`（或该节的 `}`）结束该声明 |

其他任何内容都是 [`DSH3271`](../diagnostics/DSH3xxx.md#dsh3271)，解析器会在下一个
`;` 处恢复。`=` 之后无内容的默认值也是 `DSH3271`。

<a id="opt"></a>

### `opt`

`opt` 不区分大小写地匹配。它是标记引脚为可选的关键，并且*（自 2.0.0 起）*默认值
也会如此（旧规则 L7）：`<type> <name> = <expr>;` 是可选输入，而 1.x 将其保持为必需，
仅有一个预览值。`opt` 用在输出上没有任何意义，并且是一个警告
（[`DSH3272`](../diagnostics/DSH3xxx.md#dsh3272)）。

<a id="whitespace"></a>

### 空白

*（自 2.0.0 起）* 空白仅用于分隔词，在这些节中与 [`Properties`](properties.md) 中一样：
`opt`、类型和名称之间的制表符与空格一样好。1.x 在最后一个字面
空格处分割 —— 因此制表符会使类型标记变为 `opt\tfloat` —— 这种做法已不存在。类型和名称
各为一个词：`Material Attributes X;` 或 `float 4 Colour;` 是 `DSH3271`。

> [!NOTE]
> `in` 和 `out` 在这些节中**不是**限定符。它们仅存在于 HLSL
> [`Function`](function.md) / [`GraphFunction`](graph-function.md) 签名形式上，那是
> 一种不同的语法。写 `Inputs = { in float X; }` 是 `DSH3271`：`in` 是关键字，不是类型。

<a id="accepted-types"></a>

## 接受的类型

在文件绑定时解析。标记不区分大小写地匹配。

| 标记 | `EFunctionInputType` | 分量 |
| :-- | :-- | :-- |
| `float` `float1` `half` `half1` `int` `uint` `bool` | `FunctionInput_Scalar` | 1 |
| `float2` `half2` `vec2` `int2` `uint2` `bool2` `ivec2` `uvec2` `bvec2` | `FunctionInput_Vector2` | 2 |
| `float3` `half3` `vec3` `int3` `uint3` `bool3` `ivec3` `uvec3` `bvec3` | `FunctionInput_Vector3` | 3 |
| `float4` `half4` `vec4` `int4` `uint4` `bool4` `ivec4` `uvec4` `bvec4` | `FunctionInput_Vector4` | 4 |
| `StaticBool` `StaticBoolParameter` | 在输入上为 `FunctionInput_StaticBool`；在输出上读取为 `bool` | 1 *（自 1.6.0 起 —— 之前在调用点不是单分量类型）* |
| `MaterialAttributes` | `FunctionInput_MaterialAttributes` | — |
| `Substrate` | `FunctionInput_Substrate` | — |
| `Texture2D` | `FunctionInput_Texture2D` | — |
| `TextureCube` | `FunctionInput_TextureCube` | — |
| `Texture2DArray` | `FunctionInput_Texture2DArray` | — |
| `Texture3D` `VolumeTexture` | `FunctionInput_VolumeTexture` | — |

不表示任何类型的词是 [`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201)。`SamplerState` 是一种
type，但没有任何函数引脚携带采样器：[`DSH4364`](../diagnostics/DSH4xxx.md#dsh4364)
*（自 2.0.0 起；1.x 将其作为 `Texture2D` 输入）*。引脚类型在正在运行的引擎自身的
列表中查找；引擎没有的类型是 [`DSH8228`](../diagnostics/DSH8xxx.md#dsh8228)。

同一名称的两个输入或输出是 [`DSH4214`](../diagnostics/DSH4xxx.md#dsh4214)。

<a id="input-defaults"></a>

## 输入默认值

输入的默认值是调用方未连接该引脚时其采用的值；它使该输入变为可选*（自 2.0.0 起）*。

| 情况 | 行为 |
| :-- | :-- |
| 默认值折叠为常量 | 写入 `PreviewValue` |
| 其他任何内容 | 构建为**图表表达式**并连接到输入的 `Preview` 引脚 |

图表达式路径正是让默认值能够引用代码块自身生成的节点的方式，
包括在 [`Properties`](properties.md) 中声明的 `const` 或参数节点 *(自 1.2.6 起)*：

```c
ShaderFunction(Name="Functions/F_Sample")
{
    Properties = {
        const Texture2D PreviewTex = Path(Engine, "EngineResources/DefaultTexture");
    }
    Inputs = {
        opt Texture2D Tex = PreviewTex;      // preview graph, not a literal
        opt float     Mix = 0.5;             // constant → PreviewValue
    }
    Outputs = { vec4 OutColor; }
    Graph   = { OutColor = Tex(Coordinates = UE.TexCoord(Index = 0)) * Mix; }
}
```

默认值会像赋值一样转换为输入的类型：不适合的值会被
[`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226)，而更宽的向量会被裁剪到其前导
分量，使用 [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289)。

> [!NOTE]
> *(自 1.6.0 起)* `true` 和 `false` 是图字面量。`StaticBool` 输入的默认值会成为 `Preview` 引脚上的
> `StaticBool` 节点——引擎会忽略静态布尔输入的 `PreviewValue`——
> 因此 `opt StaticBool Flag = false;` 会生成 Unreal 所需的节点。

<a id="output-defaults"></a>

## 输出默认值

> [!WARNING]
> 材质函数的 `Outputs` / `Results` 条目上的 `= <expression>` 会被**丢弃**，并产生一条
> 警告（[`DSH3273`](../diagnostics/DSH3xxx.md#dsh3273)）。请改为在 `Graph` 中写入该值。
> 这不适用于 `Shader` 的 `Outputs`，在那里初始化器是有意义的 *(自 1.3.4 起)*。

除第一个名为 `Result` 的输出之外，每个声明的输出都必须在 `Graph` 中的某处被赋值
（[`DSH6211`](../diagnostics/DSH6xxx.md#dsh6211)）；第一个 `Result` 是函数的返回值，
并从零开始。已赋的值若不适合则为 `DSH4226`（更宽的向量：`DSH5289`）。

材质函数必须声明至少一个输出
（[`DSH4315`](../diagnostics/DSH4xxx.md#dsh4315)）；`VirtualFunction` 也必须如此
（[`DSH6313`](../diagnostics/DSH6xxx.md#dsh6313)）。

<a id="per-parameter-metadata"></a>

## 每个参数的元数据

输入或输出末尾的 `[ … ]` 块由与
[`Properties` 元数据](../parameters/metadata.md)相同的代码读取，但只会消费以下键：

| 元数据键 | 别名 | 效果 |
| :-- | :-- | :-- |
| `Description` | `Desc`, `Tooltip` | `UMaterialExpressionFunctionInput` / `…FunctionOutput` `Description` |
| `SortPriority` | `Sort` | 对区段的引脚进行排序：它们会按此稳定排序，没有该键的引脚按其声明索引计算，资产会按该顺序获得密集优先级 `0, 1, 2, …` *(自 2.0.0 起)*。不是整数：[`DSH3258`](../diagnostics/DSH3xxx.md#dsh3258)。 |
| `Group` | `Category` | 被忽略——引擎的函数输入/输出节点没有分组字段 |
| *任何其他键* | — | **被静默忽略**——函数输入和输出不会运行反射属性处理 |

> [!WARNING]
> 上述键以外的元数据键会被解析器接受并在没有
> 诊断的情况下被丢弃。`[SamplerType="LinearColor"]` 作用于 `Inputs` 条目时不起任何作用；该键只对
> [`Properties`](properties.md) 声明有效果。

格式错误的块——没有 `]`、键缺少 `=` 或值——会
[`DSH3255`](../diagnostics/DSH3xxx.md#dsh3255)；同一个键写两次会
[`DSH3256`](../diagnostics/DSH3xxx.md#dsh3256)。

引脚 GUID 会按名称在重新生成之间缓存，这就是为什么重新生成 `ShaderFunction` 不会
破坏现有的调用点。

<a id="materialattributes-outputs"></a>

## `MaterialAttributes` 输出

`MaterialAttributes` 输出以空材质开始，因此诸如
`Attrs.BaseColor = …` 的成员写入有一个目标。层的输出则改为以传入的材质开始；
参见 [ShaderLayer / ShaderLayerBlend](shader-layer.md#interface-rules)。对于 `Shader` 的
`MaterialAttributes` 输出声明也是如此。参见 [MaterialAttributes](../graph/material-attributes.md)。

<a id="layer-and-blend-rules"></a>

## 层与混合规则

`ShaderLayer` 和 `ShaderLayerBlend` 进一步约束这些区段：

| 类型 | 输入 | 输出 |
| :-- | :-- | :-- |
| `ShaderFunction` | 无约束 | ≥ 1 |
| `ShaderLayer` | 无，或一个 `MaterialAttributes`——无论哪种都是层的材质输入 | 恰好一个，`MaterialAttributes` |
| `ShaderLayerBlend` | 至少一个 `MaterialAttributes` | 恰好一个，`MaterialAttributes` |

层控制项应放在 [`Properties`](properties.md) 中，而不是 `Inputs` 中。完整规则及其代码——
包括 `BlendInputRelevance` 名称匹配 *(自 UE 5.7 起)*——见
[ShaderLayer / ShaderLayerBlend](shader-layer.md)。

<a id="outputs-in-a-shader"></a>

## `Outputs` 中的 `Shader`

`Shader` 的 `Outputs` 区段从同一个主体填充**两个**列表：输出变量声明和
输出绑定。每条语句按其开头的词进行分类：

| 语句形式 | 分类为 |
| :-- | :-- |
| `<type> <name> ;` | 输出变量声明；它从零开始，与 1.x 中一样 |
| `<type> <name> = <expr> ;` | **已初始化**的输出声明 *(自 1.3.4 起)* |
| `Base.<target> = <source> ;`, `Expression( … ).Pin[<i>] = <source> ;`, `Expression( … ) { … }` | 输出绑定 |
| 其他任何内容 | [`DSH3266`](../diagnostics/DSH3xxx.md#dsh3266) |

声明的语法是上面的类型化声明形式，不带 `opt` 且不带元数据。
`Shader` 的 `Outputs` 语句上的 `[ … ]` 块为 `DSH3266`。后面没有任何内容的 `=` 为
[`DSH3270`](../diagnostics/DSH3xxx.md#dsh3270)。

仅适用于 `Shader` 的规则：

- `return` 是关键字 *(自 2.0.0 起)*：它不能命名输出声明（`DSH3266`），也不是
  绑定源。参见 [输出绑定](output-bindings.md#the-reserved-name-return)。
- 同一个输出名声明两次为 [`DSH4220`](../diagnostics/DSH4xxx.md#dsh4220)；不是类型的类型为 `DSH4201`。
- `Shader` 何时需要 `Graph`，以及没有 `Outputs` 区段的 `Shader` 会做什么，见
  [Shader](shader.md#the-outputs--graph-relationship)。

完整的绑定目录、验证规则以及所有绑定诊断见
[输出绑定](output-bindings.md)。

<a id="notes"></a>

## 说明

- 凡是空白可以出现的地方都会跳过注释。
- `Options` 和 `Settings` 是这些区段对应的 `VirtualFunction`；参见
  [选项](options.md)。
- `VirtualFunction` 声明一个已存在的资产，自身从不生成引脚——其 `Inputs` /
  `Outputs` 描述该资产的接口，以便 `Graph` 调用可以进行类型检查。参见
  [VirtualFunction](virtual-function.md)。

<a id="diagnostics"></a>

## 诊断

每个代码都带有声明的行和列；该代码的页面中有消息内容。

<a id="parse-time"></a>

### 解析时

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH3271` | 区段未以 `{` 开始；声明不是 `[opt] <type> <name> [= <default>] [[ … ]] ;`；默认值为空 |
| `DSH3272` | 输出上的 `opt`（警告；被忽略） |
| `DSH3273` | 输出上的默认值（警告；被丢弃） |
| `DSH3255`, `DSH3256`, [`DSH3257`](../diagnostics/DSH3xxx.md#dsh3257), `DSH3258` | 格式错误的元数据块、同一个键写两次、格式错误的 `Slider(…)`、不是整数的 `SortPriority` |
| `DSH2245` | 该代码块没有的区段名称 |
| [`DSH2247`](../diagnostics/DSH2xxx.md#dsh2247) | `VirtualFunction` 中的 `Graph` 或 `Code` |
| `DSH6313` | 没有 `Outputs` / `Results` 条目的 `VirtualFunction` |
| `DSH3266`, `DSH3270` | `Shader` `Outputs`：没有已知形式的语句 / 后面没有任何内容的 `=` |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | 文件在区段内部结束 |

<a id="binding-and-ir"></a>

### 绑定与 IR

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH4201` | 类型令牌未命名任何类型 |
| `DSH4214` | 两个参数共享同一名称 |
| `DSH4220` | `Shader` `Outputs`：同一名称被声明两次 |
| `DSH4226`, `DSH5289` | 默认值或指定值不匹配 / 较宽的向量被截断（信息） |
| `DSH6211` | `Graph` 从未指定输出 |
| `DSH4315` | 没有输出的材质函数 |
| `DSH4364` | 没有函数引脚承载的类型 |

<a id="emit-time"></a>

### 发射时

| 代码 | 触发条件 |
| :-- | :-- |
| `DSH8228` | 引擎没有该名称的函数输入类型 |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | 无法创建 `FunctionInput` / `FunctionOutput` 节点 |

完整的跨阶段列表见[诊断索引](../diagnostics/index.md)。

<a id="example"></a>

## 示例

```c
ShaderFunction(Name="Functions/F_Opt")
{
    Inputs = {
        vec3 InColor;
        opt float Strength = 1.0 [
            Description="Preview strength";
            SortPriority=10;
        ];
    }

    Results = {                       // alias for Outputs
        vec3 OutColor [Description="Tinted result"];
    }

    Settings = {
        Description     = "Strength helper";
    }

    Graph = {
        OutColor = InColor * Strength;
    }
}
```

生成的资产接口：

```text
UMaterialFunction  /Game/Functions/F_Opt.F_Opt
  FunctionInput   InColor    Vector3   required, SortPriority 0
  FunctionInput   Strength   Scalar    optional, PreviewValue 1.0, SortPriority 1
  FunctionOutput  OutColor   Vector3   SortPriority 0
```

<a id="see-also"></a>

## 另请参阅

- [属性](properties.md) — 参数/常量声明部分
- [ShaderFunction](shader-function.md) — 这些部分所描述的代码块
- [ShaderLayer / ShaderLayerBlend](shader-layer.md) — 这些部分上的约束
- [VirtualFunction](virtual-function.md) — `Properties` 在此别名 `Inputs`
- [选项](options.md) — `VirtualFunction` `Asset` 部分
- [输出绑定](output-bindings.md) — `Base.*` 和 `Expression(…).Pin[i]` 绑定目录
- [Shader](shader.md) — `Outputs` 承载绑定的代码块
- [类型](types.md) — 完整的类型令牌目录及按上下文有效性矩阵
- [元数据](../parameters/metadata.md) — `[ … ]` 代码块语法
- [调用](../graph/calls.md) — 从 `Graph` 调用 `ShaderFunction` 或 `VirtualFunction`
- [MaterialAttributes](../graph/material-attributes.md) — 成员写入与 Substrate 互操作
- [函数设置](../../settings/function.md) — 函数的 `Settings` 所遵循的键
- [诊断索引](../diagnostics/index.md) — 按阶段列出的每个代码
