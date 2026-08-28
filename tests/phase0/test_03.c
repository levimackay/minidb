/* Defines "done" for Task 0.3. Run: make test-phase0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "phase0.h"

int main(void)
{
    /* exact bytes: 258 = 0x0102 -> little-endian 02 01 00 00 */
    Rec r = { .id = 258, .name = "ada" };
    uint8_t out[REC_SIZE];
    memset(out, 0xAA, sizeof out); /* poison, so zeroing is really yours */
    rec_serialize(&r, out);

    assert(out[0] == 0x02 && out[1] == 0x01 && out[2] == 0x00 && out[3] == 0x00);
    assert(out[4] == 'a' && out[5] == 'd' && out[6] == 'a' && out[7] == '\0');
    for (int i = 8; i < REC_SIZE; i++)
        assert(out[i] == 0); /* deterministic files: no junk after the name */

    /* roundtrip */
    Rec back;
    memset(&back, 0, sizeof back);
    rec_deserialize(out, &back);
    assert(back.id == 258);
    assert(strcmp(back.name, "ada") == 0);

    /* a big id exercises all four bytes */
    Rec big = { .id = 0xDEADBEEF, .name = "x" };
    rec_serialize(&big, out);
    assert(out[0] == 0xEF && out[1] == 0xBE && out[2] == 0xAD && out[3] == 0xDE);
    rec_deserialize(out, &back);
    assert(back.id == 0xDEADBEEFu);

    printf("test_03 (struct serialization): PASS\n");
    return 0;
}
