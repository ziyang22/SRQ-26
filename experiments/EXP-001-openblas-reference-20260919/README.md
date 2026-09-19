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

The original native Task baseline was measured five times and fixed in `baseline.tsv`. After replacing the slow correctness GEMM with OpenBLAS, the current candidate was sampled twice only (the requested early-stage sampling policy). `Task baseline` remains fixed for subsequent optimization work; `OpenBLAS reference` is only the correctness oracle and is excluded from score timing.

| Case | OpenBLAS oracle seconds (not scored) | Fixed native Task baseline seconds | Later score formula | End-to-end correctness |
|---|---:|---:|---:|---|
| case 1 | 0.012809 | 1.167841 | `1.167841 / candidate` | passed in all runs |
| case 2 | 0.035183 | 2.281671 | `2.281671 / candidate` | passed in all runs |
| case 3 | 0.667933 | 56.250248 | `56.250248 / candidate` | passed in all runs |
| case 4 | 0.376709 | 0.687851 | `0.687851 / candidate` | passed in all runs |
| Weighted `2:2:2:4` | | **fixed in `baseline.tsv`** | weighted fixed-baseline / candidate | all passed |

The fixed comparison data is versioned in `baseline.tsv`. It contains the five-run medians for the original native Task baseline and the non-scored OpenBLAS oracle. The benchmark executable now uses the fixed native values for score calculation; OpenBLAS is called only before timing to produce the correctness reference. The raw five-run output was retained on the remote node at `~/Ziyoung/experiments/EXP-001-openblas-reference-20260919/benchmark-runs.txt`. The updated harness excludes OpenBLAS from timing and compares the timed Task candidate against the fixed native baseline. Two early verification runs at commit `3aaaa6f` produced:

| Run | case 1 | case 2 | case 3 | case 4 | weighted |
|---|---:|---:|---:|---:|---:|
| 1 | 0.708x | 0.678x | 0.971x | 0.993x | not recorded |
| 2 | 1.051x | 1.017x | 1.001x | 1.009x | not recorded |

The third run was stopped and is excluded. These two samples demonstrate substantial runtime noise; no five-run candidate conclusion is drawn yet. All completed cases passed correctness.

## Hardware microbench

Command: `sudo -n perf stat ... -- taskset -c 0-31 ./scripts/microbench.sh`

Cache topology from `/sys/devices/system/cpu/cpu0/cache`:

- Cache line: `64 B`
- L1D: `48 KiB`, 12-way, shared by logical CPUs `0,64`
- L1I: `32 KiB`, 8-way, shared by logical CPUs `0,64`
- L2: `2 MiB`, 16-way, shared by logical CPUs `0,64`
- L3: `60 MiB`, 15-way, shared by NUMA-node-0 CPUs `0-31,64-95`

Working-set copy probes reported `memcpy_gib_s=9.349` to `9.393` for the 256 MiB probe. The OpenMP probe reported `63.077` to `64.773 GFLOP/s`.

Process perf sample for the microbench: `3,017,669,449 cycles`, `6,221,070,827 instructions`, `89,419,761 cache-references`, `39,688,742 cache-misses`, `1,629,910,465 L1-dcache-loads`, `123,035,614 L1-dcache-load-misses`, `4,746,392 LLC-loads`, `892,924 LLC-load-misses`, elapsed `0.729 s`.

System-wide IMC sample during the probe: `2865.01 MiB` reads and `2700.19 MiB` writes over `0.692 s`, approximately `8.10 GiB/s` read and `7.63 GiB/s` write aggregate. These are exploratory probes, not the official score.
