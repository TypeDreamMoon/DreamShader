// CP_Tagged.dsp -- objects the game tags with custom stencil 5 glow with a pattern their OWN material writes
// (M_DemoTagged's UE.DreamPassOutput): a stencil-filtered mesh pass in Own mode, then a material composite.
#pragma pipeline(Order = 150, Requires = CustomStencil)

uniform float4 TagColor = float4(0.2, 1.0, 0.3, 1.0);

/// @desc r = the stripe, g = covered.
buffer Tag : RGBA16F(Clear = 0);

pass DrawTagged : mesh
{
    Injection = AfterOpaque;
    Filter    = Stencil(5);
    Mode      = Own;
    write Output0 = Tag;
}

pass Composite : fullscreen
{
    Injection = BeforePostProcess;
    Material  = "PP_TaggedComposite";
    read  Tag;
    write SceneColor;
    param Tint = TagColor;
}
