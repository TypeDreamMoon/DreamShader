// File comment above the pragma.
#pragma pipeline(Order = 1) // after the pragma

// Above the buffer.
buffer Mask : R8; // after the buffer

/* A block comment above the pass. */
pass Fill : clear
{
    // Above the setting.
    Value = 1.0; // after the setting
    write Mask; /* after the write */
    // After the last statement.
} // after the pass
// At the end of the file.
