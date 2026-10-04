// The X-ray of Docs/examples/custom-pass.md: a mesh pass by layer and list with its own depth, composited against the
// scene depth.
#pragma pipeline(Order = 200)

uniform float4 XRayColor = float4(0.2, 0.8, 1.0, 0.6);

buffer ObjDepth   : Depth32;
/// @desc r = covered, g = the object's scene depth.
buffer Silhouette : RG16F(Clear = 0);

pass DrawObjects : mesh
{
    Injection = AfterOpaque;
    Filter    = Layer(XRay) | List(Enemies);
    Material  = "M_XRayDepth";
    Depth     = Own(ObjDepth);
    write Output0 = Silhouette;
}

pass Composite : fullscreen
{
    Injection = BeforePostProcess;
    Material  = "PP_XRayComposite";
    read  Silhouette;
    write SceneColor;
    param Tint = XRayColor;
}
