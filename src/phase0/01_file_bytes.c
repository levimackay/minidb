/* Task 0.1 — write a file of bytes, read it back.
 * Read first: docs/phases/00-binary-file-io.md, task 0.1.
 * Done when: make test-phase0 passes test_01.
 */
#include "phase0.h"

bool write_file(const char *path, const uint8_t *buf, size_t n)
{
    /* TODO(you):
     * 1. fopen(path, "wb") — binary write mode; NULL means failure.
     * 2. fwrite the n bytes; check the returned count equals n.
     * 3. fclose (check it too — buffered bytes are flushed here).
     */
    (void)path; (void)buf; (void)n;
    return false;
}

long read_file(const char *path, uint8_t *buf, size_t max)
{
    /* TODO(you):
     * 1. fopen(path, "rb"); NULL -> return -1.
     * 2. fread up to max bytes; fread's return value IS your answer.
     * 3. fclose, return the count.
     */
    (void)path; (void)buf; (void)max;
    return -1;
}
