// A mesh pass filter in its normal form: clauses joined by `|`, the terms of one by `&`, and `&` distributed over `|`;
// with a Stencil term the Nanite policy defaults to StencilMask, and without a Material the mode to Own.
buffer Mask : R8;

pass ByStencil : mesh
{
    Injection = AfterOpaque;
    Filter = Stencil(4, 0x0F) | Stencil(8);
    write Output0 = Mask;
}

pass ByLayer : mesh
{
    Injection = AfterOpaque;
    Filter = (Layer(A | B) | List(Enemies)) & Stencil(1);
    Mode = Own;
    Depth = TestScene;
    write Output0 = Mask;
}
