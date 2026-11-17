# Relatório — 2026-11-17 — ART-003: a saída de áudio morre em silêncio

## O que foi medido

- **Repositório/branch/SHA:** `D:\CORE\CORE` (`d-aicore/core`), `main`, SHA inicial e final `e436ede`.
  Pré-condições verificadas: HEAD `e436ede`; `git status --porcelain` só com os 4 itens de outras entregas
  (` M ESTADO.md`, ` M docs/relatorios/README.md`, `?? …triagem-art.md`, `?? .qoderignore`). **Sem commit/push.**
- **Autorização:** prompt do dono «CORRECÇÃO ART-003» (17/11), `Autorizado-por: D98`, âmbito fechado «só isto»,
  OBJECTIVO decidido (motor sabe e di-lo; app mostra na mensagem existente e reabre uma vez; sem ecrãs novos).
  Nota: D98 é delegação ratificada, mas **P-D98** (alcance vs D48/D50/D76) está aberta — ver Pendências.
- **Ambiente:** Windows 10.0.26300, PC do dono (16 lógicos), MSVC VS 18 BuildTools, CMake 4.4.4 + Ninja, Release.
  Build `D:\Core-build\gate-CORE`. JUCE `D:/Core-deps/JUCE-9.0.5`.
- **Testes:** novo teste ART-003 (`engine_tests.cpp:530`) no código actual (vermelho) e após correcção (verde);
  gate `ctest -L merge` (15 suites, compila também o CoreApp); contagens por suite medidas.
- **Limitações:** o teste do motor usa uma **saída falsa** (não hardware). A reacção da app e o comportamento real
  do WASAPI (desligar USB, mudar dispositivo por omissão) **não foram verificados em execução** — roteiro abaixo.

## Caminho escolhido (5 linhas)

`notificationCallback` do miniaudio → **flag atómica** em `MiniaudioOutput` (0 a tocar / 1 stopped / 2 rerouted) →
novo accessor virtual `AudioOutput::deviceNotification()` (default 0, sem mudar comportamento de saídas que não
notificam) → `Server` expõe-no em `engine.stats` e no `stateEvent` → a app detecta a mudança no `onEvent`/`run` que
já existem, mostra na mensagem de estado existente e reabre **uma vez** via `openDevice`. Escolhi-o porque é o único
que torna a perda visível sem tocar em `graph.cpp`/`engine.cpp`, sem locks/alocações/I/O no thread de áudio (só um
store atómico; o miniaudio proíbe reiniciar o dispositivo no callback) e sem ecrãs/botões novos.

## Resultados

| ID | Problema / resultado | Evidência | Classificação | Estado |
| --- | --- | --- | --- | --- |
| ART-003 | Sem `notificationCallback`, a saída que pára não chega ao motor nem à app | `device.cpp` @ `e436ede` (só `dataCallback`/`pUserData`); vermelho: 4 falhas | confirmado no código + comprovado por teste | corrigido (motor) |
| ART-003 | Motor reporta dispositivo parado/rerouted a quem pergunta | `device.cpp:59,64-71,126,155`; `server.hpp:28`; `server.cpp:424,747`; verde | comprovado por teste | corrigido |
| ART-003 | App mostra na mensagem existente e reabre uma vez; não finge tocar | `EngineSession.cpp:400-409,639-654,425`; `EngineSession.h` (2 membros) | **confirmado no código** (compila; runtime não verificado) | implementado |
| ART-003 | Reroute reamostra e o buffer só é comunicado no arranque | `device.cpp:189` (reset) + reabrir da app faz novo `start` → novo buffer/taxa | confirmado no código | mitigado pela reabertura |
| Gate | `merge` VERDE 15/15; 116 casos / 1469 verificações / 0 falhas | `ci/gate.ps1` exit 0 | comprovado por teste | verde |
| Hardware | Desligar interface USB; mudar dispositivo por omissão | não executado | **não verificado** | roteiro abaixo |

## Alterações

- **`engine/src/io/device.cpp` (+25):** `#include <atomic>`; membro `std::atomic<int> deviceNotification_`; callback
  estático `notification` (stopped→1, rerouted→2, started→0; só store atómico); `config.notificationCallback`;
  accessor `deviceNotification()`; reset da flag no início do `start` e após `ma_device_start` com sucesso.
- **`engine/src/core/server.hpp` (+5):** `virtual int AudioOutput::deviceNotification() const { return 0; }`.
- **`engine/src/core/server.cpp` (+5/-2):** campo `deviceNotification` em `engine.stats` (:424) e `stateEvent` (:747).
- **`app/desktop/src/EngineSession.h` (+2) / `.cpp` (+28):** `lastDeviceNotification_`, `deviceReopenPending_`;
  no `onEvent` (estado) força `playing=false` quando há notificação e, na mudança (edge), põe a mensagem + pede
  reabertura; no `run` reabre uma vez (`openDevice`+`restoreTransport`); predicado `ready` inclui a reabertura.
- **`tests/engine_tests.cpp` (+32):** teste ART-003 com saída falsa que simula stopped/rerouted.
- **NÃO tocados:** `graph.cpp`, `engine.cpp`, `engine.hpp`, `ESTADO.md`, `.qoderignore`, e o resto de `app/`.
  `docs/relatorios/README.md`: só acrescentei a minha linha (a linha da triagem, de outra entrega, fica intacta).

## Verificação

Bat `D:\Core-build\art003-run.bat` (fora da árvore): vcvars64 + `cmake --build … --target engine_tests` +
`engine_tests.exe "ART-003"`; invocado `cmd //c …`.

**VERMELHO no código actual (`e436ede`, antes da correcção):**
```
FALHOU …engine_tests.cpp:551: …value("deviceNotification", 0) == 1: medido 0, esperado 1
FALHOU …engine_tests.cpp:552: server.stateEvent().value("deviceNotification", 0) == 1: medido 0, esperado 1
FALHOU …engine_tests.cpp:555: …value("deviceNotification", 0) == 2: medido 0, esperado 2
FALHOU …engine_tests.cpp:556: server.stateEvent().value("deviceNotification", 0) == 2: medido 0, esperado 2
FAIL ART-003: o Server reporta uma saída que parou ou mudou (dispositivo desligado / rerouted)
1 testes, 5 verificacoes, 4 falhas   EXIT=1
```
(A guarda «a tocar» ==0 passou; o campo não existia.) O override `deviceNotification()` no fake foi acrescentado
com a correcção (a interface é nova); o vermelho foi medido no código exacto sem ele.

**VERDE após a correcção:**
```
ok   ART-003: o Server reporta uma saída que parou ou mudou (dispositivo desligado / rerouted)
1 testes, 5 verificacoes, 0 falhas   EXIT=0
```
(Warning C4996 `u8path` em `server.cpp:706` é pré-existente, fora do âmbito.)

**Gate merge (`ci/gate.ps1`):** `100% tests passed out of 15`, `Gate do Core: VERDE`, `@ e436ede (com alteracoes
por commitar)`, 36 s, `GATE_EXIT=0`. Contagens medidas (11 exes; os 4 `cli_*` não imprimem casos):

| Suite | Casos | Verif. | | Suite | Casos | Verif. |
| --- | --- | --- | --- | --- | --- | --- |
| engine | 23 | 94 | | audit | 7 | 65 |
| domain | 9 | 64 | | audit2 | 5 | 66 |
| role | 21 | 216 | | hunt | 14 | 649 |
| midi | 8 | 115 | | round3 | 7 | 82 |
| bug | 10 | 49 | | calls | 10 | 44 |
| bitwig | 2 | 25 | | **Total** | **116** | **1469** |

**0 falhas.** Delta vs baseline `15/15, 115/1464`: **+1 caso / +5 verificações = teste ART-003**. Restantes suites
inalteradas — **nenhum teste existente falhou**. Nada reduzido nem relaxado.

**Testabilidade da app sem hardware:** o caminho do motor é comprovado por teste (saída falsa). A reacção da app
(`EngineSession::onEvent`/`run`) **não tem harness de unit-test** (acoplada ao IPC `EngineClient`/JUCE) e o
self-test de null device não consegue simular uma notificação de dispositivo parado. Está **confirmada no código e
compila** (o gate liga o `Core.exe`); o runtime fica **não verificado** sem hardware. Não alarguei o âmbito para
criar um harness de app.

## Roteiro de hardware para o dono (2 passos — NÃO testado)

1. **Desligar a interface USB** com uma canção a tocar (backend WASAPI, dispositivo escolhido por nome): a linha de
   estado deve passar a «A saída de áudio parou (dispositivo desligado?). A reabrir...», `playing` pára de fingir, e
   a app tenta reabrir uma vez; se a interface não voltar, fica «No audio device: …» sem fingir que toca.
2. **Mudar o dispositivo por omissão do Windows** (ex.: para 44,1 kHz) com o dispositivo por omissão seleccionado:
   a app deve dizer «A saída mudou de dispositivo…», reabrir uma vez e o som regressar à taxa certa (o novo `start`
   recomunica o buffer/taxa ao motor). Confirmar que não há reamostragem silenciosa nem posição congelada sem aviso.

## Pendências e passagem

**Linhas PROPOSTAS para `ESTADO.md` (NÃO escritas — D72/D80, só o escrivão):**
- *Estado medido:* «ART-003 (17/11, prompt do dono, `Autorizado-por: D98`): a saída que pára/muda já é reportada.
  Motor: `notificationCallback`→flag atómica→`AudioOutput::deviceNotification()`→`engine.stats`/`stateEvent`
  (`device.cpp`, `server.hpp:28`, `server.cpp:424,747`). App: mensagem de estado existente + reabrir uma vez
  (`EngineSession.cpp:400,639`). Regressão ART-003 (`engine_tests.cpp:530`, 5 verif.): vermelho 4 falhas @ `e436ede`,
  verde depois. Gate VERDE 15/15, **116/1469/0**. `graph.cpp`/`engine.cpp` por tocar. Runtime WASAPI e reacção da
  app: **não verificados** (roteiro de 2 passos no relatório).»
- *Pendência (owner = Diogo / escrivão):* **(1)** validar o roteiro de hardware acima. **(2)** **P-D98** continua
  aberta: a entrada desta correcção no ledger (caminho de áudio) depende do alcance da delegação D98 vs D48/D50/D76;
  o prompt actual autorizou «só isto», mas a classificação da decisão é do escrivão conforme P-D98. **(3)** FL-2
  (app nunca envia `plugin.retry`) e ART-002 (política do bus) continuam pendentes, fora deste âmbito.

**Próximos passos:** committer integra motor+app+teste+relatório/índice; escrivão regista as linhas e resolve P-D98;
dono corre o roteiro de hardware. **Test Engineer:** `engine_tests.exe "ART-003"`; aceitação independente pode
estender o fake para verificar que um `start` bem-sucedido limpa a notificação (já coberto: o fake repõe 0 no start).
**Code reviewer:** o thread de áudio só ganha um store atómico no callback de notificação (não no data callback).
**Risco residual:** baixo no motor (aditivo, default 0 para saídas que não notificam); a reabertura automática da app
é o comportamento novo a validar com hardware.