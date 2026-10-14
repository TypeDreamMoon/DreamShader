---
title: 类型与值
description: 52 个类型 token、分上下文的有效性矩阵、GLSL 别名、字面量，以及已经不存在的那些 token。
---

类型 token 是语言里每一种声明形式的第一个元素。这个集合是封闭的 —— 一共 52 个 token，没有用户自定义
类型 —— 而且在**所有上下文中都按大小写不敏感匹配**。

所有类型 token 都过同一张表 （自 2.0.0 起）：1.x 拼法读成对应的 2.0 类型（`vec3` 和 `Float3` 读成
`float3`，`MaterialAttributes` 读成 `material`，`StaticBool` 读成 `bool`），不给诊断；不是类型的单词原样保留，
由绑定器拒绝（`DSH4201`）—— 在 `Properties` 里前端先拒绝（`DSH3252`）。2.0 的类型名见
[DreamShaderLang 2.0](../language-v2/index.md)。

```c
// Properties section
[const] <type> <name> [ = <default> ] [ [ <metadata> ] ] ;

// 材质函数的 Inputs / Outputs / Results
[opt] <type> <name> [ = <default> ] [ [ <metadata> ] ] ;

// Shader Outputs 变量声明
<type> <name> [ = <expression> ] ;

// Function / GraphFunction 签名
Function [ <type> ] <name> ( [ { in | out } ] <type> <name> , … ) { … }

// Graph 声明
<type> <name> [ = { <expression> | { <brace-initializer> } } ] ;
```

| 记号 | 含义 | 示例 |
| --- | --- | --- |
| `<x>` | 占位符——替换成实际内容，尖括号本身不写出来。 | `Name = <string>` |
| `[ x ]` | 可选——整段可以整体省略。 | `[, Root = <string>]` |
| `{ a \| b }` | 多选一——从竖线分隔的写法里取其中一个。 | `{ Node( … ) \| Comment( … ) }` |
| `…` | 可重复——前一项可以出现任意多次。 | `<property-declaration> …` |

<a id="token-家族"></a>

## Token 家族

| 家族 | Token | 数量 |
| :-- | :-- | --: |
| 标量（1 分量） | `float` `half` `double` `int` `uint` `bool`，以及各自带 `1` 后缀的写法（`float1`…） | 12 |
| 向量（2 / 3 / 4 分量） | `float2..4` `half2..4` `double2..4` `int2..4` `uint2..4` `bool2..4` `vec2..4` `ivec2..4` `uvec2..4` `bvec2..4` | 30 |
| 纹理 | `Texture2D` `TextureCube` `Texture2DArray` `Texture3D` `VolumeTexture` | 5 |
| 不透明 / 其他 | `SamplerState` `MaterialAttributes` `Substrate` `StaticBool` `StaticBoolParameter` | 5 |

**提示：**

没有 `vec1`、`ivec1`、`uvec1`、`bvec1`，也没有 `half` 的 GLSL 向量拼法。单分量的 GLSL 风格写法根本
不存在 —— 用 `float` 或 `float1`。token 的宽度就是它的数字后缀：`…2` 到 `…4` 是那么多分量的向量，`…1` 或
没有数字是标量，`…NxM` 是矩阵。

<a id="六个上下文"></a>

## 六个上下文

各个声明位置**并不共用**同一个类型集合。下面矩阵的每一列对应一个位置。

| 列 | 位置 |
| :-- | :-- |
| `Prop` | `Shader` / `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` 的 `Properties` |
| `I/O` | 材质函数或 `VirtualFunction` 的 `Inputs` / `Outputs` / `Results`（以及 `VirtualFunction` 里作为 `Inputs` 别名的 `Properties`） |
| `Fn in` | `Function` / `GraphFunction` 的 `in` 参数 |
| `Fn out` | `Function` / `GraphFunction` 的 `out` 参数，或声明的返回类型 |
| `Out decl` | `Shader` 的 `Outputs` section 里的变量声明 |
| `Graph` | `Graph` 块里的声明语句 |

**提示：**

`Prop` 由前端在读 section 时检查。其他各列由绑定器对整个文件检查，再由降级到图的那一步在某个类型的值要变成
引脚的地方检查 —— 没有引脚能承载的类型，在需要那个引脚的声明或调用处被拒绝 （自 2.0.0 起）。1.x 只在函数
被调用时才校验 `Function` 的参数类型；现在不是类型的 token 在声明处就是 `DSH4201`。

<a id="有效性矩阵"></a>

## 有效性矩阵

`✔` 接受 · `✘` 拒绝，单元格里写的是拒绝时的诊断码。

| Token | `Prop` | `I/O` | `Fn in` | `Fn out` | `Out decl` | `Graph` |
| :-- | :-: | :-: | :-: | :-: | :-: | :-: |
| 标量和向量：`float`、`half`、`int`、`uint`、`bool` 家族及其 GLSL 拼法 | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| `Texture2D` `TextureCube` `Texture2DArray` `Texture3D` `VolumeTexture` | ✔ | ✔ ⁽¹⁾ | ✔ ⁽²⁾ | ✘ `DSH6254` | ✔ ⁽³⁾ | ✔ ⁽⁴⁾ |
| `MaterialAttributes` | ✘ `DSH3252` | ✔ | ✘ `DSH6210` | ✔ | ✔ | ✔ |
| `Substrate` | ✘ `DSH3252` | ✔ ⁽⁵⁾ | ✘ `DSH6253` | ✘ `DSH6253` | ✔ ⁽³⁾ | ✔ ⁽⁴⁾ |
| `StaticBool` | ✔ ⁽⁶⁾ | ✔ ⁽⁷⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ |
| `StaticBoolParameter` | ✔ ⁽⁹⁾ | ✔ ⁽⁷⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ | ✔ ⁽⁸⁾ |
| `mat2` `mat3` `mat4`、`float2x2` … `float4x4` | ✘ `DSH3252` | ✘ | ✘ | ✘ | ✘ | ✘ —— 见 [矩阵](#矩阵) |

1. `Texture3D` 输入是一个 `VolumeTexture` 引脚。
2. `Function` 的纹理类型 `in` 参数在生成的 HLSL 里还会配一个 `SamplerState <Name>Sampler` —— 引擎在 `Custom`
   节点的每个纹理输入旁都会声明它。见 [函数](functions.md)。
3. 只能是变量：没有 `Base.` 引脚接受纹理，`Substrate` 只能接到 `Base.FrontMaterial`。它没有零值，所以 `Graph`
   必须在读取前给它赋值。
4. `Graph` 块里的纹理、`SamplerState` 和 `Substrate` 声明**必须**带初始化式 —— 它们没有默认值：`DSH2215`。
5. `Substrate` 输入是函数的 Substrate 引脚；Substrate 值来自 `Substrate.*` 节点，引擎从 （自 UE 5.4 起）
   起才有。
6. 读作 `bool` （自 2.0.0 起），所以 `Properties` 里的 `StaticBool X = true;` 声明的是**动态**布尔参数而不是
   静态的；1.x 在那里拒绝这个 token。静态开关参数请写 `StaticBoolParameter`。
7. 这两个 token 的输入都是函数的 `StaticBool` 引脚。
8. 读作 `bool`。
9. 在 `Properties` 里，`StaticBoolParameter` 是**参数节点 token**，不是类型 token：它生成一个
   `UMaterialExpressionStaticBoolParameter`，默认值只接受 `true` / `false`（否则 `DSH3254`）。见
   [Properties 类型](properties.md)。

**警告：**

`SamplerState` 现在是独立的类型 （自 2.0.0 起）；1.x 在接受它的地方都把它读成 `Texture2D`。它在
`Properties` 里被拒绝（`DSH3252`），作为材质函数输入被拒绝（`DSH4364` —— 纹理输入自带采样器），作为 `Function`
结果被拒绝（`DSH6254`）；`Graph` 里声明它需要初始化式（`DSH2215`）。

<a id="glsl-别名"></a>

## GLSL 别名

`vec` / `ivec` / `uvec` / `bvec` 拼法在其 `float` / `int` / `uint` / `bool` 对应写法被接受的每个位置都是
一等 token —— `Properties`、`Inputs`、`Outputs`、`Graph` 声明和 `Graph` 构造器里都是 —— 并读成那个对应写法。

| GLSL 拼法 | 等价于 |
| :-- | :-- |
| `vec2` `vec3` `vec4` | `float2` `float3` `float4` |
| `ivec2` `ivec3` `ivec4` | `int2` `int3` `int4` |
| `uvec2` `uvec3` `uvec4` | `uint2` `uint3` `uint4` |
| `bvec2` `bvec3` `bvec4` | `bool2` `bool3` `bool4` |

图里每个值都是浮点，所以在 `int3`、`ivec3`、`bool3` 和 `float3` 之间选哪个基本只是文档意义。可观测的差别在
除法上：材质图没有整数除法，两侧都是整数构造器调用（`int(7) / int(2)`）的 `/` 是 `DSH4243`；1.x `Graph` 里的
其他 `/` 都是浮点除法，`7 / 2` 是 `3.5` （自 2.1.1 起）（2.0.0 – 2.1.0 拒绝任何两个整数的除法）。见
[运算符与转换](../graph/expressions.md)。

<a id="function-函数体内的改写"></a>

### `Function` 函数体内的改写

`Function` / `GraphFunction` 的**签名**按上面的表读每个类型 token，**函数体**则会把每个标识符过同一张映射表的
超集。两者都按小写化的整个标识符匹配。

| 被改写的标识符 | 变成 | 签名类型中 | 函数体文本中 |
| :-- | :-- | :-: | :-: |
| `vec2` `vec3` `vec4` | `float2` `float3` `float4` | ✔ | ✔ |
| `ivec2` `ivec3` `ivec4` | `int2` `int3` `int4` | ✔ | ✔ |
| `uvec2` `uvec3` `uvec4` | `uint2` `uint3` `uint4` | ✔ | ✔ |
| `bvec2` `bvec3` `bvec4` | `bool2` `bool3` `bool4` | ✔ | ✔ |
| `mat2` | `float2x2` | ✔ | ✔ |
| `mat3` | `float3x3` | ✔ | ✔ |
| `mat4` | `float4x4` | ✔ | ✔ |
| `mix` | `lerp` | ✘ | ✔ |
| `fract` | `frac` | ✘ | ✔ |
| `mod` | `fmod` | ✘ | ✔ |

签名 15 个别名，函数体 18 个。改写感知注释和字符串，而且**只**作用于 `Function` 和 `GraphFunction` 的
函数体 —— `Graph` 块、`Shader` 和 `ShaderFunction` 的函数体都不改写。`Graph` 块里的 `mix`、`fract`、`mod` 调用
`lerp`、`frac`、`fmod`，并给出警告 `DSH5277` （自 2.0.0 起）；见 [数学内置](../builtins/math.md)。

**警告：**

函数体改写按整个标识符匹配且忽略大小写。`Function` 或 `GraphFunction` 函数体里名为 `Mix`、`Mod`、
`Fract`、`Vec3`、`Mat4` 的 helper、局部变量或结构体成员，会被静默改名成 `lerp`、`fmod`、`frac`、
`float3`、`float4x4`。没有任何诊断（`DSH5277` 只针对 `Graph` 文本）；失败表现为 HLSL 编译错误，或者悄悄算错。
请改名。

<a id="矩阵"></a>

## 矩阵

**图里没有矩阵值。** `mat2` / `mat3` / `mat4` 和 `float2x2` / `float3x3` / `float4x4` 在 `Properties` 里被拒绝
（`DSH3252`），在其他任何地方，一个矩阵类型的值要变成节点或引脚时就是 `DSH4361`。

不过 `mat2` / `mat3` / `mat4` 在 `Function` 签名里*词法上*是被接受的，读成 `float2x2` … `float4x4`。于是声明能
解析通过，调用时才失败：

```c
Function float3 Rotate(in mat3 basis, in vec3 v) { return mul(basis, v); }
// 能解析；Graph 调用给 basis 传矩阵时是 DSH4361
```

矩阵形状的运算请用 transform 内置（见 [UE.* 节点](../builtins/ue.md)），或者整段写在 HLSL 函数体里。

<a id="字面量"></a>

## 字面量

<a id="标量"></a>

### 标量

```c
float a = 1.0;
float b = -0.5;
float c = 1e-3;
```

数字在任何地方都是真正的 token，可带指数和一个类型后缀（`f F h H` 是浮点，`u U l L` 是整数），后缀属于 token
文本：`0.55f` 就是 `0.55`。声明默认值按消费者的要求读这个 token；`float Strength = 1abc;` 现在是 `DSH2105`
加 `DSH3254`，不再静默变成 `1.0` （自 2.0.0 起）。见 [词法与大小写](lexical.md#数字字面量)。

<a id="向量构造"></a>

### 向量构造

```c
vec2   uv    = vec2(0.5, 0.5);
vec3   color = vec3(1.0, 0.2, 0.1);
float4 rgba  = float4(color, 1.0);
```

构造器的分量数取自名字的数字后缀。只要各部分加起来等于目标宽度，构造器接受标量和向量的任意组合。

<a id="brace-initializer"></a>

### Brace initializer

```c
vec3   color = {1.0, 0.2, 0.1};
float4 rgba  = {color, 1.0};
```

声明的初始化式写成花括号列表时，会被改写成声明类型的构造器调用，适合把已有的标量或向量拼成目标类型。

<a id="布尔与字符串"></a>

### 布尔与字符串

`true` 和 `false` 在声明默认值里被接受，需要标量的地方会转成 `1.0` / `0.0`。字符串字面量用于 settings
值、元数据、类名和 `UE.*` 的命名参数；它们**不是**表达式能参与运算的值。

```c
Settings = { Domain = "UI"; }

float pulse = UE.Expression(Class="Sine", OutputType="float1", Input=UE.Time());
```

<a id="资产引用"></a>

## 资产引用

纹理和资产类声明写 `Path( … )` 引用，而不是字面量：

```c
Properties = {
    Texture2D   MainTex     = Path(Game, "Textures/T_Main");
    Texture2D   PluginTex   = Path(Plugins.MoonToon, "Textures/T_Plugin");
    Texture2D   DefaultTex  = Path("/Engine/EngineResources/DefaultTexture");
    TextureCube SkyTex      = Path(Engine, "EngineResources/DefaultTextureCube");
    Texture2D   Pasted      = Texture2D'/Game/Textures/T_X.T_X';   // Copy Reference，1.9.0 起
}
```

每一种被接受的写法、两套解析器及其全部诊断见 [Path 资产引用](../parameters/path.md)。

<a id="token-内部的空白"></a>

## Token 内部的空白

类型 token 是一个单词 （自 2.0.0 起）。1.x 在部分上下文里会去掉 token 内部的空格，于是 `float 3` 读成
`float3`、`Material Attributes` 读成 `MaterialAttributes`；现在它们是两个单词，这条声明解析不了。

<a id="生成的-hlsl-拼写"></a>

## 生成的 HLSL 拼写

`Function` 降级成 HLSL 时，每个声明的类型按 2.0 拼法写进签名：

| 声明的 token | 产出的 HLSL |
| :-- | :-- |
| `VolumeTexture` | `Texture3D` |
| `MaterialAttributes` | `FMaterialAttributes` —— 只能作为结果；作为输入是 `DSH6210` |
| `StaticBool`、`StaticBoolParameter` | `bool` （自 2.0.0 起） |
| GLSL 拼法或其他大小写（`vec3`、`Float3`） | 2.0 拼法（`float3`） |
| 其他一切 | 原样输出 |

1.x 把 `MaterialAttributes`、`StaticBool`、`StaticBoolParameter` 原样写进签名，产出 Unreal 编译不了的 helper；
现在不会了 （自 2.0.0 起）。

<a id="已移除的-token"></a>

## 已移除的 token

| Token | 状态 | 替代 |
| :-- | :-- | :-- |
| `Scalar` | **已移除** | `float` |
| `Color` | **已移除** | `float4` / `vec4` |
| `Vector` | **已移除** | `float2` … `float4` / `vec2` … `vec4` |

它们没有在任何地方被特殊处理，和其他不是类型的单词一样被拒绝 —— `Properties` 里是 `DSH3252`，其他地方是
`DSH4201`。

<a id="示例"></a>

## 示例

每个家族各取一个 token，各自放在接受它的上下文里：

```c
ShaderFunction(Name="Functions/F_Types")
{
    Properties = {
        float     Strength = 1.0;              // 标量 token
        vec3      Tint     = vec3(1, 1, 1);    // GLSL 向量拼法
        Texture2D Noise    = Path(Game, "Textures/T_Noise");
    }

    Inputs = {
        vec2                   UV;             // 数值
        opt StaticBool         UseTint;        // StaticBool 引脚
        opt MaterialAttributes InAttrs;        // MaterialAttributes 引脚
    }

    Outputs = {
        vec3 OutColor;
    }

    Graph = {
        Texture2D Src = Noise;                 // 纹理声明必须带初始化式
        vec4   Sampled = SampleTexture2D(Src, UV);
        float3 Base    = Sampled.rgb * Strength;
        OutColor       = Base * Tint;
    }
}
```

<a id="诊断"></a>

## 诊断

| 消息 | 原因 | 处理 |
| --- | --- | --- |
| `DSH3252: Expected a property type such as 'float', 'float4', 'Texture2D' or 'ScalarParameter', found '{0}'.` | Properties 不接受这个 token —— MaterialAttributes、Substrate、SamplerState、矩阵和已移除的 Scalar / Color / Vector 都在其中。 |  [详解](sections.md) |
| `DSH3253: Expected a property type with a 2.0 spelling, found '{0}{1}', which has none; move this material to a .dss file and write the node with UE.Expression.` | Properties 里一个没有 2.0 形式的参数节点 token。 |  [详解](sections.md) |
| `DSH4201: '{0}' is not a type; did you mean '{1}'? Type names are case-sensitive.` | Properties 之外类型位置上的单词不是类型。 | 检查拼写；消息会给出最接近的类型名。 |
| `DSH2215: Expected an initializer on the 1.x Graph variable '{0}' of type '{1}', found none, and 1.x had no zero value for that type.` | Graph 里的纹理、SamplerState 或 Substrate 声明没有写 =。数字从 0 开始，MaterialAttributes 从空集合开始。 | 给变量写初始化式。 [详解](../graph/statements.md) |
| `DSH4361: This expression is {0} and the material graph has no matrices; compute it inside the custom body instead.` | 一个矩阵值要变成节点或引脚 —— 材质图没有矩阵引脚。 | 坐标空间变换用 UE.Transform / UE.TransformPosition；真正的矩阵运算整段放进 HLSL 函数体，向量进出。 |
| `DSH4364: A parameter of type {0} cannot be a material function input; the graph has no pin that carries one.` | 材质函数输入用了没有引脚能承载的类型，比如 SamplerState。 | 去掉采样器参数：纹理输入自带采样器。 |
| `DSH6210: '{0}' is '@custom', so '{1}' becomes an input pin of a Custom node, and a Custom node cannot take a material; pass the fields it needs instead.` | Function / GraphFunction 把 MaterialAttributes 当输入。 | 传函数体需要的字段（float3 BaseColor、float Roughness…）；MaterialAttributes 可以作为结果。 [详解](functions.md) |
| `DSH6253: '{0}' uses Substrate on '{1}'; a custom node has no Substrate pins, so build that part of the material out of reflected Substrate nodes.` | Function / GraphFunction 的签名里出现 Substrate。任何引擎版本的 Custom 节点都没有 Substrate 引脚。 | 在 ShaderFunction 或 Shader 的 Graph 里用 Substrate.\* 节点构建这部分。 [详解](../builtins/substrate.md) |
| `DSH6254: '{0}' returns a texture through '{1}'; a custom node output carries float1..4 or material attributes, never a texture object.` | Function / GraphFunction 返回纹理或采样器，或通过 out 参数交出一个。 | 在函数体里采样，返回采样结果；纹理可以作为输入。 |
| `DSH4228: {0} expects {1}, and this is {2}.` | 一种值赋到了声明为另一种的地方 —— 数字赋给 MaterialAttributes 或纹理变量，纹理赋给数字。 | 检查两侧的类型。 |
| `DSH4226: {0} expects {1}, and this is {2}.` | 宽度对不上（向量从不静默扩宽），或者把不是数字的值放进了算术。 | 用 swizzle 或构造器让宽度一致；标量可以广播到任意宽度。 [详解](../graph/expressions.md) |
| `DSH4243: Both sides of this '/' are integers, and the material graph has no integer division; write 'float(a) / b' for the fraction, or 'floor(float(a) / b)' for the whole part.` | 1.x Graph 里两侧都是整数构造器调用的 /。 | 写 float(a) / b 取小数结果，或 floor(float(a) / b) 取整数部分。 [详解](../graph/expressions.md) |
| `DSH5277: '{0}' is the GLSL spelling of '{1}'; a 1.x source may use it and it is read as '{1}', and a '.dss' writes '{1}'.` | Graph 里用了 GLSL 拼法 mix、fract、mod（警告）。 | 写消息给出的 HLSL 名字；dsc migrate 会替你改。 [详解](../builtins/math.md) |

<a id="继续阅读"></a>

## 继续阅读

- [Section](sections.md) — 六个声明上下文分别在哪里。
  - [Properties 类型](properties.md) — 每个紧凑 token 生成哪种节点，以及 22 个显式参数节点。
  - [运算符与转换](../graph/expressions.md) — 强转、扩宽与整数除法规则。
  - [Substrate 节点](../builtins/substrate.md) — Substrate token 背后的 UE 5.4+ 能力。
