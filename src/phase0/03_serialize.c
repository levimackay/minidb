/* Task 0.3 — serialize a struct to an exact 36-byte layout.
 * Read first: docs/phases/00-binary-file-io.md, task 0.3.
 * Layout is specified in phase0.h above REC_SIZE.
 * Done when: make test-phase0 passes test_03.
 */
#include "phase0.h"

void rec_serialize(const Rec *r, uint8_t out[REC_SIZE])
{
    /* TODO(you):
     * 1. id as 4 little-endian bytes using shifts and masks:
     *      out[0] = id & 0xff;  out[1] = (id >> 8) & 0xff;  ...
     * 2. Zero bytes 4..35 first (so junk after the name's NUL never
     *    reaches the file), then copy the name string into out+4.
     */
    (void)r; (void)out;
}

void rec_deserialize(const uint8_t in[REC_SIZE], Rec *r)
{
    /* TODO(you): rebuild id from the 4 bytes (reverse the shifts),
     * copy the 32 name bytes back. */
    (void)in; (void)r;
}
