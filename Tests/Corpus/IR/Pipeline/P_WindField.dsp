// The Wind field of Docs/examples/custom-pass.md: a fixed-size history buffer advanced at BeginView, its last frame
// read as `.Previous`.
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
