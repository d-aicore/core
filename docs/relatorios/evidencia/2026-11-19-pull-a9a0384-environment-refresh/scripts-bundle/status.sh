#!/usr/bin/env bash
B=${B:-$HOME/core-review-build}
echo "=== markers ==="
ls -1 "$B"/*.done 2>/dev/null | while read -r f; do printf "%-22s %s\n" "$(basename "$f" .done)" "$(cat "$f")"; done
echo "=== tails ==="
for f in 01-gdb asanubsan-build asanubsan-ctest tsan-build tsan-ctest 40-tidy-summary 10-gdb-summary; do
  p="$B/$f.txt"
  if [ -f "$p" ]; then printf "\n--- %s ---\n" "$f"; tail -5 "$p"; fi
done
