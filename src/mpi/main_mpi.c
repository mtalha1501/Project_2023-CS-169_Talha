/*
 * main_mpi.c  –  LogForge MPI Version
 * Rank 0 reads file, Scatterv distributes byte chunks,
 * each process scans independently, Reduce combines counts.
 * Usage: mpirun -np <P> ./mpi <log_file>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "../aho_corasick.h"

static const char *PATTERNS[] = {
    "ERROR", "WARNING", "CRITICAL", "FAILED LOGIN", "ATTACK"
};
#define NP (int)(sizeof(PATTERNS)/sizeof(PATTERNS[0]))

static long next_line(const char *buf, long pos, long len) {
    while (pos < len && buf[pos] != '\n') pos++;
    return (pos < len) ? pos + 1 : len;
}

void print_results(const char *pats[], int n, const long *counts, long total) {
    printf("=== LogForge Pattern Match Results ===\n");
    for (int i = 0; i < n; i++)
        printf("%-14s : %ld\n", pats[i], counts[i]);
    printf("%-14s : %ld\n", "TOTAL", total);
    printf("===\n");
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 2) {
        if (rank == 0)
            fprintf(stderr, "Usage: mpirun -np <P> %s <log_file>\n", argv[0]);
        MPI_Finalize(); return 1;
    }

    /* All processes build the automaton independently
     * (deterministic construction, negligible cost) */
    AhoCorasick ac;
    ac_compile(&ac, PATTERNS, NP);
    int overlap = 12; /* max_pattern_len - 1 */

    char  *text      = NULL;
    long   total_len = 0;
    int   *sendcounts = calloc(size, sizeof(int));
    int   *displs     = calloc(size, sizeof(int));

    /* Rank 0: read file and compute chunk boundaries */
    if (rank == 0) {
        FILE *f = fopen(argv[1], "rb");
        if (!f) { perror("fopen"); MPI_Abort(MPI_COMM_WORLD, 1); }
        fseek(f, 0, SEEK_END); total_len = ftell(f); fseek(f, 0, SEEK_SET);
        text = malloc(total_len + 1);
        total_len = (long)fread(text, 1, total_len, f);
        text[total_len] = '\0'; fclose(f);

        /* Line-aligned boundaries */
        long *starts = calloc(size + 1, sizeof(long));
        starts[0] = 0;
        for (int i = 1; i < size; i++)
            starts[i] = next_line(text, (long)i * (total_len / size), total_len);
        starts[size] = total_len;

        for (int i = 0; i < size; i++) {
            long pe = starts[i + 1];
            long se = pe + overlap; if (se > total_len) se = total_len;
            displs[i]     = (int)starts[i];
            sendcounts[i] = (int)(se - starts[i]); /* primary + overlap */
        }
        free(starts);
    }

    /* Broadcast metadata */
    MPI_Bcast(&total_len,  1,    MPI_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(sendcounts,  size, MPI_INT,  0, MPI_COMM_WORLD);
    MPI_Bcast(displs,      size, MPI_INT,  0, MPI_COMM_WORLD);

    /* Allocate local buffer and scatter */
    int local_len = sendcounts[rank];
    char *local_buf = malloc(local_len + 1);
    MPI_Scatterv(text, sendcounts, displs, MPI_CHAR,
                 local_buf, local_len, MPI_CHAR, 0, MPI_COMM_WORLD);
    local_buf[local_len] = '\0';

    /* Compute primary region length for this rank */
    int primary_len = local_len;
    if (rank < size - 1) {
        /* primary_len = sendcounts[rank] - overlap bytes */
        int next_start = displs[rank] + (displs[rank + 1] - displs[rank]);
        /* Simpler: primary = next rank's displs - my displs */
        int my_primary = displs[rank + 1] - displs[rank];
        primary_len = my_primary;
    }

    /* Scan full local buffer */
    MatchResult local_res = {0};
    double t0 = MPI_Wtime();
    ac_scan(&ac, local_buf, local_len, &local_res, 0);

    /* Subtract overlap-only region */
    if (primary_len < local_len) {
        MatchResult ov = {0};
        ac_scan(&ac, local_buf + primary_len, local_len - primary_len, &ov, 0);
        for (int i = 0; i < MAX_PATTERNS; i++)
            local_res.counts[i] -= ov.counts[i];
        local_res.total_matches -= ov.total_matches;
    }
    double elapsed = MPI_Wtime() - t0;

    /* Reduce to rank 0 */
    long global_counts[MAX_PATTERNS] = {0};
    long global_total = 0;
    MPI_Reduce(local_res.counts, global_counts, MAX_PATTERNS,
               MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_res.total_matches, &global_total, 1,
               MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    /* Max scan time across all ranks */
    double max_elapsed;
    MPI_Reduce(&elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        print_results(PATTERNS, NP, global_counts, global_total);
        printf("Algorithm       : MPI (Aho-Corasick)\n");
        printf("Threads/Procs   : %d\n", size);
        printf("File size       : %ld bytes\n", total_len);
        printf("Scan time (s)   : %.6f\n", max_elapsed);
        free(text);
    }

    free(local_buf); free(sendcounts); free(displs);
    MPI_Finalize();
    return 0;
}
