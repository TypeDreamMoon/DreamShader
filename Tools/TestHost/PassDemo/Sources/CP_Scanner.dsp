// CP_Scanner.dsp -- a sonar pulse: a ring of light sweeps out from the camera over the opaque scene, leaving a fading
// trail. A pixel HLSL pass that reads the scene depth through the view uniform buffer, written as the statements of
// its entry: UV is the pixel's place in the view, Out the pass's one write.
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
