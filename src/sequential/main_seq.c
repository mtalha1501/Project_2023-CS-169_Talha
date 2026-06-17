/*
 * main_seq.c  –  LogForge Sequential Baseline
 * Aho-Corasick multi-pattern log file scanner, single thread.
 * Usage: ./seq <log_file>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../aho_corasick.h"

static const char *PATTERNS[] = {
    "ERROR", "WARNING", "CRITICAL", "FAILED LOGIN", "ATTACK"
};
#define NP (int)(sizeof(PATTERNS)/sizeof(PATTERNS[0]))

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

void print_results(const char *pats[], int n, const MatchResult *r) {
    printf("=== LogForge Pattern Match Results ===\n");
    for (int i = 0; i < n; i++)
        printf("%-14s : %ld\n", pats[i], r->counts[i]);
    printf("%-14s : %ld\n", "TOTAL", r->total_matches);
    printf("===\n");
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "Usage: %s <log_file>\n", argv[0]); return 1; }

    /* Build automaton */
    AhoCorasick ac;
    ac_compile(&ac, PATTERNS, NP);

    /* Load file */
    long len;
    char *text = read_file(argv[1], &len);

    /* Sequential scan */
    MatchResult result = {0};
    double t0 = now();
    ac_scan(&ac, text, len, &result, 0);
    double elapsed = now() - t0;

    print_results(PATTERNS, NP, &result);
    printf("Algorithm       : Sequential (Aho-Corasick)\n");
    printf("Threads/Procs   : 1\n");
    printf("File size       : %ld bytes\n", len);
    printf("Scan time (s)   : %.6f\n", elapsed);

    free(text);
    return 0;
}
