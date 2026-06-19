/*
 * main_pth.c  –  LogForge Pthreads Version
 * Each thread scans its own chunk. Results merged after join.
 * Usage: ./pth <log_file> <num_threads>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "../aho_corasick.h"

static const char *PATTERNS[] = {
    "ERROR", "WARNING", "CRITICAL", "FAILED LOGIN", "ATTACK"
};
#define NP (int)(sizeof(PATTERNS)/sizeof(PATTERNS[0]))

/* ── Shared automaton (read-only after build → zero contention) ── */
static AhoCorasick g_ac;

typedef struct {
    const char *text;
    long        primary_start;
    long        primary_end;
    long        scan_end;   /* primary_end + overlap */
    MatchResult result;
} ThreadArg;

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static char *read_file(const char *path, long *len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); exit(1); }
    fseek(f, 0, SEEK_END); *len = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(*len + 1);
    if (!buf) { fputs("malloc\n", stderr); exit(1); }
    *len = (long)fread(buf, 1, *len, f);
    buf[*len] = '\0'; fclose(f);
    return buf;
}

static long next_line(const char *buf, long pos, long len) {
    while (pos < len && buf[pos] != '\n') pos++;
    return (pos < len) ? pos + 1 : len;
}

/* ── Thread worker ─────────────────────────────────────────────
 *  Scans (primary + overlap) then subtracts overlap-only matches
 *  so each pattern is counted exactly once across all threads.
 * ──────────────────────────────────────────────────────────── */
static void *worker(void *arg) {
    ThreadArg *a = (ThreadArg *)arg;
    long scan_len = a->scan_end - a->primary_start;

    ac_scan(&g_ac, a->text + a->primary_start, scan_len, &a->result, 0);

    /* Subtract overlap-only region to avoid double-counting */
    if (a->scan_end > a->primary_end) {
        MatchResult ov = {0};
        long ov_len = a->scan_end - a->primary_end;
        ac_scan(&g_ac, a->text + a->primary_end, ov_len, &ov, 0);
        for (int i = 0; i < MAX_PATTERNS; i++)
            a->result.counts[i] -= ov.counts[i];
        a->result.total_matches -= ov.total_matches;
    }
    return NULL;
}

void print_results(const char *pats[], int n, const MatchResult *r) {
    printf("=== LogForge Pattern Match Results ===\n");
    for (int i = 0; i < n; i++)
        printf("%-14s : %ld\n", pats[i], r->counts[i]);
    printf("%-14s : %ld\n", "TOTAL", r->total_matches);
    printf("===\n");
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <log_file> <threads>\n", argv[0]); return 1;
    }
    int T = atoi(argv[2]);
    if (T < 1) T = 1;

    /* Build shared automaton (sequential, one-time) */
    ac_compile(&g_ac, PATTERNS, NP);
    int overlap = 12; /* len("FAILED LOGIN") - 1 = 12 */

    long len;
    char *text = read_file(argv[1], &len);

    /* Compute line-aligned chunk boundaries */
    long *starts = calloc(T + 1, sizeof(long));
    starts[0] = 0;
    for (int i = 1; i < T; i++)
        starts[i] = next_line(text, (long)i * (len / T), len);
    starts[T] = len;

    ThreadArg *args = calloc(T, sizeof(ThreadArg));
    pthread_t  *tids = calloc(T, sizeof(pthread_t));

    double t0 = now();

    for (int i = 0; i < T; i++) {
        args[i].text          = text;
        args[i].primary_start = starts[i];
        args[i].primary_end   = starts[i + 1];
        args[i].scan_end      = starts[i + 1] + overlap;
        if (args[i].scan_end > len) args[i].scan_end = len;
        pthread_create(&tids[i], NULL, worker, &args[i]);
    }

    for (int i = 0; i < T; i++)
        pthread_join(tids[i], NULL);

    double elapsed = now() - t0;

    /* Merge results */
    MatchResult global = {0};
    for (int i = 0; i < T; i++) {
        for (int p = 0; p < MAX_PATTERNS; p++)
            global.counts[p] += args[i].result.counts[p];
        global.total_matches += args[i].result.total_matches;
    }

    print_results(PATTERNS, NP, &global);
    printf("Algorithm       : Pthreads (Aho-Corasick)\n");
    printf("Threads/Procs   : %d\n", T);
    printf("File size       : %ld bytes\n", len);
    printf("Scan time (s)   : %.6f\n", elapsed);

    free(text); free(starts); free(args); free(tids);
    return 0;
}
