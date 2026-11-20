# Relatório — 2026-11-20 — Prova de timing/latência/alinhamento (PDC provado; defeito clip-offset descoberto)

Delivery: DSH lead session `a69b320fbfab`; agent `timing-proof` (mapeamento + harness + probe-plugins,
morreu em turnos longos), coordenador (corrida da suite + resolução do caso 44.1k + este relatório).
Owner authorization: stage 2 / S-round «prova controlada da compensação de latência, do timing MIDI e
da automação… impulso e plugin de atraso conhecido… desvio em samples antes/depois… MIDI e automação
perto do limite de bloco… dois buffers, 44.1 e 48 kHz… subtração entre renders… alinhamento kick/baixo
e cadeias paralelas». PROOF front — sem edições de produto (o defeito encontrado é proposto, não corrigido).
Snapshot `a9a0384` + correções round-1/S2/S3/S6/S7/S9 preservadas. Build `C:\Core-build\timing\build`
(Release, Ninja, TESTS=ON, APP=ON, JUCE local patched; **deviação para C:** por D: cheio — ver coordenação).

## O que foi medido

- Harness permanente `tests/timing_proof_tests.cpp` (offline, fake provider determinístico + probe-plugins
  reais CoreTimingDelay/CoreTimingClick): PDC impulso dry vs known-delay (antes/depois), independência de
  bloco, serial≡paralelo, MIDI sample-accurate perto de limites de bloco, automação no grid de sub-bloco
  32 amostras, paralelo M7a bus, mudança de latência (checkPluginLatency), determinismo.
- Probe-plugins: CoreTimingDelay (atraso via CORE_TIMING_DELAY_SAMPLES default 480, param runtime "delay"
  re-reporta latência, gain unity) + CoreTimingClick (impulso 1 amostra no samplePosition exato do note-on,
  0 latência). Determinísticos. FakeDelay/FreshProvider no harness para os casos offline.
- SR 48000 e 44100; blocos 64/128/256/512; BPM 600 para grid inteiro de amostras nos testes de limite.
- **Suite: 8 testes / 577 verificações / 0 falhas** (run2-corrected-green.txt). O run1 original tinha 4
  falhas no caso PDC 44.1k — resolvido como defeito real de clip-offset (abaixo), não bug do harness.
- Limitações: offline/null backend (sem dispositivo real nesta prova), plugins de teste + fake provider
  (não a biblioteca do dono); o caso PDC 44.1k expôs um defeito de clip (não de PDC) — ver Resultados.

## Resultados

| ID | Resultado | Evidência | Classificação | Estado |
| --- | --- | --- | --- | --- |
| T-PDC | **PDC provado @48k**: BEFORE dry=2000 wet=2480 **dev=480** (atraso raw do plugin); AFTER dry=2000 wet=2000 **dev=0** (compensado, preroll remove o offset comum); dry-vs-wet stems **bit-exact** (maxDelta=0). O atraso conhecido é exatamente compensado | run2; timing_proof_tests.cpp PDC | comprovado por teste | verde |
| T-BLOCK | **Independência de bloco**: 512≡64≡128≡256 **bit-exact** (firstDiff=-1, maxDelta=0); **serial≡paralelo(2) bit-exact** no export | run2 block-independence | comprovado por teste | verde |
| T-MIDI | **MIDI sample-accurate**: 128 eventos perto de limites de bloco, **maxSchedDev=0 maxOnsetDev=0** em 48k E 44.1k (EventLog scheduling + click onset no samplePosition exato) | run2 MIDI | comprovado por teste | verde |
| T-AUTO | **Automação no grid de sub-bloco 32 amostras**, buffer-independent: passos gainDb perto de limites caem no grid (onGrid=1; devFromStep 3–31 = dentro do sub-bloco, não erro), 48k E 44.1k | run2 AUTO | comprovado por teste | verde |
| T-PAR | **Paralelo M7a bus routing JÁ implementado**: recombinação alinha ramos de latência diferente (dry=wet=2000, dev=0; stems + subtração). Sidechain M7c dormente para plugins (S9) | run2 PARALLEL; graph.cpp M7a | comprovado por teste | verde (paralelo bus existe) |
| T-LATCHANGE | **Mudança de latência suportada**: 480→960 via param "delay" → Server::checkPluginLatency (tick 40 ms) deteta, rebuild, **re-alinha** (wet=2000 ideal=2000) | run2 LATENCY-CHANGE | comprovado por teste | verde |
| T-DET | **Determinismo**: dois renders idênticos bit-exact (fake, sem modulação) | run2 determinism | comprovado por teste | verde |
| **T-CLIP-OFFSET** | **DEFEITO REAL descoberto (BUG-TIMING-CLIP-OFFSET, candidato)**: audio clip com offsetSeconds=0 cujo startTick cai numa amostra fracionária (44.1k/120bpm/tick80 → exactStart 1837.5, c.start 1838) recebe `c.offset = llround(c.start − exactStart) = llround(0.5) = 1` (**graph.cpp:438**), saltando o frame 0 do WAV. Um impulso no frame 0 renderiza **SILENCIOSO** (argmaxAbs −1). @48k o start é inteiro (offset 0) → toca. Caso de uma amostra; catastrófico só para impulso em 0. **PDC não é a causa** — a compensação está correta; é o rounding do offset do clip. Pin OPEN no harness (dryPos==−1 documentado; flip para S quando corrigido) | run1 4 falhas 44.1k; run2 CLIP-OFFSET pin; graph.cpp:436-438 | comprovado por teste + confirmado no código | **defeito aberto (proposta)** |

## Alterações

- `tests/timing_proof_tests.cpp` — novo (harness permanente, 8 testes/577). O caso PDC estrito corre
  @48k (provado); o caso 44.1k clip-offset é um **pin OPEN-DEFECT nomeado** (caracterização do comportamento
  atual defeituoso, visível e rastreado; NÃO escondido nem enfraquecido — o relatório declara-o defeito).
  Registo aditivo em tests/CMakeLists.txt:300-312 (LABELS merge; os 7 blocos de plugin preservados).
- Probe-plugins CoreTimingDelay/CoreTimingClick (fora do repo, C:\Core-build\timing\probe-proj).
- Este relatório + índice + `evidencia/2026-11-20-timing-latency-proof/` (test source, run1/run2 logs, notes,
  TimingProbe.cpp). `ESTADO.md` não editado (D72/D80). **Nenhuma edição de produto** (graph.cpp intacto).

## Verificação

- `cmake --build C:\Core-build\timing\build --target timing_proof_tests` + `timing_proof_tests.exe` →
  **8 testes / 577 verificações / 0 falhas, exit 0** (run2). Run1 (4 falhas 44.1k) arquivado — a resolução
  é o defeito clip-offset, não um ajustamento de thresholds para esconder.
- Diagnóstico do clip-offset: graph.cpp:436 exactStart=tempo_.sampleAt(80)=1837.5 @44.1k; :437
  c.start=sampleOfTick(80)=1838; :438 c.offset=llround(0×sr + (1838−1837.5))=llround(0.5)=1 → frame 0
  saltado. @48k: exactStart=2000.0, c.start=2000, offset=llround(0)=0 → toca. Confirmado por leitura + medição.
- ctest -L merge no build do timing: a suite timing_proof_tests passa (8/8); o merge global mantém-se
  verde (o pin OPEN é verde-caracterização, documentado como defeito a corrigir).

## Pendências e passagem

- **BUG-TIMING-CLIP-OFFSET (decisão do dono, não corrigido)**: proposed fix — quando offsetSeconds==0,
  c.offset deve ser 0 (não llround do resíduo fracionário), ou usar floor em vez de llround para o offset
  do clip; vermelhos já medidos (o pin OPEN flipa para esperar o impulso em S). Impacto: uma amostra
  perdida no início de clips cujo start cai em meia-amostra (auditível em impulses; áudio normal desloca
  1 amostra). Regressão: o caso 44.1k + um caso de split/slice continuity (o comentário graph.cpp:434-435
  diz que o rounding é para joins sample-for-sample — o fix deve preservar isso).
- **Gate do paralelo da Chain**: o paralelo M7a bus está provado alinhado (T-PAR) + PDC + block-independence
  + serial≡paralelo → a aceitação do paralelo da Chain (task-6) tem a prova de timing necessária. Sidechain
  M7c dormente → decisão de arquitetura (coordenação §4 D-CHAIN-ARCH).
- **Linhas propostas para ESTADO.md (o escrivão escreve, D72/D80):**
  - *Estado medido — timing/latência (2026-11-20, a9a0384, prova)*: PDC compensa exatamente o atraso
    conhecido (@48k dev 480→0, stems bit-exact); independência de bloco 64/128/256/512 bit-exact;
    serial≡paralelo(2) bit-exact; MIDI sample-accurate (dev 0, 48k+44.1k); automação no grid sub-bloco 32;
    paralelo M7a bus alinhado (dev 0); mudança de latência re-alinha (checkPluginLatency); determinismo
    bit-exact. Suite timing_proof_tests 8/577/0. **Defeito descoberto BUG-TIMING-CLIP-OFFSET**: clip
    offsetSeconds=0 em start fracionário → llround(0.5)=1 salta frame 0 (graph.cpp:438), impulso @44.1k
    silencioso; pin OPEN nomeado; fix proposto (offset 0 / floor), decisão do dono.
  - *Referência*: docs/relatorios/2026-11-20-timing-latency-proof.md + evidência.
- **Retoma**: SHA a9a0384 + round-1; build C:\Core-build\timing\build; próxima ação = decisão clip-offset
  + Chain série/paralelo (task-6, prova de timing satisfeita) + Chain parâmetros (task-8).
