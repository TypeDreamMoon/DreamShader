// A pass whose '}' is missing ends where the next declaration starts, which is said once.
buffer Mask : R8;

pass First : clear
{
    write Mask;

pass Second : clear
{
    write Mask;
}
