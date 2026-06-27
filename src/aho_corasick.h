/*
 * aho_corasick.h  –  LogForge Shared Aho-Corasick Engine
 *
 * Provides:
 *   AhoCorasick   – trie + BFS failure links (automaton)
 *   MatchResult   – per-pattern counts + total
 *   ac_compile()  – build automaton from pattern array
 *   ac_scan()     – scan a byte buffer; accumulate into MatchResult
 *
 * Thread-safety: ac_scan() is read-only on AhoCorasick after ac_compile().
 * Multiple threads may call ac_scan() concurrently with no locking.
 *
 * Usage:
 *   #include "../aho_corasick.h"
 *   AhoCorasick ac;
 *   ac_compile(&ac, patterns, n);
 *   MatchResult r = {0};
 *   ac_scan(&ac, text, len, &r, 0);
 */

#ifndef AHO_CORASICK_H
#define AHO_CORASICK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Limits ──────────────────────────────────────────────────────── */
#define MAX_PATTERNS   16
#define MAX_PAT_LEN    64
#define ALPHA          128   /* ASCII printable range */
#define MAX_STATES     (MAX_PATTERNS * MAX_PAT_LEN + 1)

/* ── Data structures ─────────────────────────────────────────────── */

typedef struct {
    int  go[MAX_STATES][ALPHA];  /* goto table                        */
    int  fail[MAX_STATES];       /* failure links (BFS)               */
    int  out[MAX_STATES];        /* bitmask: which patterns end here  */
    int  pat_idx[MAX_STATES];    /* first pattern ending at state     */
    int  num_states;
    int  np;                     /* number of patterns                */
    int  pat_len[MAX_PATTERNS];  /* length of each pattern            */
} AhoCorasick;

typedef struct {
    long counts[MAX_PATTERNS];   /* per-pattern match count           */
    long total_matches;
} MatchResult;

/* ── ac_compile ──────────────────────────────────────────────────── */
static inline void ac_compile(AhoCorasick *ac,
                               const char  *pats[],
                               int          np)
{
    int s, i, c;
    ac->np          = np;
    ac->num_states  = 1;

    /* Initialise all goto transitions to -1 (undefined) */
    memset(ac->go,  -1, sizeof(ac->go));
    memset(ac->out,  0, sizeof(ac->out));
    memset(ac->fail, 0, sizeof(ac->fail));

    /* Build goto function (trie insertion) */
    for (i = 0; i < np; i++) {
        int cur = 0;
        int len = (int)strlen(pats[i]);
        ac->pat_len[i] = len;
        for (int j = 0; j < len; j++) {
            c = (unsigned char)pats[i][j];
            if (ac->go[cur][c] == -1) {
                ac->go[cur][c] = ac->num_states++;
            }
            cur = ac->go[cur][c];
        }
        ac->out[cur] |= (1 << i);
    }

    /* Root: undefined transitions loop back to root */
    for (c = 0; c < ALPHA; c++)
        if (ac->go[0][c] == -1)
            ac->go[0][c] = 0;

    /* BFS to fill failure links and propagate output */
    int queue[MAX_STATES];
    int head = 0, tail = 0;

    /* Enqueue depth-1 states */
    for (c = 0; c < ALPHA; c++) {
        s = ac->go[0][c];
        if (s != 0) {
            ac->fail[s] = 0;
            queue[tail++] = s;
        }
    }

    while (head < tail) {
        int r = queue[head++];
        for (c = 0; c < ALPHA; c++) {
            int u = ac->go[r][c];
            if (u == -1) {
                /* Redirect undefined transitions via failure */
                ac->go[r][c] = ac->go[ac->fail[r]][c];
            } else {
                ac->fail[u] = ac->go[ac->fail[r]][c];
                ac->out[u] |= ac->out[ac->fail[u]]; /* propagate output */
                queue[tail++] = u;
            }
        }
    }
}

/* ── ac_scan ─────────────────────────────────────────────────────── */
/*
 * Scan `len` bytes starting at `text`.
 * `start_state`: pass 0 for a fresh scan.
 * Results are *accumulated* into `res` (caller must zero-initialise).
 * Returns the automaton state at end of buffer (for chaining chunks).
 */
static inline int ac_scan(const AhoCorasick *ac,
                           const char        *text,
                           long               len,
                           MatchResult       *res,
                           int                start_state)
{
    int state = start_state;
    for (long i = 0; i < len; i++) {
        int c = (unsigned char)text[i];
        if (c >= ALPHA) c = 0;          /* ignore non-ASCII bytes    */
        state = ac->go[state][c];
        if (ac->out[state]) {
            int mask = ac->out[state];
            while (mask) {
                int p = __builtin_ctz(mask); /* index of lowest set bit */
                res->counts[p]++;
                res->total_matches++;
                mask &= mask - 1;
            }
        }
    }
    return state;
}

#endif /* AHO_CORASICK_H */
