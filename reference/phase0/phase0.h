#ifndef PHASE0_H
#define PHASE0_H
/* Phase 0 exercises — signatures only. You implement the bodies.
 * See docs/phases/00-binary-file-io.md for the reading + the why.
 * A task is done when its test passes: make test-phase0
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ---- Task 0.1 (01_file_bytes.c) ------------------------------------- */

/* Write exactly n bytes from buf to a new file at path (create or
 * truncate). Binary mode. Return true on success, false on any failure. */
bool write_file(const char *path, const uint8_t *buf, size_t n);

/* Read up to max bytes from the file at path into buf.
 * Return the number of bytes read, or -1 if the file can't be opened. */
long read_file(const char *path, uint8_t *buf, size_t max);

/* ---- Task 0.2 (02_hexdump.c) ---------------------------------------- */

/* Format up to 16 bytes as one hex-dump line into out (NUL-terminated).
 * Exact format, in order:
 *   - offset as 8 lowercase hex digits, then ": "        (10 chars)
 *   - 16 fields of 3 chars each: "xx " for each byte     (48 chars)
 *     (two lowercase hex digits + space), or "   " (3 spaces) for
 *     fields past n — so the ASCII column always lines up
 *   - the ASCII column: each byte as itself if printable (0x20..0x7e),
 *     otherwise '.'                                      (n chars)
 * Example (n=16, offset 0, bytes of "hello, world!!!!"):
 * 00000000: 68 65 6c 6c 6f 2c 20 77 6f 72 6c 64 21 21 21 21 hello, world!!!!
 * outsz is the capacity of out; write nothing if it is too small (< 75). */
void hex_line(const uint8_t *buf, size_t n, size_t offset,
              char *out, size_t outsz);

/* ---- Task 0.3 (03_serialize.c) -------------------------------------- */

typedef struct {
    uint32_t id;
    char name[32]; /* NUL-terminated, shorter than 32 */
} Rec;

/* On-disk layout, exactly 36 bytes:
 *   bytes 0..3   id, little-endian (out[0] = least significant byte)
 *   bytes 4..35  name bytes; every byte after the NUL must be 0
 * Do NOT fwrite/memcpy the struct itself — padding and endianness make
 * that layout compiler-dependent. Build the bytes field by field, with
 * shifts and masks for the integer. */
#define REC_SIZE 36

void rec_serialize(const Rec *r, uint8_t out[REC_SIZE]);
void rec_deserialize(const uint8_t in[REC_SIZE], Rec *r);

/* ---- Task 0.4 (04_random_access.c) ---------------------------------- */

/* Record slot N lives at byte offset N * REC_SIZE.
 * rec_write_at: seek there and write the 36 serialized bytes (use your
 * Task 0.3 functions). Writing slot 3 into an empty file must work.
 * rec_read_at: seek there, read 36 bytes, deserialize into *r.
 * Return true on success (a full-size read/write), false otherwise. */
bool rec_write_at(FILE *f, long index, const Rec *r);
bool rec_read_at(FILE *f, long index, Rec *r);

#endif
