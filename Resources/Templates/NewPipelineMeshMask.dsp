// {FILENAME} -- a DreamShader Custom Pass pipeline: a mesh mask chain. Compiles to {ASSETPATH}.
// The objects in the list {BASE} are drawn again into a mask, without a depth test, and a fullscreen pass lights the
// pixels around the mask -- so they show an outline through walls. M_{BASE}Mask.dss and PP_{BASE}Composite.dss, next to
// this file, are the two materials the passes draw.
//
// Put an object into the list from game code, UDreamPassSubsystem::AddToList("{BASE}", Mesh), or select by layer
// instead: Filter = Layer(Name), with Name one of the layers of Project Settings > DreamPlugin > DreamShader Custom
// Pass. A pipeline runs where something activates it: the Global Pipelines of those settings, or a Dream Pass Volume.
#pragma pipeline(Order = 100)

/// @group Look   @desc The outline's colour; alpha is its opacity
uniform float4 OutlineColor = float4(1.0, 0.6, 0.0, 1.0);

/// @group Look   @slider 1 8
uniform float OutlineWidth = 2.0;

/// @desc The pixels the selected objects cover
buffer Mask : R8(Clear = 0);

/// @desc The selected objects, drawn into the mask; no depth test, so walls do not hide them
pass DrawMask : mesh
{
    Injection = AfterOpaque;
    Filter    = List({BASE});
    Material  = "M_{BASE}Mask";
    Depth     = None;
    write Output0 = Mask;
}

/// @desc The outline: the pixels next to the mask that are not in it
pass Composite : fullscreen
{
    Injection = BeforePostProcess;
    Material  = "PP_{BASE}Composite";
    read  Mask;
    write SceneColor;
    param Color = OutlineColor;
    param Width = OutlineWidth;
}
