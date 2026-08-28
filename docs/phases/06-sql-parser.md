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

**Read first**
- Crafting Interpreters ch. 4, "Scanning":
  https://craftinginterpreters.com/scanning.html
- SQLite architecture (tokenizer box):
  https://www.sqlite.org/arch.html

**Build.** `Token { type, start, length }` and `tokenize(input)` producing
keywords (`insert`/`select`/`delete`/`where`/`id`), numbers, identifiers,
operators (`=` `<` `<=` `>` `>=`), and an ERROR token for anything else.
Assert-test it hard: token streams for good input, and precise behavior on
junk (`<=` vs `<` needs one-character lookahead).

**Why.** Tokenizing first means the parser never touches raw characters —
whitespace, lookahead, and "is `<=` one thing or two" get solved once, here.
This is the identical skill tinylang's scanner teaches, deliberately: seeing
the same technique work on two different languages is what turns a trick into
knowledge. Maximal munch, lookahead, and token-with-position are universal.

---

## Task 6.2 — Recursive descent parser (~2 h)

**Read first**
- Crafting Interpreters ch. 6, "Parsing Expressions" (recursive descent):
  https://craftinginterpreters.com/parsing-expressions.html
- Your grammar above — each rule becomes one function.

**Build.** `parse(tokens)` returning a richer `Statement`: type, row (for
insert), and an optional `Where { op, value }`. One function per grammar rule;
unexpected tokens produce an error naming what was expected and where
(`expected number after '>=' at position 17`). Replace `prepare_statement`;
all Phase 1 tests still pass (update expected error messages if yours are
better).

**Why.** Recursive descent — "each grammar rule becomes a function" — is how
most production parsers are actually written, including SQLite's and clang's.
The deeper lesson is representing meaning as a structure (your Statement is a
tiny AST) so that *validating* input and *acting* on it stay decoupled. Good
error messages fall out of the structure almost for free, which is why parser
quality and error quality always travel together.

---

## Task 6.3 — Executing WHERE (~1.5 h)

**Read first**
- Use The Index, Luke (when an index helps a predicate — and when it can't):
  https://use-the-index-luke.com/sql/anatomy/the-tree
- CMU Lectures #08–09 (access path selection, in miniature):
  https://15445.courses.cs.cmu.edu/fall2025/schedule.html

**Build.** `select where id = k` uses `table_find` (point lookup);
`>=`/`>`/`<`/`<=` use `find_ge` + bounded cursor advance (Phase 5's range
scan); no WHERE means full scan. One executor, three access paths, chosen by
looking at the predicate.

**Why.** You just wrote a query planner — a two-line one, but real: examine
the predicate, pick the cheapest access path the index supports. This is the
exact decision `EXPLAIN` prints in Postgres/SQLite, and having built it, index
usage in real databases stops being folklore ("why doesn't my query use the
index?") and becomes mechanics you can reason about.

---

## Task 6.4 — DELETE (~1.5–2 h)

**Read first**
- B+ tree deletion (and why everyone cheats):
  https://en.wikipedia.org/wiki/B%2B_tree
- SQLite file format — freelist pages (how SQLite recycles space):
  https://www.sqlite.org/fileformat2.html

**Build.** `delete where id = k`: find the leaf cell, shift later cells left,
decrement num_cells. Deliberately do *not* implement node merging or
rebalancing when a leaf gets empty-ish — an empty leaf may simply persist.
Document that ceiling in `docs/FORMAT.md`. Test: insert, delete, select shows
the survivors; delete a missing key errors cleanly; reopen still works.

**Why.** Textbook B-tree deletion (merge/redistribute to keep nodes half
full) is famously fussy, and real engines often skip it — SQLite tolerates
under-full pages and recycles emptied ones via a freelist rather than eagerly
rebalancing. Knowing *when* the textbook answer is not the shipped answer is
a genuine database lesson. Deletion also completes CRUD-minus-update, making
minidb a usable (tiny) database.
