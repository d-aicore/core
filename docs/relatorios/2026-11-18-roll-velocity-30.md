# Report — 2026-11-18 — BUG-SWEEP-30: velocity edits scale a roll's played envelope

## What was measured

- Repository / branch / SHA: `d-aicore/core`, worktree `.agents/worktrees/flawless-roll`, branch `flawless-roll`.
  Base `ed4642e61b78e30fc4f7d950a28a214d0ac7c801` (main). Fix commit `133430e`, CI unique-bat cherry-pick `a574da0`
  (from `fffbb41`). Main checkout `D:\CORE\CORE` untouched; nothing pushed; no `git stash` used.
- Scope and authorization: PM task BUG-SWEEP-30 (roll velocity), completing the prior agent's red test + archived
  owner evidence. Owner's external audit (2026-11-18, archived verbatim in
  `docs/relatorios/evidencia/2026-11-18-roll/`, sha256 below) proved the bug; the repair policy (proportional,
  shape-preserving, realized factor) was set by the PM brief with two nuances flagged for the owner.
- Product fact: Core has no built-in instruments/effects (removed 2026-11-16). A roll is pure MIDI data:
  `Roll` (`app/domain/src/document.hpp:47`) with `count` + `velocityFrom`/`velocityTo`; the engine
  (`engine/src/core/graph.cpp:372-379`) and the MIDI exporter (`app/domain/src/midi.cpp:244-249`) interpolate
  hit velocity `from + (to - from) * i / (count - 1)` when `count > 1`. Base `Note.velocity` is dead data for
  multi-hit rolls. No DSP involved or added.
- Environment: Windows host (10.0.26300), MSVC VS 18 BuildTools Release / Ninja, CMake + ctest, JUCE 9.0.5 from
  `D:/Core-deps/JUCE-9.0.5`. Build `D:/Core-build/flawless-roll`; evidence `D:/Core-build/flawless-roll-evidence/`.
- Limitations: proof is at the document/MIDI-export level (executed) plus code inspection of the engine consumer
  (same interpolation formula); no real-plugin listening test. The six UI surfaces are wired to the shared helper
  (confirmed in code) but the merge gate runs no interactive UI test; desktop self-test was not run in this task.

## Owner evidence (verbatim, archived)

`docs/relatorios/evidencia/2026-11-18-roll/edit.log` (owner's probe output, archived verbatim):

```
roll louder changed=1 displayedVelocity=0.57 exportedMidiUnchanged=1
ordinary louder midiChanged=1
```

The audit report (`report (5).md`, same archive) states the repair contract: "Repair should define how a
base-velocity edit scales an existing roll envelope, including clipping and zero endpoints, then update all
editing paths consistently. Preserve the envelope's intentional shape rather than flattening it without a
decision. The existing single-note velocity contract and ordinary-note control must remain valid."

Archive sha256 (verified at intake, re-verified here):

| File | sha256 |
| --- | --- |
| edit-probe.cpp | b578c3b86e4f08fe9a5da48d845a011afca2a15aff36eb07d709ef200ce7e284 |
| edit.log | 9601f4e62ae7bf928b07a6544e2b0e3a5442a7fdb5713d062ef7339e57a0452a |
| report (5).md | 02527968231c1cbe3c5e13774653751914269523a975be963dedc25ec7e55847 |

## The realized-factor rule (documented for the owner)

Louder's nominal factor is 1.15, but the base write rounds to 2 decimals: `0.5 * 1.15 * 100 = 57.4999...`
rounds to 57, so the base lands on 0.57 and the factor it ACTUALLY moved by is `0.57 / 0.5 = 1.14` (matches the
owner's edit.log). The fix scales `velocityFrom`/`velocityTo` by that realized factor, uniformly across
proportional (Louder/Softer) and absolute-set (lane, wheel, Inspector, Patterns panel, Normalize) surfaces:
`factor = newBase / oldBase`, endpoints multiplied and clamped to [0,1], ramp shape preserved. Softer on 0.5
lands on 0.43 (`round(42.5) = 43`), realized factor 0.86. Envelope endpoints keep full precision (no 2dp
rounding); only the base keeps each surface's existing rounding/clamping.

Implementation: one shared inline helper `doc::setNoteVelocity(Note&, double)` in `app/domain/src/document.hpp`
(after the `Note` struct). It sets the base exactly as before and scales the envelope only when
`roll.count > 1 && oldBase > 0 && newBase != oldBase`. A base of 0 (legal from a file, `document.cpp:119` range
[0,1]) has no realized factor: the envelope stays untouched. `count <= 1`: envelope is dead data, untouched
(single-note contract). The guard reads `roll.count` alone, exactly like the consumers — BUG-SWEEP-19 semantics
are not altered.

## Surfaces changed (all base-write surfaces in the authorized list)

| Surface | File:line (base ed4642e anchors) | Change |
| --- | --- | --- |
| scaleVelocities (Louder 1.15 / Softer 0.85) | app/domain/src/groove.cpp:159-167 | base write -> setNoteVelocity |
| Louder/Softer menu (calls scaleVelocities) | app/desktop/src/PatternEditor.cpp:1803-1805 | covered via groove.cpp |
| Velocity lane drag | app/desktop/src/PatternEditor.cpp:2055 | absolute set -> setNoteVelocity |
| Velocity wheel (+/-0.05) | app/desktop/src/PatternEditor.cpp:2116 | absolute set -> setNoteVelocity |
| Inspector velocity slider | app/desktop/src/InspectorPanel.cpp:81 | absolute set -> setNoteVelocity |
| Patterns panel step velocity | app/desktop/src/PatternsWindow.cpp:827 | absolute set -> setNoteVelocity |
| normalizeNotes (equal / accent) | app/domain/src/pattern_tools.cpp:59-88 | absolute set -> setNoteVelocity |

Consumers (`midi.cpp:244-249`, `graph.cpp:372-379`) untouched. No change to note creation/generation paths.

## Red (measured before the fix, at ed4642e + inherited red test)

Command (MSVC env via vcvars64, VSLANG=1033, chcp 65001; helper bat in the evidence dir):

```
cmake --build D:/Core-build/flawless-roll -j 4 --target midi_tests
D:\Core-build\flawless-roll\tests\midi_tests.exe "SWEEP-30"
```

Result: **2 tests, 44 checks, 14 failures, exit 1** (`red-sweep30.log`). Test 1 FAIL, test 2 (ordinary control +
single-hit contract) ok already at red. Key measured values:

- `roll.velocityFrom` stayed 0.25 (expected 0.285); `velocityTo` stayed 1.0 under Softer (expected 0.86).
- Exported MIDI byte-identical after Louder (`before != after` failed) — the owner's exact symptom.
- Exported hit velocities stuck at **{32, 64, 95, 127}** after both Louder and Softer (both bars).

Review of the inherited red test found two test defects, fixed minimally (not a rewrite; coverage unchanged):
(1) `const auto& n = songToMidi(d).tracks[0].notes;` bound a reference into a destroyed temporary
(`operator[]` breaks lifetime extension) — the first red run segfaulted (exit 139) after the genuine failures;
now a named `MidiFile` copy, matching the file's existing style. (2) Bar-2 hits were indexed `n[5+i]`, but
`n[5]` is bar 2's ordinary note (the existing test asserts `n[5].tick == kBar`); corrected to `n[6+i]`.
The clean red above was re-measured with the corrected test before any product change.

## Green (after the fix)

Same command: **2 tests, 44 checks, 0 failures, exit 0** (`green-sweep30.log`).

- Louder: base 0.5 -> 0.57, envelope 0.25..1 -> 0.285..1 (to clamped); exported hits **{36, 66, 97, 127}** in
  both bars; the ordinary note at 1.0 stays 127.
- Softer: base 0.5 -> 0.43, envelope 0.25..1 -> 0.215..0.86; exported hits **{27, 55, 82, 109}**; the ordinary
  note scales too (0.85 * 127 = 108).
- Ordinary control (hasRoll=false AND roll={}): Louder still changes the export, 0.57 * 127 = 72, full length
  kPpq (not 85% roll steps).
- Single-hit contract (count 0 and 1, hasRoll true): base 0.57, envelope untouched at 0.25/1.0, one hit at 72,
  length kPpq.

Full suites before commit (`suites-precommit.log`, exit 0): **midi_tests 10 tests / 159 checks / 0 failures**,
**domain_tests 19 tests / 148 checks / 0 failures**.

## Full gate (GATE-LOCK held for this stream)

GATE-LOCK created at 2026-11-18T06:38:53+00:00 (stream=flawless-roll), removed after the run. Command:

```
powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-roll -Jobs 6
```

**VERDE** — `gate-resumo.txt`: "Gate do Core: VERDE / Codigo: D:\CORE\CORE\.agents\worktrees\flawless-roll @
a574da0 (com alteracoes por commitar) / Testes: merge / Tempo: 213 s / 100% tests passed out of 17".
Exit 0. ctest merge total test time 15.28 s.

- Version == HEAD: `core-engine --version` prints **`core-engine 0.7.0+a574da0ba0`** — HEAD `a574da0`
  (short=10 `a574da0ba0`), **no `.modified`**. The "(com alteracoes por commitar)" note in the resumo is the
  untracked owner-evidence archive and this report, which the task flow commits AFTER the gate; the build-id
  check uses `git status --porcelain --untracked-files=no` (`cmake/core_version.cmake:24`), so untracked docs
  never mark the build modified. No tracked file was uncommitted at gate time.
- GATE-LOCK: this stream's lock (06:38:53+00:00) was removed after the run; the lock present afterwards belongs
  to the `flawless-preservation` stream (06:43:07Z) and was left untouched.
- Per-suite counts measured with `ctest --test-dir D:/Core-build/flawless-roll -L merge -V --output-on-failure
  --timeout 600` (exit 0, `ctest-merge-verbose.log`):

| Suite | Cases | Verifications | Failures | vs ed4642e |
| --- | --- | --- | --- | --- |
| engine_tests | 25 | 130 | 0 | unchanged |
| domain_tests | 19 | 148 | 0 | unchanged |
| role_tests | 21 | 216 | 0 | unchanged |
| midi_tests | 10 | 159 | 0 | +2 / +44 (SWEEP-30) |
| bug_tests | 22 | 138 | 0 | unchanged |
| bitwig_tests | 2 | 25 | 0 | unchanged |
| audit_tests | 7 | 65 | 0 | unchanged |
| audit2_tests | 6 | 80 | 0 | unchanged |
| hunt_tests | 14 | 649 | 0 | unchanged |
| round3_tests | 7 | 82 | 0 | unchanged |
| calls_tests | 10 | 44 | 0 | unchanged |
| engine_process_tests | 4 | 49 | 0 | unchanged |
| **Counting suites total** | **147** | **1785** | **0** | **+2 / +44** |
| cli_version / cli_demo_render / cli_measure / cli_unicode / cli_plugin_crash_render | pass | n/p | 0 | unchanged |
| **ctest merge label** | **17/17 pass** | | **0** | |

## Coverage vs the measured baseline

Baseline `docs/relatorios/2026-11-18-estado-ctests.md` (flawless-estado stream) at ed4642e: merge label 17/17,
counting suites **145 cases / 1741 verifications / 0 failures**; midi_tests 8/115; engine_process_tests 4/49.
This delivery ADDS 2 cases / 44 verifications (both in midi_tests): measured at the gate build **147 / 1785 / 0**,
midi_tests 10/159, engine_process_tests unchanged 4/49, merge 17/17 (table above). No coverage removed, no
criterion relaxed.

## Rendered audio

No rendered fixture contains a roll envelope: no template JSON has `velocityFrom` (checked all `templates/*.json`;
the "roll" strings in psycore-tremolo are automation lane names), and no merge test applies a velocity edit before
rendering. Ordinary notes are sound-neutral by construction: the base write stores the identical value and the
envelope is only touched when `count > 1`. Hash comparison of the gate's rendered WAVs against the ed4642e
baseline (`D:/Core-build/gate-ed4642e/tests/cli/`):

| WAV | ed4642e baseline sha256 | gate a574da0 sha256 | Delta |
| --- | --- | --- | --- |
| demo.wav | 77507ee8682b963b913bed7f5f683b994125b3aca6649424fd6020bdcb4d7912 | 77507ee8682b963b913bed7f5f683b994125b3aca6649424fd6020bdcb4d7912 | byte-identical, 0.00 dB |
| plugin-crash.wav | cfb52d1a58e61c08655d2d6e8bc953266af972397530a47421e973d2c980d523 | cfb52d1a58e61c08655d2d6e8bc953266af972397530a47421e973d2c980d523 | byte-identical, 0.00 dB |
| saída-ação-漢字.wav | 767c72c3942af31ca049e93e04117b6d706096202fe830f326870181627e8b13 | 767c72c3942af31ca049e93e04117b6d706096202fe830f326870181627e8b13 | byte-identical, 0.00 dB |

All rendered audio is byte-identical to the baseline: sound-neutral, as expected (no roll fixture is rendered,
so there is no intended Louder audio delta to report; the intended effect is proven on the exported MIDI hit
velocities above).

## Owner policy flags (decisions pending — implemented defaults are proportional)

1. **Realized factor** (implemented, documented above): the envelope follows the factor the base actually moved
   by (1.14 for Louder on 0.5), not the nominal 1.15. Flagged so the owner can ratify the rule.
2. **Zero endpoints**: [0,1] is legal for envelope endpoints (`document.cpp:123-124`); proportional scaling keeps
   a 0 endpoint silent (0 * factor = 0). Accepted default. The base keeps its 0.05 floor on the groove/wheel
   surfaces; an endpoint floor (e.g. 0.05) is UNDECIDED — owner decision.
3. **Normalize accent/equal on rolled notes**: implemented default scales the envelope proportionally (shape
   preserved). The alternative — flatten the envelope to the normalized base (all hits equal) — is an owner
   decision; not implemented.

## Remaining work and risks

- Same-class base-write surfaces found by grep but OUTSIDE the authorized list (not changed, flagged for a
  follow-up decision): `humanizeNotes` (groove.cpp:47), `varyNotes` "accent" (groove.cpp:90), the velocity-ramp
  tool (pattern_tools.cpp:264), `follow.cpp:231/262`, `pattern_rack.cpp:233`. They move the base of existing
  notes and would leave a roll envelope unscaled; generation/creation paths (pcore.cpp, edit.cpp note creation,
  midi.cpp import) create fresh notes and are not affected.
- BUG-SWEEP-19 (stale roll: consumers read `roll.count` alone; clearing a roll must clear hasRoll AND roll={})
  is a separate fix and was NOT altered; the red test's control clears both, as the owner's probe did.
- BUG-SWEEP-29 (tempo-only Randomize deletes unarranged ideas, same owner report) remains open, separate task.
- UI surfaces are confirmed in code only; a desktop self-test / manual pass over Louder on a roll in the piano
  roll is recommended for acceptance (Test Engineer).

## PROPOSED ESTADO.md lines (ledger scribe only — D72/D80; not written here)

Estado medido:

- "BUG-SWEEP-30 corrigido @133430e (branch flawless-roll; gate a574da0 = ed4642e + fix + cherry-pick de fffbb41):
  Louder/Softer, lane, wheel, Inspector, Patterns e Normalize passam a escalar o envelope tocado de um roll
  (count > 1) pelo factor realizado da base (0.57/0.5 = 1.14 no Louder), proporcional, clamp [0,1]; contrato de
  nota simples e semântica de BUG-SWEEP-19 preservados. Vermelho medido: 14 falhas, MIDI exportado byte-idêntico
  após Louder (hits 32/64/95/127); verde: 44/44 (hits 36/66/97/127 Louder, 27/55/82/109 Softer). Gate VERDE,
  merge 17/17, 147 casos / 1785 verificações / 0 falhas (baseline ed4642e: 145/1741). Fonte: evidência do dono
  em docs/relatorios/evidencia/2026-11-18-roll/edit.log («roll louder changed=1 displayedVelocity=0.57
  exportedMidiUnchanged=1»); relatório docs/relatorios/2026-11-18-roll-velocity-30.md."

Pendências:

- "BUG-SWEEP-30 — decisão do dono: (1) ratificar a regra do factor realizado; (2) floor para endpoints a zero do
  envelope (default implementado: 0 fica mudo, proporcional); (3) Normalize accent/equal em rolls: manter forma
  (default implementado) vs achatar. Enquanto não decidido, os defaults proporcionais estão em vigor."
- "BUG-SWEEP-30 — follow-up: superfícies da mesma classe fora do âmbito autorizado (humanizeNotes groove.cpp:47,
  varyNotes accent groove.cpp:90, rampa de velocidade pattern_tools.cpp:264, follow.cpp:231/262,
  pattern_rack.cpp:233) ainda escrevem só a base; decidir se entram numa próxima ronda."
- "BUG-SWEEP-29 (Randomize tempo apaga ideias não arranjadas) continua aberto — tarefa separada, mesma evidência."

## Commands and exit codes (actual runs)

| Step | Command | Exit |
| --- | --- | --- |
| Red build+run | build-run.bat (vcvars64 + cmake --build ... --target midi_tests + midi_tests.exe "SWEEP-30") | 1 (14 failures) |
| Green build+run | same | 0 (0 failures) |
| Suites pre-commit | run-suites.bat (midi_tests + domain_tests, full) | 0 |
| Full gate | powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-roll -Jobs 6 | 0 (VERDE, 17/17) |
| Counts | ctest --test-dir D:/Core-build/flawless-roll -L merge -V --output-on-failure --timeout 600 | 0 (147/1785/0) |

Logs: `D:/Core-build/flawless-roll-evidence/{red-build.log, red-sweep30.log, green-sweep30.log,
suites-precommit.log, gate-run.log}`; gate summary `D:/Core-build/flawless-roll/gate-resumo.txt`.