/* Task 0.2 — one line of a hex dump, xxd/hexdump style.
 * Read first: docs/phases/00-binary-file-io.md, task 0.2.
 * The exact output format is specified in phase0.h above hex_line().
 * Done when: make test-phase0 passes test_02.
 *
 * After the test passes, write yourself a tiny main() in a scratch file
 * that hex-dumps any file 16 bytes per line, and compare it against
 * `hexdump -C` on the same file. That tool is your debugger for the
 * rest of this project.
 */
#include "phase0.h"

void hex_line(const uint8_t *buf, size_t n, size_t offset,
              char *out, size_t outsz)
{
    /* TODO(you):
     * 1. Guard: outsz too small (< 75) -> write nothing.
     * 2. sprintf/snprintf the 8-digit hex offset + ": ".
     * 3. Loop 16 fields: "xx " for i < n, "   " otherwise.
     * 4. Append the ASCII column: printable bytes (0x20..0x7e) as-is,
     *    everything else as '.'. NUL-terminate.
     */
    (void)buf; (void)n; (void)offset;
    if (outsz > 0) out[0] = '\0';
}
