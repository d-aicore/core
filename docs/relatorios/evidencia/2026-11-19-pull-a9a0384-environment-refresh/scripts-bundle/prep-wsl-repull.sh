#!/usr/bin/env bash
# prep-wsl-repull.sh - archive the 8c6837e WSL evidence and ready the clone for the a9a0384 re-run.
# Environment/tooling only. Never writes into /mnt/d/CORE/CORE.
set -eu
B="$HOME/core-review-build"
SRC="$HOME/core-review-src"
mkdir -p "$B/archive-8c6837e"
n=0
for f in "$B"/*.txt "$B"/*.done; do
  if [ -f "$f" ]; then mv -f "$f" "$B/archive-8c6837e/"; n=$((n+1)); fi
done
echo "archived $n WSL evidence files"
echo "build dirs kept:"
ls -d "$B"/*/
# The clone's only local mod is the mirrored CMakeLists.txt (the original is kept in the Windows
# working tree); reset it so run-wsl.sh step 02 can detach-checkout the new Windows HEAD.
# Step 02b of run-wsl.sh re-mirrors the extended fix afterwards.
git -C "$SRC" checkout -- CMakeLists.txt
echo "clone status after reset (empty = clean):"
git -C "$SRC" status --porcelain=v1
echo "prep-wsl-repull.sh done"
