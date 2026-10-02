// `buffer` and `pass` begin a declaration only at file scope of a .dsp, and `read`, `write` and `param` begin a
// statement only inside a pass block: anywhere else they are ordinary names -- here parameters, a constant, a buffer
// read under its own name and a write's name inside the pass.
uniform float buffer = 1.0;
uniform float pass = 2.0;
static const float read = 0.5;

buffer Data : R8;
buffer write : R8;

pass Use : copy
{
    read write;
    write param = Data;
}
