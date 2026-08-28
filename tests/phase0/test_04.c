/* Defines "done" for Task 0.4. Run: make test-phase0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "phase0.h"

int main(void)
{
    FILE *f = tmpfile();
    assert(f != NULL);

    /* write out of order: slot 3 first — the file must grow to fit it */
    Rec r3 = { .id = 3, .name = "carol" };
    Rec r0 = { .id = 0, .name = "ada" };
    Rec r1 = { .id = 1, .name = "bob" };
    assert(rec_write_at(f, 3, &r3));
    assert(rec_write_at(f, 0, &r0));
    assert(rec_write_at(f, 1, &r1));

    /* file size: 4 slots worth, because slot 3 exists */
    assert(fseek(f, 0, SEEK_END) == 0);
    assert(ftell(f) == 4L * REC_SIZE);

    /* read back in a different order */
    Rec got;
    assert(rec_read_at(f, 1, &got));
    assert(got.id == 1 && strcmp(got.name, "bob") == 0);
    assert(rec_read_at(f, 3, &got));
    assert(got.id == 3 && strcmp(got.name, "carol") == 0);
    assert(rec_read_at(f, 0, &got));
    assert(got.id == 0 && strcmp(got.name, "ada") == 0);

    /* overwrite in place: same slot, new bytes, same file size */
    Rec r1b = { .id = 100, .name = "bob2" };
    assert(rec_write_at(f, 1, &r1b));
    assert(rec_read_at(f, 1, &got));
    assert(got.id == 100 && strcmp(got.name, "bob2") == 0);
    assert(fseek(f, 0, SEEK_END) == 0);
    assert(ftell(f) == 4L * REC_SIZE);

    /* reading past the end fails cleanly */
    assert(!rec_read_at(f, 42, &got));

    fclose(f);
    printf("test_04 (fseek random access): PASS\n");
    return 0;
}
