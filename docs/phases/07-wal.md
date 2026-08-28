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

**Read first**
- SQLite, "Atomic Commit in SQLite" (the whole thing — the best crash-safety
  writeup anywhere): https://www.sqlite.org/atomiccommit.html
- SQLite, "Write-Ahead Logging" (the WAL flavor you're building):
  https://www.sqlite.org/wal.html
- CMU 15-445 Fall 2025, Lectures #21–22 "Database Logging" / "Database
  Recovery" (ARIES, the industrial-strength version):
  https://15445.courses.cs.cmu.edu/fall2025/schedule.html

**Build.** A design page in `docs/FORMAT.md`: log record layout — you already
know how to design byte layouts, so define one: record length, type
(insert/delete), the row or key, and a checksum (a simple additive or FNV-1a
checksum is fine) — plus the two protocol rules: (1) a record is committed
only when fully on disk (fsync), (2) a database page may only be written
*after* the log records covering it are synced. Decide replay semantics
(idempotent: replaying a record twice must be harmless — think through why
insert-if-absent / delete-if-present gives you this).

**Why.** WAL is 20% code and 80% protocol — the ordering rules are the whole
trick, and stating them precisely *is* the task. The checksum matters because
the last log record may itself be torn by the crash; a checksum lets recovery
detect "this record is garbage, stop here," which is exactly how SQLite and
Postgres find the end of a valid log. Idempotent replay is the simplification
that lets you skip ARIES-style LSN bookkeeping.

---

## Task 7.2 — Append, checksum, fsync (~1.5–2 h)

**Read first**
- `fsync(2)` — the only durability primitive you have:
  https://man7.org/linux/man-pages/man2/fsync.2.html
- SQLite Atomic Commit §"...fsync" sections (what syncing actually promises,
  and its costs): https://www.sqlite.org/atomiccommit.html

**Build.** `wal_append(record)`: serialize per your format, append to
`mydb.db-wal`, `fflush` + `fsync` (you'll need `fileno()` to get the fd).
Every insert/delete appends to the WAL *before* modifying any page. Pages are
no longer flushed on every change — only the log must be current. Test:
insert rows, check the WAL file's bytes with your hexdump; kill -9 the
process; the WAL contains the records.

**Why.** You feel WAL's two performance faces here: the database gets
*faster* for writes (one sequential append + one fsync instead of scattered
page writes) while becoming *safer* — the counterintuitive pair that made WAL
universal. You'll also feel fsync's real cost (milliseconds, the slowest
thing your program does), which is why "how many fsyncs per commit" is a
headline number for every real engine.

---

## Task 7.3 — Recovery and checkpoint (~2 h)

**Read first**
- SQLite WAL doc, "Checkpointing" and "Recovery" sections:
  https://www.sqlite.org/wal.html
- CMU Lecture #22 "Database Recovery":
  https://15445.courses.cs.cmu.edu/fall2025/schedule.html

**Build.** On `db_open`: if a WAL file exists, replay every record whose
checksum validates (stop at the first bad one), apply them through the normal
insert/delete paths, then checkpoint — flush all pages, fsync the database
file, truncate the WAL. On clean `.exit`: checkpoint too. The money test:
a helper mode that inserts rows and `_exit(2)`s without flushing pages —
run it, then reopen normally and assert every row is there.

**Why.** This is the payoff of the whole roadmap: a database that a `kill -9`
cannot corrupt, built from parts you understand down to the byte. Recovery
turning "the log is the truth, pages are a cache of it" from slogan into
running code is the core mental model of modern storage systems (and of
Kafka, Raft, and every replicated log you'll ever meet). If you get here,
you have implemented, end to end, the same ideas a CMU graduate course spends
its final month on.
