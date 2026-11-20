# Relatório — 2026-11-20 — Confirmação dos 10 achados do CSV do dono (leitura @ a9a0384)

Delivery: DSH lead session `a69b320fbfab`, subagents `csv-triage` (CSV-01…05) and `csv-triage-2`
(CSV-06…10), 2026-11-19 ~23:45 → 2026-11-20 ~00:15 Europe/Lisbon. Owner instruction:
«@…table-678f46e5….csv depois confirma». Scope: **read-only confirmation** of the owner's 10 open
findings against the current code. Nothing was built, run or edited in the repository by this
triage; the only writes were the out-of-repo notes (archived verbatim in the evidence directory).

## O que foi medido

- Repositório `d-aicore/core`, branch `main`, SHA `a9a038430d781959829f8510067ce59afffaad62`
  (= `origin/main`, re-verified by both agents). Uncommitted tree: CMake sanitizer wiring +
  docs (ignored for triage; declared in the stage-0 report).
- Âmbito: confirmar/refutar cada achado no código ATUAL. Os `file:line` do CSV vêm de um snapshot
  anterior ao pull de 86 commits — cada achado foi localizado por símbolo/conteúdo, não pelas
  linhas antigas.
- Método: leitura + `git grep`/`git log` read-only; cruzamento com `ESTADO.md` (pendências FL-2,
  ART-018, D113 infra, M5/SUR-16) e os relatórios 18/11 (engine-rt-fixes, state-per-edit-root,
  plugin-crash-followups, sweep-batch-05-03).
- Testes executados: **nenhum** — classificação «confirmado no código», não «comprovado por
  teste». As reproduções executadas (vermelho) estão a cargo dos agentes da ronda S
  (`s1-red`, `s5-red`, `s11-proof`) e serão reportadas nas entregas próprias.
- Limitações: leitura não prova runtime; o resultado de um gate sem o patch JUCE não é mensurável
  por leitura (CSV-08); a experiência de UI (avisos, diálogos) não foi clicada.

## Resultados

| ID | Achado (PT verbatim, resumido) | Onde no CSV | Estado @ a9a0384 | Evidência atual | Mapeia para |
| --- | --- | --- | --- | --- | --- |
| CSV-01 | «Exportação diz "Exported" com plugin falhado ou não carregado — só verifica áudio em falta» | render_job.cpp:363-367 | **CONFIRMADO** | `engine/src/core/render_job.cpp:363-367` (único check pré-escrita = `missingAssets()`); `describe()` :417-443 sem campo de falha de plugin; `MainComponent.cpp:6765` «Exported» incondicional; `graph.cpp:1239/1273/1101` salta plugins falhados, `:300` regista `missingPlugins_` que o export nunca lê | **S1** |
| CSV-02 | «O motor diz porquê que cada plugin não carregou; a app só usa isso para desactivar "Open…" — nunca avisa» | EngineSession.cpp:644-648 | **CONFIRMADO** | `EngineSession.cpp:644-648` guarda `missingPlugins[key]="name: why"`; únicos consumidores `MainComponent.cpp:4687/4698/5300` = `setEnabled` do «Open…»; o valor nunca é mostrado; contraste: `missingAssets` define `snap_.message` (:650-652). O «Try again» (SWEEP-05) cobre `failedPlugins` (falha DURANTE o play), conjunto diferente | **S2** |
| CSV-03 | «relinkPlugins existe e tem zero chamadas» | plugin_refs.cpp:63 | **CONFIRMADO** | `app/domain/src/plugin_refs.cpp:63` definição + `plugin_refs.hpp:23` declaração; `git grep relinkPlugins -- app engine tests ci` = só essas 2 linhas | **S2** |
| CSV-04 | «Estado de plugin inválido é engolido em silêncio (plugin volta aos valores de fábrica)» | juce_host.cpp:348,351 | **CONFIRMADO** | `engine/src/plugins/juce_host.cpp:347-352`: decode Base64 sem `else` (sem why/trace/marca); `setStateInformation` é void — blob inválido aceite pelo decode deixa o plugin nos defaults sem report; `why` só existe para falhas de LOAD (:332/:338/:341/:398). O handshake statesNeeded (state-per-edit) verifica PRESENÇA, não validade | **S3** |
| CSV-05 | «Sem verificação de NaN/Inf na importação de WAV float» | audio_file.cpp:115-140 | **CONFIRMADO** | `engine/src/io/audio_file.cpp:139-140` memcpy raw de float32/float64 sem `isfinite`; header validation :84-88 só format/channels/rate/bits; `assets.cpp:49` não acrescenta check; zero ocorrências isfinite/isnan em `engine/src/io`. Mitigações parciais na app (analysis/stats/Cut silence) NÃO sanitizam o que o motor toca/exporta. Contenção downstream: ART-018 silencia sub-blocos de 32 amostras | **S4** |
| CSV-06 | «Instalador ainda sobrescreve um sample que o utilizador editou» | Setup.cpp:88-93 | **CONFIRMADO** | `app/installer/Setup.cpp:89-92`: heurística de skip = tamanho+mtime apenas; qualquer edição do utilizador a um sample instalado é sobrescrita sem hash/backup/pergunta; `samples.txt` protege só a desinstalação | **S5** |
| CSV-07 | «Não existe teste bit-a-bit serial≡M5, nem teste do watchdog de reset» | engine_tests.cpp | **PARCIAL** | Watchdog: **EXISTE** — bounded-reset test @ `tests/engine_process_tests.cpp:489-527` (88aa357), não em engine_tests.cpp; re-arm/swap em `engine_tests.cpp:720/957/1039`. Bit-a-bit serial≡M5: **CONTINUA AUSENTE** — os testes M5 existentes verificam contadores/peaks; matriz com plugins reais = follow-up declarado SUR-16 | **S6** |
| CSV-08 | «O patch do JUCE (D113) não é aplicado por nada no repo → risco de gate falso-verde num fetch novo» | ci/patches/…patch | **CONFIRMADO** | `CMakeLists.txt:64-67` FetchContent sem `PATCH_COMMAND`; `ci/gate.ps1:33/51` e `ci/compilar.bat:198-207` só passam o caminho do JUCE; nada aplica/verifica os 14 hunks. Nota: o host chama `markCrashedByHost()` — um fetch novo deve falhar a COMPILAÇÃO (não verde silencioso); o falso-verde regista-se para o caso de o símbolo deixar de ser referenciado. Pendência ESTADO.md:357 | **S11** |
| CSV-09 | «Faixas em mute pagam CPU total; bloco de plugin fixo em 128» | graph.cpp:1239/1302 | **CONFIRMADO** | `engine/src/core/graph.cpp:1302-1304`: mute só consultado no fader DEPOIS de instrument+clips+inserts correrem (serial e M5); `juce_host.cpp:23/396`: bloco live compile-time 128 (knob 256/512 só export). A antiga :1239 é agora o guard `failed()` | **S9** |
| CSV-10 | «Undo copia o documento inteiro e serializa-o em JSON a cada commit só para lhe calcular o preço» | history.cpp:9,14-18 | **CONFIRMADO** | `app/domain/src/history.cpp:8/14-18/51`: push por valor (cópia inteira do Document) + `toJson`+`dumps` só para `.size()`; também em undo/redo/gesture-end. Explicitamente NÃO corrigido por 5df584f — follow-up declarado slice 1 | **S8** |

**Contagens: 9 CONFIRMADOS, 1 PARCIAL, 0 REFUTADOS, 0 não-verificáveis-por-leitura.**
Nenhum achado foi corrigido nesta entrega (a confirmação não autoriza implementação; as correções
são as etapas S da ronda do dono, com vermelho-medido antes e verde depois).

## Alterações

- Este relatório + linha no índice + evidência:
  `evidencia/2026-11-19-pull-a9a0384-environment-refresh/csv-triage-notes-full.md` (notas verbatim
  dos dois subagentes, 10 secções com citações PT, excertos de código, raciocínio e direções) e
  `owner-csv-10-findings.csv` (o CSV do dono, arquivado).
- Nenhum ficheiro de produto/teste tocado. `ESTADO.md` não editado (D72/D80) — linhas propostas abaixo.

## Verificação

Comandos representativos (read-only, @ a9a0384): `git rev-parse HEAD`; `git grep -n relinkPlugins
-- app engine tests ci` (2 linhas); `git grep -n missingPlugins -- app` (3 consumidores, todos
`setEnabled`); leitura dirigida de `render_job.cpp:336-443`, `juce_host.cpp:320-400`,
`audio_file.cpp:80-160`, `Setup.cpp` (copy/manifest/uninstall), `graph.cpp:1230-1310`,
`history.cpp:1-60`, `engine_process_tests.cpp:489-527`. Sem builds, sem suites, sem cliques de UI.
Os excertos completos estão nas notas arquivadas.

## Pendências e passagem

- **Ronda S do dono** (dispatch «Passagem para o coordenador dos Qwens», 2026-11-20): S11 → S1 →
  S5 → S4 → S2/S3 → S6/S7, depois S8–S13 e as frentes VST3/Chain. Agentes já desplegados:
  `plan-hunt` (localizar o plano original do clone com commit `1c6238b` + extrair Q1–Q8 + verificar
  itens já integrados), `s11-proof` (prova de build limpo: fetch novo vs cópia local sem patch vs
  cópia com patch + teste crash/restart), `s1-red` e `s5-red` (vermelho medido das prioridades de
  dados). As correções aguardam a consulta às oito perguntas do plano original (Q1 export, Q7 JUCE,
  Q8 restos do som próprio) antes de alterar comportamento de produto.
- **Cruzamentos com o ledger**: CSV-01/02/03/04 = pendência FL-2 (ESTADO.md:355, ronda 18/11);
  CSV-05 = ART-018 (:332); CSV-08 = infra D113 (:357); CSV-07 = M5/SUR-16 (:356); CSV-10 =
  follow-up slice 1 do state-per-edit. Nenhum destes fecha por esta entrega — a confirmação
  revalida-as no SHA atual.
- **Linhas propostas para ESTADO.md (o escrivão escreve, D72/D80):**
  - *Estado medido — Triagem CSV do dono (2026-11-20, a9a0384, só leitura)*: 10 achados confirmados
    no código atual — 9 CONFIRMADOS (export «Exported» com plugin falhado render_job.cpp:363;
    missing-plugins nunca anunciado EngineSession.cpp:644; relinkPlugins sem chamadas
    plugin_refs.cpp:63; estado inválido silencioso juce_host.cpp:347; WAV float sem isfinite
    audio_file.cpp:139; instalador sobrescreve sample editado Setup.cpp:89; patch JUCE não aplicado
    pelo repo CMakeLists.txt:64; mute paga CPU total graph.cpp:1302 + bloco 128 juce_host.cpp:23;
    undo copia+serializa o documento inteiro history.cpp:8) + 1 PARCIAL (watchdog de reset JÁ tem
    teste engine_process_tests.cpp:489; bit-a-bit serial≡M5 continua ausente, SUR-16). Nada
    executado/corrigido; reproduções e correções = ronda S (S1/S2/S3/S4/S5/S6/S8/S9/S11).
    Relatório: docs/relatorios/2026-11-20-csv-10-findings-confirmation.md.
  - *Pendência — plano original noutro clone*: o commit local `1c6238b` (plano de continuação +
    relatório, sem push/testes) não está no checkout principal; `plan-hunt` procura o clone;
    Q1–Q8 devem ser consultadas antes de decisões de produto da ronda S.
- **Retoma**: SHA a9a0384 + wiring local por commitar; notas verbatim em
  `D:\Core-build\core-review\csv-triage-notes.md` (cópia na evidência); próximos passos =
  resultados de `plan-hunt`/`s11-proof`/`s1-red`/`s5-red` e a ronda S no task board.
