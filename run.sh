#!/usr/bin/env bash
# SRQ-26 本地统一入口：配置、编译并运行正确性或正式 benchmark。
set -euo pipefail

ROOT=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR=${BUILD_DIR:-"$ROOT/build"}
MODE=${1:-benchmark}

case "$MODE" in
  test|benchmark) ;;
  *) echo "用法: $0 [test|benchmark]" >&2; exit 2 ;;
esac

if command -v cmake >/dev/null 2>&1; then
  cmake_args=(-S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}")
  if [ -n "${CC:-}" ]; then
    cmake_args+=("-DCMAKE_C_COMPILER=$CC")
  fi
  cmake "${cmake_args[@]}"
  cmake --build "$BUILD_DIR" --parallel "${BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}"
else
  compiler=${CC:-gcc}
  command -v "$compiler" >/dev/null 2>&1 || { echo "找不到 cmake 或 C 编译器 $compiler" >&2; exit 127; }
  omp_flag=()
  if printf 'int main(void){return 0;}\n' | "$compiler" -x c -fopenmp -o /tmp/srq-openmp-check - >/dev/null 2>&1; then
    omp_flag=(-fopenmp)
    rm -f /tmp/srq-openmp-check
  fi
  "$compiler" -O3 -std=c99 "${omp_flag[@]}" \
    "$ROOT/src/main.c" "$ROOT/task/task.c" \
    "$ROOT/src/activ_baseline.c" "$ROOT/task/activation.c" \
    -o "$ROOT/run_matrix_multiplication" -lm
fi

if [ "$MODE" = test ]; then
  exec "$ROOT/run_matrix_multiplication" test
else
  exec "$ROOT/run_matrix_multiplication"
fi
