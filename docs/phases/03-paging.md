# Phase 3 — Paging: fixed-size pages and a page cache

**Why this phase exists.** Real databases never think in rows-in-a-file; they
think in *pages* — fixed-size blocks (SQLite defaults to 4096 bytes) that are
the unit of every read, write, and cache decision. This phase restructures
your file into pages behind a *pager*, so that Phase 4's B-tree can treat
"page number" as its pointer type. It's the biggest pure-refactor phase: the
REPL behaves identically at the end, and that's the point.

---

## Task 3.1 — Why pages? (reading + design, ~1–1.5 h)

**Read first**
- CMU 15-445 Fall 2025 schedule — watch/skim Lecture #03 "Database Storage I"
  (slotted pages, why fixed-size blocks) and note Lecture #04 "Memory
  Management": https://15445.courses.cs.cmu.edu/fall2025/schedule.html
- SQLite file format §"Pages" — a real page-based format:
  https://www.sqlite.org/fileformat2.html

**Build.** Extend `docs/FORMAT.md`: `PAGE_SIZE 4096`, page N starts at
`N * PAGE_SIZE`, rows are packed into pages (`ROWS_PER_PAGE = PAGE_SIZE /
ROW_SIZE`, remainder wasted for now), and a row's location becomes
(page_num, offset within page). Sketch the Pager struct: file handle, file
size, and an array of page buffers (NULL = not loaded).

**Why.** Disks and operating systems move data in blocks, so reading 1 row
costs the same I/O as reading the whole block it sits in — engines embrace
that by making the block the atom of everything: caching, allocation,
recovery, and B-tree nodes. Deciding this on paper first matters because page
size is *frozen into the file format*: every structure you build for the rest
of the project must fit inside 4096 bytes.

---

## Task 3.2 — The pager: get_page with a cache (~1.5–2 h)

**Read first**
- cstack part 5 — his Pager is exactly this shape:
  https://cstack.github.io/db_tutorial/parts/part5.html
- CMU Lecture #04 "Memory Management" (buffer pools — yours is the naive
  version): https://15445.courses.cs.cmu.edu/fall2025/schedule.html

**Build.** `pager_open()`, `pager_get_page(pager, page_num)`: if the page is
cached, return it; else `malloc` a page buffer, `fseek` + `fread` it from the
file (a page past EOF starts zeroed), cache it, return it. All row access now
goes through `pager_get_page` — nothing else touches the file.

**Why.** This is a buffer pool with no eviction — the heart of every database
engine. The contract "ask for page N, get a pointer to its bytes, the pager
worries about disk" is the single abstraction that lets the B-tree, the WAL,
and recovery code all ignore file I/O. Real buffer pools add eviction, dirty
tracking, and pinning (CMU lecture 4 shows how deep it goes); yours stays
naive on purpose — mark the ceiling and move on.

---

## Task 3.3 — Route rows through pages (~1.5 h)

**Read first**
- cstack part 5 (row_slot -> page/offset arithmetic):
  https://cstack.github.io/db_tutorial/parts/part5.html
- cstack part 6 — read ahead; the cursor will soon own this arithmetic:
  https://cstack.github.io/db_tutorial/parts/part6.html

**Build.** Replace the Phase 2 row array: `row_slot(table, row_num)` computes
`page_num = row_num / ROWS_PER_PAGE`, asks the pager for that page, returns a
pointer to `offset = (row_num % ROWS_PER_PAGE) * ROW_SIZE` inside it. Insert
serializes into that slot; select deserializes out of it. All Phase 1/2 tests
must still pass unchanged.

**Why.** This task is two ideas: indirection (the table no longer knows where
rows physically live) and *serialize-in-place* (rows exist as bytes inside
page buffers, structs only at the edges — which is how real engines avoid
copying everything twice). Keeping the old tests green while gutting the
internals is the professional skill this phase drills.

---

## Task 3.4 — Flush pages, reopen correctly (~1–1.5 h)

**Read first**
- cstack part 5 (pager_flush, the partial-page wrinkle):
  https://cstack.github.io/db_tutorial/parts/part5.html
- SQLite Atomic Commit §3 (what a single-page write really involves):
  https://www.sqlite.org/atomiccommit.html

**Build.** `db_close()` walks the cache and writes every loaded page back at
`page_num * PAGE_SIZE` — including the trailing partial page (only write the
bytes that hold rows, for now). Reopen derives row count from file size as
before. Test: insert enough rows to span 3+ pages, close, reopen, select.

**Why.** Write-back caching creates the dirty-data problem: memory and disk
now disagree until you flush, and *you* own the moment of reconciliation.
The partial-page annoyance you hit here is a real design lesson — it exists
only because rows-count-derived-from-file-size can't distinguish a half-used
page. Phase 4's B-tree fixes it by making every page self-describing, which
is why cstack drops the partial-page code the moment nodes arrive.
