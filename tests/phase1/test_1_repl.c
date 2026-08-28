/* Defines "done" for Task 1.1. Run: make test-phase1
 * Drives the real binary (build/minidb) through a pipe. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *slurp(const char *path)
{
    static char buf[4096];
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

int main(void)
{
    FILE *in = fopen("build/p1t1_in.txt", "wb");
    assert(in != NULL);
    fputs(".foo\n.exit\ninsert 1 never reached\n", in);
    fclose(in);

    int rc = system("./build/minidb < build/p1t1_in.txt > build/p1t1_out.txt");
    assert(rc == 0); /* .exit -> clean exit */

    char *out = slurp("build/p1t1_out.txt");
    assert(strstr(out, "db > ") != NULL);                       /* prompts */
    assert(strstr(out, "Unrecognized command '.foo'.") != NULL);
    assert(strstr(out, "never reached") == NULL);               /* stopped at .exit */

    /* EOF (no .exit) must also end the loop cleanly, not hang or crash */
    rc = system("printf '' | ./build/minidb > /dev/null");
    assert(rc == 0);

    printf("test_1 (repl loop + meta commands): PASS\n");
    return 0;
}
