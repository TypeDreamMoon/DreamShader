---
title: 完整示例
description: 可直接编译的完整 DreamShaderLang 材质 —— 动画着色、UV 平移、分支、静态开关变体、半透明 UI、PBR、MaterialAttributes、Substrate、参数集合、多输出 helper、Settings 一览与 Runtime Virtual Texture，每个都有 .dsm 和 .dss 两种写法。
---

下面每个配方都是一个能独立编译的完整 `.dsm`，后面跟着同一个材质的 `.dss` 写法。它们按"参数、设置、输出、图"的顺序写，
方便从上往下读。凡是有坑的地方，都直接写在代码旁边，而不是留给你自己踩。

| | |
| :-- | :-- |
| 适用于 | DreamShaderLang 1.x（`.dsm`），由 2.0 编译器经 legacy 前端构建 （自 2.0.0 起）；`.dss` 写法是 [DreamShaderLang 2.0](../language-v2/index.md) |
| 引擎 | UE 5.3 – 5.8；Substrate 那个配方需要 （自 UE 5.4 起） |
| 假定源码根目录 | `<Project>/DShader` |
| 生成产物 | `Graph` backend 的材质每次构建都存盘；`ThinCustom`（默认）材质只在内存里 —— 见[内存材质](../generation/ephemeral.md) |

一个 `.dss` 和同名的 `.dsm` 构建出同一个资产路径，两者留一个。诊断写码，读法见[错误速查](../diagnostics/index.md)。
想看这些材质是由哪些语言构件拼出来的，见[常用写法](patterns.md)。

<a id="动画着色"></a>

## 动画着色

`UE.Time()` 给出一个标量时钟；`sin` 是[数学内置](../builtins/math.md)之一，所以裸调用、按位置传参。

```c
// DShader/Materials/M_Pulse.dsm
Shader(Name="Materials/M_Pulse")
{
    Properties = {
        vec3            Tint  = vec3(1.0, 0.4, 0.1);
        ScalarParameter Speed = 2.0 [Slider(0, 10)];
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
        float t     = UE.Time();
        float pulse = sin(t * Speed) * 0.5 + 0.5;
        Color = Tint * pulse;
    }
}
```

- `sin` 建出一个 `Sine` 节点，`Period` 保持默认值 1，所以它按**周期**计、不按弧度：`Speed = 2.0` 就是每秒两次脉冲。常量
  折叠也按周期 （自 2.1.2 起）。要别的周期，直接调用节点：`UE.Sine(Input = x, Period = 2.0)`。
- 两个相同的调用就是一个节点，不论是数学内置还是 `UE.*` （自 2.0.0 起）：写两次 `UE.Time()` 只有一个 `Time` 节点。

<a id="写成-dss"></a>

### 写成 `.dss`

```hlsl
// DShader/Materials/M_Pulse.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

uniform float3 Tint = float3(1.0, 0.4, 0.1);
/// @slider 0 10
uniform float Speed = 2.0;

export void M_Pulse(inout material m)
{
    float t = UE.Time();
    float pulse = sin(t * Speed) * 0.5 + 0.5;
    m.EmissiveColor = Tint * pulse;
}
```

<a id="uv-平移"></a>

## UV 平移

```c
// DShader/Materials/M_Panned.dsm
Shader(Name="Materials/M_Panned")
{
    Properties = {
        TextureSampleParameter2D BaseTex  = Path(Engine, "EngineResources/WhiteSquareTexture");
        ScalarParameter          PanSpeed = 0.1 [Slider(-1, 1)];
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
        vec2 uv    = UE.TexCoord(Index = 0);
        vec2 moved = UE.Panner(Coordinate = uv, Time = UE.Time(), Speed = vec2(PanSpeed, 0.0));
        vec4 texel = BaseTex(Coordinates = moved);

        Color = texel.rgb;
    }
}
```

**提示：**

`UE.Panner` 的参数分两类。`Coordinate`、`Time`、`Speed` 是**输入 pin**，接受表达式 —— 所以上面能用 `PanSpeed` 参数驱动平移。
而 `SpeedX`、`SpeedY`、`FractionalPart` 是节点上的**字面量属性**，只接常量：写 `SpeedX = PanSpeed` 是 `DSH4373`。
拼错的参数名在 1.x 源文件里被丢掉并给出 warning `DSH5254`，在 `.dss` 里是错误 `DSH5213`。见
[UE.Panner](../builtins/ue.md#uepanner)。

<a id="写成-dss-1"></a>

### 写成 `.dss`

```hlsl
// DShader/Materials/M_Panned.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

/// @default /Engine/EngineResources/WhiteSquareTexture
uniform Texture2D BaseTex;
/// @slider -1 1
uniform float PanSpeed = 0.1;

export void M_Panned(inout material m)
{
    float2 uv = UE.TexCoord(Index = 0);
    float2 moved = UE.Panner(Coordinate = uv, Time = UE.Time(), Speed = float2(PanSpeed, 0.0));
    m.EmissiveColor = BaseTex.Sample(moved).rgb;
}
```

`.Sample` 建出一个 `TextureObjectParameter` 加一个 `TextureSample`，1.x 的 token 建的是一个 `TextureSampleParameter2D`；
材质看到的结果相同。

<a id="按阈值分支"></a>

## 按阈值分支

两个分支都会被建进图里，每个被改动的变量一个条件节点 —— `UMaterialExpressionIf`，或者条件是 static bool 时的
`StaticSwitch` —— 在运行时二选一。条件必须带括号，每个分支体必须带花括号。

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
| `if (x)` | 真值判断 —— 等同于 `x != 0`，所以**负值**走 then 分支 |

- 条件比较的是两个数：比较两个向量是 `DSH4260`，不是数字的操作数是 `DSH4226`。
- 在 `if` 之前声明、只在一个分支里改过的变量，和它在 `if` 之前的值合并。`if` 之前没有值、又只在一个分支里赋值的变量，
  在 `if` 之后被读取的地方报 `DSH4372`。
- 在分支*里*声明的变量只属于那个分支 （自 2.0.0 起）：它不参与合并，`if` 之后它的名字是未知的。
- 每次赋值都会把值转换成变量声明的类型，所以同一个变量的两个分支值总是一致的。
- 在分支里读参数没问题 —— 参数永远不是分支输出。
- `if` **区分大小写**：`If (x) { … }` 是对一个叫 `If` 的函数的调用，后面的 `{` 报 `DSH2154`。

**警告：**

1.x 条件里的 `&&` 和 `||` 被拒绝 （自 2.0.0 起）：`DSH2210`。1.x 会一声不吭地丢掉第一个比较之后的一切，所以
`if (a > 0 && b > 0)` 测的是 `a > 0`。请改成两层嵌套的 `if`。1.x 会截断的其他写法 —— `%`、`?:`、`&`、`|`、`^`、`<<`、
`v[i]` —— 在表达式的任何位置都是错误（`DSH2200`–`DSH2202`）；见[不支持的写法](../graph/unsupported.md#1x-会截断的运算符)。
`.dss` 里这些运算符都有。

<a id="写成-dss-2"></a>

### 写成 `.dss`

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

分支赋值的变量在 `if` 之前声明，正如 1.x 的 `Outputs` 声明那样。

<a id="静态开关变体"></a>

## 静态开关变体

`StaticSwitchParameter` 不是运行时分支 —— 它产出的是不同的 permutation。在 1.x 里，和 `if` 不同，它写成**调用**。

```c
// DShader/Materials/M_Variant.dsm
Shader(Name="Materials/M_Variant")
{
    Properties = {
        Group("Surface") {
            VectorParameter BaseColor   = float4(0.1, 0.2, 0.3, 1.0);
            VectorParameter DetailColor = float4(1.0, 0.8, 0.3, 1.0);
        }

        Group("Switches") {
            StaticSwitchParameter UseDetail = true [Description="Use the detail colour"];
        }
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        float3 Color;
        Base.BaseColor = Color;
    }

    Graph = {
        Color = UseDetail(True = DetailColor.rgb, False = BaseColor.rgb);
    }
}
```

`True=` / `False=`、`A=` / `B=` 和位置形式都可以；缺一边是 `DSH5258`。**不行**的是裸写 `Color = UseDetail;` 和
`if (UseDetail) { … }` —— 两者都报 `DSH4200`，因为 1.x 的开关参数不是值。见
[StaticSwitchParameter](../graph/calls.md#staticswitchparameter)。

<a id="写成-dss-3"></a>

### 写成 `.dss`

```hlsl
// DShader/Materials/M_Variant.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

/// @group Surface
uniform float4 BaseColor = float4(0.1, 0.2, 0.3, 1.0);
/// @group Surface
uniform float4 DetailColor = float4(1.0, 0.8, 0.3, 1.0);

/// @static
/// @group Switches   @desc Use the detail colour
uniform bool UseDetail = true;

export void M_Variant(inout material m)
{
    float3 Color = BaseColor.rgb;
    if (UseDetail)
    {
        Color = DetailColor.rgb;
    }
    m.BaseColor = Color;
}
```

在 `.dss` 里，`/// @static` 的 `uniform bool` 就是一个值：被 `if` 读取时，合并处是一个 `StaticSwitch`，参数是真正的
static switch 参数。

<a id="半透明-ui-材质"></a>

## 半透明 UI 材质

```c
// DShader/Materials/M_UI_Glass.dsm
Shader(Name="Materials/M_UI_Glass")
{
    Properties = {
        vec3            Tint    = vec3(1.0, 1.0, 1.0);
        ScalarParameter Opacity = 0.75 [Slider(0, 1)];
    }

    Settings = {
        Domain       = "UI";
        ShadingModel = "Unlit";
        BlendMode    = "Translucent";
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        Color = Tint;
        Alpha = Opacity;
    }
}
```

`Domain = "UI"` 加 `ShadingModel = "Unlit"` 是 UMG 材质想要的组合；而让 `Base.Opacity` 真正起作用的
是 `BlendMode`。所有可接受写法见[枚举取值](../settings/material-enums.md)。

<a id="写成-dss-4"></a>

### 写成 `.dss`

```hlsl
// DShader/Materials/M_UI_Glass.dss
#pragma material(Domain = UI, ShadingModel = Unlit, BlendMode = Translucent)

uniform float3 Tint = float3(1.0, 1.0, 1.0);
/// @slider 0 1
uniform float Opacity = 0.75;

export void M_UI_Glass(inout material m)
{
    m.EmissiveColor = Tint;
    m.Opacity = Opacity;
}
```

<a id="surface-pbr-骨架"></a>

## Surface PBR 骨架

```c
// DShader/Materials/M_Surface.dsm
Shader(Name="Materials/M_Surface")
{
    Properties = {
        Group("Surface") {
            VectorParameter Albedo    = float4(0.8, 0.6, 0.4, 1.0);
            ScalarParameter Roughness = 0.5 [Slider(0, 1)];
            ScalarParameter Metallic  = 0.0 [Slider(0, 1)];
        }
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
    }

    Outputs = {
        vec3  Color;
        float Rough;
        float Metal;

        Base.BaseColor = Color;
        Base.Roughness = Rough;
        Base.Metallic  = Metal;
    }

    Graph = {
        Color = Albedo.rgb;
        Rough = Roughness;
        Metal = Metallic;
    }
}
```

输出*变量*和绑定*目标*处在不同的命名空间里，但把它们写得明显不同（`Color` → `Base.BaseColor`）
能让文件更好读。完整的目标清单见[输出绑定](../generation/outputs.md)；目标名区分大小写，拼错是 `DSH5200`。

<a id="写成-dss-5"></a>

### 写成 `.dss`

```hlsl
// DShader/Materials/M_Surface.dss
#pragma material(Domain = Surface, ShadingModel = DefaultLit, BlendMode = Opaque)

/// @group Surface
uniform float4 Albedo = float4(0.8, 0.6, 0.4, 1.0);
/// @group Surface   @slider 0 1
uniform float Roughness = 0.5;
/// @group Surface   @slider 0 1
uniform float Metallic = 0.0;

export void M_Surface(inout material m)
{
    m.BaseColor = Albedo.rgb;
    m.Roughness = Roughness;
    m.Metallic  = Metallic;
}
```

<a id="写-materialattributes"></a>

## 写 `MaterialAttributes`

绑定 `Base.MaterialAttributes` 会为生成的材质打开 *Use Material Attributes*。`MaterialAttributes Attrs;` 是一个空的属性集 ——
一个 `MakeMaterialAttributes` 节点 —— 它的成员按名字写；在值被使用的地方，这些写入合成一个 `SetMaterialAttributes` 节点
（自 2.0.0 起）（1.x 每次写入串一个）。

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

        // 已经写过的成员读回来就是写进去的值，不建节点。
        float Echo = Attrs.Roughness;
        Attrs.Specular = Echo;
    }
}
```

**提示：**

`Outputs` 的绑定在 `Graph` 之后应用，所以绑定拿到的是上面在 `Graph` 里声明的 `Attrs`。（自 2.0.0 起） `Outputs` 里的
声明 `MaterialAttributes Attrs;` 也是这个变量，从空开始。`MaterialAttributes` 是 `Outputs`、`Inputs`、`Graph` 变量和函数
结果的类型；作为 `Properties` 类型是 `DSH3252`，作为 `Function` 的输入是 `DSH6210`。

- 成员名就是运行中引擎的材质属性名（`BaseColor`、`Metallic`、`Specular`、`Roughness`、`EmissiveColor`、`Opacity`、
  `Normal`……）；引擎没有的是 `DSH5200`。
- 对 `MaterialAttributes` 值用算术运算符是 `DSH4226`。
- 一个 `Shader` 同时用 `Base.MaterialAttributes` 和 `Base.FrontMaterial` 不再被拒绝 （自 2.0.0 起）：两者都会接上，
  *Use Material Attributes* 让材质读取属性集。

<a id="写成-dss-6"></a>

### 写成 `.dss`

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

入口写的是 `m` 的成员，它们直接连到材质的 pin 上：不建 `MakeMaterialAttributes` 节点。成员要先写后读（否则 `DSH4370`）。

<a id="substrate-表面"></a>

## Substrate 表面

（自 UE 5.4 起）

`Substrate.*` 构建 Substrate 节点；消费它们的绑定是 `Base.FrontMaterial`。

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

- Substrate 需要 **UE 5.4 或更新**：节点目录列出运行中引擎的 Substrate 类，更老的引擎一个都没有。那里的 `Substrate.*` 调用是
  `DSH5210`，`Base.FrontMaterial` 是 `DSH5200`；`Substrate.Select` 需要 UE 5.6（`DSH5294`）。引擎要编译这张图，项目还得打开
  Substrate —— 见 [Substrate 语法糖](../language-v2/substrate.md)。
- DreamShader 不再为 `Base.FrontMaterial` 设置着色模型 （自 2.0.0 起）（1.x 强制设为 `Substrate`，并拒绝别的
  `ShadingModel`）：写了 `ShadingModel` 就按写的应用。
- BSDF 节点按名字传参；五个组合节点（`Add`、`Weight`、`Mix`、`Layer`、`Select`）也接受位置参数 （自 2.0.0 起）。其他地方的
  位置参数是 `DSH5220`，`Class=` 是 `DSH5216`：名字本身就是类。
- `GraphFunction` 体里的 `Substrate.*` 调用不会被提升成节点 —— Custom 节点的输入带不了 Substrate 值 —— 并给出 warning
  `DSH6316` （自 2.0.0 起）。
- （自 2.0.0 起） Substrate 值可以组合：`A + B`、`A * w` 和 `lerp(A, B, t)` 分别是 `Substrate.Add`、`Substrate.Weight` 和
  `Substrate.HorizontalMix`；两个 Substrate 值之间的 `if` 在静态条件下是 `StaticSwitch`，否则是 `Substrate.Select`。其他运算符是
  `DSH5293`，对 Substrate 值读成员是 `DSH4207`。

<a id="写成-dss-7"></a>

### 写成 `.dss`

```hlsl
// DShader/Materials/M_Substrate.dss
uniform float3 Color = float3(0.1, 0.6, 1.0);

export void M_Substrate(inout material m)
{
    m.FrontMaterial = Substrate.Unlit(EmissiveColor = Color);
}
```

<a id="用参数集合驱动材质"></a>

## 用参数集合驱动材质

```c
// DShader/Materials/M_Wind.dsm
Shader(Name="Materials/M_Wind")
{
    Properties = {
        vec3 Tint = vec3(0.4, 0.7, 0.3);
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float wind = UE.CollectionParam(
            Collection = Path(Game, "Collections/MPC_Wind"),
            Parameter  = "WindStrength");

        Color = Tint * wind;
    }
}
```

- `Collection`（别名 `Asset`）和 `Parameter`（别名 `ParameterName`）都是**必填**。`UE.CollectionParameter` 是引擎的名字，
  和 `UE.CollectionParam` 是同一个类 （自 2.1.1 起）（2.0.0 – 2.1.0 在 `Graph` 里对 `UE.CollectionParam` 报 `DSH5210`）。
- 集合资产在建节点时加载，参数按名字在里面查找：集合解析不了或加载不了是 `DSH8213`，名字既不是标量参数也不是向量参数是
  `DSH8254`。
- 引擎没给这个节点的输出定类型，所以值和它被读入的位置一样宽 （自 2.0.0 起）：这里读进一个 `float`。
- `Group` 和 `SortPriority` 只在运行中引擎的类有它们时才接受（（自 UE 5.7 起））；类没有时 `Group` 是 `DSH5213`，
  `SortPriority` 建节点时以 `DSH8212` 失败。见 [UE.CollectionParam](../builtins/ue.md#uecollectionparam)。

<a id="写成-dss-8"></a>

### 写成 `.dss`

```hlsl
// DShader/Materials/M_Wind.dss
#pragma material(Domain = Surface, ShadingModel = Unlit)

uniform float3 Tint = float3(0.4, 0.7, 0.3);

export void M_Wind(inout material m)
{
    float wind = UE.CollectionParameter(Collection = "/Game/Collections/MPC_Wind",
                                        ParameterName = "WindStrength");
    m.EmissiveColor = Tint * wind;
}
```

资产属性接一个带引号的对象路径；`ParameterName` 是引擎上那个属性的名字。

<a id="多输出-helper"></a>

## 多输出 helper

两个及以上 `out` 参数时，用语句调用形式拿到全部结果，尾部的实参就是接收结果的名字。

```c
// DShader/Materials/M_Brightness.dsm
Function SplitBrightness(in vec3 color, out vec3 normalized, out float brightness) {
    brightness = max(max(color.r, color.g), color.b);
    normalized = color / max(brightness, 0.0001);
}

Shader(Name="Materials/M_Brightness")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.5, 0.25);
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
        SplitBrightness(Tint, Normalized, Brightness);
        Color = Normalized * Brightness;
    }
}
```

- 函数体是 **HLSL**，所以这里的 `max` 是 HLSL 内建函数，不是 `Graph` 的数学内置。
- 1.x 的 `Function` 要么有返回类型、要么有 `out` 参数，两者都有是 `DSH6304`，两者都没有是 `DSH6305`。
- 没人声明过的 out 目标由调用声明（info `DSH5283`）。按值调用 —— `vec3 n = SplitBrightness(Tint);` —— 拿到的是第一个
  `out` （自 2.0.0 起）。

<a id="写成-dss-9"></a>

### 写成 `.dss`

```hlsl
// DShader/Materials/M_Brightness.dss
#pragma material(Domain = UI, ShadingModel = Unlit)

uniform float3 Tint = float3(1.0, 0.5, 0.25);

/// @custom
void SplitBrightness(float3 color, out float3 normalized, out float brightness)
{
    brightness = max(max(color.r, color.g), color.b);
    normalized = color / max(brightness, 0.0001);
}

export void M_Brightness(inout material m)
{
    float3 Normalized;
    float  Brightness;
    SplitBrightness(Tint, Normalized, Brightness);
    m.EmissiveColor = Normalized * Brightness;
}
```

`/// @custom` 让函数体原样成为一个 Custom 节点，和 1.x 的 `Function` 一样；`out` 实参是先声明好的变量。

<a id="settings-一览带显式-backend-的玻璃"></a>

## Settings 一览：带显式 backend 的玻璃

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

| 键 | 别名 | 取值 |
| :-- | :-- | :-- |
| `MaterialDomain` | `Domain` | `Surface`、`DeferredDecal` / `Decal`、`LightFunction`、`Volume`、`PostProcess`、`UI` / `UserInterface`、`RuntimeVirtualTexture` / `VirtualTexture` |
| `ShadingModel` | — | `Unlit`、`DefaultLit` / `Lit`、`Subsurface`、`PreintegratedSkin`、`ClearCoat`、`SubsurfaceProfile`、`TwoSidedFoliage`、`Hair`、`Cloth`、`Eye`、`SingleLayerWater`、`ThinTranslucent`，以及 UE 5.4+ 上的 `Substrate` / `Strata` |
| `BlendMode` | `RenderType` | `Opaque`、`Masked` / `Cutout`、`Translucent` / `Transparent`、`Additive`、`Modulate`、`AlphaComposite` / `PremultipliedAlpha` / `Premultiplied`、`AlphaHoldout`、`TranslucentColoredTransmittance` |
| `Backend` | — | `Graph`、`ThinCustom`；`Instance` 是 `ThinCustom` 的旧拼写，带 warning `DSH7204` |

- 枚举值在比较前会去掉空格、`_` 和 `-`，并且不区分大小写：`"Default Lit"`、`"DefaultLit"`、
  `"default_lit"` 和 `"DEFAULT-LIT"` 是同一个别名。所有设置值的引号都可省。
- `MaterialDomain`、`ShadingModel` 和 `BlendMode` 按上表解析；`Backend` 和 （自 2.0.0 起） `Substrate` 是 DreamShader
  自己的键。**其余每个键都被直接反射到 `UMaterial` 上** —— 上面的 `TwoSided` 和 `Wireframe` 就是真实的 `UMaterial` 属性。
  不是任何属性的键会让构建失败：`DSH8215`，附带 `DSH7118`。
- 一个键写两遍是 warning `DSH3262` （自 2.0.0 起）（之前不报），后一个值生效。重复的 `Settings` section 会合并。
- 省略 `Backend` 会回退到项目的 **Default Compiler Backend**（出厂是 `ThinCustom`）。`Backend = "Graph"` 时材质是一个普通的
  `UMaterial`，每次构建成功都存盘。

**警告：**

`Backend = "";` 是错误 `DSH7201` （自 2.0.0 起）（1.x 把它读成 `Graph`），不是 backend 的值是 `DSH7202`。只有*省略*这个键
才会回退到项目设置。见 [Backend](../settings/backend.md)。

<a id="写成-dss-10"></a>

### 写成 `.dss`

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

`#pragma material` 接受和 `Settings` 一样的键和值，可以分成多行写；同一个键设两次在这里是错误 `DSH7200`。

<a id="runtime-virtual-texture采样与写入"></a>

## Runtime Virtual Texture：采样与写入

RVT 节点没有专门的内置族，也不需要。采样、替换和特性开关节点是普通表达式，通过
[`UE.<ClassName>`](../builtins/ue-expression.md) 触达；`RuntimeVirtualTextureOutput` 是一个 *custom output*，根本没有输出 pin，
所以它永远不是 `Graph` 赋值的右边 —— 它在 `Outputs` 里通过 `Expression( … ).Pin[i]` 绑定。

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

        // 两条语句复用同一个 RuntimeVirtualTextureOutput 节点。
        Expression(Class="RuntimeVirtualTextureOutput").Pin[0] = RVTBaseColor;
        Expression(Class="RuntimeVirtualTextureOutput").Pin[2] = RVTRoughness;
    }

    Graph = {
        float3 Sampled = UE.RuntimeVirtualTextureSample(
            OutputType     = "float3",
            Output         = "BaseColor",
            MaterialType   = "BaseColor_Normal_Roughness",
            VirtualTexture = Path(Game, "RVT/RVT_Terrain"));

        // 虚拟纹理没有页面的地方，回退到参数 ……
        float3 Blended = UE.RuntimeVirtualTextureReplace(
            OutputType = "float3", Default = Fallback, VirtualTextureOutput = Sampled);

        // …… 完全关掉虚拟纹理时，再回退一次。
        SurfaceColor = UE.VirtualTextureFeatureSwitch(
            OutputType = "float3", Yes = Blended, No = Fallback);

        RVTBaseColor = SurfaceColor;
        RVTRoughness = 0.5;
    }
}
```

**提示：**

这个例子引用的 `/Game/RVT/RVT_Terrain` 是项目里的 `URuntimeVirtualTexture` 资产。请换成你项目里真实存在的那个。

| 节点 | `Class` 写法 | 写在哪 |
| :-- | :-- | :-- |
| Runtime Virtual Texture Sample | `RuntimeVirtualTextureSample` | `Graph` 里的 `UE.…` |
| Runtime Virtual Texture Sample Parameter | `RuntimeVirtualTextureSampleParameter` | `Properties` 里的[参数声明](../language/properties.md) |
| Runtime Virtual Texture Replace | `RuntimeVirtualTextureReplace` | `Graph` 里的 `UE.…` |
| Virtual Texture Feature Switch | `VirtualTextureFeatureSwitch` | `Graph` 里的 `UE.…` |
| Runtime Virtual Texture Output | `RuntimeVirtualTextureOutput` | `Outputs` 里的 `Expression( … ).Pin[i]` |

- `RuntimeVirtualTextureSample` 有八个输出：`0` BaseColor、`1` Specular、`2` Roughness、`3` Normal、`4` WorldHeight、`5` Mask、
  `6` Displacement、`7` Mask4。用 `Output="BaseColor"` 按名字选一个，或用 `OutputIndex=` 按位置选。
- `OutputType` 从每个调用上拿掉 （自 2.0.0 起）；由节点目录给节点定类型。
- `RuntimeVirtualTextureOutput` 的输入 pin，按 `Pin[i]` 顺序：`0` BaseColor、`1` Specular、`2` Roughness、`3` Normal、
  `4` WorldHeight、`5` Opacity、`6` Mask、`7` Displacement、`8` Mask4。
- `VirtualTexture` 是反射出来的资产属性，所以接一个 [`Path(...)`](../parameters/path.md) 引用，或一个裸的带引号对象路径。
  `MaterialType` 和节点的其他枚举、布尔属性由同一个反射步骤写入，枚举写法规则同[枚举取值](../settings/material-enums.md)。
- *写入* RVT 的材质保持 `Domain = "Surface"`，再加上输出节点，就像上面那样。单独的 `Domain = "RuntimeVirtualTexture"` 写法
  映射到 `MD_RuntimeVirtualTexture`，引擎把它标为**已弃用并隐藏**；DreamShader 仍然接受它，好让已有资产能反编译，但新材质别用。
- 这里钉住 `Backend = "Graph"`，结果是一个普通的 `UMaterial`，每次构建都存盘，可以打开节点和源码对照。节点本身不依赖 backend。

<a id="写成-dss-11"></a>

### 写成 `.dss`

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

输出用成员访问（`.BaseColor`）选，资产属性接带引号的对象路径。custom output 节点是入口里的一条语句；`Pin[i] = …` 按引擎序号
连接第 `i` 个输入。

<a id="怎么跑这些文件"></a>

## 怎么跑这些文件

1. 把它们保存到 `<Project>/DShader` 下，或者 `SourceDirectory` 指向的位置。
2. 开着 *Auto Compile On Save*（默认开），编辑器会在保存 0.25 秒后编译源文件（*Save Debounce Seconds*）。`Graph` backend
   的材质和每个材质函数、layer、blend 每次构建成功都存盘。`ThinCustom` 材质 —— 默认 backend —— 以 Ephemeral 形式构建在内存里，
   直到 cook、显式 *Materialize*、子材质实例或命令行给它一个 package。
3. 无头编译单个文件：

```powershell
& "<Engine>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" `
  "<Project>/MyProject.uproject" `
  -run=DreamShader compile -Source="<Project>/DShader/Materials/M_Minimal.dsm" -Force `
  -unattended -nopause -nosplash -stdout -log
```

把 `-Source=` 换成 `-All` 就能编译项目的每个源文件：先编译所有 `.dsf`，再按路径顺序编译其余的；`.dsh` 经由 include 它的文件编译。
命令行会 Materialize 每个 `ThinCustom` 产物，所以每个材质都会写出一个 package。见[命令行](../tools/commandlet.md#-all-的发现与排序)。

<a id="接下来"></a>

## 接下来

- [常用写法](patterns.md) — 这些材质是由哪些构件拼起来的。
  - [Custom Pass 示例](custom-pass.md) — .dsp 管线：描边、毛玻璃、透视、风场。
  - [if / else](../graph/if.md) — 条件语句、真值语义与分支合并规则。
  - [材质 Settings](../settings/material.md) — 每一个 Settings 键，以及反射是怎么解析它的。
  - [输出绑定](../generation/outputs.md) — 每一个 Base.* 目标及其接受的值。
  - [命令行](../tools/commandlet.md) — 无头编译及其开关。
