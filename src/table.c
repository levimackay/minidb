/* Phase 1, tasks 1.3 / 1.4 — the in-memory table.
 * Read first: docs/phases/01-repl.md.
 */
#include <stdlib.h>
#include <string.h>
#include "minidb.h"

Table *table_new(void)
{
    /* TODO(you) task 1.3: malloc a Table, num_rows = 0.
     * (Table is ~140 KB — that's why it's heap, not stack.) */
    return NULL;
}

void table_free(Table *t)
{
    /* TODO(you) task 1.3 */
    (void)t;
}

ExecuteResult table_insert(Table *t, const Row *r)
{
    /* TODO(you) task 1.3: full table -> EXECUTE_TABLE_FULL *before*
     * touching rows[]; otherwise copy the row in and bump num_rows. */
    (void)t; (void)r;
    return EXECUTE_TABLE_FULL;
}

ExecuteResult table_select(const Table *t, FILE *out)
{
    /* TODO(you) task 1.4: fprintf each row as "(id, username, email)\n"
     * in insertion order. */
    (void)t; (void)out;
    return EXECUTE_SUCCESS;
}
