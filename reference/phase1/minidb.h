#ifndef MINIDB_H
#define MINIDB_H
/* minidb — Phase 1: REPL with an in-memory table.
 * See docs/phases/01-repl.md. Done when: make test-phase1
 *
 * Exact REPL output contract (test 4 checks it byte for byte):
 *   - print "db > " before reading each line
 *   - insert ok            -> "Executed.\n"
 *   - insert on full table -> "Error: Table full.\n"
 *   - select               -> one "(id, username, email)\n" per row,
 *                             then "Executed.\n"
 *   - unknown meta command -> "Unrecognized command '<line>'.\n"
 *   - unknown statement    -> "Unrecognized keyword at start of '<line>'.\n"
 *   - syntax error         -> "Syntax error. Could not parse statement.\n"
 *   - string too long      -> "String is too long.\n"
 *   - id must be positive  -> "ID must be positive.\n"
 *   - loop ends on ".exit" or end of input
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 64
#define TABLE_MAX_ROWS 1400

typedef struct {
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1]; /* +1: room for the NUL */
    char email[COLUMN_EMAIL_SIZE + 1];
} Row;

typedef struct {
    uint32_t num_rows;
    Row rows[TABLE_MAX_ROWS];
} Table;

typedef enum { STATEMENT_INSERT, STATEMENT_SELECT } StatementType;

typedef struct {
    StatementType type;
    Row row_to_insert; /* filled only for insert */
} Statement;

typedef enum {
    PREPARE_SUCCESS,
    PREPARE_SYNTAX_ERROR,       /* right keyword, wrong/missing args */
    PREPARE_STRING_TOO_LONG,    /* username or email over the column size */
    PREPARE_NEGATIVE_ID,        /* id parsed but < 0 */
    PREPARE_UNRECOGNIZED        /* first word isn't insert/select */
} PrepareResult;

typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
} ExecuteResult;

/* ---- repl.c (tasks 1.1, 1.2, 1.4) ----------------------------------- */

/* true iff the line is a meta command (starts with '.') */
bool is_meta_command(const char *line);

/* Handle a meta command. ".exit" -> return 1 (stop the loop).
 * Anything else: print Unrecognized command message to out, return 0. */
int do_meta_command(const char *line, FILE *out);

/* Parse "insert <id> <username> <email>" or "select" into *st.
 * Executes nothing. Must not modify the input string it was given. */
PrepareResult prepare_statement(const char *line, Statement *st);

/* The loop: prompt, getline from in, strip the '\n', dispatch
 * meta commands and statements, print results to out per the contract
 * above. Returns when .exit is seen or in hits EOF. */
void repl_run(Table *t, FILE *in, FILE *out);

/* ---- table.c (tasks 1.3, 1.4) --------------------------------------- */

Table *table_new(void);
void table_free(Table *t);

/* Copy *r into the table. EXECUTE_TABLE_FULL if there's no room. */
ExecuteResult table_insert(Table *t, const Row *r);

/* Print every row to out as "(id, username, email)\n", insertion order. */
ExecuteResult table_select(const Table *t, FILE *out);

#endif
