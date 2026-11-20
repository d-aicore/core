#!/usr/bin/env bash
B=${B:-$HOME/core-review-build}
echo "=== tidy output (first 18 lines) ==="
head -18 "$B/40-tidy-round.txt" 2>/dev/null
echo "=== counts so far ==="
printf "warning lines: %s\n" "$(grep -c "warning:" "$B/40-tidy-round.txt" 2>/dev/null || echo 0)"
printf "errors: %s\n" "$(grep -c "error:" "$B/40-tidy-round.txt" 2>/dev/null || echo 0)"
printf "files processed (Processing file): %s\n" "$(grep -c "Processing file" "$B/40-tidy-round.txt" 2>/dev/null || echo 0)"
