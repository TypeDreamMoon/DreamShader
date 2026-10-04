// Two passes replace the tonemapper, and one view runs one tonemapper.
pass ToneA : fullscreen
{
    Injection = PostProcess.ReplaceTonemapper;
    Material = "PP_ToneA";
    write SceneColor;
}

pass ToneB : fullscreen
{
    Injection = PostProcess.ReplaceTonemapper;
    Material = "PP_ToneB";
    write SceneColor;
}
