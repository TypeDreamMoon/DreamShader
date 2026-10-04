// Every key of `#pragma pipeline`, canonically spelled: the flags in their table's order whatever order they are
// written in, Enabled naming a uniform bool, and a pass switched by another.
#pragma pipeline(Order = -3, Injection = PostProcess.AfterDOF, Views = SceneCapture | Game, Requires = CustomStencil | PostProcess, Enabled = On)

uniform bool On = true;
/// @group Switches   @sort 4
uniform bool Strong = false;

buffer Tone : RGBA16F(Export = true);

pass Grade : copy
{
    Enabled = Strong;
    read SceneColor;
    write Tone;
}
