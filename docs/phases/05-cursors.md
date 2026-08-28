# Phase 5 — Cursors and range scans

**Why this phase exists.** `select` currently means "walk everything," and the
walking logic is smeared through the executor. A *cursor* — "a position in
the table, with `advance`" — is the abstraction every real engine uses to
separate "how to traverse storage" from "what to do with each row." Once it
exists, full scans, point lookups, and range scans are the same loop with
different start/stop conditions, and Phase 6's `WHERE` clause plugs straight
into it.

(If you followed cstack, a proto-cursor appeared during Phases 3–4; this
phase finishes and owns it.)

---

## Task 5.1 — The cursor abstraction (~1.5 h)

**Read first**
- cstack part 6, "The Cursor Abstraction":
  https://cstack.github.io/db_tutorial/parts/part6.html
- SQLite architecture — the VM executes opcodes against cursors:
  https://www.sqlite.org/arch.html

**Build.** `Cursor { table, page_num, cell_num, end_of_table }` with
`table_start()` (cursor at first row, i.e. leftmost leaf's first cell —
descend the tree with key 0), `cursor_value()` (pointer to the row bytes),
`cursor_advance()`. Rewrite `select` and the insert-position logic to use
cursors only. All existing tests stay green.

**Why.** This is the interface real engines put between the query layer and
the storage layer: SQLite's bytecode VM literally executes `OpenCursor`,
`Next`, `Column` opcodes. The immediate payoff is that only cursor code knows
the tree exists — when you change traversal in 5.2, `select` doesn't change
at all. Recognizing where to cut an abstraction like this is a senior-
engineer skill; here you get to feel a correct cut.

---

## Task 5.2 — Scanning across leaves: sibling pointers (~1.5 h)

**Read first**
- cstack part 12, "Scanning a Multi-Level B-Tree":
  https://cstack.github.io/db_tutorial/parts/part12.html
- B+ tree — why leaves form a linked list:
  https://en.wikipedia.org/wiki/B%2B_tree

**Build.** Add `next_leaf` (page number, 0 = none) to the leaf header; set it
during splits. `cursor_advance()` at the end of a leaf hops to the sibling
instead of stopping. Test: insert enough rows to force several leaves
(out of order, so splits happen in the middle), then `select` must print every
row in sorted key order.

**Why.** Sibling pointers are the B+ tree's signature move and the reason
databases use B+ trees rather than plain B-trees: after one O(log n) descent,
scanning k rows in key order costs O(k) — no climbing back up the tree. This
is mechanically why `ORDER BY primary_key` is free in real databases and why
sorted-order scans matter enough to shape the whole structure.

---

## Task 5.3 — Range scans (~1–1.5 h)

**Read first**
- Use The Index, Luke — the tree chapter's leaf-node chain discussion:
  https://use-the-index-luke.com/sql/anatomy/the-tree
- CMU Lecture #08–09 (index scans vs full scans):
  https://15445.courses.cs.cmu.edu/fall2025/schedule.html

**Build.** `table_find_ge(key)` — position a cursor at the first cell with
key >= the target (your 4.3 binary search already returns exactly this
position; expose it). Then a range select: start at `find_ge(lo)`, advance
while key <= hi. Wire a temporary command (e.g. `select 5 10`) to it; Phase 6
replaces that syntax with a real `WHERE`.

**Why.** Point lookup, range scan, and full scan are the three access paths
that every query planner on earth chooses between; you now have all three,
sharing one cursor. Seeing that a range scan is just "seek + advance while"
demystifies `BETWEEN`, `>=`, and index-backed pagination in one stroke —
and gives Phase 6's WHERE executor its fast path.
