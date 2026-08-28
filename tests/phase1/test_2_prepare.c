/* Defines "done" for Task 1.2. Run: make test-phase1 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "minidb.h"

int main(void)
{
    Statement st;

    /* the two good shapes */
    assert(prepare_statement("insert 1 ada ada@lovelace.com", &st) == PREPARE_SUCCESS);
    assert(st.type == STATEMENT_INSERT);
    assert(st.row_to_insert.id == 1);
    assert(strcmp(st.row_to_insert.username, "ada") == 0);
    assert(strcmp(st.row_to_insert.email, "ada@lovelace.com") == 0);

    assert(prepare_statement("select", &st) == PREPARE_SUCCESS);
    assert(st.type == STATEMENT_SELECT);

    /* the failure shapes */
    assert(prepare_statement("bogus", &st) == PREPARE_UNRECOGNIZED);
    assert(prepare_statement("insert 1 ada", &st) == PREPARE_SYNTAX_ERROR);
    assert(prepare_statement("insert", &st) == PREPARE_SYNTAX_ERROR);
    assert(prepare_statement("insert -1 ada a@b.c", &st) == PREPARE_NEGATIVE_ID);

    /* boundary: exactly the column size fits; one more does not */
    char line[256];
    char name33[34], name32[33];
    memset(name33, 'x', 33); name33[33] = '\0';
    memset(name32, 'x', 32); name32[32] = '\0';
    snprintf(line, sizeof line, "insert 1 %s a@b.c", name33);
    assert(prepare_statement(line, &st) == PREPARE_STRING_TOO_LONG);
    snprintf(line, sizeof line, "insert 1 %s a@b.c", name32);
    assert(prepare_statement(line, &st) == PREPARE_SUCCESS);

    /* prepare must not scribble on its input (it's const) */
    const char *orig = "insert 7 bob bob@x.com";
    char copy[64];
    strcpy(copy, orig);
    prepare_statement(copy, &st);
    assert(strcmp(copy, orig) == 0);

    /* meta detection belongs to task 1.1 but is retested here */
    assert(is_meta_command(".exit"));
    assert(!is_meta_command("select"));

    printf("test_2 (prepare_statement): PASS\n");
    return 0;
}
