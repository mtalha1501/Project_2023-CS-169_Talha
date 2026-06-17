# LogForge: Hybrid Multi-Pattern Log Analytics Engine
## Parallel Aho-Corasick Pattern Detection System

**Student:** Muhammad Talha  
**Roll No:** 2023-CS-169  
**Course:** CS-311 Parallel & Distributed Computing  
**Instructor:** Prof. Waqas Ali Zafar  

---

## Problem

Large-scale log file analysis using the **Aho-Corasick** multi-pattern string matching algorithm, parallelised across three models: Pthreads, OpenMP, and MPI.

Sequential complexity: **O(L + Z)** — L = file bytes, Z = total matches.  
All three parallel versions achieve identical pattern match counts (correctness verified).

---

## Directory Structure

```
Project_2023CS169_Talha/
├── docs/
│   ├── paper.pdf          ← IEEE-format research paper
│   └── appendices.pdf     ← Gantt Chart (A) + Logbook (B)
├── src/
│   ├── aho_corasick.h     ← shared Aho-Corasick automaton (trie + BFS failure links)
│   ├── sequential/main_seq.c
│   ├── pthreads/main_pth.c
│   ├── openmp/main_omp.c
│   └── mpi/main_mpi.c
├── data/
│   └── input_generator.c  ← synthetic log generator
├── scripts/
│   └── run_experiments.sh ← compiles ALL versions + runs ALL experiments
├── results/
│   └── raw_outputs/       ← populated by run_experiments.sh
└── README.md
```

---

## Dependencies

| Tool | Version |
|---|---|
| GCC | ≥ 7.0 |
| OpenMPI | ≥ 4.0 |
| OpenMP | ≥ 4.5 (included in GCC ≥ 6) |
| Pthreads | POSIX standard (Linux) |
| OS | Ubuntu 20.04+ |

Install:
```bash
sudo apt update
sudo apt install gcc build-essential mpich libopenmpi-dev
```

---

## Compilation

```bash
# Input generator
gcc -O2 -o data/gen data/input_generator.c

# Sequential
gcc -O2 -o src/sequential/seq src/sequential/main_seq.c

# Pthreads
gcc -O2 -pthread -o src/pthreads/pth src/pthreads/main_pth.c

# OpenMP
gcc -O2 -fopenmp -o src/openmp/omp src/openmp/main_omp.c

# MPI
mpicc -O2 -o src/mpi/mpi_bin src/mpi/main_mpi.c
```

---

## Run Commands

```bash
# Generate test data
./data/gen 1000000 data/log_1m.txt

# Sequential
./src/sequential/seq data/log_1m.txt

# Pthreads (4 threads)
./src/pthreads/pth data/log_1m.txt 4

# OpenMP (4 threads)
./src/openmp/omp data/log_1m.txt 4

# MPI (4 processes)
mpirun --oversubscribe -np 4 ./src/mpi/mpi_bin data/log_1m.txt
```

---

## Full Experiment (One Command)

```bash
chmod +x scripts/run_experiments.sh
./scripts/run_experiments.sh
```

This will: compile all versions → generate all datasets → run correctness check → run all benchmarks → print speedup table → save raw outputs to `results/raw_outputs/`.

---

## Patterns Detected

| Pattern | Description |
|---|---|
| `ERROR` | Application errors |
| `WARNING` | Warning events |
| `CRITICAL` | Critical failures |
| `FAILED LOGIN` | Authentication failures |
| `ATTACK` | Intrusion/attack indicators |

---

## Expected Output Format (identical across all 4 versions)

```
=== LogForge Pattern Match Results ===
ERROR          : 120123
WARNING        : 210456
CRITICAL       : 32012
FAILED LOGIN   : 83045
ATTACK         : 14003
TOTAL          : 459639
===
Algorithm       : OpenMP (Aho-Corasick)
Threads/Procs   : 4
File size       : 71234567 bytes
Scan time (s)   : 0.234567
```
