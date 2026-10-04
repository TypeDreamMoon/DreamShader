// CP_Matrix.dsp -- the world as falling code: drops of glyphs rain down the screen, and the scene shows through as the
// code it is made of -- its outlines stand lit in the dark, its bright parts are faint standing code, and the drops burn
// brighter where the picture is bright.
// The outlines are found before post processing, where the depth is; the rain is drawn after the tonemapper.
#pragma pipeline(Order = 970, Requires = PostProcess)

/// @group Rain   @slider 6 24
uniform float  GlyphWidth = 12.0;
/// @group Rain   @slider 2 40
uniform float  FallSpeed = 18.0;
/// @group Rain   @slider 2 40
uniform float  TrailLength = 10.0;
/// @group Look
uniform float4 CodeColor = float4(0.15, 1.0, 0.35, 1.0);
/// @group Look   @slider 0 3
uniform float  SceneGlow = 1.2;

/// @desc 1 along the outlines of things.
buffer Outline : R8(Resolution = Render);

hlsl
{
    // Shared: a hash of a 2D point, 0..1.
    float MatrixHash(float2 Point)
    {
        return frac(sin(dot(Point, float2(127.1, 311.7))) * 43758.5453);
    }

    // The distance from P to the segment from A to B.
    float MatrixSegment(float2 P, float2 A, float2 B)
    {
        const float2 PA = P - A;
        const float2 BA = B - A;
        return length(PA - BA * saturate(dot(PA, BA) / dot(BA, BA)));
    }

    // A made-up alphabet: strokes of a sixteen-segment display, about six of them lit from a hash of the glyph, so the
    // glyphs read as letters of a script nobody knows.
    float MatrixGlyph(float2 InCell, float GlyphSeed)
    {
        const float4 Strokes[16] = {
            float4(0.0, 0.0, 0.5, 0.0), float4(0.5, 0.0, 1.0, 0.0), float4(0.0, 1.0, 0.5, 1.0), float4(0.5, 1.0, 1.0, 1.0),
            float4(0.0, 0.5, 0.5, 0.5), float4(0.5, 0.5, 1.0, 0.5), float4(0.0, 0.0, 0.0, 0.5), float4(0.0, 0.5, 0.0, 1.0),
            float4(1.0, 0.0, 1.0, 0.5), float4(1.0, 0.5, 1.0, 1.0), float4(0.5, 0.0, 0.5, 0.5), float4(0.5, 0.5, 0.5, 1.0),
            float4(0.0, 0.0, 0.5, 0.5), float4(1.0, 0.0, 0.5, 0.5), float4(0.0, 1.0, 0.5, 0.5), float4(1.0, 1.0, 0.5, 0.5) };
        const float2 P = (InCell - float2(0.2, 0.12)) / float2(0.6, 0.76);
        float Lit = 0.0;
        [unroll] for (int S = 0; S < 16; ++S)
        {
            const float On = step(0.62, MatrixHash(float2(S * 1.37, GlyphSeed * 53.1)));
            Lit = max(Lit, On * (1.0 - smoothstep(0.07, 0.13, MatrixSegment(P, Strokes[S].xy, Strokes[S].zw))));
        }
        return Lit;
    }
}

/// @desc Where the depth breaks: 1 / depth is linear across a flat surface on screen.
pass Edges : fullscreen
{
    Injection = BeforePostProcess;
    write OutlineOut = Outline;

    hlsl
    {
        const float2 Step = 2.5 / float2(DP_ViewRect.zw - DP_ViewRect.xy);
        const float  Here = 1.0 / min(CalcSceneDepth(DreamPassSceneUV(UV)), 1e6);
        float Around = 0.0;
        [unroll] for (int y = -1; y <= 1; ++y)
        {
            [unroll] for (int x = -1; x <= 1; ++x)
            {
                Around += (x == 0 && y == 0) ? 0.0 : 1.0 / min(CalcSceneDepth(DreamPassSceneUV(UV + float2(x, y) * Step)), 1e6);
            }
        }
        OutlineOut = float4(smoothstep(0.015, 0.045, abs(Around - 8.0 * Here) / max(Here, 1e-6) / 8.0), 0.0, 0.0, 1.0);
    }
}

pass Rain : fullscreen
{
    Injection = PostProcess.AfterTonemap;
    read  InColor = SceneColor;
    read  Edge    = Outline;
    write Out     = SceneColor;
    param CellWidth = GlyphWidth;
    param Speed     = FallSpeed;
    param Trail     = TrailLength;
    param Code      = CodeColor;
    param Glow      = SceneGlow;

    hlsl
    {
        const float2 ViewSize = float2(DP_ViewRect.zw - DP_ViewRect.xy);
        const float2 CellSize = float2(CellWidth, CellWidth * 1.5);
        const float2 Cell     = floor(Pixel / CellSize);
        const float2 InCell   = frac(Pixel / CellSize);

        // Each column drops at a speed and from a start of its own: a head, a trail fading behind it, then dark until
        // the next drop.
        const float Column = MatrixHash(float2(Cell.x, 3.7));
        const float Span   = ViewSize.y / CellSize.y * 1.6 + Trail * 2.0;
        const float Head   = frac(DP_Time.x * lerp(0.4, 1.0, Column) * Speed / Span + Column * 7.31) * Span;
        const float Behind = Head - Cell.y;
        const float Fade   = Behind >= 0.0 ? exp(-Behind / Trail) : 0.0;
        const float AtHead = saturate(1.0 - abs(Behind - 0.5));

        // The glyphs change now and then, each at its own pace.
        const float Change = floor(DP_Time.x * lerp(1.0, 9.0, MatrixHash(Cell + 0.5)));
        const float Glyph  = MatrixGlyph(InCell, MatrixHash(Cell + Change * 0.137));

        // The scene under the cell: its outlines anywhere in the cell stand lit, its bright parts faintly; its
        // brightness feeds the drops.
        const float2 CellUV  = (Cell + 0.5) * CellSize / ViewSize;
        const float2 Quarter = CellSize * 0.3 / ViewSize;
        const float  Luma    = dot(DreamPassSample(InColor, CellUV).rgb, float3(0.3, 0.59, 0.11));
        float Shape = DreamPassSample(Edge, CellUV).r;
        Shape = max(Shape, DreamPassSample(Edge, CellUV + Quarter).r);
        Shape = max(Shape, DreamPassSample(Edge, CellUV - Quarter).r);
        Shape = max(Shape, DreamPassSample(Edge, CellUV + float2(Quarter.x, -Quarter.y)).r);
        Shape = max(Shape, DreamPassSample(Edge, CellUV - float2(Quarter.x, -Quarter.y)).r);
        const float Light = Fade * (0.25 + 0.75 * saturate(Luma * Luma * Glow + Shape)) + Shape * 0.85 + Luma * Luma * 0.3;

        float3 Rain = Code.rgb * Glyph * Light + float3(0.8, 1.0, 0.85) * Glyph * AtHead;
        Rain += Code.rgb * Luma * 0.05;
        Out = float4(lerp(DreamPassSample(InColor, UV).rgb, Rain, DP_Weight), 1.0);
    }
}
