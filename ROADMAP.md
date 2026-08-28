# Roadmap

Tasks are sized at ~1–2 hours each. Estimates assume you do the "Read first"
material for each task (reading time is included). Details, links, and
definitions of done are in `docs/phases/`.

| Phase | Doc | What you end up with | Tasks | Est. |
|-------|-----|----------------------|-------|------|
| 0 | `docs/phases/00-binary-file-io.md` | You can write/read/inspect binary files and serialize structs to exact byte layouts | 4 | 5–8 h |
| 1 | `docs/phases/01-repl.md` | A REPL with in-memory `insert`/`select` | 4 | 5–8 h |
| 2 | `docs/phases/02-persistence.md` | Rows survive restart, stored in one file with a fixed layout | 3 | 4–6 h |
| 3 | `docs/phases/03-paging.md` | The file is fixed-size pages behind a pager with an in-memory cache | 4 | 5–8 h |
| 4 | `docs/phases/04-btree.md` | Rows live in a B-tree; primary-key lookup is logarithmic | 6 | 9–14 h |
| 5 | `docs/phases/05-cursors.md` | Cursor abstraction; full scans and range scans over the tree | 3 | 4–6 h |
| 6 | `docs/phases/06-sql-parser.md` | Real tokenizer + parser: `INSERT`, `SELECT`, `DELETE` with `WHERE` | 4 | 6–8 h |
| 7 | `docs/phases/07-wal.md` | (Stretch) write-ahead log; crash mid-write, restart, no corruption | 3 | 5–7 h |

Total: roughly 43–65 hours. Phases 0–1 are fully scaffolded (stubs + tests +
reference). Scaffolding for later phases is created when you get there.

Prerequisite: basic C — variables, functions, pointers, structs, `malloc`/
`free`, compiling with `cc`, and a Makefile. The tinylang project's Phase 0
(`~/Developer/tinylang`) covers exactly that; do it first if C itself is still
new. This project's Phase 0 assumes that baseline and adds only what tinylang
doesn't: binary file I/O, byte-level serialization, and hex-dump debugging.
