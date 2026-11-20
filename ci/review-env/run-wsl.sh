#!/usr/bin/env bash
# run-wsl.sh - Core: environment for sanitization (ASan+UBSan, TSan), clang-tidy and debugger proof (WSL).
# Prepared 2026-11-19; validation hardened 2026-11-19 (stage 0.2): every .done marker is bound to
#   fp1 sha=<Windows HEAD> local=<hash of tracked build-affecting Windows diff> script=<this file hash> cfg=<configuration>
# A step is reused ONLY when that whole fingerprint matches; a new revision, a tracked source/CMake
# change on the Windows side (docs excluded), or a changed configuration invalidates the affected
# steps. Legacy markers without fp1 are always re-run. Before ANY result is accepted, the clone is
# verified to be detached at exactly the intended Windows SHA (hard gate, every run).
# Mirrors docs/relatorios/2026-11-17-preflight-sanitizadores-clang-tidy.md: own clone on ext4 (drvfs
# builds are slow), out-of-tree builds, canaries must trip before a green suite is trusted,
# ci/tidy_scope.py asserts the analysis scope. Environment/tooling only. Never writes into $WIN_SRC.
# ONLY=<pattern> filters top-level steps (proof runs); OUT overrides the Windows log-copy directory.
set -u

WIN_SRC=${WIN_SRC:-/mnt/d/CORE/CORE}
SRC=${SRC:-$HOME/core-review-src}
B=${B:-$HOME/core-review-build}
OUT=${OUT:-/mnt/d/Core-build/core-review/wsl}
JOBS=${JOBS:-$(nproc)}
ONLY=${ONLY:-}

mkdir -p "$B" "$OUT"

# ---------- context fingerprint ----------
SHA=$(git -C "$WIN_SRC" rev-parse HEAD 2>/dev/null)
if [ -z "$SHA" ]; then echo "FATAL: cannot read HEAD of $WIN_SRC"; exit 1; fi
LOCAL_HASH=$(git -C "$WIN_SRC" diff -- . ':(exclude)docs' 2>/dev/null | sha256sum | cut -d' ' -f1)
SCRIPT_HASH=$(sha256sum "$0" | cut -d' ' -f1)
CONTEXT="fp1 sha=$SHA local=$LOCAL_HASH script=$SCRIPT_HASH"
echo "context: $CONTEXT cfg=<per step>"

fp()         { printf '%s cfg=%s\n' "$CONTEXT" "$1"; }
done_mark()  { [ -f "$B/$1.done" ] && [ "$(head -1 "$B/$1.done")" = "$(fp "$2")" ]; }
mark()       { { fp "$2"; printf '%s\n' "$3"; } > "$B/$1.done"; }
say()        { printf '\n===== %s =====\n' "$1"; }
want()       { [[ -z "$ONLY" || "$1" == $ONLY ]]; }
invalidated_note() {
  if [ -f "$B/$1.done" ]; then
    echo "INVALIDATED $1 - recorded result belongs to a different SHA/local-change/script/config"
  fi
}

# ---------- 00 toolchain probe ----------
if want 00-probe; then
  if ! done_mark 00-probe toolchain; then
    say "00 toolchain probe"
    {
      echo "--- distro/kernel ---"; uname -a; head -3 /etc/os-release 2>/dev/null
      echo "--- cores ---"; nproc
      echo "--- tools ---"
      for t in git cmake ninja ctest gcc g++ clang clang++ clang-tidy gdb python3 ldd; do
        if command -v "$t" >/dev/null 2>&1; then
          printf "%-12s %s  %s\n" "$t" "$(command -v "$t")" "$("$t" --version 2>&1 | head -1)"
        else
          printf "%-12s MISSING\n" "$t"
        fi
      done
      echo "--- sanitizer runtimes ---"
      ldconfig -p 2>/dev/null | grep -E "libasan|libtsan|libubsan" | head -10
      echo "--- disk ---"; df -h "$HOME" | tail -2
    } > "$B/00-probe.txt" 2>&1
    cat "$B/00-probe.txt"
    mark 00-probe toolchain "toolchain recorded"
  else
    echo "SKIP(reuse) 00-probe"
  fi
fi

# ---------- 01 gdb (debugger proof); install when the distro provides it ----------
if want 01-gdb; then
  if ! done_mark 01-gdb toolchain; then
    say "01 gdb availability"
    if command -v gdb >/dev/null 2>&1; then
      echo "gdb already present: $(gdb --version | head -1)" | tee "$B/01-gdb.txt"
      mark 01-gdb toolchain "already installed"
    elif sudo -n true 2>/dev/null; then
      echo "installing gdb via apt-get (passwordless sudo available)" > "$B/01-gdb.txt"
      # WSL may have no route to the mirror: fail fast instead of retrying every run.
      sudo -n apt-get install -y -o Acquire::http::Timeout=4 -o Acquire::Retries=0 --no-install-recommends gdb >> "$B/01-gdb.txt" 2>&1
      rc=$?
      echo "apt rc=$rc" >> "$B/01-gdb.txt"
      if command -v gdb >/dev/null 2>&1; then
        echo "gdb now: $(gdb --version | head -1)" | tee -a "$B/01-gdb.txt"
        mark 01-gdb toolchain "installed via apt-get rc=$rc"
      else
        echo "gdb STILL MISSING (no network route to the apt mirror in this WSL)" | tee -a "$B/01-gdb.txt"
        mark 01-gdb toolchain "missing - apt-get could not reach the mirror (network unreachable in WSL)"
      fi
    else
      echo "gdb MISSING and no passwordless sudo" | tee "$B/01-gdb.txt"
    fi
  else
    echo "SKIP(reuse) 01-gdb"
  fi
fi

# ---------- 02 own clone on ext4 at the Windows SHA (hard verification) ----------
if want 02-clone; then
  if ! done_mark 02-clone clone; then
    invalidated_note 02-clone
    say "02 clone/refresh $SRC @ $SHA"
    {
      if [ ! -d "$SRC/.git" ]; then
        echo "cloning $WIN_SRC -> $SRC"
        git clone "$WIN_SRC" "$SRC"
      else
        echo "reusing $SRC"
        git -C "$SRC" fetch --all --quiet || true
      fi
      # The only tolerated local modification is the mirrored CMakeLists.txt (step 02b); reset it
      # so the detach checkout cannot be blocked by the previous mirror.
      git -C "$SRC" checkout -- CMakeLists.txt 2>/dev/null || true
      git -C "$SRC" checkout --detach "$SHA" 2>&1 || echo "WARN: checkout of $SHA failed"
      echo "--- resulting state ---"
      git -C "$SRC" rev-parse HEAD
      git -C "$SRC" status --porcelain=v1 | head -20
    } > "$B/02-clone.txt" 2>&1
    tail -5 "$B/02-clone.txt"
    if [ "$(git -C "$SRC" rev-parse HEAD 2>/dev/null)" = "$SHA" ]; then
      mark 02-clone clone "$SHA"
    fi
  else
    echo "SKIP(reuse) 02-clone"
  fi
fi

# HARD GATE (every run, even on reuse): no result is accepted unless the clone is exactly at the
# intended Windows SHA. A stale clone must never produce "green" evidence for a different revision.
CLONE_SHA=$(git -C "$SRC" rev-parse HEAD 2>/dev/null)
if [ "$CLONE_SHA" != "$SHA" ]; then
  echo "FATAL: clone $SRC is at $CLONE_SHA but the intended revision is $SHA - refusing to build/accept results"
  exit 1
fi
echo "checkout verified: $SRC @ $CLONE_SHA == Windows HEAD"

# 02b mirror the Windows working-tree CMakeLists on every run: the sanitizer wiring fix is
# uncommitted there (environment/tooling change, see docs/relatorios/2026-11-19-*).
if [ -f "$WIN_SRC/CMakeLists.txt" ]; then
  if ! cmp -s "$WIN_SRC/CMakeLists.txt" "$SRC/CMakeLists.txt"; then
    cp -f "$WIN_SRC/CMakeLists.txt" "$SRC/CMakeLists.txt"
    echo "synced CMakeLists.txt from the Windows working tree"
  fi
  if ! cmp -s "$WIN_SRC/CMakeLists.txt" "$SRC/CMakeLists.txt"; then
    echo "FATAL: CMakeLists.txt mirror does not match the Windows working tree"
    exit 1
  fi
fi

# helper: configure + build one out-of-tree configuration (extra args passed through)
build_cfg() {
  local name=$1 cfgtag=$2; shift 2
  local dir="$B/$name"
  if done_mark "cfg-$name" "$cfgtag"; then echo "SKIP(reuse) cfg-$name (fingerprint matches)"; return 0; fi
  invalidated_note "cfg-$name"
  say "configure $name"
  cmake -S "$SRC" -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCORE_BUILD_APP=OFF \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$@" > "$B/$name-configure.txt" 2>&1
  local crc=$?
  echo "configure rc=$crc"
  if [ $crc -ne 0 ]; then tail -30 "$B/$name-configure.txt"; return $crc; fi
  say "build $name"
  cmake --build "$dir" --parallel "$JOBS" > "$B/$name-build.txt" 2>&1
  local brc=$?
  echo "build rc=$brc"
  if [ $brc -ne 0 ]; then tail -40 "$B/$name-build.txt"; return $brc; fi
  mark "cfg-$name" "$cfgtag" "configured+built rc=0"
  return 0
}

# helper: count sanitizer flags actually present in a compile db
flag_proof() {
  local db=$1 out=$2; shift 2
  python3 - "$db" "$out" "$@" <<'PY'
import json, sys
db_path, out = sys.argv[1], sys.argv[2]
needles = sys.argv[3:]
db = json.load(open(db_path))
lines = ["compile db: %s" % db_path, "total translation units: %d" % len(db)]
for n in needles:
    hit = [e for e in db if n in e.get("command", "")]
    lines.append("entries containing %r: %d" % (n, len(hit)))
    miss = [e["file"].split("/")[-1] for e in db if n not in e.get("command", "")]
    lines.append("  without it: %s" % (", ".join(sorted(set(miss))) or "(none)"))
open(out, "w").write("\n".join(lines) + "\n")
print("\n".join(lines))
PY
}

# helper: run a canary N times, record exit codes, assert the sanitizer report appeared
canary_proof() {
  local exe=$1 label=$2 runs=$3 want_re=$4 cfgtag=$5 runner="${6:-}"
  local out="$B/$label-canary.txt"
  say "canary $label ($runs runs)"
  : > "$out"
  local i rc hits=0
  for i in $(seq 1 "$runs"); do
    $runner "$exe" >> "$out" 2>&1
    rc=$?
    printf "%s run%s exit=%s\n" "$label" "$i" "$rc" | tee -a "$out"
    [ $rc -ne 0 ] && hits=$((hits+1))
  done
  echo "$label: non-zero exits $hits/$runs" | tee -a "$out"
  if grep -Eq "$want_re" "$out"; then
    echo "$label: report matched /$want_re/" | tee -a "$out"
    grep -Em1 "$want_re" "$out"
    grep -Em1 "canary.cpp:[0-9]+" "$out" || true
    mark "canary-$label" "$cfgtag" "$hits/$runs non-zero; report matched"
  else
    echo "$label: NO sanitizer report matched /$want_re/ -- instrumentation NOT proven" | tee -a "$out"
  fi
}

# helper: ctest -L merge verbose + sanitizer report counters
suite_proof() {
  local name=$1 cfgtag=$2 runner="${3:-}"
  local dir="$B/$1" log="$B/$1-ctest.txt"
  if done_mark "ctest-$name" "$cfgtag"; then echo "SKIP(reuse) ctest-$name"; return 0; fi
  invalidated_note "ctest-$name"
  say "ctest merge in $name"
  # engine_tests is excluded from the green run and recorded separately: its FL-1 case needed an
  # audio device this WSL could not open at 8c6837e (ENV-08). Upstream BUG-SWEEP-06 (bcf9224) added
  # a normal-priority retry; the separate run below measures whether that holds at this revision.
  $runner ctest --test-dir "$dir" -L merge -V --timeout 900 --no-tests=error -E engine_tests > "$log" 2>&1
  local rc=$?
  echo "ctest rc=$rc"
  {
    echo "config: $name   ctest rc=$rc"
    grep -E "tests passed|tests failed|Total Test time" "$log" | tail -5
    echo "--- sanitizer/runtime report counters (0 = clean) ---"
    for p in "ERROR: AddressSanitizer" "runtime error:" "LeakSanitizer" "WARNING: ThreadSanitizer" "FATAL"; do
      c=$(grep -Fc "$p" "$log" 2>/dev/null || true)
      [ -z "$c" ] && c=0
      printf "%-32s %s\n" "$p" "$c"
    done
    echo "--- per-suite case/verification totals ---"
    grep -Eo "[0-9]+ (testes|tests), [0-9]+ (verificacoes|verifications)" "$log" | sort | uniq -c | head -30
  } > "$B/$name-ctest-summary.txt" 2>&1
  cat "$B/$name-ctest-summary.txt"
  [ $rc -eq 0 ] && mark "ctest-$name" "$cfgtag" "rc=0"
  return $rc
}

# ---------- 10 plain Debug build (no sanitizer) + gdb breakpoint proof ----------
if want 10-debug; then
  build_cfg debug "debug-testsON-nosan" -DCORE_BUILD_TESTS=ON -DCORE_SANITIZE= || true
  if [ -f "$B/cfg-debug.done" ] && [ -x "$B/debug/tests/engine_tests" ] && ! done_mark 10-gdb "debug-testsON-nosan"; then
    say "10 gdb breakpoint proof (engine_tests, Debug -O0 -g)"
    if command -v gdb >/dev/null 2>&1; then
      gdb -q -batch \
        -ex "set pagination off" \
        -ex "break main" \
        -ex "run tempo" \
        -ex "echo \n--- backtrace at main ---\n" -ex "bt" \
        -ex "echo \n--- source location ---\n" -ex "info source" \
        -ex "list" \
        -ex "kill" -ex "quit" \
        --args "$B/debug/tests/engine_tests" tempo > "$B/10-gdb-main.txt" 2>&1
      echo "gdb(main) rc=$?"
      gdb -q -batch \
        -ex "set pagination off" \
        -ex "break core::TempoMap::sampleAt" \
        -ex "break core::TempoMap::sampleOfTick" \
        -ex "break core::TempoMap::tickAt" \
        -ex "run tempo" \
        -ex "echo \n--- backtrace inside Core ---\n" -ex "bt" \
        -ex "echo \n--- source location ---\n" -ex "info source" \
        -ex "echo \n--- source lines at the breakpoint ---\n" -ex "list" \
        -ex "echo \n--- locals ---\n" -ex "info locals" \
        -ex "kill" -ex "quit" \
        --args "$B/debug/tests/engine_tests" tempo > "$B/10-gdb-core.txt" 2>&1
      echo "gdb(core) rc=$?"
      {
        echo "=== gdb proof summary ==="
        echo "-- main breakpoint --"
        grep -E "^Breakpoint 1|^#[0-9]+|Current source file|^Located in" "$B/10-gdb-main.txt" | head -12
        echo "-- Core library breakpoint --"
        grep -E "^Breakpoint [0-9]+|^#[0-9]+|Current source file|^Located in" "$B/10-gdb-core.txt" | head -20
      } > "$B/10-gdb-summary.txt" 2>&1
      cat "$B/10-gdb-summary.txt"
      if grep -qE "engine/src/core/tempo_map\.cpp" "$B/10-gdb-core.txt"; then
        mark 10-gdb "debug-testsON-nosan" "breakpoint inside core::TempoMap resolved to engine/src/core/tempo_map.cpp:line"
      elif grep -qE "tests/engine_tests\.cpp" "$B/10-gdb-main.txt"; then
        mark 10-gdb "debug-testsON-nosan" "breakpoint resolved to tests/engine_tests.cpp:line (Core-library breakpoint not hit)"
      else
        echo "WARN: no Core file:line resolved by gdb; inspect $B/10-gdb-*.txt"
      fi
    else
      echo "gdb MISSING - breakpoint proof not possible in WSL" > "$B/10-gdb-summary.txt"
    fi
  fi
fi

# ---------- 20 ASan + UBSan ----------
if want 20-asanubsan; then
  build_cfg asanubsan "asanubsan-testsON" -DCORE_BUILD_TESTS=ON -DCORE_SANITIZE=address,undefined || true
  if [ -f "$B/asanubsan/compile_commands.json" ]; then
    say "20b ASan+UBSan flag proof"
    flag_proof "$B/asanubsan/compile_commands.json" "$B/20-asanubsan-flags.txt" "-fsanitize=address,undefined" "-fno-omit-frame-pointer"
  fi
  if [ -f "$B/cfg-asanubsan.done" ]; then
    if [ -x "$B/asanubsan/tests/asan_canary" ] && ! done_mark canary-asan-wsl "asanubsan-testsON"; then canary_proof "$B/asanubsan/tests/asan_canary" asan-wsl 3 "AddressSanitizer" "asanubsan-testsON"; fi
    # UBSan does not abort by default: halt_on_error=1 turns the trap fatal so the exit code also proves it.
    if [ -x "$B/asanubsan/tests/ubsan_canary" ] && ! done_mark canary-ubsan-wsl "asanubsan-testsON"; then UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 canary_proof "$B/asanubsan/tests/ubsan_canary" ubsan-wsl 3 "runtime error:" "asanubsan-testsON"; fi
    say "engine_tests (excluded from the green suite; measured separately)"
    "$B/asanubsan/tests/engine_tests" > "$B/asanubsan-engine-tests-known.txt" 2>&1
    echo "engine_tests exit=$? (at 8c6837e: 1, FL-1 audio device ENV-08; BUG-SWEEP-06 may have changed this)" | tee -a "$B/asanubsan-engine-tests-known.txt"
    suite_proof asanubsan "asanubsan-testsON" || true
  fi
fi

# ---------- 30 TSan (separate tree: address+thread cannot be combined) ----------
if want 30-tsan; then
  build_cfg tsan "tsan-testsON" -DCORE_BUILD_TESTS=ON -DCORE_SANITIZE=thread || true
  if [ -f "$B/tsan/compile_commands.json" ]; then
    say "30b TSan flag proof"
    flag_proof "$B/tsan/compile_commands.json" "$B/30-tsan-flags.txt" "-fsanitize=thread" "-fno-omit-frame-pointer"
  fi
  if [ -f "$B/cfg-tsan.done" ]; then
    if [ -x "$B/tsan/tests/tsan_canary" ] && ! done_mark canary-tsan-wsl "tsan-testsON"; then
      # TSan needs ASLR disabled on this kernel (ENV-07): the canary runs under setarch too.
      # $(uname -m) is expanded NOW so the runner string holds the real architecture.
      canary_proof "$B/tsan/tests/tsan_canary" tsan-wsl 5 "ThreadSanitizer" "tsan-testsON" "setarch $(uname -m) -R"
    fi
    say "engine_tests under TSan (excluded from the green suite)"
    setarch "$(uname -m)" -R "$B/tsan/tests/engine_tests" > "$B/tsan-engine-tests-known.txt" 2>&1
    echo "engine_tests(tsan) exit=$? (at 8c6837e: 1, FL-1 audio device)" | tee -a "$B/tsan-engine-tests-known.txt"
    # TSan needs ASLR disabled on this kernel (ENV-07): run the whole suite through setarch.
    suite_proof tsan "tsan-testsON" "setarch $(uname -m) -R" || true
  fi
fi

# ---------- 40 clang-tidy on a valid compile db + scope gate ----------
if want 40-tidy; then
  build_cfg tidy "tidy-testsOFF" -DCORE_BUILD_TESTS=OFF || true
  if [ -f "$B/tidy/compile_commands.json" ] && [ -f "$B/cfg-tidy.done" ] && ! done_mark 40-tidy "tidy-testsOFF"; then
    say "40 tidy scope gate (ci/tidy_scope.py)"
    cd "$SRC" || exit 1
    python3 ci/tidy_scope.py "$B/tidy/compile_commands.json" "$SRC" "$B/40-tidy-scope.txt" > "$B/40-tidy-scope-gate.txt" 2>&1
    echo "scope gate rc=$?" | tee -a "$B/40-tidy-scope-gate.txt"
    cat "$B/40-tidy-scope-gate.txt"
    echo "--- negative control (must fail, rc=1) ---" >> "$B/40-tidy-scope-gate.txt"
    python3 ci/tidy_scope.py "$B/tidy/compile_commands.json" "$SRC" "$B/40-tidy-scope-neg.txt" \
      --require app/desktop/src/MainComponent.cpp >> "$B/40-tidy-scope-gate.txt" 2>&1
    echo "negative control rc=$? (expected 1)" | tee -a "$B/40-tidy-scope-gate.txt"

    if command -v clang-tidy >/dev/null 2>&1; then
      say "40b clang-tidy run"
      clang-tidy -p "$B/tidy" \
        --checks="-*,bugprone-*,concurrency-*,performance-*,clang-analyzer-*" \
        --header-filter="(engine|app)/(src|include)/" \
        $(cat "$B/40-tidy-scope.txt") > "$B/40-tidy-round.txt" 2>&1
      echo "clang-tidy rc=$?"
      {
        echo "clang-tidy output: $B/40-tidy-round.txt"
        echo "total warning lines: $(grep -Ec "warning:" "$B/40-tidy-round.txt")"
        echo "errors: $(grep -Ec "error:" "$B/40-tidy-round.txt")"
        echo "--- top files by warning count ---"
        grep -Eo "^[^ ]+\.(cpp|hpp):[0-9]+:[0-9]+: warning:" "$B/40-tidy-round.txt" | cut -d: -f1 | sort | uniq -c | sort -rn | head -15
        echo "--- top check names ---"
        grep -Eo "\[[a-z0-9-]+\]$" "$B/40-tidy-round.txt" | sort | uniq -c | sort -rn | head -15
      } > "$B/40-tidy-summary.txt" 2>&1
      cat "$B/40-tidy-summary.txt"
      mark 40-tidy "tidy-testsOFF" "scope gate + clang-tidy run complete"
    else
      echo "clang-tidy MISSING" > "$B/40-tidy-summary.txt"
    fi
  fi
fi

# ---------- 90 copy logs back to Windows ----------
if want 90-copy; then
  say "90 copy logs to $OUT"
  mkdir -p "$OUT"
  cp -f "$B"/*.txt "$B"/*.done "$OUT"/ 2>/dev/null
  ls -1 "$OUT" | head -40
fi

# ---------- 99 summary ----------
if want 99-summary; then
  say "99 summary"
  {
    echo "Core review environment - WSL run (run-wsl.sh, SHA-bound validation)"
    echo "windows src: $WIN_SRC @ $SHA"
    echo "clone:       $SRC @ $(git -C "$SRC" rev-parse HEAD 2>/dev/null)"
    echo "builds:      $B"
    echo "context:     $CONTEXT"
    echo "steps done:"
    for f in "$B"/*.done; do [ -e "$f" ] && echo "  $(basename "$f" .done): $(tail -1 "$f")"; done
  } > "$B/99-summary.txt" 2>&1
  cat "$B/99-summary.txt"
fi
echo "run-wsl.sh finished"
