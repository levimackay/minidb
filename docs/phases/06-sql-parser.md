# Phase 6 — A tiny SQL parser: INSERT / SELECT / DELETE with WHERE

**Why this phase exists.** Your `prepare_statement` is string-matching, not
parsing — it can't handle `WHERE id >= 5`, quoted strings, or good error
messages. This phase replaces it with the real two-step pipeline every
language tool uses (and that tinylang uses too): characters → tokens
(scanner), tokens → structure (parser), structure → execution. It's the same
front half SQLite's architecture diagram shows — tokenizer, parser, then
execution — just miniature.

Grammar to support (keep it this small):

```
statement := insert | select | delete
insert    := "insert" NUMBER IDENT IDENT
select    := "select" where?
delete    := "delete" where
where     := "where" "id" op NUMBER
op        := "=" | "<" | "<=" | ">" | ">="
```

---

## Task 6.1 — The tokenizer (~1.5–2 h)

**The idea.** Raw input is a stream of characters, and characters are the
wrong unit to reason about: `select where id <= 10` is 21 characters but
only five meaningful things. A tokenizer (scanner, lexer — same job) makes
one pass over the characters and groups them into *tokens*: the atomic words
of the language, each tagged with a type (keyword, number, identifier,
operator) and its position in the input. Everything annoying about raw text
— skipping whitespace, deciding where a word ends, handling junk characters
— gets solved exactly once, here, and never leaks upward.

Two classic techniques do most of the work. *Lookahead*: when you see `<`,
you can't classify it until you peek at the next character — `<=` is one
token, `<` followed by `5` is another story. One character of peeking is
enough for this grammar (and for most). *Maximal munch*: when two readings
are possible, the longest token wins — `<=` is always the single operator,
never `<` then `=`; `insertx` is an identifier, never the keyword `insert`
followed by `x`. These two rules are why tokenizers are boring in the best
way: mechanical, decidable, easy to test exhaustively.

Note what a token *is* in your design: a type plus a pointer into the input
and a length — not a copied string. Position makes precise error messages
possible later ("at position 17"); not copying keeps the tokenizer
allocation-free.

**Read first**
- Crafting Interpreters ch. 4, "Scanning" — the best walkthrough of writing
  a scanner by hand: https://craftinginterpreters.com/scanning.html
- Lexical analysis — the general theory: tokens, lexemes, maximal munch:
  https://en.wikipedia.org/wiki/Lexical_analysis
- SQLite architecture (tokenizer box — a real engine drawing the same
  boundary): https://www.sqlite.org/arch.html

**Build.** `Token { type, start, length }` and `tokenize(input)` producing
keywords (`insert`/`select`/`delete`/`where`/`id`), numbers, identifiers,
operators (`=` `<` `<=` `>` `>=`), and an ERROR token for anything else.
Assert-test it hard: token streams for good input, and precise behavior on
junk (`<=` vs `<` needs one-character lookahead).

**Why real databases care.** SQLite has a hand-written tokenizer
(`tokenize.c`) doing exactly this job for real SQL. Tokenizing first means
the parser never touches raw characters, which is why parser code in every
serious codebase reads like grammar rules instead of character soup. This is
also the identical skill tinylang's scanner teaches, deliberately: seeing
the same technique work on two different languages is what turns a trick
into knowledge.

---

## Task 6.2 — Recursive descent parser (~2 h)

**The idea.** Tokens in a valid order mean something; tokens in the wrong
order are an error — a *grammar* is the formal statement of which orders
mean what, and yours is written at the top of this file. Recursive descent
is the technique of translating that grammar directly into code: each rule
becomes one function, and a rule that references other rules becomes a
function that calls them. `parse_statement` looks at the first token and
dispatches; `parse_where` expects `where`, then `id`, then an operator, then
a number, consuming tokens as it matches and complaining the moment reality
diverges from the rule. The parser is the grammar, executable.

Its output is the point: not "valid/invalid" but a *structure* — your
enriched `Statement` with an optional `Where {op, value}` is a (very small)
abstract syntax tree. Meaning has been extracted from text into typed
fields the executor can switch on, and text never appears again downstream.
And error messages come nearly free: at any failure, the parser knows
exactly which rule it was in, what token it expected, and where in the input
it stood — so "expected number after '>=' at position 17" is just printing
your current state. Bad error messages in real tools are almost always a
symptom of parsing done sloppily; good parsers make good errors cheap.

**Read first**
- Crafting Interpreters ch. 6, "Parsing Expressions" (recursive descent
  mechanics): https://craftinginterpreters.com/parsing-expressions.html
- Crafting Interpreters ch. 5, "Representing Code" — grammars and why the
  output of parsing is a tree: https://craftinginterpreters.com/representing-code.html
- Recursive descent parser — the definition plus a complete worked example
  in C: https://en.wikipedia.org/wiki/Recursive_descent_parser
- Your grammar above — each rule becomes one function.

**Build.** `parse(tokens)` returning a richer `Statement`: type, row (for
insert), and an optional `Where { op, value }`. One function per grammar rule;
unexpected tokens produce an error naming what was expected and where
(`expected number after '>=' at position 17`). Replace `prepare_statement`;
all Phase 1 tests still pass (update expected error messages if yours are
better).

**Why real databases care.** Recursive descent is how many production
parsers are written by hand — clang and GCC parse C and C++ this way.
(SQLite is an instructive exception: its grammar is fed to a parser
*generator* called Lemon, which emits table-driven C — a different route to
the same tokens-to-tree pipeline, chosen because full SQL's grammar is huge.
At your grammar's size, hand-written descent is simpler and clearer.) The
durable lesson is the pipeline itself: text → tokens → tree → execution,
with validation finished before execution begins — the same shape as Phase
1's prepare/execute split, now industrial-strength.

---

## Task 6.3 — Executing WHERE (~1.5 h)

**The idea.** The executor now holds a parsed predicate in one hand and, from
Phase 5, three ways to visit rows in the other: point lookup, range scan,
full scan. This task is the marriage: *look at the predicate and pick the
cheapest access path that answers it*. `id = 7` → point lookup, one
tree descent. `id >= 5` → seek to 5, scan right. `id < 100` → full-ish scan
with an early stop (start at the leftmost leaf, stop when keys reach 100).
No WHERE → full scan. The decision takes a few lines of `switch`, and those
few lines are a genuine, if minimal, *query planner* — the component whose
job is choosing how to compute an answer, separate from what the answer is.

Internalize the shape of the reasoning, because it generalizes: the planner
asks "does the predicate constrain the key the tree is sorted by, and in a
way that maps to seek/scan bounds?" When yes, the index helps; when no
(imagine `WHERE username = 'ada'` — sorted by id, not username), nothing
beats reading everything. Every mysterious "why isn't my query using the
index?" in real systems is this question answered "no" for a reason the
developer didn't see.

**Read first**
- SQLite, "The SQLite Query Optimizer Overview" / query planning — a real
  engine describing exactly this predicate-to-access-path decision:
  https://www.sqlite.org/queryplanner.html
- Use The Index, Luke (when an index helps a predicate — and when it can't):
  https://use-the-index-luke.com/sql/anatomy/the-tree
- CMU Lecture #08 (index scans vs full scans; video):
  https://www.youtube.com/watch?v=u7ii_Lvm9rM

**Build.** `select where id = k` uses `table_find` (point lookup);
`>=`/`>`/`<`/`<=` use `find_ge` + bounded cursor advance (Phase 5's range
scan); no WHERE means full scan. One executor, three access paths, chosen by
looking at the predicate.

**Why real databases care.** This is the exact decision `EXPLAIN` prints in
Postgres/SQLite — "SEARCH t USING INTEGER PRIMARY KEY (id=?)" versus "SCAN
t" is your `switch`, scaled up with cost estimation and statistics. Having
built the two-line version, index usage in real databases stops being
folklore and becomes mechanics you can reason about — arguably the most
practically valuable intuition this whole project builds.

---

## Task 6.4 — DELETE (~1.5–2 h)

**The idea.** Deleting from a leaf is insertion mirrored: find the cell (the
same search as everything else), shift the later cells left over it,
decrement the count. The interesting decision is what you *don't* do. The
textbook B-tree stays half-full everywhere by merging or redistributing
nodes whenever deletion drops one below half capacity — an algorithm
famously fussier than splitting, with a bestiary of cases. You will skip it,
deliberately, and let under-full (even empty) leaves persist. The tree stays
*correct* — searches, scans, and inserts all still work, since no invariant
you rely on requires minimum occupancy — it just wastes space if many
deletes never get refilled.

What makes this a real lesson rather than a shortcut is that shipping
engines make the same call. Postgres's B-tree deliberately never merges
half-full pages — its internal README calls moving items between pages
impractical (concurrent scans could miss them) — and only reclaims
*completely* empty pages. SQLite tolerates under-full pages and recycles
freed ones through an on-disk freelist. The textbook optimizes worst-case
space bounds; engineers optimize for code they can trust under concurrency
and crashes, and accept some slack. Knowing when the shipped answer differs
from the textbook answer — and why — is a genuine database lesson.

**Read first**
- B+ tree deletion (the textbook algorithm you're deliberately not
  implementing): https://en.wikipedia.org/wiki/B%2B_tree
- Postgres nbtree README — "We consider deleting an entire page from the
  btree only when it's become completely empty" (search "empty"): 
  https://github.com/postgres/postgres/blob/master/src/backend/access/nbtree/README
- SQLite file format — freelist pages (how SQLite recycles space instead of
  rebalancing eagerly): https://www.sqlite.org/fileformat2.html
- Interactive visualization — watch textbook deletion merge nodes, so you
  know exactly what you're skipping:
  https://www.cs.usfca.edu/~galles/visualization/BPlusTree.html

**Build.** `delete where id = k`: find the leaf cell, shift later cells left,
decrement num_cells. Deliberately do *not* implement node merging or
rebalancing when a leaf gets empty-ish — an empty leaf may simply persist.
Document that ceiling in `docs/FORMAT.md`. Test: insert, delete, select shows
the survivors; delete a missing key errors cleanly; reopen still works.

**Why real databases care.** Deletion completes CRUD-minus-update, making
minidb a usable (tiny) database. And the documented ceiling — "space from
deletes is not reclaimed" — is the honest version of a real phenomenon:
databases grow and don't shrink on delete (hence `VACUUM` in SQLite and
Postgres, which exist precisely because deletion leaves holes that ordinary
operation never repacks).
