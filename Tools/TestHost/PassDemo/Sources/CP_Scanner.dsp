// CP_Scanner.dsp -- a sonar pulse: a ring of light sweeps out from the camera over the opaque scene, leaving a fading
// trail. A pixel HLSL pass that reads the scene depth through the view uniform buffer.
#pragma pipeline(Order = 300)

/// @group Look
uniform float4 PulseColor = float4(0.1, 0.9, 1.0, 1.0);
/// @group Look   @slider 100 5000
uniform float  Speed = 1500.0;
/// @group Look   @slider 10 400
uniform float  Width = 150.0;
/// @slider 500 10000
uniform float  Range = 4000.0;

pass Pulse : fullscreen
{
    Injection = BeforePostProcess;
    Shader    = "Scanner.usf";
    Entry     = PulsePS;
    read  InColor = SceneColor;
    write Out     = SceneColor;
    param PulseTint  = PulseColor;
    param PulseSpeed = Speed;
    param PulseWidth = Width;
    param PulseRange = Range;
}
