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

**Read first**
- Beej's Guide to C, File I/O chapter (modes, `"rb"`/`"wb"`, fread/fwrite):
  https://beej.us/guide/bgc/html/split/file-inputoutput.html
- `fopen(3)` man page (what every mode string actually does):
  https://man7.org/linux/man-pages/man3/fopen.3.html
- `fread(3)`/`fwrite(3)` man page (both are on this one page):
  https://man7.org/linux/man-pages/man3/fread.3.html

**Build.** `write_file()` and `read_file()` in `src/phase0/01_file_bytes.c`:
open a file in binary mode, write an exact buffer of bytes, close; then open,
read it back, return how many bytes you got. Handle the failure cases the test
checks (nonexistent file, short buffer).

**Why.** Every database is built on exactly these four calls: open, read,
write, close. The subtleties you meet here — binary vs text mode, `"w"`
truncating the file instantly, `fread` returning a *count* you must check
rather than assuming — are the same subtleties that corrupt real databases
when ignored. SQLite's entire storage layer bottoms out in calls like these.

---

## Task 0.2 — Hex dump: see your bytes (~1–1.5 h)

**Read first**
- `hexdump(1)` man page (the tool you'll compare against):
  https://man7.org/linux/man-pages/man1/hexdump.1.html
- Beej's File I/O chapter again, the binary-file sections:
  https://beej.us/guide/bgc/html/split/file-inputoutput.html
- Skim the top of SQLite's file format doc to see what a real database's
  bytes look like: https://www.sqlite.org/fileformat2.html

**Build.** `hex_line()` in `src/phase0/02_hexdump.c`: format up to 16 bytes as
one hex-dump line — offset, hex pairs, ASCII column — to the exact spec in the
header comment. Then (not tested, but do it) write a tiny `main` that dumps
any file, and run `hexdump -C` on some file you wrote in Task 0.1 to compare.

**Why.** For the rest of this project, your debugger for "why is my database
broken" is looking at the raw bytes of the file. Building the hex dump
yourself (instead of only running `xxd`) forces you to internalize offsets,
hex, and printable-vs-raw bytes — the exact literacy you need to read a page
header or a B-tree cell later. From Phase 2 on, `hexdump -C mydb.db` is the
first thing you run when a test fails.

---

## Task 0.3 — Serialize a struct to an exact byte layout (~1.5–2 h)

**Read first**
- Data structure alignment / struct padding (why `fwrite(&s, sizeof s, ...)`
  is a trap): https://en.wikipedia.org/wiki/Data_structure_alignment
- Endianness (why "the 4 bytes of an int" is ambiguous):
  https://en.wikipedia.org/wiki/Endianness
- Beej's Guide to Network Programming §7.5, "Serialization — How to Pack
  Data" (the same problem, solved for networks):
  https://beej.us/guide/bgnet/html/split/slightly-advanced-techniques.html

**Build.** `rec_serialize()` / `rec_deserialize()` in
`src/phase0/03_serialize.c`: convert a `Rec { uint32_t id; char name[32]; }`
to exactly 36 bytes — id as 4 little-endian bytes written with shifts and
masks (not `memcpy` of the int), then the name buffer — and back. Unused name
bytes must be zero.

**Why.** The compiler is allowed to insert invisible padding inside your
structs, and different CPUs disagree on byte order, so a struct dumped raw
with `fwrite` is not a file format — it's whatever your compiler felt like
today. A database file must mean the same thing on every machine forever, so
real engines define the byte layout explicitly and convert field by field.
This is exactly what SQLite's file-format spec is: hundreds of "byte 3–4 means
X" rules. Your 36-byte `Rec` is the first format you own.

---

## Task 0.4 — Random access with fseek: fixed-size records (~1–1.5 h)

**Read first**
- `fseek(3)` man page (SEEK_SET/CUR/END, ftell):
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

**Why.** `offset = index * record_size` is the single most important equation
in this project. It's how Phase 2 finds row N, how Phase 3 finds page N
(`page_num * PAGE_SIZE`), and how a B-tree finds a child node. Databases are
fast because they never scan a file to find something whose address they can
compute — this task is that idea in its smallest possible form.
