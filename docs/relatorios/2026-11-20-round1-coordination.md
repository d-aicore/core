# Relatório — 2026-11-20 — Ronda 1: coordenação, gate integrado VERDE 25/25, passagem

Delivery: DSH lead session `a69b320fbfab` (coordinator), 2026-11-20 Europe/Lisbon.
Owner authorization: dispatch «Core — Passagem para o coordenador dos Qwens» (2026-11-20),
ordem **dados > tempo real > performance > compatibilidade** (D54). This report aggregates the
four round-1 deliveries (each with its own report, red→green proof and evidence) and records the
**registered integrated gate** at the integrated working tree.

## O que foi medido

- Repositório `d-aicore/core`, `main @ a9a038430d781959829f8510067ce59afffaad62` (= origin/main).
  **Integrated working tree (uncommitted)**: 11 tracked files changed +546/−71, plus new tests,
  reports and evidence. Version measured: `core-engine 0.7.0+a9a038430d.modified` — the
  `.modified` is the uncommitted round-1 set, declared (version==sha only after the owner's commit).
- Gate registado: `ci/gate.ps1 -Build D:\Core-build\gate-round1 -Desktop -Jobs 8` — Release,
  fresh folder, unique bat, VSLANG=1033 + chcp 65001, Ninja header-deps check, `--no-tests=error`.
  Configure verified the D113 patch on the local JUCE («already carries the full crash/restart
  patch (verified: git apply --check -R)»). Build 559 targets rc=0.
- **Gate verdict: VERDE — 25/25 (24 merge + 1 desktop), 100% passed, 0 failures.**
  Gate time 382 s (build+tests); test time 61.96 s (merge 37.20 s, desktop 23.73 s).
  Per-suite totals from the verbose re-run: **274 cases / 3748 verifications / 0 failures**
  (17 suite summaries; cli tests carry no case counts). `engine_process_tests` PASSED in
  Release (19.86 s) — ENV-03 is Debug-only (3A investigating). `desktop_self_test` passed
  (23.73 s, screenshot archived).
- Test-count evolution: baseline a9a0384 = 21 merge + 1 desktop; round 1 adds
  `export_plugin_failure_tests` (S1, 5/54), `wav_float_tests` (S4, 16/360), `installer_samples`
  (S5, 70 checks) → **24 merge + 1 desktop = 25**. Nothing removed or weakened; the shared
  merge-label list keeps all 18 original names (coordinator-verified); the
  `cli_plugin_crash_render` contract rewrite is sanctioned and documented in the S1 report
  (the old assertion encoded the defect itself; anti-hang + crash-proof + healthy control kept).
- Limitações: gate na árvore com alterações por commitar (D90 precedent — não é gate de árvore
  limpa; version==sha fica para depois do commit); headless/null device (o self-test abre a
  janela no dispositivo nulo); dispositivo real, VST3 reais e a app sob ASan continuam por provar
  (S7/frentes próprias); Q1–Q8 do plano original não consultáveis (1c6238b não localizável —
  busca exaustiva arquivada em evidencia/2026-11-20-s11-juce-reproducible-build/plan-hunt-closure.md).

## Resultados (por item, com relatório próprio)

| Item | Entrega | Vermelho → Verde | Relatório |
| --- | --- | --- | --- |
| **S11** JUCE reproduzível | CMake aplica o patch D113 à cópia descarregada (idempotente) e verifica a local (falha cedo, nunca modifica); prova de patch completo por `git apply --check -R` | fetch limpo + local sem patch → C2039 juce_host.cpp:136 rc=2; 5 casos verdes incl. Release fresco + crash/restart 3/3 (17 verif) + processBlock-crash 3/3 (25 verif) + cli 1/1 | [s11](2026-11-20-s11-juce-reproducible-build.md) |
| **S1** exportação incompleta | `checkExportPlugins` nos dois pontos das duas vias; nome/pista/razão; bypass≠falha; stems conjunto todo; MainComponent não tocado | a–e medidos (degradado byte-idêntico ao saudável; **WAV anterior substituído + backup apagado no sucesso** render_job.cpp:183; stem todo-zero publicado); regressão 5/54: 33 falhas→0; CLI real 5 casos | [s1](2026-11-20-s1-export-refusal.md) |
| **S5** samples do utilizador | propriedade por sha256 no manifesto; legacy provado contra o source; sem source nunca apaga; re-validação na remoção; Audit-27 preservado | matriz real 7 casos (i/ii/iv/vii vermelhos); regressão 70 checks: 29 falhas→70/70 ×2 + manual cruzada | [s5](2026-11-20-s5-installer-sample-protection.md) |
| **S4** WAV float inválido | bit-patterns inteiros à prova de /fp:fast (incondicional Debug+Release); rejeição nomeia ficheiro/frame/canal/kind; AssetStore nada regista | NaN/±Inf raw + overflow f64→float + cache completo; regressão 16/360: 22 falhas→0 **Debug E Release** + probe cruzado | [s4](2026-11-20-s4-float-wav-rejection.md) |
| **Gate integrado** | ci/gate.ps1 -Desktop, Release, pasta nova | **VERDE 25/25, 274/3748/0, 382 s** | este relatório + `gate-resumo.txt`, `gate-verbose-counts.txt`, self-test |

## Alterações

- Código (por commitar, mapa de commits lógicos C1–C6 no manifest
  `D:\Core-build\core-review\tooling-delivery\manifest.md` §Round-1 addendum):
  CMakeLists.txt (ENV-01 +13, D113 +56); app/installer/Setup.cpp (+189/−28);
  engine/src/io/audio_file.cpp (+39/−2); engine/src/core/{graph,render_job,server} (.hpp/.cpp);
  tests/{installer_samples.cmake, wav_float_tests.cpp, export_plugin_failure_tests.cpp} novos +
  tests/CMakeLists.txt (+44/−1 aditivo) + tests/cli_plugin_crash_render.cmake (contrato sancionado).
- Documentação: 5 relatórios novos (S11/S1/S5/S4 + este) + linhas no índice + diretórios de
  evidência (11+30+6+7+gate ficheiros). `ESTADO.md` não editado (D72/D80).
- Fora do repo: builds próprios por agente (`D:\Core-build\{s1-red,s1-fix,s4-red,s4-fix,s5-installer,s5-fix,s11-*,gate-round1}`),
  notas `D:\Core-build\coordinator-round2\`, arquivos históricos intactos.

## Verificação

- Gate: `powershell -NoProfile -ExecutionPolicy Bypass -File ci\gate.ps1 -Build D:\Core-build\gate-round1 -Desktop -Jobs 8`
  → exit 0, «Gate do Core: VERDE», 25/25. Verbose re-run para contagens: 274/3748/0.
- Cada item: vermelho medido ANTES no mesmo SHA (builds não-corrigidos próprios), verde DEPOIS,
  diffs revistos pelo coordenador (pontos de chamada, bypass, bit-patterns, ownership, contratos).
- ENV-03 pré-existente provado no build não-corrigido (s1-red falha identicamente em Debug);
  Release passa no gate. Não é regressão da ronda.

## Pendências e passagem

1. **Decisão do dono — commit**: nada commitado (D48/D50; sem autorização verbatim). Mapa C1–C6
   pronto; após commit, re-correr o gate em árvore limpa para version==sha (D90).
2. **3A (ENV-03)**: agente `env03-debug` a investigar (tarefa própria, task-2) — reproduzir
   Debug/Release, medir fecho dos pipes/espera do cliente/saída real, capturar stacks da thread
   bloqueante, provar a causa antes de propor alteração. Dados novos: passou numa corrida Debug
   (s4-fix), falhou 3/3 noutras — sensível a timing.
3. **S2/S3**: agente `s2s3-fix` (tasks 14/15) — aviso de missing-plugins + relinkPlugins +
   estado inválido recuperável (vermelho primeiro, incluindo o caminho de perda do estado original).
4. **S6/S7** a seguir (task board): bit-a-bit serial≡M5 + watchdog; alloc/lock/IO com controlos
   negativos; baseline Release dispositivo real 64/128. **Frentes VST3/Chain** depois (tasks 4/6/8).
5. **Recomendações registadas, NÃO implementadas (decisões separadas)**: retenção do backup
   `.previous` no caminho de SUCESSO do publish; reserva exclusiva SWEEP-02 no `.part` legacy do
   CLI; colocação crash-atómica dos stems; subnormais f64→f32 fora do contrato S4.
6. **Q1–Q8 / plano original**: não localizável (local + remote); consulta pendente; os contratos
   explícitos do dispatch governaram a ronda; reconciliar por referência se o plano aparecer.
7. **S8–S13** conforme o dispatch (undo, mute, VST2, restos do som próprio/Q8, UX aguarda revisão
   do dono à slice A).

**Linhas propostas para ESTADO.md (o escrivão escreve, D72/D80):**
- *Estado medido — Ronda 1 (2026-11-20, a9a0384 + correções locais por commitar)*: gate registado
  ci/gate.ps1 -Desktop Release pasta nova **VERDE 25/25** (24 merge + desktop), **274 casos /
  3748 verificações / 0 falhas**, 382 s, version 0.7.0+a9a038430d.modified (árvore por commitar,
  D90). S11 JUCE reproduzível (patch D113 aplicado/verificado pelo CMake, 5 casos, crash/restart
  Release 3/3+3/3+1/1); S1 exportação recusada com plugin falhado (vermelho a–e incl. perda do
  WAV anterior + backup apagado no sucesso; regressão 5/54 33→0; CLI real 5 casos; contrato
  cli_plugin_crash_render reescrito com justificação); S5 samples protegidos por hash (70 checks
  29→70/70 ×2); S4 WAV float rejeitado à prova de /fp:fast (16/360 22→0 Debug+Release). Baseline
  21 merge → 24, nada removido/enfraquecido. ENV-03 Debug-only pré-existente (3A em curso).
- *Pendências*: commit C1–C6 (dono) + re-gate version==sha; 3A/S2/S3 em curso; S6/S7 + frentes
  VST3/Chain a seguir; recomendações (backup no sucesso, .part CLI, stems atómicos) = decisões
  separadas; Q1–Q8 pendentes (plano 1c6238b não localizável).
- *Referência*: este relatório + os 4 relatórios de item + evidência; manifest com o mapa de commits.

**Retoma (próximo agente)**: SHA a9a0384 + correções locais (mapa C1–C6); gate VERDE em
D:\Core-build\gate-round1 (Release) e Debug integrado em D:\Core-build\s1-fix\build; agentes
ativos env03-debug (3A) e s2s3-fix (S2/S3); task board tasks 2/14/15 em curso, 16/17 (S6/S7)
próximos; scripts de ambiente SHA-bound em D:\Core-build\core-review (verify.ps1 → ready).

---

## Addendum datado (2026-11-20 ~02:15) — S2/S3 entregues + gate re-corrido + ENV-03 provado

1. **S2/S3 entregues** (agent `s2s3-fix`, relatório próprio
   [2026-11-20-s2s3-state-recovery.md](2026-11-20-s2s3-state-recovery.md), 7 ficheiros de
   evidência): vermelho 7 testes/68 checks/**16 falhas** na árvore não-corrigida — incluindo a
   prova byte-a-byte de onde o estado original se perde (`captureStates` juce_host.cpp:472-493 →
   `fetchPluginStates`/`adoptPluginStates` → `doc::applyPluginStates` edit.cpp:1053-1074 no
   Save/export → factory blob persistido). Correção minimal: estado inválido recusado com why
   via canal missing existente (Base64 + getXmlFromBinary gates, +16/−2) → instância identificada,
   **recusa de exportação S1 herdada**, captureStates nunca sobrescreve o original; aviso
   missing-plugins name+why no snapshot/status line (sobrevive ao clobber do openDevice);
   relinkPlugins ligado no open (catálogo persistido, sem live-fetch) e no scan-complete
   (live-states primeiro, um passo de undo «Relink plugins»); plugin_refs/server/graph intactos.
   Verde **7/68/0**; merge Debug 24/25 com ENV-03 pré-existente provado no build s1-red.
   Diff revisto pelo coordenador. Limitações declaradas: UI click-through não feito; blob
   válido-XML-sem-IComponent aplica como no-op (indetetável sem validação interna do formato).
2. **Gate integrado re-corrido com S2/S3** — `ci/gate.ps1 -Build D:\Core-build\gate-round1b
   -Desktop -Jobs 8`, Release, pasta nova: **VERDE 26/26 (25 merge + 1 desktop), 100%, 0 falhas**,
   387 s; contagens verbose: **281 casos / 3816 verificações / 0 falhas** (18 suite summaries;
   +7 casos/+68 verif vs round1 = state_recovery_tests). D113 verificado no configure;
   engine_process_tests Release PASS 19.79 s; desktop_self_test PASS 23.73 s. Version
   `0.7.0+a9a038430d.modified` (árvore por commitar, declarado). Evidência:
   `gate-round1b-resumo.txt`, `gate-round1b-verbose-counts.txt`, self-test png/txt.
   **Este é o veredicto integrado atual** (substitui o 25/25 do round1 para a árvore com S2/S3).
3. **ENV-03 (3A) CAUSA PROVADA** — agent `env03-debug`, relatório
   [2026-11-20-env03-debug-259.md](2026-11-20-env03-debug-259.md) (47 ficheiros de evidência):
   não é hang; o motor sai sempre código 0; Debug demora ~2.7–3.3 s no leak dump automático do
   CRT em exit (instâncias leaked por design M1, ~1.2 s/instância, stacks capturadas ao vivo);
   o cliente espera 2000 ms hardcoded após EOF → 259. Controlos 0/1/2 leaks provam a
   proporcionalidade; Release 4–25 ms. **Correção proposta (A: uma linha, esperar closeTimeout;
   A2: default da app; B: leak-check off) AGUARDA AUTORIZAÇÃO do dono** — o dispatch manda
   provar a causa antes de propor. Plano red/green pronto.
4. **S6 em curso** (agent `s6-proof`, task-16): matriz bit-a-bit serial≡M5 com prova de uso do
   paralelo + watchdog timing; seam de captura existente encontrado
   (Engine::startCapture/stopCapture). **S7** (baseline Release dispositivo real) aguarda janela
   de máquina calma.
5. **Mapa de commits atualizado**: C1–C6 + **C7 S2/S3** (juce_host.cpp, EngineSession.h/.cpp,
   MainComponent.cpp, tests/state_recovery_tests.cpp + registo) — tudo por commitar, decisão
   do dono; após commit, re-gate em árvore limpa para version==sha (D90).
6. **Linhas de ESTADO adicionais propostas** (o escrivão escreve): as do relatório S2/S3
   (BUG-S2-NOTICE, BUG-S2-RELINK, BUG-S3-STATE com o caminho de perda provado) + as do ENV-03
   (causa provada, correção pendente de autorização) + o veredicto integrado 26/26 (281/3816/0).

## Addendum datado (2026-11-20 ~02:45) — S6 entregue + gate round1c VERDE 27/27 + S7 lançado

1. **S6 entregue** (agent `s6-proof`, relatório próprio
   [2026-11-20-s6-engine-rt-proofs.md](2026-11-20-s6-engine-rt-proofs.md), 6 ficheiros de
   evidência): **serial ≡ M5(2) ≡ M5(4) BIT-EXACT** — 16 corridas, um único SHA-256
   (`0b5bbf92…`), blocos 128/333/512/1024, plugins VST3 reais determinísticos (controlo
   serial×serial bit-idêntico provado primeiro); uso do paralelo PROVADO por contagem de jobs
   dos workers (seam mínimo justificado `RtWorkerPool::workerJobs()` +7 linhas, relaxed add só
   nos workers, audio thread intacto) + stats + caso late-worker com fallback; bónus:
   independência de bloco serial 128≡333≡512≡1024. **Watchdog medido**: Stop com plugin preso
   2205 ms (timeout 2 s dispara, limitado vs stall de 8 s); reset+300 ms → 332 ms;
   reset+3000 ms → 3008 ms (limite documentado: o watchdog não interrompe código de plugin
   dentro do reset). Suite permanente m5_bitexact 2/129 + 3 casos serve aditivos. Merge Debug
   25/26 (ENV-03 pré-existente). D51 default OFF intacto (RAII guard, opt-in explícito).
   Diff revisto pelo coordenador (seam só nos workers). Nenhum defeito no âmbito S6.
2. **Gate integrado round1c** (S6 incluído): `ci/gate.ps1 -Build D:\Core-build\gate-round1c
   -Desktop -Jobs 8`, Release, pasta nova: **VERDE 27/27 (26 merge + 1 desktop), 100%, 0 falhas**,
   417 s; contagens verbose: **286 casos / 3977 verificações / 0 falhas** (19 suite summaries).
   m5_bitexact_tests PASS 17.47 s; engine_process_tests Release PASS 32.76 s (incl. os 3 casos
   novos de watchdog); desktop_self_test PASS 23.77 s. D113 verificado. Version
   `0.7.0+a9a038430d.modified` (por commitar). **Veredicto integrado atual: 27/27.**
   Evidência: gate-round1c-resumo.txt, gate-round1c-verbose-counts.txt, self-test png/txt.
3. **S7 lançado** (agent `s7-rt`, task-17): fase A — instrumentação explícita de
   alloc/lock/IO no callback (new/delete/malloc + IAT hooks file IO/EnterCriticalSection + seam
   mínimo de flag no dataCallback) com **controlos negativos que TÊM de disparar**, e declaração
   do que fica fora (DLLs de plugins, SpinLock intrínseco, terceiros); fase B — baseline Release
   com **dispositivo real** a 64/128 samples, projeto de referência kick+bass+lead, rampa de
   carga, dropouts/max/p99 por bloco, ponto de rutura — em máquina calma (após os builds de A).
4. **Mapa de commits**: C1–C7 + **C8 S6** (rt_workers.hpp +7, tests/m5_bitexact_tests.cpp novo,
   tests/engine_process_tests.cpp +115, registo) — por commitar. Após S7, gate final da ronda 1.
5. **Pendências do dono**: (a) commit C1–C8 (+ relatórios) e re-gate version==sha; (b) autorização
   da correção ENV-03 (A recomendada / A2 default app / B leak-check); (c) M5 default-ON continua
   a exigir o soak 8 h + bit-a-bit no PC do dono (D51) — o bit-a-bit offline está provado (S6).

## Addendum datado (2026-11-20 ~03:35) — S7 fase A: VERMELHO REAL no caminho RT (decisão do dono)

**Achado medido (agent `s7-rt`, stacks simbolizados, controlos negativos todos disparam):**
- **LOCKS no audio thread**: ~4005 EnterCriticalSection + 4006 Leave por 2 s (bloco 64),
  ~3978+3978 (bloco 128) ≈ **1.3 pares CS por processBlock de plugin**; M5(4): audio 1116 pares +
  workers 2889 pares. Raiz: `juce::MidiEventList` usa `Array<Vst::Event, CriticalSection>`
  (juce_VST3Common.h:1289, JUCE estaticamente ligada no exe via core_plugin_host — NÃO é interior
  de DLL de plugin); cada getEventCount/getEvent/addEvent locka, alcançado por processAudio →
  associateWith/hostToPluginEventList/toMidiBuffer (impl.h:2538/3431-3459/2558) + callbacks
  IEventList plugin→host. **Nuance registada**: CS não-contendido = fast path user-mode
  (interlocked), transição ao kernel só em contenção; medido não-contendido durante o process.
  O critério 3 da barra de qualidade («no lock on the audio thread») **NÃO está cumprido como
  escrito** — chamadas de lock presentes no caminho RT, com risco de inversão de prioridade se
  outra thread tocar a lista.
- **ZERO alocações no audio thread E workers** (todo HeapAlloc/new na janela é TimerThread da
  JUCE a postar mensagens — EditControllerParameterDispatcher 60 Hz, impl.h:2250 — ou json do
  control thread; classificado por thread id + stacks). **ZERO file IO, zero SRWLOCK/Sleep/Wait
  no audio thread.** processMutex da VST3 = SpinLock atomics (intrínsecos, auditados, CAS não
  contendido, acquire crash-aware vendored).
- Ferramenta: detector = overrides globais new/delete + IAT hooks (20 entradas: heap, file IO,
  CS, SRWLOCK, waits) + seam mínimo test-only em device.cpp (core::rtProbe: janela in-callback +
  thread id + inject + observador de timing por bloco; custo produção = um relaxed load;
  engine.cpp intacto). Cross-validação: max por bloco do seam == engine.stats maxRenderMs exato.
- Suite permanente s7_rt_probe_tests: wiring + 3 controlos negativos verdes; a assertiva estrita
  zero-locks fica em caso SEPARADO (label rt_strict, fora de merge) como porta de decisão do
  dono — padrão ENV-03 (exclusão explícita + medição separada, nunca verde silencioso).
- **Propostas de correção (NÃO implementadas — autorização do dono, precedente D113 para
  vendored JUCE)**: patch vendored a substituir o array CriticalSection das event lists RT por
  array single-threaded/lock-free (acesso só audio/worker durante process), ou tipo de lock
  no-op para as event lists RT. Decisão registada como pendência; o vermelho é honesto e fica
  documentado até decisão.
- **Fase B em curso** (máquina calma confirmada): baseline Release dispositivo real, 3/6/12/24
  instâncias × 64/128 × 60 s + ponto de rutura + comparação M5, com contagens de lock por cenário.

**Correcção datada (~03:50, full-window ring 65536 — os números ~1.3 pares acima eram subestimativa
por wrap do ring 8192):** serial-64 = 22674 chamadas de lock no audio thread/2 s = 15.06/callback;
serial-128 = 22644/753 = 30.11/callback → **~7.5 chamadas CS ≈ 3.76 pares enter/leave por
processBlock de plugin** (consistente entre blocos). M5(4): audio 6296 + worker/outros 16602,
workerJobs=1496, stalls=0. **NOVO: alocações RARAS no audio thread também encontradas** (3/3/1 por
janela 2 s serial-64/serial-128/M5): HeapAlloc 384 = MidiEventList::addEvent → Array<Event>::add →
**realloc de crescimento** (mesma camada JUCE, stacks capturadas). File IO/SRWLOCK/waits no audio: 0.
O critério 3 não está cumprido por **locks E realloc raro** no caminho RT, mesma raiz MidiEventList;
a direção de correção proposta (array single-threaded/lock-free com capacidade pré-reservada)
endereça ambos. Merge reestruturado: **27/27 verde em Release** (wiring + 3 controlos negativos +
caracterização como dados); porta estrita 0/0/0 em caso separado (CORE_S7_STRICT=1, label rt_strict,
fora de merge) medida VERMELHA = decisão do dono, padrão ENV-03, nada relaxado. Fase B a correr:
Auscultadores (USB-C Audio), 48 kHz, listenDb -15 + ear guard ON, matriz completa + rutura + M5.

## Addendum datado (2026-11-20 ~04:35) — S7 ENTREGUE + RONDA 1 FECHADA (gate final VERDE)

1. **S7 entregue** (agent `s7-rt`, relatório [2026-11-20-s7-rt-measurement.md](2026-11-20-s7-rt-measurement.md),
   47 ficheiros de evidência). Fase A: cobertura explícita (hooked: new/delete + 20-24 IAT entries
   heap/fileIO/CS/SRWLOCK/waits; auditado: SpinLock atomics, mutex control-thread, arrays fixos/SPSC,
   MidiMessage inline, cache atómica de automação; fora: DLLs de plugins/terceiros/OS/JUCE atomics).
   **Controlos negativos TODOS disparam** (malloc/new/IO/CS no audio thread, stacks). Caminho do
   motor: **ZERO file IO/waits/SRWLOCK**; locks ~3.76 pares CS/processBlock + realloc raro
   (MidiEventList, camada JUCE estática) = **critério 3 NÃO cumprido como escrito** (porta rt_strict
   VERMELHA = decisão do dono; propostas: patch vendored array single-threaded/lock-free ou lock
   no-op, precedente D113). Fase B baseline real (Auscultadores USB-C, WASAPI 48 kHz, 14×60 s,
   −15 dB + ear guard): **0 overruns até 72 instâncias @64** (max 92.2% load); **rutura em 96**
   (30 overruns/45012 cb = 0.067%, tail-first, maxLoad 130% vs mean 28%); motor continuou a tocar,
   0 erros/stalls. **M5(4): p99 2.3× melhor** (0.131 vs 0.306 ms @24 instâncias), p50 2.6×,
   workerJobs=388134. Shared: **0 starvations** (bate com o teste real do dono de 17/11); exclusive
   small-buffer: starvations ~54/s INDEPENDENTES da carga (3374 idênticos em 3/6/12) = pacing de
   bursts ~18 ms da interface vs tolerância 7.33 ms do detetor — não é sobrecarga do motor;
   underrun de hardware em exclusive não provável engine-side → pendência declarada. Limitações:
   plugins de teste (não a biblioteca do dono), bloco de plugin fixo 128 (CSV-09), 60 s não é soak D51.
2. **GATE FINAL DA RONDA 1** (`gate-round1d`, Release, pasta nova, D113 verificado):
   - **Registado merge: VERDE 27/27, 100%, 0 falhas, 89 s** — contagens verbose: **292 casos /
     4054 verificações / 0 falhas** (20 suite summaries; +6/+77 vs round1c = s7_rt_probe_tests).
   - **desktop_self_test: PASS 23.73 s** (1/1).
   - All-labels run: 27/29 — a ÚNICA falha é `s7_rt_strict_tests` (label **rt_strict**, 1/12/4),
     a porta de decisão do dono, VERMELHA POR DESIGN (locks/allocs MidiEventList ≠ 0), padrão
     ENV-03 (exclusão explícita do âmbito de integração + medição separada, nada relaxado).
     **O gate aplicável à integração (merge+desktop) está VERDE 28/28.**
   - Version `0.7.0+a9a038430d.modified` (árvore por commitar; version==sha após commit + re-gate limpo).
3. **RONDA 1 COMPLETA**: S11 ✅ S1 ✅ S5 ✅ S4 ✅ S2/S3 ✅ 3A ✅ (causa provada) S6 ✅ S7 ✅.
   Diff integrado: 19 ficheiros rastreados +880/−74 + novos testes/relatórios/evidência.
   Baseline a9a0384: 21 merge → **27 merge + 1 desktop** (+6 testes novos: export_plugin_failure,
   wav_float, installer_samples, m5_bitexact, s7_rt_probe, state_recovery; +3 casos watchdog em
   engine_process; contrato cli_plugin_crash_render reescrito com justificação). Nada removido/
   enfraquecido; os 18 nomes originais da lista merge preservados.
4. **Decisões acumuladas do dono** (nenhuma bloqueia o trabalho seguinte):
   (a) **commit C1–C9** + relatórios (mapa no manifest) + re-gate version==sha;
   (b) **ENV-03**: A (1 linha, recomendada) / A2 (default app) / B (leak-check off);
   (c) **locks RT MidiEventList**: autorizar patch vendored (array single-threaded/lock-free +
       capacidade pré-reservada, endereça locks E realloc) ou aceitar o estado atual (porta
       rt_strict continua vermelha como decisão registada);
   (d) **M5 default-ON**: soak 8 h + bit-a-bit no PC do dono (D51) — bit-a-bit offline já provado;
   (e) **S13 UX**: revisão do dono à slice A;
   (f) pendências menores: retenção de backup no sucesso do publish, .part legacy CLI (SWEEP-02),
       stems crash-atómicos, starvation exclusive (hardware), subnormais f64→f32.
5. **Vaga seguinte lançada** (ordem do dono): frente **VST3 host** (EchoBoy/Decapitator, task-4,
   capacitador da Chain) + **S8 undo** (medição, task-18) + **S9 mute** (medição, task-19).
   Depois: Chain áudio (série→paralelo→sidechain, task-6, paralelo gated por S2-timing/task-5),
   Chain parâmetros (task-8), S10 VST2 (caso concreto), S12 restos/Q8, S13 (revisão do dono).

## Addendum datado (2026-11-20 ~05:30) — Vaga 2 FECHADA (S8/S9/VST3) + decisões consolidadas

1. **S9 entregue** ([s9-mute-block.md](2026-11-20-s9-mute-block.md), 33 evidências, suite 5/194,
   merge 28/28): mute paga CPU TOTAL (gate só no fader graph.cpp:1329; bypass poupa o insert,
   mute não poupa nada — 0.71% de um core serial para 3 tracks de teste, escala com plugins reais);
   bloco-128 fixo = **latência de gather constante +128 amostras** (tracksSamples 256 invariante
   em todos os blocos), NÃO chamadas extra — problema SEPARADO do mute. Quatro coberturas
   caracterizadas (cauda drena em mute → política de cauda é decisão; sidechain M7c dormente para
   plugins; PDC invariante sob mute; retoma bit-exact warm, ramp 31 amostras, sem clique).
   Propostas P-F-S9-A (mute-suspend) e P-F-S9-B (block-match) NÃO implementadas — decisões.
2. **S8 entregue** ([s8-undo-cost.md](2026-11-20-s8-undo-cost.md), probe + CSV run3 limpo,
   integridade 0 falhas): **serialização ≈97–98% do commit** (t32_s318128 151 ms vs counterfactual
   3.27 ms → ~46× headroom; t32_s1048576 415 ms); undo/redo ≈ puro toJson+dump; **ceiling preços
   só os dumps** — gap não-preçado 158 MB, peak WS 1727.6 MB vs 1 GB nominal (~1.7×),
   BUG-S8-MEM-ACCOUNTING candidato; conservação do ceiling verificada (D-a refutado);
   modified() first-ask 12–42 ms (follow-up 08). Proposta P-F-S8-SHARED (COW + contadores) com as
   três provas esboçadas — NÃO implementada, decisão do dono. history.cpp intacto.
3. **Frente VST3 entregue** ([vst3-host-soundtoys.md](2026-11-20-vst3-host-soundtoys.md), 59
   evidências): EchoBoy/Decapitator 5.5.5.19885 — scan/load/editor/bypass/render/automation
   (Mix/Drive)/tail **PASS** (tail EchoBoy 5.49 s, 11 ecos, spacing 0.25 s = preset). **Defeito
   candidato BUG-VST3-ST-RECALL**: estado Soundtoys capturado SEM um move de parâmetro com o
   editor aberto → defaults de fábrica no reopen (ambos os plugins); com editor+move restaura;
   fluxo do Core são (CoreTestGain bit-idêntico). Impacto: músicas configuradas só host-side
   (macros/randomize/live writes) perdem settings Soundtoys no reopen; tweaking normal no editor
   restaura. Atribuição **suspeita plugin-side, NÃO confirmada** (segundo host não realizado).
   4 mitigações como decisões. **VST2 recusado pelo engine** (project.cpp:387, VST3-only) → S10;
   18 DLLs VST2 SoundToys presentes, sem projetos antigos. Owner presets/projetos: não encontrados
   (Song.core missing; único «Psy demo.core» sem plugins) → casos owner-preset não executados.
   **NOTA DE TRANSPARÊNCIA**: durante a automação UI do segundo host (Bitwig), um SendKeys
   mal focado acrescentou «EchoBoy» a um rascunho ChatGPT do dono (NÃO ENVIADO; tentativas de
   reparação não landed; deixado intacto; a app/engine do dono NUNCA tocados). Incidente declarado
   no relatório; sem dano (texto não enviado). Lição: automação UI de teclas em máquina live é
   frágil — as provas seguintes preferem seams headless.
4. **DECISÕES CONSOLIDADAS DO DONO** (nenhuma bloqueia as provas em curso; todas para implementação):
   - **D-commit**: commit C1–C9 + relatórios (mapa no manifest) + re-gate version==sha em árvore limpa.
   - **D-ENV03**: correção A (1 linha, esperar closeTimeout — recomendada) / A2 (default app) / B (leak-check off Debug).
   - **D-RT-LOCKS**: patch vendored MidiEventList (array single-threaded/lock-free + capacidade
     pré-reservada, endereça locks ~3.7 pares/processBlock E realloc raro) OU aceitar (porta rt_strict fica vermelha).
   - **D-M5**: default-ON exige soak 8 h + bit-a-bit no PC do dono (D51); bit-a-bit offline já provado (S6).
   - **D-S8**: avançar shared-blob (com as 3 provas) ou aceitar o custo; BUG-S8-MEM-ACCOUNTING.
   - **D-S9**: mute-suspend (com política de cauda + 4 coberturas) e block-match (latência vs amortização).
   - **D-VST3-RECALL**: mitigação do recall Soundtoys (editor-init antes de capture / forçar
     controller state / aceitar+documentar) + atribuição via segundo host (Bitwig/Ableton/Cakewalk disponíveis).
   - **D-CHAIN-ARCH**: paralelo/sidechain de plugins NÃO implementado (M7c dormente) — decisão de
     arquitetura numerada (modelo de routing, identidade de instâncias, PDC entre ramos, alinhamento).
   - **D-UX**: revisão do dono à slice A (S13).
   - Menores: retenção de backup no sucesso do publish; .part legacy CLI (SWEEP-02); stems
     crash-atómicos; starvation exclusive (hardware); subnormais f64→f32; modified() dirty-flag (08).
5. **Provas em curso** (sem decisões necessárias): **timing/latência** (task-5, agent timing-proof —
   gate do paralelo da Chain; PDC/MIDI/automação, impulso+atraso conhecido, matriz SR/bloco;
   facto mapeado: paralelo M7a bus routing JÁ implementado, sidechain M7c dormente, mudança de
   latência via checkPluginLatency tick 40 ms) + **Chain série** (task-6 fase série, agent
   chain-series — valida a cadeia de inserts existente com EchoBoy→Decapitator + inversa,
   save/reopen, reorder/bypass; caracteriza paralelo/sidechain para a decisão de arquitetura).
   Restantes: Chain parâmetros (task-8, Delay Drive — precisa do sistema de macros, decisão),
   S10 VST2 (caso concreto + SDK/licença), S12 restos/Q8 (projeto antigo com sample_host + render
   de referência), S13 (revisão do dono).
6. **Manutenção de disco (2026-11-20 ~05:45)**: D: ficou 100% cheio (0 bytes) durante o build do
   timing-proof (C1085). O agente adaptou-se com segurança (build próprio em C:\Core-build\timing,
   443 GB livres; removeu só o seu parcial em D:). O coordenador libertou **56 GB** em D: removendo
   árvores de build SUPERSEDIDAS e reproduzíveis desta ronda (gates round1/b/c; builds de prova
   S11 clean/local-unpatched/local-patched/release-fixed; subdirs build/ dos agentes concluídos
   s1-red/s1-fix/s2s3/s6/s5-fix/s4-fix/s8). **Preservados**: core-review (ambiente canónico),
   gate-round1d (gate final ativo), todos os subdir probe/ (fontes+executáveis pequenos),
   toda a evidência arquivada em docs/relatorios/evidencia, e os builds dos agentes ativos.
   D: passou de 1.3 GB → ~57 GB livres. Nada de fonte/evidência/produto removido; builds são
   reproduzíveis a partir do SHA + receitas arquivadas. Lição: builds out-of-tree acumulam-se
   (~288 GB em D:\Core-build de muitas rondas); limpeza periódica das árvores superseded é
   manutenção legítima (não é limpeza de código/dados/história, AGENTS.md).
