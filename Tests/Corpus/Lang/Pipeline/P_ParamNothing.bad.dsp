// 'param' at the start of a pass statement begins a binding, which names its target.
buffer Mask : R8;

pass Blur : compute
{
    Shader = "Blur.usf";
    Entry = BlurCS;
    write Result = Mask;
    param;
}
