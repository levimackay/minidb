/* Reference solution — Task 0.3. Open it as a last resort. */
#include <string.h>
#include "phase0.h"

void rec_serialize(const Rec *r, uint8_t out[REC_SIZE])
{
    /* id, little-endian, byte by byte — never memcpy the int */
    out[0] = (uint8_t)(r->id & 0xff);
    out[1] = (uint8_t)((r->id >> 8) & 0xff);
    out[2] = (uint8_t)((r->id >> 16) & 0xff);
    out[3] = (uint8_t)((r->id >> 24) & 0xff);

    /* zero the whole name area first so no stack junk reaches the file */
    memset(out + 4, 0, 32);
    strncpy((char *)(out + 4), r->name, 31);
}

void rec_deserialize(const uint8_t in[REC_SIZE], Rec *r)
{
    r->id = (uint32_t)in[0]
          | ((uint32_t)in[1] << 8)
          | ((uint32_t)in[2] << 16)
          | ((uint32_t)in[3] << 24);
    memcpy(r->name, in + 4, 32);
    r->name[31] = '\0'; /* belt and suspenders: always terminated */
}
