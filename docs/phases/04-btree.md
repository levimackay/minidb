# Phase 4 — A B-tree for primary-key lookup

**Why this phase exists.** The append-only array finds a row by id in O(n) and
can't stay sorted without shifting everything. The B-tree (strictly, a B+
tree: data only in leaves) is *the* database data structure — SQLite, MySQL,
Postgres indexes, and most filesystems all sit on one — because it stays
balanced, keeps keys sorted, and does lookup/insert in O(log n) while mapping
one node to one page. This is the hardest phase and the most valuable line on
a resume-level explanation of "I built a database." Take it slowly; the tasks
follow cstack parts 7–14, which exist because he needed the same slicing.

Throughout: a node = a page. "Pointer to a node" = a page number. That's why
Phase 3 had to come first.

---

## Task 4.1 — B-tree theory on paper (~1–1.5 h, no code)

**Read first**
- cstack part 7, "Introduction to the B-Tree":
  https://cstack.github.io/db_tutorial/parts/part7.html
- Use The Index, Luke — "The Search Tree (B-Tree) Makes the Index Fast":
  https://use-the-index-luke.com/sql/anatomy/the-tree
- B+ tree overview: https://en.wikipedia.org/wiki/B%2B_tree
- CMU 15-445 Fall 2025, Lectures #08–09 "Indexes & Filters" (videos linked on
  the schedule): https://15445.courses.cs.cmu.edu/fall2025/schedule.html

**Build.** Nothing in C. On paper, insert keys 1..20 into an order-3 B+ tree
by hand, splitting as you go, until drawing a split is mechanical. Then write
into `docs/FORMAT.md` the two node types you'll build (leaf: sorted key+row
cells; internal: sorted keys + child page numbers) and the invariants (all
leaves at same depth, keys sorted, internal keys separate children).

**Why.** Every B-tree bug you will write in the next five tasks is an
invariant violation, and you can't spot a broken invariant you can't state.
The paper exercise is not busywork — splitting by hand is how you'll debug
your code later ("what *should* the tree look like after this insert?"). The
CMU lectures give the why-it-wins argument: high fanout means a tree of
millions of rows is 3–4 pages deep, i.e. 3–4 disk reads.

---

## Task 4.2 — Leaf node byte layout (~1.5–2 h)

**Read first**
- cstack part 8, "B-Tree Leaf Node Format":
  https://cstack.github.io/db_tutorial/parts/part8.html
- SQLite file format §"B-tree Pages" — a production version of the same idea
  (page type byte, cell count, cells): https://www.sqlite.org/fileformat2.html

**Build.** A leaf node format inside one 4096-byte page: a small header (node
type, is_root, parent page num, num_cells) followed by cells of (key, serialized
row). Write the accessor functions that return pointers into the page for each
field (`leaf_node_num_cells(page)`, `leaf_node_key(page, i)`, ...), plus
`initialize_leaf_node`. Convert the table to "one leaf node at page 0";
insert appends into the node (still unsorted-tolerant until 4.3). Reopen must
now read num_cells from the page itself — delete the file-size arithmetic and
the partial-page hack.

**Why.** This is Phase 0's serialization lesson at full scale: a page is no
longer a dumb row array but a self-describing structure with a header — the
same move SQLite makes with its page-type byte and cell count. Accessor
functions over raw offsets are the idiom that keeps byte-layout code sane:
define the offset once, in one place, or spend a week chasing a +2 error.
`hexdump -C` earns its keep from here to the end.

---

## Task 4.3 — Sorted inserts: binary search, duplicate keys (~1.5 h)

**Read first**
- cstack part 9, "Binary Search and Duplicate Keys":
  https://cstack.github.io/db_tutorial/parts/part9.html
- Use The Index, Luke (why leaves must be sorted at all):
  https://use-the-index-luke.com/sql/anatomy/the-tree

**Build.** `table_find(table, key)` binary-searches the leaf for the key's
position; insert shifts later cells right and places the new cell there,
keeping cells sorted by key; inserting an existing id returns a duplicate-key
error. The id has just become a real primary key.

**Why.** Sorted order is what the B-tree sells: it's the precondition for
binary search inside a node, for splitting a node in halves that stay valid,
and for Phase 5's range scans. The duplicate check teaches what "primary key"
actually means mechanically — uniqueness is enforced at insert time by the
same search that finds the insert position, for free. Real engines do exactly
this: the uniqueness check *is* the index lookup.

---

## Task 4.4 — Splitting a leaf; the root grows (~2 h)

**Read first**
- cstack part 10, "Splitting a Leaf Node":
  https://cstack.github.io/db_tutorial/parts/part10.html
- B+ tree insertion/splitting: https://en.wikipedia.org/wiki/B%2B_tree

**Build.** Internal node layout (header + right-child + (child, key) cells),
and the two-part split: a full leaf splits its cells into two nodes
(upper half moves to a fresh page), and — when the split node was the root —
`create_new_root` puts a new internal node at page 0 pointing at both halves.
After 15 inserts you have a 3-node tree; `.btree` (add a meta-command that
prints the tree) shows it.

**Why.** Splitting is how a B-tree *is* balanced — it never rebalances after
the fact; it grows upward from the leaves, so every leaf stays at the same
depth by construction. Keeping the root at page 0 (copying the old root out
rather than moving the root pointer) is a real file-format trick: the tree
can grow forever while "where do I start reading" stays a constant. The
`.btree` debug command is your invariant-checker from now on — cheap
introspection tools are how real database teams survive.

---

## Task 4.5 — Searching a multi-level tree (~1.5 h)

**Read first**
- cstack part 11, "Recursively Searching the B-Tree":
  https://cstack.github.io/db_tutorial/parts/part11.html
- CMU Lecture #09 (B+ tree operations):
  https://15445.courses.cs.cmu.edu/fall2025/schedule.html

**Build.** `internal_node_find`: binary-search the internal node's keys to
pick the child, recurse until a leaf, then reuse the leaf search from 4.3.
Inserts into a multi-level tree now land in the correct leaf.

**Why.** This is the payoff moment: lookup cost is now the *height* of the
tree, not the number of rows — the O(log n) that justifies the whole phase.
The recursion also proves your internal-node invariant from 4.1 ("keys
separate children") actually holds; if a key lands in the wrong leaf, the
invariant is broken somewhere and your `.btree` printout will show where.

---

## Task 4.6 — Updating the parent after a split (~2 h)

**Read first**
- cstack part 13, "Updating Parent Node After a Split":
  https://cstack.github.io/db_tutorial/parts/part13.html
- cstack part 14, "Splitting Internal Nodes" (read; implementing is optional
  stretch): https://cstack.github.io/db_tutorial/parts/part14.html

**Build.** When a non-root leaf splits, fix the parent: update the old key,
insert a (new-child, key) cell in sorted position. Store the parent page
number in each node's header so a child can find its parent. Implementing
internal-node *splits* (a full parent) is optional stretch — at minimum,
detect the case and fail loudly rather than corrupting.

**Why.** Structure changes in a B-tree ripple upward, and this is your first
multi-node mutation: three pages (left, right, parent) must end mutually
consistent. That's the ancestor of everything hard in databases — atomic
multi-page updates are exactly what write-ahead logging (Phase 7) exists to
protect. "Fail loudly on the case you didn't implement" is also the correct
engineering move, and it's what cstack himself ships for several parts.
