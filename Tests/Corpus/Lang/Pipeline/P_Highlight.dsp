// The Highlight outline of Docs/examples/custom-pass.md, as written there: a mesh pass by layer with an override
// material, a compute blur and a fullscreen material composite.
#pragma pipeline(Order = 100)

/// @group Look
uniform float4 OutlineColor = float4(1.0, 0.6, 0.0, 1.0);
/// @group Look   @slider 1 8
uniform float  OutlineWidth = 3.0;

/// @desc What the objects cover.
buffer Mask    : R8(Clear = 0);
/// @desc The blurred mask; the ground material reads it too.
buffer Blurred : R8(Scale = 0.5, Export = true);

/// @desc No depth test: the outline shows through walls.
pass DrawMask : mesh
{
    Injection = AfterOpaque;
    Filter    = Layer(Highlight);
    Material  = "M_HighlightMask";
    Depth     = None;
    write Output0 = Mask;
}

pass Blur : compute
{
    Injection = AfterOpaque;
    Shader    = "BoxBlur.usf";
    Entry     = BlurCS;
    Dispatch  = Blurred;
    read  Source = Mask;
    write Result = Blurred;
    param Radius = OutlineWidth;
}

pass Composite : fullscreen
{
    Injection = BeforePostProcess;
    Material  = "PP_OutlineComposite";
    read  Mask;
    read  Blurred;
    write SceneColor;
    param Color = OutlineColor;
}
