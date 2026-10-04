// CP_Highlight.dsp -- objects with the Highlight pass layer get an orange outline, also behind walls, and the ground
// glows under them (M_DemoGround reads the exported blur).
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

/// @desc A (2R+1)^2 box over the mask, into the half-size Blurred. The block holds the statements of the entry, whose
/// function the compiler writes: Id is the thread's texel, and the threads past the dispatch have returned already.
pass Blur : compute
{
    Injection = AfterOpaque;
    Dispatch  = Blurred;
    read  Source = Mask;
    write Result = Blurred;
    param Radius = OutlineWidth;

    hlsl
    {
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
