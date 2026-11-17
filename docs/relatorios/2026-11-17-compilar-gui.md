# Relatório — 2026-11-17 — launcher de build com GUI (`ci/compilar.bat`)

## O que foi medido

- Repositório, branch e SHA inicial/final: `D:/CORE/CORE`, `main`. Início da sessão `eb9d847`; **os builds e gates abaixo foram medidos em `9d87065` com alterações por commitar de outro actor** (`CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/canary.cpp` — wiring de `CORE_SANITIZE`). A árvore moveu-se durante a sessão (`eb9d847` → `e502be9` → `9d87065`); nenhum ficheiro desse trabalho foi tocado por mim.
- Âmbito e autorização: pedido do dono nesta sessão — «quero fazer um bat para compilar o core […] tem de ter gui, para escolher onde o quero compilar ser facil de usar», acompanhado de uma proposta do Qwen. Âmbito: um launcher `.bat` com GUI + a sua prova. **Nenhum código de produto, CMake ou teste foi alterado.**
- Ambiente: Windows 10.0.26300 x64; Windows PowerShell 5.1.26100.9818; MSVC 19.51.36260 (VS 18 BuildTools, `vcvars64.bat` presente); CMake 4.4.4; Ninja 1.13.2; JUCE local `D:/Core-deps/JUCE-9.0.5`. GUI conduzida por Win32 (`BM_CLICK`, `WM_SETTEXT`, `PrintWindow`) a partir de scripts de prova em `D:/Core-build/prova-compilar/` — não por cliques humanos.
- Testes executados: `ctest` do próprio repo através do botão Gate, duas vezes: `-L merge` = **15/15** e merge+desktop = **16/16**, ambos VERDE (ver Verificação).
- Limitações: tipo **Debug não foi compilado**; os botões «…» (`FolderBrowserDialog`) não foram exercitados; a guarda de gerador diferente de Ninja não foi exercitada (não existe cache não-Ninja disponível para apontar); caminhos de build **não-ASCII falham no `juceaide` do JUCE** (medido, não é do launcher); durante algumas capturas a janela foi minimizada por um actor externo ao meu controlo (capturas de 850 bytes descartadas; leitura refeita com `ShowWindow` restore); numa ronda o meu driver de prova fechou por engano uma janela do Explorador do dono intitulada «CORE» (confusão por título; sem perda de dados; ronda refeita identificando o processo `Core.exe`).

## Resultados

| ID | Problema / resultado | Evidência (SHA, ficheiro:linha ou execução) | Classificação | Estado |
| --- | --- | --- | --- | --- |
| QW-01 | O `.bat` do Qwen **nunca abre janela**: `IndexOf('#PS1#')` encontra o marcador dentro da própria linha do loader (índice 123, linha 2), e o `iex` recebe `'); iex $s.Substring($i+5)"…` → ParseException. Duplo clique = consola que fecha sem nada. | execução do bat colado verbatim: `iex : At line:5 char:44 … Unexpected token 'Core'` + `The string is missing the terminator: '`; sonda do índice: 123 | teste | corrigido no launcher novo (marcador por regex `(?m)^#PS1#\r?$`, que só casa no início de linha) |
| QW-02 | O dropdown «Sanitize» do Qwen era um **falso verde**: `CORE_SANITIZE` não existia no build nos SHA medidos (`b80017e`/`eb9d847`); `cl.exe` ignora `undefined`/`thread` com exit 0. | [pré-flight de hoje](2026-11-17-preflight-sanitizadores-clang-tidy.md) PF-01/PF-02; grep sem ocorrências nesses SHA. Nota: em `9d87065`+por commitar outro actor já ligou `CORE_SANITIZE` com `FATAL_ERROR` | código + teste (pré-flight) | decidido: o launcher **não expõe** sanitizers (é o fluxo de auditoria, não o de compilar); ver Pendências |
| QW-03 | O gate do Qwen (`ctest -L merge --output-on-failure`) não tem `--no-tests=error`: com `CORE_BUILD_TESTS=OFF` na cache dava verde sem testes. | lição Audit 30 em `ci/gate.ps1:43-44`; o launcher novo chama `ci/gate.ps1`, que a tem | código | corrigido (reutiliza o gate auditado) |
| QW-04 | `cmake --build … -j` sem número = paralelismo ilimitado num portátil de 8C/16T com JUCE. | proposta colada; `ci/gate.ps1:13` usa 8 | reportado | corrigido (campo Jobs, default 8) |
| QW-05 | `FolderBrowserDialog` não deixa escolher uma pasta que ainda não existe escrita à mão; `StartsWith` do build-dir dava falso positivo (`D:\CORE\CORE-build`); bat temporário em ASCII partia caminhos com acentos. | leitura da proposta colada | código | corrigido (campo editável + criação automática, comparação de paths com separador, runner em UTF-8 + `chcp 65001`) |
| MY-01 | A **minha primeira versão** do launcher morria sem janela assim que o build produzia saída: scriptblocks PowerShell nos eventos .NET do processo correm em threads do threadpool; a morte levava a árvore toda (runner, `cl.exe`), exit 2, sem `FormClosing`, sem `ProcessExit`, sem evento WER. | vigiamento de processos 1 s a 1 s: `MORREU pid=… powershell … compilar-debug.bat` + todos os `cl.exe` no mesmo segundo; reproduzido até com um runner trivial (`echo`+`ping`) | teste | corrigido: **sem eventos nenhuns** — o runner escreve a saída em `compilar-saida.log` e um `Timer` no thread da UI lê o ficheiro (`StreamReader` com partilha de escrita) e o `HasExited`; re-medido verde |
| GU-01 | Guarda «build dentro do repo» recusa sem criar nada. | clique com `D:\CORE\CORE\build-guarda`: `ERRO: a pasta de build esta dentro do repo…`; `Test-Path` → False; estado fica `pronto` | teste | validado |
| GU-02 | Guarda «cache de outro repo» recusa sem tocar na cache alheia. | clique com `D:\Core-build\0.7.0-VST3-Host` (home `D:/Core Master/development/0.7.0/VST3-Host`): `ERRO: essa pasta ja foi configurada para outro repo… nada aqui e apagado`; cache intacta | teste | validado |
| JU-01 | Pasta de build com **espaços** funciona; com **não-ASCII** o configure falha dentro do JUCE: `Running juceaide failed … output: Unhandled exception`. O CMake recebe o path como um argumento único (aspas do `-D` inteiras, lição Audit 31) — a falha é do `juceaide`, não do quoting. | `Configurar` em `D:\Core-build\prova espaco` → exit 0, 24 s; em pasta com `ç` → exit 1, 24 s, `JUCEUtils.cmake:630` | teste | aberto — limitação JUCE, reportada na GUI como VERMELHO com log |
| PR-01 | Percurso completo pela GUI: janela abre pelo caminho de duplo clique; Configure/Build/Gate/Parar/Abrir Core.exe/memória entre sessões funcionam. | capturas em [evidencia/2026-11-17-compilar-gui/](evidencia/2026-11-17-compilar-gui/); números em Verificação | teste | validado |

## Alterações

- **Implementação (autorizada pelo pedido):** `ci/compilar.bat` — novo, único ficheiro, ASCII + CRLF, ~20 KB. Loader polyglot corrigido; GUI WinForms (repo/build/JUCE escolhíveis, lista de builds já configurados em `D:\Core-build`, Release/Debug, Jobs, APP/TESTES, «Gate tambem com a app»); botões Configurar / Compilar / Config+Compilar / Gate (merge) / Parar / Abrir pasta / Abrir Core.exe; log ao vivo + `compilar.log` gravado no fim; escolhas memorizadas em `%LOCALAPPDATA%\CoreCompilar\escolhas.json`; guardas GU-01/GU-02 e de gerador; `vcvars64` do VS 18 BuildTools com fallback `vswhere -all`; reutiliza `ci/gate.ps1` no gate.
- **Documentação:** este relatório, entrada no índice, pasta de evidência (7 capturas + 3 logs).
- **Não tocado:** `ESTADO.md` (D72/D80 — linhas propostas abaixo), código de produto, CMake, testes, e o trabalho por commitar de outro actor.
- Scratch de prova fica fora da árvore em `D:\Core-build\prova-compilar/` (drivers Win32, capturas, logs). As duas pastas de build de sondagem com espaços/acentos foram removidas por mim; `D:\Core-build\gui-prova` ficou de propósito: build Release completo e verde, pronto a abrir.

## Verificação

Comandos conduzidos **por cliques nos botões da GUI** (Win32 `BM_CLICK`), não por linha de comando equivalente:

1. `Configurar` em `D:\Core-build\gui-prova` (novo): **exit 0, 24 s, VERDE**.
2. `Config+Compilar` na mesma pasta: **exit 0, 196 s, VERDE**, ninja `[329/329]`, `Core Setup.exe` ligado. Captura `04-build-fim.png`; log completo `log-configbuild.txt` (46 641 chars lidos da caixa de log).
3. `Compilar` (no-op): **exit 0, 1 s, VERDE**.
4. `Gate (merge)`: **exit 0, 10 s**, `100% tests passed out of 15`, `Gate do Core: VERDE` (`log-gate.txt`, `06-gate-fim.png`).
5. `Gate (merge)` + checkbox «Gate tambem com a app»: **exit 0, 26 s**, `100% tests passed out of 16` (`desktop` 1 teste, 16.70 s; `merge` 15 testes, 6.06 s) — `gate-resumo.txt`: «Testes: todos (merge + desktop)».
6. `Configurar` em `D:\Core-build\prova espaco`: **exit 0, 24 s, VERDE** (aspas do `-D` inteiras sobrevivem ao espaço).
7. `Config+Compilar` nessa pasta + `Parar` aos 15 s: **exit 1, 17 s, VERMELHO**, `cl/ninja restantes: 0` (árvore morta), botões reactivados (`14-parado.png`).
8. `Abrir Core.exe`: processo `Core.exe` nasce, janela «Core - Psy demo [TEST BUILD (Release) 0.7.0+9d87065226.modified]» (`15-core.png`), fecha por `WM_CLOSE` só nas janelas desse pid → 0 processos.
9. Fechar e reabrir o `.bat`: campos e estado de `Abrir Core.exe` repostos (`17-memoria.png`).
10. Guardas GU-01/GU-02: mensagens exactas em Resultados; nada criado, nada apagado (`20-guardas.png`).
11. Bat do Qwen colado verbatim: ParseException, janela nunca aparece (QW-01).
12. Parse do corpo PowerShell do launcher: 0 erros (`PSParser::Tokenize` + `[scriptblock]::Create`); 0 caracteres não-ASCII; CRLF.

Contagens por âmbito: `merge` = 15 suites/testes ctest (engine, domain, role, midi, bug, audit, audit2, hunt, round3, calls, bitwig + 4 `cli_*`); `desktop` = 1 (`desktop_self_test`). Os 113 casos/1456 verificações por suite são da medição WSL de hoje noutro relatório e **não** foram re-medidos aqui.

## Pendências e passagem

Próxima tarefa: nenhuma bloqueante. O launcher está utilizável: duplo clique em `ci\compilar.bat`, escolher a pasta de build, `Config+Compilar`.

Decisões/perguntas para o dono (uma ronda):

1. **Expor `CORE_SANITIZE` no launcher?** Hoje não expõe de propósito: o wiring existe apenas por commitar (outro actor) e sanitizers pertencem ao fluxo de auditoria (pré-flight PF-01…PF-15). Se quiseres o dropdown, diz — passa a ser um campo a mais, com os `FATAL_ERROR` do wiring a protegerem os falsos verdes.
2. **Caminhos de build não-ASCII**: limitação medida do `juceaide` (JU-01). Aceitar e documentar, ou investigar um workaround no JUCE (fora do âmbito deste pedido)?

### Linhas propostas para `ESTADO.md` (escreve-as o escrivão, não eu)

- **Estado medido** — `Launcher de build com GUI | ci/compilar.bat (novo, 17/11): configure/build/gate por botões, medidos em Windows @ 9d87065 + wiring de sanitizers por commitar — Config+Compilar 329/329 exit 0 (196 s); gate merge 15/15 e merge+desktop 16/16 VERDE; Parar mata a árvore; guardas dentro-do-repo e cache-alheia recusam sem escrever; build com espaços ok, não-ASCII falha no juceaide | medido 17/11, [relatório](2026-11-17-compilar-gui.md)`
- **Pendentes** — `Launcher: expor CORE_SANITIZE? + limitação não-ASCII do juceaide | [relatório](2026-11-17-compilar-gui.md) §Pendências | duas perguntas ao dono; por exercitar: Debug, diálogos «…», guarda de gerador`

---

## Adendum — 17-11-2026 (noite) — «ele actualiza com novos testes?»

Pergunta do dono depois da entrega. Medido, não afirmado: cópia da árvore de trabalho (`cp -a`, 24 MB) para
`D:\Core-build\prova-testes\src` (scratch meu, removido no fim; o repo partilhado não foi tocado), testes novos
acrescentados **à cópia** e conduzidos pelos botões da GUI.

Mecanismo confirmado no código gerado: `build.ninja` tem `build build.ninja … tests\CTestTestfile.cmake: RERUN_CMAKE … <repo>\tests\CMakeLists.txt …`
— qualquer edição a `tests/CMakeLists.txt` faz o Ninja re-configurar no próximo `cmake --build`, mesmo sem carregar em
Configurar. O botão Gate chama `ci/gate.ps1`, que configura explicitamente em todas as execuções. Não há contagens fixas
no launcher: o `ctest` descobre os testes no configure.

| Passo | Acção na GUI | Resultado medido |
| --- | --- | --- |
| T1 | `Config+Compilar` na cópia com `prova_novo` (LABELS merge) acrescentado antes do configure | exit 0, 233 s, VERDE |
| T2 | `Gate (merge)` | **17/17** = 16 merge (15 + `prova_novo`) + `desktop_self_test`; o checkbox «Gate tambem com a app» tinha ficado memorizado ligado da ronda anterior — por isso este gate correu todos |
| T3 | acrescentar `prova_dois` (merge) e `prova_sem_label` (sem label) **depois** do configure; carregar **só em `Compilar`** | exit 0, 2 s; `tests\CTestTestfile.cmake` passa a conter os dois → o Ninja re-configurou sozinho |
| T4 | `Gate` (checkbox ainda ligado = todos) | **19/19**, inclui `prova_sem_label` (#19) e `desktop_self_test` (#16) |
| T5 | desligar o checkbox; `Gate (merge)` | **17/17** = 15 + `prova_novo` + `prova_dois`; **sem** `prova_sem_label` e **sem** desktop |

Conclusões: (1) sim, testes novos entram sem actualizar o launcher — por Configurar, por Config+Compilar, e até por
Compilar sozinho; (2) **o gate merge só corre testes com `LABELS merge`**: um teste novo sem label não entra no gate
merge (entra no gate com a app, que corre todos). Quem acrescentar testes fora do lista de labels em
`tests/CMakeLists.txt:90-97` fica fora do gate de merge — é o desenho actual do repo, não do launcher.
Evidência: `log-t2.txt`, `log-t4.txt`, `log-t5.txt`, `22-testes-novos.png` na pasta de evidência.