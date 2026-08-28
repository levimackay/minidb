# reference/ — last resort only

Working solutions for Phases 0 and 1. This folder exists so a bad day can't
kill the project — it is not the first thing to open when stuck. Stuck order:
the task's "Read first" links, the cited cstack part, `hexdump -C` on your
file, and only then the smallest slice of this folder that unblocks you.
Then close it and write your own version.

Verify these pass the same tests you're aiming at:

```
make test-phase0 P0=reference/phase0
make test-phase1 P1=reference/phase1
```
