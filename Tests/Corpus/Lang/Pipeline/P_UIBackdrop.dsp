// The UI frosted glass of Docs/examples/custom-pass.md: a copy grab and a compute downsample inside the post-process
// chain, at output resolution.
#pragma pipeline(Order = 900, Views = Game, Requires = PostProcess)

/// @slider 1 12
uniform float BlurRadius = 6.0;

buffer Half    : RGBA8(Scale = 0.5,  Resolution = Output);
buffer Quarter : RGBA8(Scale = 0.25, Resolution = Output, Export = true);

pass Grab : copy
{
    Injection = PostProcess.AfterTonemap;
    read  SceneColor;
    write Half;
}

pass Down : compute
{
    Injection = PostProcess.AfterTonemap;
    Shader    = "Downsample.usf";
    Entry     = DownCS;
    Dispatch  = Quarter;
    read  Source = Half;
    write Result = Quarter;
    param Radius = BlurRadius;
}
