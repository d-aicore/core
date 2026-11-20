#!/usr/bin/env bash
B=${B:-$HOME/core-review-build}
echo "=== tsan canary ==="
grep -E "ThreadSanitizer|run[0-9] exit=|non-zero exits|report matched" "$B/tsan-wsl-canary.txt" 2>/dev/null | head -10
echo "=== asanubsan ctest summary ==="
head -12 "$B/asanubsan-ctest-summary.txt" 2>/dev/null
echo "=== tsan ctest summary ==="
head -12 "$B/tsan-ctest-summary.txt" 2>/dev/null
echo "=== tidy ==="
head -18 "$B/40-tidy-summary.txt" 2>/dev/null
echo "=== tidy scope ==="
head -6 "$B/40-tidy-scope-gate.txt" 2>/dev/null
echo "=== markers ==="
ls -1 "$B"/*.done | xargs -n1 basename
