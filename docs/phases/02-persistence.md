# Phase 2 — Persist records to a single file, fixed layout

**Why this phase exists.** A database that forgets everything on exit is a
spreadsheet with extra steps. This phase gives the Phase 1 table a home on
disk using exactly the skills from Phase 0: a fixed byte layout per row,
written at computed offsets in one file. You are designing your first real
file format — small, but yours, and version 1 of the file `minidb` will keep
using through Phase 7.

No stubs are provided from here on: you write the headers and tests yourself,
following the Phase 0/1 pattern (assert-based `main()` per task, wired into
the Makefile). Designing the interface is now part of the task.

---

## Task 2.1 — Design the file format on paper (~1 h)

**Read first**
- SQLite's file format spec — skim the header section to see what format
  decisions look like written down: https://www.sqlite.org/fileformat2.html
- cstack part 5, "Persistence to Disk" (the design he lands on):
  https://cstack.github.io/db_tutorial/parts/part5.html

**Build.** A short `docs/FORMAT.md` (half a page) defining your format:
serialized row layout (you have this: 4-byte LE id + fixed name/email
buffers), row N's offset, and how the row count is known on reopen (derive it
from file size — file size / row size — like cstack does; no header needed
yet). Write down what is *not* handled (partial writes, versioning) so future
you knows it was a choice.

**Why.** Databases outlive the programs that write them; the format *is* the
database. Writing the format down before coding it is how real engines work —
SQLite's spec promises the layout will be readable for decades. Deriving the
count from file size also teaches a classic trick: don't store what you can
compute, because stored copies of derivable facts can disagree.

---

## Task 2.2 — Save on exit, load on open (~1.5–2 h)

**Read first**
- cstack part 5 (db_open, and flushing on `.exit`):
  https://cstack.github.io/db_tutorial/parts/part5.html
- `fopen(3)` — pick the right mode for "open or create, read and write":
  https://man7.org/linux/man-pages/man3/fopen.3.html
- `fread(3)`/`fwrite(3)` (check those return counts):
  https://man7.org/linux/man-pages/man3/fread.3.html

**Build.** `db_open(filename)` replaces `table_new()`: open (or create) the
file, compute `num_rows` from its size, load rows. `db_close()` serializes
every row at its offset and closes. `main` now takes the filename as
`argv[1]`. Your test: insert rows, close, reopen, select — same rows.

**Why.** This is the moment your program grows a second lifetime — state that
exists when the process doesn't. The open/close pair you build here is the
seam where the pager (Phase 3) will slot in without the REPL noticing. The
reopen test is the most important test in the repo from now on: every later
phase must keep it green, which is exactly the compatibility burden real
database developers carry.

---

## Task 2.3 — Break it on purpose: durability limits (~1 h)

**Read first**
- SQLite, "Atomic Commit in SQLite" §1–2 (what can go wrong between memory
  and platter): https://www.sqlite.org/atomiccommit.html
- `fsync(2)` — what the OS actually promises about your writes:
  https://man7.org/linux/man-pages/man2/fsync.2.html

**Build.** No new features — experiments and notes. Kill the process (Ctrl-C,
`kill -9`) mid-session before `.exit`: what survives? Truncate the file to a
non-multiple of the row size with a hex editor or `dd`: what does your loader
do? Make `db_open` at least *detect* a torn file (size not a multiple of row
size) and refuse it with an error instead of loading garbage. Record findings
in `docs/FORMAT.md`.

**Why.** Knowing precisely how your database fails is the setup for the whole
back half of the roadmap. Writes sit in user-space buffers, then kernel
buffers, then the disk's cache — a crash can freeze any layer mid-thought.
Real engines exist in the gap between `fwrite` returning and bytes being
truly durable; the WAL you may build in Phase 7 is the industry's answer, and
it will make no sense unless you've watched the naive version lose data first.
