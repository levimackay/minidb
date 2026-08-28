# Phase 1 — A REPL with in-memory insert/select

**Why this phase exists.** Before touching disk, you build the front half of a
database: the loop that reads a command, figures out what it means, and
executes it against a table. SQLite has the same split — a front end that
compiles your input and a back end that executes it
(https://www.sqlite.org/arch.html) — and you're building a miniature of it.
Keeping the table in memory first means every bug in this phase is a logic
bug, never a file bug. Phase 2 then swaps the guts for disk storage without
the front end noticing — the first of many times this project changes the
inside of the program while its outside behavior stays frozen.

Code lives in `src/` (`minidb.h`, `repl.c`, `table.c`, `main.c`). Tasks are
done when their tests pass:

```
make test-phase1
```

This phase tracks cstack parts 1–3 closely; read the cited part before each
task, but write your own code.

---

## Task 1.1 — The read-print loop (~1 h)

**The idea.** A REPL — read, evaluate, print, loop — is the oldest
interactive program shape there is: prompt, read one line, act on it, print
the result, repeat until the user quits. Every database shell (`sqlite3`,
`psql`, `mysql`), every language shell (`python`, `node`), and every command
line you've ever used is this loop. Yours needs three pieces of C machinery:
`getline` (which, unusually for C, allocates and grows the line buffer for
you — you just have to free it once at the end), stripping the trailing
newline that `getline` leaves on (forgetting this is the classic first bug:
`".exit\n"` is not `".exit"`), and a clean way to decide what kind of input
you got.

That decision is the real design content of the task. Input starting with a
dot (`.exit`) is a *meta-command*: an instruction about the session itself,
not about the data. Everything else is a *statement*: an instruction about
the data, which later tasks will parse and execute. Keeping those two
channels separate — one function to detect, one to dispatch — is what lets
the statement pipeline grow into a real SQL front end in Phase 6 without the
control commands getting tangled into it.

One more deliberate choice: `repl_run` takes `FILE *in` and `FILE *out` as
parameters instead of using `stdin`/`stdout` directly. To a caller that
passes `stdin, stdout`, nothing changes. But a *test* can pass a temp file or
a pipe and check every output byte. Hardcoded globals are the reason most
first programs are untestable; this habit costs one extra parameter and buys
the entire test suite.

**Read first**
- cstack part 1, "Introduction and Setting up the REPL":
  https://cstack.github.io/db_tutorial/parts/part1.html
- `getline(3)` man page (it allocates the buffer for you; you free it):
  https://man7.org/linux/man-pages/man3/getline.3.html
- Read–eval–print loop — the general concept, and why interactive shells all
  converge on this shape:
  https://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop

**Build.** `repl_run()` in `src/repl.c` plus `is_meta_command()` and
`do_meta_command()`: print a `db > ` prompt, read a line with `getline`,
strip the trailing newline. Lines starting with `.` are meta-commands:
`.exit` ends the loop; anything else dot-prefixed prints
`Unrecognized command '<line>'.` and continues. Other input just prints an
error for now. `main.c` (already written — it's three lines of glue) calls
`repl_run(table, stdin, stdout)`.

**Why real databases care.** The meta-command/statement split you make here
is the same distinction sqlite3 makes with its dot-commands (`.tables`,
`.schema`, `.exit`) versus SQL. It will matter concretely in Phase 4, when
you add a `.btree` command to print the tree — a debugging tool that belongs
to the session, not the query language. And the `FILE *in/out` parameters are
what let test 4 of this phase drive your real binary through a pipe and
compare output byte for byte — the style of end-to-end test that guards every
later phase.

---

## Task 1.2 — Prepare: turn a line into a Statement (~1.5 h)

**The idea.** The lazy way to execute `insert 1 ada ada@x.com` is to pick the
line apart and act on the pieces in one function. Databases never do this,
and the reason is worth internalizing: they split *understanding* the input
from *executing* it. The understanding step — here called `prepare` — reads
the raw string once, validates everything about it, and produces a filled-in
struct (`Statement`) that the execution step can trust completely. If
anything is wrong (unknown keyword, missing argument, id that isn't a
number, a name too long for the column), prepare rejects it with a specific
error *before* any state changes.

Why bother with the split? Three reasons that get bigger as the project
grows. Safety: the executor and everything below it (the table now, the
B-tree and the file later) never see malformed input, so validation logic
lives in exactly one place. Reuse: a prepared statement is a value — real
databases prepare once and execute many times with different parameters,
which is also their defense against SQL injection. And evolvability: Phase 6
throws away this string-matching version of prepare and replaces it with a
real tokenizer and parser, and *nothing downstream changes*, because
downstream only ever depended on the `Statement` struct.

Your length checks deserve respect: the username column is a fixed-size
buffer, and C will happily let you `strcpy` 40 bytes into a 33-byte array,
silently trampling whatever lives after it. Prepare is the trust boundary
where "string from the outside world" becomes "string that provably fits."
Every buffer overflow exploit in history is a missing version of the check
you're writing here.

**Read first**
- cstack part 2, "World's Simplest SQL Compiler and Virtual Machine":
  https://cstack.github.io/db_tutorial/parts/part2.html
- SQLite architecture — see where "prepare" sits in a real engine (the whole
  front half of the diagram exists to produce a trusted, executable form):
  https://www.sqlite.org/arch.html
- `strtok(3)` man page (one way to split the input into words — note it
  modifies the string it's given, hence the local copy):
  https://man7.org/linux/man-pages/man3/strtok.3.html

**Build.** `prepare_statement()` in `src/repl.c`: recognize
`insert <id> <username> <email>` and `select`, filling a `Statement` struct.
Return the right `PrepareResult` for: unrecognized keyword, missing arguments
(syntax error), negative id, username/email longer than the column allows.
Nothing is executed yet — parsing and execution stay separate.

**Why real databases care.** This is the "compile" half of a database, and
the name is not a metaphor: SQLite literally compiles your SQL into bytecode
for a little virtual machine, and `sqlite3_prepare` is the API call that does
it. Your enum of `PrepareResult` values is the miniature of a real engine's
error taxonomy — and the discipline that every failure mode gets its own
named result, checked by a test, is what makes error messages accurate
instead of vaguely wrong.

---

## Task 1.3 — The in-memory table: insert (~1.5 h)

**The idea.** A table, at its dumbest, is an array of identical structs and a
count of how many are used. Insert copies the incoming row into slot
`count` and increments the count. That's it — deliberately. This phase's
table has no ordering, no search, no persistence; it exists so that when
later phases add each of those, you feel exactly what each one costs and
buys.

Two C-specific points carry the weight here. First, the `Table` goes on the
heap (`malloc`/`calloc`), not the stack — at ~1400 rows of ~100 bytes it's
around 140 KB, and stack frames are typically limited to a few megabytes
total; big long-lived objects belong on the heap as a matter of habit.
Second, *copy semantics*: `table_insert` copies the row into the table
(struct assignment in C copies every byte) rather than storing the caller's
pointer. If you stored the pointer, the table's contents would silently
change whenever the caller reused their row variable — a class of aliasing
bug that C makes easy and painful.

The full-table check matters more than it looks. Writing to `rows[count]`
when `count == TABLE_MAX_ROWS` doesn't crash in C — it scribbles over
whatever memory happens to be next, and the program keeps running with
corrupted state until something unrelated fails. Refusing the write with
`EXECUTE_TABLE_FULL` *before* touching the array is the difference between
an error message and silent corruption — the cardinal sin of storage.

**Read first**
- cstack part 3, "An In-Memory, Append-Only, Single-Table Database":
  https://cstack.github.io/db_tutorial/parts/part3.html
- Beej's Guide to C, manual memory allocation (`malloc`/`calloc`/`free` —
  and why `calloc`'s zeroing is useful here):
  https://beej.us/guide/bgc/html/split/manual-memory-allocation.html
- Struct padding refresher (your Row's in-memory size vs its future on-disk
  size are different things — Phase 2 will make that distinction real):
  https://en.wikipedia.org/wiki/Data_structure_alignment

**Build.** `table_new()`, `table_free()`, `table_insert()` in `src/table.c`:
a `Table` holding a fixed array of `Row`s and a count. Insert copies the row
in and bumps the count; a full table returns `EXECUTE_TABLE_FULL` instead of
writing out of bounds.

**Why real databases care.** This is the storage engine at its most
primitive: an append-only array. Append-only is not a toy idea — it's the
shape of real log-structured storage, and Phase 7's write-ahead log is
append-only for deep reasons you'll meet there. What this table *can't* do —
find a row by id without scanning, keep rows sorted, survive a restart — is
precisely the feature list of Phases 2 through 5. Building the dumb version
first is how you'll know what each smart version is actually for.

---

## Task 1.4 — Select, and the loop end to end (~1–1.5 h)

**The idea.** Select, for now, walks the array from 0 to `count` and prints
each row in a fixed format. The interesting part of this task isn't the loop
— it's *closing* the loop: input goes in one end, prepare turns it into a
Statement, execute mutates or reads the table, and formatted results come
out the other end. Once those stages connect, you have a database. Small,
amnesiac, but structurally the real thing.

The test for this task drives your actual compiled binary through a pipe and
compares the entire output — prompts, results, error messages — byte for
byte. That strictness is a lesson in itself: output format is a *contract*.
People write scripts that parse `sqlite3`'s output; if a release changed
`(1, ada, ada@x.com)` to `(1,ada,ada@x.com)`, downstream scripts would break,
so the format is pinned by tests and kept stable for years. Yours now is too:
the exact strings in `minidb.h` are frozen, and every later phase must keep
producing them.

**Read first**
- cstack part 3 again (execute_select / print_row):
  https://cstack.github.io/db_tutorial/parts/part3.html
- cstack part 4, "Our First Tests (and Bugs)" — he pins the REPL's exact
  output with end-to-end tests just like test 4 does, and immediately catches
  two real bugs: https://cstack.github.io/db_tutorial/parts/part4.html
- SQLite architecture (you now have every box in that diagram, tiny):
  https://www.sqlite.org/arch.html

**Build.** `table_select()` in `src/table.c` printing every row as
`(id, username, email)`, and the wiring in `repl_run` that executes prepared
statements: insert prints `Executed.` (or `Error: Table full.`), select prints
the rows then `Executed.`. Test 4 drives the real binary end to end through a
pipe.

**Why real databases care.** From here on, every phase changes the *inside*
of this program — disk in Phase 2, pages in Phase 3, a B-tree in Phase 4 —
while these same end-to-end tests keep passing. That's what "refactoring
behind an interface" means, and it's the only reason a codebase like SQLite
can rewrite its storage internals across decades without breaking the
millions of programs that call it. The end-to-end test you make green today
is the safety net for everything you'll do for the rest of the project.
