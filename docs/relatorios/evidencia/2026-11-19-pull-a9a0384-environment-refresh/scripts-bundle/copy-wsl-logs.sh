#!/usr/bin/env bash
B=${B:-$HOME/core-review-build}
D=/mnt/d/Core-build/core-review/wsl
mkdir -p "$D"
for f in 00-probe.txt 01-gdb.txt 02-clone.txt 20-asanubsan-flags.txt 30-tsan-flags.txt \
         asan-wsl-canary.txt ubsan-wsl-canary.txt ubsan-wsl-canary-strict.txt tsan-wsl-canary.txt \
         tsan-wsl-canary-setarch.txt asanubsan-ctest.txt asanubsan-ctest-summary.txt \
         tsan-ctest.txt tsan-ctest-summary.txt tsan-ctest-setarch.txt \
         40-tidy-scope.txt 40-tidy-scope-gate.txt 40-tidy-round.txt 40-tidy-summary.txt \
         10-gdb-summary.txt 99-summary.txt; do
  [ -f "$B/$f" ] && cp -f "$B/$f" "$D/"
done
echo "copied:"; ls -1 "$D" | wc -l
echo "=== tsan setarch ctest tail ==="
tail -5 "$B/tsan-ctest-setarch.txt" 2>/dev/null
echo "=== tidy summary ==="
head -12 "$B/40-tidy-summary.txt" 2>/dev/null
