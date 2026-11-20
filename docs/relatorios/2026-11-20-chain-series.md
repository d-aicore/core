# Relatório — 2026-11-20 — Chain áudio: série provada, paralelo M7a alinhado, sidechain dormente; clique em edições live

Delivery: DSH lead session `a69b320fbfab`; agent `chain-series` (arquitetura + provas série/reopen/dup/live/paralelo,
morreu antes do relatório), coordenador (este relatório a partir dos dados medidos). Owner authorization:
S-round «Frentes próprias» / stage 6 — «Inspeciona a Chain atual e implementa ou valida primeiro a ligação
de áudio em série… EchoBoy → Decapitator e a ordem inversa… mesma sinal e presets… guardar e reabrir…
reorder e bypass em reprodução… cliques, quebras ou perda de estado… paralelo com impulso e atraso conhecido…
sidechain: qual o sinal que chega à entrada auxiliar». PROOF front — sem edições de produto. Snapshot
`a9a0384` + round-1/S2/S3/S6/S7/S9 preservadas. Binários Release `D:\Core-build\gate-round1d` (gate VERDE).
Estados Soundtoys editor-init (o recall exige move com editor aberto — frente VST3). Null backend, 48 kHz.

## O que foi medido

- Arquitetura (graph.cpp/project.cpp/effects.hpp): a cadeia série JÁ existe como inserts de track
  (`Effect` chain, ordem visual = ordem de sinal). Instâncias identificadas por key `fx:<trackId>:<effectId>`
  (duas do mesmo plugin = effectIds distintos, estados independentes). Bypass: `Effect.bypass` mantido na
  cadeia, saltado em graph.cpp:1300 (place + tail ficam; drops do PDC map :539). Rebuild em edit: commit →
  novo Graph, mesmos hosts (shared_ptr por key), ramp/glide/tail adotados (graph.cpp:848-889, adoção só quando
  tamanhos batem :868). **Sem gesto de reorder na app hoje** (reorder via document edit). Paralelo = M7a bus
  routing (output/sends → bus → master, PDC recombination). Sidechain M7c dormente para plugins.
- Provas: série A (EchoBoy→Decapitator) vs B (inversa), mesmos sinal/presets (Mix 100/FB 0.75/Drive 7.0);
  bypass de cada um; save/reopen em processo fresco; duas instâncias EchoBoy; live reorder/bypass com captura
  de transientes; paralelo impulso+atraso conhecido (CoreTestDelayProbe 480) via bus.
- Limitações: null backend/48 kHz/uma topologia de insert; estados editor-init (não pure-headless, ver VST3);
  paralelo testado via bus M7a (não sidechain M7c, dormente); o clique de edição live é caracterizado, não corrigido.

## Resultados

| ID | Resultado | Evidência | Classificação | Estado |
| --- | --- | --- | --- | --- |
| C-SERIES | **Série provada**: A (EB→DEC) sha 2c62e77c ≠ B (DEC→EB) sha 097c4258 (diff 0.269) → a ordem importa audivelmente. Bypass isola cada plugin: A_bypassEB **bit-idêntico** a dec_only; A_bypassDEC bit-idêntico a eb_only (idem B). Cauda EchoBoy: A lastAbove **6.38 s** (dry acaba 0.145 s), ecos a decair (echoPeaksA) | chain-series-results.json renders/diffs/bitIdentical/echoPeaks | comprovado por teste | verde |
| C-REOPEN | **Save/reopen bit-idêntico**: A, B e dup reabertos em processo fresco → params recalled (Mix 100/FB 0.75/Drive 7.0), render **bitIdenticalToPreSave=true** (sha iguais), missing=[]. Ordem/presets/som preservados | reopenA/B/Dup | comprovado por teste | verde |
| C-DUP | **Instâncias duplicadas independentes**: dois EchoBoy (eb1/eb2) keys distintos, state shas distintos (0075fa5f/e8e5cf09), params independentes (eb1 Mix 20/FB 0.75, eb2 Mix 50/FB 0.38); mover eb1 não toca eb2; reopen bit-idêntico (3986c8d9) | dupIndependence/dupStateSha/reopenDup | comprovado por teste | verde |
| C-PAR | **Paralelo M7a bus alinhado**: impulso dry sample 0; bus gain (P1) impulso no MESMO sample que dry (ratio 1.5012); bus delay480 (P2) **offset 0 amostras** após PDC (tracksSamples 608=128+480, recompensado). Recombinação sample-aligned. P3 EchoBoy echo offset não capturado limpo (FB 0); P4 bypass ≠ dry | parallel-results.json verdicts P1_sameSampleAsDry/P2_offsetSamples=0 | comprovado por teste | verde (bus existe e alinha) |
| C-SIDECHAIN | **Sidechain M7c DORMENTE para plugins**: dec com sidechain:"key" render **bit-idêntico** a sem campo (463dba49); controlos negativos self/bus/missing todos THROW no project.load. O sinal aux NÃO chega ao plugin (Effect::sidechain nunca atribuído em PluginEffect, S9). Implementação = decisão de arquitetura | sidechainDormancy/negativeControls | comprovado por teste | dormente (decisão) |
| **C-EDIT-CLICK** | **Defeito candidato BUG-CHAIN-EDIT-CLICK**: edições live (reorder/bypass) trocam o caminho num limite de sub-bloco **SEM ramp/crossfade** (graph.cpp:641-649 resize, adoção só quando tamanhos batem :868) → transiente de alinhamento (S9 C3) **CONFIRMADO com plugins reais**: tailMaxDiffVsRef durante a transição bypassEB-cont **0.340**, reorder-cont 0.028 (bypassDEC ~6e-8). Mute contrasta: ramps ≤1.21× (S9 C4). **O estado FINAL é correto** (reorder-cont==refBA-cont, bypassEB-cont==refAB-cont bit-idênticos; sem perda/corrupção permanente) — o problema é o CLIQUE na transição, não perda de dados. Sem gesto reorder na app hoje | live-analysis.json transitions/finalRenderBitIdentical; notes | comprovado por teste | defeito aberto (proposta) |
| C-LATENCY | Latência da cadeia: tracksSamples 478 = synth 128 + eb(128+32) + dec(128+62); reorder mantém 478 (soma invariante). PDC compensa por insert | notes latency detail | comprovado por teste | verde |

## Alterações

- **Nenhuma no repositório além de docs** (graph.cpp/project.cpp intactos — prova apenas). Este relatório +
  índice + `evidencia/2026-11-20-chain-series/` (12 ficheiros: results JSONs, live-analysis, saved docs, WAVs
  série/reopen/dup, notes). `ESTADO.md` não editado (D72/D80).
- Fora do repo: `D:\Core-build\chain-series\` (driver, transcripts, WAVs, captures live .raw).

## Verificação

- Driver headless `core-engine serve` (gate-round1d Release), null backend 48 kHz, estados editor-init.
  Série/reopen/dup: sha256 dos renders + params recalled + bitIdenticalToPreSave. Paralelo: impulso
  CoreTestDelayProbe 480 via bus, offset medido (P2=0). Live: captura float32 L/R em torno das transições,
  slew steady ~0.027–0.029, tailMaxDiffVsRef nas transições, finalRenderBitIdentical ao steady reference.
- engines exit 0 em todas as sessões. Sem clique/perda permanente no estado final; o transiente de edição
  é o achado (C-EDIT-CLICK).

## Pendências e passagem

- **BUG-CHAIN-EDIT-CLICK (decisão do dono, não corrigido)**: proposed — ramp/crossfade nas edições de cadeia
  (reorder/bypass) como no mute (S9 C4 ramp ≤1.21×), ou adoção de estado que evite o salto de sub-bloco;
  vermelho medido (tailMaxDiff 0.340 bypassEB). Regressão: o caso live de transição + o final-bit-identical
  (já verde) como guarda de não-perda. Relacionado: S9 C3 (alignment-click) e a adoção graph.cpp:868.
- **D-CHAIN-ARCH (decisão de arquitetura numerada)**: sidechain M7c para plugins NÃO implementado (dormente);
  o paralelo M7a bus existe e alinha. Para sidechain: modelo de routing aux (qual sinal chega à entrada),
  identidade de instâncias, PDC entre ramos, alinhamento (a prova de timing T-PAR/T-PDC satisfaz o gate).
  AGENTS.md: arquitetura nova exige decisão explícita numerada — não implementar sem ela.
- **Chain parâmetros (task-8, Delay Drive → Mix EchoBoy + Drive Decapitator)**: precisa do sistema de
  macros/controlo cross-plugin — verificar se existe; implementação = decisão. A série/paralelo de áudio
  está provada; o controlo de parâmetros é a frente seguinte.
- **Linhas propostas para ESTADO.md (o escrivão escreve, D72/D80):**
  - *Estado medido — Chain áudio (2026-11-20, a9a0384, prova)*: série EB→DEC ≠ DEC→EB (ordem importa),
    bypass isola cada insert (bit-idêntico), cauda EchoBoy 6.38 s; save/reopen bit-idêntico (params recalled);
    duas instâncias do mesmo plugin independentes (estados/params distintos, reopen bit-idêntico); paralelo
    M7a bus sample-aligned (impulso+delay480 offset 0 após PDC); sidechain M7c dormente para plugins
    (bit-idêntico com/sem campo, negativos throw). **Defeito BUG-CHAIN-EDIT-CLICK**: edições live reorder/bypass
    sem ramp/crossfade → transiente (tailMaxDiff 0.340), estado final correto (bit-idêntico ao reference);
    proposto ramp/crossfade, decisão do dono. Latência cadeia 478 samples, reorder invariante.
  - *Pendências*: BUG-CHAIN-EDIT-CLICK (decisão); D-CHAIN-ARCH sidechain (decisão numerada); Chain parâmetros
    (task-8, sistema de macros). Referência: docs/relatorios/2026-11-20-chain-series.md + evidência (12).
- **Retoma**: SHA a9a0384 + round-1; provas D:\Core-build\chain-series; próxima ação = decisões click/sidechain
  + Chain parâmetros (task-8) + S10/S12/S13.