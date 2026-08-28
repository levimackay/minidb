/* Reference solution — Tasks 1.3, 1.4. Open it as a last resort. */
#include <stdlib.h>
#include "minidb.h"

Table *table_new(void)
{
    /* calloc: num_rows starts at 0 and the row area is zeroed */
    return calloc(1, sizeof(Table));
}

void table_free(Table *t)
{
    free(t);
}

ExecuteResult table_insert(Table *t, const Row *r)
{
    if (t->num_rows >= TABLE_MAX_ROWS)
        return EXECUTE_TABLE_FULL; /* refuse before touching rows[] */
    t->rows[t->num_rows] = *r;     /* struct assignment copies the row */
    t->num_rows++;
    return EXECUTE_SUCCESS;
}

ExecuteResult table_select(const Table *t, FILE *out)
{
    for (uint32_t i = 0; i < t->num_rows; i++)
        fprintf(out, "(%u, %s, %s)\n",
                t->rows[i].id, t->rows[i].username, t->rows[i].email);
    return EXECUTE_SUCCESS;
}
