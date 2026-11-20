#!/usr/bin/env bash
B=${B:-$HOME/core-review-build}
echo "=== TSan with ASLR disabled (setarch -R) ==="
exe="$B/tsan/tests/tsan_canary"
out="$B/tsan-wsl-canary-setarch.txt"
: > "$out"
for i in 1 2 3; do
  setarch "$(uname -m)" -R "$exe" >> "$out" 2>&1
  printf "tsan-setarch run%s exit=%s\n" "$i" "$?" | tee -a "$out"
done
grep -E "ThreadSanitizer|data race|run[0-9] exit=" "$out" | head -10
echo "=== asanubsan failed test ==="
grep -E "^The following tests FAILED|Failed|Not Run" "$B/asanubsan-ctest.txt" | head -8
echo "=== asanubsan ctest tail ==="
tail -12 "$B/asanubsan-ctest.txt"
