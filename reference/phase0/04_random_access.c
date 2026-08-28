/* Reference solution — Task 0.4. Open it as a last resort. */
#include "phase0.h"

bool rec_write_at(FILE *f, long index, const Rec *r)
{
    if (fseek(f, index * REC_SIZE, SEEK_SET) != 0)
        return false;
    uint8_t buf[REC_SIZE];
    rec_serialize(r, buf);
    return fwrite(buf, 1, REC_SIZE, f) == REC_SIZE;
}

bool rec_read_at(FILE *f, long index, Rec *r)
{
    if (fseek(f, index * REC_SIZE, SEEK_SET) != 0)
        return false;
    uint8_t buf[REC_SIZE];
    if (fread(buf, 1, REC_SIZE, f) != REC_SIZE)
        return false;
    rec_deserialize(buf, r);
    return true;
}
