// 'write' at the start of a pass statement begins a binding, which names a buffer.
buffer Mask : R8;

pass Fill : clear
{
    Value = 1.0;
    write;
}
