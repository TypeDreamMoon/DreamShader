// {FILENAME} -- a DreamShader Custom Pass pipeline: a compute chain. Compiles to {ASSETPATH}.
// A compute shader advances a 256 x 256 wind field every frame from last frame's field -- the buffer keeps a history --
// and the field is exported as the render target {STEM}_Wind: a material reads it with
// UE.DreamPassBuffer(Pipeline = "{STEM}", Buffer = "Wind"), Niagara and Blueprints as that render target.
//
// The shader is {SHADERPATH}, next to this file. The engine runs a snapshot of it that passed a pre-check, kept in
// DShader/.dreampass/ -- commit that folder with your sources. A pipeline runs where something activates it: the Global
// Pipelines of Project Settings > DreamPlugin > DreamShader Custom Pass, or a Dream Pass Volume.
#pragma pipeline(Order = 0, Views = Game | Editor)

/// @group Wind   @desc The way the field drifts
uniform float2 WindDirection = float2(1, 0);

/// @group Wind   @slider 0 2
uniform float Gust = 0.5;

/// @desc xy = the wind vector
buffer Wind : RG16F(Size = int2(256, 256), History = true, Export = true, Clear = 0);

/// @desc One step of the field, before anything is drawn, so every material reads this frame's
pass Simulate : compute
{
    Injection = BeginView;
    Shader    = "{SHADERPATH}";
    Entry     = MainCS;
    Dispatch  = Wind;
    read  Previous = Wind.Previous;
    write Result   = Wind;
    param Direction = WindDirection;
    param Strength  = Gust;
}
