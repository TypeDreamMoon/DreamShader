// CP_WindField.dsp -- a 256x256 wind field advanced every frame by a compute shader, for the foliage's World Position
// Offset (M_DemoFoliage) and for anything else that reads the export. At BeginView, before the view uniform buffer
// exists: the pass's code may not use `View`, and DP_Time stands in for the timing values.
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
