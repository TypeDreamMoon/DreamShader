---
title: "Custom Pass 示例"
description: "Custom Pass 示例"
---

<a id="custom-pass-examples"></a>

# Custom Pass 示例

> [DreamShader](../index.md) » [示例](index.md) » **Custom Pass 示例**

几条完整的管线，各自带着用到的 `.dss` 材质；HLSL pass 的代码都直接写在 `.dsp`
里，用的是 [HLSL pass](../../runtime/hlsl.md#where-the-code-is) 讲的三种写法。每一条都在测试宿主的演示关卡里跑过 —— 从这些源码构建、在游戏视口里渲染（`Tools/TestHost/PassDemo`）。材质浏览器 **New Source** 的管线模板有两个是它们的精简版 —— 描边的 mesh 遮罩和风场；后处理模板是一个自己的柔光。

| | |
| :-- | :-- |
| 引擎 | UE `5.8` 及以上 |
| 假定的源码根 | `<Project>/DShader`；每条管线的材质都在它旁边，`DShader/Passes/` |

| 示例 | 展示了什么 |
| :-- | :-- |
| [高亮描边](#highlight-outline) | 按 layer 选物体、带覆盖材质的 mesh pass；只写入口语句的 compute 模糊；全屏材质合成；导出的缓冲区 |
| [UI 毛玻璃](#ui-frosted-glass) | 后处理链里的 `copy` 抓屏和 compute 降采样，入口在文件的 `hlsl` 块里、带一个辅助函数；导出给 UMG |
| [X 光](#x-ray) | 按 layer 和 list 选物体、带自己深度的 mesh pass，再对照场景深度合成 |
| [风场](#wind-field) | `BeginView` 上对固定尺寸历史缓冲区的 compute pass，导出给植被材质的 WPO 和 Niagara |
| [扫描脉冲](#scanner-pulse) | `BeforePostProcess` 的像素 HLSL pass，只写入口语句，通过 `View` 读场景深度 |
| [老式 CRT](#old-crt) | `PostProcess.AfterTonemap` 的像素 HLSL pass，块里是一个完整函数，按管线权重混合 |
| [带标记的物体](#tagged-objects) | 按 custom stencil 选物体、用物体自己的材质画的 mesh pass |

<a id="highlight-outline"></a>

## 高亮描边

带 `Highlight` pass layer 的物体描一圈橙色轮廓，隔着墙也看得见，它们下面的地面还会发光。
给这些 actor 加一个 `UDreamPassLayerComponent`，layer 设为 `Highlight`（工程设置 ▸ DreamShader Custom
Pass ▸ Layers），然后在工程设置里或用体积激活这条管线。

```hlsl
// DShader/Passes/CP_Highlight.dsp
#pragma pipeline(Order = 100)

/// @group Look
uniform float4 OutlineColor = float4(1.0, 0.6, 0.0, 1.0);
/// @group Look   @slider 1 8
uniform float  OutlineWidth = 3.0;

/// @desc What the objects cover.
buffer Mask    : R8(Clear = 0);
/// @desc The blurred mask; the ground material reads it too.
buffer Blurred : R8(Scale = 0.5, Export = true);

/// @desc No depth test: the outline shows through walls.
pass DrawMask : mesh
{
    Injection = AfterOpaque;
    Filter    = Layer(Highlight);
    Material  = "M_HighlightMask";
    Depth     = None;
    write Output0 = Mask;
}

/// @desc A (2R+1)^2 box over the mask, into the half-size Blurred.
pass Blur : compute
{
    Injection = AfterOpaque;
    Dispatch  = Blurred;
    read  Source = Mask;
    write Result = Blurred;
    param Radius = OutlineWidth;

    hlsl
    {
        // The statements of the entry: Id is this thread's texel; the threads past the dispatch returned already.
        const float2 UV = (Id.xy + 0.5) * ResultSize.zw;
        const int R = (int)Radius;
        float Sum = 0;
        for (int y = -R; y <= R; ++y)
        {
            for (int x = -R; x <= R; ++x)
            {
                Sum += Source.SampleLevel(DP_LinearClamp, UV + float2(x, y) * SourceSize.zw, 0).r;
            }
        }
        Result[DP_ViewRect.xy + Id.xy] = Sum / ((2 * R + 1) * (2 * R + 1));
    }
}

pass Composite : fullscreen
{
    Injection = BeforePostProcess;
    Material  = "PP_OutlineComposite";
    read  Mask;
    read  Blurred;
    write SceneColor;
    param Color = OutlineColor;
}
```

```hlsl
// DShader/Passes/M_HighlightMask.dss -- the override material
#pragma material(ShadingModel = Unlit, bUsedWithSkeletalMesh = true, bUsedWithInstancedStaticMeshes = true)

export void M_HighlightMask(inout material m)
{
    m.EmissiveColor = float3(0, 0, 0);
    UE.DreamPassOutput(Output0 = float4(1, 0, 0, 0));
}
```

```hlsl
// DShader/Passes/PP_OutlineComposite.dss
#pragma material(Domain = PostProcess, BlendableLocation = SceneColorAfterDOF)

uniform float4 Color = float4(1, 0.6, 0, 1);
uniform float  DreamPassWeight = 1.0;

export void PP_OutlineComposite(inout material m)
{
    float2 UV     = UE.ScreenPosition().ViewportUV;
    float3 Scene  = UE.SceneTexture(SceneTextureId = PostProcessInput0, Coordinates = UV).Color.rgb;
    float  Inside = UE.UserSceneTexture(UserSceneTexture = "Mask",    Coordinates = UV).Color.r;
    float  Halo   = UE.UserSceneTexture(UserSceneTexture = "Blurred", Coordinates = UV).Color.r;
    float  Edge   = saturate(Halo * 4.0) * (1.0 - Inside);
    m.EmissiveColor = lerp(Scene, Color.rgb, Edge * Color.a * DreamPassWeight);
}
```

```hlsl
// In the ground's material: read the exported buffer
float Glow = UE.DreamPassBuffer(Pipeline = "CP_Highlight", Buffer = "Blurred").r;
m.EmissiveColor += Glow * float3(1.0, 0.6, 0.0) * 0.5;
```

要点：

- 地面是不透明的，`Blurred` 写在 `AfterOpaque`，所以地面读到的是**上一帧**的模糊。
  编译器会这么提示。把 `DrawMask` 和 `Blur` 挪到 `BeforeBasePass` 就是本帧的；那里的 mesh pass
  只能用 `Depth = None` 或 `Own`，这个正好是后者。
- `Highlight` layer 里的 Nanite 成员被跳过，并警告一次。在 `Nanite = AssignStencil(200)` 上写
  `DrawMask` 可以把它们以 stencil 遮罩的形式包含进来。

<a id="ui-frosted-glass"></a>

## UI 毛玻璃

tonemap 之后的画面，降采样再模糊，给 UMG 做毛玻璃背景。

```hlsl
// DShader/Passes/CP_UIBackdrop.dsp
#pragma pipeline(Order = 900, Views = Game, Requires = PostProcess)

/// @slider 1 12
uniform float BlurRadius = 6.0;

buffer Half    : RGBA8(Scale = 0.5,  Resolution = Output);
buffer Quarter : RGBA8(Scale = 0.25, Resolution = Output, Export = true);

hlsl
{
    // Shared code: the direction of tap Index of Count on a circle. It may use no pass's names; the entry below may.
    float2 CircleTap(int Index, int Count)
    {
        const float Angle = Index * 6.2831853 / Count;
        return float2(cos(Angle), sin(Angle));
    }

    // Twelve taps on a circle of Radius texels of Half, into Quarter.
    [numthreads(8, 8, 1)]
    void DownCS(uint3 Id : SV_DispatchThreadID)
    {
        if (any(Id.xy >= DP_DispatchSize.xy))
        {
            return;
        }
        const float2 UV = (Id.xy + 0.5) * ResultSize.zw;
        float4 Sum = 0;
        [unroll] for (int i = 0; i < 12; ++i)
        {
            Sum += Source.SampleLevel(DP_LinearClamp, UV + CircleTap(i, 12) * Radius * SourceSize.zw, 0);
        }
        // The picture after the tonemapper has no alpha (it reads 0); UMG multiplies by it, so write 1.
        Result[Id.xy] = float4(Sum.rgb / 12.0, 1.0);
    }
}

pass Grab : copy
{
    Injection = PostProcess.AfterTonemap;
    read  SceneColor;
    write Half;
}

pass Down : compute
{
    Injection = PostProcess.AfterTonemap;
    Entry     = DownCS;
    Dispatch  = Quarter;
    read  Source = Half;
    write Result = Quarter;
    param Radius = BlurRadius;
}
```

要点：

- 后处理链上不写场景颜色的 pass，链保持原样。
- tonemap 后画面的 alpha 是 0，`Grab` 原样复制；`DownCS` 写 1，否则 `Quarter` 做的 UMG 图片是透明的。
- `DownCS` 是文件 `hlsl` 块里的入口：单写 `Entry = DownCS;` 就选中它。另一个 pass 点名它，就会用自己的绑定跑同一段代码；`CircleTap` 是共享代码，编进每一个 HLSL 写在这个文件里的 pass。
- 这个导出是给 UI 用的，UI 在场景之后画。世界里的玻璃面板在自己的屏幕位置采样它，会在抓屏里看到自己。
- `Requires = PostProcess`：没有后处理的视图里整条管线都关掉，而不是关一半。
- UMG 在场景之后渲染，所以读的是本帧的 `Quarter`。分屏时只有第一个玩家的视图写导出的缓冲区。

<a id="x-ray"></a>

## X 光

墙后的敌人显示成青色剪影。

```hlsl
// DShader/Passes/CP_XRay.dsp
#pragma pipeline(Order = 200)

uniform float4 XRayColor = float4(0.2, 0.8, 1.0, 0.6);

buffer ObjDepth   : Depth32;
/// @desc r = covered, g = the object's scene depth.
buffer Silhouette : RG16F(Clear = 0);

pass DrawObjects : mesh
{
    Injection = AfterOpaque;
    Filter    = Layer(XRay) | List(Enemies);
    Material  = "M_XRayDepth";
    Depth     = Own(ObjDepth);
    write Output0 = Silhouette;
}

pass Composite : fullscreen
{
    Injection = BeforePostProcess;
    Material  = "PP_XRayComposite";
    read  Silhouette;
    write SceneColor;
    param Tint = XRayColor;
}
```

```hlsl
// DShader/Passes/M_XRayDepth.dss
#pragma material(ShadingModel = Unlit, bUsedWithSkeletalMesh = true, bUsedWithInstancedStaticMeshes = true)

export void M_XRayDepth(inout material m)
{
    m.EmissiveColor = float3(0, 0, 0);
    UE.DreamPassOutput(Output0 = float4(1, UE.PixelDepth(), 0, 0));
}
```

```hlsl
// DShader/Passes/PP_XRayComposite.dss
#pragma material(Domain = PostProcess, BlendableLocation = SceneColorAfterDOF)

uniform float4 Tint = float4(0.2, 0.8, 1.0, 0.6);

export void PP_XRayComposite(inout material m)
{
    float2 UV     = UE.ScreenPosition().ViewportUV;
    float3 Scene  = UE.SceneTexture(SceneTextureId = PostProcessInput0, Coordinates = UV).Color.rgb;
    float  Depth  = UE.SceneTexture(SceneTextureId = SceneDepth,        Coordinates = UV).Color.r;
    float2 Sil    = UE.UserSceneTexture(UserSceneTexture = "Silhouette", Coordinates = UV).Color.rg;
    float  Hidden = Sil.r * step(Depth + 1.0, Sil.g);
    m.EmissiveColor = lerp(Scene, Tint.rgb, Hidden * Tint.a);
}
```

要点：

- 游戏代码填 `List(Enemies)`：`UDreamPassSubsystem::AddToList("Enemies", Mesh)`，或蓝图里的 *Add To List*。
- `Depth = Own(ObjDepth)`：物体之间互相遮挡，但不被墙挡。
- GPU 蒙皮的骨骼网格能画，CPU 蒙皮的被跳过。

<a id="wind-field"></a>

## 风场

一张 256×256 的风场，每帧由 compute shader 推进一步，供植被的 World Position Offset 和 Niagara 使用。

```hlsl
// DShader/Passes/CP_WindField.dsp
#pragma pipeline(Order = 0, Views = Game | Editor)

uniform float2 WindDirection = float2(1, 0);
/// @slider 0 2
uniform float  Gust = 0.5;

/// @desc xy = the wind vector; tiles world XY every 50 m.
buffer Wind : RG16F(Size = int2(256, 256), History = true, Export = true, Clear = 0);

/// @desc Last frame's field advected along the wind and pulled toward a gusty target.
pass Simulate : compute
{
    Injection = BeginView;
    Dispatch  = Wind;
    read  Previous = Wind.Previous;
    write Result   = Wind;
    param Direction = WindDirection;
    param Strength  = Gust;

    hlsl
    {
        const float2 UV   = (Id.xy + 0.5) * ResultSize.zw;
        const float2 Prev = Previous.SampleLevel(DP_LinearClamp, UV - Direction * 0.002, 0).xy;
        const float  N    = frac(sin(dot(UV + DP_Time.x * 0.05, float2(12.9898, 78.233))) * 43758.5453);
        Result[Id.xy] = float4(lerp(Prev, Direction * (1.0 + (N - 0.5) * Strength), 0.05), 0, 0);
    }
}
```

```hlsl
// In the foliage material: sampled in the vertex shader
float3 WorldPos = UE.WorldPosition();
float2 Wind = UE.DreamPassBuffer(Pipeline = "CP_WindField", Buffer = "Wind",
                                 Coordinates = WorldPos.xy / 5000.0).rg;
m.WorldPositionOffset = float3(Wind * UE.VertexColor().r * 20.0, 0);
```

要点：

- `BeginView` 时还没有 view uniform buffer：代码用 `DP_Time` 而不是 `View.GameTime`，在这里写 `View` 会在那一行报编译错误。
- `read Previous = Wind.Previous` 和 `write Result = Wind` 是两张纹理 —— 上一帧的和本帧的。
- 每个编辑器视口各自推进自己的历史；只有一个会写导出的渲染目标。
- Niagara：把一个 Texture Sample 数据接口指向资产 `CP_WindField_Wind`。

<a id="scanner-pulse"></a>

## 扫描脉冲

每隔几秒，一圈光从相机向外扫过不透明场景，留下渐隐的拖尾：一个通过 view uniform buffer 读场景深度的像素 HLSL pass。

```hlsl
// DShader/Passes/CP_Scanner.dsp
#pragma pipeline(Order = 300)

/// @group Look
uniform float4 PulseColor = float4(0.1, 0.9, 1.0, 1.0);
/// @group Look   @slider 100 5000
uniform float  Speed = 1500.0;
/// @group Look   @slider 10 400
uniform float  Width = 150.0;
/// @slider 500 10000
uniform float  Range = 4000.0;

pass Pulse : fullscreen
{
    Injection = BeforePostProcess;
    read  InColor = SceneColor;
    write Out     = SceneColor;
    param PulseTint  = PulseColor;
    param PulseSpeed = Speed;
    param PulseWidth = Width;
    param PulseRange = Range;

    hlsl
    {
        const float3 Scene = DreamPassSample(InColor, UV).rgb;

        // The scene depth at this pixel, and from it the distance from the camera along the view ray.
        const float  Depth     = CalcSceneDepth(DreamPassSceneUV(UV));
        const float2 ScreenPos = ViewportUVToScreenPos(UV);
        const float3 Position  = mul(float4(ScreenPos * Depth, Depth, 1), View.ScreenToTranslatedWorld).xyz;
        const float  Distance  = length(Position - View.TranslatedWorldCameraOrigin);

        // The front is at the distance a pulse has travelled; every pixel it has passed gets the trail, the pixels at
        // the front the bright edge.
        const float Front  = frac(DP_Time.x * PulseSpeed / PulseRange) * PulseRange;
        const float Behind = Front - Distance;
        const float Trail  = Behind > 0.0 ? exp(-Behind / (PulseWidth * 3.0)) * 0.35 : 0.0;
        const float Edge   = saturate(1.0 - abs(Behind) / PulseWidth);
        const float Fade   = 1.0 - saturate(Front / PulseRange);

        const float Amount = (Trail + Edge * Edge) * Fade * PulseTint.a * DP_Weight;
        Out = float4(Scene + PulseTint.rgb * Amount, 1.0);
    }
}
```

要点：

- 块里是入口的语句：`UV` 是像素在视图里的位置，`Out` 是这个 pass 唯一的 write，即它的 `SV_Target0`，语句运行前清零。
  一边写 `SceneColor` 一边读它没问题：pass 画进一张临时纹理，再复制回去。
- `View` 和 `CalcSceneDepth` 从 `BeforeBasePass` 起可用。`DreamPassSceneUV(UV)` 是这个像素在场景纹理里的 UV（经由 `View.ViewRectMin` 和 `View.BufferSizeAndInvSize`）；`DreamPassSample(InColor, UV)` 是视图画面里的颜色。
- 绑定名会变成宏，所以叫 `PulseTint`、`PulseSpeed`，而不是引擎头文件也在用的 `Color` 或 `Speed`。
- 加上去的光在预曝光的场景颜色里，所以任何曝光下看起来都一样。覆盖 `PulseColor` 的体积会改变它里面那些视图的光圈颜色。

<a id="old-crt"></a>

## 老式 CRT

在 tonemap 后的画面上加边缘色散、扫描线和暗角，按管线的权重混进来。

```hlsl
// DShader/Passes/CP_Retro.dsp
#pragma pipeline(Order = 950, Requires = PostProcess)

/// @slider 0 1
uniform float Scanlines = 0.35;
/// @slider 0 8
uniform float Fringe = 3.0;
/// @slider 0 1
uniform float Vignette = 0.6;

pass Crt : fullscreen
{
    Injection = PostProcess.AfterTonemap;
    read  InColor = SceneColor;
    write Out     = SceneColor;
    param ScanlineAmount = Scanlines;
    param FringePixels   = Fringe;
    param VignetteAmount = Vignette;

    hlsl
    {
        // A whole function, the entry by its name: Main.
        void Main(float4 SvPosition : SV_POSITION, out float4 OutColor0 : SV_Target0)
        {
            const float2 ViewSize   = float2(DP_ViewRect.zw - DP_ViewRect.xy);
            const float2 ViewportUV = (SvPosition.xy - float2(DP_ViewRect.xy)) / ViewSize;
            const float2 Centered   = ViewportUV - 0.5;

            const float3 Original = DreamPassSample(InColor, ViewportUV).rgb;

            // Red a little outward, blue a little inward, by up to FringePixels at the corners.
            const float2 Shift = Centered * 2.0 * FringePixels / ViewSize;
            float3 Crt;
            Crt.r = DreamPassSample(InColor, ViewportUV + Shift).r;
            Crt.g = Original.g;
            Crt.b = DreamPassSample(InColor, ViewportUV - Shift).b;

            // Every other row darker, and the corners darker still.
            Crt *= 1.0 - ScanlineAmount * fmod(floor(SvPosition.y), 2.0);
            Crt *= 1.0 - VignetteAmount * saturate(dot(Centered, Centered) * 2.5);

            OutColor0 = float4(lerp(Original, Crt, DP_Weight), 1.0);
        }
    }
}
```

要点：

- 装整个函数的块就是 `.usf` 装的东西；运行的是 `Main`，除非 pass 用 `Entry` 点名别的。这时像素 pass 的 `write` 只给出尺寸（`OutSize`）；颜色是 `SV_Target0`。
- `PostProcess.AfterTonemap` 在上采样之后：`DP_ViewRect` 是输出矩形，一个纹素就是一个屏幕像素，所以隔行正好是屏幕上的隔行。
- `DP_Weight` 是管线在这个视图里的权重：带 `BlendWeight = 0.5` 的体积，或相机正在淡出的体积，给出一半的效果。
- `Requires = PostProcess`：没有后处理的视图里这条管线关掉。

<a id="tagged-objects"></a>

## 带标记的物体

游戏给物体打上 custom stencil 5，它们就会发出由自己材质写的流动条纹光：`Mode = Own` 的按 stencil 选择的 mesh pass，再接一个材质合成。

```hlsl
// DShader/Passes/CP_Tagged.dsp
#pragma pipeline(Order = 150, Requires = CustomStencil)

uniform float4 TagColor = float4(0.2, 1.0, 0.3, 1.0);

/// @desc r = the stripe, g = covered.
buffer Tag : RGBA16F(Clear = 0);

pass DrawTagged : mesh
{
    Injection = AfterOpaque;
    Filter    = Stencil(5);
    Mode      = Own;
    write Output0 = Tag;
}

pass Composite : fullscreen
{
    Injection = BeforePostProcess;
    Material  = "PP_TaggedComposite";
    read  Tag;
    write SceneColor;
    param Tint = TagColor;
}
```

```hlsl
// DShader/Passes/M_DemoTagged.dss
uniform float3 BaseTint = float3(0.9, 0.7, 0.2);
uniform float  StripeSize = 40.0;
uniform float  StripeSpeed = 0.5;

export void M_DemoTagged(inout material m)
{
    m.BaseColor = BaseTint;
    m.Roughness = 0.35;
    m.Metallic = 1.0;
    float3 P = UE.WorldPosition();
    float Stripe = step(0.5, frac((P.x + P.y + P.z) / StripeSize + UE.Time() * StripeSpeed));
    UE.DreamPassOutput(Output0 = float4(Stripe, 1.0, 0.0, 1.0));
}
```

```hlsl
// DShader/Passes/PP_TaggedComposite.dss
#pragma material(Domain = PostProcess, BlendableLocation = SceneColorAfterDOF)

uniform float4 Tint = float4(0.2, 1.0, 0.3, 1.0);
uniform float  DreamPassWeight = 1.0;

export void PP_TaggedComposite(inout material m)
{
    float2 UV    = UE.ScreenPosition().ViewportUV;
    float3 Scene = UE.SceneTexture(SceneTextureId = PostProcessInput0, Coordinates = UV).Color.rgb;
    float4 Tag   = UE.UserSceneTexture(UserSceneTexture = "Tag", Coordinates = UV).Color;
    float  Amount = (Tag.g * 0.25 + Tag.r * 0.75) * Tint.a * DreamPassWeight;
    m.EmissiveColor = lerp(Scene, Tint.rgb, Amount);
}
```

要点：

- `Mode = Own` 用每个选中物体自己的材质来画，读那个材质的
  `UE.DreamPassOutput`：不用覆盖材质就能拿到逐物体数据。在 base pass 里这个节点什么都不做。
- `Filter = Stencil(5)` 按 custom stencil 选，需要 `r.CustomDepth=3`，物体上要开 *Render
  CustomDepth Pass* 并把 *CustomDepth Stencil Value* 设为 5。`Requires = CustomStencil` 让没有它的视图里整条管线关掉。
- stencil 筛选下 Nanite 成员默认 `Nanite = StencilMask`：它们的像素由 custom
  stencil 用 `NaniteValue` 填上，而不是用它们的材质画。

<a id="see-also"></a>

## 另见

- [Custom Pass 管线 — `.dsp`](../language-v2/passes.md)
- [Custom Pass 运行时](../runtime/index.md)
- [HLSL pass](../../runtime/hlsl.md)
- [Custom Pass 材质节点](../builtins/dream-pass.md)
