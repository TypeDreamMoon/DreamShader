// An integer buffer: no pass can read or write one yet (an R32F holds an id exactly up to 16777216).
buffer Ids : R32U;

pass Reset : clear
{
    write Ids;
}
