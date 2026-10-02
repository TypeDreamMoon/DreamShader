// CP_UIBackdrop.dsp -- the picture after tonemapping, downsampled and blurred, for a frosted-glass background
// (M_DemoFrosted reads the export). The blur is an entry of the file's `hlsl` block, which the pass names.
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
