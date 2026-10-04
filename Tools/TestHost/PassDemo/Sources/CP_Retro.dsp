// CP_Retro.dsp -- an old CRT after the tonemapper: colour fringes toward the edges, scanlines, a vignette. Blended by
// the pipeline's weight, so a volume's BlendWeight sets how much of it shows. Its block holds a whole function, which
// runs as the entry under the name a block's entry has unless the pass says otherwise: Main.
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
        // On the tonemapped picture at output resolution.
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
