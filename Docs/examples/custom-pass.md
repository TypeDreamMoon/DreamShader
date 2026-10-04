# Custom Pass examples

> [DreamShader](../index.md) » [Examples](index.md) » **Custom Pass examples**

Complete pipelines, each with the `.dss` materials it uses; the HLSL of their HLSL passes is written in the `.dsp`
itself, in the three ways [HLSL passes](../runtime/hlsl.md#where-the-code-is) describes. Every one of them runs in the
test host's demo level, built from these sources and rendered in a game viewport (`Tools/TestHost/PassDemo`). Two of the
material browser's **New Source** pipeline templates are cut-down versions of them — the mesh mask of the outline,
and the wind field; the post-process template is a soft glow of its own.

| | |
| :-- | :-- |
| Engine | UE `5.8` and later |
| Assumed source root | `<Project>/DShader`; each pipeline's materials next to it, in `DShader/Passes/` |

| Example | What it shows |
| :-- | :-- |
| [Highlight outline](#highlight-outline) | a mesh pass by layer with an override material, a compute blur written as the statements of its entry, a fullscreen material composite, an exported buffer |
| [UI frosted glass](#ui-frosted-glass) | a `copy` grab and a compute downsample inside the post-process chain, its entry in the file's `hlsl` block with a helper; exported for UMG |
| [X-ray](#x-ray) | a mesh pass by layer and list with its own depth, composited against the scene depth |
| [Wind field](#wind-field) | a compute pass at `BeginView` on a fixed-size history buffer, exported for a foliage material's WPO and for Niagara |
| [Scanner pulse](#scanner-pulse) | a pixel HLSL pass at `BeforePostProcess`, the statements of its entry, that reads the scene depth through `View` |
| [Old CRT](#old-crt) | a pixel HLSL pass at `PostProcess.AfterTonemap` whose block holds a whole function, blended by the pipeline's weight |
| [Tagged objects](#tagged-objects) | a mesh pass by custom stencil that draws with each object's own material |

## Highlight outline

Objects with the `Highlight` pass layer get an orange outline, also behind walls, and the ground glows under them.
Give the actors a `UDreamPassLayerComponent` with the layer `Highlight` (Project Settings ▸ DreamShader Custom
Pass ▸ Layers), and activate the pipeline from the project settings or with a volume.

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

Notes:

- The ground is opaque and `Blurred` is written at `AfterOpaque`, so the ground reads **last frame's** blur.
  The compiler says so. Moving `DrawMask` and `Blur` to `BeforeBasePass` makes it this frame's; a mesh pass
  there can only use `Depth = None` or `Own`, which this one does.
- Nanite members of the `Highlight` layer are skipped, with one warning. `Nanite = AssignStencil(200)` on
  `DrawMask` would include them, as a stencil mask.

## UI frosted glass

The picture after tonemapping, downsampled and blurred, for a frosted-glass UMG background.

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

Notes:

- A pass on the post-process chain that does not write the scene colour leaves the chain as it was.
- The tonemapped picture's alpha is 0, and `Grab` copies it as it is; `DownCS` writes 1, or a UMG image of
  `Quarter` would be invisible.
- `DownCS` is an entry of the file's `hlsl` block: `Entry = DownCS;` alone picks it. A second pass naming it would
  run the same code with its own bindings; `CircleTap` is shared code, compiled for every pass whose HLSL is in the
  file.
- The export is for UI, which draws after the scene. A glass panel in the world that sampled it at its own place on
  screen would see itself in the grab.
- `Requires = PostProcess`: in a view without post processing the whole pipeline is off, not half of it.
- UMG renders after the scene, so it reads this frame's `Quarter`. In split screen only the first player's view
  writes the exported buffer.

## X-ray

Enemies behind walls show as a cyan silhouette.

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

Notes:

- Game code fills `List(Enemies)`: `UDreamPassSubsystem::AddToList("Enemies", Mesh)`, or *Add To List* in Blueprint.
- `Depth = Own(ObjDepth)`: the objects hide each other but not behind the walls.
- GPU-skinned skeletal meshes are drawn; CPU-skinned ones are skipped.

## Wind field

A 256×256 wind field advanced every frame by a compute shader, for the foliage's World Position Offset and for
Niagara.

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

Notes:

- At `BeginView` there is no view uniform buffer yet: the code uses `DP_Time` rather than `View.GameTime`, and a
  `View` in it would be a compile error at its line.
- `read Previous = Wind.Previous` and `write Result = Wind` are two textures — last frame's and this frame's.
- Every editor viewport advances its own history; only one writes the exported render target.
- Niagara: point a Texture Sample data interface at the asset `CP_WindField_Wind`.

## Scanner pulse

A ring of light sweeps out from the camera over the opaque scene every few seconds and leaves a fading trail: a
pixel HLSL pass that reads the scene depth through the view uniform buffer.

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

Notes:

- The block holds the statements of the entry: `UV` is the pixel's place in the view, and `Out`, the pass's one
  write, is its `SV_Target0`, cleared to 0 before the statements run. Reading `SceneColor` while writing it is
  fine: the pass draws into a scratch texture that is copied back.
- `View` and `CalcSceneDepth` work from `BeforeBasePass` on. `DreamPassSceneUV(UV)` is the scene textures' UV of
  the pixel (through `View.ViewRectMin` and `View.BufferSizeAndInvSize`); `DreamPassSample(InColor, UV)` the colour
  inside the view's picture.
- Binding names become macros, so they are `PulseTint` and `PulseSpeed`, not `Color` or `Speed` alone, which
  engine headers use.
- The added light is in pre-exposed scene colour, so it looks the same at any exposure. A volume that overrides
  `PulseColor` changes the ring's colour for the views inside it.

## Old CRT

Colour fringes toward the edges, scanlines and a vignette on the tonemapped picture, blended in by the
pipeline's weight.

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

Notes:

- A block that holds whole functions is what a `.usf` holds; the one that runs is `Main` unless the pass names
  another with `Entry`. A pixel pass's `write` gives only a size (`OutSize`) there; the colour is `SV_Target0`.
- `PostProcess.AfterTonemap` is after the upscaler: `DP_ViewRect` is the output rect and a texel is a screen
  pixel, so every other row is exactly every other row on screen.
- `DP_Weight` is the pipeline's weight in the view: a volume with `BlendWeight = 0.5`, or one the camera is
  fading out of, gives half the effect.
- `Requires = PostProcess`: in a view without post processing the pipeline is off.

## Tagged objects

Objects the game tags with custom stencil 5 glow with moving stripes that their own material writes: a mesh
pass by stencil in `Mode = Own`, then a material composite.

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

Notes:

- `Mode = Own` draws each selected object with its own material and reads that material's
  `UE.DreamPassOutput`: per-object data without an override. In the base pass the node does nothing.
- `Filter = Stencil(5)` selects by the custom stencil, which needs `r.CustomDepth=3` and, on the object, *Render
  CustomDepth Pass* with *CustomDepth Stencil Value* 5. `Requires = CustomStencil` keeps the whole pipeline off in
  a view without one.
- For a stencil filter, Nanite members default to `Nanite = StencilMask`: their pixels are filled from the custom
  stencil with `NaniteValue` rather than drawn with their material.

## See also

- [Custom Pass pipelines — `.dsp`](../language-v2/passes.md)
- [Custom Pass runtime](../runtime/index.md)
- [HLSL passes](../runtime/hlsl.md)
- [Custom Pass material nodes](../builtins/dream-pass.md)
