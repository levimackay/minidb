# Phase 1 — A REPL with in-memory insert/select

**Why this phase exists.** Before touching disk, you build the front half of a
database: the loop that reads a command, figures out what it means, and
executes it against a table. SQLite has the same split — a front end that
compiles your input and a back end that executes it
(https://www.sqlite.org/arch.html) — and you're building a miniature of it.
Keeping the table in memory first means every bug in this phase is a logic
bug, never a file bug.

Code lives in `src/` (`minidb.h`, `repl.c`, `table.c`, `main.c`). Tasks are
done when their tests pass:

```
make test-phase1
```

This phase tracks cstack parts 1–3 closely; read the cited part before each
task, but write your own code.

---

## Task 1.1 — The read-print loop (~1 h)

**Read first**
- cstack part 1, "Introduction and Setting up the REPL":
  https://cstack.github.io/db_tutorial/parts/part1.html
- `getline(3)` man page (it allocates the buffer for you; you free it):
  https://man7.org/linux/man-pages/man3/getline.3.html

**Build.** `repl_run()` in `src/repl.c` plus `is_meta_command()` and
`do_meta_command()`: print a `db > ` prompt, read a line with `getline`,
strip the trailing newline. Lines starting with `.` are meta-commands:
`.exit` ends the loop; anything else dot-prefixed prints
`Unrecognized command '<line>'.` and continues. Other input just prints an
error for now. `main.c` (already written — it's three lines of glue) calls
`repl_run(table, stdin, stdout)`.

**Why.** Every interactive database — `sqlite3`, `psql`, `mysql` — is this
exact loop. The meta-command/statement split you make here (commands *about*
the session vs statements *about* the data) is the same distinction sqlite3
makes with its dot-commands. Taking `FILE *in/out` as parameters instead of
hardcoding stdin/stdout is what makes the whole program testable — a habit
worth forming on day one.

---

## Task 1.2 — Prepare: turn a line into a Statement (~1.5 h)

**Read first**
- cstack part 2, "World's Simplest SQL Compiler and Virtual Machine":
  https://cstack.github.io/db_tutorial/parts/part2.html
- SQLite architecture — see where "prepare" sits in a real engine:
  https://www.sqlite.org/arch.html
- `strtok(3)` man page (one way to split the input into words):
  https://man7.org/linux/man-pages/man3/strtok.3.html

**Build.** `prepare_statement()` in `src/repl.c`: recognize
`insert <id> <username> <email>` and `select`, filling a `Statement` struct.
Return the right `PrepareResult` for: unrecognized keyword, missing arguments
(syntax error), negative id, username/email longer than the column allows.
Nothing is executed yet — parsing and execution stay separate.

**Why.** This is the "compile" half of a database: validate the input *once*,
up front, and convert it into a checked internal representation the executor
can trust. Real engines do this so the storage layer never has to worry about
malformed input — and so the same prepared statement can be executed many
times. Your length checks here are also your first trust boundary: the column
is 32 bytes, and nothing longer may ever reach it.

---

## Task 1.3 — The in-memory table: insert (~1.5 h)

**Read first**
- cstack part 3, "An In-Memory, Append-Only, Single-Table Database":
  https://cstack.github.io/db_tutorial/parts/part3.html
- Struct padding refresher (your Row's in-memory size vs its future on-disk
  size are different things):
  https://en.wikipedia.org/wiki/Data_structure_alignment

**Build.** `table_new()`, `table_free()`, `table_insert()` in `src/table.c`:
a `Table` holding a fixed array of `Row`s and a count. Insert copies the row
in and bumps the count; a full table returns `EXECUTE_TABLE_FULL` instead of
writing out of bounds.

**Why.** This is the storage engine at its dumbest: an append-only array.
It's deliberately primitive so that when Phase 2 makes it survive restarts and
Phase 4 replaces it with a B-tree, you feel exactly what each upgrade buys and
what it costs. The full-table check matters more than it looks: refusing
writes at capacity instead of scribbling past the end is the difference
between an error message and silent corruption — the cardinal sin of storage.

---

## Task 1.4 — Select, and the loop end to end (~1–1.5 h)

**Read first**
- cstack part 3 again (execute_select / print_row):
  https://cstack.github.io/db_tutorial/parts/part3.html
- SQLite architecture (you now have every box in that diagram, tiny):
  https://www.sqlite.org/arch.html

**Build.** `table_select()` in `src/table.c` printing every row as
`(id, username, email)`, and the wiring in `repl_run` that executes prepared
statements: insert prints `Executed.` (or `Error: Table full.`), select prints
the rows then `Executed.`. Test 4 drives the real binary end to end through a
pipe.

**Why.** Closing the loop — input → prepare → execute → output — turns three
functions into a database. The exact-output test teaches the discipline every
real engine lives by: output format is a contract (people write scripts
against `sqlite3`'s output), so it gets tested byte for byte. From here on,
every phase changes the *inside* of this program while these same end-to-end
tests keep passing — that's what "refactoring behind an interface" means.
