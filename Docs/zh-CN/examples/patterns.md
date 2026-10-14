---
title: "常用写法"
description: "DreamShaderLang 的文件级构件 —— 最小材质、参数、纹理采样、共享头文件、函数、UE.* 节点、Layer、VirtualFunction、GraphFunction 与 Layout，每一段都是完整源文件，1.x 写法旁边并列 .dss 写法。"
---

本页的每一段都是一个完整文件或一个完整块，短到可以直接粘贴改名。顺序从"能产出资产的最小文件"开始，
一直排到需要 `.dsh` 头文件、`.dsf` 函数文件或特定引擎版本的写法。每段 1.x 写法后面都跟着同一个东西的 `.dss` 写法。

| | |
| :-- | :-- |
| 适用于 | DreamShaderLang 1.x（`.dsm`、`.dsf`、`.dsh`），由 2.0 编译器经 legacy 前端构建（自 2.0.0 起）；`.dss` 写法是 [DreamShaderLang 2.0](../language-v2/index.md) |
| 引擎 | UE 5.3 – 5.8；有版本门槛的都在正文里标出 |
| 假定源码根目录 | `<Project>/DShader` —— 即 `SourceDirectory` [项目设置](../settings/project.md) |
| 生成产物 | `Graph` backend 的材质和每个材质函数、layer、blend：每次构建成功都存盘。`ThinCustom` 材质 —— 默认 backend —— 保持 Ephemeral，只在内存里，直到被 Materialize；见[内存材质](../generation/ephemeral.md) |

每段开头注释里的路径就是该文件假定所在的位置，`import` 和 `#include` 的路径按这个布局解析。用 `Path(Engine, …)`
或 `/Engine/…` 引用的资产随引擎自带，所以那几个例子照抄就能加载；引用项目资产的那个（[VirtualFunction](#virtualfunction-wrapping-an-existing-asset)）已经标出。

注释里提到的诊断都写码。编译把每一条报成 `<file>(<line>,<col>): DSHnnnn: <message>`，位置就是它所说的那个构造；码怎么读见[错误速查](../diagnostics/index.md)。

每段下面的「*写成 `.dss`*」是同一个文件用 DreamShaderLang 2.0 手写的样子：`#pragma material(…)` 换成
`Settings`，每个参数是一个带 `///` 标签的 `uniform`，再加一个名字就是资产名的 `export` 函数。
`.dss` 把它的 export 构建在文件自己所在的文件夹里，所以这里每个都构建出和旁边 1.x 文件同一个资产路径 —— 两者留一个，别都留。
[`dsc migrate`](../tools/migrate.md) 会写出一个 1.x 文件的 `.dss` 并证明两者构建出同一张图；它的输出比这里的长，
因为它保留 1.x 的每个细节（每个参数上的 `/// @sort 32`、1.x 给每个输出的零初始值）。

本页讲的是*形状*。想要能跑出效果的完整材质，见[完整示例](recipes.md)。

<a id="a-minimal-material"></a>

## 最小材质

能产出资产的最小文件：一个参数、一条绑定、一次赋值。

```c
// DShader/Materials/M_Minimal.dsm
Shader(Name="Materials/M_Minimal")
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

- `Shader` **区分大小写**；section 名 `Properties`、`Settings`、`Outputs`、`Graph` 不区分。
  见[词法与大小写](../language/lexical.md#case-sensitivity)。
- section 名后的 `=` 是可选糖（自 1.5.0 起）：`Properties { … }` 解析结果完全一样。section
  体内最后一个 `;` 同样可省。
- `Root` 默认是 `/Game`，所以头部写成 `Shader(Name="Materials/M_Minimal", Root="Game")` 含义不变。
  完整规则见[资产路径](../generation/asset-paths.md)。
- 一个 `Shader` 既没有 `Graph` section、`Outputs` 里也没有东西时报 `DSH2255`。没有 `Graph` 时，由 `Outputs`
  自己算值 —— 带初始化式的输出声明，或者（自 2.0.0 起）右边是表达式的绑定。没有 `Outputs` section 的 `Shader`
  照样构建，只是什么都没接上，并给出 warning `DSH2256`（自 2.0.0 起）。

<a id="written-as-a-dss"></a>

### 写成 `.dss`

```hlsl
// DShader/Materials/M_Minimal.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

uniform float3 Tint = float3(1.0, 0.2, 0.2);

export void M_Minimal(inout material m)
{
    m.EmissiveColor = Tint;
}
```

签名 `export void <Name>(inout material m)` 让这个文件成为一个材质；1.x 的 `Outputs` 绑定就是对 `m` 某个成员的写入。

<a id="parameters-groups-and-metadata"></a>

## 参数、分组与元数据

一个文件里覆盖所有参数族，带 `Group("…")` 作用域、`[ … ]` 元数据和 `Slider(min, max)` 简写。

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

- `float` / `vec3` / `Texture2D` 是**紧凑**写法；`ScalarParameter`、`VectorParameter`、
  `TextureSampleParameter2D` 则直接点名 Unreal 节点。两套写法都收录在
  [Properties 类型](../language/properties.md)。
- `Group("…") { … }` 作用域（自 1.5.0 起）会把组名盖到里面的参数上，并从**每个 `Properties` section 一个计数器**
  按 `0, 10, 20, …` 编号；显式写的 `SortPriority` 优先，且不占号。
  这里 `Roughness` 是 0，`Albedo` 是 10。嵌套的组用 `|` 拼接（`Outer|Inner`）。
- （自 2.0.0 起）组名会盖到标量、向量、纹理和 static bool 参数上。带 pin 的参数和开关参数 —— `DetailMap`、
  `UseDetail` —— 在每次使用处写成一个节点，那个节点只带它自己 `[ … ]` 里的元数据：`DetailMap` 得到 `SortPriority = 99`、
  没有组，`UseDetail` 两样都没有。
- `const` 声明的是谁也覆盖不了的值 —— 编译期常量，不是参数。它接受标量、向量或纹理类型；
  其他类型是 `DSH3253`。

**警告：**

**裸读一个 `StaticSwitchParameter` 不是值。** `Color = UseDetail;` 报 `DSH4200`。用 `True=`
和 `False=`（或 `A=` / `B=`，或按位置）调用它；缺一边是 `DSH5258`，别的参数被丢掉并给出 warning `DSH5254`。

<a id="written-as-a-dss-1"></a>

### 写成 `.dss`

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

- 纹理 `uniform` 不写初始化式：它的资产写在 `/// @default`，采样器类型写在 `/// @sampler`。用
  `.Sample(UV)` 采样时它建出一个 `TextureObjectParameter` 加一个 `TextureSample`，而 1.x 的 token 建的是一个
  `TextureSampleParameter2D`；采样器来源取节点的默认值 *From Texture Asset*。
- 被 `if` 读取的 `/// @static` `uniform bool` 就是静态开关。`/// @group` 和 `/// @sort` 写每个
  参数的组和优先级；`static const` 就是 1.x 的 `const`。

<a id="two-ways-to-sample-a-texture"></a>

## 采样纹理的两种写法

上面那两个 1.x 纹理声明的采样方式不同，而且这个区别不是写法偏好问题。

| 声明 | 生成的节点 | 采样方式 |
| :-- | :-- | :-- |
| `Texture2D NoiseTex = Path(…);` | `TextureObjectParameter` —— 纹理*对象*，没有输入 pin | `SampleTexture2D(NoiseTex, UV)` |
| `TextureSampleParameter2D DetailMap = Path(…);` | `TextureSampleParameter2D` —— 自带 `Coordinates` pin | `DetailMap(Coordinates = UV)` |

**警告：**

**pin 调用形式 `Name(Pin = …)` 只属于十个参数 token**，它们的节点自带输入 pin ——
`ChannelMaskParameter`、`StaticComponentMaskParameter`，以及八个 `*SampleParameter*` token —— 并且每个参数
都要带名字（否则 `DSH5259`）。纹理**对象**参数（紧凑的 `Texture2D`，或 `TextureObjectParameter`）
没有 pin、也不是函数，所以 `NoiseTex(Coordinates = UV)` 报 `DSH4208`。

采样纹理对象请改用保留写法 `SampleTexture2D(textureObject, uv)`。它
**区分大小写**，并且只接受两个位置参数（否则 `DSH5256`）。在 `.dss` 里两种都写成
`Tex.Sample(UV)`。

<a id="a-shared-dsh-header"></a>

## 共享 `.dsh` 头文件

`.dsh` 头文件包含 `Function`、`GraphFunction`、`Namespace` 和 `VirtualFunction` 块、`import` 行以及 2.0
声明 —— 没有资产块（`DSH2249`）。它不生成任何资产。它被单独解析，其声明会成为
每个 import 它的文件中的名字（自 2.0.0 起）（1.x 会把它的文本粘贴进 import 它的文件）。

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

引入它：

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

- `import "Shared/Common"` 等价：当说明符**完全不带扩展名**时，会自动追加 `.dsh`。只有
  `.dsh` 才能被 import（自 2.0.0 起）：`.dsf` 或 `.dsm` 路径会报 `DSH2252`。
- 相对说明符会依次尝试 import 它的文件所在目录、它所属源码根的目录
  （`DShader`），然后是该根的 `Packages` —— 从不尝试另一个根的。解析到其根之外的候选会被
  跳过。参见 [Imports 与命名空间](../language/import.md#resolution)。
- （自 2.0.0 起）`import` 在文件作用域内作为 token 读取：`;` 是可选的，`'single quotes'` 在 1.x
  文件中也能用，而 `/* … */` 注释内的 `import` 会被注释掉（1.x 会处理它）。不过仍请让每条 `import`
  独占一行：编辑器的依赖扫描器在保存头文件时会重编 import 它的文件，它仍然
  逐行读取。
- `Function` 函数体是 HLSL，不是 `Graph` 语句：这里的 `dot`、`saturate` 和 `float3(…)` 是 HLSL 内建函数。GLSL
  拼写会在这些函数体内被改写，不区分大小写且不给诊断（`vec3` → `float3`、
  `mix` → `lerp`、`fract` → `frac`、`mod` → `fmod`）。
- （自 2.0.0 起）每个 `Function` 节点都会嵌入其函数体调用的 `Function`，而 1.x 只对
  `SelfContained` 这样做，所以 `SelfContained` 不改变任何东西。`Inline` 是它的旧拼写，带 warning `DSH6306`；
  用在 `GraphFunction` 上两者都是 `DSH6307`。
- （自 2.0.0 起）在另一个 `Function` 或 `GraphFunction` 函数体内使用带命名空间的调用是可以的：函数体
  规范化会把 `Common::ApplyTint` 拍平为 `Common_ApplyTint`，这正是那个函数自己的名字，被调方
  会被嵌入调用方的节点。1.x 会给 shader 编译器留下一个未定义符号。请书写 `::` 时
  不要在它两边加空格。

<a id="written-as-a-dss-2"></a>

### 写成 `.dss`

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

`.dsh` 同时容纳两种方言，所以 `.dss` 会按原样 include 这个头文件。带命名空间的函数按它声明的名字
调用，即 `Common_ApplyTint`，而 `out` 实参是先声明好的变量。头文件本身的 2.0
写法如下 —— `.dsm` 同样能 import 它：

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

`/// @custom` 会原样保留函数体、并让每次调用成为一个 Custom 节点，如同 1.x 的 `Function`；`/// @name` 保留
节点标题 `Common::ApplyTint`。

<a id="calling-functions-value-form-and-statement-form"></a>

## 调用函数：值形式与语句形式

函数的值是它的返回值，或者它的第一个 `out`。语句形式先传输入，再给每个输出一个目标；
其尾部实参就是接收结果的变量名。

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

        // value form: the return value, or the first out
        float L    = Common::Luma(Tinted);
        float Soft = Remap01(L);

        Color = Tinted * Soft;
        Alpha = A;
    }
}
```

| 被调用者种类 | 值形式 | 语句形式 | 命名参数 |
| :-- | :-- | :-- | :-- |
| `Function` | 可以 —— 返回值，或第一个 `out`（自 2.0.0 起）（1.x：只能一个结果） | 可以 | 可以（自 2.0.0 起） |
| `GraphFunction` | 同 `Function` | 可以 | 可以（自 2.0.0 起） |
| `ShaderFunction` | 可以 —— 第一个输出，除非用选择器挑别的；只能在它自己的文件里调用 | 可以 | 可以 |
| `ShaderLayer` / `ShaderLayerBlend` | **不能调用**：`DSH6208`（自 2.0.0 起） | —— | —— |
| `VirtualFunction` | 可以 | 可以 | 可以 |

- 没人声明过的 out 目标会由调用方声明，成为输出类型的局部变量（info `DSH5283`）。目标必须
  可赋值 —— 一个变量，或者它的成员或 swizzle（`DSH4239`）—— 并且类型和输出一致（`DSH4218`）。
- 在全按位置的语句调用里，最后几个实参依声明顺序各接收一个输出，一个对一个；它们
  之前的是输入。实参太多是 `DSH4224`，而漏掉一个非 `opt` 的输入是 `DSH4217`。
- 函数名**区分大小写**（自 2.0.0 起）：只靠大小写对上的调用仍能
  解析，带 warning `DSH5275`。**没有重载解析** —— 一个名字只声明一样东西，对它再做一次
  声明会在写的地方报 `DSH4210`。
- [数学内建函数](../builtins/math.md)（`lerp`、`dot`、`pow`、`min`、`max`、`clamp`、`saturate`、`sin`、`frac`、
  `fmod`……）先于用户函数解析；和它们同名的函数会在声明处报 `DSH6206`。`mix`、
  `fract` 和 `mod` 是它们的 GLSL 拼写，在 1.x `Graph` 中被接受，带 warning `DSH5277`。

<a id="written-as-a-dss-3"></a>

### 写成 `.dss`

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

`.dss` 会把一次调用的每个 `out` 传给一个它先声明好的变量。对没有返回类型的函数做 1.x 值调用
即 `Remap01(L)`，是一条旧规则；`dsc migrate` 会把它写成上面的语句形式。

<a id="ue-nodes-and-a-generic-ueexpression"></a>

## `UE.*` 节点与通用 `UE.Expression`

`UE.<Name>(…)` 会创建一个 `UMaterialExpression`。`<Name>` 是节点目录中的一个类 —— 任何引擎表达式类
用其短名（`Sine`、`CameraVectorWS`），或者目录携带的 1.x 别名之一（`TexCoord`、
`TransformVector`……）。`UE.Expression(Class = …)` 则改为在参数里写类名；两者是同一条路径
（自 2.0.0 起）。

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

- `Class="Sine"`、`"MaterialExpressionSine"`、`"UMaterialExpressionSine"`（自 2.0.0 起）和
  `"/Script/Engine.MaterialExpressionSine"` 都解析到同一个类。引号可省：`Class=Sine` 和
  `OutputType=float1` 效果一样。
- 参数名先匹配类的输入 pin，再匹配它的一个属性；同名时 pin 优先。pin 接一个
  表达式；属性接一个字面量（运行时才算出的值是 `DSH4373`）。
- 不带 `Class=` 的 `UE.Expression(…)` 是 `DSH5218`。
- `OutputType`（别名 `ResultType`）不再是必需的（自 2.0.0 起）：legacy 前端会把它从
  调用上拿掉，由目录给节点定类型。它仍然决定 `Custom` 节点的输出类型；在其他地方，一个数值 token 只
  给出单输出节点值的宽度，其余的被丢弃。参见
  [`UE.Expression`](../builtins/ue-expression.md)。

**警告：**

对 [`UE.*` 目录](../builtins/ue.md)里 1.x 的 27 个名字 —— `TexCoord`、`Time`、`CameraVectorWS`、
`TransformVector`…… —— legacy 前端保留 1.x 读的那些参数，其余的 —— 未知名字或
位置参数 —— 一律丢掉，带 warning `DSH5254`（自 2.0.0 起）（1.x 一声不吭地丢掉）。`UE.Time(Bogus = 1)`
和 `UE.TexCoord(0)` 照样构建，并说明原因。唯一保留的位置参数是 `UE.TransformVector` /
`UE.TransformPosition` 的第一个（→ `Input`）。在其他任何节点上，位置参数只在目录给了该类
参数顺序时才绑定，否则是 `DSH5220`。

<a id="written-as-a-dss-4"></a>

### 写成 `.dss`

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

`.dss` 不受 legacy 过滤：节点没有的参数是 `DSH5213`，不是警告。

<a id="a-shaderfunction-in-a-dsf"></a>

## 放在 `.dsf` 里的 `ShaderFunction`

`.dsf` 声明函数资产——`ShaderFunction`、`ShaderLayer`、`ShaderLayerBlend`——旁边可以放 helper 块
（`Function`、`GraphFunction`、`Namespace`、`VirtualFunction`）。只有 `.dsh` 能被 import，所以另一个文件里的材质触达生成的函数，
和触达任何已有资产一样：通过 `VirtualFunction`（自 2.0.0 起）。
和函数在同一个 `.dsm` 里的材质直接按名字调用它。

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

从材质里调用：

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

- （自 2.0.0 起）1.9.x 及以前，材质 import 函数文件——`import "Functions/F_Tint.dsf";`——一次编译构建两个资产。这条 import 现在是
  `DSH2252`；`VirtualFunction` 声明 `F_Tint.dsf` 构建的那个资产，两个文件各自构建自己的。
- 先构建函数。调用在构建材质时加载资产，找不到是 `DSH8219`。材质函数每次构建成功都存盘，commandlet 的 `-All`
  先编译所有 `.dsf`。
- 在它自己的 `.dsf` 里，`ShaderFunction` 按 `Name` 的最后一段（`F_Tint`）调用；在别处按
  `VirtualFunction` 的 `Name` 调用。名字区分大小写（自 2.0.0 起）；只靠大小写对上的调用仍能解析，带 warning
  `DSH5275`。
- 实参可以全按位置、全命名，或先位置后命名（自 2.0.0 起）（1.x 拒绝混用）；命名参数之后再出现位置参数是 `DSH2158`。
  调用里省略的输入必须是 `opt` 或有默认值，否则是 `DSH4217`。
- `opt`——或者（自 2.0.0 起）单独一个默认值——是让输入在生成的 `UMaterialFunction` 上变成可选的东西；
  它的默认值驱动该输入的预览值。
- 材质函数认 `Description`、`ExposeToLibrary` 和 `LibraryCategories`。（自 2.0.0 起）
  `ExposeToLibrary` 只在 `LibraryCategories` 非空时生效，`UserExposedCaption` 不被应用
  （warning `DSH3264`），其他键被忽略并给出 warning `DSH3263`。而在 `Shader` 里，不是材质属性的键
  会让构建失败。

<a id="written-as-a-dss-5"></a>

### 写成 `.dss`

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

不是材质入口的 `export` 函数就是材质函数；默认参数让输入可选，`out` 参数是额外的输出，
`/// @library` 是库分类。材质通过一个由 `/// @asset` 绑定的 `extern` 原型调用这个资产——那就是 2.0 的
`VirtualFunction`。

<a id="shaderlayer-and-shaderlayerblend"></a>

## `ShaderLayer` 与 `ShaderLayerBlend`

这两个块生成原生的 `UMaterialFunctionMaterialLayer` 和 `UMaterialFunctionMaterialLayerBlend` 资产
（自 1.3.0 起），接口形状由固定的签名规则约束。

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

| 块 | 输入 | 输出 |
| :-- | :-- | :-- |
| `ShaderLayer` | 没有，或一个 `MaterialAttributes`。（自 2.0.0 起）layer 总是恰好得到一个 `MaterialAttributes` 输入，以它的输出命名——也就是它所作用的那个材质 | 恰好一个 `MaterialAttributes` |
| `ShaderLayerBlend` | 只能是 `MaterialAttributes`，至少一个（自 2.0.0 起）（1.x：恰好两个）；引擎的图层栈混合两个 | 恰好一个 `MaterialAttributes` |

- 有其他输入或输出的 layer 是 `DSH6204`；没有 `MaterialAttributes` 输入、或有其他输出的 blend `DSH6205`；两者没有
  `MaterialAttributes` 输出都是 `DSH3278`。Layer 和 blend 的*控制项*放 `Properties`，在那里它们成为生成函数的参数。
- layer 的输出从进来的那个材质开始，所以 `L_SimpleSurface` 改的是它所作用材质的两个属性；blend 的输出从空开始。
- 在（自 UE 5.7 起）上，每个 blend 输入按名字得到一个 `BlendInputRelevance`：`Top` / `TopLayer` 是 `Top`，
  `Bottom` / `BottomLayer` / `Base` / `BaseLayer` 是 `Bottom`，其他名字第一个输入是 `Bottom`、
  后面的是 `Top`。更早的引擎上没有这个属性。
- 两个块都不能从 `Graph` 调用（自 2.0.0 起）：`DSH6208`。它们的使用者是材质或材质实例的图层栈。

**自 1.3.0 起弃用。**请改用 `ShaderLayer`。

`MaterialLayer(...)` 和 `MaterialLayerBlend(...)` 仍然作为旧拼写解析，生成的资产也完全相同，
但会给出 warning `DSH2251`。

<a id="written-as-a-dss-6"></a>

### 写成 `.dss`

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

一个文件、两个 export、两个资产：`/// @layer` 和 `/// @layerblend` 说明各自是哪一种。

<a id="virtualfunction-wrapping-an-existing-asset"></a>

## `VirtualFunction`：包装已有资产

`VirtualFunction` 不生成任何东西。它声明一个**已经存在**的 `UMaterialFunction` 的接口，好让
`Graph` 块能带类型检查地调用它。

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

**提示：**

这个例子引用的 `/Game/MaterialFunctions/F_BufferWriter` 是项目资产。请换成你项目里真实存在的路径——
或者让编辑器帮你写：`UMaterialFunction` 右键菜单里的 **CreateVirtualFunction** 会在 `DShader/VirtualFunctions` 下写一个
对应的 `.dsh`，**Create extern Prototype (2.0)** 写出 `.dss` include 用的 `extern` 形式。
见[编辑器工具](../tools/editor-integration.md)。

- 资产可以来自 `Options = { Asset = … }`，也可以来自头部属性 `VirtualFunction(Name="…", Asset="…")`。两者都没有是 `DSH6312`。
- 仅在这个块内，`Settings` 是 `Options` 的别名，`Properties` 是 `Inputs` 的别名。`Graph` 或
  `Code` section 是 `DSH2247`。
- 至少要有一个输出（`DSH6313`），`Name` 必须是标识符（`DSH6311`）。
- 在有 `Graph` 调用它之前，什么都不会对照资产检查。调用时，声明的输入和输出按名字匹配资产的 pin——先精确、
  再忽略大小写——两者都落空就按位置；什么都匹配不上是 `DSH8220` / `DSH8221`，资产加载不到是 `DSH8219`。
- `Path` 的根：`Game`、`Engine`、`Plugin.<Name>` / `Plugins.<Name>`、完整对象路径，或者裸的带引号 `"/Game/…"`。
  见 [Path 资产引用](../parameters/path.md)。

<a id="written-as-a-dss-7"></a>

### 写成 `.dss`

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

头文件是同一个声明的 2.0 写法——一个 `extern` 原型；名为 `Result` 的第一个输出就是它的返回值，
默认参数是调用可以省略的输入。`.dsm` import 这个头文件和 import 1.x 那个一样，
`.dss` 两个都能 include。

<a id="graphfunction-hoisting-a-ue-call-into-a-custom-node"></a>

## `GraphFunction`：把 `UE.*` 调用提升进 Custom 节点

`GraphFunction` 的体和 `Function` 一样是 HLSL，但里面每个 `UE.*` 调用都会被当作真正的材质节点求值，
并作为额外输入 pin 接到生成的 Custom 节点上。

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

生成的 Custom 节点代码，大致形状：

```hlsl
float pulse = (float)0;
// Begin DreamShader source: <file>
// DreamShader custom: WindPulse line <N>

    float t = _ds_WindPulse_UE0;
    pulse = sin(uv.x * 8.0 + t);
// End DreamShader source: <file>
return pulse;
```

- 生成的 pin 名为 `_ds_<Function>_UE<N>`：基础名 `__ds_<Function>_UE<N>` 经过标识符净化，
  其开头的 `__` 收成一个 `_`，与已声明的 `in` 参数重名的名字会得到 `_1`、`_2`……
- （自 2.0.0 起） 被提升的调用在函数自己的作用域里绑定：它的参数（每次调用时换成那次调用的实参）和文件的声明。只有调用方
  才有的名字是 `DSH6325` —— 1.x 读的是调用方的
  `Graph` 作用域。
- 只提升 `UE.` 前缀。`Substrate.*` 调用留作 HLSL 文本，带 warning `DSH6316`；对另一个
  `Function` 的调用被嵌进节点，和 `Function` 体里一样。
- 被提升的值可以是数字、bool，或者 （自 2.0.0 起） 纹理对象；`MaterialAttributes` 或 Substrate
  值是 `DSH6329`。
- `GraphFunction` 不接受 `SelfContained` / `Inline` 修饰符（`DSH6307`），也不允许递归（`DSH6260`，以及经由提升出来的调用
  的 `DSH6330`）。它接受命名参数 （自 2.0.0 起）。空函数体可以，它的输出是 0。
- 语句形式同样可用：没有别处声明时，`WindPulse(UV, Pulse);` 会声明 `Pulse`（info `DSH5283`）。

<a id="written-as-a-dss-8"></a>

### 写成 `.dss`

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

`/// @custom` 函数同时就是 `GraphFunction` 和 `Function`：它的体是原样的 HLSL，里面的 `UE.` 调用
同样被提升为 Custom 节点的输入。

<a id="layout-and-region"></a>

## `Layout` 与 `#Region`

`Layout` 把生成的节点钉在固定位置并画注释框。`#Region` / `#EndRegion` 在 `Graph` 块内
给语句分组；每个 region 在生成图里变成一个注释框。

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

| 调用 | 必填参数 | 可选 |
| :-- | :-- | :-- |
| `Node` | `Var`（文本）、`X`、`Y`（整数） | — |
| `Comment` | `Name`（文本）、`X`、`Y`、`W`、`H`（整数） | `Color`（一个 `float4` 字面量） |

- `Var` 指的是一个 `Graph` 变量。``Layout`` section 里除 `Node(…)` 和 `Comment(…)` 之外的东西是 `DSH3274`；缺参数
  或参数不是整数是 `DSH3275`，`Color` 写坏了是 `DSH3276`。
- 第二个 `Layout` section 会**替换**第一个，而不是追加，并给出 warning `DSH2258` —— 这和会追加的
  `Properties`、`Inputs`、`Outputs` 不同。
- `#Region` 名可以带引号也可以裸写；指令不区分大小写，region 可以嵌套。没名字的 `#Region` 是 `DSH2216`，没有打开的
  region 却写了 `#EndRegion` 是 `DSH2217`，一直没关的 region 是 `DSH2218`。指令是一个
  token，所以每条诊断都保留真实的行列。
- region 指令只在 `Graph` 体里读取；`Function` 或 `GraphFunction` 体是 HLSL，原样传下去。
- `#Region` 名和 `Layout` 的 `Comment` 名互不相干：上面的 `Sampling` region 和 `Sampling` 注释
  是两个框。
- *Export Decompiled Layout* 项目设置开着时（默认开），反编译器会写回节点位置和独立的注释框 —— 写成 `#pragma layout` 行，
  或者 legacy 格式下的 `Layout` section。

**警告：**

重新生成会清空目标图。没有被 `Layout` 钉住的节点位置、手加的节点、手改的节点属性，以及文本以
`DreamShader: ` 开头的注释框，都会被销毁。只有不带这个前缀的注释框能保留。生成之后被手改过的
资产根本不会被重建（`DSH8207`）；见
[重新生成](../generation/regeneration.md)。

<a id="written-as-a-dss-9"></a>

### 写成 `.dss`

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

`#pragma layout(…)` 是 2.0 的 `Layout` 语句，`#pragma region` / `#pragma endregion` 是 2.0 的 `#Region` /
`#EndRegion`。

<a id="importing-from-a-package"></a>

## 从 package 引入

package 就是 `DShader/Packages` 下的一个目录。它的头文件 import 起来和项目文件完全一样，因为
`DShader/Packages` 正是第三个解析根。

```text
<Project>/DShader/
  Materials/
    M_Noisy.dsm
  Packages/
    @typedreammoon/
      dream-noise/
        Library/
          Noise.dsh
```

```c
// DShader/Materials/M_Noisy.dsm
import "@typedreammoon/dream-noise/Library/Noise.dsh";
```

`.dss` 写成 `#include "@typedreammoon/dream-noise/Library/Noise.dsh"`，解析规则相同。说明符会依次尝试每个根：

```text
specifier   @typedreammoon/dream-noise/Library/Noise.dsh
candidate 1 <Project>/DShader/Materials/@typedreammoon/dream-noise/Library/Noise.dsh   missing
candidate 2 <Project>/DShader/@typedreammoon/dream-noise/Library/Noise.dsh             missing
candidate 3 <Project>/DShader/Packages/@typedreammoon/dream-noise/Library/Noise.dsh    resolved
```

这里同样可以省掉扩展名 —— `import "@typedreammoon/dream-noise/Library/Noise";` 是同一条 import。哪个根都解析不到是
`DSH8292`。package 的编写方式、清单文件和编辑器工具见
[Package](../tools/packages.md)。

<a id="where-to-next"></a>

## 接下来

  - [完整示例](recipes.md) — 用这些形状拼出来的完整材质。
  - [Custom Pass 示例](custom-pass.md) — .dsp 管线，连同它们的材质和 shader。
  - [DreamShaderLang 2.0](../language-v2/index.md) — 每段第二种写法所用的 .dss 语言。
  - [dsc migrate](../tools/migrate.md) — 把 1.x 源文件改写成 .dss，写盘之前先证明。
  - [顶层块](../language/blocks.md) — 全部七个块，以及各自生成什么。
  - [调用](../graph/calls.md) — 完整调用语法：形式、实参、输出选择。
