#!/usr/bin/env bash
B=${B:-$HOME/core-review-build}
echo "=== warnings from engine_client.cpp (the Windows-only file) ==="
grep -c "engine_client.cpp.*warning:" "$B/40-tidy-round.txt" || true
echo "=== pure warning count (excluding the two diagnostic errors) ==="
grep -c ": warning:" "$B/40-tidy-round.txt"
