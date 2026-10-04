// A pass block opens with '{' after the pass's kind.
buffer Mask : R8;

pass Fill : clear
    Value = 1.0;
    write Mask;
}
