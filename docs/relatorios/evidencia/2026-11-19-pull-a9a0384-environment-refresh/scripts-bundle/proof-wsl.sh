#!/usr/bin/env bash
# proof-wsl.sh - stage 0.2 proof: SHA-bound validation + hard checkout gate in run-wsl.sh.
# Fresh folders (~/core-review-proof, ~/core-review-proof-src, wsl-proof logs); the historical
# reference (~/core-review-build) is never touched. Proves:
#   W1 clone/checkout at the intended Windows SHA, hard gate passes, marker fingerprinted
#   W2 same context -> SKIP(reuse), gate re-verified on every run
#   W3 WIN_SRC at the OLD revision (8c6837e worktree) -> INVALIDATED, checkout follows, gate verifies
#   W4 WIN_SRC back to main -> INVALIDATED, checkout returns to a9a0384
#   W5 a real configuration (plain Debug) configures+builds in the fresh proof root
#   W6 identical context -> SKIP(reuse) of the build
# Build-step revision/flag invalidation uses the same fingerprint comparison proven by execution
# on Windows (proof-windows.ps1 P3/P6/P7); here the clone-level revision binding is proven live.
set -u
ROOT=/mnt/d/Core-build/core-review
B="$HOME/core-review-proof"
SRC="$HOME/core-review-proof-src"
OUT="$ROOT/wsl-proof"
OLD="$ROOT/proof-src-old"
MAIN=/mnt/d/CORE/CORE
mkdir -p "$B" "$OUT"
if [ ! -d "$SRC/.git" ]; then
  echo "creating shared proof clone (borrows objects from the main clone; disposable)"
  git clone --shared "$HOME/core-review-src" "$SRC" || exit 1
fi
LOG="$OUT/proof-wsl-log.txt"
: > "$LOG"

invoke() {
  local label=$1 only=$2 winsrc=${3:-$MAIN}
  local tmp="$B/$label.out"
  WIN_SRC="$winsrc" SRC="$SRC" B="$B" OUT="$OUT" ONLY="$only" bash "$ROOT/run-wsl.sh" > "$tmp" 2>&1
  local rc=$?
  { echo ""; echo "=== $label (WIN_SRC=$winsrc ONLY=$only) rc=$rc ==="; cat "$tmp"; } >> "$LOG"
  echo "--- $label rc=$rc"
  grep -E "SKIP\(reuse\)|INVALIDATED|checkout verified|FATAL|configure rc=|build rc=" "$tmp" | sed 's/^/    /'
  if [ -f "$B/02-clone.done" ]; then cp -f "$B/02-clone.done" "$OUT/$label-02-clone.done"; fi
  if [ -f "$B/cfg-debug.done" ]; then cp -f "$B/cfg-debug.done" "$OUT/$label-cfg-debug.done"; fi
}

invoke W1 02-clone
invoke W2 02-clone
invoke W3 02-clone "$OLD"
invoke W4 02-clone
invoke W5 10-debug
invoke W6 10-debug

{
  echo ""
  echo "stage 0.2 WSL proof summary (expected -> observed)"
  grep -q "=== W1 .*rc=0" "$LOG" && grep -A20 "=== W1 " "$LOG" | grep -q "checkout verified" && echo "W1 checkout at intended SHA + hard gate: PASS" || echo "W1: FAIL"
  grep -A20 "=== W2 " "$LOG" | grep -q "SKIP(reuse) 02-clone" && echo "W2 same context reuses: PASS" || echo "W2: FAIL"
  grep -A20 "=== W3 " "$LOG" | grep -q "INVALIDATED 02-clone" && echo "W3 old revision invalidates: PASS" || echo "W3: FAIL"
  grep -A20 "=== W4 " "$LOG" | grep -q "INVALIDATED 02-clone" && echo "W4 return to main invalidates: PASS" || echo "W4: FAIL"
  grep -A40 "=== W5 " "$LOG" | grep -q "build rc=0" && echo "W5 fresh debug configure+build: PASS" || echo "W5: FAIL"
  grep -A20 "=== W6 " "$LOG" | grep -q "SKIP(reuse) cfg-debug" && echo "W6 same config reuses build: PASS" || echo "W6: FAIL"
  echo ""
  echo "marker fingerprints (first line of each snapshot):"
  for f in "$OUT"/W*-02-clone.done "$OUT"/W5-cfg-debug.done; do
    [ -f "$f" ] && echo "$(basename "$f"): $(head -1 "$f")"
  done
} | tee -a "$LOG"
echo "proof-wsl.sh finished"
