// A param is 'param Target = Value;'.
buffer Mask : R8;

pass Blur : compute
{
    Shader = "Blur.usf";
    Entry = BlurCS;
    write Result = Mask;
    param Gain 2.0;
}
