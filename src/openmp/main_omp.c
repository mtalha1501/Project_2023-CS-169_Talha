/*
 * main_omp.c  –  LogForge OpenMP Version
 * Parallel Aho-Corasick scan with chunk partitioning.
 * Usage: ./omp <log_file> <num_threads>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "../aho_corasick.h"

static const char *PATTERNS[] = {
    "ERROR", "WARNING", "CRITICAL", "FAILED LOGIN", "ATTACK"
};
#define NP (int)(sizeof(PATTERNS)/sizeof(PATTERNS[0]))

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
    omp_set_num_threads(T);

    /* Build automaton once — read-only, shared safely across threads */
    AhoCorasick ac;
    ac_compile(&ac, PATTERNS, NP);
    int overlap = 12; /* max_pattern_len - 1 */

    long len;
    char *text = read_file(argv[1], &len);

    /* Line-aligned chunk boundaries */
    long *starts = calloc(T + 1, sizeof(long));
    starts[0] = 0;
    for (int i = 1; i < T; i++)
        starts[i] = next_line(text, (long)i * (len / T), len);
    starts[T] = len;

    MatchResult *local = calloc(T, sizeof(MatchResult));

    double t0 = omp_get_wtime();

    #pragma omp parallel num_threads(T)
    {
        int tid = omp_get_thread_num();
        long ps = starts[tid];
        long pe = starts[tid + 1];
        long se = pe + overlap; if (se > len) se = len;

        /* Scan primary + overlap */
        ac_scan(&ac, text + ps, se - ps, &local[tid], 0);

        /* Subtract overlap-only to avoid double-counting */
        if (se > pe) {
            MatchResult ov = {0};
            ac_scan(&ac, text + pe, se - pe, &ov, 0);
            for (int p = 0; p < MAX_PATTERNS; p++)
                local[tid].counts[p] -= ov.counts[p];
            local[tid].total_matches -= ov.total_matches;
        }
    }

    double elapsed = omp_get_wtime() - t0;

    /* Merge */
    MatchResult global = {0};
    for (int i = 0; i < T; i++) {
        for (int p = 0; p < MAX_PATTERNS; p++)
            global.counts[p] += local[i].counts[p];
        global.total_matches += local[i].total_matches;
    }

    print_results(PATTERNS, NP, &global);
    printf("Algorithm       : OpenMP (Aho-Corasick)\n");
    printf("Threads/Procs   : %d\n", T);
    printf("File size       : %ld bytes\n", len);
    printf("Scan time (s)   : %.6f\n", elapsed);

    free(text); free(starts); free(local);
    return 0;
}
