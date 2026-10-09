// Nonzero mip levels cannot be initialized or updated by a pass yet.
buffer Color : RGBA16F(Size = int2(16, 16), Mips = 2, Clear = float4(1.0, 0.0, 0.0, 1.0));

pass Show : copy
{
    Injection = BeforePostProcess;
    read Color;
    write SceneColor;
}
