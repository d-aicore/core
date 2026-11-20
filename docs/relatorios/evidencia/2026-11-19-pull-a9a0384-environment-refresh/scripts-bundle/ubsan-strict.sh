#!/usr/bin/env bash
# Strict UBSan canary: halt_on_error=1 makes the trap fatal, so a green suite cannot hide it.
B=${B:-$HOME/core-review-build}
exe="$B/asanubsan/tests/ubsan_canary"
out="$B/ubsan-wsl-canary-strict.txt"
: > "$out"
for i in 1 2 3; do
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$exe" >> "$out" 2>&1
  printf "ubsan-strict run%s exit=%s\n" "$i" "$?" | tee -a "$out"
done
grep -E "runtime error|SUMMARY|non-zero" "$out" | head -8
