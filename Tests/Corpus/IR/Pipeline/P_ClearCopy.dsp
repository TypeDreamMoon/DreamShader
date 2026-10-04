// A clear's Value broadcast from one number or given per channel, a buffer cleared to nothing, a narrower Clear padded
// with zeros, and a copy of last frame's contents.
buffer Field : RGBA16F(History = true, Clear = None);
buffer Grey  : R16F;
buffer Spot  : RGBA8(Clear = float3(1.0, 0.5, 0.25), Export = true);

pass Fill : clear
{
    Injection = AfterOpaque;
    Value = 0.5;
    write Grey;
}

pass Mark : clear
{
    Injection = AfterOpaque;
    Value = float4(1.0, 0.0, 0.0, 1.0);
    write Field;
}

pass Keep : copy
{
    read Field.Previous;
    write Spot;
}

pass Show : copy
{
    read Grey;
    write SceneColor;
}
