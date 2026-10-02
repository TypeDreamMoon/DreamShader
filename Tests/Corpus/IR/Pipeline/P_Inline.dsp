// HLSL in the .dsp (DreamShader_Plan/10): an entry of the file's block, and a pass's own block of statements.
buffer Mask : R8;
buffer Soft : R8;

hlsl
{
    [numthreads(8, 8, 1)]
    void FillCS(uint3 Id : SV_DispatchThreadID)
    {
        Result[Id.xy] = 1;
    }
}

pass Fill : compute
{
    Entry = FillCS;
    write Result = Mask;
}

pass Again : compute
{
    read Source = Mask;
    write Result = Soft;

    hlsl
    {
        Result[Id.xy] = Source.Load(int3(Id.xy, 0));
    }
}
