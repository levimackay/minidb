/* Defines "done" for Task 0.1. Run: make test-phase0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "phase0.h"

int main(void)
{
    const char *path = "build/t01_scratch.bin";
    uint8_t data[8] = { 0x00, 0x01, 0x7f, 0x80, 0xff, 'a', 'b', 0x00 };
    uint8_t back[16];

    /* roundtrip: every byte value comes back exactly, including 0x00 */
    assert(write_file(path, data, sizeof data));
    long got = read_file(path, back, sizeof back);
    assert(got == (long)sizeof data);
    assert(memcmp(data, back, sizeof data) == 0);

    /* reading with a small buffer returns only what fits */
    got = read_file(path, back, 3);
    assert(got == 3);
    assert(back[0] == 0x00 && back[1] == 0x01 && back[2] == 0x7f);

    /* a missing file is a clean -1, not a crash */
    assert(read_file("build/definitely_missing.bin", back, sizeof back) == -1);

    /* "wb" truncates: rewriting with fewer bytes shrinks the file */
    assert(write_file(path, data, 2));
    assert(read_file(path, back, sizeof back) == 2);

    printf("test_01 (write/read bytes): PASS\n");
    return 0;
}
