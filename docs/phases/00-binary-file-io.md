# Phase 0 — Binary file I/O and serialization

**Prerequisite:** basic C (pointers, structs, `malloc`/`free`, compiling,
Makefiles, lldb). That's tinylang's Phase 0 (`~/Developer/tinylang`); if you
haven't done it, you can still do this phase as long as you're comfortable
reading and writing simple C with pointers and structs.

**Why this phase exists.** A database is a program whose entire job is turning
in-memory structs into bytes in a file and back, without losing or corrupting
anything. Nothing later (persistence, pages, B-tree nodes, the WAL) makes
sense until writing bytes to exact offsets in a file feels boring. That's the
goal here: make it boring.

Everything lives in `src/phase0/`. Signatures are in `src/phase0/phase0.h`.
Each task is done when its test passes:

```
make test-phase0
```

---

## Task 0.1 — Write a file of bytes, read it back (~1 h)

**The idea.** Everything your program knows lives in RAM, and RAM is wiped
the instant the process exits (or the power drops). A *file* is the operating
system's offer of memory that survives: a named sequence of bytes on disk
that outlives your program. And that's all a file is — not text, not lines,
not a document. A sequence of numbered bytes, each one a value from 0 to 255,
starting at byte 0. "Text file" just means someone chose to put only
printable characters in it. A database file will contain mostly *non*-printable
bytes — raw integers, headers, packed records — so you have to stop thinking
in `printf`/strings and start thinking in byte buffers.

C's standard library talks to files through four calls: `fopen` (get a handle
to a file, creating or truncating it depending on the mode string), `fread`
and `fwrite` (move N bytes between a memory buffer and the file), and
`fclose` (flush and release the handle). Two traps are waiting, and both
corrupt real databases when ignored. First, the mode string matters:
`"w"` *instantly destroys* the existing file contents even if you never write
a byte, and on some platforms omitting the `b` (binary) flag makes the
library silently translate bytes for you — deadly when the bytes are data,
not text. Second, `fread` and `fwrite` return a *count* of how much they
actually moved, and that count is allowed to be less than what you asked
for. Code that ignores the return value is code that silently loses data.

**Read first**
- Beej's Guide to C, File I/O chapter (modes, `"rb"`/`"wb"`, fread/fwrite) —
  written tutorial: https://beej.us/guide/bgc/html/split/file-inputoutput.html
- `fopen(3)` man page — the official reference for what every mode string
  actually does: https://man7.org/linux/man-pages/man3/fopen.3.html
- `fread(3)`/`fwrite(3)` man page (both are on this one page) — note what the
  return value means: https://man7.org/linux/man-pages/man3/fread.3.html

**Build.** `write_file()` and `read_file()` in `src/phase0/01_file_bytes.c`:
open a file in binary mode, write an exact buffer of bytes, close; then open,
read it back, return how many bytes you got. Handle the failure cases the test
checks (nonexistent file, short buffer).

**Why real databases care.** Every database is built on exactly these four
calls: open, read, write, close. SQLite's entire storage layer bottoms out in
calls like these (wrapped in a "VFS" layer, but the same operations). The
discipline you're drilling — check every return value, know exactly when the
file is truncated, know that `fclose` can itself fail because buffered bytes
are flushed there — is not beginner pedantry. A database that ignores a short
write has already corrupted someone's data; it just hasn't been caught yet.

---

## Task 0.2 — Hex dump: see your bytes (~1–1.5 h)

**The idea.** You cannot `printf` a database file — most of its bytes aren't
printable characters, and the ones that are will mislead you. The universal
way to *look* at raw bytes is a hex dump: each byte shown as two hexadecimal
digits (hex because one byte is exactly two hex digits — `0x00` to `0xff` —
so the visual width of every byte is identical), sixteen bytes per line, with
the byte *offset* at the left edge and an ASCII rendering at the right so
human-readable fragments jump out. Offsets are the crucial part: "the 4 bytes
at offset 36" is how you'll talk about file contents for the rest of this
project, the way an address is how you talk about memory.

Reading hex fluently is a learnable, physical skill, like reading a clock.
After this task you should be able to look at `1f 00 00 00` in a dump and
think "the little-endian 32-bit integer 31" without reaching for a
calculator for small values.

**Read first**
- `hexdump(1)` man page — the standard tool you'll compare your output
  against (`hexdump -C` is the canonical format):
  https://man7.org/linux/man-pages/man1/hexdump.1.html
- Beej's File I/O chapter again, the binary-file sections — you're reading
  arbitrary bytes now, not text:
  https://beej.us/guide/bgc/html/split/file-inputoutput.html
- Skim the top of SQLite's file format doc to see what a real database's
  bytes look like — notice how the whole spec is written in terms of byte
  offsets: https://www.sqlite.org/fileformat2.html

**Build.** `hex_line()` in `src/phase0/02_hexdump.c`: format up to 16 bytes as
one hex-dump line — offset, hex pairs, ASCII column — to the exact spec in the
header comment. Then (not tested, but do it) write a tiny `main` that dumps
any file, and run `hexdump -C` on some file you wrote in Task 0.1 to compare.

**Why real databases care.** For the rest of this project, your debugger for
"why is my database broken" is looking at the raw bytes of the file. Building
the hex dump yourself (instead of only running `xxd`) forces you to
internalize offsets, hex, and printable-vs-raw bytes — the exact literacy you
need to read a page header or a B-tree cell later. Real database engineers do
exactly this: SQLite's file-format documentation exists so a human with a hex
dump can decode any database file by hand. From Phase 2 on, `hexdump -C
mydb.db` is the first thing you run when a test fails.

---

## Task 0.3 — Serialize a struct to an exact byte layout (~1.5–2 h)

**The idea.** It's tempting to persist a struct with one line:
`fwrite(&s, sizeof s, 1, f)`. This is the single most important trap in this
phase, and it fails for two separate reasons. First, *padding*: the compiler
is allowed to insert invisible unused bytes between struct members so each
member sits at an address the CPU likes (a `uint32_t` typically wants an
address divisible by 4). Different compilers, platforms, and even compiler
flags produce different padding, so `sizeof(struct)` and the positions of
fields inside it are not stable facts about your data — they're facts about
today's compiler. Second, *endianness*: a 32-bit integer is four bytes, and
CPUs disagree about their order. Little-endian machines (x86, and ARM as
macOS runs it) store the least significant byte first, so `287` (`0x011f`)
is laid out `1f 01 00 00`; big-endian machines store `00 00 01 1f`. Dump the
raw int and the file means different numbers on different machines.

The fix is *serialization*: you define, in writing, exactly which byte of the
file means what, and you convert field by field. For integers that means
building each byte yourself with shifts and masks: `out[0] = id & 0xff` takes
the lowest 8 bits, `out[1] = (id >> 8) & 0xff` the next 8, and so on. This
code produces identical bytes on every machine, forever, because *you*
decided the order rather than inheriting the CPU's. Deserialization is the
mirror: reassemble the integer with shifts and ORs. Once a layout is written
down, the file has a meaning independent of any program — which is the whole
point of a file format.

**Read first**
- Data structure alignment / struct padding — why `fwrite(&s, sizeof s, ...)`
  is a trap: https://en.wikipedia.org/wiki/Data_structure_alignment
- Endianness — why "the 4 bytes of an int" is ambiguous:
  https://en.wikipedia.org/wiki/Endianness
- Beej's Guide to Network Programming §7.5, "Serialization — How to Pack
  Data" — the same problem, solved for bytes crossing a network instead of
  going to disk:
  https://beej.us/guide/bgnet/html/split/slightly-advanced-techniques.html

**Build.** `rec_serialize()` / `rec_deserialize()` in
`src/phase0/03_serialize.c`: convert a `Rec { uint32_t id; char name[32]; }`
to exactly 36 bytes — id as 4 little-endian bytes written with shifts and
masks (not `memcpy` of the int), then the name buffer — and back. Unused name
bytes must be zero.

**Why real databases care.** A database file must mean the same thing on
every machine forever, so real engines define the byte layout explicitly and
convert field by field. This is exactly what SQLite's file-format spec is:
hundreds of "byte 3–4 means X" rules. (SQLite happens to pick *big*-endian
for its integers; you're picking little-endian. Neither is more correct —
what matters is that the choice is written down and honored everywhere.)
Zeroing the unused name bytes matters too: whatever garbage is in that buffer
otherwise gets written to disk, making files non-reproducible and leaking
stack contents — a real class of security bug. Your 36-byte `Rec` is the
first format you own.

---

## Task 0.4 — Random access with fseek: fixed-size records (~1–1.5 h)

**The idea.** Reading a file front to back is like a cassette tape. But files
support *random access*: `fseek` moves the read/write position to any byte
offset instantly, without touching the bytes in between. That turns a file
from a tape into something like a giant array of bytes on disk — and arrays
have the property the whole project is built on: if every element is the same
size, you can *compute* where element N lives instead of searching for it.
Record N starts at byte `N * REC_SIZE`. No scanning, no index, no per-record
bookkeeping — arithmetic.

Two wrinkles make this more than one line of code. Seeking past the end of a
file and writing there is legal: the file silently grows, and the gap
contains zeros (or garbage, depending on platform) — your code must be fine
with that, because "write record 3 into an empty file" is exactly what a
database does when rows arrive out of order. And reading a slot that was
never fully written gives a *short read* — `fread` returns fewer bytes than
asked — which your code must detect and report as "no valid record here"
rather than deserializing half-garbage.

**Read first**
- `fseek(3)` man page — SEEK_SET/CUR/END, and `ftell` to ask where you are:
  https://man7.org/linux/man-pages/man3/fseek.3.html
- `fread(3)` again — what a short read means mid-file:
  https://man7.org/linux/man-pages/man3/fread.3.html
- cstack part 3 — see the same offset arithmetic used for real rows:
  https://cstack.github.io/db_tutorial/parts/part3.html

**Build.** `rec_write_at()` / `rec_read_at()` in
`src/phase0/04_random_access.c`: record N lives at byte offset `N * REC_SIZE`.
Seek there, write/read the 36 serialized bytes using Task 0.3's functions.
Writing record 3 into an empty file must work (the file grows; the gap is
zero/garbage — the test writes those slots afterward).

**Why real databases care.** `offset = index * record_size` is the single
most important equation in this project. It's how Phase 2 finds row N, how
Phase 3 finds page N (`page_num * PAGE_SIZE`), and how a B-tree finds a child
node ("child pointer" will literally be a page number to multiply). Databases
are fast because they never scan a file to find something whose address they
can compute — this task is that idea in its smallest possible form.
