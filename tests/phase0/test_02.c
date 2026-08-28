/* Defines "done" for Task 0.2. Run: make test-phase0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "phase0.h"

int main(void)
{
    char line[128];

    /* full 16-byte line, exact match */
    const uint8_t *full = (const uint8_t *)"hello, world!!!!";
    hex_line(full, 16, 0, line, sizeof line);
    assert(strcmp(line,
        "00000000: 68 65 6c 6c 6f 2c 20 77 6f 72 6c 64 21 21 21 21 "
        "hello, world!!!!") == 0);

    /* short line: hex area still 48 chars so the ASCII column lines up;
       non-printable bytes render as '.' */
    const uint8_t shrt[3] = { 0x01, 0x02, 0xff };
    hex_line(shrt, 3, 16, line, sizeof line);
    assert(strlen(line) == 10 + 48 + 3);
    assert(strncmp(line, "00000010: 01 02 ff ", 19) == 0);
    for (int i = 19; i < 58; i++) assert(line[i] == ' ');
    assert(strcmp(line + 58, "...") == 0);

    /* offsets are hex, not decimal */
    hex_line(shrt, 1, 255, line, sizeof line);
    assert(strncmp(line, "000000ff: 01 ", 13) == 0);

    /* too-small buffer: writes nothing (no overflow, no partial line) */
    char tiny[8] = "sent";
    hex_line(full, 16, 0, tiny, 4);
    assert(tiny[0] == '\0' || strcmp(tiny, "sent") == 0); /* untouched or emptied */

    printf("test_02 (hex dump line): PASS\n");
    return 0;
}
