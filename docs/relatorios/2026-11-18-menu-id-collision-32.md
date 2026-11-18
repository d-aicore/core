# Report — 2026-11-18 — Plugin menu ID collision (BUG-SWEEP-32)

## What was measured

- Repository / branch / SHA: d-aicore/core, worktree `.agents/worktrees/flawless-menu`, branch `flawless-menu`,
  base `ed4642e` (no commits at start). Final: fix `ca70435`, CI cherry-pick `c0accc1`, docs: the commit
  carrying this report.
- Scope and authorization: completion of BUG-SWEEP-32 from the owner's consolidated 32-finding list
  (item 32, "High conditional", archived verbatim in
  [evidencia/2026-11-18-menu/consolidated-list.md](evidencia/2026-11-18-menu/consolidated-list.md)).
  The prior agent extracted the allocation/dispatch into `app/desktop/src/MenuIds.h` (verbatim ed4642e
  behaviour) and wrote `tests/menu_tests.cpp`; this delivery measured the red, implemented the disjoint-ID
  fix, wired both menus through the helper, measured green, and ran the full gate.
- Environment: Windows 10.0.26300 x64, MSVC 19.51.36260.0 (VS 18 BuildTools), Ninja, CMake 4.4.4,
  Release, JUCE 9.0.5 (`D:/Core-deps/JUCE-9.0.5`), build dir `D:/Core-build/flawless-menu`,
  evidence `D:/Core-build/flawless-menu-evidence/`. `VSLANG=1033` + `chcp 65001` as the gate requires.
- Tests executed: `menu_tests` red (pre-fix binary) and green (post-fix); full `ci/gate.ps1 -L merge`.
- Limitations: the regression proves the extracted dispatch headlessly, not a real menu click; no catalog
  of 501+ installed plugins exists in this sandbox, so the live UI path at those sizes remains
  Windows-unverified (as in the original finding). The shared harness `tests/support/check.hpp` keeps its
  Portuguese verdict words (`FALHOU`, `N testes, N verificacoes, N falhas`); it is shared by every suite and
  was not touched (no translation sweep in an unrelated fix). The new test's own messages are English.

## Results

| ID | Problem / result | Evidence (SHA, file:line or execution) | Classification | State |
| --- | --- | --- | --- | --- |
| BUG-SWEEP-32 | Plugin item IDs shared the fixed action IDs: with 501+ instruments, native-replace (800), effect Open/Remove/On (900/920/940) and Add-effect (4000) routed to `replaceInstrumentWithPlugin`; with 1001+ instruments the Add Track menu's native ids (3000+) did the same | Red: `red-run.log` / `red-reconfirm.log` (56 failures); fix `MenuIds.h` + `MainComponent.cpp` @ `ca70435`; green: `green-menu-tests.log` (366/366) | Proven by test (dispatch); confirmed in code (wiring) | Fixed |
| BUG-SWEEP-09/11 | `doc::builtinInstruments()` still lists 8 legacy names (`edit.cpp:45-49`) although Core has no built-in instruments (removed 2026-11-16) | Not touched here | Confirmed in code (prior reports) | Open — separate item |

## Red — measured before the fix

Commands (build dir configured with `CORE_BUILD_TESTS=ON`, Ninja, Release):

```
cmake --build D:/Core-build/flawless-menu --target menu_tests --parallel 6
ctest --test-dir D:/Core-build/flawless-menu -R "^menu_tests$" --output-on-failure
```

Exit code 8 (ctest). `menu_tests`: **7 tests, 366 checks, 56 failures** (4 of 7 cases FAIL). The inherited
`red-run.log` holds the full first measurement; `red-reconfirm.log` is my re-run against the same pre-fix
binary before rebuilding (identical summary, rc 8). Both are committed byte-identical as `red-run.txt` /
`red-reconfirm.txt` in [evidencia/2026-11-18-menu/](evidencia/2026-11-18-menu/) (the repo gitignores `*.log`;
evidence logs are tracked as `.txt`, the convention of the sibling streams). Boundary numbers measured:

- catalog 501, chosen 800 (native instrument) → `replaceWithPlugin index=500` (expected `replaceWithBuiltin index=0`); same at 600/601/620/621/640/641/1000/1001/5000, and 807 → index 507.
- catalog 601, chosen 900 (Open effect 0) → `replaceWithPlugin index=600`; also at 620/621/640/641/1000/1001/5000.
- catalog 621, chosen 920 (Remove effect 0) → `replaceWithPlugin index=620`; also at 640/641/1000/1001/5000.
- catalog 641, chosen 940 (On effect 0) → `replaceWithPlugin index=640`; also at 1000/1001/5000.
- catalog 5000, chosen 4000 (Add plugin effect 0) → `replaceWithPlugin index=3700`; 4299 → index 3999; the two plugin bands overlap at 5000.
- Add Track: catalog 1001, chosen 3000 (native) → `addPluginInstrument index=1000`; catalog 5000, 3000/3007 → indices 1000/1007.
- Full-band disjointness failures: replace band hits action id 800 at every catalog ≥ 501; add-track band hits 3000 at ≥ 1001.

These match the owner's archived `menu-ranges.log` (committed as the byte-identical `menu-ranges.txt`;
501→800→idx500; 601→900→600; 621→920→620; 641→940→640; Add Track 1001→3000→idx1000).

## Fix — disjoint identifier spaces via MenuIds.h

One identifier-allocation defect across both menus; fixed by construction, no catalog cap:

- Every fixed action id stays below 4000: track menu {1, 2, 3, 4, 6, 800–807, 900–959}, Add Track
  {1000, 1001, 1002, 3000–3007}. The effect chain keeps the `% 20` wrap and the cap-16 semantics
  (`edit.hpp:148`); ids past the chain still do nothing.
- Plugin bands start at `kPluginBandFirst = 4000` and stack upward, sized by the uncapped catalogs:
  - track menu Add-effect: `[4000, 4000 + effectCount)` (historical base kept);
  - track menu Replace: `[4000 + effectCount, 4000 + effectCount + instrumentCount)` — stacks above the
    add-effect band, so the two never overlap at any sizes;
  - Add Track plugins: `[4000, 4000 + instrumentCount)`.
  No plugin index can equal a fixed action id at any catalog size (all fixed ids are below the bands).
- The disabled "None found yet" placeholder moved 99999 → −1 (`kNoPlugins`). Reason: the uncapped
  add-effect band `[4000, 4000 + fx)` reaches 99999 at fx ≥ 95999, so the placeholder had to leave the
  positive id space for the guarantee to hold at any size. −1 is JUCE's own "no result id" convention:
  `PopupMenu::Item(String)` defaults `itemID` to −1 (`juce_PopupMenu.cpp:1820`, header docs :128–143),
  `addItem` only asserts against 0 (:1951), and the item is disabled so it is never chosen. The Tools
  submenu's `setAction` items already return −1 to the callback (`getResultItemID`, :620–630); −1 decodes
  to `none`, exactly as before.
- `MainComponent.cpp`: both menus' construction and callbacks now go through the helper
  (`menuids::` alias — the local `juce::PopupMenu menu` shadows the namespace name). Every action keeps
  its exact behaviour (edit labels, undo, editor opening, selection); the callback's branch order is
  preserved in `decodeTrackMenu` / `decodeAddTrackMenu`. The 5000 sample band is never populated
  (`offered` stays empty; Tools uses `setAction`); its comment now says that if revived it must be
  allocated through `MenuIds.h`, because the plugin bands grow past 5000.
- Not touched: the Restart-visibility region (~:6427, flawless-crash territory), `Plugins.cpp` (catalog
  stays uncapped), `edit.cpp` / `builtinInstruments()` (BUG-SWEEP-09/11), no DSP reintroduced.

## Green — measured after the fix

Same commands; `ctest -R "^menu_tests$"` exit 0, **1/1 passed**; the exe directly:
**7 tests, 366 checks, 0 failures** — all 7 cases ok (`green-menu-tests.txt`, `green-run.txt` in the archive):

- every boundary catalog (500/501/600/601/620/621/640/641/1000/1001/5000) routes 800/807 → native
  replace 0/7, 900/920/940 → Open/Remove/On effect 0, 4000 → add-effect 0, 6/1/2/3/4 → their actions;
  Add Track 3000/3007 → native 0/7, 1000/1001/1002 → audio/import/sampler;
- full-band disjointness (interval arithmetic over every plugin index, not a sample) passes for both
  bands of both menus at every boundary size, and the two track bands never overlap;
- first and last plugin of each list round-trip to their own index at every size;
- the cap-16 wrap semantics and the unknown-id cases (incl. 99999, now just an unused id) unchanged.

The whole tree was rebuilt (`green-run.bat`, 386 targets) — the `MainComponent.cpp` wiring compiles and
links in Release.

## Full gate

GATE-LOCK protocol: the lock was held by other streams while this delivery built
(`flawless-preservation` 06:43:07Z, then `flawless-randomize` 06:47:48Z); waited and re-checked, created
`flawless-menu 2026-11-18T06:55:03Z` when free, deleted it after the run.

```
powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-menu -Jobs 6
```

(run from the worktree, after the fix commit and the cherry-pick of `fffbb41` — unique temp bat per
invocation, so concurrent gates cannot clobber each other).

- Verdict: **VERDE**; time **42 s**; `gate-resumo.txt` / `gate.log` copied to
  `D:/Core-build/flawless-menu-evidence/` and committed in the archive (`gate-resumo.txt`, `gate.txt`).
- Version: `core-engine 0.7.0+c0accc110c` — equals the HEAD sha (`git rev-parse --short=10 HEAD` =
  `c0accc110c`), **no `.modified`** (the dirty check is
  `git status --porcelain --untracked-files=no`, `cmake/core_version.cmake:17`; the resumo's
  "com alteracoes por commitar" note is the untracked evidence archive + this report, committed in the
  docs commit right after — tracked files are exactly HEAD).
- `ctest -L merge`: **18/18** — **152 cases / 2107 checks / 0 failures**.
- Coverage vs the measured ed4642e baseline (145/1741, 17 ctest tests; `2026-11-18-estado-ctests.md`,
  written on the `flawless-estado` branch — used as the reference, not an ancestor of this branch):
  this delivery **adds** `menu_tests` 7 cases / 366 checks → **152 / 2107, 18 ctest tests**.
  `engine_process_tests` stays 4/49. No suite lost cases or checks; nothing relaxed.
  Counts derived from my own verbose run after the gate:
  `ctest --test-dir D:/Core-build/flawless-menu -L merge -V --output-on-failure` → rc 0, 18/18,
  13.29 s; the 13 counting suites' `N testes, N verificacoes, N falhas` lines sum to 152/2107/0
  (`merge-verbose.txt` in the archive).

## Changes

- `app/desktop/src/MenuIds.h` (new): the extracted allocation/dispatch + the disjoint-band fix.
- `app/desktop/src/MainComponent.cpp`: both catalog menus built and dispatched through the helper.
- `tests/menu_tests.cpp` (new): boundary routing + full-band disjointness regression (7 cases, 366 checks).
- `tests/CMakeLists.txt`: `menu_tests` target + `merge` label.
- `ci/gate.ps1`: cherry-pick `fffbb41` (unique temp bat), unchanged content.
- Docs: this report, one index row in `docs/relatorios/README.md`, and the evidence archive
  `docs/relatorios/evidencia/2026-11-18-menu/`: the owner's `consolidated-list.md` and `menu-ranges.log`
  (inherited, committed as-is; the log also as the byte-identical `menu-ranges.txt` because `*.log` is
  gitignored), plus this delivery's run logs as `.txt` (red-run, red-reconfirm, green-menu-tests,
  green-run, gate, gate-resumo, merge-verbose). Originals stay in `D:/Core-build/flawless-menu-evidence/`.

Commits: `ca70435` (fix + helper + test), `c0accc1` (cherry-pick fffbb41), and the docs commit carrying
this report + index row + evidence archive.

## Pending and handoff

- For the Test Engineer: acceptance is the headless dispatch proof + gate above. A live UI acceptance
  would need a catalog of 501+ instrument plugins (or a synthetic catalog injection) and real menu
  clicks on native-replace / effect Open / Remove / On; not executed here, no such catalog in the
  sandbox. The green run is a build + headless test, not a UI test.
- Integration: rebase onto `flawless-crash` expected; the Restart-visibility region (~:6427) was not
  touched, the menu region (:4606–4761) is disjoint from it.
- BUG-SWEEP-09/11 remain open: `builtinInstruments()` still lists 8 legacy names; the menus still offer
  them (behaviour preserved here by design — fixing that is a separate authorized item). No DSP
  reintroduced; the catalog is not capped.
- The 5000 sample band stays unpopulated; if ever revived, allocate it through `MenuIds.h`.
- The shared harness's Portuguese verdict words are a repo-wide language item, not swept here.

## Proposed ESTADO.md lines (not written — only the ledger scribe writes ESTADO.md, D72/D80)

- Estado medido: «BUG-SWEEP-32 corrigido: os ids dos menus de catálogo de plugins são disjuntos dos ids
  de acção por construção (bandas a partir de 4000, placeholder −1), sem tecto no catálogo. Regressão
  `menu_tests` vermelho medido (56 falhas @ ed4642e+extracção) → verde (7 casos / 366 verificações).
  Gate merge VERDE 18/18, 152/2107 @ `ca70435` (version `0.7.0+c0accc110c`, sem `.modified`).
  Fonte: docs/relatorios/2026-11-18-menu-id-collision-32.md.»
- Pendência (owner = Diogo): fechar o item 32 da lista consolidada como corrigido-ao-nível-do-dispatch
  (clique real de menu com catálogo ≥ 501 não verificado); manter 09/11 abertos
  (`builtinInstruments()` legacy); integrar `flawless-menu` sobre `flawless-crash`.