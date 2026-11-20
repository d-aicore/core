# Relatório — 2026-11-20 — S4: WAV float inválido rejeitado na importação, com o ficheiro identificado

Delivery: DSH lead session `a69b320fbfab`, agents `s4-red` (vermelho, probe sobre a lib real) and
`s4-fix` (correção + regressão permanente Debug+Release), coordinator (revisão do diff),
2026-11-20 Europe/Lisbon. Owner authorization: S-round dispatch 2026-11-20, item S4 («Cobre NaN,
infinito positivo e negativo, e um WAV válido… valor finito em double que transborde… Corre a
regressão também em Release no Windows, porque o motor é compilado com /fp:fast… não deixa um
asset parcialmente registado»). Plano original Q-consulta pendente (1c6238b não localizável);
o contrato explícito do dispatch governa.

## O que foi medido

- Repositório `d-aicore/core`, `main @ a9a038430d781959829f8510067ce59afffaad62` (= origin/main);
  alterações locais build-affecting preservadas (CMake ENV-01+D113; S5 Setup.cpp; registos de testes).
- Ambiente: Windows 11 x64, MSVC 19.51.36260, CMake 4.4.4, Ninja 1.13.2; vcvars64 + VSLANG=1033 +
  chcp 65001. Builds próprios: `D:\Core-build\s4-fix\build` (Debug) e `…\release` (Release),
  CORE_BUILD_TESTS=ON, CORE_BUILD_APP=OFF (engine+tests).
- **Facto corrigido ao dispatch**: `engine/CMakeLists.txt:9` `add_compile_options(/W3 /utf-8 /fp:fast)`
  é INCONDICIONAL para MSVC — aplica-se a Debug E Release (o dispatch dizia «Release»; medido e
  registado pelo s4-red). A validação tem de sobreviver a /fp:fast nas duas configurações.
- Testes executados: regressão permanente `wav_float_tests` (16 casos / 360 verificações) em
  Debug e Release, vermelho→verde; probe do s4-red religado à lib corrigida (segunda confirmação);
  ctest -L merge completo no Debug próprio (20/20).
- Limitações: âmbito APP=OFF (sem desktop/installer/crash-plugins — gate Windows completo fica
  para a integração); subnormais float64→float32 sob /fp:fast não medidos (fora do contrato);
  a regra de overflow é a conservadora |d|>FLT_MAX pedida pelo dono (RN arredondaria alguns
  valores logo acima para FLT_MAX — fixado por teste de fronteira).

## Resultados

| ID | Problema / resultado | Evidência | Classificação | Estado |
| --- | --- | --- | --- | --- |
| S4-RED | float32 NaN/±Inf aceites raw (memcpy :139), float64 aceite (:87) com 1e300 finito → ±Inf via cast (:140); SEM mensagem; ficheiros inválidos TOTALMENTE registados+cached no AssetStore (512 B, segundo get = mesmo ponteiro) | `s4-red-notes.md`, fixtures+probe `D:\Core-build\s4-red\` | comprovado por teste | vermelho medido (antes) |
| S4-FIX | Validação por BIT-PATTERNS INTEIROS nos bytes raw (nunca isfinite/isnan/comparações float — /fp:fast pode eliminá-las): f32 expoente-todos-uns → NaN/Inf (zero/subnormais passam); f64 NaN/Inf + magnitude > padrão double de FLT_MAX (0x47EFFFFFE0000000, ordem monotónica dos doubles positivos como inteiros) ANTES do cast. Rejeição no estilo do header: `<path>: invalid float sample at frame N (left/right): NaN.|Infinity.|overflow…` | `engine/src/io/audio_file.cpp` +39/−2 (u64 :16, float32Defect :23, float64Defect :27, rejectSample :153, checks :172/:175); diff revisto pelo coordenador | confirmado no código + comprovado por teste | corrigido (por commitar) |
| S4-ASSETS | AssetStore NADA regista na rejeição: assets.cpp :49 readWav dentro do try, :55 inserção única só após sucesso, :57-60 catch nunca insere — INALTERADO, provado por teste+probe (bytes()==0, get null) | `s4-fix-notes.md`; probe `probe-fixed-output.txt` | comprovado por teste | validado |
| S4-REG | Regressão permanente `tests/wav_float_tests.cpp` (16 casos: NaN/±Inf f32, mixed, f64 NaN/overflow, par de fronteira FLT_MAX, canal direito stereo, sNaN, válidos f32/f64 bit-idênticos, badbits header, PCM16; fixtures gerados no teste). **Vermelho (Debug não-corrigido): 16 testes / 360 verif / 22 falhas**, exit 1 (12 FAIL recusa+AssetStore, 4 ok controlos). **Verde: 16/360/0 em Debug E Release**, exit 0 | `red-wav-float-debug.log`, `green-wav-float-debug.log`, `green-wav-float-release.log` | comprovado por teste | verde |
| S4-PROBE | Probe do vermelho (inalterado) religado à lib Debug corrigida: 7 inválidos REJEITADOS com path+frame+kind; AssetStore recusado (null/bytes_delta=0); 2 válidos ACEITES bit-idênticos (L[1]=3cebc842, igual ao vermelho), 512 B cached | `probe-fixed-output.txt`, `probe-relink.log` | comprovado por teste | verde (segunda confirmação) |
| S4-MERGE | ctest -L merge no Debug próprio: **20/20, 17.84 s**; por suite: engine 39/221, wav_float 16/360 (novo), domain 33/568, modules 6/128, role 21/215, midi 20/402, bug 48/462, bitwig 2/25, audit 7/65, audit2 15/364, hunt 11/106, round3 7/82, calls 12/69, menu 7/301, window 10/70, engine_process 3/98 + 4 cli; soma 257 casos / 3538 verif / 0 falhas. **engine_process_tests PASSOU nesta corrida** — ENV-03 (259) não ocorreu; dado para a tarefa 3A (sensível a timing, não determinístico entre builds/corridas) | `merge-debug.log` | comprovado por teste | verde (âmbito APP=OFF) |
| S4-COUNTS | Registo: bloco próprio tests/CMakeLists.txt:17-27 (LABELS merge); lista partilhada estendida com export_plugin_failure_tests (S1, concorrente) — os 18 nomes originais preservados (verificado pelo coordenador); installer_samples (S5) intacto | git diff tests/CMakeLists.txt (+33/−1, a deleção = linha da lista partilhada substituída pela extensão) | comprovado por execução | validado |

## Alterações

- `engine/src/io/audio_file.cpp` — +39/−2 (validação inteira + rejeição nomeada; caminho dos
  válidos inalterado, bit-idêntico).
- `tests/wav_float_tests.cpp` — novo (16 casos / 360 verificações).
- `tests/CMakeLists.txt` — bloco de registo próprio (adição).
- `engine/src/core/assets.cpp` — INALTERADO (a ordem de inserção já garantia o contrato).
- Este relatório + índice + `evidencia/2026-11-20-s4-float-wav-rejection/` (7 ficheiros).
- `ESTADO.md` não editado (D72/D80) — linhas propostas abaixo.
- Fora do repo: `D:\Core-build\s4-red\` (fixtures+probe vermelho), `D:\Core-build\s4-fix\`
  (builds Debug/Release, logs red/green/merge/probe), notas `coordinator-round2\s4-*.md`.

## Verificação

- Vermelho: build Debug próprio NÃO-corrigido → `wav_float_tests` 16/360/**22 falhas** exit 1
  («expected readWav to refuse …f32_nan.wav (no exception thrown)»; «store.bytes()==0: medido 512»).
- Verde: mesma suite após a correção → **16/360/0 exit 0 em Debug** (incremental 15/15) **e em
  Release** (91/91 alvos) — o requisito /fp:fast do dono cumprido nas duas configurações.
- Segunda confirmação: probe do vermelho religado à lib corrigida → tabela completa acima.
- Merge completo (Debug, APP=OFF): 20/20, 257 casos / 3538 verificações / 0 falhas.
- Diff revisto pelo coordenador: checks inteiros corretos (expoente/mantissa, magnitude monotónica
  vs FLT_MAX), rejeição antes de qualquer retorno, válidos bit-idênticos, registo de testes aditivo.

## Pendências e passagem

- **Commit**: correção S4 + regressão aguardam decisão do dono (nada commitado).
- **Gate integrado**: o gate merge+desktop completo (APP=ON, com installer_samples e
  export_plugin_failure_tests) corre na revisão integrada — este relatório declara apenas o
  âmbito APP=OFF medido.
- **3A (ENV-03)**: engine_process_tests passou nesta corrida Debug (3/98/0) mas falhou 3/3 (259)
  nas corridas do ambiente core-review @ a9a0384 — sensível a timing; a investigação 3A mantém-se.
- **Linhas propostas para ESTADO.md (o escrivão escreve, D72/D80):**
  - *Estado medido — S4 WAV float (2026-11-20, a9a0384 + correção local por commitar)*: vermelho
    medido (NaN/±Inf f32 raw, overflow f64→float, sem mensagem, asset totalmente registado);
    correção por bit-patterns inteiros à prova de /fp:fast (incondicional em engine/CMakeLists.txt:9,
    Debug E Release — facto corrigido ao dispatch); rejeição nomeia ficheiro/frame/canal/kind;
    AssetStore nada regista (inalterado, provado); regressão wav_float_tests 16/360: vermelho 22
    falhas → verde 0 em Debug E Release + probe cruzado; merge APP=OFF 20/20 (257/3538/0).
    engine_process_tests passou nesta corrida (ENV-03 não ocorreu — 3A continua).
  - *Pendência*: commit S4 (decisão do dono); subnormais f64→f32 fora do contrato; gate integrado pendente.
  - *Referência*: docs/relatorios/2026-11-20-s4-float-wav-rejection.md + evidência (7 ficheiros).
- **Retoma**: SHA a9a0384 + correções locais; builds D:\Core-build\s4-fix\{build,release};
  próxima ação = fechar S1 e correr o gate integrado.
