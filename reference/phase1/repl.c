/* Reference solution — Tasks 1.1, 1.2, 1.4. Open it as a last resort. */
#include <stdlib.h>
#include <string.h>
#include "minidb.h"

bool is_meta_command(const char *line)
{
    return line[0] == '.';
}

int do_meta_command(const char *line, FILE *out)
{
    if (strcmp(line, ".exit") == 0)
        return 1;
    fprintf(out, "Unrecognized command '%s'.\n", line);
    return 0;
}

PrepareResult prepare_statement(const char *line, Statement *st)
{
    if (strcmp(line, "select") == 0) {
        st->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    if (strncmp(line, "insert", 6) == 0) {
        /* strtok writes into its argument, so tokenize a local copy —
         * the caller's line must come back untouched */
        char copy[512];
        snprintf(copy, sizeof copy, "%s", line);
        strtok(copy, " ");                 /* the word "insert" */
        char *id_str = strtok(NULL, " ");
        char *username = strtok(NULL, " ");
        char *email = strtok(NULL, " ");
        if (id_str == NULL || username == NULL || email == NULL)
            return PREPARE_SYNTAX_ERROR;
        long id = strtol(id_str, NULL, 10);
        if (id < 0)
            return PREPARE_NEGATIVE_ID;
        if (strlen(username) > COLUMN_USERNAME_SIZE ||
            strlen(email) > COLUMN_EMAIL_SIZE)
            return PREPARE_STRING_TOO_LONG;
        st->type = STATEMENT_INSERT;
        st->row_to_insert.id = (uint32_t)id;
        strcpy(st->row_to_insert.username, username); /* lengths checked above */
        strcpy(st->row_to_insert.email, email);
        return PREPARE_SUCCESS;
    }
    return PREPARE_UNRECOGNIZED;
}

void repl_run(Table *t, FILE *in, FILE *out)
{
    char *buf = NULL;
    size_t cap = 0;

    for (;;) {
        fprintf(out, "db > ");
        long n = getline(&buf, &cap, in);
        if (n < 0)
            break; /* EOF */
        if (n > 0 && buf[n - 1] == '\n')
            buf[n - 1] = '\0';

        if (is_meta_command(buf)) {
            if (do_meta_command(buf, out))
                break;
            continue;
        }

        Statement st;
        PrepareResult pr = prepare_statement(buf, &st);
        if (pr == PREPARE_SYNTAX_ERROR) {
            fprintf(out, "Syntax error. Could not parse statement.\n");
            continue;
        }
        if (pr == PREPARE_STRING_TOO_LONG) {
            fprintf(out, "String is too long.\n");
            continue;
        }
        if (pr == PREPARE_NEGATIVE_ID) {
            fprintf(out, "ID must be positive.\n");
            continue;
        }
        if (pr == PREPARE_UNRECOGNIZED) {
            fprintf(out, "Unrecognized keyword at start of '%s'.\n", buf);
            continue;
        }

        if (st.type == STATEMENT_INSERT) {
            if (table_insert(t, &st.row_to_insert) == EXECUTE_TABLE_FULL)
                fprintf(out, "Error: Table full.\n");
            else
                fprintf(out, "Executed.\n");
        } else {
            table_select(t, out);
            fprintf(out, "Executed.\n");
        }
    }
    free(buf);
}
