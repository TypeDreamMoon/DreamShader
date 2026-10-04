// Defaults the binder fills in: a pass that names no injection point takes the pragma's, and a buffer that names no
// resolution takes its first writer's in frame order -- not in declaration order -- which is Output from the
// tonemapper replacement on.
#pragma pipeline(Injection = PostProcess.AfterTonemap)

buffer Late  : RGBA8;
buffer Early : RGBA8;
buffer Both  : RGBA8(Export = true);

pass Second : copy
{
    read  SceneColor;
    write Late;
}

pass Third : copy
{
    read  Late;
    write Both;
}

pass First : copy
{
    Injection = AfterOpaque;
    read  SceneColor;
    write Early;
}

pass Fourth : copy
{
    Injection = BeforePostProcess;
    read  Early;
    write Both;
}
