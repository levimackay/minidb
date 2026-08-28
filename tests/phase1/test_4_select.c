/* Defines "done" for Task 1.4. Run: make test-phase1
 * Part 1 unit-tests table_select's exact output.
 * Part 2 drives the real binary end to end and compares every byte. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minidb.h"

int main(void)
{
    /* ---- part 1: table_select prints the contract format ---- */
    Table *t = table_new();
    assert(t != NULL);
    Row a = { .id = 1, .username = "ada", .email = "ada@x.com" };
    Row b = { .id = 2, .username = "bob", .email = "bob@x.com" };
    assert(table_insert(t, &a) == EXECUTE_SUCCESS);
    assert(table_insert(t, &b) == EXECUTE_SUCCESS);

    FILE *mem = tmpfile();
    assert(mem != NULL);
    assert(table_select(t, mem) == EXECUTE_SUCCESS);
    rewind(mem);
    char got[256] = {0};
    fread(got, 1, sizeof got - 1, mem);
    assert(strcmp(got, "(1, ada, ada@x.com)\n(2, bob, bob@x.com)\n") == 0);
    fclose(mem);
    table_free(t);

    /* ---- part 2: the whole program, byte for byte ---- */
    FILE *in = fopen("build/p1t4_in.txt", "wb");
    assert(in != NULL);
    fputs("insert 1 ada ada@x.com\n"
          "insert 2 bob bob@x.com\n"
          "select\n"
          ".exit\n", in);
    fclose(in);

    int rc = system("./build/minidb < build/p1t4_in.txt > build/p1t4_out.txt");
    assert(rc == 0);

    FILE *outf = fopen("build/p1t4_out.txt", "rb");
    assert(outf != NULL);
    char out[512] = {0};
    fread(out, 1, sizeof out - 1, outf);
    fclose(outf);

    const char *expected =
        "db > Executed.\n"
        "db > Executed.\n"
        "db > (1, ada, ada@x.com)\n"
        "(2, bob, bob@x.com)\n"
        "Executed.\n"
        "db > ";
    assert(strcmp(out, expected) == 0);

    printf("test_4 (select + end to end): PASS\n");
    return 0;
}
