// CP_Retro.dsp -- an old CRT after the tonemapper: colour fringes toward the edges, scanlines, a vignette. Blended by
// the pipeline's weight, so a volume's BlendWeight sets how much of it shows.
#pragma pipeline(Order = 950, Requires = PostProcess)

/// @slider 0 1
uniform float Scanlines = 0.35;
/// @slider 0 8
uniform float Fringe = 3.0;
/// @slider 0 1
uniform float Vignette = 0.6;

pass Crt : fullscreen
{
    Injection = PostProcess.AfterTonemap;
    Shader    = "Retro.usf";
    Entry     = CrtPS;
    read  InColor = SceneColor;
    write Out     = SceneColor;
    param ScanlineAmount = Scanlines;
    param FringePixels   = Fringe;
    param VignetteAmount = Vignette;
}
