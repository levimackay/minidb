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

**The idea.** A file format is a promise: "any program that follows these
rules can read this file, forever." The program that wrote the file might be
gone — recompiled, rewritten, running on a different machine, or a decade
dead — and the file still has to mean what it meant. That's why formats get
written down as prose *before* they exist as code: the document is the
authority, and the code merely implements it. When (not if) your code and
your document disagree later, the document tells you which one is the bug.

Your first format can be almost trivially simple, and that's a feature. One
file. Row N's 100 serialized bytes live at offset `N * ROW_SIZE` — Phase 0's
equation. No header, no magic number, no version field yet. Even the row
*count* isn't stored: on reopen you derive it from the file's size
(`file_size / ROW_SIZE`). That derivation teaches a classic design rule —
don't store what you can compute — whose reason is subtle: a stored copy of a
derivable fact is a second source of truth, and two sources of truth can
disagree. A stored count of 10 in a file holding 8 rows is a corruption bug
you'd have to write detection code for; a derived count can't disagree with
the file, because it *is* the file.

Equally important is writing down what your format deliberately does *not*
handle. No versioning: a future format change breaks old files, and you're
accepting that for now. No torn-write protection: a crash mid-write can
leave half a row. Recording the non-decisions is what separates a designed
format from an accidental one — future-you needs to know that a limitation
was a choice, not an oversight.

**Read first**
- SQLite's file format spec — skim the header section to see what format
  decisions look like written down at production scale:
  https://www.sqlite.org/fileformat2.html
- cstack part 5, "Persistence to Disk" (the design he lands on — same
  derive-count-from-size trick):
  https://cstack.github.io/db_tutorial/parts/part5.html
- "Designing File Formats" (Andy McFadden) — practical checklist of what real
  formats put in a header (magic number, version, checksums) and why; you're
  skipping most of it *knowingly*: https://fadden.com/tech/file-formats.html

**Build.** A short `docs/FORMAT.md` (half a page) defining your format:
serialized row layout (you have this: 4-byte LE id + fixed name/email
buffers), row N's offset, and how the row count is known on reopen (derive it
from file size — file size / row size — like cstack does; no header needed
yet). Write down what is *not* handled (partial writes, versioning) so future
you knows it was a choice.

**Why real databases care.** Databases outlive the programs that write them;
the format *is* the database. SQLite's spec promises the layout will be
readable for decades, and it's the reason a `.db` file from 2004 opens today.
Your format document will grow with every phase — pages in 3, node layouts in
4, the WAL record in 7 — and by the end it will be the most complete
description of your database that exists, more authoritative than the code.

---

## Task 2.2 — Save on exit, load on open (~1.5–2 h)

**The idea.** Until now your program had one lifetime: state born at launch,
dead at exit. This task gives it a second, longer lifetime — state that
exists *between* runs, in the file. That changes what "correct" means. A
purely in-memory bug is gone when the process dies; a persistence bug is
*permanent* — a file written wrong today is corrupt every day after. This is
why database developers are paranoid in a way application developers usually
aren't, and this task is where you inherit the paranoia.

Mechanically, you're building a matched pair. `db_open`: open the file
(creating it if missing — check what `fopen` mode gives you
open-or-create-read-write without truncating; `"r+b"` fails on a missing
file, `"w+b"` destroys an existing one — this is a real decision, look at
the mode table), find its size, compute `num_rows`, load the rows through
your Phase 0 deserializer. `db_close`: serialize every row to its offset,
close cleanly. The pair must be inverses: open(close(state)) == state, for
any state. Your reopen test is exactly that assertion.

**Read first**
- cstack part 5 (db_open, and flushing on `.exit`):
  https://cstack.github.io/db_tutorial/parts/part5.html
- `fopen(3)` — pick the right mode for "open or create, read and write";
  the mode table repays careful reading:
  https://man7.org/linux/man-pages/man3/fopen.3.html
- `fread(3)`/`fwrite(3)` (check those return counts — a short read of a row
  is now a *persistent* data problem, not a transient one):
  https://man7.org/linux/man-pages/man3/fread.3.html

**Build.** `db_open(filename)` replaces `table_new()`: open (or create) the
file, compute `num_rows` from its size, load rows. `db_close()` serializes
every row at its offset and closes. `main` now takes the filename as
`argv[1]`. Your test: insert rows, close, reopen, select — same rows.

**Why real databases care.** The open/close pair you build here is the seam
where the pager (Phase 3) will slot in without the REPL noticing — `db_open`
is the function every later phase renovates. The reopen test is the most
important test in the repo from now on: every later phase must keep it green,
which is exactly the compatibility burden real database developers carry.
When Phase 4 changes the file layout to B-tree nodes, you'll feel what a
format migration costs — and why engines avoid them.

---

## Task 2.3 — Break it on purpose: durability limits (~1 h)

**The idea.** When `fwrite` returns, your bytes are *not* on disk. They're in
a buffer inside your process (the C library's). `fflush` pushes them to the
kernel — still not on disk, now in the operating system's page cache, which
the OS writes back whenever it feels like it, seconds later. Even `fsync`,
the system call that means "really, put it on the platter and don't return
until it's there," historically got lied to by disk hardware that cached
writes in volatile memory. Between your `fwrite` and physical permanence
there are three or four layers of buffering, and a crash — process killed,
kernel panic, power cut — can freeze any layer mid-thought.

So "the write happened" is not a yes/no fact; it's a question of *which
layer* the bytes had reached when the music stopped. This task is a lab
session for feeling that. Kill your process before `.exit`: everything since
open is gone, because your design only writes at close — note that this is a
*choice* with a cost (lose everything on crash) and a benefit (fast, simple).
Then vandalize a file to a size that isn't a multiple of the row size and
watch what your loader does with the fraction — and then make it refuse:
detecting a torn file and erroring beats loading garbage every time, because
garbage loaded silently gets *re-saved* as if it were data, laundering
corruption into truth.

**Read first**
- SQLite, "Atomic Commit in SQLite" §1–2 (what can go wrong between memory
  and platter — the hardware-assumptions section is eye-opening):
  https://www.sqlite.org/atomiccommit.html
- `fsync(2)` — what the OS actually promises about your writes, and how
  little that is: https://man7.org/linux/man-pages/man2/fsync.2.html
- SQLite, "How To Corrupt An SQLite Database File" — a production engine's
  catalog of every way this goes wrong in the field:
  https://www.sqlite.org/howtocorrupt.html

**Build.** No new features — experiments and notes. Kill the process (Ctrl-C,
`kill -9`) mid-session before `.exit`: what survives? Truncate the file to a
non-multiple of the row size with a hex editor or `dd`: what does your loader
do? Make `db_open` at least *detect* a torn file (size not a multiple of row
size) and refuse it with an error instead of loading garbage. Record findings
in `docs/FORMAT.md`.

**Why real databases care.** Real engines exist in the gap between `fwrite`
returning and bytes being truly durable — that gap is where transactions,
journals, and recovery live. SQLite's atomic-commit paper is essentially a
250-line answer to "what if the power dies between these two writes?", asked
at every step. The WAL you may build in Phase 7 is the industry's answer to
what you just watched: it will make no sense unless you've seen the naive
version lose data first, which is why this task exists.
