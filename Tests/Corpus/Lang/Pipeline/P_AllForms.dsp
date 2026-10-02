// Every form of the pipeline grammar once: keyed pragma values with dots and bars, uniforms and a constant, buffer
// argument lists (empty, expressions, words), every pass kind, every statement kind, `.Previous` and string values.
#pragma pipeline(Order = -5, Injection = PostProcess.AfterDOF, Views = Game | SceneCapture, Requires = PostProcess | SceneResolve, Enabled = On)

uniform bool On = true;
/// @group Look   @slider 0 4   @sort 2
/// @desc How strong the effect is.
uniform float Strength = 1.5;
uniform float3 Tint = float3(1.0, 0.5, 0.25);
uniform Texture2D Noise;
static const float Half = 0.5;

buffer ObjDepth : Depth32;
buffer Mask : R8();
buffer Field : RG16F(Size = int2(64, 32), History = true, Clear = None);
buffer Blurred : RGBA16F(Scale = Half * 0.5, Resolution = Output, Mips = 2, Export = true);

pass Draw : mesh
{
    Filter = (Layer(Highlight | "Other Layer") | List(Enemies)) & Stencil(4, 0x0F);
    Material = "M_Mask";
    Mode = OwnOrOverride;
    Depth = Own(ObjDepth);
    Cull = None;
    Blend = Max;
    Usage = StaticMesh | SplineMesh;
    Nanite = AssignStencil(200);
    NaniteValue = float4(1.0, 0.0, 0.0, 1.0);
    write Output0 = Mask;
}

pass Advance : compute
{
    Injection = BeginView;
    Enabled = On;
    Shader = "/Project/Passes/Field.usf";
    Entry = "AdvanceCS";
    Threads = uint3(8, 4, 2);
    Dispatch = Field / 2;
    read Previous = Field.Previous;
    write Result = Field;
    param Gain = Strength * 2.0 - 1.0;
    param Weight = DreamPassWeight;
    param Flip = -Strength;
    param Fixed = float2(Half, 1.0);
}

pass Fill : clear
{
    Value = float4(0.25, 0.5, 0.75, 1.0);
    write Blurred;
}

pass Grab : copy
{
    read SceneColor;
    write Blurred;
}

pass Show : fullscreen
{
    Shader = "Show.usf";
    Entry = ShowPS;
    read Mask;
    read Blurred;
    write SceneColor;
    param Tint = Tint;
}
