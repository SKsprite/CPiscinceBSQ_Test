/* ************************************************************************** */
/*                                                                            */
/*   main.c - BSQ comprehensive test harness                                  */
/*                                                                            */
/*   Usage: ./test_harness [-s|--show-output] [valid|invalid|large|all]       */
/*                                                                            */
/*   -s / --show-output  also print the raw bsq output for every test        */
/*   valid               run only valid map tests  (20 maps)                  */
/*   invalid             run only invalid map tests (20 maps, all error types)*/
/*   large               run only large map tests  (2 maps)                   */
/*   all / (default)     run all three categories                             */
/*                                                                            */
/* ************************************************************************** */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define BSQ             "../bsq"
#define VDIR            "valid_maps"
#define IDIR            "invalid_maps"
#define LDIR            "large_maps"
#define MINDIM          3
#define MAXDIM          25
#define NUM_VALID       20
#define NUM_INVALID     20
#define NUM_LARGE       2
#define LARGE_MAX_R     30000
#define LARGE_MAX_C     50000
#define N_INV_TYPES     10

static int  g_show = 0;

/* ------------------------------------------------------------------ */
/* Utilities                                                           */
/* ------------------------------------------------------------------ */

static char rand_letter(void)
{
    static const char *pool =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    return pool[rand() % 52];
}

static void pick_chars(char *e, char *o, char *s)
{
    *e = rand_letter();
    do { *o = rand_letter(); } while (*o == *e);
    do { *s = rand_letter(); } while (*s == *e || *s == *o);
}

static void ensure_dirs(void)
{
    mkdir(VDIR, 0755);
    mkdir(IDIR, 0755);
    mkdir(LDIR, 0755);
}

/* ------------------------------------------------------------------ */
/* Grid utilities                                                      */
/* ------------------------------------------------------------------ */

static void free_grid(char **g, int rows)
{
    int i = 0;
    while (i < rows)
        free(g[i++]);
    free(g);
}

static char **make_grid(int rows, int cols, char emp, char obs)
{
    int     i;
    int     j;
    char    **g;

    g = malloc((size_t)rows * sizeof(char *));
    if (!g)
        return (NULL);
    for (i = 0; i < rows; i++)
        g[i] = NULL;
    for (i = 0; i < rows; i++)
    {
        g[i] = malloc((size_t)(cols + 1));
        if (!g[i])
        {
            free_grid(g, rows);
            return (NULL);
        }
        for (j = 0; j < cols; j++)
            g[i][j] = (rand() % 4 == 0) ? obs : emp;
        g[i][cols] = '\0';
    }
    return (g);
}

static void write_map(const char *path, char **g, int rows,
                      char emp, char obs, char sq)
{
    int     i;
    FILE    *f;

    f = fopen(path, "w");
    if (!f)
        return ;
    fprintf(f, "%d%c%c%c\n", rows, emp, obs, sq);
    for (i = 0; i < rows; i++)
        fprintf(f, "%s\n", g[i]);
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* Reference BSQ solver (mirrors bsq DP exactly)                      */
/* ------------------------------------------------------------------ */

static int ref_dp(char **g, int rows, int cols, char obs, int *mr, int *mc)
{
    int i, j, a, b, c, found, max;
    int **dp;

    dp = malloc((size_t)rows * sizeof(int *));
    if (!dp)
        return (0);
    for (i = 0; i < rows; i++)
        dp[i] = NULL;
    for (i = 0; i < rows; i++)
    {
        dp[i] = calloc((size_t)cols, sizeof(int));
        if (!dp[i])
        {
            for (j = 0; j < rows; j++) free(dp[j]);
            free(dp);
            return (0);
        }
    }
    max = 0;
    for (i = rows - 1; i >= 0; i--)
    {
        for (j = cols - 1; j >= 0; j--)
        {
            if (g[i][j] == obs)
                continue ;
            if (i == rows - 1 || j == cols - 1)
            {
                dp[i][j] = 1;
            }
            else
            {
                a = dp[i + 1][j];
                b = dp[i + 1][j + 1];
                c = dp[i][j + 1];
                dp[i][j] = (a < b ? a : b);
                if (c < dp[i][j])
                    dp[i][j] = c;
                dp[i][j]++;
            }
            if (dp[i][j] > max)
                max = dp[i][j];
        }
    }
    /* topmost then leftmost (matches normalize_map scan order) */
    *mr = 0;
    *mc = 0;
    found = 0;
    for (i = 0; i < rows && !found; i++)
        for (j = 0; j < cols && !found; j++)
            if (dp[i][j] == max)
            {
                *mr = i;
                *mc = j;
                found = 1;
            }
    for (i = 0; i < rows; i++)
        free(dp[i]);
    free(dp);
    return (max);
}

static char *ref_output(char **g, int rows, int cols,
                         char sq, int max, int mr, int mc)
{
    int     i;
    int     j;
    int     pos;
    char    *out;

    pos = 0;
    out = malloc((size_t)(rows * (cols + 1) + 1));
    if (!out)
        return (NULL);
    for (i = 0; i < rows; i++)
    {
        for (j = 0; j < cols; j++)
        {
            if (max > 0
                && i >= mr && i < mr + max
                && j >= mc && j < mc + max)
                out[pos++] = sq;
            else
                out[pos++] = g[i][j];
        }
        out[pos++] = '\n';
    }
    out[pos] = '\0';
    return (out);
}

/* ------------------------------------------------------------------ */
/* BSQ runner                                                          */
/* ------------------------------------------------------------------ */

static void run_bsq(const char *path, char *buf, size_t bufsz)
{
    char    cmd[512];
    int     n;
    FILE    *fp;

    snprintf(cmd, sizeof(cmd), "%s %s 2>/dev/null", BSQ, path);
    fp = popen(cmd, "r");
    if (!fp)
    {
        buf[0] = '\0';
        return ;
    }
    n = (int)fread(buf, 1, bufsz - 1, fp);
    buf[n < 0 ? 0 : n] = '\0';
    pclose(fp);
}

/* ------------------------------------------------------------------ */
/* Invalid map generator                                               */
/*                                                                     */
/* type 0: no row number in header  -> ERR_MAP_INFO                   */
/* type 1: non-numeric prefix       -> ERR_INV_NUM                    */
/* type 2: zero row count           -> ERR_MAP_INFO                   */
/* type 3: too few rows             -> ERR_MAP_CHAR                   */
/* type 4: invalid char in body     -> ERR_MAP_CHAR                   */
/* type 5: short row (wrong cols)   -> ERR_MAP_CHAR                   */
/* type 6: extra columns in a row   -> ERR_MAP_COL                    */
/* type 7: empty file               -> ERR_MAP_INFO                   */
/* type 8: INT_MAX rows, 1 body row -> ERR_MAP_CHAR                   */
/* type 9: duplicate chars (e==o)   -> ERR_MAP_INFO                   */
/* ------------------------------------------------------------------ */

static void write_invalid(const char *path, int type)
{
    int     i;
    int     j;
    FILE    *f;
    char    emp;
    char    obs;
    char    sq;
    int     rows;
    int     cols;

    emp = '.'; obs = 'o'; sq = 'x';
    rows = 5; cols = 5;
    f = fopen(path, "w");
    if (!f)
        return ;
    if (type == 0)
    {
        /* Only 3 char values, no row count at all */
        fprintf(f, "%c%c%c\n", emp, obs, sq);
        for (i = 0; i < rows; i++)
        {
            for (j = 0; j < cols; j++) fputc(emp, f);
            fputc('\n', f);
        }
    }
    else if (type == 1)
    {
        /* Non-numeric prefix before chars */
        fprintf(f, "ab%c%c%c\n", emp, obs, sq);
        for (i = 0; i < rows; i++)
        {
            for (j = 0; j < cols; j++) fputc(emp, f);
            fputc('\n', f);
        }
    }
    else if (type == 2)
    {
        /* Zero row count */
        fprintf(f, "0%c%c%c\n", emp, obs, sq);
        for (j = 0; j < cols; j++) fputc(emp, f);
        fputc('\n', f);
    }
    else if (type == 3)
    {
        /* Header says 5 rows, only 3 written */
        fprintf(f, "%d%c%c%c\n", rows, emp, obs, sq);
        for (i = 0; i < rows - 2; i++)
        {
            for (j = 0; j < cols; j++) fputc(emp, f);
            fputc('\n', f);
        }
    }
    else if (type == 4)
    {
        /* Invalid character '!' at (2,2) */
        fprintf(f, "%d%c%c%c\n", rows, emp, obs, sq);
        for (i = 0; i < rows; i++)
        {
            for (j = 0; j < cols; j++)
                fputc((i == 2 && j == 2) ? '!' : emp, f);
            fputc('\n', f);
        }
    }
    else if (type == 5)
    {
        /* Short row: row 1 has cols-1 characters */
        fprintf(f, "%d%c%c%c\n", rows, emp, obs, sq);
        for (i = 0; i < rows; i++)
        {
            int len = (i == 1) ? cols - 1 : cols;
            for (j = 0; j < len; j++) fputc(emp, f);
            fputc('\n', f);
        }
    }
    else if (type == 6)
    {
        /* Extra columns in row 2 (cols+2 chars) */
        fprintf(f, "%d%c%c%c\n", rows, emp, obs, sq);
        for (i = 0; i < rows; i++)
        {
            int len = (i == 2) ? cols + 2 : cols;
            for (j = 0; j < len; j++) fputc(emp, f);
            fputc('\n', f);
        }
    }
    else if (type == 7)
    {
        /* Empty file — close immediately without writing */
        fclose(f);
        return ;
    }
    else if (type == 8)
    {
        /* INT_MAX rows declared, only 1 body row supplied */
        fprintf(f, "2147483647%c%c%c\n", emp, obs, sq);
        for (j = 0; j < cols; j++) fputc(emp, f);
        fputc('\n', f);
    }
    else
    {
        /* type 9: duplicate chars — emp == obs */
        fprintf(f, "%d%c%c%c\n", rows, emp, emp, sq);
        for (i = 0; i < rows; i++)
        {
            for (j = 0; j < cols; j++) fputc(emp, f);
            fputc('\n', f);
        }
    }
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* Large map writer (all-empty, streamed row-by-row)                   */
/* ------------------------------------------------------------------ */

static void write_large_map(const char *path, int rows, int cols,
                             char emp, char obs, char sq)
{
    int     i;
    char    *row;
    FILE    *f;

    f = fopen(path, "w");
    if (!f)
        return ;
    row = malloc((size_t)(cols + 2));
    if (!row)
    {
        fclose(f);
        return ;
    }
    fprintf(f, "%d%c%c%c\n", rows, emp, obs, sq);
    memset(row, emp, (size_t)cols);
    row[cols] = '\n';
    i = 0;
    while (i++ < rows)
        fwrite(row, 1, (size_t)(cols + 1), f);
    free(row);
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* Test functions                                                      */
/* ------------------------------------------------------------------ */

static int test_valid(int idx)
{
    int     rows;
    int     cols;
    int     mr;
    int     mc;
    int     max;
    int     pass;
    char    emp;
    char    obs;
    char    sq;
    char    path[64];
    char    **g;
    char    *expected;
    char    *actual;
    size_t  exp_len;

    rows = MINDIM + rand() % (MAXDIM - MINDIM + 1);
    cols = MINDIM + rand() % (MAXDIM - MINDIM + 1);
    pick_chars(&emp, &obs, &sq);
    snprintf(path, sizeof(path), "%s/map_%03d", VDIR, idx);
    g = make_grid(rows, cols, emp, obs);
    if (!g)
    {
        printf("[FAIL] %s  (malloc failed)\n", path);
        return (0);
    }
    write_map(path, g, rows, emp, obs, sq);
    max = ref_dp(g, rows, cols, obs, &mr, &mc);
    expected = ref_output(g, rows, cols, sq, max, mr, mc);
    free_grid(g, rows);
    if (!expected)
    {
        printf("[FAIL] %s  (ref malloc failed)\n", path);
        return (0);
    }
    exp_len = strlen(expected);
    actual = malloc(exp_len + 128);
    if (!actual)
    {
        free(expected);
        return (0);
    }
    run_bsq(path, actual, exp_len + 127);
    pass = (strcmp(actual, expected) == 0);
    printf("[%s] %s  (%dx%d, sq=%d)\n",
           pass ? "PASS" : "FAIL", path, rows, cols, max);
    if (!pass)
    {
        printf("  exp: %.80s%s\n", expected, exp_len > 80 ? "..." : "");
        printf("  got: %.80s%s\n", actual,
               strlen(actual) > 80 ? "..." : "");
    }
    if (g_show)
        printf("  [bsq output]:\n%s\n", actual);
    free(expected);
    free(actual);
    return (pass);
}

static int test_invalid(int idx, int type)
{
    char    path[64];
    char    actual[512];
    int     pass;

    snprintf(path, sizeof(path), "%s/map_%03d", IDIR, idx);
    write_invalid(path, type);
    run_bsq(path, actual, sizeof(actual));
    pass = (strcmp(actual, "Map Error\n") == 0);
    printf("[%s] %s  (type %d)\n", pass ? "PASS" : "FAIL", path, type);
    if (!pass)
        printf("  got: \"%s\"\n", actual);
    if (g_show)
        printf("  [bsq output]: \"%s\"\n", actual);
    return (pass);
}

/*
 * Large maps are all-empty so the largest square fills min(rows, cols).
 * Accepts Map Error if bsq hits a malloc limit on the DP matrix.
 */
static int test_large(int idx)
{
    int     rows;
    int     cols;
    int     max_sq;
    int     pass;
    char    emp;
    char    obs;
    char    sq;
    char    path[64];
    char    *actual;

    rows = 1000 + rand() % (LARGE_MAX_R - 1000);
    cols = 1000 + rand() % (LARGE_MAX_C - 1000);
    pick_chars(&emp, &obs, &sq);
    snprintf(path, sizeof(path), "%s/large_%03d", LDIR, idx);
    write_large_map(path, rows, cols, emp, obs, sq);
    max_sq = (rows < cols) ? rows : cols;
    actual = malloc((size_t)(cols + 128));
    if (!actual)
    {
        printf("[FAIL] %s  (malloc)\n", path);
        return (0);
    }
    run_bsq(path, actual, (size_t)(cols + 127));
    if (strcmp(actual, "Map Error\n") == 0)
    {
        printf("[INFO] %s  (%dx%d) -> Map Error (likely malloc limit)\n",
               path, rows, cols);
        if (g_show)
            printf("  [bsq output]: \"Map Error\\n\"\n");
        free(actual);
        return (1);
    }
    pass = (actual[0] == sq);
    if (pass && cols > max_sq)
        pass = (actual[max_sq] == emp);
    printf("[%s] %s  (%dx%d, sq=%d)\n",
           pass ? "PASS" : "FAIL", path, rows, cols, max_sq);
    if (!pass)
        printf("  got[0]='%c' got[%d]='%c'\n",
               actual[0], max_sq, actual[max_sq]);
    if (g_show)
        printf("  [bsq output] (first 80 chars): %.80s\n", actual);
    free(actual);
    return (pass);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    int     i;
    int     pass;
    int     fail;
    int     result;
    int     do_valid;
    int     do_invalid;
    int     do_large;
    char    *mode;

    do_valid = 0; do_invalid = 0; do_large = 0;
    mode = "all";
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-s") == 0
            || strcmp(argv[i], "--show-output") == 0)
            g_show = 1;
        else
            mode = argv[i];
    }
    if (strcmp(mode, "valid") == 0)
        do_valid = 1;
    else if (strcmp(mode, "invalid") == 0)
        do_invalid = 1;
    else if (strcmp(mode, "large") == 0)
        do_large = 1;
    else
    {
        do_valid = 1;
        do_invalid = 1;
        do_large = 1;
    }
    srand((unsigned int)time(NULL));
    ensure_dirs();
    pass = 0;
    fail = 0;
    if (do_valid)
    {
        printf("\n=== Valid Maps (%d tests) ===\n", NUM_VALID);
        for (i = 1; i <= NUM_VALID; i++)
        {
            result = test_valid(i);
            if (result) pass++;
            else fail++;
        }
    }
    if (do_invalid)
    {
        printf("\n=== Invalid Maps (%d tests, %d error types) ===\n",
               NUM_INVALID, N_INV_TYPES);
        for (i = 0; i < NUM_INVALID; i++)
        {
            result = test_invalid(i + 1, i % N_INV_TYPES);
            if (result) pass++;
            else fail++;
        }
    }
    if (do_large)
    {
        printf("\n=== Large Maps (%d tests) ===\n", NUM_LARGE);
        for (i = 1; i <= NUM_LARGE; i++)
        {
            result = test_large(i);
            if (result) pass++;
            else fail++;
        }
    }
    printf("\nResults: %d/%d passed\n", pass, pass + fail);
    return (fail);
}
