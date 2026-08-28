# Phase 3 — Paging: fixed-size pages and a page cache

**Why this phase exists.** Real databases never think in rows-in-a-file; they
think in *pages* — fixed-size blocks (SQLite has defaulted to 4096 bytes
since 2016) that are the unit of every read, write, and cache decision. This
phase restructures your file into pages behind a *pager*, so that Phase 4's
B-tree can treat "page number" as its pointer type. It's the biggest
pure-refactor phase: the REPL behaves identically at the end, and that's the
point.

---

## Task 3.1 — Why pages? (reading + design, ~1–1.5 h)

**The idea.** Why would a database chop its file into fixed 4096-byte blocks
instead of just writing rows end to end? Because the hardware and the
operating system already think that way, and fighting them is expensive.
Disks (and SSDs) transfer data in sectors/blocks, not bytes: asking the disk
for 1 byte costs the same as asking for the whole block containing it. The
OS manages memory and file caches in pages (4 KB on most systems). So "read
one 100-byte row" was never really possible — the machine reads a block, and
the only question is whether your software is organized to exploit that or
to ignore it.

Databases exploit it by making the block the atom of *everything*. The unit
of reading and writing: one page. The unit of caching: you cache whole pages,
and "is page N in memory?" is the only question the cache answers. The unit
of allocation: a file grows page by page. And — the payoff coming in Phase 4
— the unit of data structure: one B-tree node is exactly one page, so
"pointer to a node" is just a page number, and following a pointer is one
page read. When every part of the system agrees on the atom, the parts
compose; that agreement is what a page *is*.

There's also a discipline cost you should design in from the start: the page
size gets frozen into the file format. Every structure you build for the
rest of the project — node headers, cells, the WAL's page images — must fit
inside 4096 bytes, and "how many of these fit in a page" becomes the
arithmetic behind every capacity constant. Note the wasted remainder in your
format (4096 / 100-byte rows = 40 rows, 96 bytes dead per page): real
formats waste space at page tails too; it's the accepted price of
fixed-size blocks.

**Read first**
- CMU 15-445 Lecture #03, "Database Storage: Files, Pages, Tuples" — the
  canonical lecture on why pages, page directories, and slotted layouts
  (video): https://www.youtube.com/watch?v=PRLXdIMJhOg
  and the matching lecture notes (PDF):
  https://15445.courses.cs.cmu.edu/fall2025/notes/03-storage1.pdf
- SQLite file format §"Pages" — a real page-based format, where everything
  in the file is one of a handful of page types:
  https://www.sqlite.org/fileformat2.html
- Why 4096 became SQLite's default page size (short, concrete: it matches
  modern OS/hardware block sizes):
  https://www.sqlite.org/pgszchng2016.html

**Build.** Extend `docs/FORMAT.md`: `PAGE_SIZE 4096`, page N starts at
`N * PAGE_SIZE`, rows are packed into pages (`ROWS_PER_PAGE = PAGE_SIZE /
ROW_SIZE`, remainder wasted for now), and a row's location becomes
(page_num, offset within page). Sketch the Pager struct: file handle, file
size, and an array of page buffers (NULL = not loaded).

**Why real databases care.** Every serious engine — SQLite, Postgres, MySQL —
is page-based for exactly these reasons, and their tuning guides are full of
page-size talk because it's the knob connecting the format to the hardware.
Deciding this on paper first matters because page size is *frozen into the
file format*: SQLite stores it at byte 16 of the header precisely because a
reader must know it before it can interpret anything else in the file.

---

## Task 3.2 — The pager: get_page with a cache (~1.5–2 h)

**The idea.** Disk is slow and memory is fast — not by a little, by orders of
magnitude (a memory access is nanoseconds; a disk read is microseconds to
milliseconds). Every storage system therefore keeps recently used data in
memory and goes to disk only on a miss. The component that does this for
pages is the *buffer pool* (your simplified version: the pager's cache), and
its interface is one function: `pager_get_page(pager, n)` returns a pointer
to the bytes of page N. Cached? Return the pointer. Not cached? Allocate a
4096-byte buffer, seek to `n * PAGE_SIZE`, read it in, remember it, return
it. The caller can't tell which happened — and that's the entire design.

Two details are load-bearing. A page that lies past the current end of file
comes back zero-filled rather than erroring — that's how "allocate a new
page" works, for free: ask for a page that doesn't exist yet, get clean
bytes, write into them (they reach disk when flushed). And *nothing else in
the program may touch the file* — every read and write goes through
`pager_get_page`. This funnel is what makes everything later possible: a
future eviction policy, dirty tracking, or WAL interception needs one
choke-point to live at, and this is it.

Yours deliberately stops short of a real buffer pool: nothing is ever
evicted (the cache only grows), no dirty flags (Phase 3.4 just flushes
everything), no pinning (nobody else can free a page you hold a pointer
into). Real pools need all three because databases are bigger than RAM and
concurrent. Mark the ceiling in a comment and move on — but watch CMU
lecture #04 so you know what the grown-up version looks like.

**Read first**
- cstack part 5 — his Pager is exactly this shape:
  https://cstack.github.io/db_tutorial/parts/part5.html
- CMU 15-445 Lecture #04, "Memory Management & Buffer Pools" — what yours
  omits and why real ones need it (video):
  https://www.youtube.com/watch?v=8-2yv4z0VZc
- The matching buffer-pool lecture notes (PDF) — replacement policies, dirty
  pages, pinning:
  https://15445.courses.cs.cmu.edu/fall2025/notes/04-bufferpool.pdf

**Build.** `pager_open()`, `pager_get_page(pager, page_num)`: if the page is
cached, return it; else `malloc` a page buffer, `fseek` + `fread` it from the
file (a page past EOF starts zeroed), cache it, return it. All row access now
goes through `pager_get_page` — nothing else touches the file.

**Why real databases care.** The contract "ask for page N, get a pointer to
its bytes, the pager worries about disk" is the single abstraction that lets
the B-tree, the WAL, and recovery code all ignore file I/O. In SQLite's
architecture the B-tree module literally sits on top of the pager module and
never performs I/O itself. Buffer-pool management is also where databases
famously refuse the OS's help — Postgres and MySQL manage their own page
caches rather than trusting the kernel's, because the database knows its
access patterns and the kernel doesn't.

---

## Task 3.3 — Route rows through pages (~1.5 h)

**The idea.** Finding a row is now two divisions instead of one
multiplication: `page_num = row_num / ROWS_PER_PAGE` picks the page,
`(row_num % ROWS_PER_PAGE) * ROW_SIZE` finds the byte offset inside it. Same
address-is-computable idea as Phase 0, one level of indirection deeper. The
function that owns this arithmetic, `row_slot`, returns a *pointer into the
page buffer* — and that return type is the deeper idea of the task.

Until now a row's true form was the `Row` struct, and the file was a copy.
That inverts here: rows now *live* as serialized bytes inside page buffers,
and `Row` structs exist only momentarily at the edges — deserialize just
before printing, serialize just after parsing an insert. There is no array
of structs anymore. This is called operating on data *in place*, and it's
how real engines work: a row is bytes in a page in the buffer pool, and
executing a query means walking pointers into those buffers, not copying
everything into friendly objects first. The copy you don't make is a large
part of why databases are fast.

The other half of the task is invisible in the diff but visible in the test
run: the Phase 1/2 tests must pass *unchanged*. You're gutting the storage
representation under a frozen interface, and the tests are the harness that
proves the surgery clean. This exact experience — big internal change, zero
external change, green suite as evidence — is the professional skill this
phase drills.

**Read first**
- cstack part 5 (row_slot -> page/offset arithmetic):
  https://cstack.github.io/db_tutorial/parts/part5.html
- cstack part 6 — read ahead; the cursor will soon own this arithmetic:
  https://cstack.github.io/db_tutorial/parts/part6.html
- CMU Lecture #03 notes, the tuple-layout sections — how real pages organize
  the tuples inside them (slotted pages; yours is the simpler fixed-size
  version): https://15445.courses.cs.cmu.edu/fall2025/notes/03-storage1.pdf

**Build.** Replace the Phase 2 row array: `row_slot(table, row_num)` computes
`page_num = row_num / ROWS_PER_PAGE`, asks the pager for that page, returns a
pointer to `offset = (row_num % ROWS_PER_PAGE) * ROW_SIZE` inside it. Insert
serializes into that slot; select deserializes out of it. All Phase 1/2 tests
must still pass unchanged.

**Why real databases care.** Serialize-in-place is how engines avoid copying
every row twice on every access, and pointer-into-page is exactly what a
real engine's "tuple" is under the hood. The indirection also quietly
prepares Phase 4: once nothing outside `row_slot` knows where rows physically
live, moving them into B-tree leaf cells changes one function's internals,
not the program.

---

## Task 3.4 — Flush pages, reopen correctly (~1–1.5 h)

**The idea.** The pager's cache introduced a problem that didn't exist
before: two copies of the truth. Page N exists on disk *and* in a memory
buffer, and the moment you write into the buffer they disagree. The memory
copy is now called *dirty*, and someone must eventually reconcile the two by
writing the buffer back to its slot in the file — *flushing*. Your design
does it all at close: walk every loaded page, `fseek` to
`page_num * PAGE_SIZE`, write the buffer. Miss one page, or write one at the
wrong offset, and the file is silently wrong — this task is where
write-back bugs are born, and the insert-3-pages-close-reopen test is the
net that catches them.

You'll hit one genuinely annoying wrinkle: the last page may be only partly
full of rows, and because your format still derives row count from *file
size*, flushing the full 4096 bytes of a half-used page would inflate the
count with phantom rows. So for now you flush only the bytes that hold rows.
Sit with how ugly that is — the file format's one clever trick (no stored
count) is now actively fighting the paging design. That tension is real
format-design feedback, and Phase 4 resolves it properly: pages become
self-describing (each node stores its own cell count in a header), the file
becomes always a whole number of pages, and the partial-page code gets
deleted. cstack drops it at the same moment for the same reason.

**Read first**
- cstack part 5 (pager_flush, the partial-page wrinkle):
  https://cstack.github.io/db_tutorial/parts/part5.html
- SQLite Atomic Commit §3 (what a single-page write really involves once you
  care about crashes — foreshadowing Phase 7):
  https://www.sqlite.org/atomiccommit.html
- CMU Lecture #04 notes — dirty pages and write-back policy in a real buffer
  pool: https://15445.courses.cs.cmu.edu/fall2025/notes/04-bufferpool.pdf

**Build.** `db_close()` walks the cache and writes every loaded page back at
`page_num * PAGE_SIZE` — including the trailing partial page (only write the
bytes that hold rows, for now). Reopen derives row count from file size as
before. Test: insert enough rows to span 3+ pages, close, reopen, select.

**Why real databases care.** Write-back caching creates the dirty-data
problem, and *you* own the moment of reconciliation — real engines track
dirtiness per page and decide continuously (checkpoints, eviction, commit)
rather than once at exit, but the problem is identical. When a page can be
written back matters even more than whether: Phase 7's WAL rule is literally
"no dirty page reaches the database file before its log records are synced."
You're meeting the mechanism now so the rule has something to attach to
later.
