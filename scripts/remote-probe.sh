#!/usr/bin/env bash
# Read-only probe of the configured remote compute node.
set -euo pipefail

REMOTE_HOST=${SRQ_REMOTE_HOST:-global.prd.ga.launchpad.nvidia.com}
REMOTE_PORT=${SRQ_REMOTE_PORT:-17255}
REMOTE_USER=${SRQ_REMOTE_USER:-nvidia}
SSH_OPTS=(-o ConnectTimeout=15 -o BatchMode=yes -o StrictHostKeyChecking=accept-new)

ssh "${SSH_OPTS[@]}" -p "$REMOTE_PORT" "$REMOTE_USER@$REMOTE_HOST" 'set -e
printf "=== identity ===\n"; id
printf "=== kernel ===\n"; uname -a
printf "=== cpu ===\n"; nproc; lscpu | sed -n "/Architecture:/p;/Model name:/p;/Thread(s) per core:/p;/Core(s) per socket:/p;/Socket(s):/p;/CPU(s):/p;/NUMA node(s):/p;/NUMA node[0-9].*CPU(s):/p;/Flags:/p"
printf "=== memory ===\n"; free -h
printf "=== toolchain ===\n"
for tool in gcc clang cmake taskset numactl git; do
  if command -v "$tool" >/dev/null 2>&1; then
    printf "%s: %s\n" "$tool" "$(command -v "$tool")"
    case "$tool" in
      gcc) gcc --version | head -1;;
      clang) clang --version | head -1;;
      cmake) cmake --version | head -1;;
      git) git --version;;
    esac
  else
    printf "%s: unavailable\n" "$tool"
  fi
done
printf "=== limits ===\n"; ulimit -a
printf "=== openmp ===\n"; ldconfig -p 2>/dev/null | grep -E "lib(gomp|omp|iomp)" || true
'
