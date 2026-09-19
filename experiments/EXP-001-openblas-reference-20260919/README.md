# EXP-001: OpenBLAS reference replacement and remote hardware probe

- Date: 2026-09-19
- Remote path: `/home/nvidia/Ziyoung/SRQ-26`
- Commit: `3dfb264af1316421b1cc3b7a05318c997bf325c1`
- Host: Intel Xeon Gold 6548Y+, 2 sockets, 2 NUMA nodes, 128 logical CPUs
- Resource cap: `taskset -c 0-31`
- OpenMP: `OMP_NUM_THREADS=32`, `OMP_DYNAMIC=false`, `OMP_PROC_BIND=close`, `OMP_PLACES=cores`
- Seed: `SRQ_SEED=260919`
- Reference library: OpenBLAS 0.3.28, user-local at `/home/nvidia/Ziyoung/deps/openblas`
- Build: GCC 13.3, `-O3 -std=c99 -fopenmp`, OpenBLAS `DYNAMIC_ARCH=1 NO_AFFINITY=1 USE_OPENMP=0 NUM_THREADS=32`

## Reference replacement

`src/main.c:multiply_reference()` now calls row-major, no-transpose CBLAS `dgemm` from `task/blas_reference.c`. The previous scalar reference loop is no longer used.

## Fixed-seed correctness

Command:

```sh
SRQ_SEED=260919 OPENBLAS_ROOT=/home/nvidia/Ziyoung/deps/openblas taskset -c 0-31 ./run.sh test
```

Result: `Correctness test passed!` for `302 x 240 * 240 x 494`.

## Official benchmark

Five runs used the same seed and CPU/OpenMP settings before replacing the slow native reference timing. The table reports medians. `Task baseline` is now fixed for subsequent optimization work; `OpenBLAS reference` is only the correctness oracle and is excluded from score timing.

| Case | OpenBLAS oracle seconds (not scored) | Fixed native Task baseline seconds | Later score formula | End-to-end correctness |
|---|---:|---:|---:|---|
| case 1 | 0.012809 | 1.167841 | `1.167841 / candidate` | passed in all runs |
| case 2 | 0.035183 | 2.281671 | `2.281671 / candidate` | passed in all runs |
| case 3 | 0.667933 | 56.250248 | `56.250248 / candidate` | passed in all runs |
| case 4 | 0.376709 | 0.687851 | `0.687851 / candidate` | passed in all runs |
| Weighted `2:2:2:4` | | **fixed in `baseline.tsv`** | weighted fixed-baseline / candidate | all passed |

The fixed comparison data is versioned in `baseline.tsv`. It contains the five-run medians for the original native Task baseline and the non-scored OpenBLAS oracle. The benchmark executable now uses the fixed native values for score calculation; OpenBLAS is called only before timing to produce the correctness reference. The raw five-run output was retained on the remote node at `~/Ziyoung/experiments/EXP-001-openblas-reference-20260919/benchmark-runs.txt`. The updated harness excludes OpenBLAS from timing and compares the timed Task candidate against the fixed native baseline. A verification run at commit `3aaaa6f` produced case speedups `0.691x`, `0.720x`, `0.978x`, `0.991x`, with weighted score `0.874x`; all four correctness checks passed.

## Hardware microbench

Command: `taskset -c 0-31 ./scripts/microbench.sh`

- `memcpy_gib_s=8.850`
- `omp_threads=32`
- `matmul_gflop_s=63.799`
- `checksum=0.008192`

These are exploratory probes, not the official score.
