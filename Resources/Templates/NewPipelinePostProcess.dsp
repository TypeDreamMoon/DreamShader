// {FILENAME} -- a DreamShader Custom Pass pipeline: a fullscreen post-process chain. Compiles to {ASSETPATH}.
// A copy grabs the lit scene at half size, which softens it, and a fullscreen pass blends the soft copy back over the
// scene, tinted. PP_{BASE}.dss, next to this file, is the Post Process material that pass draws.
//
// A pipeline runs where something activates it: the Global Pipelines of Project Settings > DreamPlugin > DreamShader
// Custom Pass, or a Dream Pass Volume. A volume's blend weight reaches the material as DreamPassWeight.
#pragma pipeline(Order = 100)

/// @group Look   @desc The colour the soft copy is tinted with; alpha is how much of it shows
uniform float4 Tint = float4(1.0, 0.85, 0.6, 1.0);

/// @group Look   @slider 0 1
uniform float Strength = 0.35;

/// @desc The lit scene at half resolution
buffer Half : RGBA16F(Scale = 0.5);

/// @desc Copy the scene into the half-size buffer: the copy's filtering is the blur
pass Grab : copy
{
    Injection = BeforePostProcess;
    read  SceneColor;
    write Half;
}

/// @desc Blend the soft copy back over the scene
pass Composite : fullscreen
{
    Injection = BeforePostProcess;
    Material  = "PP_{BASE}";
    read  Half;
    write SceneColor;
    param Tint     = Tint;
    param Strength = Strength;
}
