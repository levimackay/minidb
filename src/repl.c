/* Phase 1, tasks 1.1 / 1.2 / 1.4 — the REPL and statement preparation.
 * Read first: docs/phases/01-repl.md.
 * The output contract your code must hit is at the top of minidb.h.
 */
#include <stdlib.h>
#include <string.h>
#include "minidb.h"

bool is_meta_command(const char *line)
{
    /* TODO(you) task 1.1: meta commands start with '.' */
    (void)line;
    return false;
}

int do_meta_command(const char *line, FILE *out)
{
    /* TODO(you) task 1.1:
     * ".exit" -> return 1.
     * anything else -> fprintf(out, "Unrecognized command '%s'.\n", line),
     *                  return 0.
     */
    (void)line; (void)out;
    return 0;
}

PrepareResult prepare_statement(const char *line, Statement *st)
{
    /* TODO(you) task 1.2:
     * - "select" (exact word) -> STATEMENT_SELECT, PREPARE_SUCCESS.
     * - starts with "insert" -> split the rest into id, username, email
     *   (strtok on a local copy works; the parameter is const).
     *   * any of the three missing        -> PREPARE_SYNTAX_ERROR
     *   * id < 0 (parse with strtol/atoi) -> PREPARE_NEGATIVE_ID
     *   * username > COLUMN_USERNAME_SIZE or email > COLUMN_EMAIL_SIZE
     *                                     -> PREPARE_STRING_TOO_LONG
     *   * otherwise fill st->row_to_insert -> PREPARE_SUCCESS
     * - anything else -> PREPARE_UNRECOGNIZED.
     */
    (void)line; (void)st;
    return PREPARE_UNRECOGNIZED;
}

void repl_run(Table *t, FILE *in, FILE *out)
{
    /* TODO(you) tasks 1.1 + 1.4:
     * loop:
     *   1. fprintf(out, "db > ");
     *   2. getline(&buf, &cap, in); EOF -> break. Strip the trailing '\n'.
     *   3. meta command? do_meta_command; returns 1 -> break.
     *   4. prepare_statement; on failure print the matching message
     *      (see minidb.h) and continue.
     *   5. execute: insert -> table_insert (print "Executed." or
     *      "Error: Table full."); select -> table_select then "Executed.".
     * free the getline buffer before returning.
     */
    (void)t; (void)in; (void)out;
}
