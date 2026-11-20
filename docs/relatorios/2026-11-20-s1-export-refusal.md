# Relatório — 2026-11-20 — S1: exportação recusada quando um plugin necessário falha (dados primeiro)

Delivery: DSH lead session `a69b320fbfab`, agents `s1-build`/`s1-repro`/`s1-repro-2` (vermelho,
casos a–e) and `s1-fix` (correção + regressão), coordinator (revisão do diff, gate integrado),
2026-11-20 Europe/Lisbon. Owner authorization: S-round dispatch 2026-11-20, item S1 («recusar a
exportação se um plugin necessário ao sinal exportado não carregar ou falhar durante o render.
Mostrar o nome, a pista e a razão. Distinguir um plugin em bypass intencional de uma falha… Na
falha, o WAV anterior fica byte a byte igual e a app não mostra "Exported"… uma falha tardia não
substitui apenas parte do conjunto»). Q1 do plano original não consultável (1c6238b não
localizável — ver S11); o contrato explícito do dispatch governa.

## O que foi medido

- Repositório `d-aicore/core`, `main @ a9a038430d781959829f8510067ce59afffaad62` (= origin/main);
  alterações locais build-affecting preservadas (CMake ENV-01+D113, S5 Setup.cpp, S4 audio_file.cpp,
  registos de testes concorrentes).
- Ambiente: Windows 11 x64, MSVC 19.51.36260, CMake 4.4.4, Ninja 1.13.2; vcvars64 + VSLANG=1033 +
  chcp 65001. Builds próprios: `D:\Core-build\s1-red\build` (vermelho, 555 alvos, 6 plugins VST3
  de teste), `D:\Core-build\s1-fix\build` (Debug, JUCE local com patch D113 verificado).
- Testes executados: reprodução headless CLI (casos a–e, incluindo 6 kills TerminateProcess para
  o conjunto parcial), regressão permanente in-process `export_plugin_failure_tests` (5 casos /
  54 verificações) vermelho→verde, casos CLI com plugins REAIS (Core Test Synth / Core Test Crash,
  SEH access-violation real), ctest -L merge completo (24 registados).
- Limitações: headless — o aviso da app verificado por código (MainComponent.cpp:6765 succeeded→
  «Exported»; ramo de falha :6767-6781 mostra job.error verbatim; SEM edição de MainComponent);
  desktop self-test não corrido por este agente (gate integrado do coordenador cobre-o); Debug,
  48 kHz/24-bit/stereo, bloco 512 no CLI (app kBlock 4096 só por código); conjunto parcial
  mid-publish (janela sub-ms de renames) = veredicto por leitura, não medição; variante de crash
  compile-time (CORE_TEST_CRASH) — o crash real é incondicional 0.1 s após play.

## Resultados

| ID | Problema / resultado | Evidência | Classificação | Estado |
| --- | --- | --- | --- | --- |
| S1-RED-a | Load failure: exit 0, ok:true, track fantasma silenciosa; «missing» só no attachment CLI (main.cpp:710) + stderr; WAV byte-idêntico ao controlo saudável (ff4b1f30…) | `red/s1-repro-notes.md` caso a, a-answer.json | comprovado por teste | vermelho medido |
| S1-RED-b | Crash SEH durante o render: exit 0, ok:true, **missing:[]** — nenhum traço no resultado; WAV byte-idêntico ao controlo; 3 corridas iguais | caso b, b-answer.json, b-stderr.txt | comprovado por teste | vermelho medido |
| S1-RED-d | **PERDA DE DADOS**: export «sucedido» degradado SUBSTITUI o export saudável anterior (prova content-distinct: d917fe28…→ff4b1f30…) e APAGA o único backup — publishRenderOutputs renomeia old→.previous (:158), part→target (:165), e **apaga o .previous no sucesso (:181-184, remove :183)**; zero artefactos de backup (scan recursivo). SWEEP-03 só protege o caminho de FALHA de publicação | caso d, d-prev2/d-overwrite-answer.json; render_job.cpp:110-186 | comprovado por teste + confirmado no código | vermelho medido |
| S1-RED-e | Stems: conjunto completo publicado com stem do crash TODO-ZERO (peak 0, 9363ef6d…); 3 crashes SEH + 3 instâncias leaked por export; silêncio passa checkExportLevel (:238-249). Kills mid-render/mid-write (6×): SEM conjunto final parcial (só .part órfãos); morte entre renames de publish PODE deixar parcial — veredicto por leitura (janela sub-ms, :163-168, sem seam headless) | caso e, e-answer.json, e-stems*.wav, artefactos e-k*/e-l* | comprovado por teste (kills) + leitura (mid-publish) | vermelho medido |
| S1-FIX | `checkExportPlugins(const Graph&)` partilhado, chamado nos DOIS pontos das DUAS vias (após construir o graph = falhas de load, antes de qualquer working file; após render e ANTES de publish = falhas durante o render). Lança com nome/pista/razão de cada plugin; a maquinaria de falha existente faz o resto (app: discard→State::Failed→describe error; CLI: ok:false→exit 1). Accessors `missingNeededPlugins()`/`failedPlugins()` reutilizam {key,plugin,why} e o mesmo flag failed() do engine.stats. **Bypass ≠ falha** (bypass é build-time, grep-verificado; excluído dos dois conjuntos, incluindo efeitos bypassed com ficheiro ausente). Stems: mix primeiro, qualquer falha necessária recusa o CONJUNTO TODO. Sem edição de MainComponent | graph.hpp:113-119/398, graph.cpp:282-302/326-341, render_job.hpp:66-72, render_job.cpp:294-316/391-393/418-423, server.cpp:152-154/163-165; diff revisto pelo coordenador | confirmado no código + comprovado por teste | corrigido (por commitar) |
| S1-REG | Regressão permanente `tests/export_plugin_failure_tests.cpp` (in-process, RenderJobs+renderProject, providers falsos para o estado pós-SEH; 5 casos/54 verif). **Vermelho não-corrigido: 5/54/33 falhas** (state succeeded esperado failed; WAV anterior diferente; caso 3 ok; bypass parcial). **Verde: 5/54/0**. CLI plugins reais: (1) load-fail RECUSADO + WAV anterior SHA-idêntico; (2) crash SEH RECUSADO antes de publish + anterior intacto + sem hang; (3) saudável sucede peak 0.40157; (4) stems+crash CONJUNTO TODO recusado, 3 alvos SHA-idênticos, sem .part; (5) efeito crash bypassed sucede (controles ativo/master recusam in-process) | `fix/red-inprocess.txt`, `fix/red-cli.txt`, `fix/green-inprocess.txt`, `fix/green-cli-cases.txt` | comprovado por teste | verde |
| S1-CONTRACT | **Mudança de contrato sancionada, não enfraquecimento**: `cli_plugin_crash_render` asserting sucesso de export com crash — exatamente o defeito S1 — reescrito para o contrato de recusa, MANTENDO o TIMEOUT anti-hang + prova de que o crash aconteceu + controlo saudável. Registado explicitamente (AGENTS.md: não relaxar critérios para obter verde; aqui o critério antigo codificava o defeito) | tests/cli_plugin_crash_render.cmake (+159/−… reescrita); diff revisto | confirmado no código | alterado com justificação |
| S1-MERGE | ctest -L merge no build integrado do agente: **24 registados = 21 baseline + export_plugin_failure + wav_float (S4) + installer_samples (S5)**; **23/24 passam**; só engine_process_tests falha com o ENV-03 Debug conhecido (exit 259, :159, caso live-playback; outros 14 casos passam) — **provado pré-existente: o build NÃO-corrigido s1-red falha identicamente**. Excluindo-o: **23/23, 100%** (os 20 do baseline passam todos). Por suite (testes/verif, 0 falhas): engine 39/221, wav_float 16/360, domain 33/568, modules 6/128, role 21/215, midi 20/402, bug 48/462, bitwig 2/25, audit 7/65, audit2 15/364, export_plugin_failure 5/54, hunt 11/106, round3 7/82, calls 12/69, menu 7/301, window 10/70 + 7 cli | `fix/merge-full.txt`, `fix/merge-excl.txt` | comprovado por teste | verde com ENV-03 declarado (tarefa 3A) |

## Alterações

- `engine/src/core/graph.hpp/.cpp` — accessors missingNeededPlugins/failedPlugins + registo de
  bypass build-time (+31/−… e +8).
- `engine/src/core/render_job.hpp/.cpp` — checkExportPlugins + dois pontos de chamada na via app (+33/+7).
- `engine/src/core/server.cpp` — dois pontos de chamada na via CLI (+6).
- `tests/export_plugin_failure_tests.cpp` — novo; registo tests/CMakeLists.txt:77-84/151.
- `tests/cli_plugin_crash_render.cmake` — reescrito para o contrato de recusa (justificado acima).
- `app/desktop/src/MainComponent.cpp` — **NÃO tocado** (o ramo de falha existente já mostra a razão).
- Este relatório + índice + `evidencia/2026-11-20-s1-export-refusal/` (red 19 + fix 11 ficheiros).
- `ESTADO.md` não editado (D72/D80) — linhas propostas abaixo.

## Verificação

- Vermelho (antes): in-process 5/54/**33 falhas**; CLI real «the export was NOT refused (exit 0)»
  com ok:true — igual à reprodução a–e.
- Verde (depois): in-process 5/54/**0**; CLI real casos 1–5 na tabela acima (SHA256 dos WAVs
  anteriores byte-idênticos nas recusas; conjunto de stems todo recusado; bypass sucede).
- Merge integrado (Debug, build do agente): 23/24; ENV-03 pré-existente provado no build não-corrigido.
- **Gate registado na revisão integrada** (ci/gate.ps1 -Desktop, Release, pasta nova
  D:\Core-build\gate-round1): corrido pelo coordenador — resultado registado no relatório da
  ronda 1 (docs/relatorios/2026-11-20-round1-coordination.md) quando disponível; este relatório
  não declara o gate integrado antes de o ver.
- Diff revisto pelo coordenador: pontos de chamada corretos (antes de working files / antes de
  publish), bypass excluído, mensagens nomeadas, contratos JSON dos outros estados inalterados.

## Pendências e passagem

- **Commit**: correção S1 + regressões aguardam decisão do dono (nada commitado).
- **Recomendações registadas (NÃO implementadas — decisões separadas)**: (1) reter o backup
  `.previous` também no caminho de SUCESSO do publish (hoje apagado, render_job.cpp:183) — o
  contrato S1 protege a falha; a retenção no sucesso é decisão de produto; (2) estender a
  reserva exclusiva SWEEP-02 ao `.part` legacy fixo do CLI (server.cpp:795); (3) colocação
  crash-atómica dos stems (janela sub-ms de renames) — decisão separada.
- **3A (ENV-03)**: mantém-se tarefa própria; este merge confirma o padrão (Debug live-playback
  case 259; Release passa — gate integrado verifica).
- **Linhas propostas para ESTADO.md (o escrivão escreve, D72/D80):**
  - *Estado medido — S1 exportação (2026-11-20, a9a0384 + correção local por commitar)*: vermelho
    medido a–e (export degradado byte-idêntico ao saudável publicado como sucesso; crash sem
    traço no resultado; **WAV anterior saudável substituído e único backup apagado no sucesso**
    render_job.cpp:183; stem todo-zero publicado; kills mid-render sem conjunto parcial);
    correção checkExportPlugins nos dois pontos das duas vias (load + durante o render, antes
    de publish), nome/pista/razão, bypass≠falha, stems conjunto todo, app mostra a razão pelo
    ramo de falha existente (MainComponent não tocado); regressão export_plugin_failure 5/54
    vermelho 33 falhas→verde 0 + CLI plugins reais 5 casos (SHA anterior intacto nas recusas);
    cli_plugin_crash_render reescrito para o contrato de recusa (anti-hang+crash-proof mantidos);
    merge 23/24 com ENV-03 pré-existente provado (23/23 excluindo). BUG- S1 proposto nas notas.
  - *Pendências*: commit S1 (dono); retenção de backup no sucesso = decisão de produto; .part
    legacy CLI = follow-up SWEEP-02; stems crash-atómicos = decisão separada; 3A continua.
  - *Referência*: docs/relatorios/2026-11-20-s1-export-refusal.md + evidência (30 ficheiros).
- **Retoma**: SHA a9a0384 + correções locais; builds D:\Core-build\s1-{red,fix}\build; gate
  integrado D:\Core-build\gate-round1; próxima ação = relatório da ronda 1 + decisão de commit.
