#!/usr/bin/env bash
B=${B:-$HOME/core-review-build}
echo "=== engine_tests block (lines 12-46) ==="
sed -n "12,46p" "$B/asanubsan-ctest.txt"
echo "=== tsan setarch ctest done? ==="
ls -l "$B/tsan-ctest-setarch.txt" 2>/dev/null && tail -6 "$B/tsan-ctest-setarch.txt"
