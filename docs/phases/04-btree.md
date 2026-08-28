# Phase 4 — A B-tree for primary-key lookup

**Why this phase exists.** The append-only array finds a row by id in O(n) and
can't stay sorted without shifting everything. The B-tree (strictly, a B+
tree: data only in leaves) is *the* database data structure — SQLite, MySQL,
Postgres indexes, and many filesystems sit on one — because it stays
balanced, keeps keys sorted, and does lookup/insert in O(log n) while mapping
one node to one page. This is the hardest phase and the most valuable line in
a real explanation of "I built a database." Take it slowly; the tasks follow
cstack parts 7–14, which exist because he needed the same slicing.

Throughout: a node = a page. "Pointer to a node" = a page number. That's why
Phase 3 had to come first.

---

## Task 4.1 — B-tree theory on paper (~1–1.5 h, no code)

**The idea.** Start from the problem, not the structure. You want to find a
row by id without scanning everything, which means keeping rows sorted so
you can binary-search. But a sorted *array* on disk is unusable: inserting
in the middle means shifting every later row, and at millions of rows that's
rewriting most of the file per insert. You need something that is sorted for
reading but doesn't demand mass movement for writing. The answer every
database converged on: keep rows in many small sorted chunks (nodes), and
build a tree of signposts over them. Inserting touches one chunk. Searching
walks signposts to the right chunk. Chunks are pages, so "small" means
4096 bytes.

Why a *fat* tree rather than a binary tree? Because the costly operation is
reading a page from disk, so you want as few page reads per lookup as
possible. A binary node makes one two-way decision per read — a tree of a
million keys is ~20 levels, 20 page reads. A B-tree node packs hundreds of
keys into its 4 KB and makes a hundreds-way decision per read: the same
million keys fit in a tree 3–4 levels deep. Fanout is the whole game — the
tree is wide *because* the page is the unit of I/O.

The B+ variant adds one more decision: internal nodes hold only keys and
child pointers (pure signposts), and actual row data lives only in leaves.
That keeps internal nodes maximally fatty (no row bytes stealing fanout) and
sets up Phase 5's trick of chaining leaves for sequential scans. Finally,
the invariants — every leaf at the same depth, keys sorted within every
node, an internal node's keys correctly fencing its children — are not
descriptions of the structure, they *are* the structure: every operation
you write in tasks 4.2–4.6 is "make the change, restore the invariants."

**Read first**
- cstack part 7, "Introduction to the B-Tree":
  https://cstack.github.io/db_tutorial/parts/part7.html
- Use The Index, Luke — "The Search Tree (B-Tree) Makes the Index Fast":
  https://use-the-index-luke.com/sql/anatomy/the-tree
- Interactive B+ tree visualization — do your 1..20 insertions here first
  and watch the splits happen (set max degree to 3):
  https://www.cs.usfca.edu/~galles/visualization/BPlusTree.html
- CMU 15-445 Lecture #08, "B+Trees: The Best Data Structure in the World"
  (video): https://www.youtube.com/watch?v=u7ii_Lvm9rM — notes:
  https://15445.courses.cs.cmu.edu/fall2025/notes/08-indexes1.pdf
- B+ tree overview (structure, insertion, deletion, linked leaves):
  https://en.wikipedia.org/wiki/B%2B_tree

**Build.** Nothing in C. On paper, insert keys 1..20 into an order-3 B+ tree
by hand, splitting as you go, until drawing a split is mechanical. Then write
into `docs/FORMAT.md` the two node types you'll build (leaf: sorted key+row
cells; internal: sorted keys + child page numbers) and the invariants (all
leaves at same depth, keys sorted, internal keys separate children).

**Why real databases care.** Every B-tree bug you will write in the next five
tasks is an invariant violation, and you can't spot a broken invariant you
can't state. The paper exercise is not busywork — splitting by hand is how
you'll debug your code later ("what *should* the tree look like after this
insert?"). The high-fanout argument is also why databases dominate: 3–4 page
reads to find one row among millions is the performance contract everything
else (indexes, query planning, ORMs) is built on.

---

## Task 4.2 — Leaf node byte layout (~1.5–2 h)

**The idea.** Until now a page was a dumb egg-carton of rows, and the file's
*size* was the only metadata. A B-tree node needs to answer questions about
itself — what kind of node am I, how many cells do I hold, who is my parent —
so the page gets a *header*: a handful of fixed-position fields at the top,
followed by the cells (key + serialized row pairs). The page becomes
self-describing: any reader can interpret it from its own bytes, no external
bookkeeping needed. This kills Phase 3's partial-page hack for good — the
count lives in the page now, so the file is always a whole number of pages.

The engineering idiom that keeps this sane: never write raw offset
arithmetic at usage sites. Define one accessor per field —
`leaf_node_num_cells(page)`, `leaf_node_key(page, i)` — each returning a
pointer computed from constants defined once. If the header grows a field
later (4.6 adds parent; 5.2 adds next_leaf), you change the constants in one
place and every access moves correctly. The alternative — offsets sprinkled
through the code — is a week of chasing a +2 error. Note these accessors
return pointers *into the page buffer*: reading and writing node fields is
reading and writing the page in place, Phase 3.3's lesson applied to
metadata.

**Read first**
- cstack part 8, "B-Tree Leaf Node Format":
  https://cstack.github.io/db_tutorial/parts/part8.html
- SQLite file format §"B-tree Pages" — a production version of the same idea
  (page type byte, cell count, cells): https://www.sqlite.org/fileformat2.html
- CMU Lecture #03 notes — page headers and layouts in general, the family
  your leaf format belongs to:
  https://15445.courses.cs.cmu.edu/fall2025/notes/03-storage1.pdf

**Build.** A leaf node format inside one 4096-byte page: a small header (node
type, is_root, parent page num, num_cells) followed by cells of (key, serialized
row). Write the accessor functions that return pointers into the page for each
field (`leaf_node_num_cells(page)`, `leaf_node_key(page, i)`, ...), plus
`initialize_leaf_node`. Convert the table to "one leaf node at page 0";
insert appends into the node (still unsorted-tolerant until 4.3). Reopen must
now read num_cells from the page itself — delete the file-size arithmetic and
the partial-page hack.

**Why real databases care.** This is Phase 0's serialization lesson at full
scale: SQLite makes exactly this move with its page-type byte and cell count,
and its entire file is readable page by page because every page announces
what it is. Self-describing pages are also what make recovery possible at
all — a repair tool can walk a damaged file page by page and salvage what
parses. `hexdump -C` earns its keep from here to the end: you should be able
to point at your header bytes in a dump and name each one.

---

## Task 4.3 — Sorted inserts: binary search, duplicate keys (~1.5 h)

**The idea.** Binary search is the payoff of sorted order: to find a key
among n sorted cells, compare against the middle one — the answer tells you
which half to keep — and repeat. Each comparison halves the candidates, so
35 cells take at most 6 comparisons and a million would take 20. The
implementation detail that matters here is *which* position to return when
the key is absent: you want the index where the key *would* go — the first
cell with a key >= the target. That one choice makes the same function serve
three masters: lookup (is the key at the returned position?), insert (shift
everything from that position right, place the new cell), and, in Phase 5,
range scans (start scanning here). Getting the boundary conditions of this
search exactly right — empty node, key smaller than everything, key larger
than everything — is the task's real work; off-by-one errors here corrupt
the sort order and everything downstream.

Notice what falls out for free: if the returned position already holds the
key, the insert is a duplicate. That check is what makes id a real *primary
key* — uniqueness isn't a separate scan, it's a byproduct of finding where
the key belongs.

**Read first**
- cstack part 9, "Binary Search and Duplicate Keys":
  https://cstack.github.io/db_tutorial/parts/part9.html
- Binary search — the algorithm and its notorious boundary conditions:
  https://en.wikipedia.org/wiki/Binary_search
- Use The Index, Luke (why leaves must be sorted at all):
  https://use-the-index-luke.com/sql/anatomy/the-tree

**Build.** `table_find(table, key)` binary-searches the leaf for the key's
position; insert shifts later cells right and places the new cell there,
keeping cells sorted by key; inserting an existing id returns a duplicate-key
error. The id has just become a real primary key.

**Why real databases care.** Sorted order is what the B-tree sells: it's the
precondition for binary search inside a node, for splitting a node in halves
that stay valid, and for Phase 5's range scans. Real engines enforce
uniqueness exactly this way — the uniqueness check *is* the index lookup,
which is why a UNIQUE constraint in Postgres or SQLite is implemented as an
index, and why inserting into a table with many unique constraints costs
one tree-descent per constraint.

---

## Task 4.4 — Splitting a leaf; the root grows (~2 h)

**The idea.** A node is a page, a page is 4096 bytes, so a node *will* fill
up. The B-tree's answer is the *split*: allocate a fresh page, move the
upper half of the full leaf's cells there, and hand the parent a new
signpost ("keys above K now live in that node"). Two properties make this
brilliant rather than merely workable. Locality: a split touches exactly
three nodes (the full one, the new one, the parent) no matter how big the
tree is — no global reorganization, ever. And balance for free: the tree
never rebalances after the fact, because it only ever grows *upward*. When
splits propagate all the way to the root and the root itself splits, a new
root appears above it, and every leaf's depth increases by one
simultaneously. Leaves can't drift to different depths because depth only
changes at the top.

Your version adds a file-format constraint worth noticing: the root must
*stay at page 0*, because reopen has to know where to start reading without
any external record. So "the root splits" is implemented as: copy the old
root's contents to a fresh page, turn page 0 into a new internal node
pointing at the copy and the new sibling. The root's identity is an address,
not a page's contents — a real engine trick (SQLite pins each table's root
page the same way, recorded in its catalog).

**Read first**
- cstack part 10, "Splitting a Leaf Node":
  https://cstack.github.io/db_tutorial/parts/part10.html
- B+ tree insertion/splitting (the general algorithm your code implements):
  https://en.wikipedia.org/wiki/B%2B_tree
- Interactive B+ tree visualization — watch a split ripple to the root, then
  make your code produce the same tree:
  https://www.cs.usfca.edu/~galles/visualization/BPlusTree.html

**Build.** Internal node layout (header + right-child + (child, key) cells),
and the two-part split: a full leaf splits its cells into two nodes
(upper half moves to a fresh page), and — when the split node was the root —
`create_new_root` puts a new internal node at page 0 pointing at both halves.
After 15 inserts you have a 3-node tree; `.btree` (add a meta-command that
prints the tree) shows it.

**Why real databases care.** Splitting is how a B-tree *is* balanced — the
locality of the operation (three pages, always) is what makes writes to a
billion-row table affordable. The `.btree` debug command you add is your
invariant-checker from now on, and it mirrors real practice: production
engines ship introspection tools (SQLite's `sqlite3_analyzer`, Postgres's
`pageinspect`) because cheap visibility into on-disk structures is how
database teams survive their own bugs.

---

## Task 4.5 — Searching a multi-level tree (~1.5 h)

**The idea.** Search now has two flavors of node to handle, and recursion
matches the structure exactly. At an internal node, binary-search the *keys*
to pick which child page fences the target, ask the pager for that page, and
recurse; at a leaf, run 4.3's search and you're done. The subtle part is
that internal-node search answers a different question than leaf search:
not "where is this key?" but "which child could contain it?" — every
descent must pick exactly one child, whether or not the key exists. The
boundary convention (is a child responsible for keys <= its fence key or <
it?) must match what your split code wrote into the parent in 4.4; a
mismatch sends keys down the wrong branch, and the tree quietly diverges
from its invariants.

**Read first**
- cstack part 11, "Recursively Searching the B-Tree":
  https://cstack.github.io/db_tutorial/parts/part11.html
- CMU Lecture #08 video (B+ tree operations, the search walk-through):
  https://www.youtube.com/watch?v=u7ii_Lvm9rM
- Interactive visualization — build a 3-level tree and trace a search's
  path by eye, then in lldb:
  https://www.cs.usfca.edu/~galles/visualization/BPlusTree.html

**Build.** `internal_node_find`: binary-search the internal node's keys to
pick the child, recurse until a leaf, then reuse the leaf search from 4.3.
Inserts into a multi-level tree now land in the correct leaf.

**Why real databases care.** This is the payoff moment: lookup cost is now
the *height* of the tree, not the number of rows — the O(log n) that
justifies the whole phase, and in page terms, the "3–4 disk reads for any
row among millions" that databases are sold on. The recursion also proves
your internal-node invariant from 4.1 ("keys separate children") actually
holds; if a key lands in the wrong leaf, the invariant is broken somewhere
and your `.btree` printout will show where.

---

## Task 4.6 — Updating the parent after a split (~2 h)

**The idea.** When the *root* split, the parent was brand new and trivially
correct. When any other leaf splits, an *existing* parent must absorb the
news, and that's a genuinely harder problem: the old key that pointed at the
split node may now be wrong (the node kept only its lower half), and a new
(child, key) cell must be inserted in sorted position. For a child to tell
its parent anything, it must be able to *find* its parent — hence the parent
page number in every node's header, a back-pointer you now have to keep
correct through every operation that moves cells between pages.

Step back and see what this really is: your first multi-page mutation. Left
node, right node, and parent must all change, and the tree is only valid
when all three changes are present — halfway through, the structure is
briefly inconsistent, and your code is trusted to finish. In memory that's
just careful sequencing. But project it onto disk and you've found the
central problem of database engineering: a crash between page writes leaves
a *permanently* half-changed structure. Hold that thought; it is exactly
what Phase 7's write-ahead log exists to solve.

The task also hands you a scoping decision made honestly: a full *internal*
node (parent with no room for the new cell) triggers internal-node splitting,
a fiddly algorithm you may treat as stretch. What you may not do is corrupt:
detect the case and abort loudly. "Fail loudly on the case you didn't
implement" is correct engineering — cstack ships exactly that for several
parts — because a crash with a message costs a bug report, while silent
corruption costs someone's data.

**Read first**
- cstack part 13, "Updating Parent Node After a Split":
  https://cstack.github.io/db_tutorial/parts/part13.html
- cstack part 14, "Splitting Internal Nodes" (read; implementing is optional
  stretch): https://cstack.github.io/db_tutorial/parts/part14.html
- B+ tree insertion — how the textbook states the parent-update/propagation
  step your code implements: https://en.wikipedia.org/wiki/B%2B_tree

**Build.** When a non-root leaf splits, fix the parent: update the old key,
insert a (new-child, key) cell in sorted position. Store the parent page
number in each node's header so a child can find its parent. Implementing
internal-node *splits* (a full parent) is optional stretch — at minimum,
detect the case and fail loudly rather than corrupting.

**Why real databases care.** Structure changes in a B-tree ripple upward, and
atomic multi-page updates are exactly what write-ahead logging protects in
every real engine — the WAL's job description is "make the three-page change
look all-or-nothing even through a crash." When you reach Phase 7, this task
is the concrete scenario to keep in mind.
