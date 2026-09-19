#!/usr/bin/env bash
# Hardware probe and small CPU/memory microbench. Runs within the project CPU cap.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CPUS=${SRQ_REMOTE_CPUS:-32}
LAST=$((CPUS - 1))
CC_BIN=${CC:-gcc}
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/srq-microbench.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

case "$CPUS" in
  ''|*[!0-9]*) echo 'SRQ_REMOTE_CPUS must be numeric' >&2; exit 2 ;;
esac
[ "$CPUS" -ge 1 ] && [ "$CPUS" -le 32 ] || { echo 'SRQ_REMOTE_CPUS must be between 1 and 32' >&2; exit 2; }

printf '%s\n' '=== hardware ==='
lscpu | grep -E '^(Architecture|Model name|Socket|Core|Thread|CPU\(s\)|NUMA node|Flags):' || true
numactl --hardware 2>/dev/null || true
printf 'cpuset=0-%s omp_threads=%s compiler=%s\n' "$LAST" "$CPUS" "$CC_BIN"
printf '%s\n' '=== cache topology (cpu0) ==='
for cache in /sys/devices/system/cpu/cpu0/cache/index*; do
  printf 'level=%s type=%s line_bytes=%s size=%s ways=%s shared=%s\n' \
    "$(cat "$cache/level")" "$(cat "$cache/type")" \
    "$(cat "$cache/coherency_line_size")" "$(cat "$cache/size")" \
    "$(cat "$cache/ways_of_associativity")" "$(cat "$cache/shared_cpu_list")"
done

cat > "$TMP_DIR/microbench.c" <<'SOURCE'
#define _GNU_SOURCE
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(void) {
    const size_t bytes = 256ULL * 1024ULL * 1024ULL;
    const size_t count = bytes / sizeof(double);
    double *a = NULL, *b = NULL;
    if (posix_memalign((void **)&a, 64, bytes) || posix_memalign((void **)&b, 64, bytes)) return 2;
    for (size_t i = 0; i < count; ++i) { a[i] = 1.0; b[i] = 2.0; }
    const size_t working_sets[] = {32ULL * 1024, 1024ULL * 1024, 32ULL * 1024 * 1024, bytes};
    const int repeats[] = {4096, 512, 32, 8};
    const char *names[] = {"l1_working_set", "l2_working_set", "l3_working_set", "dram_working_set"};
    for (int i = 0; i < 4; ++i) {
        double t0 = now();
        for (int r = 0; r < repeats[i]; ++r) memcpy(b, a, working_sets[i]);
        double t1 = now();
        printf("%s_bytes=%zu %s_gib_s=%.3f\n", names[i], working_sets[i], names[i],
               (double)(working_sets[i] * (size_t)repeats[i]) / (t1 - t0) /
               (1024.0 * 1024.0 * 1024.0));
    }

    const int n = 512, repeats = 8;
    double *x = aligned_alloc(64, (size_t)n * n * sizeof(double));
    double *y = aligned_alloc(64, (size_t)n * n * sizeof(double));
    double *z = aligned_alloc(64, (size_t)n * n * sizeof(double));
    if (!x || !y || !z) return 3;
    for (int i = 0; i < n * n; ++i) { x[i] = 0.001; y[i] = 0.002; z[i] = 0.0; }
    t0 = now();
    for (int r = 0; r < repeats; ++r) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < n; ++k) {
                double v = x[i * n + k];
                for (int j = 0; j < n; ++j) z[i * n + j] += v * y[k * n + j];
            }
        }
    }
    t1 = now();
    printf("omp_threads=%d matmul_gflop_s=%.3f checksum=%.6f\n", omp_get_max_threads(),
           (2.0 * n * n * n * repeats / (t1 - t0)) / 1e9, z[0]);
    free(a); free(b); free(x); free(y); free(z);
    return 0;
}
SOURCE

"$CC_BIN" -O3 -std=c11 -fopenmp "$TMP_DIR/microbench.c" -lm -o "$TMP_DIR/microbench"
export OMP_NUM_THREADS="$CPUS" OMP_DYNAMIC=false OMP_PROC_BIND=close OMP_PLACES=cores
printf '%s\n' '=== microbench ==='
taskset -c "0-$LAST" "$TMP_DIR/microbench"
