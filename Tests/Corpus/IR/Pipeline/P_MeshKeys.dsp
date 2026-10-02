// Every key of a mesh pass, each away from its default: the payload holds them as written, canonically spelled --
// the usage flags in their table's order.
buffer Depth : Depth32;
buffer Data  : RG16F;

pass Draw : mesh
{
    Injection = BeforeBasePass;
    Filter = Layer(Highlight);
    Material = "M_Data";
    Mode = OwnOrOverride;
    Depth = Own(Depth);
    Cull = Front;
    Blend = AlphaBlend;
    Usage = SplineMesh | StaticMesh | GeometryCache;
    Nanite = AssignStencil(200);
    NaniteValue = float4(0.5, 0.25, 0.0, 1.0);
    write Output1 = Data;
}
