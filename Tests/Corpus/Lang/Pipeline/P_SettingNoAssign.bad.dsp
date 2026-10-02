// A setting is 'Key = Value;'.
buffer Mask : R8;

pass Fill : clear
{
    Value 1.0;
    write Mask;
}
