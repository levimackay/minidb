# minidb — plain make, no dependencies beyond cc.
#
#   make minidb                          build the REPL -> build/minidb
#   make test-phase0                     run Phase 0 tests against src/phase0
#   make test-phase1                     run Phase 1 tests against src
#   make test-phase0 P0=reference/phase0 same tests, reference solution
#   make test-phase1 P1=reference/phase1 same tests, reference solution

CC      = cc
CFLAGS  = -std=gnu11 -Wall -Wextra -g
P0      = src/phase0
P1      = src

.PHONY: all minidb test-phase0 test-phase1 clean

all: minidb

build:
	mkdir -p build

minidb: build
	$(CC) $(CFLAGS) -I$(P1) $(P1)/main.c $(P1)/repl.c $(P1)/table.c -o build/minidb

test-phase0: build
	$(CC) $(CFLAGS) -I$(P0) tests/phase0/test_01.c $(P0)/01_file_bytes.c   -o build/p0t1 && ./build/p0t1
	$(CC) $(CFLAGS) -I$(P0) tests/phase0/test_02.c $(P0)/02_hexdump.c      -o build/p0t2 && ./build/p0t2
	$(CC) $(CFLAGS) -I$(P0) tests/phase0/test_03.c $(P0)/03_serialize.c    -o build/p0t3 && ./build/p0t3
	$(CC) $(CFLAGS) -I$(P0) tests/phase0/test_04.c $(P0)/03_serialize.c $(P0)/04_random_access.c -o build/p0t4 && ./build/p0t4
	@echo "phase 0: all tests passed"

test-phase1: minidb
	$(CC) $(CFLAGS) -I$(P1) tests/phase1/test_1_repl.c                          -o build/p1t1 && ./build/p1t1
	$(CC) $(CFLAGS) -I$(P1) tests/phase1/test_2_prepare.c $(P1)/repl.c $(P1)/table.c -o build/p1t2 && ./build/p1t2
	$(CC) $(CFLAGS) -I$(P1) tests/phase1/test_3_insert.c  $(P1)/repl.c $(P1)/table.c -o build/p1t3 && ./build/p1t3
	$(CC) $(CFLAGS) -I$(P1) tests/phase1/test_4_select.c  $(P1)/repl.c $(P1)/table.c -o build/p1t4 && ./build/p1t4
	@echo "phase 1: all tests passed"

clean:
	rm -rf build
