#!/usr/bin/env bash
B=${B:-$HOME/core-review-build}
echo "=== engine_tests known-env run (ASan+UBSan) ==="
"$B/asanubsan/tests/engine_tests" > "$B/asanubsan-engine-tests-known.txt" 2>&1
echo "exit=$? (expected 1: FL-1 device, ENV-08)" | tee -a "$B/asanubsan-engine-tests-known.txt"
echo "=== ASan+UBSan suite, engine_tests excluded ==="
ctest --test-dir "$B/asanubsan" -L merge -V --timeout 900 --no-tests=error -E engine_tests > "$B/asanubsan-ctest-green.txt" 2>&1
echo "ctest rc=$?"
grep -E "tests passed|tests failed|Total Test time" "$B/asanubsan-ctest-green.txt" | tail -3
echo "=== engine_tests known-env run (TSan) ==="
setarch "$(uname -m)" -R "$B/tsan/tests/engine_tests" > "$B/tsan-engine-tests-known.txt" 2>&1
echo "exit=$? (expected 1: FL-1 device)" | tee -a "$B/tsan-engine-tests-known.txt"
echo "=== TSan suite under setarch, engine_tests excluded ==="
setarch "$(uname -m)" -R ctest --test-dir "$B/tsan" -L merge -V --timeout 900 --no-tests=error -E engine_tests > "$B/tsan-ctest-green.txt" 2>&1
echo "ctest rc=$?"
grep -E "tests passed|tests failed|Total Test time" "$B/tsan-ctest-green.txt" | tail -3
printf "TSan warnings: %s\n" "$(grep -c "WARNING: ThreadSanitizer" "$B/tsan-ctest-green.txt" || true)"
