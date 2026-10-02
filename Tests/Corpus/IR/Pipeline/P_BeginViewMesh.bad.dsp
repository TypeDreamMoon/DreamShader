// A mesh pass at BeginView, where the view has no depth to draw against yet.
buffer Mask : R8;

pass Draw : mesh
{
    Injection = BeginView;
    Filter = Stencil(1);
    write Output0 = Mask;
}
