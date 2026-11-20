#!/usr/bin/env bash
B=${B:-$HOME/core-review-build}
echo "=== plain Debug engine_tests, filter engine.start ==="
"$B/debug/tests/engine_tests" "engine.start" 2>&1 | tail -20
echo "exit=$?"
echo "=== same case under ASan+UBSan ==="
"$B/asanubsan/tests/engine_tests" "engine.start" 2>&1 | tail -20
echo "exit=$?"
