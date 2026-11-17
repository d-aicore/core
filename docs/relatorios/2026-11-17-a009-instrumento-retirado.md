# Relatório — 2026-11-17 — A-009: canção antiga com instrumento retirado

## O que foi medido

- **Repositório, branch e SHA:** `D:\CORE\CORE` (`d-aicore/core`), branch `main`, SHA inicial e final `7ad8cca`
  (raiz da linhagem `b80017e`). Nenhum commit feito nesta entrega (sem autorização para commitar; ver Pendências).
- **Âmbito e autorização:** prompt do dono «CORREÇÃO A-009» (17/11): reproduzir primeiro, corrigir depois;
  «Se não conseguires reproduzir o crash, pára e reporta: não corrigas o que não viste falhar.» Comportamento
  decidido (17/11): converter, não recusar — instrumento fora do registo abre em `none`, conversão registada em
  `conversionNotes` (faixa + nome desconhecido), aviso no retorno de `project.load` (campo `converted`). Âmbito
  fechado: `project.cpp` + teste; `graph.cpp` só se a validação não resolvesse; não tocar `ESTADO.md`/`.qoderignore`.
- **Ambiente:** Windows 10.0.26300, MSVC VS 18 BuildTools, CMake 4.4.4 + Ninja, Release. Build
  `D:\Core-build\gate-CORE` (fora da árvore). JUCE `D:/Core-deps/JUCE-9.0.5`.
- **Testes executados:** novo teste de regressão A-009 (`engine_tests.cpp:482`); controlo de não-vacuidade
  (bloco de conversão removido temporariamente); suite `engine_tests` completa; gate `ctest -L merge` (15 suites).
- **Limitações:** o «estado anterior» sem validação **não existe no histórico desta linhagem** — a raiz `b80017e`
  já contém o bloco. O vermelho foi medido num **controlo temporário** (bloco removido, depois reposto byte-a-byte),
  não no código actual. Build Release no host; não é prova de dispositivo de áudio real nem de sanitizers.

## Resultados

| ID | Problema / resultado | Evidência (SHA, ficheiro:linha ou execução) | Classificação | Estado |
| --- | --- | --- | --- | --- |
| A-009 | **Crash não reproduzido no código actual.** `project.load` com `instrument="elastic"` abre a faixa em `none`, sem crash em `allNotesOff()`/`process()` | `7ad8cca`; teste `engine_tests.cpp:482` verde (1 teste, 5 verificações, 0 falhas, exit 0) | comprovado por teste | não reproduzido |
| A-009 alvo | A premissa «`project.cpp:520` lê sem validar» **não corresponde a esta árvore**: 520 lê, mas 524-529 valida contra o registo e converte para `none` | `project.cpp:520-529` @ `b80017e`/`7ad8cca`; registo `plugin/audio/bus/none` em `instruments.cpp:29` | confirmado no código | já prevenido na raiz |
| Decisão 17/11 | Conversão + aviso **já implementados**: faixa `none` com as notas; `conversionNotes` = `elastic1: Core's own 'elastic' is not played any more: the track waits for a plugin`; `converted` no retorno de `project.load` | `project.cpp:526-527` e `:367`; `server.cpp:465`; teste verifica faixa+nome e `converted` | confirmado no código + comprovado por teste | validado |
| Mecanismo do crash | **Real, mas prevenido:** sem validação, `makeInstrument("elastic")` = `nullptr` (`instruments.cpp:32-36`), guardado em `graph.cpp:346` e desreferenciado em `graph.cpp:870` (`allNotesOff`) | Controlo: 3 `FALHOU` + segfault, exit 139 (ver Verificação) | comprovado por teste (controlo) | prevenido pela validação |
| Regressão | Teste A-009 acrescentado como guarda; **pode falhar** (controlo prova não-vacuidade) | `engine_tests.cpp:482-515`; vermelho/verde em Verificação | comprovado por teste | entregue |
| Gate | `merge` **VERDE 15/15**; 114 casos, 1461 verificações, 0 falhas (engine 21 casos / 86 verificações) | `ci/gate.ps1` exit 0; contagens medidas por suite | comprovado por teste | verde |

## Alterações

- **`tests/engine_tests.cpp` (+37 linhas):** teste de regressão A-009. Único ficheiro de código alterado.
  Carrega um projecto com `instrument="elastic"`, verifica faixa `none` + notas preservadas, `conversionNotes`
  nomeia faixa e nome, `converted` chega ao cliente via `project.load`, e o `Graph` não crasha em
  `allNotesOff()`/`process()`.
- **`docs/relatorios/2026-11-17-a009-instrumento-retirado.md`** (este) + linha no índice `docs/relatorios/README.md`.
- **`project.cpp`: nenhuma alteração final.** O controlo removeu temporariamente o bloco 524-529; foi reposto
  byte-a-byte (`cmp` OK, `git diff -- project.cpp` vazio).
- **`graph.cpp`, `ESTADO.md`, `.qoderignore`: por tocar.** O crash resolve-se na validação (já presente), por isso
  não foi preciso mexer em `graph.cpp` (condição do prompt).
- **Sem correcção de código:** o crash não foi visto falhar no código actual, conforme a instrução do dono.

## Verificação

Bat de build/run (`D:\Core-build\a009-run.bat`, fora da árvore): `vcvars64` (VS 18) +
`cmake --build D:\Core-build\gate-CORE --target engine_tests --parallel 8` + `engine_tests.exe "A-009"`.
Invocado como `cmd //c ...` (o `/c` simples é convertido pelo MSYS e não corre).

**1. Teste no código actual — VERDE (antes de qualquer toque no código):**
```
[1/5] Building CXX object tests\CMakeFiles\engine_tests.dir\engine_tests.cpp.obj
[2/5] Linking CXX executable tests\engine_tests.exe
ok   A-009: canção com instrument='elastic' abre em 'none', conversão reportada, e o grafo não crasha
1 testes, 5 verificacoes, 0 falhas
EXIT=0
```

**2. Controlo de não-vacuidade — VERMELHO + CRASH (bloco 524-529 removido temporariamente):**
```
[1/4] Building CXX object engine\CMakeFiles\core_engine_lib.dir\src\core\project.cpp.obj
[2/4] Linking CXX static library engine\core_engine_lib.lib
[3/4] Linking CXX executable tests\engine_tests.exe
  FALHOU D:\CORE\CORE\tests\engine_tests.cpp:492: p.tracks[0].instrument == std::string("none"): medido elastic, esperado none
  FALHOU D:\CORE\CORE\tests\engine_tests.cpp:497: named
  FALHOU D:\CORE\CORE\tests\engine_tests.cpp:507: clientNamed
/usr/bin/bash: line 1:   882 Segmentation fault      cmd //c "D:\Core-build\a009-run.bat" 2>&1
EXIT=139
```
O processo morre em `Graph::allNotesOff()` (`graph.cpp:870`, `nullptr->allNotesOff()`) antes de imprimir o
resultado do caso — é o crash A-009. As 3 falhas confirmam que o teste detecta a ausência de conversão/aviso.

**3. Reposição + VERDE:** `cmp project.cpp backup` → idêntico; `git diff -- project.cpp` vazio. Recompile:
```
ok   A-009: canção com instrument='elastic' abre em 'none', conversão reportada, e o grafo não crasha
1 testes, 5 verificacoes, 0 falhas
EXIT=0
```

**4. Gate merge (`powershell -ExecutionPolicy Bypass -File ci\gate.ps1`):**
```
100% tests passed out of 15
Gate do Core: VERDE
Codigo: D:\CORE\CORE @ 7ad8cca (com alteracoes por commitar)
Build:  D:\Core-build\gate-CORE
Testes: merge
Tempo:  35 s
GATE_EXIT=0
```
Contagens medidas (11 executáveis de suite; os 4 `cli_*` não imprimem casos):

| Suite | Casos | Verificações | Falhas |
| --- | --- | --- | --- |
| engine | 21 | 86 | 0 |
| domain | 9 | 64 | 0 |
| role | 21 | 216 | 0 |
| midi | 8 | 115 | 0 |
| bug | 10 | 49 | 0 |
| bitwig | 2 | 25 | 0 |
| audit | 7 | 65 | 0 |
| audit2 | 5 | 66 | 0 |
| hunt | 14 | 649 | 0 |
| round3 | 7 | 82 | 0 |
| calls | 10 | 44 | 0 |
| **Total** | **114** | **1461** | **0** |

Delta vs baseline `113/1456` (memória, ronda sanitizers 17/11): **+1 caso / +5 verificações = teste A-009**.
Nenhuma cobertura reduzida nem critério relaxado.

## Pendências e passagem

**Linhas propostas para `ESTADO.md` (NÃO escritas — D72/D80, só o escrivão do ledger):**

- *Estado medido:* «A-009 (17/11, prompt do dono): **não reproduzido** em `main @ 7ad8cca`. A conversão de
  instrumento fora do registo já existe desde a raiz `b80017e`: `project.cpp:524-529` (`elastic`→`none` +
  `loadNotes`→`conversionNotes:367`) e `converted` no retorno de `project.load` (`server.cpp:465`). Regressão
  A-009 (`engine_tests.cpp:482`, 5 verificações) verde; controlo sem o bloco: 3 falhas + segfault em
  `allNotesOff` (exit 139). Sem correcção de código; `graph.cpp` por tocar. Gate merge VERDE 15/15,
  114 casos / 1461 verificações / 0 falhas.»
- *Pendência (owner = Diogo):* confirmar o fecho de A-009 como «já prevenido nesta linhagem». A descrição do alvo
  («520 sem validar») não corresponde a `main`; se A-009 veio de outra linhagem/snapshot, indicar qual para
  revalidar lá. Número `BUG-` (D75) a atribuir pelo escrivão.
- *Proposta separada (sem decisão, não implementada):* null-check defensivo em `graph.cpp:870`/`:867`. Hoje a
  única defesa é a validação em `project.cpp`; todos os `Graph` do produto vêm de `project_` validado
  (`server.cpp:202` é o único sítio de construção; `track.add` passa por `trackFromJson:252`; `track.set` recusa
  mudar `instrument:512`). Risco residual: uma futura via que contorne `trackFromJson` reintroduz o crash.

**Próximos passos:** o escrivão regista as linhas; o dono confirma o fecho ou indica a linhagem do A-009 original.
**Test Engineer:** a regressão é executável (`engine_tests.exe "A-009"`); a aceitação independente pode repetir o
controlo (remover 524-529 → vermelho/crash; repor → verde). **Code reviewer:** o único código alterado é o teste.

**Nota de entrega:** não foi feito commit (sem autorização explícita; o prompt pede «git status limpo» mas não
autoriza commitar, e `ESTADO.md` tem alterações por commitar do escrivão que não me cabe tocar nem commitar).
O estado real do git e o `diff --stat` vão na resposta final.