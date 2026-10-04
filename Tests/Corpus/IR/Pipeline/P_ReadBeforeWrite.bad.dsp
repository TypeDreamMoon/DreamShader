// A buffer read before anything writes it in the frame, with no Clear value to read instead.
buffer Late : RGBA8(Clear = None);

pass Use : copy
{
    Injection = AfterOpaque;
    read Late;
    write SceneColor;
}

pass Fill : copy
{
    Injection = BeforePostProcess;
    read SceneColor;
    write Late;
}
