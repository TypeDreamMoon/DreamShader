// 'read' at the start of a pass statement begins a binding, which names a buffer.
buffer Mask : R8;

pass Keep : copy
{
    read;
    write Mask;
}
