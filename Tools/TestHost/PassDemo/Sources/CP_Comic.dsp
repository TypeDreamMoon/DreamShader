// CP_Comic.dsp -- the scene as a comic book page: ink where the depth breaks or bends and where the colour jumps, flat
// colours in a few tones, halftone dots in the shadows, paper where it is white. The ink is found before post
// processing, where the depth is, into a buffer of its own; the page is drawn after the tonemapper, on the final colours.
#pragma pipeline(Order = 960, Requires = PostProcess)

/// @group Ink    @slider 0.5 4
uniform float  LineWidth = 1.5;
/// @group Ink    @slider 0.005 0.2
uniform float  DepthSensitivity = 0.03;
/// @group Ink    @slider 0.05 2
uniform float  ColorSensitivity = 0.35;
/// @group Page   @slider 2 8
uniform float  Tones = 4.0;
/// @group Page   @slider 3 16
uniform float  DotSize = 6.0;
/// @group Page
uniform float3 InkColor = float3(0.04, 0.03, 0.07);
/// @group Page
uniform float3 PaperColor = float3(1.0, 0.96, 0.86);

/// @desc x: 1 where a line is inked; y: 1 where the sky is.
buffer Ink : RG8(Resolution = Render);

hlsl
{
    // Shared: hue, saturation and value, and back again.
    float3 ComicToHsv(float3 Rgb)
    {
        const float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
        const float4 P = Rgb.g < Rgb.b ? float4(Rgb.bg, K.wz) : float4(Rgb.gb, K.xy);
        const float4 Q = Rgb.r < P.x ? float4(P.xyw, Rgb.r) : float4(Rgb.r, P.yzx);
        const float D = Q.x - min(Q.w, Q.y);
        return float3(abs(Q.z + (Q.w - Q.y) / (6.0 * D + 1e-5)), D / (Q.x + 1e-5), Q.x);
    }

    float3 ComicToRgb(float3 Hsv)
    {
        const float3 P = abs(frac(Hsv.xxx + float3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
        return Hsv.z * lerp(float3(1.0, 1.0, 1.0), saturate(P - 1.0), Hsv.y);
    }

    float ComicInverseDepth(float2 ViewportUV)
    {
        return 1.0 / min(CalcSceneDepth(DreamPassSceneUV(ViewportUV)), 1e6);
    }
}

/// @desc 1 / depth is linear across a flat surface on screen: where its Laplacian is not zero, an edge or a crease is.
pass Lines : fullscreen
{
    Injection = BeforePostProcess;
    read  Lit       = SceneColor;
    write InkOut    = Ink;
    param Width     = LineWidth;
    param DepthEdge = DepthSensitivity;
    param ColorEdge = ColorSensitivity;

    hlsl
    {
        const float2 Step = Width / float2(DP_ViewRect.zw - DP_ViewRect.xy);
        const float  Here = ComicInverseDepth(UV);
        float Around = 0.0;
        float Gx = 0.0;
        float Gy = 0.0;
        [unroll] for (int y = -1; y <= 1; ++y)
        {
            [unroll] for (int x = -1; x <= 1; ++x)
            {
                if (x == 0 && y == 0)
                {
                    continue;
                }
                const float2 At = UV + float2(x, y) * Step;
                Around += ComicInverseDepth(At);
                // Sobel on the log of the luminance: the colour's own lines, the checker's and the shadows' edges.
                const float Luma = log2(dot(DreamPassSample(Lit, At).rgb, float3(0.3, 0.59, 0.11)) + 1e-3);
                const float Weight = (x == 0 || y == 0) ? 2.0 : 1.0;
                Gx += Luma * x * Weight;
                Gy += Luma * y * Weight;
            }
        }
        const float Laplacian = abs(Around - 8.0 * Here) / max(Here, 1e-6) / 8.0;
        const float DepthInk  = smoothstep(DepthEdge, DepthEdge * 2.0, Laplacian);
        const float ColorInk  = smoothstep(ColorEdge, ColorEdge * 2.0, length(float2(Gx, Gy)) * 0.25);
        InkOut = float4(max(DepthInk, ColorInk), Here < 1e-5 ? 1.0 : 0.0, 0.0, 1.0);
    }
}

/// @desc The page: flat tones, dots, paper, ink.
pass Page : fullscreen
{
    Injection = PostProcess.AfterTonemap;
    read  InColor = SceneColor;
    read  InkMask = Ink;
    write Out     = SceneColor;
    param Levels  = Tones;
    param Dot     = DotSize;
    param InkTint = InkColor;
    param Paper   = PaperColor;

    hlsl
    {
        const float3 Color = DreamPassSample(InColor, UV).rgb;
        const float2 Marks = DreamPassSample(InkMask, UV).rg;
        const float  Sky   = Marks.y;
        const float3 Hsv   = ComicToHsv(Color);

        // A few flat tones, a little more saturated than life; the sky in bands of its own, never paler than a
        // printed blue.
        const float  Tone = saturate((floor(Hsv.z * Levels) + 0.75) / Levels);
        const float  Saturation = lerp(saturate(Hsv.y * 1.4), max(saturate(Hsv.y * 2.4), 0.5), Sky);
        float3 Page = ComicToRgb(float3(Hsv.x, Saturation, Tone));

        // Halftone dots on a grid at 45 degrees, in the darker tones only and bigger the darker; none in the sky.
        const float2 Grid   = float2(Pixel.x + Pixel.y, Pixel.y - Pixel.x) * 0.70710678 / Dot;
        const float  Centre = length(frac(Grid) - 0.5) * 2.0;
        const float  Size   = sqrt(saturate((0.72 - Hsv.z) * 1.6));
        const float  InDot  = (1.0 - smoothstep(Size - 0.15, Size + 0.15, Centre)) * (1.0 - Sky);
        Page = lerp(Page, Page * 0.3, InDot);

        // White becomes paper, the sky excepted; the lines go on last.
        Page = lerp(Page, Paper, smoothstep(0.8, 0.95, Hsv.z) * (1.0 - Hsv.y) * (1.0 - Sky));
        Page = lerp(Page, InkTint, Marks.x);
        Out = float4(lerp(Color, Page, DP_Weight), 1.0);
    }
}
