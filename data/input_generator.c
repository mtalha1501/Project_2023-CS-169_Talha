/*
 * input_generator.c  –  LogForge synthetic log generator
 * Usage: ./gen <num_lines> <output_file>
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct { const char *level, *module, *msg; int w; } T;
static T tpls[] = {
    {"INFO",     "web_server",  "GET /api/users 200 OK",                   40},
    {"INFO",     "auth_service","User session refreshed",                   25},
    {"WARNING",  "db_pool",     "Connection pool 80% full",                 12},
    {"WARNING",  "cache_layer", "Cache miss rate above threshold",           8},
    {"ERROR",    "auth_service","FAILED LOGIN for user admin",               7},
    {"ERROR",    "payment_gw",  "Transaction declined",                      4},
    {"CRITICAL", "scheduler",   "Task queue overflow detected",              2},
    {"CRITICAL", "network",     "Possible ATTACK pattern detected",          1},
    {"ERROR",    "firewall",    "Multiple FAILED LOGIN attempts blocked",     1},
};
#define NT (int)(sizeof(tpls)/sizeof(tpls[0]))

int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr,"Usage: %s <lines> <file>\n",argv[0]); return 1; }
    long N = atol(argv[1]);
    FILE *f = fopen(argv[2], "w");
    if (!f) { perror("fopen"); return 1; }
    unsigned int seed = 42;
    time_t base = 1700000000;
    for (long i = 0; i < N; i++) {
        int r = rand_r(&seed) % 100, acc = 0, j;
        for (j = 0; j < NT-1; j++) { acc += tpls[j].w; if (r < acc) break; }
        struct tm tm; time_t ts = base + i; gmtime_r(&ts, &tm);
        char tb[32]; strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", &tm);
        fprintf(f, "[%s] %-8s %-14s %s\n", tb, tpls[j].level, tpls[j].module, tpls[j].msg);
    }
    fclose(f);
    printf("Generated %ld lines -> %s\n", N, argv[2]);
    return 0;
}
