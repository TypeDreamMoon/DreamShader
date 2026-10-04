// Every pass statement ends with ';', the last one too.
buffer Mask : R8;

pass Fill : clear
{
    Value = 1.0;
    write Mask
}
