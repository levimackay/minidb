/* Reference solution — Task 0.1. Open it as a last resort. */
#include "phase0.h"

bool write_file(const char *path, const uint8_t *buf, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
        return false;
    bool ok = (fwrite(buf, 1, n, f) == n);
    /* fclose can fail too: buffered bytes are flushed here */
    if (fclose(f) != 0)
        ok = false;
    return ok;
}

long read_file(const char *path, uint8_t *buf, size_t max)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return -1;
    size_t got = fread(buf, 1, max, f);
    fclose(f);
    return (long)got;
}
