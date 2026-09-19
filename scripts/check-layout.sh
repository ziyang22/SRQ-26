#!/usr/bin/env bash
# Reject source files outside their intended project directories.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

bad=0
while IFS= read -r file; do
  case "$file" in
    task/*.c|task/*.cc|task/*.cpp|task/*.h|include/*.h|src/*.c|src/*.cc|src/*.cpp|src/*.h|archive/*|build/*|.git/*) ;;
    *) echo "unexpected source-like file: $file" >&2; bad=1 ;;
  esac
done < <(find . -type f \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.h' \) -not -path './.git/*' -not -path './build/*' | sed 's#^./##' | sort)

if find . -maxdepth 1 -type f \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.h' \) | grep -q .; then
  echo 'source files are not allowed in the project root' >&2
  bad=1
fi

if [ "$bad" -ne 0 ]; then
  exit 1
fi
printf 'Project layout OK\n'
