// {FILENAME} -- a DreamShader Custom Pass pipeline: a compute chain. Compiles to {ASSETPATH}.
// A compute shader advances a 256 x 256 wind field every frame from last frame's field -- the buffer keeps a history --
// and the field is exported as the render target {STEM}_Wind: a material reads it with
// UE.DreamPassBuffer(Pipeline = "{STEM}", Buffer = "Wind"), Niagara and Blueprints as that render target.
//
// The shader is the pass's `hlsl` block: the statements of its entry, whose function the compiler writes; Id is the
// thread's texel, and the threads past the dispatch have returned already. The pass's names are defined for it:
// Previous and Result are its `read` and `write` textures, PreviousSize and ResultSize their sizes (xy in texels, zw one
// over them), Direction and Strength its `param`s. Saving the file pre-checks the shader; an error is reported at its
// line, and the last version that compiled keeps running: the engine runs a snapshot that passed the pre-check, kept in
// DShader/.dreampass/ -- commit that folder with your sources. BeginView comes before the view's uniform buffer exists:
// DP_Time stands in for View's time. A pipeline runs where something activates it: the Global Pipelines of Project
// Settings > DreamPlugin > DreamShader Custom Pass, or a Dream Pass Volume.
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
    Dispatch  = Wind;
    read  Previous = Wind.Previous;
    write Result   = Wind;
    param Direction = WindDirection;
    param Strength  = Gust;

    hlsl
    {
        const float2 UV     = (Id.xy + 0.5) * ResultSize.zw;
        const float2 Upwind = Previous.SampleLevel(DP_LinearClamp, UV - Direction * 0.002, 0).xy;
        const float  Noise  = frac(sin(dot(UV + DP_Time.x * 0.05, float2(12.9898, 78.233))) * 43758.5453);
        const float2 Target = Direction * (1.0 + (Noise - 0.5) * Strength);
        Result[Id.xy] = float4(lerp(Upwind, Target, 0.05), 0, 0);
    }
}
