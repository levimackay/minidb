# Phase 7 (stretch) — A write-ahead log for crash safety

**Why this phase exists.** Phase 2.3 showed you the wound: a crash mid-write
can tear your file. The write-ahead log is the standard cure, used by SQLite
(WAL mode), Postgres, MySQL — nearly everything: *first* append a description
of the change to a log and sync it, *then* touch the database file. After a
crash, replay the log. Append-then-sync is crash-safe in a way that
overwrite-in-place can never be, because appends don't destroy old data.

This is a stretch phase: the design is yours, guided by the reading. Scope it
to single-statement transactions (each insert/delete = one committed log
record) — no BEGIN/COMMIT, no rollback of half-applied statements.

---

## Task 7.1 — Understand the failure, design the log (~1.5–2 h, mostly reading)

**The idea.** Start from why overwrite-in-place can't be fixed by being
careful. To update a page you must overwrite its 4096 bytes, and a crash can
stop the disk partway — leaving a page that is half new, half old, and
entirely invalid. Worse, an update like a B-tree split (task 4.6) touches
*three* pages, and a crash between page writes leaves the structure
permanently inconsistent even if every individual write was clean. No
ordering of in-place writes escapes this, because in-place writing destroys
the old data as it goes: at the moment of the crash, neither the old state
nor the new state exists in full. The only way out is to stop destroying
the old state — write the *description* of the change somewhere else first.

That somewhere is the log: an append-only file of records, each saying
"insert this row" or "delete this key." Two protocol rules make it work.
Rule one: a change counts as committed only when its log record is fully on
disk — `fsync` returned — and not before. Rule two: the database file may
only be modified *after* the log records describing the modification are
safely down ("write ahead" is literally the ordering). Under those rules,
any crash leaves you in one of two recoverable states: the record didn't
finish (change never happened — fine, it was never committed), or the record
is durable (replay it — the database file catches up). The middle case,
"database half-changed with no record of what the change was," has been
made impossible.

Two design details make recovery trustworthy. The crash can tear *the log
itself* — the last record may be half-written — so every record carries a
checksum: a number computed from its bytes that won't match if any byte is
missing or mangled. Recovery reads records until a checksum fails and treats
that as the end of the valid log; that is precisely how SQLite and Postgres
find where a crashed log stops. And replay must be *idempotent* — safe to
apply twice — because recovery itself can crash and rerun. Your operations
can get there cheaply: think through why insert-if-absent and
delete-if-present can be replayed any number of times and land in the same
state. Idempotence is the simplification that lets you skip the LSN
bookkeeping industrial engines (ARIES) use to know what's already applied.

**Read first**
- SQLite, "Atomic Commit in SQLite" (the whole thing — the best crash-safety
  writeup anywhere): https://www.sqlite.org/atomiccommit.html
- SQLite, "Write-Ahead Logging" (the WAL flavor you're building):
  https://www.sqlite.org/wal.html
- Postgres WAL introduction — the same two rules stated by another engine:
  https://www.postgresql.org/docs/current/wal-intro.html
- CMU 15-445 Lecture #21, "Write-Ahead Logging" (video):
  https://www.youtube.com/watch?v=CedEy54pe3g — notes:
  https://15445.courses.cs.cmu.edu/fall2025/notes/21-logging.pdf

**Build.** A design page in `docs/FORMAT.md`: log record layout — you already
know how to design byte layouts, so define one: record length, type
(insert/delete), the row or key, and a checksum (a simple additive or FNV-1a
checksum is fine) — plus the two protocol rules: (1) a record is committed
only when fully on disk (fsync), (2) a database page may only be written
*after* the log records covering it are synced. Decide replay semantics
(idempotent: replaying a record twice must be harmless — think through why
insert-if-absent / delete-if-present gives you this).

**Why real databases care.** WAL is 20% code and 80% protocol — the ordering
rules are the whole trick, and stating them precisely *is* the task. Every
serious engine's recovery correctness rests on exactly the two rules you're
writing down; Postgres's documentation opens its WAL chapter by stating
them, and SQLite's atomic-commit paper is a book-length meditation on
enforcing them against lying hardware.

---

## Task 7.2 — Append, checksum, fsync (~1.5–2 h)

**The idea.** The implementation of rule one is a small, strict sequence:
serialize the record (your Phase 0 skills), append it to the `-wal` file,
`fflush` (drain the C library's buffer into the kernel), then `fsync` the
file descriptor (force the kernel's buffer to the device — you'll need
`fileno()` to get the descriptor from the `FILE *`). Only when `fsync`
returns is the statement committed; only then may the in-memory pages
change. Note what quietly stopped happening: pages are no longer flushed
per change. The database file can lag arbitrarily far behind — memory holds
the truth, the log holds the durable record of it, the database file is
just a checkpoint of the past. That inversion ("the log is the database;
the file is a cache of the log") is the mental model shift of the phase.

You'll also feel both of WAL's performance faces. Writes get *faster* in
structure: one sequential append + one fsync per statement, instead of
scattered random page writes — sequential appends are the thing disks and
SSDs do best, which is a big part of why WAL won everywhere. And fsync is
*slow* — milliseconds, the slowest single thing your program does — which
is why "fsyncs per commit" is a headline number for every engine, and why
real systems batch many transactions into one sync (group commit). Measure
it: time 100 inserts with and without the fsync line.

**Read first**
- `fsync(2)` — the only durability primitive you have:
  https://man7.org/linux/man-pages/man2/fsync.2.html
- SQLite Atomic Commit — the fsync/"flushing" sections (what syncing
  actually promises, what it costs, and how hardware lies):
  https://www.sqlite.org/atomiccommit.html
- Dan Luu, "Files are hard" — a survey of how many real programs get exactly
  this write-then-sync dance wrong: https://danluu.com/file-consistency/

**Build.** `wal_append(record)`: serialize per your format, append to
`mydb.db-wal`, `fflush` + `fsync` (you'll need `fileno()` to get the fd).
Every insert/delete appends to the WAL *before* modifying any page. Pages are
no longer flushed on every change — only the log must be current. Test:
insert rows, check the WAL file's bytes with your hexdump; kill -9 the
process; the WAL contains the records.

**Why real databases care.** The faster-and-safer pair is the
counterintuitive result that made WAL universal — SQLite's WAL mode is its
recommended high-concurrency configuration largely because commits become
one sequential append. And the `fflush`-then-`fsync` two-step is a
perennial source of real-world bugs (flushing the library buffer but not
the kernel's, or vice versa); you're building the muscle memory that the
two layers are different and both must be crossed.

---

## Task 7.3 — Recovery and checkpoint (~2 h)

**The idea.** Recovery is where the design pays out, and it's almost
anticlimactically simple because 7.1 did the thinking. On `db_open`, look
for a `-wal` file. If present, the last shutdown wasn't clean: read records
one at a time, verify each checksum, and apply each valid record through
your ordinary insert/delete code paths — recovery is just replaying
commands, which is why idempotence was the design requirement. Stop at the
first bad checksum (that's the torn tail of the crash, not an error). Then
*checkpoint*: flush all pages to the database file, fsync it, and truncate
the WAL — the log's contents are now fully reflected in the file, so the
log can be emptied. A clean `.exit` checkpoints too, which is why a clean
shutdown leaves no WAL to replay.

Checkpointing is the log's other half for a practical reason: without it
the log grows forever and every recovery replays history from the
beginning. The checkpoint is the moment "the log is the truth" gets folded
back into "the file is the truth," letting the log restart from empty.
Real engines checkpoint continuously in the background and must handle
readers mid-checkpoint; yours checkpoints at open and exit, which is the
same operation without the concurrency.

The money test deserves its name: a helper mode that inserts rows and calls
`_exit(2)` — hard process death, no flush, no atexit handlers — after which
a normal reopen must recover every committed row. That test is a *crash
simulation*, and passing it means something no earlier phase could claim:
your database survives dying mid-flight.

**Read first**
- SQLite WAL doc, "Checkpointing" and the recovery/read-behavior sections:
  https://www.sqlite.org/wal.html
- CMU 15-445 Lecture #22, "Database Crash Recovery" (video — ARIES, the
  industrial-strength version of your replay loop):
  https://www.youtube.com/watch?v=X2jc4qalNy0 — notes:
  https://15445.courses.cs.cmu.edu/fall2025/notes/22-recovery.pdf
- Write-ahead logging — the concept summarized, now that you've built it:
  https://en.wikipedia.org/wiki/Write-ahead_logging

**Build.** On `db_open`: if a WAL file exists, replay every record whose
checksum validates (stop at the first bad one), apply them through the normal
insert/delete paths, then checkpoint — flush all pages, fsync the database
file, truncate the WAL. On clean `.exit`: checkpoint too. The money test:
a helper mode that inserts rows and `_exit(2)`s without flushing pages —
run it, then reopen normally and assert every row is there.

**Why real databases care.** This is the payoff of the whole roadmap: a
database that a `kill -9` cannot corrupt, built from parts you understand
down to the byte. "The log is the truth, pages are a cache of it" is the
core mental model of modern storage systems — and of Kafka, Raft, and every
replicated log you'll ever meet, which are this same idea with the log
shipped over a network. If you get here, you have implemented, end to end,
the ideas a graduate database course spends its final month on.
