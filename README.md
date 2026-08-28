# minidb

A tiny single-file database engine, written in C, by hand. The goal is not the
database — it is understanding how databases work by building every layer
yourself: binary file formats, paging, B-trees, cursors, a small SQL parser,
and (stretch) a write-ahead log.

The end state is a REPL like this:

```
db > insert 1 ada ada@lovelace.com
Executed.
db > select
(1, ada, ada@lovelace.com)
db > .exit
```

backed by a single `.db` file on disk that survives restarts, organized into
fixed-size pages holding a B-tree.

## Current status

Scaffolding is done: the full 7-phase roadmap, teaching docs for every phase,
and the Phase 0 stub files with signatures and tests are all written and
committed. No implementation code yet — next up is actually writing Phase 0
(binary file I/O and serialization) by hand.

## The ground rule

**You write every line of implementation code yourself.** No AI-written
implementation code, ever. The scaffolding in this repo gives you function
signatures, comments describing what to implement, and tests that define
"done" — the bodies are yours. If you can't explain a line in your own code,
you're not done with it.

## How the roadmap is organized

- `ROADMAP.md` — the phase list with time estimates.
- `docs/phases/NN-name.md` — one file per phase, broken into tasks sized at
  roughly 1–2 hours each. Every task tells you:
  1. **The idea** — the concept taught in plain language, assuming no
     background, so the task doc alone can unstick you.
  2. **Read first** — at least 3 verified links per task, from different
     angles (reference, tutorial, video/interactive) to read *before* you
     code.
  3. **Build** — what to implement.
  4. **Why real databases care** — the connection to how real engines work.
- `src/` — your code. Phase 0 exercises live in `src/phase0/`; the actual
  database starts in `src/` with Phase 1. Stubs exist for Phases 0 and 1;
  later phases you scaffold yourself as you reach them (that's part of the
  learning).
- `tests/` — one small assert-based test per task (plain `main()` + `assert()`,
  no framework). A task is done when its test passes.
- `reference/` — a working reference solution for Phases 0 and 1 **only**.

## Build and run

Requires only `cc` (clang on macOS) and `make`.

```
make minidb          # build the REPL -> build/minidb
./build/minidb       # run it
make test-phase0     # run all Phase 0 tests against src/phase0
make test-phase1     # run all Phase 1 tests against src
```

To confirm the reference solutions pass the same tests:

```
make test-phase0 P0=reference/phase0
make test-phase1 P1=reference/phase1
```

## When you're stuck

In this order:

1. Re-read the "Read first" links for the task — the answer is usually there.
2. Read the cited part of the cstack tutorial
   (https://cstack.github.io/db_tutorial/) — it builds the same shape of
   program and each phase doc points at the matching parts.
3. Inspect your data files with `hexdump -C mydb.db` (or `xxd`). Most bugs in
   this project are visible in the bytes.
4. **Last resort:** `reference/`. It exists so a bad day can't kill the
   project, not as the first tab to open. Read the smallest slice that
   unblocks you, close it, and write your own version from memory.
