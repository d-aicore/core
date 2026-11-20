#!/usr/bin/env bash
echo "--- clone CMakeLists ---"
grep -n "legacy_guard_tests menu_tests" "$HOME/core-review-src/CMakeLists.txt" || echo "NOT FOUND in clone"
echo "--- md5 ---"
md5sum "$HOME/core-review-src/CMakeLists.txt" /mnt/d/CORE/CORE/CMakeLists.txt
echo "--- markers ---"
ls -1 "$HOME/core-review-build"/*.done 2>/dev/null | xargs -n1 basename
