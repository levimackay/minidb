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

**The idea.** Right now, any code that wants to visit rows must know the
whole truth about storage: that rows live in B-tree leaves, that leaves are
pages, that cells are found by index. That knowledge is smeared through the
executor, which means every storage change rewrites the executor — you felt
this in Phase 4. A cursor gathers all of it behind three tiny operations:
give me a cursor at the start (`table_start`), give me the bytes of the row
under the cursor (`cursor_value`), move to the next row
(`cursor_advance`), and tell me when there's nothing left (`end_of_table`).
The consumer's loop becomes storage-agnostic: open, while not at end, read,
advance.

A cursor is just a struct of coordinates — which table, which page, which
cell within it, plus the end flag. There's no magic; the value is entirely
in what the *consumer* no longer knows. "Position in a collection +
advance" is one of the great recurring abstractions in software (C++
iterators, Java's `Iterator`, Python generators are all this shape);
databases arrived at it decades ago because they change storage engines
under stable query engines all the time, and this is the seam that permits
it.

The proof of the cut comes in 5.2: you'll change *how traversal works*
(hopping between leaves via sibling pointers) by editing only
`cursor_advance` — `select` won't change by a character. When an
abstraction lets you make that kind of change in one place, it's cut in the
right spot; learning to feel where that spot is takes years, and this phase
lets you feel a correct one from the inside.

**Read first**
- cstack part 6, "The Cursor Abstraction":
  https://cstack.github.io/db_tutorial/parts/part6.html
- SQLite architecture — the VM executes opcodes against cursors:
  https://www.sqlite.org/arch.html
- SQLite's bytecode opcodes — search for OpenRead, Rewind, Next, Column:
  real queries compile to exactly your cursor loop:
  https://www.sqlite.org/opcode.html

**Build.** `Cursor { table, page_num, cell_num, end_of_table }` with
`table_start()` (cursor at first row, i.e. leftmost leaf's first cell —
descend the tree with key 0), `cursor_value()` (pointer to the row bytes),
`cursor_advance()`. Rewrite `select` and the insert-position logic to use
cursors only. All existing tests stay green.

**Why real databases care.** This is the interface real engines put between
the query layer and the storage layer: SQLite compiles `SELECT * FROM t`
into bytecode that opens a cursor (`OpenRead`), rewinds it, and loops
`Column`/`Next` until done — run `EXPLAIN SELECT * FROM t;` in any sqlite3
shell and you'll see your Phase 5 loop printed as opcodes. Postgres's
executor does the same through its access-method API. The cursor is where
"query" meets "storage" in every database you'll ever use.

---

## Task 5.2 — Scanning across leaves: sibling pointers (~1.5 h)

**The idea.** Advancing within a leaf is trivial — bump the cell index. The
problem is the end of the leaf: where's the next row? It's in the *next
leaf in key order*, and finding that through the tree means climbing to the
parent (and possibly grandparent) and descending again — awkward,
stateful, and O(log n) per hop. The B+ tree's fix is almost embarrassingly
simple: every leaf stores the page number of its right sibling in its
header. Advance-at-end-of-leaf becomes "load `next_leaf`, cell 0." One
header field turns the collection of leaves into a linked list threaded
through the tree, and a full scan becomes: descend once to the leftmost
leaf, then walk the chain.

The cost of the field is paid at split time: when a leaf splits, the chain
must be re-stitched (new leaf takes the old leaf's `next_leaf`; old leaf
points at the new one) — exactly like linked-list insertion, and just as
easy to get subtly wrong. That's why the test inserts keys *out of order*:
it forces splits in the middle of the chain, where a mis-stitched pointer
makes rows vanish from scans or appear out of order while every individual
node still looks healthy.

**Read first**
- cstack part 12, "Scanning a Multi-Level B-Tree":
  https://cstack.github.io/db_tutorial/parts/part12.html
- B+ tree — why leaves form a linked list:
  https://en.wikipedia.org/wiki/B%2B_tree
- Use The Index, Luke — the doubly linked leaf chain in real engines' index
  anatomy: https://use-the-index-luke.com/sql/anatomy/the-tree

**Build.** Add `next_leaf` (page number, 0 = none) to the leaf header; set it
during splits. `cursor_advance()` at the end of a leaf hops to the sibling
instead of stopping. Test: insert enough rows to force several leaves
(out of order, so splits happen in the middle), then `select` must print every
row in sorted key order.

**Why real databases care.** Sibling pointers are the B+ tree's signature
move and the reason databases use B+ trees rather than plain B-trees: after
one O(log n) descent, scanning k rows in key order costs O(k) — no climbing
back up the tree. This is mechanically why `ORDER BY primary_key` is free in
real databases and why sorted-order scans matter enough to shape the whole
structure. (Production engines usually link leaves in both directions so
scans can run backwards too; yours needs only forward.)

---

## Task 5.3 — Range scans (~1–1.5 h)

**The idea.** Everything is now in place for the query shape that indexes
exist to serve: "give me the rows with keys from lo to hi." The algorithm is
two pieces you already own, composed: *seek* — descend the tree to the
first cell with key >= lo (your 4.3 binary search already computes exactly
this position; this task just exposes it as `table_find_ge`) — then *scan* —
advance the cursor, riding 5.2's sibling chain, until the key exceeds hi or
the table ends. Cost: one O(log n) descent plus O(k) for the k rows
returned. The size of the table stops mattering; only the size of the
*answer* does.

Notice the shape of what you've built: a full scan is a range scan with
bounds of (-∞, +∞); a point lookup is a range scan where lo = hi. Three
access paths, one mechanism, distinguished only by start and stop
conditions — which is precisely why the cursor abstraction was worth
building before this.

**Read first**
- Use The Index, Luke — searching for ranges (how real engines run exactly
  this seek-then-scan, and what breaks it):
  https://use-the-index-luke.com/sql/where-clause/searching-for-ranges
- Use The Index, Luke — the tree chapter's leaf-node chain discussion:
  https://use-the-index-luke.com/sql/anatomy/the-tree
- CMU Lecture #08 (index scans; video):
  https://www.youtube.com/watch?v=u7ii_Lvm9rM

**Build.** `table_find_ge(key)` — position a cursor at the first cell with
key >= the target (your 4.3 binary search already returns exactly this
position; expose it). Then a range select: start at `find_ge(lo)`, advance
while key <= hi. Wire a temporary command (e.g. `select 5 10`) to it; Phase 6
replaces that syntax with a real `WHERE`.

**Why real databases care.** Point lookup, range scan, and full scan are the
three access paths every query planner on earth chooses between; you now
have all three, sharing one cursor. Seeing that a range scan is just "seek +
advance while" demystifies `BETWEEN`, `>=`, and index-backed pagination in
one stroke — and gives Phase 6's WHERE executor its fast path. It's also the
key to reading `EXPLAIN` output for the rest of your career: "Index Range
Scan" in a query plan means exactly the loop you just wrote.
