---
title: "示例"
description: "示例"
---

<a id="examples"></a>

# 示例

> [DreamShader](../index.md) » **示例**

十五个完整、自包含的 DreamShaderLang 1.x 源文件，从最小的可能材质排序到需要 `.dsh` 头文件、`.dsf` 函数文件或特定引擎版本的构造 —— 每个后面都跟着以 `.dss` 形式编写的相同内容。

| | |
| :-- | :-- |
| 适用于 | DreamShaderLang 1.x（`.dsm`、`.dsf`、`.dsh`），由 2.0 编译器通过其旧版前端构建 *（自 2.0.0 起）*；`.dss` 形式为 [DreamShaderLang 2.0](../language-v2/index.md) |
| 引擎 | UE `5.3` – `5.8`；任何受版本限制的内容都会在行内标记 |
| 假定的源根目录 | `<Project>/DShader` —— `SourceDirectory` [项目设置](../settings/project.md) |
| 生成的输出 | 一个 `Graph` 后端材质以及每个材质函数、层和混合：在每次成功构建时保存。`ThinCustom` 材质 —— 默认后端 —— 保持 Ephemeral（临时）、在内存中，直到被实体化；参见 [临时材质](../generation/ephemeral.md) |

下面的每个代码片段都是一个完整的文件。开头注释中的路径是示例假定的磁盘位置；`import` 和 `#include` 路径相对于该布局解析。使用 `Path(Engine, …)` 或 `/Engine/…` 引用的资产随引擎一起提供，因此这些示例可以按原样加载；引用项目资产的两个示例（[11](#11-virtualfunction-wrapping-an-existing-asset)、[15](#15-runtime-virtual-texture-sampling-and-writing)）没有任何示例会构建它们，并已标记。

注释中的诊断按代码引用。一次编译会将每个诊断报告为 `<file>(<line>,<col>): DSHnnnn: <message>`，位于它所涉及的构造的行和列；代码的页面包含消息、其原因和修复方法。代码才是契约，措辞不是。在旧日志中找到的 1.x 生成器消息映射在 [诊断](../diagnostics/index.md#1x-messages-and-their-codes) 上。

*在 `.dss` 中的相同内容*，位于每个示例下方，是用 DreamShaderLang 2.0 手写的该文件：`#pragma material(…)` 用于 `Settings`，一个 `uniform` 带有每个参数的 `///` 标签，以及一个 `export` 函数，其名称是资产的名称。`.dss` 在文件自己的文件夹中构建其导出，因此这里的每个示例构建的资产路径与其旁边的 1.x 文件相同 —— 保留其中之一，而非两者。[`dsc migrate`](../tools/migrate.md) 编写 1.x 文件的 `.dss` 并证明它构建相同的图；其输出比这些更长，因为它保留了每个 1.x 细节（每个参数上的 `/// @sort 32`、1.x 为每个输出提供的零初始化器）。

<a id="contents"></a>

## 目录

| # | 示例 | 练习内容 |
| :-- | :-- | :-- |
| [1](#1-minimal-unlit-material) | 最小无光照材质 | [`Shader`](../language/shader.md)、[输出绑定](../language/output-bindings.md) |
| [2](#2-parameters-scalar-vector-static-switch-texture) | 参数、元数据、组作用域 | [`Properties`](../language/properties.md)、[元数据](../parameters/metadata.md) |
| [3](#3-a-shared-dsh-header-namespace--function) | 共享的 `.dsh` 头文件 | [`Namespace`](../language/namespace.md)、[`import`](../language/import.md) |
| [4](#4-calling-functions-value-form-and-statement-form) | 调用函数 | [图调用](../graph/calls.md) |
| [5](#5-ue-nodes-and-a-generic-ueexpression) | `UE.*` 和 `UE.Expression` | [`UE.*`](../builtins/ue.md)、[`UE.Expression`](../builtins/ue-expression.md) |
| [6](#6-if--else-in-graph) | `if` / `else` | [`if`](../graph/if.md) |
| [7](#7-materialattributes-output-binding) | `MaterialAttributes` | [MaterialAttributes](../graph/material-attributes.md) |
| [8](#8-substrate-material-ue-54) | Substrate 材质 *（UE 5.4+）* | [`Substrate.*`](../builtins/substrate.md) |
| [9](#9-shaderfunction-in-a-dsf-imported-and-called) | `ShaderFunction` 在 `.dsf` 中 | [`ShaderFunction`](../language/shader-function.md)、[`VirtualFunction`](../language/virtual-function.md) |
| [10](#10-shaderlayer--shaderlayerblend) | `ShaderLayer` / `ShaderLayerBlend` | [层](../language/shader-layer.md) |
| [11](#11-virtualfunction-wrapping-an-existing-asset) | `VirtualFunction` | [`VirtualFunction`](../language/virtual-function.md) |
| [12](#12-graphfunction-hoisting-a-ue-call-into-a-custom-node) | `GraphFunction` 提升 | [`GraphFunction`](../language/graph-function.md) |
| [13](#13-settings-domain-shading-model-blend-mode-backend) | `Settings` 和 `Backend` | [着色器设置](../settings/material.md)、[后端](../settings/backend.md) |
| [14](#14-layout-placement-and-region) | `Layout` 和 `#Region` | [`Layout`](../language/layout.md) |
| [15](#15-runtime-virtual-texture-sampling-and-writing) | 运行时虚拟纹理 | [`UE.Expression`](../builtins/ue-expression.md)、[输出绑定](../language/output-bindings.md) |

自定义 Pass 管线（`.dsp`、UE 5.8）有自己的页面：[自定义 Pass 示例](custom-pass.md)。

---

<a id="1-minimal-unlit-material"></a>

## 1. 最小无光照材质

产生资产的最小文件：一个参数、一个绑定、一个赋值。

```c
// DShader/Materials/M_Minimal.dsm
Shader(Name="Materials/M_Minimal", Root="Game")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.2, 0.2);
    }

    Settings = {
        Domain       = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = Tint;
    }
}
```

生成的资产：

```text
package     /Game/Materials/M_Minimal
object path /Game/Materials/M_Minimal.M_Minimal
class       UDreamShaderMaterialInstance over a hidden base UMaterial (the default ThinCustom backend)
```

- `Shader` 是**区分大小写**匹配的；`Properties`、`Settings`、`Outputs`、`Graph` 不是。参见 [词法元素](../language/lexical.md#case-sensitivity)。
- 节名称后的 `=` 是可选语法糖 *（自 1.5.0 起）*：`Properties { … }` 的解析完全相同。节主体中的最后一个 `;` 也是可选的。
- `Root` 默认为 `/Game`，因此上面的 `Root="Game"` 是冗余的。完整规则：[资产路径](../generation/asset-paths.md)。
- 一个没有 `Graph` 节且其 `Outputs` 中没有任何内容的 `Shader` 是 [`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255)。没有 `Graph` 时，`Outputs` 自行计算值 —— 带有初始化器的输出声明，或 *（自 2.0.0 起）* 右侧为表达式的绑定。一个没有 `Outputs` 节的 `Shader` 在没有任何连接的情况下构建，并通过警告 [`DSH2256`](../diagnostics/DSH2xxx.md#dsh2256) *（自 2.0.0 起）* 说明这一点。

<a id="the-same-in-a-dss"></a>

### 在 `.dss` 中的相同内容

```hlsl
// DShader/Materials/M_Minimal.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

uniform float3 Tint = float3(1.0, 0.2, 0.2);

export void M_Minimal(inout material m)
{
    m.EmissiveColor = Tint;
}
```

签名 `export void <Name>(inout material m)` 是使该文件成为材质的原因；1.x `Outputs` 绑定是对 `m` 成员的写入。

*另见：* [`Shader`](../language/shader.md) · [输出绑定](../language/output-bindings.md) · [材质枚举](../settings/material-enums.md)

---

<a id="2-parameters-scalar-vector-static-switch-texture"></a>

## 2. 参数：标量、向量、静态开关、纹理

每个参数系列，带有 `Group("…")` 作用域、`[ … ]` 元数据和 `Slider(min, max)` 简写。

```c
// DShader/Materials/M_Params.dsm
Shader(Name="Materials/M_Params")
{
    Properties = {
        Group("Surface") {
            ScalarParameter Roughness = 0.55 [Slider(0, 1)];
            VectorParameter Albedo    = float4(0.8, 0.8, 0.8, 1.0) [Description="Base albedo"];
        }

        Group("Detail") {
            TextureSampleParameter2D DetailMap = Path(Engine, "EngineResources/WhiteSquareTexture") [
                SamplerType   = "LinearColor";
                SamplerSource = "FromTextureAsset";
                SortPriority  = 99;
            ];
            StaticSwitchParameter UseDetail = true;
        }

        Texture2D   NoiseTex   = Path(Engine, "EngineResources/WhiteSquareTexture");
        const float DebugScale = 1.0;
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        float3 Color;
        float  Rough;

        Base.BaseColor = Color;
        Base.Roughness = Rough;
    }

    Graph = {
        vec2 UV     = UE.TexCoord(Index = 0);
        vec4 Detail = DetailMap(Coordinates = UV);
        vec4 Noise  = SampleTexture2D(NoiseTex, UV);

        Color = UseDetail(True = Detail.rgb * Albedo.rgb, False = Albedo.rgb);
        Rough = Roughness * DebugScale * Noise.r;
    }
}
```

- `float`/`vec3`/`Texture2D` 是**紧凑**拼写；`ScalarParameter`/`VectorParameter`/`TextureSampleParameter2D` 显式命名 Unreal 节点。两者都在 [紧凑类型](../parameters/compact-types.md) 和 [参数节点](../../parameters/parameter-nodes.md) 中记录。
- `Group("…") { … }` 作用域将其名称标记到其中的参数上，并从**每个 `Properties` 节一个计数器**开始将它们编号为 `0, 10, 20, …`；显式 `SortPriority` 优先，并且不占用编号。这里 `Roughness` 获得 0，`Albedo` 获得 10。嵌套组通过 `|`（`Outer|Inner`）组合。
- *（自 2.0.0 起）* 该标记会作用到标量、向量、纹理和静态布尔参数。带引脚或开关的参数 —— `DetailMap`、`UseDetail` —— 在每次使用时都写为节点，该节点只携带其自身 `[ … ]` 中的元数据：`DetailMap` 获得 `SortPriority = 99` 而没有组，`UseDetail` 两者都没有。
- `const` 声明一个任何人都无法覆盖的值 —— 一个编译期常量，而不是参数。它接受标量、向量或纹理类型；任何其他类型都会产生 [`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253)。

> [!NOTE]
> 调用形式 `Name(Pin = …)` 仅适用于其节点拥有输入引脚的十个参数令牌——
> `ChannelMaskParameter`、`StaticComponentMaskParameter` 以及八个 `*SampleParameter*` 令牌——
> 并且每个实参都必须命名（[`DSH5259`](../diagnostics/DSH5xxx.md#dsh5259)）。**纹理
> 对象**参数（`Texture2D NoiseTex`、`TextureObjectParameter`）没有引脚，也不是函数：
> `NoiseTex(Coordinates = UV)` 是 [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208)。如上述，改用
> 保留的 `SampleTexture2D(textureObject, uv)` 形式来采样它。`SampleTexture2D` 按
> **区分大小写**匹配，并且恰好接受两个位置实参
> （否则为 [`DSH5256`](../diagnostics/DSH5xxx.md#dsh5256)）。

> [!WARNING]
> 裸读取 `StaticSwitchParameter` 并非一个值：`Color = UseDetail;` 是
> [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)。用 `True=` 和 `False=`（或 `A=`/`B=`，或
> 按位置）来调用它；缺少两个分支的调用会是 [`DSH5258`](../diagnostics/DSH5xxx.md#dsh5258)，而
> 任何其他实参都会被丢弃并给出警告 [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254)。

<a id="the-same-in-a-dss-1"></a>

### 在 `.dss` 中同样如此

```hlsl
// DShader/Materials/M_Params.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

/// @group Surface   @sort 0    @slider 0 1
uniform float Roughness = 0.55;
/// @group Surface   @sort 10   @desc Base albedo
uniform float4 Albedo = float4(0.8, 0.8, 0.8, 1.0);

/// @group Detail   @sort 99   @sampler LinearColor
/// @default /Engine/EngineResources/WhiteSquareTexture
uniform Texture2D DetailMap;
/// @static
/// @group Detail   @sort 20
uniform bool UseDetail = true;

/// @default /Engine/EngineResources/WhiteSquareTexture
uniform Texture2D NoiseTex;
static const float DebugScale = 1.0;

export void M_Params(inout material m)
{
    float2 UV = UE.TexCoord(Index = 0);
    float4 Detail = DetailMap.Sample(UV);
    float4 Noise = NoiseTex.Sample(UV);

    float3 Color = Albedo.rgb;
    if (UseDetail)
    {
        Color = Detail.rgb * Albedo.rgb;
    }
    m.BaseColor = Color;
    m.Roughness = Roughness * DebugScale * Noise.r;
}
```

- 纹理 `uniform` 不接受初始化器：其资产为 `/// @default`，其采样器类型
  为 `/// @sampler`。用 `.Sample(UV)` 采样时，它会构建一个 `TextureObjectParameter` 和一个
  `TextureSample`，而 1.x 令牌构建的是一个 `TextureSampleParameter2D`；采样器源为该
  节点的默认值，*From Texture Asset*。
- 由 `if` 读取的 `/// @static` `uniform bool` 是静态开关。`/// @group` 和 `/// @sort`
  写入每个参数的组和优先级；`static const` 是 1.x 的 `const`。

*另请参阅：* [`Properties`](../language/properties.md) · [元数据](../parameters/metadata.md) ·
[`SamplerType`](../../parameters/sampler-type.md) · [`Path(...)`](../parameters/path.md) ·
[在 `Graph` 中读取参数](../../parameters/graph-usage.md)

---

<a id="3-a-shared-dsh-header-namespace--function"></a>

## 3. 共享的 `.dsh` 头文件：`Namespace` + `Function`

`.dsh` 头文件包含 `Function`、`GraphFunction`、`Namespace` 和 `VirtualFunction` 块、
`import` 行以及 2.0 声明——没有资产块（[`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249)）。
它不生成任何资产。它会被单独解析，其声明会成为每个导入它的文件的名称
*（自 2.0.0 起；1.x 会将其文本粘贴到导入文件中）*。

```c
// DShader/Shared/Common.dsh
Namespace(Name="Common")
{
    Function ApplyTint(in vec3 color, in vec3 tint, out vec3 result) {
        result = color * tint;
    }

    Function float Luma(in vec3 color) {
        return dot(color, float3(0.299, 0.587, 0.114));
    }
}

Function SelfContained Remap01(in float value, out float result) {
    result = saturate(value * 0.5 + 0.5);
}

Function SplitChannels(in vec4 src, out vec3 rgb, out float alpha) {
    rgb   = src.rgb;
    alpha = src.a;
}
```

导入它：

```c
// DShader/Materials/M_Tinted.dsm
import "Shared/Common.dsh";

Shader(Name="Materials/M_Tinted")
{
    Properties = {
        vec3 Albedo = vec3(0.6, 0.8, 1.0);
        vec3 Tint   = vec3(1.0, 0.4, 0.1);
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Common::ApplyTint(Albedo, Tint, Tinted);
        Color = Tinted;
    }
}
```

- `import "Shared/Common"` 是等效的：当说明符**完全不含扩展名**时，会
  追加 `.dsh`。只有 `.dsh` 才能被导入 *（自 2.0.0 起）*：`.dsf` 或 `.dsm` 路径是
  [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252)。
- 相对说明符会先针对导入文件自身的目录进行尝试，然后是其源根的
  目录（`DShader`），再然后是那个根的 `Packages`——绝不会是另一个根的。解析到其根之外的
  候选会被跳过。参见 [`import`](../language/import.md#resolution)。
- *（自 2.0.0 起）* `import` 在文件作用域被读取为令牌：`;` 可选，`'single quotes'`
  在 1.x 文件中可用，而 `/* … */` 注释内的 `import` 会被注释掉（1.x 会处理它）。
  无论如何请将每个 `import` 单独放在一行：编辑器的依赖扫描器在保存头文件时会
  重新编译导入方，它仍会逐行读取它们。
- `Function` 主体是 HLSL，而非 `Graph` 语句：这里的 `dot`、`saturate` 和 `float3(…)` 是
  HLSL 内建函数。GLSL 写法会在这些主体内被重写，不区分大小写且不会有
  诊断（`vec3`→`float3`、`mix`→`lerp`、`fract`→`frac`、`mod`→`fmod`）。
- *（自 2.0.0 起）* 每个 `Function` 节点都会嵌入其主体调用的 `Function`，而 1.x 仅对
  `SelfContained` 这样处理，因此 `SelfContained` 不会改变任何东西。`Inline` 是其旧写法，会带有
  警告 [`DSH6306`](../diagnostics/DSH6xxx.md#dsh6306)；在 `GraphFunction` 上两者都是
  [`DSH6307`](../diagnostics/DSH6xxx.md#dsh6307)。
- *（自 2.0.0 起）* 另一个 `Function` 或 `GraphFunction` 主体内的命名空间限定调用可以工作：
  主体规范化会将 `Common::ApplyTint` 展平为 `Common_ApplyTint`，即该函数自身的
  名称，而被调用者会嵌入调用者的节点中。1.x 会留下一个未定义符号给着色器
  编译器。编写 `::` 时不要在它周围加空格。

<a id="the-same-in-a-dss-2"></a>

### 在 `.dss` 中同样如此

```hlsl
// DShader/Materials/M_Tinted.dss
#include "Shared/Common.dsh"

#pragma material(Domain = Surface, ShadingModel = Unlit)

uniform float3 Albedo = float3(0.6, 0.8, 1.0);
uniform float3 Tint   = float3(1.0, 0.4, 0.1);

export void M_Tinted(inout material m)
{
    float3 Tinted;
    Common_ApplyTint(Albedo, Tint, Tinted);
    m.EmissiveColor = Tinted;
}
```

`.dsh` 同时持有两种方言，因此 `.dss` 会按原样包含该头文件。命名空间函数
以其声明所用的名称被调用，即 `Common_ApplyTint`，而 `out` 实参是先声明的
变量。头文件本身采用 2.0 写法——`.dsm` 也会导入这一个：

```hlsl
// DShader/Shared/Common.dsh
/// @custom
/// @name Common::ApplyTint
void Common_ApplyTint(float3 color, float3 tint, out float3 result)
{
    result = color * tint;
}

/// @custom
/// @name Common::Luma
float Common_Luma(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

/// @custom
void Remap01(float value, out float result)
{
    result = saturate(value * 0.5 + 0.5);
}

/// @custom
void SplitChannels(float4 src, out float3 rgb, out float alpha)
{
    rgb   = src.rgb;
    alpha = src.a;
}
```

`/// @custom` 会原样保留主体，并让每次调用成为一个 Custom 节点，就像 1.x 的 `Function` 那样；
`/// @name` 保留节点的标题 `Common::ApplyTint`。

*另请参阅：* [`import`](../language/import.md) · [`Namespace`](../language/namespace.md) ·
[`Function`](../language/function.md) · [源文件](../language/source-files.md) ·
[包](../tools/packages.md)

---

<a id="4-calling-functions-value-form-and-statement-form"></a>

## 4. 调用函数：值形式与语句形式

函数的值是其返回值，或其第一个 `out`。语句形式先传递输入，
然后每个输出一个目标；其末尾实参命名接收结果的变量。

```c
// DShader/Materials/M_Calls.dsm
import "Shared/Common.dsh";

Shader(Name="Materials/M_Calls")
{
    Properties = {
        vec4 Source = vec4(0.4, 0.6, 0.9, 0.75);
        vec3 Tint   = vec3(1.0, 0.4, 0.1);
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Translucent"; }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        // statement form: two out results, two target names
        SplitChannels(Source, Rgb, A);

        // statement form: one out result
        Common::ApplyTint(Rgb, Tint, Tinted);

        // value form: single-output functions
        float L    = Common::Luma(Tinted);
        float Soft = Remap01(L);

        Color = Tinted * Soft;
        Alpha = A;
    }
}
```

| 被调用者种类 | 值形式 | 语句形式 | 命名实参 |
| :-- | :-- | :-- | :-- |
| `Function` | 是——返回值，或第一个 `out` *（自 2.0.0 起；1.x：仅一个结果）* | 是 | 是 *（自 2.0.0 起）* |
| `GraphFunction` | 同 `Function` | 是 | 是 *（自 2.0.0 起）* |
| `ShaderFunction` | 是——第一个输出，除非选择器挑选了另一个；只能从其自身文件调用 | 是 | 是 |
| `ShaderLayer` / `ShaderLayerBlend` | **不可调用**：[`DSH6208`](../diagnostics/DSH6xxx.md#dsh6208) *（自 2.0.0 起）* | — | — |
| `VirtualFunction` | 是 | 是 | 是 |

- 一个未被声明的 out 目标会由该调用声明为输出类型的局部变量（信息
  [`DSH5283`](../diagnostics/DSH5xxx.md#dsh5283)）。目标必须可赋值——即一个变量，或该变量的
  成员或 swizzle（[`DSH4239`](../diagnostics/DSH4xxx.md#dsh4239)）——并且类型为输出类型
  （[`DSH4218`](../diagnostics/DSH4xxx.md#dsh4218)）。
- 在全位置参数语句调用中，最后几个实参接收输出，每个输出对应一个，按
  声明顺序；它们之前的实参是输入。实参过多会
  [`DSH4224`](../diagnostics/DSH4xxx.md#dsh4224)，而省略一个并非 `opt` 的输入则是
  [`DSH4217`](../diagnostics/DSH4xxx.md#dsh4217)。
- 函数名**区分大小写** *(自 2.0.0 起)*：一个仅大小写不同的调用
  仍能解析，并带有警告 [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275)。**没有
  重载解析**——一个名称声明一个事物，对它的第二次声明在其书写处
  为 [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210)。
- 数学内建函数（`lerp`、`dot`、`pow`、`min`、`max`、`clamp`、`saturate`、`sin`、`frac`、`fmod`、
  …）在用户函数之前解析；与其中之一同名的函数
  在其声明处为 [`DSH6206`](../diagnostics/DSH6xxx.md#dsh6206)。`mix`、`fract` 和 `mod` 是
  它们的 GLSL 拼写，可在 1.x `Graph` 中接受，并带有警告
  [`DSH5277`](../diagnostics/DSH5xxx.md#dsh5277)。

<a id="the-same-in-a-dss-3"></a>

### 在 `.dss` 中的相同示例

```hlsl
// DShader/Materials/M_Calls.dss
#include "Shared/Common.dsh"

#pragma material(Domain = Surface, ShadingModel = Unlit, BlendMode = Translucent)

uniform float4 Source = float4(0.4, 0.6, 0.9, 0.75);
uniform float3 Tint   = float3(1.0, 0.4, 0.1);

export void M_Calls(inout material m)
{
    float3 Rgb;
    float  A;
    SplitChannels(Source, Rgb, A);

    float3 Tinted;
    Common_ApplyTint(Rgb, Tint, Tinted);

    float L = Common_Luma(Tinted);
    float Soft;
    Remap01(L, Soft);

    m.EmissiveColor = Tinted * Soft;
    m.Opacity = A;
}
```

`.dss` 会将调用的每个 `out` 传递给一个它首先声明的变量。无返回类型的函数的 1.x 值调用 `Remap01(L)` 是一条遗留规则（L3b）；`dsc migrate` 会将其写成
上面的语句——参见[重写做了什么](../../tools/migrate.md#what-the-rewrite-does)。

*另见：* [图调用](../graph/calls.md) · [名称解析](../graph/name-resolution.md) ·
[数学内建函数](../builtins/math.md) · [语句](../graph/statements.md)

---

<a id="5-ue-nodes-and-a-generic-ueexpression"></a>

## 5. `UE.*` 节点与泛型 `UE.Expression(...)`

`UE.<Name>(…)` 创建一个 `UMaterialExpression`。`<Name>` 是内建目录中的一个类——可以是
任何引擎表达式类（用其短名称，如 `Sine`、`CameraVectorWS`），或目录
携带的 1.x 别名之一（`TexCoord`、`TransformVector`、…）。`UE.Expression(Class = …)` 则是在
参数中命名该类；两者是同一路径 *(自 2.0.0 起)*。

```c
// DShader/Materials/M_UEBuiltins.dsm
Shader(Name="Materials/M_UEBuiltins")
{
    Settings = {
        Domain       = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float2 uv    = UE.TexCoord(Index = 0);
        float  t     = UE.Time();
        float3 view  = UE.CameraVectorWS();

        // Generic form: any MaterialExpression class, by name.
        float pulse  = UE.Expression(Class="Sine", OutputType="float1", Input=t);

        // Class= may be omitted when the function name IS the class name.
        float pulse2 = UE.Sine(OutputType="float1", Input=t * 0.5);

        // Reflected literal properties are written by name.
        float3 local = UE.TransformVector(Input = view, Source = "World", Destination = "Local");

        Color = float3(uv.x, uv.y, pulse + pulse2 + local.z);
    }
}
```

- `Class="Sine"`、`"MaterialExpressionSine"`、`"UMaterialExpressionSine"` *(自 2.0.0 起)* 和
  `"/Script/Engine.MaterialExpressionSine"` 都解析为同一个类。引号是可选的：
  `Class=Sine` 和 `OutputType=float1` 的作用完全相同。
- 参数首先命名该类的一个输入引脚，然后是它的一个属性；若相同则
  引脚优先。引脚接受一个表达式；属性接受一个字面量
  （[`DSH4373`](../diagnostics/DSH4xxx.md#dsh4373) 表示在运行时计算的值）。既不是引脚也不是属性的
  名称会怎样，见 [`UE.Expression`](../builtins/ue-expression.md#argument-dispatch)。
- 不带 `Class=` 的 `UE.Expression(…)` 是 [`DSH5218`](../diagnostics/DSH5xxx.md#dsh5218)。
- `OutputType`（别名 `ResultType`）不再必需 *(自 2.0.0 起)*：遗留前端会将其
  从调用中移除，而由目录确定节点类型。它仍然设置 `Custom` 节点的输出类型；
  在其他地方，数字标记只给出单输出节点值的宽度，其他一切都会被
  丢弃。参见 [`OutputType`](../builtins/output-type.md)。

> [!WARNING]
> 对于 [`UE.*` 目录](../builtins/ue.md#catalogue)的 27 个 1.x 名称——`TexCoord`、`Time`、
> `CameraVectorWS`、`TransformVector`、…——遗留前端会保留 1.x 读取的实参，
> 并丢弃其他所有实参、未知名称或位置参数，并带有警告
> [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254) *(自 2.0.0 起；1.x 会无声地丢弃它)*。
> `UE.Time(Bogus = 1)` 和 `UE.TexCoord(0)` 仍会构建，并说明原因。唯一保留的位置参数
> 是 `UE.TransformVector` / `UE.TransformPosition` 的第一个（→ `Input`）。在任何其他节点上，
> 位置参数仅在目录为类给出参数顺序时绑定，在其他情况下为
> [`DSH5220`](../diagnostics/DSH5xxx.md#dsh5220)。

<a id="the-same-in-a-dss-4"></a>

### 在 `.dss` 中的相同示例

```hlsl
// DShader/Materials/M_UEBuiltins.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

export void M_UEBuiltins(inout material m)
{
    float2 uv   = UE.TexCoord(Index = 0);
    float  t    = UE.Time();
    float3 view = UE.CameraVectorWS();

    float  pulse  = UE.Expression(Class = "Sine", Input = t);
    float  pulse2 = UE.Sine(Input = t * 0.5);
    float3 local  = UE.TransformVector(Input = view, Source = "World", Destination = "Local");

    m.EmissiveColor = float3(uv.x, uv.y, pulse + pulse2 + local.z);
}
```

`.dss` 不会得到遗留过滤器：节点没有的实参是
[`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213)，而不是警告。

*另见：* [`UE.*` 目录](../builtins/ue.md) · [`UE.Expression`](../builtins/ue-expression.md) ·
[`OutputType`](../builtins/output-type.md) · [变换基准](../builtins/transform.md)

---

<a id="6-if--else-in-graph"></a>

## 6. `if` / `else` 中的 `Graph`

条件必须加括号，每个分支体必须加花括号。两个分支都会构建到
图中；每个被更改的变量对应一个条件节点——一个 `UMaterialExpressionIf`，或在静态 bool 上的 `StaticSwitch`
——在运行时在它们之间选择。

```c
// DShader/Materials/M_Branch.dsm
Shader(Name="Materials/M_Branch")
{
    Properties = {
        ScalarParameter Threshold = 0.5  [Group="Surface"];
        VectorParameter Lit       = float4(1.0, 0.85, 0.2, 1.0) [Group="Surface"];
        VectorParameter Dark      = float4(0.05, 0.05, 0.1, 1.0) [Group="Surface"];
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "Unlit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float2 uv   = UE.TexCoord(Index = 0);
        float  mask = uv.x;

        if (mask > Threshold) {
            Color = Lit.rgb;
        } else if (mask > Threshold * 0.5) {
            Color = Lit.rgb * 0.5;
        } else {
            Color = Dark.rgb;
        }
    }
}
```

| 条件 | 含义 |
| :-- | :-- |
| `a > b`、`a < b`、`a >= b`、`a <= b`、`a == b`、`a != b` | 六个比较运算符 |
| `if (x)` | 真值——与 `x != 0` 相同 |

- 条件比较两个数字：比较两个向量是
  [`DSH4260`](../diagnostics/DSH4xxx.md#dsh4260)，不是数字的操作数是
  [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226)。
- 在 `if` 之前声明且仅在一个分支中被更改的变量，会与其在
  `if` 之前所拥有的值合并。在它之前没有值且仅在一个分支中被赋值的变量，
  在 `if` 之后读取它的地方为 [`DSH4372`](../diagnostics/DSH4xxx.md#dsh4372)。
- 在分支*内部声明*的变量是该分支的局部变量 *(自 2.0.0 起)*：它不会被合并，并且
  在 `if` 之后其名称未知。
- 值在每次赋值时都会转换为其变量的声明类型，因此一个变量的两个分支值
  总是一致的。
- 在分支内读取参数没有问题——参数从来不是分支输出。
- `if` 是**区分大小写**匹配的：`If (x) { … }` 是对名为 `If` 的函数的调用，而其后的
  `{` 是 [`DSH2154`](../diagnostics/DSH2xxx.md#dsh2154)。

> [!WARNING]
> `&&` 和 `||` 在 1.x 条件中被拒绝 *(自 2.0.0 起)*：[`DSH2210`](../diagnostics/DSH2xxx.md#dsh2210)。
> 1.x 会无声地丢弃第一个比较之后的所有内容，因此 `if (a > 0 && b > 0)` 测试的是
> `a > 0`。请改为嵌套两个 `if` 语句。其他被 1.x 截断的构造——`%`、`?:`、`&`、`|`、
> `^`、`<<`、`v[i]`——在表达式中任何地方都是错误
> （[`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200)–[`DSH2202`](../diagnostics/DSH2xxx.md#dsh2202)）；参见
> [不支持的构造](../graph/unsupported.md#operators-1x-truncated)。

<a id="the-same-in-a-dss-5"></a>

### 在 `.dss` 中的相同示例

```hlsl
// DShader/Materials/M_Branch.dss
#pragma material(Domain = Surface, ShadingModel = Unlit, BlendMode = Opaque)

/// @group Surface
uniform float Threshold = 0.5;
/// @group Surface
uniform float4 Lit = float4(1.0, 0.85, 0.2, 1.0);
/// @group Surface
uniform float4 Dark = float4(0.05, 0.05, 0.1, 1.0);

export void M_Branch(inout material m)
{
    float2 uv = UE.TexCoord(Index = 0);
    float mask = uv.x;

    float3 Color = float3(0, 0, 0);
    if (mask > Threshold)
    {
        Color = Lit.rgb;
    }
    else if (mask > Threshold * 0.5)
    {
        Color = Lit.rgb * 0.5;
    }
    else
    {
        Color = Dark.rgb;
    }
    m.EmissiveColor = Color;
}
```

分支所赋值的变量在 `if` 之前声明，正如 1.x 的 `Outputs` 声明一样。

*另见：* [`if` / `else`](../graph/if.md) · [表达式](../graph/expressions.md) ·
[转换](../graph/conversions.md)

---

<a id="7-materialattributes-output-binding"></a>

## 7. `MaterialAttributes` 输出绑定

绑定 `Base.MaterialAttributes` 会在生成的材质上开启 *Use Material Attributes*。
`MaterialAttributes Attrs;` 是一个空属性集——一个 `MakeMaterialAttributes` 节点——其成员按名称写入；在使用该值的地方，这些写入会变成一个
`SetMaterialAttributes` 节点 *(自 2.0.0 起；1.x 中每次写入会链式产生一个节点)*。

```c
// DShader/Materials/M_Attrs.dsm
Shader(Name="Materials/M_Attrs")
{
    Properties = {
        vec3  BaseTint = vec3(0.6, 0.8, 1.0);
        float R        = 0.35;
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        Base.MaterialAttributes = Attrs;
    }

    Graph = {
        MaterialAttributes Attrs;

        Attrs.BaseColor = BaseTint;
        Attrs.Roughness = R;
        Attrs.Metallic  = 0.0;

        // A member already written reads back as the value written; no node is made.
        float Echo = Attrs.Roughness;
        Attrs.Specular = Echo;
    }
}
```

> [!NOTE]
> `Outputs` 绑定是在 `Graph` 之后应用的，因此该绑定会采用在
> `Graph` 中声明的 `Attrs`，如上所述。*(自 2.0.0 起)* 一个 `Outputs` 声明 `MaterialAttributes Attrs;` 也是
> 该变量，初始为空——参见 [MaterialAttributes](../graph/material-attributes.md#creating-a-value)。
> `MaterialAttributes` 是 `Outputs`、`Inputs`、`Graph` 变量和函数结果的一种类型；作为一种
> `Properties` 类型，它是 [`DSH3252`](../diagnostics/DSH3xxx.md#dsh3252)，而作为 `Function` 的输入时，它是
> [`DSH6210`](../diagnostics/DSH6xxx.md#dsh6210)。

- 成员名称是运行中引擎的材质属性名称（`BaseColor`、`Metallic`、`Specular`、`Roughness`、
  `EmissiveColor`、`Opacity`、`Normal`……）；引擎没有的名称是
  [`DSH5200`](../diagnostics/DSH5xxx.md#dsh5200)。
- 对 `MaterialAttributes` 值使用算术运算符会得到 [`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226)。
- 同一个 `Shader` 中的 `Base.MaterialAttributes` 和 `Base.FrontMaterial` 不再被拒绝 *(自
  2.0.0 起)*：两者都会被连接，并且 *Use Material Attributes* 会让材质读取该属性集。

<a id="the-same-in-a-dss-6"></a>

### 在 `.dss` 中的相同内容

```hlsl
// DShader/Materials/M_Attrs.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

uniform float3 BaseTint = float3(0.6, 0.8, 1.0);
uniform float  R        = 0.35;

export void M_Attrs(inout material m)
{
    m.BaseColor = BaseTint;
    m.Roughness = R;
    m.Metallic  = 0.0;
    m.Specular  = m.Roughness;
}
```

该条目写入 `m` 的成员，它们会直接进入材质的引脚：不会生成
`MakeMaterialAttributes` 节点。成员只有在其被写入后才会被读回
（否则为 [`DSH4370`](../diagnostics/DSH4xxx.md#dsh4370)）。

*另请参见：* [MaterialAttributes](../graph/material-attributes.md) ·
[输出绑定](../language/output-bindings.md) · [类型](../language/types.md)

---

<a id="8-substrate-material-ue-54"></a>

## 8. Substrate 材质 *(UE 5.4+)*

`Substrate.*` 会构建 Substrate 节点；`Base.FrontMaterial` 是使用它们的绑定。

```c
// DShader/Materials/M_Substrate.dsm
Shader(Name="Materials/M_Substrate")
{
    Properties = {
        vec3 Color = vec3(0.1, 0.6, 1.0);
    }

    Outputs = {
        Substrate Surface;
        Base.FrontMaterial = Surface;
    }

    Graph = {
        Surface = Substrate.Unlit(EmissiveColor = Color);
    }
}
```

- Substrate 需要 **UE 5.4 或更高版本**：目录会列出运行中引擎的 Substrate 类，
  而较旧的引擎没有这些类。在旧引擎中，`Substrate.*` 调用会
  [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210)，而 `Base.FrontMaterial`
  [`DSH5200`](../diagnostics/DSH5xxx.md#dsh5200)；`Substrate.Select` 需要 UE 5.6
  ([`DSH5294`](../diagnostics/DSH5xxx.md#dsh5294))。项目还必须开启 Substrate，
  引擎才能编译该图表——参见
  [一个源文件，两种项目](../language-v2/substrate.md#one-source-two-kinds-of-project--substrate-)。
- DreamShader 不会为 `Base.FrontMaterial` 设置着色模型 *(自 2.0.0 起；1.x 会强制使用 `Substrate`
  并拒绝任何其他 `ShadingModel`)*：`ShadingModel` 设置会按原样应用。
- BSDF 节点按名称接收其参数；五个合成节点（`Add`、`Weight`、`Mix`、
  `Layer`、`Select`）也按位置接收它们 *(自 2.0.0 起)*。其他地方的位置参数会
  [`DSH5220`](../diagnostics/DSH5xxx.md#dsh5220)，而 `Class=` 是
  [`DSH5216`](../diagnostics/DSH5xxx.md#dsh5216)：名称本身已经是该类。
- `GraphFunction` 主体中的 `Substrate.*` 调用不会被提升为节点——没有 Custom 节点输入
  会携带 Substrate 值——并会以警告
  [`DSH6316`](../diagnostics/DSH6xxx.md#dsh6316) 说明这一点 *(自 2.0.0 起)*。
- *(自 2.0.0 起)* Substrate 值可以组合：`A + B`、`A * w` 和 `lerp(A, B, t)` 分别是 `Substrate.Add`、
  `Substrate.Weight` 和 `Substrate.HorizontalMix`，而它们两者之间的 `if` 在静态条件下是
  `StaticSwitch`，否则是 `Substrate.Select`。任何其他运算符会
  [`DSH5293`](../diagnostics/DSH5xxx.md#dsh5293)，而对 Substrate 值读取成员会
  [`DSH4207`](../diagnostics/DSH4xxx.md#dsh4207)。参见 [Substrate 语法糖](../language-v2/substrate.md)。

<a id="the-same-in-a-dss-7"></a>

### 在 `.dss` 中的相同内容

```hlsl
// DShader/Materials/M_Substrate.dss
uniform float3 Color = float3(0.1, 0.6, 1.0);

export void M_Substrate(inout material m)
{
    m.FrontMaterial = Substrate.Unlit(EmissiveColor = Color);
}
```

*另请参见：* [`Substrate.*`](../builtins/substrate.md) ·
[输出绑定](../language/output-bindings.md) · [类型](../language/types.md)

---

<a id="9-shaderfunction-in-a-dsf-imported-and-called"></a>

## 9. `.dsf` 中的 `ShaderFunction`，从材质调用

`.dsf` 会声明函数资产——`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend`——以及
辅助块（`Function`、`GraphFunction`、`Namespace`、`VirtualFunction`）。只有 `.dsh` 可以
被导入，因此另一个文件中的材质会像访问任何现有资产一样访问生成的函数：
通过 `VirtualFunction` *(自 2.0.0 起)*。与函数位于同一 `.dsm` 中的材质
会按名称调用它。

```c
// DShader/Functions/F_Tint.dsf
ShaderFunction(Name="Functions/F_Tint")
{
    Inputs = {
        vec3 InColor;
        opt float Strength = 1.0 [
            Description  = "Preview strength";
            SortPriority = 10;
        ];
    }

    Outputs = {
        vec3 OutColor [Description="Tinted colour"];
    }

    Settings = {
        Description       = "Tint helper";
        ExposeToLibrary   = true;
        LibraryCategories = "Examples";
    }

    Graph = {
        OutColor = InColor * Strength;
    }
}
```

从材质调用它：

```c
// DShader/Materials/M_UsesTint.dsm
VirtualFunction(Name="F_Tint")
{
    Options = { Asset = Path(Game, "Functions/F_Tint"); }

    Inputs = {
        vec3 InColor;
        opt float Strength = 1.0;
    }

    Outputs = {
        vec3 OutColor;
    }
}

Shader(Name="Materials/M_UsesTint")
{
    Properties = {
        vec3 Albedo = vec3(0.6, 0.8, 1.0);
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        // named form; the opt input may be omitted or passed `default`
        Color = F_Tint(InColor = Albedo, Strength = 0.5);
    }
}
```

- *(自 2.0.0 起)* 在 1.9.x 及之前，材质会导入函数文件——
  `import "Functions/F_Tint.dsf";`——一次编译会构建两个资产。该导入现在会
  [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252)；`VirtualFunction` 声明由
  `F_Tint.dsf` 构建的资产，并且每个文件构建自己的资产。
- 先构建函数。该调用会在构建材质时加载资产，缺失时会
  [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219)。材质函数会在每次成功
  构建时保存，而命令行工具的 `-All` 会先编译每个 `.dsf`，然后才处理其他内容。
- 在其自己的 `.dsf` 内，`ShaderFunction` 通过其 `Name` 的最后一段来调用（`F_Tint`）；
  在其他任何地方，则通过 `VirtualFunction` 的 `Name` 调用。名称区分大小写 *(自 2.0.0 起)*；
  仅在大小写上匹配的调用会以警告 [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275) 解析。
- 参数可以是位置参数、命名参数，或先位置后命名 *(自 2.0.0 起；1.x 拒绝混合使用)*；
  命名参数之后的位置参数会 [`DSH2158`](../diagnostics/DSH2xxx.md#dsh2158)。调用中省略的输入
  必须是 `opt` 或有默认值，否则该调用会
  [`DSH4217`](../diagnostics/DSH4xxx.md#dsh4217)。
- `opt`——或 *(自 2.0.0 起)* 仅一个默认值——会使输入在生成的
  `UMaterialFunction` 上成为可选；其默认值驱动输入的预览值。
- 材质函数遵循 `Description`、`ExposeToLibrary` 和 `LibraryCategories`。
  *(自 2.0.0 起)* `ExposeToLibrary` 仅在 `LibraryCategories` 非空时生效，
  `UserExposedCaption` 不会被应用（警告 [`DSH3264`](../diagnostics/DSH3xxx.md#dsh3264)），而任何
  其他键都会被忽略并给出警告 [`DSH3263`](../diagnostics/DSH3xxx.md#dsh3263)。在 `Shader` 中，
  一个未命名任何材质属性的键会导致构建失败——参见
  [示例 13](#13-settings-domain-shading-model-blend-mode-backend)。

<a id="the-same-in-a-dss-8"></a>

### 在 `.dss` 中的相同内容

```hlsl
// DShader/Functions/F_Tint.dss
/// @desc Tint helper
/// @library Examples
/// @param Strength  Preview strength
export void F_Tint(float3 InColor, float Strength = 1.0, out float3 OutColor)
{
    OutColor = InColor * Strength;
}
```

```hlsl
// DShader/Materials/M_UsesTint.dss
/// @asset /Game/Functions/F_Tint
extern void F_Tint(float3 InColor, float Strength = 1.0, out float3 OutColor);

#pragma material(Domain = Surface, ShadingModel = Unlit)

uniform float3 Albedo = float3(0.6, 0.8, 1.0);

export void M_UsesTint(inout material m)
{
    float3 Tinted;
    F_Tint(Albedo, Strength = 0.5, OutColor = Tinted);
    m.EmissiveColor = Tinted;
}
```

不是材质入口的 `export` 函数是材质函数；默认参数会使
输入成为可选，`out` 参数是额外输出，而 `/// @library` 是库类别。
材质会通过由 `/// @asset` 绑定的 `extern` 原型调用该资产——即 2.0
的 `VirtualFunction`。

*另请参见：* [`ShaderFunction`](../language/shader-function.md) ·
[`VirtualFunction`](../language/virtual-function.md) ·
[`Inputs` / `Outputs`](../language/inputs-outputs.md) ·
[函数设置](../../settings/function.md) · [图表调用](../graph/calls.md)

---

<a id="10-shaderlayer--shaderlayerblend"></a>

## 10. `ShaderLayer` / `ShaderLayerBlend`

它们会生成原生 `UMaterialFunctionMaterialLayer` 和 `UMaterialFunctionMaterialLayerBlend`
资产 *(自 1.3.0 起)*，其接口由签名规则固定。

```c
// DShader/Layers/L_SimpleSurface.dsf
ShaderLayer(Name="Layers/L_SimpleSurface")
{
    Properties = {
        VectorParameter LayerColor = float4(0.8, 0.2, 0.1, 1.0) [Group="Layer"];
        ScalarParameter LayerRough = 0.5                        [Group="Layer"; Slider(0, 1)];
    }

    Outputs = {
        MaterialAttributes Attrs;
    }

    Graph = {
        Attrs.BaseColor = LayerColor.rgb;
        Attrs.Roughness = LayerRough;
    }
}

ShaderLayerBlend(Name="Layers/LB_Overlay")
{
    Properties = {
        ScalarParameter Alpha = 0.5 [Group="Blend"; Slider(0, 1)];
    }

    Inputs = {
        MaterialAttributes Bottom;
        MaterialAttributes Top;
    }

    Outputs = {
        MaterialAttributes Attrs;
    }

    Graph = {
        Attrs.BaseColor = lerp(Bottom.BaseColor, Top.BaseColor, Alpha);
        Attrs.Roughness = lerp(Bottom.Roughness, Top.Roughness, Alpha);
    }
}
```

| 模块 | 输入 | 输出 |
| :-- | :-- | :-- |
| `ShaderLayer` | 无，或一个 `MaterialAttributes`。*(自 2.0.0 起)* 该层始终获得恰好一个 `MaterialAttributes` 输入，以其输出命名——即它所应用到的材质 | 恰好一个 `MaterialAttributes` |
| `ShaderLayerBlend` | 仅 `MaterialAttributes`，至少一个 *(自 2.0.0 起；1.x：恰好两个)*；引擎的层堆栈混合两个 | 恰好一个 `MaterialAttributes` |

- 具有任何其他输入或输出的层会 [`DSH6204`](../diagnostics/DSH6xxx.md#dsh6204)；没有 `MaterialAttributes` 输入或具有其他输出的混合
  会 [`DSH6205`](../diagnostics/DSH6xxx.md#dsh6205)；其中任何一个若没有 `MaterialAttributes` 输出
  会 [`DSH3278`](../diagnostics/DSH3xxx.md#dsh3278)。层与混合控件属于 `Properties`，
  在那里它们成为所生成函数的参数。
- 层的输出起初就是传入的材质，因此 `L_SimpleSurface` 会改变它所应用到的材质的两个
  属性；混合的输出起初为空。
- 在 **UE 5.7+** 上，每个混合输入会根据其名称获得一个 `BlendInputRelevance`：`Top` / `TopLayer` 为
  `Top`，`Bottom` / `BottomLayer` / `Base` / `BaseLayer` 为 `Bottom`，而任何其他名称为 `Bottom`
  在第一个输入上，`Top` 在后续输入上。在更早的引擎上该属性不存在。
- `MaterialLayer(...)` / `MaterialLayerBlend(...)` 仍按旧拼写解析，并带有警告
  [`DSH2251`](../diagnostics/DSH2xxx.md#dsh2251) *(于 1.3.0 弃用)*。
- 这两个模块都不能从 `Graph` 调用 *(自 2.0.0 起)*：[`DSH6208`](../diagnostics/DSH6xxx.md#dsh6208)。
  它们的消费者是材质或材质实例的层堆栈。

<a id="the-same-in-a-dss-9"></a>

### 在 `.dss` 中同样

```hlsl
// DShader/Layers/L_SimpleSurface.dss
/// @group Layer
uniform float4 LayerColor = float4(0.8, 0.2, 0.1, 1.0);
/// @group Layer   @slider 0 1
uniform float LayerRough = 0.5;
/// @group Blend   @slider 0 1
uniform float Alpha = 0.5;

/// @layer
export void L_SimpleSurface(inout material m)
{
    m.BaseColor = LayerColor.rgb;
    m.Roughness = LayerRough;
}

/// @layerblend
export void LB_Overlay(material Bottom, material Top, inout material Attrs)
{
    Attrs.BaseColor = lerp(Bottom.BaseColor, Top.BaseColor, Alpha);
    Attrs.Roughness = lerp(Bottom.Roughness, Top.Roughness, Alpha);
}
```

一个文件、两个导出、两个资产：`/// @layer` 和 `/// @layerblend` 说明每一个是哪种类型。

*另见：* [`ShaderLayer` / `ShaderLayerBlend`](../language/shader-layer.md) ·
[MaterialAttributes](../graph/material-attributes.md) · [源文件](../language/source-files.md)

---

<a id="11-virtualfunction-wrapping-an-existing-asset"></a>

## 11. `VirtualFunction`：包装现有资产

`VirtualFunction` 不生成任何内容。它声明一个已存在的 `UMaterialFunction` 的接口，
以便 `Graph` 模块可以在类型检查下调用它。

```c
// DShader/VirtualFunctions/BufferWriter.dsh
VirtualFunction(Name="BufferWriter")
{
    Options = {
        Asset       = Path(Game, "MaterialFunctions/F_BufferWriter");
        Description = "Existing material function declared for Graph calls.";
    }

    Inputs = {
        float3 Color;
        opt float Alpha = 1.0;
    }

    Outputs = {
        float3 Result;
    }
}
```

```c
// DShader/Materials/M_Buffered.dsm
import "VirtualFunctions/BufferWriter.dsh";

Shader(Name="Materials/M_Buffered")
{
    Properties = { vec3 Tint = vec3(1.0, 0.4, 0.1); }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = BufferWriter(Color = Tint, Alpha = 0.5);
    }
}
```

> [!NOTE]
> 此示例引用了 `/Game/MaterialFunctions/F_BufferWriter`，一个项目资产。请替换为
> 你项目中存在的路径，或让编辑器为你编写声明：
> `UMaterialFunction` 右键菜单中的 **CreateVirtualFunction** 条目会在
> `DShader/VirtualFunctions` 下写入一个匹配的 `.dsh`，而 **Create extern Prototype (2.0)** 会写入
> `.dss` 所包含的 `extern` 形式。参见 [VirtualFunction tools](../../tools/virtual-function-tools.md)。

- 该资产可以来自 `Options = { Asset = … }` 或来自头属性
  `VirtualFunction(Name="…", Asset="…")`。没有其中之一时该模块为
  [`DSH6312`](../diagnostics/DSH6xxx.md#dsh6312)。
- `Settings` 是 `Options` 的可接受别名，而 `Properties` 是 `Inputs` 的可接受别名，
  仅在此模块内部。`Graph` 或 `Code` 部分为 [`DSH2247`](../diagnostics/DSH2xxx.md#dsh2247)。
- 至少需要一个输出（[`DSH6313`](../diagnostics/DSH6xxx.md#dsh6313)），并且 `Name` 必须
  是一个标识符（[`DSH6311`](../diagnostics/DSH6xxx.md#dsh6311)）。
- 在 `Graph` 调用它之前不会对照资产进行任何检查。此时，声明的输入或输出
  会按名称与资产的引脚匹配——先精确匹配，然后忽略大小写——两者都失败则按其
  位置匹配；匹配不到任何引脚的会 [`DSH8220`](../diagnostics/DSH8xxx.md#dsh8220) /
  [`DSH8221`](../diagnostics/DSH8xxx.md#dsh8221)，而无法加载的资产
  会 [`DSH8219`](../diagnostics/DSH8xxx.md#dsh8219)。
- `Path` 根：`Game`、`Engine`、`Plugin.<Name>` / `Plugins.<Name>`、完整对象路径，或裸的
  带引号 `"/Game/…"`。参见 [`VirtualFunction`](../language/virtual-function.md#the-asset-reference)。

<a id="the-same-in-a-dss-10"></a>

### 在 `.dss` 中同样

```hlsl
// DShader/VirtualFunctions/BufferWriter.dsh
/// @asset /Game/MaterialFunctions/F_BufferWriter
/// @desc Existing material function declared for Graph calls.
extern float3 BufferWriter(float3 Color, float Alpha = 1.0);
```

```hlsl
// DShader/Materials/M_Buffered.dss
#include "VirtualFunctions/BufferWriter.dsh"

#pragma material(Domain = Surface, ShadingModel = Unlit)

uniform float3 Tint = float3(1.0, 0.4, 0.1);

export void M_Buffered(inout material m)
{
    m.EmissiveColor = BufferWriter(Tint, Alpha = 0.5);
}
```

头文件是同一个声明，采用 2.0 拼写——一个 `extern` 原型；名为
`Result` 的第一个输出即其返回值，而默认参数即调用可以省略的输入。`.dsm`
像导入 1.x 一样导入此头文件，而 `.dss` 包含两者中任意一个。

*另见：* [`VirtualFunction`](../language/virtual-function.md) ·
[`Options`](../language/options.md) · [`Path(...)`](../parameters/path.md) ·
[VirtualFunction tools](../../tools/virtual-function-tools.md)

---

<a id="12-graphfunction-hoisting-a-ue-call-into-a-custom-node"></a>

## 12. `GraphFunction`：将 `UE.*` 调用提升到 Custom 节点中

`GraphFunction` 主体像 `Function` 一样是 HLSL，但其中的每个 `UE.*` 调用都作为
真实的材质节点求值，并作为额外输入引脚连接到生成的 Custom 节点。

```c
// DShader/Shared/Wind.dsh
GraphFunction WindPulse(in float2 uv, out float pulse) {
    float t = UE.Time();
    pulse = sin(uv.x * 8.0 + t);
}
```

```c
// DShader/Materials/M_Wind.dsm
import "Shared/Wind.dsh";

Shader(Name="Materials/M_Wind")
{
    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        vec2  UV    = UE.TexCoord(Index = 0);
        float Pulse = WindPulse(UV);          // value form: one out result
        Color = vec3(Pulse, Pulse, Pulse);
    }
}
```

生成的 Custom 节点代码，概要如下：

```hlsl
float pulse = (float)0;
// Begin DreamShader source: <file>
// DreamShader custom: WindPulse line <N>

    float t = _ds_WindPulse_UE0;
    pulse = sin(uv.x * 8.0 + t);
// End DreamShader source: <file>
return pulse;
```

- 生成的引脚名为 `_ds_<Function>_UE<N>`：基础名称 `__ds_<Function>_UE<N>` 会进行
  标识符清理，将其前导的 `__` 合并为一个 `_`，而与已声明的
  `in` 参数冲突的名称会获得 `_1`、`_2`、… 参见
  [`GraphFunction`](../language/graph-function.md#generated-input-pins)。
- *(自 2.0.0 起)* 被提升的调用在函数自身的作用域中绑定：其参数在每次
  调用时被该调用的实参替换，以及文件的声明。仅调用者拥有的名称会
  [`DSH6325`](../diagnostics/DSH6xxx.md#dsh6325)——1.x 读取调用者的 `Graph` 作用域。
- 仅 `UE.` 前缀会被提升。`Substrate.*` 调用保持为 HLSL 文本，并带有警告
  [`DSH6316`](../diagnostics/DSH6xxx.md#dsh6316)；对另一个 `Function` 的调用会嵌入
  节点中，如同 `Function` 主体。
- 被提升的值可以是数字、布尔值或 *(自 2.0.0 起)* 纹理对象；`MaterialAttributes`
  或 Substrate 值会 [`DSH6329`](../diagnostics/DSH6xxx.md#dsh6329)。
- `GraphFunction` 不接受 `SelfContained`/`Inline` 修饰符（[`DSH6307`](../diagnostics/DSH6xxx.md#dsh6307)）
  也不接受递归（[`DSH6260`](../diagnostics/DSH6xxx.md#dsh6260)，以及
  通过被提升调用的 [`DSH6330`](../diagnostics/DSH6xxx.md#dsh6330)）。它接受命名实参
  *(自 2.0.0 起)*。空主体被接受，其输出结果为 0。
- 语句形式也可用：在没有其他内容声明时，`WindPulse(UV, Pulse);` 会声明 `Pulse`（信息
  [`DSH5283`](../diagnostics/DSH5xxx.md#dsh5283)）。

<a id="the-same-in-a-dss-11"></a>

### 在 `.dss` 中同样

```hlsl
// DShader/Materials/M_Wind.dss
#pragma material(Domain = Surface, ShadingModel = Unlit)

/// @custom
void WindPulse(float2 uv, out float pulse)
{
    float t = UE.Time();
    pulse = sin(uv.x * 8.0 + t);
}

export void M_Wind(inout material m)
{
    float2 UV = UE.TexCoord(Index = 0);
    float Pulse;
    WindPulse(UV, Pulse);
    m.EmissiveColor = float3(Pulse, Pulse, Pulse);
}
```

`/// @custom` 函数同时是 `GraphFunction` 和 `Function`：其主体是逐字 HLSL，
其中的 `UE.` 调用同样会被提升为 Custom 节点的一个输入。

*另见：* [`GraphFunction`](../language/graph-function.md) · [`Function`](../language/function.md) ·
[生成的 HLSL](../generation/generated-hlsl.md) · [`UE.*` 目录](../builtins/ue.md)

---

<a id="13-settings-domain-shading-model-blend-mode-backend"></a>

## 13. 设置：域、着色模型、混合模式、后端

```c
// DShader/Materials/M_Glass.dsm
Shader(Name="Materials/M_Glass")
{
    Properties = {
        vec3  Tint    = vec3(0.7, 0.9, 1.0);
        float Opacity = 0.35;
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Translucent";
        TwoSided     = true;
        Wireframe    = false;
        Backend      = "Graph";
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.BaseColor = Color;
        Base.Opacity   = Alpha;
    }

    Graph = {
        Color = Tint;
        Alpha = Opacity;
    }
}
```

| 键 | 别名 | 值 |
| :-- | :-- | :-- |
| `MaterialDomain` | `Domain` | `Surface`, `DeferredDecal` / `Decal`, `LightFunction`, `Volume`, `PostProcess`, `UI` / `UserInterface`, `RuntimeVirtualTexture` / `VirtualTexture` |
| `ShadingModel` | — | `Unlit`, `DefaultLit` / `Lit`, `Subsurface`, `PreintegratedSkin`, `ClearCoat`, `SubsurfaceProfile`, `TwoSidedFoliage`, `Hair`, `Cloth`, `Eye`, `SingleLayerWater`, `ThinTranslucent`，以及在 UE 5.4+ 上的 `Substrate` / `Strata` |
| `BlendMode` | `RenderType` | `Opaque`, `Masked` / `Cutout`, `Translucent` / `Transparent`, `Additive`, `Modulate`, `AlphaComposite` / `PremultipliedAlpha` / `Premultiplied`, `AlphaHoldout`, `TranslucentColoredTransmittance` |
| `Backend` | — | `Graph`, `ThinCustom`；`Instance` 是 `ThinCustom` 的旧拼写，并带有警告 [`DSH7204`](../diagnostics/DSH7xxx.md#dsh7204) |

- 枚举值在匹配时会去除空格、`_` 和 `-`，且不区分大小写：`"Default Lit"`、
  `"DefaultLit"`、`"default_lit"` 和 `"DEFAULT-LIT"` 是同一个别名。每个设置值上的
  引号都是可选的。
- `MaterialDomain`、`ShadingModel` 和 `BlendMode` 通过上面的表解析；`Backend`
  和 *（自 2.0.0 起）* `Substrate` 是 DreamShader 自身的键。**其他所有键都会直接反映到
  `UMaterial` 上** —— 上面的 `TwoSided` 和 `Wireframe` 是真实的 `UMaterial` 属性。一个未指向
  任何属性的键会导致构建失败：[`DSH8215`](../diagnostics/DSH8xxx.md#dsh8215)，并携带
  [`DSH7118`](../diagnostics/DSH7xxx.md#dsh7118)。
- 同一个键写两次会触发警告 [`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262) *（自 2.0.0 起；
  在此之前为静默）*，且后出现的值生效。重复的 `Settings` 节会合并。
- 省略 `Backend` 会回退到项目的 **Default Compiler Backend**（随附为 `ThinCustom`）。
  使用 `Backend = "Graph"` 时，材质就是普通的 `UMaterial`，每次成功构建时
  都会保存。

> [!WARNING]
> `Backend = "";` 会引发错误 [`DSH7201`](../diagnostics/DSH7xxx.md#dsh7201) *（自 2.0.0 起；1.x 中将其
> 读取为 `Graph`）*，而任何不是后端的值都会引发 [`DSH7202`](../diagnostics/DSH7xxx.md#dsh7202)。
> 只有*省略*该键时才会回退到项目设置。

- 修改过的引擎会自动添加自己的着色模型 —— 反射表在运行时从
  `EMaterialShadingModel` 读取，而项目的映射表可以添加或覆盖拼写。

<a id="the-same-in-a-dss-12"></a>

### 在 `.dss` 中同样如此

```hlsl
// DShader/Materials/M_Glass.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Translucent)
#pragma material(TwoSided = true, Wireframe = false, Backend = Graph)

uniform float3 Tint = float3(0.7, 0.9, 1.0);
uniform float Opacity = 0.35;

export void M_Glass(inout material m)
{
    m.BaseColor = Tint;
    m.Opacity = Opacity;
}
```

`#pragma material` 接受与 `Settings` 相同的键和值，并且可以拆分为多行；
在其中同一个键设置两次会报错，[`DSH7200`](../diagnostics/DSH7xxx.md#dsh7200)。

*另请参阅：* [`Settings`](../../settings/index.md) · [着色器设置](../settings/material.md) ·
[材质枚举](../settings/material-enums.md) · [后端](../settings/backend.md) ·
[项目设置](../settings/project.md)

---

<a id="14-layout-placement-and-region"></a>

## 14. 布局放置与 `#Region`

`Layout` 将生成的节点固定到指定位置并绘制注释框。`#Region` / `#EndRegion`
将语句分组到一个 `Graph` 块中；每个区域在生成的图中都会变成一个注释框。

```c
// DShader/Materials/M_Laid.dsm
Shader(Name="Materials/M_Laid")
{
    Properties = {
        VectorParameter BaseColor = float4(0.8, 0.8, 0.8, 1.0) [Group="Surface"; SortPriority=10];
        ScalarParameter Roughness = 0.55                       [Group="Surface"; SortPriority=20];
        Texture2D       NoiseTex  = Path(Engine, "EngineResources/WhiteSquareTexture");
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        float3 Color;
        float  Rough;

        Base.BaseColor = Color;
        Base.Roughness = Rough;
    }

    Graph = {
        #Region "Sampling"
        vec2 UV    = UE.TexCoord(Index = 0);
        vec4 Noise = SampleTexture2D(NoiseTex, UV);
        #EndRegion

        #Region "Surface"
        Color = BaseColor.rgb * Noise.rgb;
        Rough = Roughness;
        #EndRegion
    }

    Layout = {
        Comment(Name="Sampling", X=-1200, Y=-200, W=900, H=400, Color=float4(0.10, 0.16, 0.22, 0.35));
        Comment(Name="Surface",  X=-1200, Y=260,  W=900, H=400);
        Node(Var="UV",    X=-1100, Y=-120);
        Node(Var="Noise", X=-760,  Y=-120);
    }
}
```

| 调用 | 必需参数 | 可选参数 |
| :-- | :-- | :-- |
| `Node` | `Var`（文本）、`X`、`Y`（整数） | — |
| `Comment` | `Name`（文本）、`X`、`Y`、`W`、`H`（整数） | `Color`（一个 `float4` 字面量） |

- `Var` 命名一个 `Graph` 变量。在 `Layout` 节中，除 `Node(…)` 和 `Comment(…)` 之外的任何内容都会触发
  [`DSH3274`](../diagnostics/DSH3xxx.md#dsh3274)；缺少参数或参数不是整数会触发
  [`DSH3275`](../diagnostics/DSH3xxx.md#dsh3275)，格式错误的 `Color` 会触发
  [`DSH3276`](../diagnostics/DSH3xxx.md#dsh3276)。
- 第二个 `Layout` 节会**替换**第一个，而不是追加，并带有警告
  [`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258) —— 这与 `Properties`、`Inputs` 和 `Outputs` 不同，
  后者会追加。
- `#Region` 名称可以带引号或裸写，指令匹配不区分大小写，且区域
  可以嵌套。没有名称的 `#Region` 会引发 [`DSH2216`](../diagnostics/DSH2xxx.md#dsh2216)，没有开启任何区域时的 `#EndRegion`
  会引发 [`DSH2217`](../diagnostics/DSH2xxx.md#dsh2217)，从未关闭的区域会引发
  [`DSH2218`](../diagnostics/DSH2xxx.md#dsh2218)。指令是一个词元，因此每条诊断都保留其
  真实行号和列号。
- 区域指令只在 `Graph` 主体中被读取；`Function` 或 `GraphFunction` 主体是 HLSL，
  按原样传递。
- `#Region` 名称与 `Layout` `Comment` 名称相互独立：`Sampling` 区域与
  上方的 `Sampling` 注释是两个框。
- 当“Export Decompiled Layout”（导出反编译布局）项目设置开启（默认）时，反编译器会把节点位置和自由注释框
  写回 —— 以 `#pragma layout` 行的形式，或在使用旧格式时以 `Layout` 节的形式。

> [!WARNING]
> 重新生成会清空目标图。未被 `Layout` 固定的节点位置、手动添加的节点、
> 节点属性调整，以及文本以 `DreamShader: ` 开头的注释框都会被销毁。只有
> 不含该前缀的注释框会保留。自生成以来被手动编辑过的资产根本不会
> 重建（[`DSH8207`](../diagnostics/DSH8xxx.md#dsh8207)）；请参阅
> [Divergence](../generation/divergence.md)。

<a id="the-same-in-a-dss-13"></a>

### 在 `.dss` 中同样如此

```hlsl
// DShader/Materials/M_Laid.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

/// @group Surface   @sort 10
uniform float4 BaseColor = float4(0.8, 0.8, 0.8, 1.0);
/// @group Surface   @sort 20
uniform float Roughness = 0.55;
/// @default /Engine/EngineResources/WhiteSquareTexture
uniform Texture2D NoiseTex;

#pragma layout(Comment, Name = "Sampling", X = -1200, Y = -200, W = 900, H = 400, Color = "0.10 0.16 0.22 0.35")
#pragma layout(Comment, Name = "Surface", X = -1200, Y = 260, W = 900, H = 400)
#pragma layout(Node, Var = "UV", X = -1100, Y = -120)
#pragma layout(Node, Var = "Noise", X = -760, Y = -120)

export void M_Laid(inout material m)
{
    #pragma region Sampling
    float2 UV = UE.TexCoord(Index = 0);
    float4 Noise = NoiseTex.Sample(UV);
    #pragma endregion

    #pragma region Surface
    m.BaseColor = BaseColor.rgb * Noise.rgb;
    m.Roughness = Roughness;
    #pragma endregion
}
```

`#pragma layout(…)` 是 2.0 的 `Layout` 语句，而 `#pragma region` / `#pragma endregion`
则是 2.0 的 `#Region` / `#EndRegion`。

*另请参阅：* [`Layout`](../language/layout.md) · [图布局](../generation/graph-layout.md) ·
[重新生成](../generation/regeneration.md) · [反编译器](../tools/decompiler.md)

---

<a id="15-runtime-virtual-texture-sampling-and-writing"></a>

## 15. 运行时虚拟纹理：采样与写入

RVT 节点没有专门的内置系列，也不需要。采样、替换和功能切换
节点都是通过 [`UE.<ClassName>`](../builtins/ue-expression.md) 访问的普通表达式；
`RuntimeVirtualTextureOutput` 是一个*自定义输出*，完全没有输出引脚，因此它绝不会是
`Graph` 赋值的右侧 —— 它在 `Outputs` 中通过
[`Expression( … ).Pin[i]`](../language/output-bindings.md#the-expression--pini-target) 进行绑定。

```c
// DShader/Materials/M_RVTSurface.dsm
Shader(Name="Materials/M_RVTSurface", Root="Game")
{
    Properties = {
        vec3 Fallback = vec3(0.5, 0.5, 0.5);
    }

    Settings = {
        Backend      = "Graph";
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        float3 SurfaceColor;
        float3 RVTBaseColor;
        float  RVTRoughness;

        Base.BaseColor = SurfaceColor;

        // Both statements reuse one RuntimeVirtualTextureOutput node.
        Expression(Class="RuntimeVirtualTextureOutput").Pin[0] = RVTBaseColor;
        Expression(Class="RuntimeVirtualTextureOutput").Pin[2] = RVTRoughness;
    }

    Graph = {
        float3 Sampled = UE.RuntimeVirtualTextureSample(
            OutputType     = "float3",
            Output         = "BaseColor",
            MaterialType   = "BaseColor_Normal_Roughness",
            VirtualTexture = Path(Game, "RVT/RVT_Terrain"));

        // Where the virtual texture has no page, fall back to the parameter ...
        float3 Blended = UE.RuntimeVirtualTextureReplace(
            OutputType = "float3", Default = Fallback, VirtualTextureOutput = Sampled);

        // ... and where virtual texturing is off entirely, fall back to it again.
        SurfaceColor = UE.VirtualTextureFeatureSwitch(
            OutputType = "float3", Yes = Blended, No = Fallback);

        RVTBaseColor = SurfaceColor;
        RVTRoughness = 0.5;
    }
}
```

> [!NOTE]
> 此示例引用了 `/Game/RVT/RVT_Terrain`，一个项目 `URuntimeVirtualTexture` 资产。
> 请替换为你项目中存在的资产。

| 节点 | `Class` 说明符 | 写法 |
| :-- | :-- | :-- |
| Runtime Virtual Texture Sample | `RuntimeVirtualTextureSample` | 在 `Graph` 中的 `UE.…` |
| Runtime Virtual Texture Sample Parameter | `RuntimeVirtualTextureSampleParameter` | 在 `Properties` 中的[参数声明](../../parameters/parameter-nodes.md) |
| Runtime Virtual Texture Replace | `RuntimeVirtualTextureReplace` | 在 `Graph` 中的 `UE.…` |
| Virtual Texture Feature Switch | `VirtualTextureFeatureSwitch` | 在 `Graph` 中的 `UE.…` |
| Runtime Virtual Texture Output | `RuntimeVirtualTextureOutput` | 在 `Outputs` 中的 `Expression( … ).Pin[i]` |

- `RuntimeVirtualTextureSample` 带有八个输出：`0` BaseColor、`1` Specular、`2` Roughness、
  `3` Normal、`4` WorldHeight、`5` Mask、`6` Displacement、`7` Mask4。可通过
  `Output="BaseColor"` 按名称选择其中一个输出，或通过 `OutputIndex=` 按位置选择；若没有选择器，该调用会被读取为一个
  带有多个输出的节点——参见[输出宽度](../builtins/ue.md#output-widths)。
- 每次调用都会去掉 `OutputType` *（自 2.0.0 起）*；目录为节点指定类型。
- `RuntimeVirtualTextureOutput` 输入引脚，按 `Pin[i]` 顺序：`0` BaseColor、`1` Specular、
  `2` Roughness、`3` Normal、`4` WorldHeight、`5` Opacity、`6` Mask、`7` Displacement、`8` Mask4。
- `VirtualTexture` 是反射的资产属性，因此它接受一个 [`Path(...)`](../parameters/path.md)
  引用——`Path(Game, …)`、`Path(Plugin.<Name>, …)`——或一个裸的带引号对象路径。`MaterialType`
  以及该节点的其他枚举和布尔属性由同一次反射过程写入，遵循
  通常的[枚举拼写](../settings/material-enums.md)规则。
- *写入* RVT 的材质保留 `Domain = "Surface"` 并添加输出节点，与
  上文完全相同。单独的 `Domain = "RuntimeVirtualTexture"` 拼写映射到 `MD_RuntimeVirtualTexture`，
  引擎将其标记为**已弃用并隐藏**；DreamShader 仍接受它，以便现有资产
  继续反编译，但新材质不应使用它。
- `Backend = "Graph"` 在此被固定，因此结果是一个普通的 `UMaterial`，在每次构建时保存，其
  节点可以打开并与源进行对比。节点本身不依赖于
  后端。

<a id="the-same-in-a-dss-14"></a>

### 在 `.dss` 中的相同内容

```hlsl
// DShader/Materials/M_RVTSurface.dss
#pragma material(Backend = Graph, Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

uniform float3 Fallback = float3(0.5, 0.5, 0.5);

export void M_RVTSurface(inout material m)
{
    float3 Sampled = UE.RuntimeVirtualTextureSample(
        MaterialType = "BaseColor_Normal_Roughness",
        VirtualTexture = "/Game/RVT/RVT_Terrain").BaseColor;
    float3 Blended = UE.RuntimeVirtualTextureReplace(Default = Fallback, VirtualTextureOutput = Sampled);
    float3 SurfaceColor = UE.VirtualTextureFeatureSwitch(Yes = Blended, No = Fallback);

    m.BaseColor = SurfaceColor;
    UE.Expression(Class = "RuntimeVirtualTextureOutput", Pin[0] = SurfaceColor, Pin[2] = 0.5);
}
```

输出通过成员访问（`.BaseColor`）选择，而资产属性接受带引号的对象
路径。自定义输出节点是入口的语句；`Pin[i] = …` 通过其
引擎索引连接输入 `i`。

*另请参阅：* [`UE.Expression`](../builtins/ue-expression.md) ·
[输出绑定](../language/output-bindings.md) · [`Path(...)`](../parameters/path.md) ·
[材质枚举](../settings/material-enums.md)

---

<a id="running-the-examples"></a>

## 运行示例

1. 将文件保存到 `<Project>/DShader` 下（或 `SourceDirectory` 指向的任何位置）。
2. 启用 *Auto Compile On Save* 时——这是默认设置——编辑器会在源保存后 0.25 秒编译它
   （*Save Debounce Seconds*）。`Graph` 后端的材质以及每个材质函数、层和
   混合在每次成功构建后都会保存到磁盘。`ThinCustom` 材质——默认后端——
   以 Ephemeral 方式在内存中构建，直到 cook、显式 *Materialize*、子材质实例
   或命令行工具为其提供一个包。
3. 无头编译单个文件：

```powershell
& "<Engine>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" `
  "<Project>/MyProject.uproject" `
  -run=DreamShader compile -Source="<Project>/DShader/Materials/M_Minimal.dsm" -Force `
  -unattended -nopause -nosplash -stdout -log
```

将 `-All` 替换为 `-Source=` 以编译项目的所有源：首先是每个 `.dsf`，然后
按路径顺序编译其他源；`.dsh` 通过包含它的文件进行编译。
命令行工具会实体化每个 `ThinCustom` 产物，因此它会为每个材质写入一个包。

*另请参阅：* [入门](../getting-started.md) ·
[Ephemeral 材质](../generation/ephemeral.md) · [命令行工具](../tools/commandlet.md) ·
[编辑器集成](../tools/editor-integration.md)

<a id="see-also"></a>

## 另请参阅

- [入门](../getting-started.md) — 安装与首次编译
- [DreamShaderLang](../language/index.md) — 1.x 声明语法，逐块讲解
- [DreamShaderLang 2.0](../language-v2/index.md) — 每个示例第二种形式的 `.dss` 语言
- [`dsc migrate`](../tools/migrate.md) — 将 1.x 源重写为 `.dss`，在写入之前先进行验证
- [图语言](../graph/index.md) — `Graph` 内部的语句和表达式语言
- [内置项](../builtins/index.md) — `UE.*`、`Substrate.*`、数学以及 HLSL 库
- [Custom Pass 示例](custom-pass.md) — 带有材质和着色器的 `.dsp` 管线
- [参数](../../parameters/index.md) — 每个参数标记、元数据键以及 `Path` 根
- [设置](../../settings/index.md) — 按文件和项目范围的配置
- [生成](../generation/index.md) — 源文件如何变成资产
- [工具](../../tools/index.md) — 编辑器集成、反编译器、命令行工具、桥接
- [诊断](../diagnostics/index.md) — 诊断出现的位置、如何阅读代码、1.x 消息
- [测试](../contributing/testing.md) — `.dss` 形式所参照的 `Tests/Corpus` 夹具
