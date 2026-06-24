#!/bin/bash
# ================================================================
#  run_experiments.sh  –  LogForge Complete Experiment Runner
#  Compiles all 4 versions, generates data, runs all experiments,
#  saves outputs to results/raw_outputs/
#
#  Usage: ./scripts/run_experiments.sh
#  Must be run from project root: Project_2023CS169_Talha/
# ================================================================

set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/src"
DATA="$ROOT/data"
RES="$ROOT/results/raw_outputs"

mkdir -p "$RES" "$DATA"
cd "$ROOT"

echo "================================================================"
echo "  LogForge – Parallel Aho-Corasick Log Analytics Engine"
echo "  Student: Muhammad Talha  |  2023-CS-169"
echo "================================================================"
echo ""

# ── Step 1: Compile all versions ─────────────────────────────────
echo "[1/5] Compiling..."

gcc -O2 -o "$DATA/gen" "$DATA/input_generator.c"
echo "  OK: input_generator"

gcc -O2 -o "$SRC/sequential/seq" \
    "$SRC/sequential/main_seq.c"
echo "  OK: sequential"

gcc -O2 -pthread -o "$SRC/pthreads/pth" \
    "$SRC/pthreads/main_pth.c"
echo "  OK: pthreads"

gcc -O2 -fopenmp -o "$SRC/openmp/omp" \
    "$SRC/openmp/main_omp.c"
echo "  OK: openmp"

mpicc -O2 -o "$SRC/mpi/mpi_bin" \
    "$SRC/mpi/main_mpi.c"
echo "  OK: mpi"
echo ""

# ── Step 2: Generate datasets ─────────────────────────────────────
echo "[2/5] Generating datasets..."
SIZES=(100000 500000 1000000 5000000)
for N in "${SIZES[@]}"; do
    F="$DATA/log_${N}.txt"
    [ -f "$F" ] || "$DATA/gen" "$N" "$F"
    echo "  log_${N}.txt ready"
done
echo ""

# ── Step 3: Correctness check ──────────────────────────────────────
echo "[3/5] Correctness check (1M lines, comparing all 4 versions)..."
F1M="$DATA/log_1000000.txt"

SEQ_OUT=$("$SRC/sequential/seq" "$F1M" | grep -E "^(ERROR|WARNING|CRITICAL|FAILED|ATTACK|TOTAL)")
PTH_OUT=$("$SRC/pthreads/pth"   "$F1M" 4 | grep -E "^(ERROR|WARNING|CRITICAL|FAILED|ATTACK|TOTAL)")
OMP_OUT=$("$SRC/openmp/omp"     "$F1M" 4 | grep -E "^(ERROR|WARNING|CRITICAL|FAILED|ATTACK|TOTAL)")
MPI_OUT=$(mpirun -np 4 "$SRC/mpi/mpi_bin" "$F1M" | grep -E "^(ERROR|WARNING|CRITICAL|FAILED|ATTACK|TOTAL)")

if [ "$SEQ_OUT" = "$PTH_OUT" ] && [ "$SEQ_OUT" = "$OMP_OUT" ] && [ "$SEQ_OUT" = "$MPI_OUT" ]; then
    echo "  PASS: All 4 versions produce identical pattern counts"
else
    echo "  WARN: Outputs differ – check raw files for details"
fi
echo ""

# ── Step 4: Performance experiments ───────────────────────────────
echo "[4/5] Running performance experiments..."
THREADS=(1 2 4 8)

for N in "${SIZES[@]}"; do
    F="$DATA/log_${N}.txt"
    OUT="$RES/results_${N}.txt"

    echo "=== Dataset: ${N} lines ===" | tee "$OUT"

    # Sequential
    echo -n "  Sequential  ... "
    "$SRC/sequential/seq" "$F" >> "$OUT" 2>&1
    grep "Scan time" "$OUT" | tail -1

    # Pthreads
    for T in "${THREADS[@]}"; do
        echo -n "  Pthreads T=$T ... "
        echo "--- Pthreads T=$T ---" >> "$OUT"
        "$SRC/pthreads/pth" "$F" "$T" >> "$OUT" 2>&1
        grep "Scan time" "$OUT" | tail -1
    done

    # OpenMP
    for T in "${THREADS[@]}"; do
        echo -n "  OpenMP   T=$T ... "
        echo "--- OpenMP T=$T ---" >> "$OUT"
        "$SRC/openmp/omp" "$F" "$T" >> "$OUT" 2>&1
        grep "Scan time" "$OUT" | tail -1
    done

    # MPI
    for T in "${THREADS[@]}"; do
        echo -n "  MPI      P=$T ... "
        echo "--- MPI P=$T ---" >> "$OUT"
        mpirun --oversubscribe -np "$T" "$SRC/mpi/mpi_bin" "$F" >> "$OUT" 2>&1
        grep "Scan time" "$OUT" | tail -1
    done

    echo ""
done

# ── Step 5: Speedup summary table ─────────────────────────────────
echo "[5/5] Speedup Summary (1M lines)"
echo ""
printf "%-12s %-10s %-12s %-10s\n" "Algorithm" "Threads" "Scan(s)" "Speedup"
printf "%-12s %-10s %-12s %-10s\n" "---------" "-------" "-------" "-------"

F="$DATA/log_1000000.txt"
TBASE=$("$SRC/sequential/seq" "$F" 2>/dev/null | grep "Scan time" | awk '{print $NF}')
printf "%-12s %-10s %-12s %-10s\n" "Sequential" "1" "$TBASE" "1.00x"

for T in "${THREADS[@]}"; do
    PT=$("$SRC/pthreads/pth" "$F" "$T" 2>/dev/null | grep "Scan time" | awk '{print $NF}')
    SPD=$(awk "BEGIN{printf \"%.2fx\", $TBASE/$PT}" 2>/dev/null || echo "n/a")
    printf "%-12s %-10s %-12s %-10s\n" "Pthreads" "$T" "$PT" "$SPD"
done
for T in "${THREADS[@]}"; do
    OT=$("$SRC/openmp/omp" "$F" "$T" 2>/dev/null | grep "Scan time" | awk '{print $NF}')
    SPD=$(awk "BEGIN{printf \"%.2fx\", $TBASE/$OT}" 2>/dev/null || echo "n/a")
    printf "%-12s %-10s %-12s %-10s\n" "OpenMP" "$T" "$OT" "$SPD"
done
for T in "${THREADS[@]}"; do
    MT=$(mpirun --oversubscribe -np "$T" "$SRC/mpi/mpi_bin" "$F" 2>/dev/null | grep "Scan time" | awk '{print $NF}')
    SPD=$(awk "BEGIN{printf \"%.2fx\", $TBASE/$MT}" 2>/dev/null || echo "n/a")
    printf "%-12s %-10s %-12s %-10s\n" "MPI" "$T" "$MT" "$SPD"
done

echo ""
echo "Raw outputs saved to: $RES"
echo "================================================================"
echo "  Experiments complete."
echo "================================================================"
