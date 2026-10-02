// 'param' at the start of a pass statement is a binding, never a setting named 'param': its target comes first.
buffer Mask : R8;

pass Blur : compute
{
    Shader = "Blur.usf";
    Entry = BlurCS;
    write Result = Mask;
    param = 3;
}
