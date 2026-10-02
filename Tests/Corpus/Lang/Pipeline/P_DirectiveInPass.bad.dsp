// A '#' line cannot stand inside a pass block.
buffer Mask : R8;

pass Fill : clear
{
    #pragma region Inside
    write Mask;
}
