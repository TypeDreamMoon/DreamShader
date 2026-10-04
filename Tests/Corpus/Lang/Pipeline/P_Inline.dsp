// HLSL in the .dsp (DreamShader_Plan/10): the file's block and a pass's, each verbatim -- comments, `#` lines and
// braces in strings and comments included -- and `hlsl` as a name nowhere else.
#pragma pipeline(Order = 400)

uniform float Gain = 1.0;

buffer Mask : R8;
buffer Soft : R8;

hlsl
{
    // A helper every inline pass is compiled with. A } in a comment ends nothing.
    #define HALF 0.5
    float Halve(float X)
    {
        return X * HALF; /* { */
    }

    [numthreads(8, 8, 1)]
    void FillCS(uint3 Id : SV_DispatchThreadID)
    {
        Result[Id.xy] = Halve(Strength);
    }
}

pass Fill : compute
{
    Injection = AfterOpaque;
    Entry = FillCS;
    write Result = Mask;
    param Strength = Gain;
}

/// The body form: the compiler writes the function around the statements.
pass Again : compute
{
    Injection = AfterOpaque;
    read Source = Mask;
    write Result = Soft; // trailing

    hlsl
    {
#if 1
        Result[Id.xy] = Source.Load(int3(Id.xy, 0));
#endif
    }
}
