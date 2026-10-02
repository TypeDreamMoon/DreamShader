// A named binding names its buffer after the '='.
buffer Mask : R8;
buffer Soft : R8;

pass Blur : compute
{
    Shader = "Blur.usf";
    Entry = BlurCS;
    read Source = ;
    write Result = Soft;
}
