/* Task 0.4 — random access: fixed-size records at computed offsets.
 * Read first: docs/phases/00-binary-file-io.md, task 0.4.
 * offset = index * REC_SIZE is the whole trick — it is also how Phase 3
 * will find page N and how the B-tree will find a node.
 * Done when: make test-phase0 passes test_04.
 */
#include "phase0.h"

bool rec_write_at(FILE *f, long index, const Rec *r)
{
    /* TODO(you):
     * 1. fseek(f, index * REC_SIZE, SEEK_SET) — check the return value.
     * 2. rec_serialize into a local uint8_t[REC_SIZE] buffer.
     * 3. fwrite it; success only if the full REC_SIZE was written.
     */
    (void)f; (void)index; (void)r;
    return false;
}

bool rec_read_at(FILE *f, long index, Rec *r)
{
    /* TODO(you): fseek, fread REC_SIZE bytes (full count or fail),
     * rec_deserialize into *r. */
    (void)f; (void)index; (void)r;
    return false;
}
