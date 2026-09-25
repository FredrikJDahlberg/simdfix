#!/usr/bin/env bash
# bench-compare.sh <base-ref> [rounds] — compare SimdFixBenchmark at HEAD with base-ref.
#
#   .github/bench-compare.sh v0.1.0
#
# Builds both in Release, runs them alternately (rounds times each, 5 by default)
# and compares the median ns/msg of each benchmark they share. Fails if one is
# more than BENCH_THRESHOLD percent (default 3) slower at HEAD. Run it on an
# otherwise idle machine: the numbers are only as steady as the machine is.

set -euo pipefail

BASE="${1:?usage: $0 <base-ref> [rounds]}"
ROUNDS="${2:-5}"
THRESHOLD="${BENCH_THRESHOLD:-3}"
ROOT="$(git rev-parse --show-toplevel)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

build() {
    if ! { cmake -S "$1" -B "$1/build" -DCMAKE_BUILD_TYPE=Release &&
           cmake --build "$1/build" --target SimdFixBenchmark -j; } > "$1/build.log" 2>&1; then
        tail -n 50 "$1/build.log"
        exit 1
    fi
}

echo "Building $BASE and HEAD"
mkdir -p "$WORK/base" "$WORK/head"
git -C "$ROOT" archive "$BASE" | tar -x -C "$WORK/base"
git -C "$ROOT" archive HEAD | tar -x -C "$WORK/head"
build "$WORK/base"
build "$WORK/head"

echo "Running $ROUNDS rounds"
for round in $(seq 1 "$ROUNDS"); do
    "$WORK/base/build/SimdFixBenchmark" all > "$WORK/base-$round.txt"
    "$WORK/head/build/SimdFixBenchmark" all > "$WORK/head-$round.txt"
done

python3 - "$WORK" "$BASE" "$THRESHOLD" <<'EOF'
import glob, re, statistics, sys

work, base, threshold = sys.argv[1], sys.argv[2], float(sys.argv[3])
line = re.compile(r'^(\w[\w ]*?)\s+\d+ ms\s+[\d.]+ GB/s\s+([\d.]+) ns/msg', re.M)

def medians(prefix):
    runs = {}
    for path in glob.glob(f'{work}/{prefix}-*.txt'):
        for name, ns in line.findall(open(path).read()):
            runs.setdefault(name.strip(), []).append(float(ns))
    return {name: statistics.median(values) for name, values in runs.items()}

before, after = medians('base'), medians('head')
slower = []
print(f"{'benchmark':16} {base:>10} {'HEAD':>10} {'change':>8}")
for name in before:
    if name not in after:
        continue
    change = 100 * (after[name] - before[name]) / before[name]
    flag = '  <-- slower' if change > threshold else ''
    print(f'{name:16} {before[name]:10.1f} {after[name]:10.1f} {change:+7.1f}%{flag}')
    if change > threshold:
        slower.append(name)
if slower:
    print(f'\n{len(slower)} benchmark(s) more than {threshold:g}% slower than {base}: {", ".join(slower)}')
    sys.exit(1)
print(f'\nNo benchmark more than {threshold:g}% slower than {base}.')
EOF
