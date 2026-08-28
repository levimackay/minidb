/* Defines "done" for Task 1.3. Run: make test-phase1 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "minidb.h"

int main(void)
{
    Table *t = table_new();
    assert(t != NULL);
    assert(t->num_rows == 0);

    /* insert copies the row — mutating the source later must not matter */
    Row r = { .id = 1, .username = "ada", .email = "ada@x.com" };
    assert(table_insert(t, &r) == EXECUTE_SUCCESS);
    r.id = 999;
    strcpy(r.username, "mutated");
    assert(t->num_rows == 1);
    assert(t->rows[0].id == 1);
    assert(strcmp(t->rows[0].username, "ada") == 0);

    /* fill to capacity: every insert up to the cap succeeds */
    for (uint32_t i = 1; i < TABLE_MAX_ROWS; i++) {
        Row x = { .id = i + 1 };
        snprintf(x.username, sizeof x.username, "u%u", i);
        snprintf(x.email, sizeof x.email, "u%u@x.com", i);
        assert(table_insert(t, &x) == EXECUTE_SUCCESS);
    }
    assert(t->num_rows == TABLE_MAX_ROWS);

    /* one more is refused — and changes nothing */
    Row overflow = { .id = 9999, .username = "no", .email = "no@x.com" };
    assert(table_insert(t, &overflow) == EXECUTE_TABLE_FULL);
    assert(t->num_rows == TABLE_MAX_ROWS);
    assert(t->rows[TABLE_MAX_ROWS - 1].id != 9999);

    table_free(t);
    printf("test_3 (table insert): PASS\n");
    return 0;
}
