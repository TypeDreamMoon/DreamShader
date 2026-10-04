// CP_Shockwave.dsp -- a shockwave: a sphere of compressed air grows from Epicenter, bends the picture behind its front
// and splits it into colours, and burns a ring onto whatever it passes through, bright enough for the bloom to catch.
// Radius is the gameplay's to drive: a Blueprint sets it with SetFloatParameter as the blast grows (TakeShots.py
// does, frame by frame). The statements of a pixel pass's entry: UV is the pixel's place in the view, Out its write.
#pragma pipeline(Order = 350)

/// @group Blast
uniform float3 Epicenter = float3(60.0, 0.0, 0.0);
/// @group Blast   @slider 0 4000
uniform float  Radius = 800.0;
/// @group Blast   @slider 20 600
uniform float  Thickness = 160.0;
/// @group Blast   @slider 500 10000
uniform float  FadeRadius = 1600.0;
/// @group Look    @slider 0 0.1
uniform float  Refraction = 0.06;
/// @group Look
uniform float4 RimColor = float4(1.0, 0.45, 0.12, 1.0);
/// @group Look    @slider 0 40
uniform float  RimIntensity = 7.0;

pass Blast : fullscreen
{
    Injection = BeforePostProcess;
    read  InColor = SceneColor;
    write Out     = SceneColor;
    param Center     = Epicenter;
    param WaveRadius = Radius;
    param WaveWidth  = Thickness;
    param WaveFade   = FadeRadius;
    param Bend       = Refraction;
    param Rim        = RimColor;
    param RimPower   = RimIntensity;

    hlsl
    {
        // The surface behind this pixel, the camera and the blast, in translated world space.
        const float  Depth   = min(CalcSceneDepth(DreamPassSceneUV(UV)), 1e6);
        const float3 Surface = mul(float4(ViewportUVToScreenPos(UV) * Depth, Depth, 1.0), View.ScreenToTranslatedWorld).xyz;
        const float3 Eye     = View.TranslatedWorldCameraOrigin;
        const float3 Ray     = normalize(Surface - Eye);
        const float3 Origin  = Center + View.PreViewTranslationHigh + View.PreViewTranslationLow;
        const float  Fade    = saturate(1.0 - WaveRadius / WaveFade) * DP_Weight;

        // The air: the ray passes the blast's centre at Closest, and the front bends it where the ray grazes the sphere --
        // outward just outside the front, inward just inside it. A surface in front of the sphere hides the bend.
        const float Along    = dot(Origin - Eye, Ray);
        const float Closest  = length(Origin - Eye - Ray * Along);
        const float Across   = (Closest - WaveRadius) / WaveWidth;
        const float NearSide = Along - sqrt(max(WaveRadius * WaveRadius - Closest * Closest, 0.0));
        const float Visible  = saturate((length(Surface - Eye) - NearSide + WaveWidth) / WaveWidth);
        const float Lens     = -Across * exp(-2.0 * Across * Across) * 2.33 * Visible * Fade;

        // Away from the blast's centre on screen, the same distance either way.
        const float2 ViewSize   = float2(DP_ViewRect.zw - DP_ViewRect.xy);
        const float2 Aspect     = float2(ViewSize.x / ViewSize.y, 1.0);
        const float4 CenterClip = mul(float4(Origin, 1.0), View.TranslatedWorldToClip);
        const float2 CenterUV   = ScreenPosToViewportUV(CenterClip.xy / max(CenterClip.w, 1e-3));
        const float2 Away       = (UV - CenterUV) * Aspect;
        const float2 Offset     = Away / max(length(Away), 1e-4) / Aspect * Lens * Bend;

        // Red bent the most and blue the least: the front's edge splits into colours.
        float3 Scene;
        Scene.r = DreamPassSample(InColor, UV - Offset * 1.3).r;
        Scene.g = DreamPassSample(InColor, UV - Offset).g;
        Scene.b = DreamPassSample(InColor, UV - Offset * 0.7).b;

        // The ring the front burns onto the surfaces it passes through -- thinner than the bend, white-hot at its core --
        // and the dust it pushes, darkening the ground just behind it.
        const float  Inside = (length(Surface - Origin) - WaveRadius) / (WaveWidth * 0.6);
        const float  Burn   = exp(-3.0 * Inside * Inside) * Fade;
        const float  Dust   = exp(-2.0 * (Inside + 2.0) * (Inside + 2.0)) * 0.3 * Fade;
        const float3 Hot    = lerp(Rim.rgb, float3(1.0, 0.95, 0.85), Burn * Burn * Burn);
        Out = float4(Scene * (1.0 - Dust) + Hot * Burn * RimPower, 1.0);
    }
}
