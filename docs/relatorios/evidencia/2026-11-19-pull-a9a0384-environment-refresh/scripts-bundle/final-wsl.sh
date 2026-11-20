#!/usr/bin/env bash
B=${B:-$HOME/core-review-build}
echo "=== tsan setarch: result lines ==="
grep -E "tests passed|tests failed|Total Test time" "$B/tsan-ctest-setarch.txt" | tail -4
echo "=== tsan setarch: failed tests ==="
grep -A6 "The following tests FAILED" "$B/tsan-ctest-setarch.txt" | head -8
echo "=== tsan counters ==="
printf "WARNING: ThreadSanitizer = %s\n" "$(grep -c "WARNING: ThreadSanitizer" "$B/tsan-ctest-setarch.txt" || true)"
printf "FATAL: ThreadSanitizer   = %s\n" "$(grep -c "FATAL: ThreadSanitizer" "$B/tsan-ctest-setarch.txt" || true)"
echo "=== tidy summary ==="
head -16 "$B/40-tidy-summary.txt" 2>/dev/null || echo "(no summary yet)"
echo "=== tidy round size ==="
ls -l "$B/40-tidy-round.txt" 2>/dev/null || echo "(no tidy output yet)"
echo "=== tidy processes ==="
ps aux | grep -c "[c]lang-tidy" || true
