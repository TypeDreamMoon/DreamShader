# Custom Pass examples

> [DreamShader](../index.md) » [Examples](index.md) » **Custom Pass examples**

Four complete pipelines, each with the `.dss` materials and `.usf` shaders it uses. The render tests
(`DreamShader.Pass.Render.*`) build these. Two of the material browser's **New Source** pipeline templates are
cut-down versions of them — the mesh mask of the outline, and the wind field; the post-process template is a
soft glow of its own.

| | |
| :-- | :-- |
| Engine | UE `5.8` and later |
| Assumed source root | `<Project>/DShader`; each pipeline's materials and shaders next to it, in `DShader/Passes/` |

| Example | What it shows |
| :-- | :-- |
| [Highlight outline](#highlight-outline) | a mesh pass by layer with an override material, a compute blur, a fullscreen material composite, an exported buffer |
| [UI frosted glass](#ui-frosted-glass) | a `copy` grab and a compute downsample inside the post-process chain, exported for UMG |
| [X-ray](#x-ray) | a mesh pass by layer and list with its own depth, composited against the scene depth |
| [Wind field](#wind-field) | a compute pass at `BeginView` on a fixed-size history buffer, exported for a foliage material's WPO and for Niagara |

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

pass Blur : compute
{
    Injection = AfterOpaque;
    Shader    = "BoxBlur.usf";
    Entry     = BlurCS;
    Dispatch  = Blurred;
    read  Source = Mask;
    write Result = Blurred;
    param Radius = OutlineWidth;
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
// DShader/Passes/BoxBlur.usf
[numthreads(8, 8, 1)]
void BlurCS(uint3 Id : SV_DispatchThreadID)
{
    if (any(Id.xy >= DP_DispatchSize.xy))
    {
        return;
    }
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

pass Grab : copy
{
    Injection = PostProcess.AfterTonemap;
    read  SceneColor;
    write Half;
}

pass Down : compute
{
    Injection = PostProcess.AfterTonemap;
    Shader    = "Downsample.usf";
    Entry     = DownCS;
    Dispatch  = Quarter;
    read  Source = Half;
    write Result = Quarter;
    param Radius = BlurRadius;
}
```

```hlsl
// DShader/Passes/Downsample.usf
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
        const float2 Offset = float2(cos(i * 0.5236), sin(i * 0.5236)) * Radius * SourceSize.zw;
        Sum += Source.SampleLevel(DP_LinearClamp, UV + Offset, 0);
    }
    Result[Id.xy] = Sum / 12.0;
}
```

Notes:

- A pass on the post-process chain that does not write the scene colour leaves the chain as it was.
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
#pragma material(ShadingModel = Unlit, bUsedWithSkeletalMesh = true)

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

pass Simulate : compute
{
    Injection = BeginView;
    Shader    = "WindField.usf";
    Entry     = WindCS;
    Dispatch  = Wind;
    read  Previous = Wind.Previous;
    write Result   = Wind;
    param Direction = WindDirection;
    param Strength  = Gust;
}
```

```hlsl
// DShader/Passes/WindField.usf
[numthreads(8, 8, 1)]
void WindCS(uint3 Id : SV_DispatchThreadID)
{
    if (any(Id.xy >= DP_DispatchSize.xy))
    {
        return;
    }
    const float2 UV   = (Id.xy + 0.5) * ResultSize.zw;
    const float2 Prev = Previous.SampleLevel(DP_LinearClamp, UV - Direction * 0.002, 0).xy;
    const float  N    = frac(sin(dot(UV + DP_Time.x * 0.05, float2(12.9898, 78.233))) * 43758.5453);
    Result[Id.xy] = float4(lerp(Prev, Direction * (1.0 + (N - 0.5) * Strength), 0.05), 0, 0);
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

- At `BeginView` there is no view uniform buffer yet: the shader uses `DP_Time` rather than `View.GameTime`.
- `read Previous = Wind.Previous` and `write Result = Wind` are two textures — last frame's and this frame's.
- Every editor viewport advances its own history; only one writes the exported render target.
- Niagara: point a Texture Sample data interface at the asset `CP_WindField_Wind`.

## See also

- [Custom Pass pipelines — `.dsp`](../language-v2/passes.md)
- [Custom Pass runtime](../runtime/index.md)
- [HLSL passes](../runtime/hlsl.md)
- [Custom Pass material nodes](../builtins/dream-pass.md)
