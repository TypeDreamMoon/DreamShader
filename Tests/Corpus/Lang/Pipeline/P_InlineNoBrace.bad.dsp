// `hlsl` opens a block, and nothing else: without its `{` it is DSH2313.
buffer Mask : R8;

pass Fill : compute
{
    write Result = Mask;
    hlsl;
}
