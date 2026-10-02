// Every shape a `param` folds to: a parameter or the pipeline weight times a number plus a number, and a constant,
// typed by its value.
uniform float Width = 2.0;
uniform float4 Tint = float4(1.0, 0.5, 0.25, 1.0);
uniform int Taps = 4;
static const float Twice = 2.0;

buffer Out : RGBA16F;

pass Blur : compute
{
    Shader = "Blur.usf";
    Entry = MainCS;
    write Result = Out;
    param A = Width * 2.0 + 1.0;
    param B = (Width - 1.0) * 0.25;
    param C = -Width;
    param D = 1.0 - Width;
    param E = DreamPassWeight * 0.5;
    param F = Twice * 3.0;
    param G = float3(1.0, 0.5, 0.25);
    param H = 7;
    param I = true;
    param J = Tint;
    param K = Taps + 1;
    param L = Width * Twice;
}
