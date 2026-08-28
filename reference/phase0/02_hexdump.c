/* Reference solution — Task 0.2. Open it as a last resort. */
#include <stdio.h>
#include "phase0.h"

void hex_line(const uint8_t *buf, size_t n, size_t offset,
              char *out, size_t outsz)
{
    if (n > 16)
        n = 16;
    /* 10 (offset + ": ") + 48 (16 fields x 3) + n (ascii) + NUL */
    if (outsz < 10 + 48 + n + 1) {
        if (outsz > 0)
            out[0] = '\0';
        return;
    }

    char *p = out;
    p += sprintf(p, "%08zx: ", offset);
    for (size_t i = 0; i < 16; i++) {
        if (i < n) {
            p += sprintf(p, "%02x ", buf[i]);
        } else {
            *p++ = ' '; *p++ = ' '; *p++ = ' ';
        }
    }
    for (size_t i = 0; i < n; i++)
        *p++ = (buf[i] >= 0x20 && buf[i] <= 0x7e) ? (char)buf[i] : '.';
    *p = '\0';
}
