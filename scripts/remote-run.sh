#!/usr/bin/env bash
# Update the remote Git checkout and run on at most 32 logical CPUs.
set -euo pipefail

MODE=${1:-test}
case "$MODE" in
  test|benchmark) ;;
  *) echo "Usage: $0 [test|benchmark]" >&2; exit 2 ;;
esac

REMOTE_HOST=${SRQ_REMOTE_HOST:-global.prd.ga.launchpad.nvidia.com}
REMOTE_PORT=${SRQ_REMOTE_PORT:-17255}
REMOTE_USER=${SRQ_REMOTE_USER:-nvidia}
REMOTE_DIR=${SRQ_REMOTE_DIR:-"/home/$REMOTE_USER/SRQ-26"}
REMOTE_CPUS=${SRQ_REMOTE_CPUS:-32}
# Public HTTPS URL cloned on the compute node. It is intentionally independent of the
# local `origin` remote, because the local machine pushes over SSH while the compute
# node pulls anonymously over HTTPS.
REMOTE_GIT_URL=${SRQ_REMOTE_GIT_URL:-https://github.com/ziyang22/SRQ-26.git}
REMOTE_OPENBLAS_ROOT=${SRQ_OPENBLAS_ROOT:-"/home/$REMOTE_USER/Ziyoung/deps/openblas"}
REMOTE_SEED=${SRQ_REMOTE_SEED:-260919}
SSH_OPTS=(-o ConnectTimeout=15 -o BatchMode=yes -o StrictHostKeyChecking=accept-new)
if [ "${SRQ_SSH_FORWARD_AGENT:-false}" = true ]; then
  SSH_OPTS+=(-A)
fi
REMOTE="$REMOTE_USER@$REMOTE_HOST"

case "$REMOTE_CPUS" in
  ''|*[!0-9]*) echo "SRQ_REMOTE_CPUS must be numeric" >&2; exit 2 ;;
esac
if [ "$REMOTE_CPUS" -lt 1 ] || [ "$REMOTE_CPUS" -gt 32 ]; then
  echo "SRQ_REMOTE_CPUS must be between 1 and 32" >&2
  exit 2
fi

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || { echo 'Local project is not a Git repository' >&2; exit 2; }
if [ -n "$(git status --porcelain)" ]; then
  echo 'Local Git worktree is dirty; commit and push before remote execution' >&2
  exit 2
fi
BRANCH=$(git branch --show-current)
[ -n "$BRANCH" ] || { echo 'Detached local HEAD is not supported' >&2; exit 2; }
COMMIT=$(git rev-parse HEAD)
# The local branch must already be recorded on the public repository.
if git rev-parse --verify --quiet "refs/remotes/origin/$BRANCH" >/dev/null; then
  if [ "$(git rev-parse "refs/remotes/origin/$BRANCH")" != "$COMMIT" ]; then
    echo "Local $BRANCH differs from origin/$BRANCH; push before remote execution" >&2
    exit 2
  fi
fi
REMOTE_CPU_LAST=$((REMOTE_CPUS - 1))

ssh "${SSH_OPTS[@]}" -p "$REMOTE_PORT" "$REMOTE" \
  "bash -s -- '$REMOTE_DIR' '$REMOTE_GIT_URL' '$BRANCH' '$COMMIT' '$MODE' '$REMOTE_CPUS' '$REMOTE_CPU_LAST' '$REMOTE_OPENBLAS_ROOT' '$REMOTE_SEED'" <<'REMOTE_SCRIPT'
set -euo pipefail
remote_dir=$1
remote_git_url=$2
branch=$3
commit=$4
mode=$5
cpu_count=$6
cpu_last=$7
openblas_root=$8
seed=$9

if [ ! -d "$remote_dir/.git" ]; then
  [ ! -e "$remote_dir" ] || { echo "Remote path exists but is not a Git checkout: $remote_dir" >&2; exit 2; }
  git clone --branch "$branch" --single-branch "$remote_git_url" "$remote_dir"
fi
cd "$remote_dir"
[ -z "$(git status --porcelain)" ] || { echo 'Remote Git worktree is dirty; refusing to overwrite it' >&2; exit 2; }
[ "$(git remote get-url origin)" = "$remote_git_url" ] || { echo 'Remote origin differs from SRQ_REMOTE_GIT_URL' >&2; exit 2; }
git fetch origin "$branch"
if git show-ref --verify --quiet "refs/heads/$branch"; then
  git switch "$branch"
else
  git switch --track -c "$branch" "origin/$branch"
fi
git merge --ff-only "origin/$branch"
[ "$(git rev-parse HEAD)" = "$commit" ] || { echo 'Local commit has not been pushed to origin, or origin advanced' >&2; exit 2; }

export OMP_NUM_THREADS=$cpu_count
export OMP_DYNAMIC=false
export OMP_PROC_BIND=close
export OMP_PLACES=cores
export OPENBLAS_ROOT="$openblas_root"
export OPENBLAS_NUM_THREADS="$cpu_count"
export SRQ_SEED="$seed"
printf 'remote_commit=%s cpuset=0-%s omp_threads=%s seed=%s openblas=%s\n' "$commit" "$cpu_last" "$OMP_NUM_THREADS" "$SRQ_SEED" "$OPENBLAS_ROOT"
exec taskset -c "0-$cpu_last" ./run.sh "$mode"
REMOTE_SCRIPT
