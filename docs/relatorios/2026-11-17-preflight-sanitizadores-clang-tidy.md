# Relatório — 2026-11-17 — pré-flight do plano tidy + ASan/UBSan/TSan

## O que foi medido

- Repositório, branch e SHA inicial/final: `D:/CORE/CORE`, `main`, `b80017efdbb6b47bc65a82338c403548e095967f` (inicial = final; nenhum ficheiro do plano foi alterado).
- Âmbito e autorização: verificar se a sequência de 3 passos proposta pelo dono (clang-tidy estático, ASan+UBSan, TSan) corre nesta máquina e se o seu verde significaria alguma coisa. Leitura + sondas de configure/compilação de ficheiros mínimos. **Nenhum passo do plano foi executado contra o produto; nenhuma implementação feita.**
- Ambiente: Windows 10.0.26300 x64. Host: MSVC 19.51.36260, toolset 14.51.36231 (VS 18 BuildTools), CMake 4.4.4, Ninja. `clang`, `clang-cl`, `clang-tidy`, `gcc`: **ausentes no host**. WSL Ubuntu-24.04: clang 18.1.3, clang-tidy 18.1.3 (LLVM 18.1.3), g++ 13.3.0 (`libasan` presente), cmake 3.28.3, ninja 1.11.1, 16 threads, 15 GiB RAM, repo visível em `/mnt/d/CORE/CORE`. Docker: ausente. JUCE local: `D:/Core-deps/JUCE-9.0.5` existe (não é preciso download).
- Testes executados (ou “nenhum”): **nenhum** teste do produto. As compilações abaixo são de ficheiros de sondagem de 1 linha, propositadamente fora da árvore.
- Limitações: o Core não foi aberto; as suites `merge` não foram corridas; nada foi medido sobre o código real em Linux — apenas as toolchains. As sondas de flags não instalam nem escrevem na árvore do código (scratch em `D:/Core-build`, removido no fim).

## Resultados

| ID | Problema / resultado | Evidência (SHA, ficheiro:linha ou execução) | Classificação | Estado |
| --- | --- | --- | --- | --- |
| PF-01 | `CORE_SANITIZE` **não existe** no build: os passos 2 e 3 produzem um Debug normal e o `ctest -L merge` verde não diria nada sobre memória nem races. | grep sem ocorrências em `CMakeLists.txt`, `cmake/`, `ci/`, `tests/CMakeLists.txt`; configure de sondagem ficou com `CORE_SANITIZE:UNINITIALIZED=address,undefined` em `CMakeCache.txt`; `compile_commands.json` (65 TUs) tem **0** ocorrências de `fsanitize`. | código + teste (configure) | aberto — decisão |
| PF-02 | O `cl.exe` deste toolset **ignora em silêncio** `/fsanitize=undefined` e `/fsanitize=thread`: `warning D9002 : ignorando opção desconhecida`, com **exit code 0**. Mesmo com o wiring feito, UBSan e TSan no host seriam falsos verdes que o `ctest` reporta como passados. | compilação de `int main(){return 0;}` com cada flag, `cmd` dentro de `vcvars64.bat`: exit 0 + D9002 nos dois casos. | teste | aberto |
| PF-03 | ASan do MSVC **funciona** no host: `/fsanitize=address` compila e liga (exit 0, `.exe` produzido); runtime presente no toolset. Advertências por resolver no wiring: com `/MTd` estático e sem `/DEBUG` aparecem `warning C5072` e `LNK4302` (relatórios ASan sem símbolos). | `clang_rt.asan_dynamic-x86_64.dll` e variante `_dbg` em `VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64`; `CMakeLists.txt:24` e `engine/CMakeLists.txt:9` fixam `/MT`/`/MTd`. | teste | aberto — decisão |
| PF-04 | O passo 1 falha no primeiro comando: `clang-tidy` não está instalado no host (nem via LLVM nem no BuildTools). Está no WSL (LLVM 18.1.3). | `command -v clang-tidy` → ausente no host; `clang-tidy --version` → 18.1.3 no WSL. | teste | aberto — decisão |
| PF-05 | O passo 1 nomeia `engine/src/plugins/juce_host.cpp` mas passa `-DCORE_BUILD_APP=OFF`: `engine/CMakeLists.txt:45` só cria `core_plugin_host` quando existe o alvo `juce::juce_audio_processors`. Resultado medido: **0 entradas** para `juce_host` no banco de compilação — o ficheiro fica por analisar, sem erro visível. | `engine/CMakeLists.txt:45` (`if(TARGET juce::juce_audio_processors)`); `grep -c juce_host compile_commands.json` → `0`. | código + teste | aberto |
| PF-06 | Cobertura real do glob do passo 1: **49 de 65** TUs. Ficam de fora `engine/src/app` ×3 (`main.cpp`, `measure.cpp`, `device.cpp`), `app/sampling/src` ×1, `app/previews/src` ×1 e `tests` ×11. `device.cpp` é o callback do dispositivo — o sítio mais provável para os races que o passo 3 procura. | script sobre `compile_commands.json` do configure de sondagem (contagem por directório). | teste | aberto |
| PF-07 | `halt_on_error=0` com `ctest`: um relatório do sanitizer não faz necessariamente o caso falhar, por isso o resumo do `ctest` não é prova de sanidade. É preciso `halt_on_error=1` e gredar os logs por `ERROR: AddressSanitizer` / `runtime error:` / `WARNING: ThreadSanitizer`. | proposta do dono (passos 2 e 3) revista contra o comportamento documentado dos runtimes; não executado contra o produto. | não verificado | aberto |
| PF-08 | Higiene de execução nesta máquina: `b-tidy`/`b-asan`/`b-tsan` **não** são ignorados (`.gitignore` só tem `build/`, `build-*/`, `Testing/`), e o Git Bash não vê o MSVC — `cmake -G Ninja` sem `vcvars64` falha `The CXX compiler identification is unknown`. A convenção local já é compilar fora da árvore (`ci/gate.ps1` usa `D:\Core-build\...`). | `.gitignore:1-13`; configure tentado a partir do Git Bash (falha) e o mesmo configure a correr depois de `vcvars64.bat` (sucesso). | teste | aberto |
| PF-09 | O rótulo `-L merge` existe e cobre 15 testes (`engine/domain/role/midi/bug/audit/audit2/hunt/round3/calls/bitwig` + `cli_version`, `cli_demo_render`, `cli_measure`, `cli_unicode`); `desktop_self_test` está noutro rótulo (`desktop`) e só nasce com o app ligado. | `tests/CMakeLists.txt:90-97`. | código | validado |
| PF-10 | TSan não tem caminho no Windows com este toolchain (medido em PF-02). O suporte ausente também do lado do clang para Windows é conhecimento documentado do LLVM, **não verificado nesta máquina** — por isso a recomendação passa por Linux (WSL), onde clang 18 e `libasan` já existem. | PF-02 (medido); limitação LLVM/reportada. | reportado | aberto — decisão |

## Alterações

Documentação apenas: este relatório, entrada no índice, e uma linha em `ESTADO.md`. Nenhum ficheiro de código, CMake ou teste foi tocado. Trabalho alheio não rastreado preservado e intacto (`app/desktop/src/DspPerformance.{cpp,h}`, `docs/DECISOES.md`, quatro relatórios de 17-11, `.qoderignore`).

## Verificação

1. `cmake -S . -B D:/Core-build/cfg-probe -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCORE_BUILD_APP=OFF -DCORE_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCORE_SANITIZE=address,undefined`
   - a partir do Git Bash: falha — `The CXX compiler identification is unknown`, `No CMAKE_CXX_COMPILER could be found` (exit ≠ 0).
   - depois de `call vcvars64.bat`: `Configuring done`, `Generating done`; compilador `cl.exe` MSVC 19.51.36260. **Nenhum** aviso sobre a variável `CORE_SANITIZE`.
2. No banco gerado: 65 TUs; `juce_host` → 0; `fsanitize` → 0; `CMakeCache.txt` → `CORE_SANITIZE:UNINITIALIZED=address,undefined`. Linha de comando tipo: `cl.exe /nologo /TP -I...engine\src -std:c++20 -MTd /utf-8 /W3 /fp:fast ...`.
3. Ficheiro de sondagem de 1 linha, dentro de `vcvars64`: `/fsanitize=address` → exit 0, executável produzido; `/fsanitize=undefined` → exit 0 com `D9002`; `/fsanitize=thread` → exit 0 com `D9002`; `where clang-cl` → ausente.
4. Ferramentas no host: `cmake` 4.4.4 e `ctest` presentes, `ninja` presente; `clang-tidy`, `clang`, `gcc` ausentes. No WSL: clang/clang-tidy 18.1.3, g++ 13.3.0 com `libasan`, cmake 3.28.3, ninja 1.11.1; `nproc` = 16; repo em `/mnt/d/CORE/CORE`.
5. Nada do acima executa os binários do produto; os executáveis de sondagem compilados nunca foram corridos.

## Pendências e passagem

Decisões necessárias (uma ronda, depois executo):

1. **Ligar ou não `CORE_SANITIZE` no CMake** (é implementação no build system; não fiz). Desenho proposto: `option` declarado na raiz para o nome deixar de ser silencioso; `address` → `/fsanitize=address` no MSVC e `-fsanitize=address` em Clang/GCC, com `/DEBUG` para relatórios legíveis; `undefined` e `thread` → **aplicar só quando o compilador for Clang/GCC**, com `FATAL_ERROR` caso contrário em vez do `D9002` ignorado. Sem isto, os passos 2 e 3 não devem ser corridos nem reportados como evidência.
2. **Onde corre a análise**: recomendação é **WSL Ubuntu-24.04** para os três passos (já tem clang-tidy 18, ASan/UBSan/TSan; zero instalações), deixando no host apenas o ASan MSVC como prova de Windows. Alternativa: instalar LLVM no host (`winget LLVM.LLVM`) para ter clang-tidy fora do WSL — implica instalar software de terceiros nesta máquina, por isso precisa do teu sim.

Sequência corrigida, pronta a executar quando os pontos 1 e 2 estiverem decididos (compila fora da árvore; nada de sanitizers sem o wiring do ponto 1):

```sh
# WSL, ext4 nativa (clone próprio, não /mnt/d: builds em drvfs são lentos)
cmake -S . -B ~/core-b/tidy -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCORE_BUILD_APP=OFF -DCORE_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build ~/core-b/tidy -j
clang-tidy -p ~/core-b/tidy \
  --checks='-*,bugprone-*,concurrency-*,performance-*,clang-analyzer-*' \
  --header-filter='(engine|app)/(src|include)/' \
  engine/src/core/*.cpp engine/src/dsp/*.cpp engine/src/io/*.cpp engine/src/app/*.cpp \
  app/domain/src/*.cpp app/control/src/*.cpp app/engine-client/src/*.cpp \
  app/sampling/src/*.cpp app/previews/src/*.cpp > tidy-round1.txt
```

`engine/src/plugins/juce_host.cpp` só aparece no banco com `-DCORE_BUILD_APP=ON` (e `FETCHCONTENT_SOURCE_DIR_JUCE` apontado para `D:/Core-deps/JUCE-9.0.5`, ou a cópia nativa equivalente no WSL), pelo que ou se faz uma segunda configuração com o app ligado só para este ficheiro, ou se retira do comando. Risco residual: nada do que corre em Linux prova o runtime Windows (`AGENTS.md`), e o `concurrency-*`/TSan sobre `device.cpp` continua sem prova Windows até o ponto 1 estar ligado e o ASan MSVC corrido.

---

## Adendum — 17-11-2026 (noite) — plano reescrito para WSL + regras novas do ledger

Âmbito: o dono ementou o `AGENTS.md` entretanto (`ESTADO.md` escrito **só** pelo escrivão — D72/D80) e substituiu o plano por uma versão WSL com gate de scope, canários e `halt_on_error=1`. Nova ronda de sondas: só configure fora da árvore (`~/core-build`, removido no fim), nada do produto executado, nada instalado, nenhum ficheiro do repo alterado por mim.

| ID | Problema / resultado | Evidência (SHA, ficheiro:linha ou execução) | Classificação | Estado |
| --- | --- | --- | --- | --- |
| PF-11 | **A cobertura do `juce_host.cpp` não se ganha em Linux.** `-DCORE_BUILD_APP=ON` não traz nem o JUCE nem o desktop: os dois blocos estão gateados por `AND WIN32` (`CMakeLists.txt:29` e `CMakeLists.txt:45`) e `engine/CMakeLists.txt:45` só cria `core_plugin_host` se existir o alvo `juce::juce_audio_processors`. O próprio CMo faz o favor de o dizer. | configure em WSL Ubuntu-24.04 com `-DCORE_BUILD_APP=ON -DCORE_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JUCE=/mnt/d/Core-deps/JUCE-9.0.5`: `Manually-specified variables were not used by the project: FETCHCONTENT_SOURCE_DIR_JUCE`; `juce_host` → **0** entradas em 64 TUs; `$B/tidy-probe/app` contém apenas `control domain engine-client previews sampling`. | teste | aberto — decisão |
| PF-12 | O compilador que o CMake escolhe no WSL é **GCC 13** (`/usr/bin/c++`), não clang. Não é obstáculo a ASan/UBSan/TSan em Linux nem à leitura do banco pelo clang-tidy; se quisermos o TSan mais capaz, é `-DCMAKE_CXX_COMPILER=clang++-18` na configuração, e nada no repo o impede. | mesma sonda: `CMAKE_CXX_COMPILER:FILEPATH=/usr/bin/c++`; no WSL existem clang++-18 18.1.3, clang-tidy 18.1.3 e `libasan` do GCC 13. | código + teste | aberto |
| PF-13 | `asan_canary` e `tsan_canary` **não existem** no repo: 0 ocorrências de `canary` em CMakeLists, fontes, `cmake/`, `ci/` e `tests/`. Com o `&&` do plano, os passos 2 e 3 interrompem-se antes do `ctest` — falham, não fingem. O desenho do canário (provar que o binário está mesmo instrumentado antes de confiar no verde) é a peça que faltava ao plano antigo; o alvo é que tem de ser escrito. | grep repo-wide `canary` → vazio; `tests/CMakeLists.txt:15-91` regista os 15 testes `merge` e nenhum canário. | código | aberto — implementação |
| PF-14 | O bloco `python3 - <<'EOF'` do plano novo não corre como está: heredoc **citado** ⇒ o shell não expande `$B`; `'/home/USER/core-build'` é placeholder; `scope=[...]` é pseudo-código ⇒ `SyntaxError`. A ideia, sim, é a correcta: um assert de scope converte PF-05 e PF-11 em erro visível em vez de análise a meio. Abaixo deixo a versão que funciona. | leitura do comando proposto. | reportado | aberto |
| PF-15 | `CORE_SANITIZE` **mantém-se** inexistente no plano reescrito (linhas `asan`/`tsan`) ⇒ mesmo resultado de PF-01: build normal, verde sem instrumentação. | reconfirmado na sonda WSL: variável não usada; 0 `fsanitize` nos TUs. | código + teste | aberto — decisão |

Estado do ledger: as três linhas que escrevi em `ESTADO.md` nesta primeira ronda foram substituídas pela reescrita canónica do escrivão; não as repus. A entrada no índice de relatórios manteve-se. Passam a proposta.

### Linhas propostas para `ESTADO.md` (escreve-as o escrivão, não eu)

- **Pendentes** — `Plano tidy + ASan/UBSan/TSan | esta sessão + [relatório](2026-11-17-preflight-sanitizadores-clang-tidy.md) (PF-01…PF-15) | três decisões: (1) ligar `CORE_SANITIZE` no CMake, com `FATAL_ERROR` quando o compilador não suporta o sanitizer pedido; (2) criar os alvos `asan_canary`/`tsan_canary` que o plano já assume; (3) como entra `juce_host.cpp` na análise estática — Windows com clang-tidy instalado (PF-11) ou fora do âmbito, dito à superfície.`
- **Estado medido** — `Sanitizers e clang-tidy | **nenhum resultado**: as sondas mediram o build, nunca o produto; os três passos estão por executar | medido 17/11 em b80017e (PF-01…PF-15)`

### Sequência executável hoje, sem tocar no CMake

O passo 1 corre em WSL sobre tudo o que o Linux compila e declara `juce_host.cpp` fora do âmbito (PF-11). Clone próprio em ext4, builds fora da árvore; não medi a penalização de compilar em `/mnt/d` (drvfs), por isso o clone é a recomendação, não uma medição.

```sh
SRC=~/core-src; B=~/core-build; rm -rf $SRC && git clone /mnt/d/CORE/CORE $SRC   # ext4, árvore limpa
cmake -S $SRC -B $B/tidy -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCORE_BUILD_APP=OFF -DCORE_BUILD_TESTS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build $B/tidy -j

cd $SRC && python3 - "$B/tidy/compile_commands.json" <<'PY'
import json, sys, glob, os
db = json.load(open(sys.argv[1]))
have = [os.path.normpath(e['file']).replace('\\', '/') for e in db]
want = sorted(sum([glob.glob(p) for p in (
    'engine/src/core/*.cpp', 'engine/src/dsp/*.cpp', 'engine/src/io/*.cpp', 'engine/src/app/*.cpp',
    'app/domain/src/*.cpp', 'app/control/src/*.cpp', 'app/engine-client/src/*.cpp',
    'app/sampling/src/*.cpp', 'app/previews/src/*.cpp')], []))
missing = [f for f in want if not any(f.replace('\\', '/') in h for h in have)]
sys.exit('SCOPE INCOMPLETO: %s' % missing if missing else 0)
PY

cd $SRC && clang-tidy -p $B/tidy \
  --checks='-*,bugprone-*,concurrency-*,performance-*,clang-analyzer-*' \
  --header-filter='(engine|app)/(src|include)/' \
  engine/src/core/*.cpp engine/src/dsp/*.cpp engine/src/io/*.cpp engine/src/app/*.cpp \
  app/domain/src/*.cpp app/control/src/*.cpp app/engine-client/src/*.cpp \
  app/sampling/src/*.cpp app/previews/src/*.cpp > $B/tidy-round1.txt
```

Os passos 2 e 3 **não correm** enquanto as decisões (1) e (2) não existirem: hoje a cadeia parte no canário inexistente e, se for afrouxada para `;`, dá verde sem nada instrumentado. Quando existirem, manter `halt_on_error=1` **e** gredar os logs — `ERROR: AddressSanitizer`, `runtime error:`, `WARNING: ThreadSanitizer` — porque o código de saída do `ctest` não é prova por si (PF-07).

---

## Adendum 2 — 17-11-2026 (noite) — execução autorizada: «Correr o plano já»

Âmbito e autorização: o dono escolheu executar o plano. Implementação limitada ao que os passos 2/3 assumem e não existia — `CORE_SANITIZE` no CMake e os dois canários. **Nenhuma linha de código de produto, DSP ou UI foi alterada.** Base das medições: `eb9d847` (o HEAD moveu-se durante a sessão: `b80017e` → `eb9d847` → `1bf65f3` → `e502be9`, commits do ledger pelo escrivão). O diff `eb9d847..e502be9` é **só documental** (`ESTADO.md`, três ficheiros em `docs/`), pelo que o código medido é o mesmo do HEAD actual. As alterações desta entrega estão por commitar: `CMakeLists.txt` +27, `tests/CMakeLists.txt` +11, `tests/canary.cpp` novo.

### Alterações (implementação)

| Ficheiro | O que entrou | Porque |
| --- | --- | --- |
| `CMakeLists.txt` | `CORE_SANITIZE` como variável de cache declarada + validação + flags por compilador | a opção não existia (PF-01/PF-15); `cl.exe` ignora `undefined`/`thread` com `D9002` e exit 0 (PF-02), por isso o MSVC **recusa** em vez de avisar |
| `tests/CMakeLists.txt` | alvos `asan_canary` / `tsan_canary`, fora do `ctest` | o plano já os invocava e não existiam (PF-13); são o controlo que prova instrumentação |
| `tests/canary.cpp` | novo: use-after-free (ASan) e corrida intencional (TSan) | rebentar de propósito; se não rebentar, o verde da suite não vale nada |

### Verificação do wiring (medido, não inferido)

| Prova | Resultado |
| --- | --- |
| Recusa `address,undefined` em MSVC | `CMake Error ... MSVC supports address only`, exit **1** |
| Recusa `thread` em MSVC | idem, exit **1** |
| Recusa mistura `address,thread` (qualquer compilador) | `address and thread cannot be combined`, exit **1** |
| Recusa valor desconhecido (`bogus`, GCC/WSL) | `unknown value 'bogus'`, exit **1** |
| Instrumentação ASan+UBSan (WSL, GCC 13) | **65/65** TUs com `-fsanitize=address,undefined` |
| Instrumentação TSan (WSL) | **65/65** TUs com `-fsanitize=thread` |
| Instrumentação ASan (host, MSVC 19.51) | **66/66** TUs com `/fsanitize=address` |
| Inércia sem a opção (WSL) | 64 TUs, **0** com `fsanitize` |
| Canário ASan | exit **1**, `ERROR: AddressSanitizer: heap-use-after-free ... canary.cpp:10` |
| Canário TSan | exit **66**, `WARNING: ThreadSanitizer: data race ... canary.cpp:22` |

O TSan neste WSL (kernel 6.18) morre com `FATAL: ThreadSanitizer: unexpected memory mapping` enquanto `vm.mmap_rnd_bits=32`. Baixado para **28** dentro do WSL apenas durante cada corrida e **reposto a 32** no fim da mesma cadeia (verificado: `reposto=32`). Nada foi alterado no Windows.

### Passo 1 — clang-tidy (WSL, LLVM 18.1.3)

Gate de scope antes da análise: **52 ficheiros pedidos → 50 analisados, 2 excluídos declarados, 0 em falta**. O controlo negativo (exigir `juce_host.cpp` ao banco Linux) falha com `SCOPE INCOMPLETO`, exit 1 — o gate não é vazio.

Excluídos, com motivo: `engine/src/plugins/juce_host.cpp` (JUCE/desktop gateados por `WIN32`, PF-11) e `app/engine-client/src/engine_client.cpp` (`app/engine-client/CMakeLists.txt` sai cedo fora de Windows; `#error` em `engine_client.cpp:5`). **Nenhum dos dois foi analisado nem instrumentado em lado nenhum nesta ronda.**

Resultado: **766 avisos, 0 erros**, em 50 TUs. Destes, **~310 são do `json.hpp` vendored** (`engine/third_party/json.hpp`) — o `--header-filter='(engine|app)/(src|include)/'` do plano deixa entrar terceiros porque o caminho relativo passa por `engine/src/core/../../third_party`. **456 são do nosso código.**

| Check (só código nosso) | N |
| --- | --- |
| bugprone-easily-swappable-parameters | 193 |
| bugprone-implicit-widening-of-multiplication-result | 95 |
| bugprone-narrowing-conversions | 63 |
| performance-enum-size | 15 |
| performance-inefficient-vector-operation | 12 |
| concurrency-mt-unsafe | 10 |
| performance-inefficient-string-concatenation | 9 |
| bugprone-misplaced-widening-cast | 7 |
| bugprone-empty-catch | 7 |
| bugprone-suspicious-string-compare | 6 |
| performance-avoid-endl | 5 |
| bugprone-not-null-terminated-result | 4 |
| bugprone-branch-clone | 4 |
| bugprone-assignment-in-if-condition | 3 |
| restantes (performance-*) | 4 |

Ficheiros com mais avisos nossos (caminhos normalizados — o tidy emite `engine/src/core/../dsp/…`, que sem normalizar parte a contagem do mesmo ficheiro): `engine/src/core/graph.cpp` 49, `engine/src/core/project.hpp` 44, `app/domain/src/edit.cpp` 32, `engine/src/dsp/clip_grains.hpp` 27, `engine/src/dsp/song_clock.hpp` 24, `app/domain/src/pcore.cpp` 22.

Triagem feita à mão (classificação, não correcção):

- `concurrency-mt-unsafe` ×10 → é o mesmo `std::getenv("CORE_EXPORT_THREADS")` em `engine/src/core/workers.hpp:51`, repetido por cada TU que o inclui. Chamadores: `engine/src/core/server.cpp:111` e `engine/src/core/render_job.cpp:286`, ambos a construir o `WorkerPool` no arranque do export — **fora do callback de áudio**. Confirmado no código; risco baixo, não é race demonstrado.
- `bugprone-not-null-terminated-result` ×4 → `engine/src/io/wav.cpp:53-56`, `memcpy` das tags `"RIFF"`, `"WAVE"`, `"fmt "`, `"data"` para o cabeçalho binário. **Ruído**: não são strings.
- `bugprone-assignment-in-if-condition` em `engine/src/dsp/output_guard.cpp:63` → atribuição intencional dentro do `else if` (actualiza o release e compara no mesmo passo). **Ruído**. Os outros dois (`engine.cpp:314`, `samples.cpp:118`) não foram lidos.
- Observação de leitura, não verificada por teste: `engine/src/io/wav.cpp:52` calcula `dataSize` em `uint32_t` — truncaria acima de 4 GiB. Fora do âmbito desta ronda; fica como pendência de leitura, não como bug provado.

### Passo 2 — ASan+UBSan (WSL, GCC 13, `halt_on_error=1`, `detect_leaks=1`)

Canário primeiro: rebentou (acima). Suite `merge`: **15/15 passadas, 113 casos, 1456 verificações, 0 falhas** — 135,7 s na primeira corrida, 132,6 s na repetição verbose de onde vêm as contagens.

Contagens por suite (do log verbose): engine 20/81, domain 9/64, role 21/216, midi 8/115, bug 10/49, bitwig 2/25, audit 7/65, audit2 5/66, hunt 14/649, round3 7/82, calls 10/44; os quatro `cli_*` não imprimem casos. Total **113 casos / 1456 verificações** — os mesmos números que o ledger tinha como *reportados* do audit Linux em `59cf952`, agora **medidos** neste snapshot e sob sanitizers.

Reports nos logs: **0 `AddressSanitizer`, 0 `runtime error:`, 0 `LeakSanitizer`**. Isto só é dizível porque a primeira corrida (`--output-on-failure`, 15/15 verdes) produziu um log **sem output nenhum** e o grep de 0 era vazio; repetida com `-V`, o log tem 249 linhas com output próprio dos 15 testes (prefixos `1:`…`15:`, linhas `ok …`). O zero é sobre output real.

### Passo 3 — TSan (WSL, GCC 13, `halt_on_error=1`)

Canário primeiro: rebentou com `data race` em `canary.cpp:22`, exit 66. Suite `merge` em verbose: **15/15 passadas, 113 casos, 1456 verificações, 0 falhas** (160,7 s na primeira corrida, 158,0 s na verbose). Log com output próprio dos 15 testes (113 linhas `ok`, 15 prefixos), **0 `FATAL`**, **0 menções a `ThreadSanitizer`** — ou seja, nem races nem o erro de mapeamento de memória. `vm.mmap_rnd_bits` reposto a **32** no fim da cadeia.

Tal como no ASan, a primeira corrida com `--output-on-failure` deu um grep de 0 races **vazio**; o número acima é da repetição em `-V`.

### Não-regressão do build normal (WSL, sem `CORE_SANITIZE`)

Build Debug sem a opção: exit 0, **0** TUs com `fsanitize`, `ctest -L merge -V` **15/15, 113 casos, 1456 verificações, 0 falhas**. A alteração ao CMake é inerte por omissão e não mexe nas contagens. (O primeiro awk desta contagem deu `verificacoes=0` por erro de parsing dos campos — corrigido e recontado sobre o mesmo log; não é um resultado diferente.)

### ASan MSVC (host Windows, MSVC 19.51 / toolset 14.51)

Configurado com `-DCORE_SANITIZE=address -DCORE_BUILD_APP=OFF -DCORE_BUILD_TESTS=ON`, build fora da árvore em `D:/Core-build/msvc-asan`, dentro de `vcvars64`. **66/66 TUs com `/fsanitize=address`**, incluindo `app/engine-client/src/engine_client.cpp` (1) e `engine/src/io/device.cpp` (2). `juce_host.cpp`: 0 (app OFF).

Canário: exit **1**, `ERROR: AddressSanitizer: heap-use-after-free`, com símbolos — `#0 ... in main D:\CORE\CORE\tests\canary.cpp:10` (o `/DEBUG` do wiring dá relatórios legíveis).

Suite `merge` em verbose (`ASAN_OPTIONS=halt_on_error=1`): **15/15 passadas, 113 casos, 1456 verificações, 0 falhas**, 297,8 s. Log com 248 linhas, 113 linhas `ok` e os 15 prefixos de teste; **0 `AddressSanitizer`, 0 `runtime error:`**. Contagens por suite idênticas às do Linux (engine 20/81, domain 9/64, role 21/216, midi 8/115, bug 10/49, bitwig 2/25, audit 7/65, audit2 5/66, hunt 14/649, round3 7/82, calls 10/44).

A primeira corrida (`--output-on-failure`) deu 15/15 em 318,4 s mas um log de 38 linhas sem output próprio — o grep de 0 reports aí era vazio, como nos outros dois casos. Os números acima são da repetição em `-V`.

Isto fecha uma lacuna que o ledger registava como «Contagem de casos em Windows: não existe no log»: **agora existe**, medida sob ASan, neste snapshot. Não é o gate Release da app nem prova de UI: `CORE_BUILD_APP=OFF`, sem janela, sem self-test.

### Evidência ancorada

[`2026-11-17-sanitizadores-evidencia.txt`](2026-11-17-sanitizadores-evidencia.txt) — recusas do wiring, os três canários, as quatro corridas da suite com contagens por âmbito e o resumo do tidy. Logs completos ficam em `D:/Core-build` (host) e `~/core-build` (WSL); o ficheiro ancorado é o que sobrevive a uma limpeza de builds.

### Limitações desta ronda

- **A lacuna é de races e de UBSan, não de memória** (correcção à formulação inicial desta mesma entrega, que dizia «intersecção vazia» sem qualificar): o build ASan do host instrumenta **66/66** TUs, incluindo `app/engine-client/src/engine_client.cpp` (1 TU) e `engine/src/io/device.cpp` (2 TUs) — o transporte entre processos e o dispositivo áudio têm prova de memória em Windows. O que não existe em lado nenhum é **TSan e UBSan sobre esses TUs Windows-only**, porque o toolchain do host não os oferece (PF-02) e o cliente recusa compilar em Linux (`engine_client.cpp:5`). `juce_host.cpp` fica de fora das duas plataformas nesta ronda por `CORE_BUILD_APP=OFF`.
- Linux não prova runtime Windows (`AGENTS.md`): os 113/1456 acima são WSL/GCC, não MSVC.
- O ASan do MSVC não faz detecção de leaks; `detect_leaks` só vale no Linux.
- `app/desktop` e o self-test não entraram em nenhuma ronda (`CORE_BUILD_APP=OFF`); nada disto é prova de UI.
- O tidy correu sobre o scope Linux; os ~310 avisos de `json.hpp` não são dívida nossa e não devem entrar em nenhuma contagem de qualidade do Core.

### Linhas propostas para `ESTADO.md` (escreve-as o escrivão)

- **Estado medido** — `Sanitizers + tidy, ronda de 17/11 (noite) | wiring CORE_SANITIZE ligado e provado (65/65 WSL, 66/66 MSVC; quatro recusas com FATAL_ERROR; três canários rebentam) · tidy WSL: 766 avisos / 0 erros, 456 nossos + 310 do json.hpp vendored, 50 TUs analisados e 2 excluídos declarados · ASan+UBSan WSL 15/15 · TSan WSL 15/15 · ASan MSVC host 15/15 · build normal sem a opção 15/15 — todos 113 casos / 1456 verificações / 0 falhas / 0 reports, sobre logs verbose com output real | medido em eb9d847 + alterações por commitar; evidência em docs/relatorios/2026-11-17-sanitizadores-evidencia.txt`
- **Estado medido** — `Contagem de casos em Windows | 113 casos / 1456 verificações, 15 suites merge, sob ASan MSVC, 297,8 s | medido 17/11 (noite); substitui a ausência registada antes. Não é gate Release nem prova de UI: CORE_BUILD_APP=OFF`
- **Pendentes** — `Lacuna de races/UBSan no transporte | engine_client.cpp:5 (#error Windows-only) + CMakeLists.txt:29/45 | ASan MSVC cobre o ficheiro (66/66 TUs), mas TSan/UBSan não existem no host e o cliente não compila em Linux: o código entre processos nunca é instrumentado para races. Decisão: portar para POSIX, ou aceitar e registar a lacuna`
- **Pendentes** — `Triagem dos 456 avisos tidy | relatório §Adendum 2 | 193 swappable-parameters / 95 widening / 63 narrowing por classificar; três já triados (getenv fora do RT, tags RIFF, output_guard intencional). Correcções exigem âmbito explícito`
- **Pendentes** — `header-filter do tidy deixa entrar terceiros | engine/third_party/json.hpp | ~310 avisos que não são dívida do Core; excluir third_party antes de qualquer contagem de qualidade`

---

## Adendum 3 — 17-11-2026 (noite) — ratificação D92/D93, alinhamento ao texto do dono e re-medição

Autorização: o dono ratificou **D92** (wiring do `CORE_SANITIZE` + canários + assertiva de cobertura) e **D93** (os três passos em WSL; no host só ASan MSVC; sem instalar LLVM), escolhendo a opção (a) que o escrivão tinha proposto para a colisão **C12**. Citação desta sessão: «D92/D93 (ratifico já)» e «Alinhar primeiro (recomendado)». O texto original do wiring está em C12, fonte `D--CORE/61b1abc0…jsonl` **L616**, ts `2026-11-17T19:30:21.934Z`. **O `ESTADO.md` não foi tocado por esta entrega**; a formalização cabe ao escrivão.

### Os quatro desvios ao texto do dono, fechados

O texto de C12 pedia quatro coisas que a primeira implementação não fazia. Foram implementadas e re-medidas:

| Pedido no texto | O que estava | O que ficou |
| --- | --- | --- |
| «exige Debug» | aplicava-se em qualquer configuração | `FATAL_ERROR` se `CMAKE_BUILD_TYPE` ≠ Debug — provado: Release + address recusa com `CORE_SANITIZE requires CMAKE_BUILD_TYPE=Debug (got 'Release')` |
| «instrumenta só os nossos targets (JUCE/miniaudio ficam de fora, declarado)» | flags globais; `miniaudio_impl.c` instrumentado (medido) | lista explícita `CORE_SANITIZED_TARGETS` aplicada no fim da raiz, depois de todos os subdirectórios. **Único TU de fora: `miniaudio_impl.c`**, nas três árvores. JUCE e `CoreSetupData` fora; os alvos que o JUCE gera (ex. `CoreTestSynth_VST3`) não estão na lista e não são instrumentados — declarado no comentário |
| «canários (asan/tsan/ubsan)» | só asan e tsan | `ubsan_canary` novo (`CORE_CANARY_UBSAN`, overflow signed via `volatile`), criado só quando `undefined` está pedido e o compilador não é MSVC |
| «assertiva de cobertura que falha se algum ficheiro do scope não estiver no compile_commands» | script no meu scratch, fora do repo | **`ci/tidy_scope.py`** no repo. As exclusões são **derivadas do build** (JUCE presente? compiler é `cl.exe`?), não declaradas pelo caller, para não poderem ser usadas para esconder ficheiros |

### Dois defeitos meus, apanhados pelos controlos e corrigidos

1. **Canário TSan flaky.** O desenho inicial (um par de escritas, `std::thread` + `join`) falhou a detecção em **3 de 5** corridas: exit 0, log com uma linha (`2`), sem report. Um canário que falha metade das vezes não prova instrumentação nenhuma — e o verde da suite TSan da ronda 2 tinha sido precedido por esse canário mudo. Substituído por 200 000 acessos de cada lado, libertados por um `std::atomic<bool>`: **5/5 corridas com `WARNING: ThreadSanitizer: data race`**, exit 66. Os binários estavam correctamente ligados ao `libtsan.so.2` e o TU tinha `-fsanitize=thread` (verificado por `ldd` e pelo banco) — o defeito era do desenho do canário, não do wiring.
2. **`ci/tidy_scope.py --require` não falhava e escrevia um ficheiro.** A primeira versão (a) deixava a exclusão derivada sobrepor-se ao pedido explícito, por isso o controlo negativo passava, e (b) tratava o valor de `--require` como terceiro posicional, escrevendo a lista de ficheiros analisados para um caminho com o nome do ficheiro exigido. O controlo negativo foi o que apanhou os dois: chegou a criar `engine/src/plugins/juce_host.cpp` **na cópia WSL** (`~/core-src`, 2452 bytes de lista). O ficheiro real do repo foi verificado intacto: 25115 bytes, mtime 17:33, `git status engine/` vazio. Corrigido: `--require` sobrepõe-se a qualquer exclusão e o parsing separa opções de posicionais. Agora o controlo dá **exit 1** com `SCOPE INCOMPLETO: ['engine/src/plugins/juce_host.cpp']`, e o gate normal **exit 0**.

Nota de método: o `exit=0` que apareceu na primeira leitura do controlo corrigido era o código de saída do `tail` num pipeline, não o do python. Medido sem pipeline: 1.

### Re-medição completa (ronda 3, código alinhado)

| Árvore | TUs | Instrumentados | De fora | Canário | Suite `merge` | Casos / verificações | Reports |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ASan+UBSan · WSL GCC 13 | 66 | 65 | `miniaudio_impl.c` | asan 3/3, ubsan 3/3 | 15/15 | 113 / 1456 | 0 |
| TSan · WSL GCC 13 | 65 | 64 | `miniaudio_impl.c` | tsan **5/5** | 15/15 | 113 / 1456 | 0 races, 0 FATAL |
| Normal · WSL, sem opção | 64 | 0 | — | n/a | 15/15 | 113 / 1456 | 0 |
| ASan · host MSVC 19.51 | 66 | 65 | `miniaudio_impl.c` | asan exit 1 com símbolos | 15/15 | 113 / 1456 | 0 |

Todas as suites em `-V`, com 113 linhas `ok` e os 15 prefixos de teste em cada log — nenhum zero vazio. Contagens por suite inalteradas (engine 20/81, domain 9/64, role 21/216, midi 8/115, bug 10/49, bitwig 2/25, audit 7/65, audit2 5/66, hunt 14/649, round3 7/82, calls 10/44). `ctest -N` continua a listar **15 testes, 0 canários**: os canários estão fora do `ctest`, por isso as contagens `merge` não se mexeram. `vm.mmap_rnd_bits` reposto a **32** no fim (verificado).

O tidy **não foi repetido**: os 50 TUs analisados não mudaram (as alterações são build system + um ficheiro de teste fora do scope), e a árvore tidy tem 0 TUs instrumentados. Os 766 avisos / 0 erros e a separação 456 nossos / 310 `json.hpp` mantêm-se, agora com o gate a viver em `ci/tidy_scope.py`.

### Âmbito dos commits desta entrega

- **Commit 1 (build)**: `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/canary.cpp`, **`ci/tidy_scope.py`**. Quarto ficheiro além dos três listados no pedido do dono, porque a assertiva de cobertura passou a viver no repo (pedido de C12). Zero produto/DSP/UI.
- **Commit 2 (docs)**: este relatório, `2026-11-17-sanitizadores-evidencia.txt`, `tidy-round1.txt` (6551 linhas, ancorado do WSL), e a linha do índice.
- **`docs/relatorios/README.md` tem duas alterações concorrentes**: a minha linha e a linha de outra entrega (launcher GUI). O staging é cirúrgico — só a minha linha entra no índice do commit; a linha alheia fica por commitar na árvore, intacta.
- Não tocados: `ESTADO.md` (single writer = escrivão, D72/D80), `.qoderignore`, `ci/compilar.bat`, `docs/relatorios/2026-11-17-compilar-gui.md`, `docs/relatorios/evidencia/` — trabalho alheio.

### Linhas propostas para `ESTADO.md` (acrescentam às do Adendum 2)

- **Decisões** — `D92 | 17/11 noite | «D92/D93 (ratifico já)» (selecção do dono nesta sessão, sobre a opção (a) de C12); texto do wiring em L616, ts 19:30:21.934Z | wiring CORE_SANITIZE + canários asan/tsan/ubsan + assertiva de cobertura | ratificada | resolve a colisão C12 dos números «75:»/«D76»`
- **Decisões** — `D93 | 17/11 noite | mesma selecção; texto em L616 | os três passos correm em WSL; no host só ASan MSVC; não se instala LLVM no host | ratificada | —`
- **Estado medido** — `Sanitizers alinhados a D92 | Debug exigido; só targets nossos (único de fora: miniaudio_impl.c); canário ubsan novo; ci/tidy_scope.py no repo com exclusões derivadas do build · ASan+UBSan WSL 15/15, TSan WSL 15/15, ASan MSVC 15/15, normal 15/15 — todos 113 casos / 1456 verificações / 0 reports em logs -V · canários asan 3/3, ubsan 3/3, tsan 5/5 | medido 17/11 noite; dois defeitos próprios (canário tsan flaky 2/5, gate --require) corrigidos e re-medidos`
- **Pendentes** — `Alvos JUCE gerados não instrumentados | CMakeLists.txt (CORE_SANITIZED_TARGETS) | com CORE_BUILD_APP=ON, CoreTestSynth_VST3 e semelhantes ficam fora da lista; não medido (app OFF em todas as rondas)`
