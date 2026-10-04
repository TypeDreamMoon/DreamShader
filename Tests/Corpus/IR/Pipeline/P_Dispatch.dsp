// The forms of `Dispatch` and `Threads`: a fixed size, a buffer divided or scaled, a thread group written out (two
// numbers mean z = 1), and a virtual shader path, which the payload keeps as the virtual path too.
buffer Big   : RGBA16F;
buffer Small : RGBA16F(Scale = 0.5);

pass Fill : compute
{
    Shader = "/Project/Passes/Fill.usf";
    Entry = FillCS;
    Threads = uint3(4, 4, 4);
    Dispatch = uint3(64, 32, 2);
    write Result = Big;
}

pass Halved : compute
{
    Shader = "Down.usf";
    Entry = DownCS;
    Dispatch = Big / 2;
    read Source = Big;
    write Result = Small;
}

pass Scaled : compute
{
    Shader = "Down.usf";
    Entry = UpCS;
    Threads = uint2(16, 4);
    Dispatch = Small * 0.25;
    read Source = Small;
    write Result = Big;
}
