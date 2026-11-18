# Report — 2026-11-18 — BUG-SWEEP-19/20: roll preservation (export after reopen, reusable patterns)

## What was measured

- Repository / branch / SHA: `d-aicore/core`, worktree `D:/CORE/CORE/.agents/worktrees/flawless-roll2`,
  branch `flawless-roll2`, baseline `f6b86dc` (integrated main). Fix commits `6b0f044` (BUG-SWEEP-19) and
  `c26dc08` (BUG-SWEEP-20), one per finding; this report is the docs commit on top. Main checkout
  `D:/CORE/CORE` untouched; nothing pushed or merged; no `git stash` used.
- Scope and authorization: BUG-SWEEP-19 and BUG-SWEEP-20 (roll preservation) of the consolidated sweep,
  red-first then minimal fix then full gate. Explicitly NOT in scope: finding 21 (freeze-split, needs owner
  policy) and the BUG-SWEEP-30 follow-up surfaces (pending owner ratification of the envelope policy) —
  neither touched. Product fact honored: Core has no built-in instruments/effects (removed 2026-11-16);
  a roll is a MIDI multi-hit note (`count` + `velocityFrom`→`velocityTo`). No engine or DSP code changed.
- Environment: Windows host (10.0.26300), MSVC VS 18 BuildTools Release / Ninja, JUCE 9.0.5 from
  `D:/Core-deps/JUCE-9.0.5`. Build `D:/Core-build/flawless-roll2`; evidence
  `D:/Core-build/flawless-roll2-evidence/`. Full gate run under the machine-wide `D:/Core-build/GATE-LOCK`
  (stream `flawless-roll2`, timestamped; lock deleted after the run).
- Tests executed: red and green runs of `midi_tests "SWEEP-19"` and `bug_tests "sweep-20"` (name filters)
  plus full suites; ten domain suites green after each fix; full `ci/gate.ps1 -Build
  D:/Core-build/flawless-roll2 -Jobs 6` (merge label); `ctest -L merge -V` for the per-suite counters;
  `core-engine --version`; sha256 of the gate's rendered WAVs against the `f6b86dc` baseline renders.
- Limitations: no UI click-through (the Patterns window template list and the Inspector roll controls were
  read, not clicked); no real-device audio. The template roll proof is domain-level (save/load/apply through
  the real file system in a temp folder). The red runs are on Windows/MSVC at the stated SHAs.

## Finding 19 — Density leaves a stale roll; the export shrinks after reopen (commit 6b0f044)

Confirmed in code at `f6b86dc`:

- `app/domain/src/midi.cpp:244` (`songToMidi`): `const int count = std::max(1, n.roll.count);` — reads the
  payload without `hasRoll`.
- `app/domain/src/pattern_tools.cpp:202` (octave copy), `:233` (ratchet repeat), `:425`
  (`presetFromPattern`): `c.hasRoll = false;` keeps the copied `roll` payload (count 4 in the fixture).
- `app/domain/src/document.cpp:397` (`noteJson`): writes `roll` only when `hasRoll` — so save/reopen drops
  the stale count and the reopened song exports fewer notes than the in-memory one.
- The engine projection (`document.cpp:727`, `toProjection`) also writes `roll` only when `hasRoll`, so the
  engine already plays such a copy as a plain note: the export disagreed with both playback and reopen.

Red, measured at `f6b86dc` + the new tests (before the fix):

```
D:/Core-build/flawless-roll2/tests/midi_tests.exe "SWEEP-19"      (exit 1)
3 testes, 18 verificacoes, 7 falhas
```

Measured failures (verbatim in `evidencia/2026-11-18-roll-preservation/red19-midi.txt`):
stale-roll note exported 10 notes instead of 4 (hit length 204 instead of 960); octave AND ratchet each
exported **20 notes before reopen, 14 after** (`after == before: medido 14, esperado 20`); the density
copies and the preset copy kept `roll.count` 4 with `hasRoll` false. Full suite on the red build:
17 tests / 226 checks / 7 failures — all 7 in the 3 new tests, the 14 existing tests green.

The fix (minimal, `6b0f044`):

- `songToMidi`: `const int count = n.hasRoll ? std::max(1, n.roll.count) : 1;` — the roll counts only when
  `hasRoll` says there is one, exactly as the engine projection serializes it.
- `roll = Roll{}` beside `hasRoll = false` at the three clearing sites (the `stutter.cpp:85,94` precedent).
- The SWEEP-30 control comment that described the old stale-count behavior updated to the new contract.
- No playback change for valid rolls: the existing expansion test (10 notes, hits 32/64/95/127, 85% gate)
  and the single-hit contract (`hasRoll` true, count 0/1 → one hit at `n.velocity`) stay green untouched.

Green: `midi_tests` **17/226/0** (baseline 14/208 + 3 tests / 18 checks), `domain_tests` 30/525/0,
`bug_tests` 39/356/0, `legacy_guard_tests` 9/169/0.

## Finding 20 — Save as Template discards active rolls (commit c26dc08)

Confirmed in code at `f6b86dc`:

- `app/domain/src/pattern_rack.cpp:491` (`rackTemplateFromSlot`): `n.hasRoll = false;` strips the roll.
- The template schema had no roll at all: writer `:445` and reader `:429-435` carry
  tick/length/pitch/velocity only.

Red, measured at `6b0f044` + the new tests (before the fix):

```
D:/Core-build/flawless-roll2/tests/bug_tests.exe "sweep-20"       (exit 1)
2 testes, 38 verificacoes, 7 falhas
```

Measured failures (verbatim in `evidencia/2026-11-18-roll-preservation/red20-bug.txt`): `saved.hasRoll`
false at `rackTemplateFromSlot`; after `saveRackTemplate` → `loadRackTemplates` the rolled note read back
`hasRoll` false, `count` 0 (expected 4), `velocityFrom` 1 (expected 0.25); after `applyRackTemplate` the
slot's pattern came back plain. The legacy round-trip test was green BEFORE the fix (characterization of
the old schema, kept green after).

The fix (minimal, `c26dc08`):

- `rackTemplateFromSlot` no longer strips `hasRoll`/`roll`; ids and `extra` are still stripped (the extra
  strip is deliberate and kept).
- Writer: an OPTIONAL `"roll"` object (`count`, `velocityFrom`, `velocityTo`) when `hasRoll` — the same
  optional shape the engine projection writes. Reader: absent `roll` = no roll; present object parsed with
  the same clamps as the document reader (count 0..64, velocities 0..1, defaults at the note's velocity).
- `applyRackTemplate` already copies template notes whole (`putNotes`), so the roll flows into the slot;
  the BUG-SWEEP-26 swing reconcile and BUG-SWEEP-28 part-first `matchRows` (already in `f6b86dc`) are
  untouched, as is roll30's `normalizeNotes`.
- Header contract comment updated (`pattern_rack.hpp:98`).

Green: `bug_tests` **41/394/0** (baseline 39/356 + 2 tests / 38 checks); all ten domain suites 0 failures
(`evidencia/2026-11-18-roll-preservation/green20-suites.txt`).

Old-catalogue preservation proof (the second new test, green before AND after the fix):

- A template JSON exactly as the old writer produced it (no `roll` anywhere) loads; every note has
  `hasRoll` false and `roll.count` 0.
- `rackTemplateToJson(parsed) == json::parse(legacy)` — key for key, nothing added.
- Loaded from disk and re-saved: the file equals the legacy form except the collision-renamed id
  (`old` → `old-2`, the existing never-overwrite dedup); no `roll` substring in the bytes.
- Applied into a song: plain notes, as before.
- The shipped `templates/*.core-template.json` are song templates (a different schema, not touched);
  `legacy_guard_tests` (9/169) pins them and the catalogue green.

## Sound

No engine/DSP change. The gate's rendered fixtures were compared with the `f6b86dc` baseline renders
(`D:/Core-build/integration/tests/cli/`):

- `demo.wav`: sha256 `77507ee8682b963b913bed7f5f683b994125b3aca6649424fd6020bdcb4d7912` — byte-identical
  to the baseline, **0.00 dB**.
- `plugin-crash.wav`: sha256 `cfb52d1a58e61c08655d2d6e8bc953266af972397530a47421e973d2c980d523` —
  byte-identical to the baseline, **0.00 dB**.

The only behavior changes are data restoration: 19 makes the export of an INVALID stale-roll state agree
with what the engine plays and with the reopened song; 20 makes a user's saved rolls sound again when the
template is applied (the work that was being discarded). Valid-roll playback and export are unchanged.

## Full gate (merge)

```
powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-roll2 -Jobs 6
```

- `gate-resumo.txt`: **VERDE**, `100% tests passed out of 20` (merge), 337 s, code tip `c26dc08`, exit 0.
- Per-suite counters (`ctest -L merge -V`, exit 0): **207 cases / 3115 verifications / 0 failures** —
  engine 25/130, domain 30/525, legacy_guard 9/169, role 21/216, midi 17/226, bug 41/394, bitwig 2/25,
  audit 7/65, audit2 6/80, hunt 13/647, round3 7/82, calls 12/69, menu 7/366, engine_process 10/121.
  Baseline `f6b86dc` 202/3059: additions only (+3/+18 midi, +2/+38 bug); no suite shrunk.
- `core-engine --version`: `0.7.0+c26dc085c0` == HEAD `c26dc085c040663e5654ad586272420f0bd42895`,
  no `.modified` (the version header ignores untracked files, `--untracked-files=no`).
- This first gate run had the untracked report present, so the resumo notes "com alteracoes por commitar";
  the binary is exactly `c26dc08`. A clean-tree re-gate at the report commit is recorded in the follow-up
  docs commit (resumo archived alongside).

## Changes

- `6b0f044` — `app/domain/src/midi.cpp` (hasRoll gate in `songToMidi`), `app/domain/src/pattern_tools.cpp`
  (roll payload reset at the three `hasRoll = false` sites), `tests/midi_tests.cpp` (+3 tests, +1 include,
  SWEEP-30 control comment).
- `c26dc08` — `app/domain/src/pattern_rack.cpp` (strip removed; optional roll in writer/reader),
  `app/domain/src/pattern_rack.hpp` (contract comment), `tests/bug_tests.cpp` (+2 tests).
- Documentation (this commit): this report, one index row in `docs/relatorios/README.md`, run logs in
  `docs/relatorios/evidencia/2026-11-18-roll-preservation/`. Build evidence (outside the tree):
  `D:/Core-build/flawless-roll2-evidence/` (configure/build logs, red/green run logs, gate log and resumo,
  per-suite counts, version output, render hashes).

## Verification

Commands and exit codes (all on the Windows host, MSVC Release):

| Step | Command | Result |
| --- | --- | --- |
| 19 red | `midi_tests.exe "SWEEP-19"` @ f6b86dc+tests | exit 1 — 3 tests / 18 checks / 7 failures |
| 19 red full | `midi_tests.exe` @ f6b86dc+tests | exit 1 — 17/226/7 (only the new tests fail) |
| 19 green | `midi_tests.exe`, `domain_tests.exe`, `bug_tests.exe`, `legacy_guard_tests.exe` | exit 0 — 17/226/0, 30/525/0, 39/356/0, 9/169/0 |
| 20 red | `bug_tests.exe "sweep-20"` @ 6b0f044+tests | exit 1 — 2 tests / 38 checks / 7 failures |
| 20 green | ten domain suites (see green20-suites.txt) | exit 0 — 0 failures everywhere |
| Gate | `powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-roll2 -Jobs 6` | exit 0 — VERDE 20/20, 337 s |
| Counts | `ctest --test-dir D:/Core-build/flawless-roll2 -L merge -V --output-on-failure` | exit 0 — 20/20, 207/3115/0 |
| Version | `D:/Core-build/flawless-roll2/engine/core-engine.exe --version` | `0.7.0+c26dc085c0` == HEAD, no `.modified` |

Coverage vs the `f6b86dc` baseline (202 cases / 3059 verifications / 0 failures, merge 20/20, measured in
`D:/Core-build/integration-evidence/per-suite-counts-finaltip2.txt`): additions only — midi_tests +3/+18,
bug_tests +2/+38; no test removed, no criterion relaxed.

## PROPOSED ESTADO.md lines (D72/D80 — the scribe writes them, not this delivery)

Under Estado medido / bugs (BUG namespace, D75):

- `BUG-SWEEP-19 (roll preservation) corrigido @6b0f044 (branch flawless-roll2, base f6b86dc): songToMidi
  lê o roll só quando hasRoll o declara (como a projecção do motor) e Density octave/ratchet +
  presetFromPattern limpam o payload com a flag (roll=Roll{}, precedente stutter). Vermelho medido:
  7 falhas em 3 testes novos — export 20 notas antes do reopen, 14 depois (octave e ratchet); verde
  midi_tests 17/226/0. Rolls válidos inalterados (expansão e contrato single-hit verdes).`
- `BUG-SWEEP-20 (roll preservation) corrigido @c26dc08 (branch flawless-roll2, base f6b86dc):
  rackTemplateFromSlot deixa de descartar hasRoll/roll e o esquema de templates ganha um objecto "roll"
  OPCIONAL (count, velocityFrom, velocityTo); ausente = sem roll, os templates antigos carregam e
  re-gravam chave-a-chave inalterados (prova de round-trip legacy verde antes e depois). Vermelho medido:
  7 falhas em 2 testes novos (roll perdido no save/load/apply); verde bug_tests 41/394/0. Extra continua
  descartado; swing reconcile (26) e matchRows part-first (28) por tocar.`
- Gate line: `Gate merge VERDE 20/20 @ c26dc08 (337 s), 207 casos / 3115 verificações / 0 falhas (base
  f6b86dc 202/3059; +3/+18 midi_tests, +2/+38 bug_tests, nada reduzido), versão 0.7.0+c26dc085c0 == HEAD
  sem .modified; renders do gate byte-idênticos à base (demo.wav 77507ee8…, plugin-crash.wav cfb52d1a…,
  0.00 dB). Relatório: docs/relatorios/2026-11-18-roll-preservation-19-20.md.`
- Pendência (owner = Diogo): integração do branch `flawless-roll2` (2 commits + relatório) sobre o main
  integrado; as regiões tocadas (midi.cpp songToMidi, pattern_tools.cpp density/preset, pattern_rack.cpp
  template save/schema) são distintas das de pattern26-28 e roll30, já presentes na base f6b86dc.
  BUG-SWEEP-21 (freeze-split) e as superfícies de seguimento do 30 ficam de fora, à espera da política do
  dono. O escrivão regista as entradas e a pendência de integração.

## Pending and handoff

- Test Engineer acceptance: run the new tests by filter (`midi_tests.exe "SWEEP-19"`,
  `bug_tests.exe "sweep-20"`); in the app, save a slot with an active roll as a template, load it into
  another song and confirm the roll sounds (count and ramp); export a song after Density octave/ratchet,
  reopen and re-export — the note counts must match. Old `.core-patterns.json` files must keep loading.
- Integration: base is the integrated main `f6b86dc`; no rebase of pattern26-28/roll30 needed (already in
  the base, their changes preserved). Nothing pushed; the PM integrates.
- Residual risk: the template schema carries only count/velocityFrom/velocityTo (not `roll.extra`), the
  same deliberate extra-strip as note `extra`; a future schema that keeps roll extras would be additive.
  The reader is lenient (defaults + clamps) like the rest of the template reader; a malformed `roll`
  value of the wrong JSON type is reported through the existing `problems` list / safe-write check.
- Not verified here: UI click-through of the Patterns window template preview with rolled notes (the
  preview draws notes; a rolled note draws as its base note — cosmetic, no data loss), real-device audio.