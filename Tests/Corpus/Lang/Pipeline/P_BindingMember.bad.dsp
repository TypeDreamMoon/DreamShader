// The one thing a buffer has after '.' is 'Previous'.
buffer Mask : R8(History = true);
buffer Soft : R8;

pass Keep : copy
{
    read Mask.Current;
    write Soft;
}
