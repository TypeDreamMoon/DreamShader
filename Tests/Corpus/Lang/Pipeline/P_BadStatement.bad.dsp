// A pass statement is a setting or a 'read', 'write' or 'param' line; a number is neither.
buffer Mask : R8;

pass Fill : clear
{
    42;
    write Mask;
}
