/* Entry point — trivial glue, provided. Everything real lives in
 * repl.c and table.c, which you implement. */
#include "minidb.h"

int main(void)
{
    Table *t = table_new();
    repl_run(t, stdin, stdout);
    table_free(t);
    return 0;
}
