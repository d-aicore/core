# Core — work plan (handoff)

Written 2026-11-18 by the coordinator session, for whoever continues this work. Read `AGENTS.md` first (rules,
language, engine quality bar), then this file.

## Owner's mandate (verbatim, 2026-11-18)

- «Quero que melhores a app, motor, perceber conceito corrigir todos os erros bugs melhorar performance. Quero este
  daw funcionar flawless. remove codigo morto, funcionamento que ja nao para ser utilizado. Ideia é ter performance
  top notch e conseguir lidar com varios plugins sejam vst3 ou vst2»
- «eu quero melhor motor que possamos fazer» — the five target criteria are in `AGENTS.md`, "Engine quality bar".
- «a inteçao é corrigir nao fazer uma lista» — fix first; the record of a fix is its commit and its test.

Core has no instruments or effects of its own since 2026-11-16 (`engine/src/dsp/instruments.cpp`,
`engine/src/dsp/effects.cpp`): all sound comes from third-party plugins and audio tracks. A song or tool that still
uses the old built-in instruments is silent by construction, not by a bug.

## Done (on `main`)

| Commit subject | What it fixes |
| --- | --- |
| Engine: M5 OFF por omissao (FL-1) | Live multicore was on by default against the owner's decision D51 |
| Motor/App: a saida de audio que morre e reportada (ART-003) | A dead or rerouted output device is reported and reopened once |
| Dados: Remove unused … (D1) | "Remove unused" no longer trashes audio used by other songs in the same folder |
| Dados: a recuperacao … (D2) | Crash recovery is offered even when the dead Core's PID was reused |
| Dados: o Save pergunta … (D3) | Save asks before overwriting a file another window changed on disk |
| Data: a half-failed ReplaceFileW … (D4) | A partial replace no longer loses the new version |
| Data: an unreadable sample index … (D5) | The sample index is set aside, manual corrections are salvaged |
| Gate: Ninja records header dependencies again | Incremental gates were false-green after header changes |

Not verified in the running app: none of the D1–D5 dialogs or flows were exercised in the GUI; they are proven by
domain tests only. The desktop self-test (`ci\gate.ps1 -Desktop`) was not run.

## The maps (read-only studies of commit e7968c8)

Five studies were delivered in Portuguese and are stored outside the repository, in `D:\CORE\plan\`:
`perda-de-dados.pt.md`, `hosting-de-plugins.pt.md`, `tempo-real.pt.md`, `codigo-morto.pt.md`, `ux-pulse.pt.md`.
**First task for the next agent:** translate them to English into this folder (`data-loss-map.md`,
`plugin-hosting-map.md`, `realtime-map.md`, `dead-code-inventory.md`, `ux-pulse-spec.md`) without losing the
`path:line` references, the verdict labels (confirmed by reading / measured / not verified) or the owner's quotes,
which stay verbatim. Line numbers refer to e7968c8 and have drifted since.

## Next work, in the owner's order (data > real time > performance > compatibility)

1. **Engine hangs forever after a plugin crash** (both engine maps, mechanism measured). `juce_host.cpp:64-67`
   catches the crash with SEH; under `/EHsc` JUCE's process lock stays held, and destroying the instance
   (`juce_host.cpp:101-106`) spins the message thread forever. Minimal fix: when the processor failed by a crash,
   leak it (no `releaseResources`, no `delete`); guard `showEditor`/`parameters`/`setParameter`; offer Restart after
   repeated timeouts. Red test: a `CORE_TEST_CRASH` variant in `app/selftest-plugins` (access violation in
   `process`), then `core-engine render` must end and `plugins.states` must answer.
2. **8 MB message limit vs plugin states** (`server.hpp:60`, `document.cpp:757`): measured, 27 Vital tracks exceed
   it and Save/Export kills the engine. Fix at the root: do not resend state the live instance already has; ask for
   states per key.
3. **Plugin state is lost** — data-loss map items 2, 3, 7, 8: the autosave applies fetched states to a copy only
   (`MainComponent.cpp:6384-6387`); remove/replace/duplicate track and sample-on-track do not fetch state first;
   non-parameter changes do not mark the song dirty (`juce_host.cpp:85`, `:195` ignore `audioProcessorChanged`);
   undo history shrinks to a few steps with large states (`history.cpp:7`, `:20-23`).
4. **A failed plugin is never retried and the UI says otherwise** (FL-2): the app never sends `plugin.retry`;
   export reports success with a muted plugin (`render_job.cpp:295-299`); a plugin that does not load is not
   reported; an invalid state is ignored silently.
5. **Export and installer** — data-loss map items 9, 11, 12 (NaN silenced, false messages, `.previous` orphans,
   installer overwriting edited samples).
6. **`engine.start` restore hole** (FL-3, reproduced): `server.cpp:336-375`, after a failed device switch the engine
   stays silent and `engine.start` answers ok.
7. **Dead code** — lots L1…L10 of the inventory, safest first (L1, L2, L4, L3, L5 = the 16 hidden base templates).
   The 5 visible templates stay until the owner says otherwise. L10: the app opens empty; the engine demo/measure
   are rebuilt on generated audio or the test synth, with an amplitude test in the gate (today no test checks that
   sound comes out). L9 (native scaffolding in `graph.cpp`) last, with a sample-by-sample null test before/after.
8. **Live multicore (M5)** is the path to many plugins (one core saturates at about 15 Twin 3 or 32 Vital; export
   with 8 workers is 5–8× faster) but it has no test at all. Before any soak: a deterministic serial-vs-M5 test with
   a fake plugin provider; fix Stop (`juce_host.cpp:361-377`, `resetAll` waits on the process lock); fix the
   permanent fallback after one stall (`engine.cpp:193-197`). M5 stays OFF by default until the owner decides (D51).
9. **CPU waste**: muted and idle tracks cost full CPU (`graph.cpp:1292`), per-sub-block overhead, plugin block fixed
   at 128 (`juce_host.cpp:22`). Measure before and after; never guess.
10. **VST2**: the owner wants it («quero implementar vst2 tambem»). Facts: all 30 installed VST2 plugins also exist
    as VST3; JUCE 9.0.5 needs Steinberg's `pluginterfaces/vst2.x` headers, absent from this PC — ask the owner
    whether he has an old SDK copy or accepts an interface header written from scratch. Do it after items 1–4,
    behind a build flag; six slices are in the plugin hosting map.
11. **UX from Pulse Studio** (AGPL — behaviours only, never code): the spec proposes detachable windows, piano-roll
    mouse gestures, ghost notes and fit-all, scales, an expandable channel rack, generators, analyzer and fill
    pattern, all with Core's own look. **The owner has not approved the design yet: no UI code before he does.**

## How to work here

- One isolated git worktree per stream; never edit the main checkout at `D:\CORE\CORE` (other sessions work there
  and `ESTADO.md` has uncommitted ledger work that is not yours).
- A bug fix is: a test measured red on the current code, the minimal fix, the test green, the full gate green,
  one commit with explicit paths. Gate: `powershell -ExecutionPolicy Bypass -File ci\gate.ps1 -Build
  D:\Core-build\<name> -Jobs 6`. Never run two gates at once (they share `%TEMP%\core-gate.bat`). A build folder
  configured before the gate fix is refused: delete it.
- Audio tests use real sound (the test synth in `app/selftest-plugins`, or generated audio), null tests against a
  reference, and every sample rate and block size. Auditor findings are leads: confirm each in the code, reading the
  whole block, before treating it as a defect (about half of the automatic findings in this repo were wrong).
- To hear sound from the engine without the app: plugin ids are in `%APPDATA%\Core\plugins.json`; a project with a
  track `instrument: "plugin"` and `core-engine.exe render project.json out.wav` returns the peak.
- Performance tracing: the owner pointed at `melatonin_perfetto`; plan it as a build option that is off by default,
  behind a Core header (the engine core has no JUCE), tracing per callback and per track, not per sub-block.
- Keep at most two or three agents running at the same time.
