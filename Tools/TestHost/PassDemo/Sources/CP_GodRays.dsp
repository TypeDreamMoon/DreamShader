// CP_GodRays.dsp -- light shafts: the sky around the sun, smeared toward the sun, so that whatever stands in front of
// it casts rays of shadow across the air. The sun is the one the sky atmosphere is lit by, wherever it is (the view's
// AtmosphereLightDirection), so the rays follow it. Two compute passes at half resolution and a pixel pass that adds the
// light to the scene before the bloom.
#pragma pipeline(Order = 400)

/// @group Shafts   @slider 0.05 1
uniform float  GlowSize = 0.35;
/// @group Shafts   @slider 0.9 1
uniform float  Decay = 0.975;
/// @group Shafts   @slider 0.2 1.5
uniform float  Density = 0.9;
/// @group Look     @slider 0 4
uniform float  Intensity = 1.2;
/// @group Look
uniform float4 Tint = float4(1.0, 0.8, 0.55, 1.0);
/// @group Look     @slider 1 50
uniform float  SkyLimit = 12.0;

/// @desc The sky near the sun; black where anything is in front of it.
buffer Shine : RGBA16F(Scale = 0.5);
/// @desc Shine, smeared toward the sun.
buffer Shafts : RGBA16F(Scale = 0.5);

hlsl
{
    // Shared: the sun's place in the view (xy, 0..1) and whether it is in front of the camera (z > 0). A direction has
    // no position, so w = 0: the point at infinity the sun is at.
    float3 GodRaysSun()
    {
        const float4 Clip = mul(float4(View.AtmosphereLightDirection[0].xyz, 0.0), View.TranslatedWorldToClip);
        return float3(ScreenPosToViewportUV(Clip.xy / max(abs(Clip.w), 1e-4)), Clip.w);
    }
}

pass Mask : compute
{
    Injection = BeforePostProcess;
    Dispatch  = Shine;
    read  Sky     = SceneColor;
    write ShineOut = Shine;
    param Glow    = GlowSize;
    param Limit   = SkyLimit;

    hlsl
    {
        const float2 UV  = (Id.xy + 0.5) * ShineOutSize.zw;
        const float3 Sun = GodRaysSun();
        // Only the sky lets the sun through: everything with a depth is in its way.
        const float Open = CalcSceneDepth(DreamPassSceneUV(UV)) > 1e5 ? 1.0 : 0.0;
        const float2 Aspect = float2(ShineOutSize.x * ShineOutSize.w, 1.0);
        const float Near = Sun.z > 0.0 ? pow(saturate(1.0 - length((UV - Sun.xy) * Aspect) / Glow), 2.0) : 0.0;
        ShineOut[Id.xy] = float4(min(DreamPassSample(Sky, UV).rgb, Limit) * Open * Near, 1.0);
    }
}

pass Smear : compute
{
    Injection = BeforePostProcess;
    Dispatch  = Shafts;
    read  ShineIn   = Shine;
    write ShaftsOut = Shafts;
    param Falloff   = Decay;
    param Reach     = Density;

    hlsl
    {
        const float2 UV  = (Id.xy + 0.5) * ShaftsOutSize.zw;
        const float3 Sun = GodRaysSun();
        float3 Sum = 0.0;
        if (Sun.z > 0.0)
        {
            // 64 taps from here toward the sun, each fainter than the one before.
            const float2 Step = (Sun.xy - UV) * Reach / 64.0;
            float2 At = UV;
            float Weight = 1.0;
            [loop] for (int i = 0; i < 64; ++i)
            {
                Sum += DreamPassSample(ShineIn, At).rgb * Weight;
                Weight *= Falloff;
                At += Step;
            }
            Sum /= 64.0;
        }
        ShaftsOut[Id.xy] = float4(Sum, 1.0);
    }
}

pass Add : fullscreen
{
    Injection = BeforePostProcess;
    read  InColor = SceneColor;
    read  Rays    = Shafts;
    write Out     = SceneColor;
    param RayTint  = Tint;
    param RayPower = Intensity;

    hlsl
    {
        Out = float4(DreamPassSample(InColor, UV).rgb + DreamPassSample(Rays, UV).rgb * RayTint.rgb * RayPower * DP_Weight, 1.0);
    }
}
