# Report — 2026-11-18 — Cleanup of the legacy module surfaces (W-Core / F-Core / R-Core / D-Core)

## What was measured

- Repository `d-aicore/core`, worktree `D:/CORE/CORE/.agents/worktrees/flawless-modules`, branch `flawless-modules`,
  base **`f6b86dc`** (integrated main). Commits: `d438d4a` (characterization tests, red), `372d24b` (domain/bridge/lang),
  `b909ee8` (desktop surfaces + self-test), plus this docs commit.
- Authorization: owner, verbatim (2026-11-18, this dispatch): «apenas o pattern maker,mixer, piano, tools. o que esta
  a causar erros limpa, w-core, d-core f-core, tudo isso ja nao entra. se existirem templates com efeitos antigos remove».
  Product direction: `docs/DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md` (PROD-02/04/06). The template removal is a separate
  stream (`flawless-clean`); **templates were NOT touched here**.
- Environment: Windows 11 x64, MSVC VS 18 BuildTools (14.51.36231), CMake 4.4.4 + Ninja, Release, JUCE 9.0.5 from
  `D:/Core-deps/JUCE-9.0.5`, build `D:/Core-build/flawless-modules`, evidence `D:/Core-build/flawless-modules-evidence/`.
  Gates under the machine-wide `D:/Core-build/GATE-LOCK` (created with stream + timestamp, deleted after each run).
- Tests executed: red-first characterization (`modules_tests` new, `role_tests`/`round3_tests` expectations followed the
  rename), the full merge gate and the desktop gate (`ci/gate.ps1`, unique-bat fix in base), plus a direct
  `Core.exe --self-test` run for the report capture.
- Limitations: the four kept surfaces are proven by the registered desktop self-test (null audio device, test synth)
  and domain suites — not by a human UI session. Real-plugin load, WASAPI output and long-run stability are not
  re-verified here (unchanged engine/hosting code).

## Removed / renamed table (legacy module titles)

| Legacy surface (at `f6b86dc`) | Disposition | New user-facing name | Old identifiers kept as lookup aliases |
| --- | --- | --- | --- |
| `P-Core` card title (`core_plugins.cpp:18`) | renamed | **Pattern Maker** (word "Patterns") | id `pcore`; aliases `p-core`, `patterns`, `pattern core`, `channel rack`; `findCorePlugin("P-Core")` resolves |
| `W-Core` card title (`core_plugins.cpp:20`) | renamed | **Instrument** (word "By role") | id `wcore`; aliases `w-core`, `w-core synth`, `synth`, `role instrument` |
| `F-Core` card title (`core_plugins.cpp:22`) | renamed | **Effects** (word "By intent") | id `fcore`; aliases `f-core`, `fx`, `fx core`, `intent` |
| `R-Core` card title (`core_plugins.cpp:24`) | renamed | **Reverb** (word "Shared bus") | id `rcore`; aliases `r-core`, `reverb core`, `shared reverb`, `space` |
| `D-Core` card title (`core_plugins.cpp:26`) | renamed | **Delay** (word "Shared bus") | id `dcore`; aliases `d-core`, `delay core`, `shared delay`, `echo` |
| Tool menu labels `"F-Core: <intent>"`, `"R-Core <id>"`, `"D-Core <id>"` (`MainComponent.cpp` coreToolMenu/askIntentZone) | renamed | `"Effects: <intent>"`, `"Reverb <id>"`, `"Delay <id>"` | undo labels follow; lang key `Effects: {}` |
| `"P-Core: the Patterns window…"` (coreToolsMenu) | renamed | `"Pattern Maker: the Patterns window…"` | — |
| New shared-bus names `"R-Core · X"` / `"D-Core · X"` (`role_tools.cpp` busNameFor) | renamed for NEW buses | `"Reverb · X"` / `"Delay · X"` | legacy buses keep their stored names; found by the `corePlace` tag, reused, never duplicated |
| Tool sentences `"R-Core room: …"` / `"D-Core dotted: …"` | renamed | `"Reverb room: …"` / `"Delay dotted: …"` | — |
| AI catalog text `(R-Core / D-Core)`, `(F-Core)`, `reverb (R-Core) or delay (D-Core)` (`role_tools.cpp` songToolCatalog) | renamed | plain descriptions | tool ids `space`/`fx` unchanged |
| `"W-Core Synth (old, silent…)"` rack label (`PatternsWindow.cpp:31`) | renamed | `"Core Synth (old, silent…)"` (as the other legacy sounds) | engine identifier `wave` unchanged |
| Patterns-window menus/messages `"P-Core shape"`, `"Key for P-Core"`, `"Another take (P-Core)"`, `"P-Core: …"` | renamed | `"Pattern Maker …"` | undo labels follow; lang key `Pattern Maker: {}` |
| `"P-Core catalogue not read: …"` log (`MainComponent.cpp:2011`) | renamed | `"Pattern Maker catalogue not read: …"` | lang key follows |
| Error texts `write one with P-Core` (`score.cpp:248`), `the P-Core catalogue is missing` / `a part P-Core knows` (`pattern_rack.cpp:582/593`) | renamed | `… Pattern Maker …` | — |
| New track of role `wave` named `"W-Core"` (`edit.cpp:62` builtinInstruments label) | renamed | `"Synth"` (the instrumentCatalogue's own name) | id `wave` unchanged; no built-in sound returns (`kind "none"`) |
| Bridge `open what:"wcore"` — comment promised `-> "instrument"`, code did not (`calls.cpp:565-568`) | **fixed** (dead alias) | resolves to `instrument` | `pcore`/`stutter` → `patterns`, `stage` → `score` unchanged; `fcore`/`rcore`/`dcore` are tool-only: clear refusal, no window |
| Dead `pcore` branch of `coreToolMenu` (unreachable: window cards open directly, menus special-case it) | removed | — | no behavior change; the menu item lives in `coreToolsMenu` |

**Not removed (functional surfaces, per the dispatch):** SCorePanel (score, via `openCoreWindow("score")`) and
BuildupPanel (via PatternEditor) — files untouched. The nine tool cards keep their ids, order, roles and functions;
only the titles/words changed. SUCK / PUM / Build-up / ROLL cards were already plain-named and are unchanged.

## Kept surfaces + alias proof

- **Pattern Maker** — the Patterns window (`Core · Patterns` title, unchanged), the `pcore` card now titled
  "Pattern Maker"; catalogue `pcore/styles.json` and the kick/bass/cymbal alignment untouched (`patterns/`, `pcore/`,
  `pcore.cpp`, `pattern_rack` logic unchanged apart from two error strings).
- **Mixer** — untouched (`Mixer.cpp/h`); self-test steps 36–37 open it and check the strips.
- **Piano roll** — untouched (`PatternEditor.*`); self-test steps 16–19 edit notes, drag, undo.
- **Tools** — the panel and menus keep every tool; self-test step 38 now opens the panel, checks the card count and
  that **no card title contains "Core"**, and clicks the Pattern Maker card's handler (opens the window).
- **Alias proof (tested):** `modules_tests` resolves `wcore/W-Core/w-core/W-CORE`, `fcore/F-Core/f-core`,
  `rcore/R-Core/r-core`, `dcore/D-Core/d-core`, `pcore/P-Core/p-core/Pattern Maker` to the same ids;
  `corePluginListProblems()` empty (no duplicate id/alias). The pre-existing card test
  (`role_tests.cpp:405`, `findCorePlugin("R-Core")->id == "rcore"`) passes unchanged — it is now an alias test.
- **Legacy load (tested):** a legacy project with a builtin `wave` instrument + synth links, an `fcore`-tagged
  effect and a bus named `R-Core · Room` opens, keeps every datum, saves/reopens byte for byte, and the tool reuses
  the old bus (`modules_tests`, plus the pre-existing `legacy_guard_tests` 9/169 unchanged).

## Consumer map (summary)

Mapped every `W-Core|F-Core|R-Core|D-Core|P-Core` reference at `f6b86dc` (78 files matched incl. docs/history):

- **Cards/menus/titles (removed/renamed):** `core_plugins.cpp/hpp` (registry), `PluginCards.h` (draws `shortName`/`word`
  — auto-follows; accessor added), `MainComponent.cpp` coreToolMenu/coreToolsMenu/openCorePlugin/askIntentZone/Plugins
  menu (:5338, auto-follows via `shortName`), `PatternsWindow.cpp` (rack label + shape menus/messages),
  `role_tools.cpp/hpp` (bus names, sentences, catalog), `calls.cpp/hpp` (open aliases, description, stale comments),
  `edit.cpp:62` (wave label), `score.cpp:248`, `pattern_rack.cpp:582/593` (error texts), `lang/pt-PT.json` (3 live keys
  renamed + `same` list entry).
- **Lookup aliases / legacy data (kept):** ids `pcore/wcore/fcore/rcore/dcore`; the `fcore` JSON tag on old effects
  (`calls.cpp` chainJson/validateProposal still skip it); `corePlace` bus tag; builtin instrument identifiers
  (`wave`, `kick`, `sampler`…) in `document.cpp/.hpp`, engine `project.cpp/graph.cpp` (silent conversion + load notes);
  `wcoreMood` extra key (`edit.cpp:1752`); synth/modulation data paths.
- **Functional panels (kept, untouched):** `SCorePanel.*`, `BuildupPanel.*`, `Mixer.*`, `PatternEditor.*`,
  `InspectorPanel.*` (legacy knob catalogue still renders old silent tracks; its label for `wave` was already "Synth").
- **Historical comments / owner quotes (kept):** `BuildupPanel.h`, `PatternEditor.h:141`, `GlobalKeys.h:5`,
  `document.hpp:104-120`, `edit.hpp:186`, `roles.cpp:254`, `score.hpp`, `PatternsWindow.h` header, engine comments —
  they document legacy data semantics or quote the owner; not user-visible module surfaces.
- **Left for the owning streams (documented, not touched):** `template_sheet.cpp:77` `"Modulação (W-Core):"` (template
  ficha output — template-adjacent, `flawless-clean` owns templates; also pre-existing PT output, conversion belongs
  with that stream's touch); `builtinInstruments()` menu consumers in `MainComponent.cpp:4604-4761`
  (`flawless-builtins`/`flawless-menu` region — the label change in `edit.cpp` propagates without editing their code);
  stale lang keys of already-removed surfaces (F-Core effect cards etc.) — an i18n sweep is not this fix.
- **Tests:** new `tests/modules_tests.cpp` (7 cases); `role_tests` bus-name expectations followed the rename (same
  assertions, new names; 21/216 unchanged count); `round3_tests` R3-C key `F-Core: {}` → `Effects: {}` (7/82 unchanged).
  No test removed, no criterion relaxed.

## Changes

Implementation: the 18 files in `git diff --stat f6b86dc..b909ee8` (322 insertions / 86 deletions) — registry,
role_tools, calls bridge, edit/score/pattern_rack strings, MainComponent Tools region + self-test step 38,
PatternsWindow strings, PluginCards accessor, lang keys, tests. Documentation: this report + index row.
**Proposed ESTADO.md lines below — not written** (D72/D80: only the ledger scribe writes ESTADO.md).

Boundaries confirmed by empty diffs vs `f6b86dc`: `engine/`, `templates/`, `patterns/`, `pcore/`,
`app/engine-client/`, `app/selftest-plugins/`, `ci/`, `SCorePanel.*`, `BuildupPanel.*`. No DSP introduced
(Core still has no built-in instruments/effects; the tools write notes/sends/automation through role plugins).

## Verification

### Red-first (behavior changes)

At `d438d4a` (tests committed, code still base), `red-run.bat` (configure + build + ctest, UTF-8 code page):

- `modules_tests`: **7 tests / 136 checks / 14 failures** — card titles still `*-Core` (5), `Pattern Maker` title
  missing, new bus names `R-Core · Hall`/`D-Core · Dotted` measured, wave track named `W-Core`, `open "wcore"`
  refused (0 calls). The alias-resolution and legacy-load/bus-reuse cases **passed on base** (characterization).
- `role_tests`: 3 failures (`busesNamed "Reverb"/"Delay"` measured 0).
- `round3_tests`: 1 failure (lang key `Effects: {}` absent).
- Log: `D:/Core-build/flawless-modules-evidence/red-run.log`.

Green after `372d24b` (domain/bridge/lang): 14 domain-level merge suites **100% passed**
(`green-b-run.log`), including `modules_tests` 7/137/0 (one more check than red: the `wcore` open now resolves and
its inner assertion runs).

### Gates (registered `ci/gate.ps1`, unique-bat fix in base, under GATE-LOCK)

Run sequence (all logs in the evidence directory):

1. Merge gate (13:24Z): **VERDE 21/21** @ `b909ee8`, clean tree, 321 s — but the app objects came from the manual
   pre-commit build (incident 2 below): the binary still embedded the old stamp. Superseded by the clean rebuild.
2. Desktop gate (13:30Z): **VERMELHO** — `engine_process_tests` editor-timeout flake under concurrent load
   (incident 1); the desktop self-test itself passed. Investigated, isolated re-run passed.
3. Desktop re-run (13:39Z): **VERDE 22/22** (merge 21 + desktop 1), 44 s.
4. Clean rebuild merge gate (13:57Z, build folder deleted, full configure + 555 steps): **VERDE 21/21**, 413 s;
   `MainComponent.cpp.obj` now records **542 header deps** (the remaining `#deps 0` entries are JUCE
   CompilationTime units, RC resources and generated BinaryData — no Core source). The version header regenerated
   mid-build while this report was being written, so this run's stamp carries `.modified`; superseded by (5).
5. **Final re-gate at this docs commit (FINAL HEAD, clean tree): merge `ci/gate.ps1 -Build D:/Core-build/flawless-modules
   -Jobs 6` → VERDE 21/21; then `-Desktop` → VERDE 22/22; `CORE_BUILD_ID` == HEAD sha, `CORE_GIT_MODIFIED 0`,
   no `.modified`** (logs `gate-final-merge*`, `gate-final-desktop*`). The only code drift since `b909ee8` is two
   comment lines in `calls.hpp` (recompiled, no behavior change) plus this report and the index row.

- Coverage vs `f6b86dc` baseline **202 cases / 3059 checks**: now **209 cases / 3196 checks / 0 failures**
  (+7/+137, all in the new `modules_tests`; every other suite at its baseline count — per-suite table below).
  Nothing removed. Counts measured by `ctest -L merge -V` (`merge-verbose.txt`).
- Self-test (direct run capture, `selftest/self-test.txt`): `ok: M1c + M2 path, live notes, audio clips, mixer,
  patterns, tools, score, save with audio`, **51 logged transitions** (the base convention preserved — the new
  checks ride inside step 38, no extra `next()`), null device, loudest 0.735, document=68. The final desktop gate
  re-runs the same self-test at the final HEAD.

Per-suite (merge, verbose run `merge-verbose.txt`): engine 25/130, domain 30/525, legacy_guard 9/169,
**modules 7/137 (new)**, role 21/216, midi 14/208, bug 39/356, bitwig 2/25, audit 7/65, audit2 6/80, hunt 13/647,
round3 7/82, calls 12/69, menu 7/366, engine_process 10/121; cli_version/cli_demo_render/cli_measure/cli_unicode/
cli_package_stale/cli_plugin_crash_render passed (no case counts).

### Incidents measured and resolved (honest record)

1. **`engine_process_tests` flake under load** — the first desktop-gate attempt (13:30Z, concurrent streams active)
   failed `serve: a plugin that crashes opening its editor…` at `plugin.editor` 5000 ms timeout
   (`engine_process_tests.cpp:418`); the same SHA had passed the merge gate minutes earlier, the isolated re-run
   passed (12.3 s), and the branch diff touches **none** of `engine/`, `app/engine-client/`, `app/selftest-plugins/`,
   `tests/engine_process_tests.cpp` (empty diff). Classified: load-induced timing flake, not a regression of this
   change; the re-run desktop gate is VERDE 22/22. Residual risk: the 5000 ms window can be exceeded on a loaded
   machine — a CI robustness item for the engine stream (not changed here).
2. **Stale embedded build id after a manual pre-commit app build** — a manual `cmake --build --target CoreApp` run
   before commit C compiled the app objects with cl's **CP850** `/showIncludes` prefix while `rules.ninja` stores the
   **UTF-8** prefix (`Observação: incluindo arquivo: `, bytes `c3a7 c3a3` vs measured `87 c6`): those objects recorded
   `#deps 0`, so the gate did not see the `core_version.h` change and did not relink CoreApp; the direct self-test
   capture showed `build=0.7.0+372d24b1a4.modified`. The gate's own bat (CRLF, `chcp 65001`) compiles with matching
   UTF-8 (its targets show full deps, e.g. CoreTestFault 521/537; zero leaked include lines). **Resolution:** the
   build folder was deleted and both gates re-run from a clean configure under the gate's own environment; the
   final binary embeds `0.7.0+b909ee8468`, no `.modified`, and `MainComponent.cpp.obj` records its header deps.
   Recommendation (separate CI item, not implemented here): the gate's deps check asserts *some* target has deps
   (`findstr "#deps [1-9]"`); it could be strengthened to fail when any CXX object of Core's own targets has
   `#deps 0`, which would have caught this automatically.

Evidence directory `D:/Core-build/flawless-modules-evidence/`: `red-run.log`, `green-b-run.log`,
`gate-merge.log`/`gate-merge-resumo.txt` (run 1), `gate-desktop.log`/`gate-desktop-resumo.txt` (run 3),
`gate-merge-clean.log`/`gate-merge-clean-resumo.txt` (run 4), `gate-final-merge*`/`gate-final-desktop*` (run 5,
final HEAD), `merge-verbose.txt` (per-suite counts), `selftest/self-test.png` + `self-test.txt`,
`core_version-before-clean.h`, the helper bats and vcvars logs.

## Proposed ESTADO.md lines (for the ledger scribe — NOT written here)

- **Decisões / Estado medido (proposal):** «Cleanup dos módulos legados (owner 2026-11-18, verbatim: "apenas o
  pattern maker,mixer, piano, tools. o que esta a causar erros limpa, w-core, d-core f-core, tudo isso ja nao entra"):
  os títulos W-Core/F-Core/R-Core/D-Core saíram dos cards/menus do Tools; os ids e títulos antigos ficam como
  aliases de lookup (projectos legados e nomes antigos da AI resolvem). P-Core visível passa a Pattern Maker (PROD-02).
  Buses novos chamam-se Reverb/Delay · lugar; buses legados R-Core/D-Core são reutilizados pelo tag corePlace, sem
  duplicar, com o nome guardado. SCorePanel/BuildupPanel, catálogo, alinhamento kick/baixo/pratos e dados legados
  intactos; sem DSP próprio; templates NÃO tocados (stream flawless-clean). Ramos `flawless-modules` @ b909ee8+docs:
  vermelho medido (14+3+1 falhas @ d438d4a) → verde; gate merge VERDE 21/21 e desktop VERDE 22/22,
  209 casos / 3196 verificações / 0 falhas (base f6b86dc 202/3059, +7/+137 só em modules_tests),
  versão 0.7.0+b909ee8468 == HEAD sem .modified; self-test ok, 51 transições, patterns/tools/score abertos no passo 38.»
- **Pendência (proposal):** «BUG-CI-DEPS (novo, namespace BUG-): o check de deps do gate aceita qualquer alvo com
  deps; um build manual com code page errado deixou os objectos da app com #deps 0 e o gate não relinkou o CoreApp
  (build id .modified stale) — medido e resolvido por rebuild limpo nesta entrega; endurecer o check (por alvo) e
  investigar o flake de engine_process_tests plugin.editor 5000 ms sob carga (13:30Z, mesmo SHA verde antes/depois).
  Owner: Diogo; desbloqueio: decisão do stream CI/engine.»
- **Pendência (proposal):** «Integração: `flawless-modules` toca MainComponent.cpp na região Tools (~2301-2529,
  2011, 2224) e o passo 38 do self-test; os streams builtins/menu/crash/pattern tocam outras regiões — rebase na
  integração, preservar todos os lados. `template_sheet.cpp:77` (ficha "Modulação (W-Core)") fica para o stream dos
  templates; as chaves lang obsoletas de superfícies já removidas ficam para uma limpeza i18n própria.»

## Next steps

1. Integration rebase onto the round tip (other streams' MainComponent regions preserved; this branch's Tools region
   and self-test step 38 are disjoint from builtins/menu :4604-4761 and crash Restart :6427).
2. Test Engineer acceptance: open the four surfaces in a real session, load a pre-cleanup project with R-Core/D-Core
   buses and a `wave` track, run a tool (Effects/Reverb/Delay) and undo; confirm the old names still resolve from the
   AI bridge (`open "wcore"`).
3. CI/engine stream: the two proposed BUG- pendências above (deps check hardening; editor-timeout flake under load).
4. Template stream: `template_sheet.cpp` ficha heading + stale lang keys when templates are touched.