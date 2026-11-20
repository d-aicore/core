# VST3 host — Soundtoys (EchoBoy + Decapitator)

- **Date:** 2026-11-20 (Europe/Lisbon), owner stage 4 / S-round «Frentes próprias», VST3 HOST front.
- **Repo:** `D:\CORE\CORE` @ `a9a038430d781959829f8510067ce59afffaad62` + uncommitted round-1 fixes — **all preserved, zero product edits** (PROOF front; defects reported + proposed, fix authorization separate).
- **Binaries:** existing Release build `D:\Core-build\gate-round1d` (gate VERDE 27/27, D113 tree; `core-engine.exe` + `Core.exe` + test plugins). Nothing rebuilt. Probe workspace `D:\Core-build\vst3-host\`.
- **Safety:** all engine sessions `serve --backend null` (no device audio), `engine.start {listenDb:-15}`, ear guard always on; every render offline. The owner's RUNNING app (Core.exe pid 58072 / core-engine pid 40608) was never touched.
- **Owner contract (verbatim):** «Testa o host VST3 existente do Core com os Soundtoys instalados em C:\Program Files\Common Files\VST3\Soundtoys. Começa por EchoBoy e Decapitator. Usa uma cópia do projeto. Para cada plugin, verifica o scan, carregamento, abertura e fecho do editor, seleção de preset, automação, bypass, render e guardar/reabrir. Com o EchoBoy, usa um preset meu e automatiza o Mix. Com o Decapitator, automatiza o Drive. Confirma que o estado regressa e que a cauda do EchoBoy aparece no render quando prevista. Regista plugin, versão, formato e resultado de cada caso. Se houver dúvida entre problema do plugin e do Core, compara a mesma versão VST3 num segundo host disponível sob condições equivalentes. Regista os casos VST2 de projetos antigos separadamente e verifica o estado e a automação antes de propor qualquer migração.»
- **Method:** headless JSON-lines protocol (`core-engine serve`, request `{id,cmd,args}` → `{id,ok,result|error}`; `engine/src/app/main.cpp:131-172`, `server.cpp:320-355`). Probe song = `core-project` v2, one CoreTestSynth track (deterministic sine/note, 2 ms attack / 20 ms release) + one Soundtoys insert; a COPY created for probing, never an owner original. Editor open/close via the `plugin.editor` seam + `EnumWindows(pid)` + screenshots (no keystroke automation). Automation via track envelopes; render via the blocking offline `render` (48 kHz / 24-bit). WAV analysis in numpy (peak/sha256/envelope/echo spacing). Driver + case scripts: `D:\Core-build\vst3-host\engine_drive.py`, `echoboy_cases.py`, `round2_cases.py`, `boundary_regrade.py`, `b5b6.py`, `debug*.py`, `blob_*.py`.
- **Plugins under test:** EchoBoy `VST3-EchoBoy-d9085607-bc3ea0d0`, Decapitator `VST3-Decapitator-30e4085c-f0582f35`, both **version 5.5.5.19885, format vst3**, vendor Soundtoys, 2in/2out (fresh `core-engine scan`, exit 0; version resource 5.5.5 64-bit; app scan cache `plugins.json` corroborates all 23 Soundtoys bundles, `problems={}` `quarantine={}`).

## Medido — per-case table

Grades corrected per coordinator review: the first-pass EB-06/EB-10/DEC-06 "FAIL"s were threshold artifacts (plugin-internal parameter smoothing vs over-strict 1e-3 gates; dry→echo1 gap vs echo-to-echo spacing). Numeric proof below; regrade in `boundary-and-regrade.json`.

| Case | Plugin | Method | Result | Evidence (evidencia/2026-11-20-vst3-host-soundtoys/) |
|---|---|---|---|---|
| Scan | EchoBoy / Decapitator | `core-engine scan <file>` (crash-isolated subprocess) | **PASS** both, 5.5.5.19885 | scan-echoboy.json, scan-decapitator.json |
| EB-02 load | EchoBoy | `project.load` + `plugins.missing/versions` + `plugin.params` | **PASS** missing=[] versions=[]; Mix id=3 value 0.5 "50.0" | echoboy-load.json |
| EB-04 editor open/close | EchoBoy | `plugin.editor {open}` seam + EnumWindows(pid) + screenshot | **PASS** real "EchoBoy" window (hwnd 42144098) visible→hidden; engine answers after | echoboy-editor.json, echoboy-editor-open.png, echoboy-editor-close.png |
| EB-06 automation Mix | EchoBoy | envelope `fx:eb:vst:3` (0 held 0.5 s → 1 by 1.5 s) vs editor-init static Mix 0/1, render ×3, sample compare | **PASS** early\|A−B\|=0.003681 (internal Mix smoothing), late\|A−C\|=**0.0 exact**, late\|A−B\|=0.764308; 3 distinct sha | echoboy-automation-vst.json, eb2-automix.wav, eb2-mix0.wav, eb2-mix1.wav |
| EB-07 bypass | EchoBoy | `bypass:true` vs `false` vs effect absent, render sha256 | **PASS** bypass render **bit-identical** to no-fx; active differs (peaks 0.391894 / 0.448819) | echoboy-bypass.json, eb-active.wav, eb-bypass.wav, eb-nofx.wav |
| EB-08 render | EchoBoy | offline `render` 48 kHz/24-bit, null device | **PASS** 10 WAVs, peak/sha/seconds/realtimeFactor recorded (e.g. tail render 8 s in 168 ms, ×47.6) | echoboy-renders.json |
| EB-09 save/reopen — pure headless | EchoBoy | `plugin.param` → `plugins.states` sha → save doc → `plugins.release` → `project.load(states)` | **FAIL (defect candidate)** state sha 4ab36c1c…≠8e34ec27…; Mix returns **50.0 default**, Feedback 0.00 | echoboy-save-reopen.json, echoboy-debug.json, echoboy-debug2.json |
| EB-09 save/reopen — editor-move capture | EchoBoy | param moved WHILE editor open → capture → release → load(states) | **PASS** Mix 100.0 / Feedback 0.75 restored (debug3); B5/B6 Mix 80.0 restored | debug3.json, boundary-b5b6.json |
| EB-10 tail | EchoBoy | editor-init state Mix=1 Feedback=0.6, 1/8 delay @120 bpm; impulse note 125 ms; render tailMs=8000; envelope analysis | **PASS** tail **5.4923 s past note end** (0.145 s), **11 echoes**, echo-to-echo spacings **all 0.25 s** (= preset 1/8 delay), dry render ends exactly 0.145 s, not file-capped; Feedback=0 contrast = single echo | echoboy-tail-editor.json, eb2-tail.wav, eb2-tail-dry.wav, eb2-tail-fb0.wav |
| EB-11 recall boundary | EchoBoy | fresh-process B1–B6 matrix (below) | **MEASURED** condition pinned | boundary-and-regrade.json, boundary-b5b6.json |
| DEC-02 load | Decapitator | as EB-02 | **PASS** missing=[] versions=[]; Drive id=2 value 0.0 | decapitator-load.json |
| DEC-04 editor open/close | Decapitator | as EB-04 | **PASS** real "Decapitator" window (hwnd 81989762) visible→hidden; engine alive | decapitator-editor.json, decapitator-editor-open.png |
| DEC-06 automation Drive | Decapitator | envelope `fx:dec:vst:2` (0→1) vs editor-init static Drive 0/1 | **PASS** early\|A−B\|=0.0, late\|A−C\|=0.004531 (internal smoothing/AutoGain), late\|A−B\|=0.239022 | decapitator-automation.json, dec2-autodrive.wav, dec2-drive0.wav, dec2-drive1.wav |
| DEC-07 bypass | Decapitator | as EB-07 | **PASS** bypass bit-identical to no-fx; active differs | decapitator-bypass.json, dec2-active.wav, dec2-bypass.wav, dec2-nofx.wav |
| DEC-08 render | Decapitator | offline render | **PASS** 6 WAVs peak/sha recorded | decapitator-renders.json |
| DEC-09 save/reopen | Decapitator | headless vs editor-move capture | **headless FAIL** (Drive returns 0.0) / **editor-move PASS** (Drive 7.0 restored); blob unstable both ways | decapitator-state.json |
| Preset select (factory) | both | no headless load seam (`plugin.preset` is EXPORT only, juce_host.cpp:616-640); UI automation not reliable on the live owner machine | **DECLARED UNEXECUTED** | this report §Owner presets |
| Owner preset + Mix automation on it | EchoBoy | owner presets/projects not found (below) | **DECLARED UNEXECUTED** | this report §Owner presets |
| Second-host comparison | both | Bitwig 6.1 launched, equivalent state test NOT completed (live machine; aborted) | **DECLARED NOT PERFORMED** | this report §Second host |
| VST2 legacy | SoundToys VST2 | 18 DLLs present; engine refuses format vst2; no old VST2 projects found | **DECLARED** (feeds S10) | this report §VST2 |

## Resultados — numeric detail

### Automation (Mix / Drive) — the parameter moves in the audio
Envelope key for a plugin insert is `fx:<effectId>:vst:<paramId>` (`graph.cpp:722-740` `resolveEffectParam` requires the `vst:` prefix; the app builds it at `roles.cpp:233`). The first pass used `fx:eb:3` — silently unresolved by design (an fx: envelope whose target is absent is kept silent), a probe-usage error, not a Core defect. With the canonical key:
- EchoBoy Mix: render A (envelope) equals static Mix-0 render B in the held-0 region (max sample diff 0.003681 — EchoBoy's own Mix smoothing from the 0.5 default at render start) and equals static Mix-1 render C **exactly (0.0)** in the held-1 region, while differing from B by 0.764308 there. Three distinct file sha256.
- Decapitator Drive: early diff 0.0; late A−C 0.004531 (plugin smoothing/AutoGain), late A−B 0.239022. Automation is applied per aligned sub-block with glide (`graph.cpp:1187+`), identical in export and live paths.

### EchoBoy tail — confirmed when predicted
Editor-initialized state (Mix 100 %, Feedback normalized 0.6 → plugin text 0.75), Single mode, Echo1 1/8 note @120 bpm = 250 ms delay. One 125 ms sine note (+20 ms synth release → dry ends 0.145 s). Render with tailMs 8000: audio above 1e-4 continues to **5.6373 s** (tail 5.4923 s past note end), **11 envelope peaks**, echo-to-echo spacings **0.25 s ×9** — exactly the preset delay time; the file did not cap the tail. Dry reference (Mix 0) ends at 0.145 s; Feedback 0 gives a single echo. The tail appears in the render when predicted. (Default `tailMs` is 2000 ms — `render_job.hpp:25` — a long feedback tail needs an explicit larger tailMs; documented behaviour, not a defect.)

### State recall — the defect candidate (BUG-VST3-ST-RECALL, proposed namespace per D75)
**Measured condition (fresh processes, B1–B6):**

| Probe | Capture context | Recall after release+load(states) |
|---|---|---|
| B1 | pure headless: editor never opened, `plugin.param` moves | Mix **50.0 default** (FAIL) |
| B3 | fresh instance, headless moves | **50.0** (FAIL) |
| B4 | editor opened+closed WITHOUT a param move while open, then headless move | **50.0** (FAIL) |
| B5 | param moved WHILE editor open, editor closed, then capture | Mix **80.0 restored** (PASS) |
| B6 | param moved while open, capture while open | **80.0 restored** (PASS) |
| debug3 / round2 | editor open + moves + capture (EchoBoy Mix/FB; Decapitator Drive) | restored (100.0/0.75; 7.0) |
| first pass / DEC-09 | pure headless (both plugins) | defaults (50.0/0.00; 0.0) |

**Precise condition:** a Soundtoys 5.5.5 parameter change made **while the plugin editor is open** produces state that recalls (capture may happen after the editor closes); changes made only through the host API with no editor-open move produce state that **recalls as factory defaults**. Both plugins, same behaviour.

**Evidence chain:**
- Control **CoreTestGain** through the identical flow: live blob stable, recall **bit-identical** (sha 93c2f61b…), Gain 0.25 restored → Core's state flow is sound (`project.load` states arg `server.cpp:502-552`, `forEachPlugin` :111-122 visits track effects, `JuceHost::get/create` `juce_host.cpp:330-422`, offline render makes its own instances from document state `server.cpp:150`).
- Soundtoys blob = JUCE magic-XML (`0x21324356` + plain XML) with **IComponent only** (EchoBoy 3079 chars, Decapitator 1264, Soundtoys custom encoding); **no IEditController child**. JUCE writes that child only when `editController->getState == kResultTrue` (`juce_VST3PluginFormatImpl.h:3136-3150`); restore calls `component->setState`, `controller->setComponentState` + `resetParameters`, and `controller->setState` only if captured (:2884-2936). The controller state is therefore never restored, and Soundtoys' `setComponentState` evidently does not re-derive the parameter values.
- Editor-open + param move **changes the serialized IComponent content** (~30 diff regions incl. a large middle block, same length) — the headless-captured component state is a different, non-restorable serialization (`echoboy-blob-editor-diff.json`, `echoboy-blob-*.xml`).
- **Blob volatility is benign:** consecutive captures of an unchanged instance sometimes differ (tail region ~pos 3051-3079, timestamp-like); volatile blobs still restore when the condition holds (B5≠B6 blobs, both recall 80.0).
- S3 invalid-state gate passes (`missing=[]`): the blob IS valid JUCE XML; the loss is silent because the plugin accepts the state and simply does not recall the values — the existing invalid-state refusal cannot catch it.

**Attribution status: SUSPECTED PLUGIN-SIDE serialization quirk (Soundtoys 5.5.5), NOT confirmed.** Core's flow is proven correct with the control plugin; the JUCE hosting path is the standard VST3 sequence with no Core-specific deviation. The second-host comparison required to confirm was not completed (§Second host).

**Practical impact:** a song saved with Soundtoys inserts whose editors were never opened with a parameter move (host-side-only configuration: macros, randomize `vst:` curves, live `plugin.param` automation writes) **loses those settings on reopen** — the plugins return at factory defaults, silently. The natural Soundtoys workflow (tweaking in the plugin's own editor) captures restorable state, so owner-edited songs recall correctly. DATA-priority concern (criterion 1 «never loses work»): the loss is silent and the saved blob looks valid.

**Mitigation options — DECISIONS, not implemented (fix authorization separate):**
1. **Editor-init before capture**: when the app captures states of a Soundtoys (or any) plugin that has had host-side parameter moves but no editor-open move, open+close its editor (or require one editor-open move) before `plugins.states`. Cost: a window flash / plugin UI creation; needs a product decision on when it is acceptable.
2. **Force/repair the controller state**: investigate calling `IEditController::getState` directly / a raw-VST3 capture path that includes controller state, or re-apply parameter values after `setStateInformation` from the host's own parameter mirror. Needs a probe compile + second-host evidence first; touches the hosting layer (product decision).
3. **Accept + document**: declare Soundtoys headless-capture recall unsupported, surface a notice when a saved Soundtoys state was captured without an editor-open move (detectable via the blob's serialization shape), and document «open the plugin editor after host-side tweaks before saving». Cheapest; leaves the silent-loss window for automated flows.
4. **Regression once authorized**: a permanent test with a real Soundtoys bundle (or a mock plugin reproducing the shallow/deep serialization) pinning the B1–B6 matrix; red today on B1/B3/B4.

## VST2 legacy (separate record, feeds S10)
- **VST2 plugins present:** `C:\Program Files\Common Files\VST2\SoundToys` — 18 DLLs incl. `EchoBoy.dll`, `Decapitator.dll` (also kiloHearts, ValhallaDSP folders; `C:\Program Files\Steinberg\VstPlugins\Vital.dll`).
- **The current engine cannot load VST2 at all:** `validatePlugin` accepts only `vst3|clap|core` (`project.cpp:384-399`); `JuceHost::create` searches the VST3 format only (`juce_host.cpp:334-335`). A legacy project naming `format:"vst2"` is refused at load.
- **Old VST2 projects: none found.** Owner `recentSongs` → `C:\Users\vk_do\Documents\Song.core` **missing**; no `.core` under Documents (depth 2); the one owner song found, `D:\Core-Projects\Stems (Nova pasta 2)\Demo\Psy demo.core` (3 tracks), has **zero plugin references** (built-in-era demo); Bitwig Projects folder empty.
- **No migration proposed** — per contract, state/automation verification must precede any migration proposal, and there is no accessible VST2 project state to verify. Declaration only.

## Owner presets / owner project cases
- **Owner Soundtoys presets: not found.** `Documents\Soundtoys`, `Documents\VST3 Presets`, `ProgramData\Soundtoys`, AppData Soundtoys: absent. `AppData\Core\Presets` holds only an AI attachment. Soundtoys factory presets are embedded in the plugin browser (no files).
- **Owner-preset case (EchoBoy «preset meu» + Mix automation on it): DECLARED UNEXECUTED** — no owner preset or owner project with Soundtoys state exists on the machine to copy. Mix automation was verified on a factory-default probe instead (EB-06).
- **Factory-preset SELECT: DECLARED UNEXECUTED** — Core has no preset-load seam (`plugin.preset` exports `.vstpreset` for the Bitwig export only, `juce_host.cpp:616-640`, `MainComponent.cpp:4872+`); selecting a factory preset requires the plugin's own editor UI, and keystroke automation is not reliable on this live owner machine (§Second host incident). A preset-load command would be a product decision (pendência).
- Exported presets as evidence of the export seam: `echoboy-headless-mix100.vstpreset` / `echoboy-editor-mix100.vstpreset` (both 2401 bytes, Soundtoys native QWIDGET/PRESET format, contents differ — same serialization split as the state blobs).

## Second host
- **Available:** Bitwig Studio 6.1 (`D:\Program Files\Bitwig Studio`), Ableton Live 12 Trial, Cakewalk Next. No Steinberg VST3 test host.
- **Attempted:** Bitwig launched (screenshots bitwig-01-launch/02-newproject/03-search in probe dir), new project created. **Aborted:** the owner's machine is live (windows moving, owner actively chatting); a misfocused SendKeys landed in the owner's ChatGPT draft instead of Bitwig. **Equivalent state-recall test NOT completed → comparison DECLARED NOT PERFORMED; attribution remains suspected, not confirmed.**
- **Incident declaration (transparency):** the stray keystroke appended "EchoBoy" to the owner's ChatGPT input draft (UNSENT). Repair attempts (click+backspace, word-delete, clipboard probe) did not land in the input (clicks mis-targeted; Ctrl+A copied the conversation, proving focus was on the message list). Per coordinator direction the draft was then left untouched — the residue "…1:04EchoBoy" may still be in the owner's draft; nothing was sent. My Bitwig instance (pids 20056/64084/52588) was closed; the owner's Core.exe/core-engine were never touched. No other owner window was modified.

## Alterações
**None.** Zero product edits (PROOF front). Repo working tree unchanged by this front; all round-1 uncommitted fixes preserved. New files only: this report, the index row, the evidence dir, and probe artefacts outside the repo (`D:\Core-build\vst3-host\`, notes in `D:\Core-build\coordinator-round2\vst3-host-notes.md`).

## Verificação (re-run)
1. `D:\Core-build\gate-round1d\app\desktop\CoreApp_artefacts\Release\core-engine.exe scan "C:\Program Files\Common Files\VST3\Soundtoys\EchoBoy.vst3"` (and Decapitator) → JSON identity/version.
2. `python D:\Core-build\vst3-host\round2_cases.py` → EB-06/EB-10/DEC suite (needs the gate-round1d build + bundled numpy/Pillow python). Transcripts `transcript-*.jsonl`, plugin logs `plugin-log-*.jsonl` in the probe dir.
3. `python D:\Core-build\vst3-host\boundary_regrade.py` and `b5b6.py` → B1–B6 recall matrix + regrades.
4. WAV facts: `echoboy-renders.json` / `decapitator-renders.json` carry sha256/peak/seconds for every render (WAVs in `D:\Core-build\vst3-host\`; the six decisive ones copied into the evidence dir).
5. Blob anatomy: `echoboy-blob-default.xml`, `echoboy-blob-mix100.xml`, `echoboy-blob-editor-diff.json`.

## Pendências
1. **BUG-VST3-ST-RECALL** (proposed): Soundtoys headless-capture state loss — characterization complete; fix authorization separate; mitigation options 1–4 above are decisions for the owner/coordinator.
2. **Second-host comparison pending** (Bitwig/ Ableton/ Cakewalk available) to confirm plugin-vs-host attribution — needs a quiet machine or a headless second-host seam.
3. **Owner-preset case pending** until an owner preset/project with Soundtoys state exists; **factory-preset select** needs a preset-load seam (product decision) or a reliable UI session.
4. **VST2 legacy → S10**: engine refuses vst2; no old projects accessible; migration proposal blocked on state/automation verification per contract.
5. **ChatGPT draft residue** (§Second host): owner may want to delete the stray "EchoBoy" in the draft; left untouched by directive.

## Linhas ESTADO propostas (só o escrivão escreve ESTADO.md — D72/D80)
- **BUG-VST3-ST-RECALL (novo, namespace BUG per D75):** «Soundtoys 5.5.5 VST3 (EchoBoy, Decapitator): estado capturado sem uma mudança de parâmetro com o editor aberto regressa a defaults de fábrica no reopen (B1/B3/B4 medidos, ambos os plugins); captura com mudança-enquanto-editor-aberto regressa (B5/B6, debug3). Fluxo do Core provado são com controlo CoreTestGain (bit-idêntico). Suspeita: serialização do plugin (blob IComponent-only, sem IEditController; conteúdo muda com editor+move). Segundo host pendente; atribuição não confirmada. Impacto: música guardada com Soundtoys configurados só pelo host perde settings em silêncio. Mitigações propostas como decisões, nada implementado.» Fonte: este relatório + evidencia/2026-11-20-vst3-host-soundtoys/ (boundary-and-regrade.json, boundary-b5b6.json, debug3.json, echoboy-debug2.json, decapitator-state.json, echoboy-blob-editor-diff.json).
- **Estado medido:** «Host VST3 Soundtoys 5.5.5.19885 @ a9a0384+round1 (gate-round1d): scan/load/editor open-close/bypass/render PASS ambos; automação Mix (EchoBoy) e Drive (Decapitator) PASS por comparação de samples (late A−C 0.0 / 0.0045); cauda EchoBoy 5.49 s / 11 ecos / espaçamento 0.25 s = delay do preset; bypass bit-idêntico a sem-efeito. Guardar/reabrir: FAIL headless / PASS editor-move (BUG-VST3-ST-RECALL). Preset do dono e seleção de preset de fábrica não executados (sem preset/projeto do dono; sem seam de load). VST2 recusado pelo motor (validatePlugin), 18 DLLs SoundToys presentes, sem projetos antigos.» Fonte: este relatório.
- **VST2 (alimenta S10):** «Motor actual não carrega VST2 (project.cpp:387 só vst3|clap|core; juce_host.cpp:334 só VST3). SoundToys VST2 instalados (18 DLLs). Nenhum projeto VST2 antigo acessível; migração não proposta sem verificação de estado/automação (contrato do dono).» Fonte: este relatório §VST2.

## Limitações
- Null backend only (no real-device audio through Soundtoys); renders offline at 48 kHz/24-bit, one sample rate/block size — the owner's «every sample rate and block size» criterion is not covered here.
- One probe topology (synth → single insert); no master-chain, multi-instance, or sidechain cases; no long soak.
- State recall tested via the engine protocol (the app's own save flow: states arg + inline document); the app GUI save/reopen path was not click-tested (live machine).
- Attribution suspected, not confirmed (second host pending); the IEditController-absence mechanism is a code-level observation (JUCE gate at `juce_VST3PluginFormatImpl.h:3144`), not a raw-VST3 probe measurement.
- Preset cases unexecuted (no owner presets/projects; no load seam; UI unreliable on the live machine).
- The stray ChatGPT draft residue (§Second host) is declared, not repaired.
- EchoBoy Jr / other 21 Soundtoys bundles not tested (contract: EchoBoy + Decapitator first).
