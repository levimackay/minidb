# minidb

A single file database engine, written in C, by hand.

I'm building this to understand how databases actually work, not to ship a database. Every layer by hand: binary file formats, paging, B-trees, cursors, a small SQL parser, and a write-ahead log as a stretch goal.

End state is a REPL like this:

```
db > insert 1 ada ada@lovelace.com
Executed.
db > select
(1, ada, ada@lovelace.com)
db > .exit
```

backed by a single `.db` file on disk, organized into fixed size pages holding a B-tree.

## Where it's at

Scaffolding is done: the full 7-phase roadmap, teaching docs for every phase, and the Phase 0 stub files with signatures and tests are written and committed. No implementation code yet, next up is Phase 0 itself: binary file I/O and serialization.

## The rule I hold myself to

I write every line of implementation code myself, no AI generated code. The scaffolding gives me function signatures, comments describing what to implement, and tests that define done. The bodies are mine. If I can't explain a line, I'm not done with it.

## Layout

- `ROADMAP.md` — phase list with time estimates
- `docs/phases/NN-name.md` — one file per phase, tasks sized at 1-2 hours: the concept, verified reading links, what to build, why real databases care
- `src/` — my code; Phase 0 lives in `src/phase0/`, the actual database starts in `src/` at Phase 1
- `tests/` — one assert based test per task, plain `main()` + `assert()`, no framework
- `reference/` — working reference solution for Phase 0 and Phase 1 only

## Build and run

Requires only `cc` (clang on macOS) and `make`.

```
make minidb          # build the REPL -> build/minidb
./build/minidb       # run it
make test-phase0     # run all Phase 0 tests against src/phase0
make test-phase1     # run all Phase 1 tests against src
```

To check the reference solutions pass the same tests:

```
make test-phase0 P0=reference/phase0
make test-phase1 P1=reference/phase1
```

**Last updated:** 2026-08-29 11:47 PDT

