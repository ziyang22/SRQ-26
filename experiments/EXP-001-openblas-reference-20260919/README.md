# EXP-001: OpenBLAS reference replacement and remote hardware probe

- Date: 2026-09-19
- Remote path: `/home/nvidia/Ziyoung/SRQ-26`
- Commit: `8d84c7110a86507fb74ac49ba668d7efb34c0d0a`
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

| Case | Baseline seconds | Task seconds | Speedup | End-to-end correctness |
|---|---:|---:|---:|---|
| case 1 | 0.015218 | 1.206934 | 0.013x | passed |
| case 2 | 0.034830 | 2.347981 | 0.015x | passed |
| case 3 | 0.669282 | 56.336860 | 0.012x | passed |
| case 4 | 0.376321 | 0.682705 | 0.551x | passed |
| Weighted `2:2:2:4` | | | **0.228x** | all passed |

## Hardware microbench

Command: `taskset -c 0-31 ./scripts/microbench.sh`

- `memcpy_gib_s=8.850`
- `omp_threads=32`
- `matmul_gflop_s=63.799`
- `checksum=0.008192`

These are exploratory probes, not the official score.
