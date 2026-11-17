# ESTADO canónico — d-aicore/core

Único ponto de passagem do Core. Escrito **só pelo escrivão do ledger** (D72/D73/D79/D80); outras entregas
propõem linhas novas como **Pendentes** + relatório em [`docs/relatorios/`](docs/relatorios/README.md), nunca
editam esta página. Regras de trabalho em [AGENTS.md](AGENTS.md).

| | |
| --- | --- |
| Repo | `D:\CORE\CORE` · remote `https://github.com/d-aicore/core.git` (nome antigo `dmo-app/qwen-core` → histórico, D81) |
| HEAD | `b80017efdbb6b47bc65a82338c403548e095967f` — «Core: estado inicial do Pattern Maker + Tools» |
| Âmbito do ledger | **só o Core** — verbatim do dono, 17/11: «preciso que vas apontando decisoes sobre o core num ficheiro» · «apenas sobre o core nada de aicore e fundmmentals» |
| Origem desta página | reconciliação do histórico confirmada **de uma vez** pelo dono em 17-11-2026 19:17 (D73). Rascunho confirmado: `D:\CORE\.escriva\RECONCILIACAO-RASCUNHO.md`; mapa old→new em [`docs/historico/MIGRACAO-LEDGER.md`](docs/historico/MIGRACAO-LEDGER.md); página anterior arquivada em [`docs/historico/ESTADO-2026-11-17-anterior.md`](docs/historico/ESTADO-2026-11-17-anterior.md) |
| Formato | `D### · data · citação verbatim · fonte · âmbito/gatilho · estado · supersede/invalida` (D72) |
| Append-only | numeração contínua, nunca reset; invalidações **por referência**; entradas nunca editadas nem apagadas (D72) |

**Fonte de citação abreviada:** «A:nn» = linha nn de `D:\Core Master\ESTADO.md` (predecessor A, cânone por
número). «L335» = linha 335 de `D--CORE/61b1abc0-db5c-4d05-989e-4ff6c7788120.jsonl`, turno do dono com
timestamp `2026-11-17T19:17:54.706Z`. «esta sessão» = chat presente.

---

## Decisões / Proveniência

### Ratificadas pelo dono, carried do predecessor A (D84)

| D### | Data | Citação verbatim | Fonte | Âmbito/gatilho | Estado | Supersede / invalida |
| --- | --- | --- | --- | --- | --- | --- |
| D40 | 16/11 11:15 | «D+1 — Verificação de ação da AI = invariantes de documento + dry-run áudio + Undo único. `ctest` é gate de código, nunca verificação em runtime.» | A:41, msg `…6tZBa9w56YW2iN53qvuujY` | verificação de acções | ratificada | verificação fica no Core, AI fora (D45/D52) |
| D41 | 16/11 11:15 | «D+2 — Regra de ferro (5 pontos) em vigor a partir desta entrega; sem exceções por "é rápido".» | A:42 | testes/gate | ratificada | conflito com PLANO.md de 15/11 em aberto (A22) |
| D42 | 16/11 11:15 | «D+3 — Fase 0 e Fase 1 antes de qualquer feature nova. O resto entra por ordem, uma fase por entrega.» | A:43 | ordem de trabalho | ratificada | — |
| D45 | 16/11 13:09 / 13:05 | «ai nunca foi para estar no binario disse sempre como plugin» · «o painel passa a cliente fino, e a memória de receitas em SQLite sai do Core para o processo de AI.» | A:46, msgs `…VTzjKAKJ9c9QtTScrrVZR6`, `…YZs7mJd5fS81DeQsHBjQWN` | processo, persistência | ratificada | divisão dos testes AI (Core/processo/IPC) = proposta da equipa |
| D46 | 16/11 10:25 / 10:51 / 09:33 | «quero um foco total no estilo de som que quero criar removendo todo clutter que poode apenas causar problemas, asism criand o daw hiperfocused e leve» · «é meter ele a rolar bem com um determinado numero de plugisn e deixar os plugins serem a qualidade ele so tem de ser a interacao» · «core nao deve ter funcinalidade de alterar os sons. isso deve vir tudo dos plugins adicionais» | A:47, msgs `…Voth7T6Q26hD4nqQWmiWLX`, `…1ySvdxVidhP9h8xLKTW38K`, `…4A7p3vGbYsSdzPQBFu2APG` | âmbito, áudio, DSP | ratificada | T27–T29 = proposta da equipa; definição de 17/11 em D86 |
| D47 | 16/11 13:18 (e 13:09/13:10/01:05/02:03/02:16) | «Para o registo de hoje, a entrada deve dizer três coisas: foi erro da equipa sem aprovação do dono; está a ser corrigido (AI como plugin fora do binário); e invalida qualquer referência que trate a colocação como decisão.» · «onde disse que fazia parte do binarioM» · «Tentar fazer atraves de um plugin» · «tem de ser base de dados local» / «que vem com plugin» · «vamos implementar outar evz mas como plugin» | A:48, msgs `…N961E5u9ijRV11yNrTKj51`, `…AuZu4WN5nYvdA1vzU39LTT` | processo, persistência, UI | ratificada | invalida a lista da secção Invalidadas |
| D48 | 16/11 13:18 | «Só palavras explícitas tuas criam decisões.» · «Todo o PR leva `Autorizado-por: D###`.» · «nova dependência, novo processo, nova persistência (SQLite foi exactamente isto), nova superfície de UI, qualquer coisa no caminho de áudio, qualquer coisa fora da lista de âmbito» · «A ordem é sempre: **decisão → teste invariante → código**.» · «**reversibilidade por defeito** — tudo atrás de flag, módulos fora do processo.» | A:49, msg `…N961E5u9ijRV11yNrTKj51` | governação (5 camadas) | ratificada | — |
| D49 | 16/11 13:25 | cartão de decisão «O M5 (multicore ao vivo) fica ligado ou desligado por defeito até ao soak?» → escolheu «Desligar até passar o soak» | A:50 | áudio, default | ratificada | **corrigida pela D51** |
| D50 | 16/11 13:31 | «Isto é decisão, não proposta. Registem como decisões numeradas com `origem: Diogo`, citando esta mensagem. Referências anteriores a "Sonus" ficam sem efeito.» · «Arquitectura só com aprovação explícita **antes** do código.» · «qualquer default que mude comportamento.» · «Silêncio ≠ aprovação.» · «Facto implementado sem decisão = "erro da equipa, a corrigir".» · «testes em Linux não validam Windows; declarem sempre o que **não** foi testado.» · «Cleanup = só bugs.» · «Uma variável de cada vez: debug no caminho serial.» | A:51, msg `…Dpf42aq5irQKsFiEZMBmFJ` | governação, gatilhos | ratificada | acrescenta gatilho «default» à D48; invalida referências a «Sonus» |
| D51 | 16/11 13:31 | «M5 OFF por defeito. Só passa a ON por nova decisão minha, depois de soak 8 h + bit-a-bit vs serial no meu PC com `CORE_LIVE_THREADS=4`. Adiado para depois da limpeza de bugs; sem prazo.» | A:52, mesma diretiva | áudio, default | ratificada | **corrige D49** |
| D52 | 16/11 13:31 | «AI como plugin fora do binário. Painel = cliente fino; SQLite de receitas sai do Core; catálogo + verificação + Undo ficam no Core. Core-side só criação musical; análise/sound design/stems/treino = AICORE.» | A:53, mesma diretiva | processo, âmbito | ratificada | confirma D45; hosting concretizado em D91 |
| D53 | 16/11 13:31 | «A colocação do painel/memória no PR #4 foi erro da equipa, não decisão minha. Invalidar todas as referências que a tratem como decisão.» | A:54, mesma diretiva | processo, UI | ratificada | confirma D47 |
| D54 | 16/11 13:31 | «Fase 1 — Limpeza de bugs (sem prazo): triagem perda de dados/sessão > caminho de áudio > CPU > cosmético» · «Fase 3 — Medições (só quando eu disser)» | A:55, mesma diretiva, secção «3. PLANO» | ordem de trabalho | ratificada | fases do motor ≠ fases do plano de testes |
| D55 | 16/11 13:31 | «Cada PR: `Autorizado-por`, testes corridos + resultados, o que não foi testado, defaults alterados, "Decisões pendentes do Diogo".» | A:56, secções 4 e 5 | reporte, contrato | ratificada | — |
| D56 | 16/11 13:37 | «**D56 — Regras do gate por âmbito.** A contagem compara-se dentro de cada âmbito: Core (merge/desktop) e pacote da AI (ai). Mover testes entre âmbitos exige decisão numerada; D45/D52 autorizam a saída de `ai_tests`/`aifx_tests` do gate do Core.» · «A recusa "feature da AI em ramo enquanto main gateia 15" fica invalidada.» · «As respostas do Diogo entram como D56 em diante.» | A:57, msg `…Uond9RuFf3fs25J8XwNzam`, título «Rascunho para enviar» | gate, testes | **ratificada sem reserva** — a nota de A («Fica aprovado porque foi o Diogo que o enviou; se era só rascunho, ele diz») foi respondida em L335: «D56: confirmo que ENVIEI o rascunho, é meu → ratificada» | invalida a recusa citada e T11/T12 antigos |
| D57 | 16/11 13:37 | «**R1:** ratifico — descongela no mesmo Undo, com aviso, ficheiro congelado invalidado atomicamente.» «**R2:** ratifico — pré-fader, mixer vivo; teste: automação de volume/pan audível depois do freeze.» «**R3:** ratifico — recusa com mensagem clara e **zero mudança de estado** na recusa.» «**R4/P6:** o PR #20 fica autorizado por D49 + A29 + **D57** (nova decisão que ratifica R1–R3).» | A:58, msg `…Uond9RuFf3fs25J8XwNzam` | áudio, freeze, Undo | ratificada | A29 é linha de auditoria, não decisão |

D43 (constatação da equipa) e D44 (conciliação do documento Guardian) **não são decisões** em A e por isso não
estão nesta tabela; D44 mantém-se listada em Invalidadas.

### Ratificadas por verbatim do dono nesta sessão (bloco confirmado em L335: «§1.2: confirmo, incl. a definição de 17/11»)

| D### | Data | Citação verbatim | Fonte | Âmbito/gatilho | Estado | Supersede / invalida |
| --- | --- | --- | --- | --- | --- | --- |
| D72 | 17/11 | «D72: crio o escrivão do ledger (AI). Só ele escreve ESTADO.md; verbatim+fonte; pendente até eu confirmar; append-only, numeração contínua; invalidações por referência. Primeira tarefa = reconciliação do histórico para eu confirmar de uma vez.» | esta sessão (bloco PAPEL) + L335 «mantenho D72/D73 como ditei» | governação, single writer | ratificada | define o formato desta página |
| D73 | 17/11 | «D73: novo ESTADO.md canónico por reconciliação, não reset. Numeração contínua ou mapa old→new documentado; invalidações e pendentes carregados; citações+fonte obrigatórias; SHAs re-ancorados ou históricos; antigos arquivados.» | esta sessão + L335 | governação, migração | ratificada · executada | execução: esta página + [`MIGRACAO-LEDGER`](docs/historico/MIGRACAO-LEDGER.md) |
| D74 | 17/11 19:17 | «C1/C2: mantenho D72/D73 como ditei. Predecessor B (motor/Serum) = âmbito separado arquivado, prefixo M-; cânone por número = predecessor A. D58–D71 reservados como IDs de proposta/pendente com mapa documentado, nunca ratificados sem palavras minhas; próximas ratificações D74+.» | L335 | numeração, âmbito | ratificada | resolve C1/C2; D36–D140 do B passam a `M-D##` |
| D75 | 17/11 19:17 | «C3: bugs = namespace BUG-, não ocupam D.» | L335 | numeração | ratificada | `D1/D2/D4/D6` de 16/11 lêem-se `BUG-1/2/4/6` |
| D76 | 17/11 19:17 | «C4: D58–D66 do coordenador = propostas pendentes, sem efeito até eu ratificar.» | L335 | governação | ratificada | o que estava escrito como decisão do coordenador é pendente |
| D77 | 17/11 19:17 | «C5: A09 pendente; D-AI-fora/D-DEFER1 mapeiam para D45/D47/D52/D53.» | L335 | governação, AI | ratificada | IDs fora do formato `D###` deixam de aparecer como decisões |
| D78 | 17/11 19:17 | «C6: PROD = âmbito de produto com mapa para D###, não renumera D.» | L335 | âmbito de produto | ratificada | PROD-01…PROD-10 ficam fora da sequência D |
| D79 | 17/11 19:17 | «C7: docs/DECISOES.md → arquivar como rascunho em docs/historico/, remover do vivo; ESTADO single writer.» | L335 | governação | ratificada · **executada** | [`DECISOES-rascunho-2026-11-17.md`](docs/historico/DECISOES-rascunho-2026-11-17.md); AP-01…AP-35 sem efeito de ledger |
| D80 | 17/11 19:17 | «C8: emendar AGENTS/README: entregas propõem via pendentes+relatório; só o escrivão escreve ESTADO.md.» | L335 | governação | ratificada · **executada** | [AGENTS.md](AGENTS.md) e [índice](docs/relatorios/README.md) emendados nesta entrega |
| D81 | 17/11 19:17 | «C9: re-ancorar links/SHAs a d-aicore/core; anteriores históricos.» | L335 | proveniência, links | ratificada · **executada** | referência viva = caminho+linha sobre `b80017e` no remote `d-aicore/core`; SHAs anteriores = históricos. **Medido:** `git cat-file -t 599d558` (e `59cf952`, `5445152`, `ecd2c7a`, `95f411a`, `e9b1707`, `b28520e`) → *Not a valid object name* — só `b80017e` existe aqui, por isso os permalinks da fronteira ficam históricos com nota no próprio documento, e os links dentro de `docs/relatorios/*` **não foram reescritos** (relatório é evidência ligada ao seu snapshot) |
| D82 | 17/11 19:17 | «C10: predecessor B arquivado; governança vive em A; nada de B vivo por omissão.» | L335 | âmbito, migração | ratificada · **executada** | `M-D01…M-D140` não são constraint viva; T01–T33 e G01–G20 idem |
| D83 | 17/11 19:17 | «C11: master vertical/selecção = rollback/stash por agora; re-land por decisão depois.» | L335 | UI, processo | ratificada · **executada** | implementação retirada da árvore; ver Estado medido |
| D84 | 17/11 19:17 | «§1.1: confirmo D40–D57 ratificadas por proveniência (primário não re-verificado; reabre se contradito). D56: confirmo que ENVIEI o rascunho, é meu → ratificada.» | L335 | governação | ratificada | **reabre automaticamente** qualquer dessas entradas se o chat primário contradisser a Proveniência de A |
| D85 | 17/11 19:17 | «§1.2: confirmo, incl. a definição de 17/11.» | L335 | governação, âmbito | ratificada | ratifica em bloco as entradas verbatim de §1.2: o âmbito do ledger («apenas sobre o core nada de aicore e fundmmentals»), a estrutura ditada desta página (D73), a definição do Core (D86), o anúncio da remodelação (D87) e as ordens de 14/11 sobre o DSP próprio (D88) |
| D86 | 17/11 (confirmada 19:17) | «é um daw que nao tem isntrumentos, plugins, nada do genero utiliza apenas plugins de terceiros para isso. o foco e um motor extramamente fiavel, perfomance top grade, e adopçao de plugins sem crash , performan, e integraçao de ai atraves de um pllugin proprio» | esta sessão; confirmada em L335 | âmbito, áudio, performance, AI | ratificada (definição do Core) | confirma D46 e D45/D52; grafia conservada como ditada. Números de performance ficam pendentes |
| D87 | 17/11 | «atençao que o core vai ser remodelado» | esta sessão; enquadramento confirmado em L335 | âmbito, arquitectura | ratificada (anúncio) | desenho concreto da remodelação é pendente; nada do Core anterior passa a constraint viva sem re-ancoragem |
| D88 | 14/11 10:08:27Z | «Apaga todos os efeitos antigos do Core (SHIFT, GLITCH, RETRIG, BODY, FILTER, R-Core, D-Core, METAL, SPACE, ECHO, SPIRAL, ZAP, FLUX), com os testes deles. Onde eram usados, passa a usar o efeito equivalente do Serum. Num ramo, build e ctest. Se compila e passa, entra.» | `D--Qwen-work/e3a22b62…jsonl` L1853 (cwd `D:\Qwen work`); bloco confirmado em L335 | DSP próprio, plugins | ratificada | **supersed** a ordem de 10:06:23Z (`D--Qwen-work/4454b067…jsonl` L921: «Apaga o SHIFT, o GLITCH, o RETRIG, o BODY e o FILTER do Core, com os testes deles. Ficam o R-Core e o D-Core…») na parte «Ficam o R-Core e o D-Core» |
| D89 | 17/11 19:17 | «§2: PROD pendente excepto as partes com o verbatim acima.» | L335 | âmbito de produto | ratificada | PROD-01…PROD-10 ficam em Pendentes; só a parte coberta por D86/D46 está decidida |
| D90 | 17/11 19:17 | «§5: ancorar o gate Windows ao log que colei (guarda-o em docs/relatorios/2026-11-17-gate-windows.txt), caveat árvore suja; re-correr em árvore limpa depois do commit.» | L335 | gate, medição | ratificada · **executada** | invalida a linha «gate Windows sem artefacto local» da página anterior |
| D91 | 17/11 19:17 | «Q9: AI = processo separado + painel cliente fino, descoberto como add-on (addon.json); NÃO VST3 in-process. "plugin próprio" = pacote fora do binário.» | L335 | arquitectura, AI | ratificada | concretiza D45/D52; recusa a leitura in-process; a escolha de transporte continua pendente (A04) |

### Reserva de números (D74)

| Intervalo | Estatuto |
| --- | --- |
| D58–D66 | folha do coordenador — **propostas pendentes**, sem efeito (D76) |
| D67–D71 | reservados, sem conteúdo atribuído; não usar como decisão |
| D72, D73 | mantidas como ditadas pelo dono |
| D74+ | sequência de ratificações, em uso desde esta entrega |

---

## Pendentes (owner = Diogo)

Nada avança sem palavras dele: «Só palavras explícitas tuas criam decisões.» (D48) e «Silêncio ≠ aprovação.»
(D50). Falta de citação diz-se, não se adivinha.

### Do predecessor A, por responder um a um

| Pendente | Citação disponível | Pergunta |
| --- | --- | --- |
| A04 | sem citação — pipes, `core-ai.exe`, `%LOCALAPPDATA%\Programs\Core AI` | transporte e pasta do processo de AI (a forma do processo já está em D91) |
| A06 | «tem de ser base de dados local» / «que vem com plugin» (02:03) | SQLite+FTS5 é escolha da equipa: aceite ou muda |
| A08 | sem citação — permissões ask/readonly/trusted | modelo de permissões |
| **A09** | «aicore nao deve estar na barra superior deve entrar nos plugins como todo resto» (10:58) | confirmada como pendente em L335: entra como plugin (D91) ou mantém entrada própria? |
| A12 | «isto deveria estar dentro d eum plugin chamado tools» · «tools deve ter opcao ai e manual» | Tools é plugin ou painel da app? |
| A13 | sem citação — bus R-Core/D-Core, ROLL, intent FX | destino destes caminhos com D88/D86 |
| A16 | «ainda vejo as tracks com blocos em vez de waves» (12:29) | render em segundo plano: confirmar |
| A18 | «vamos ter de criar novos» (06:01, presets) | onde vivem os presets (D47 diz: pacote AI) |
| A19 | «nao criar plugins que nao sejam necessarios» (10:28) | «Core AI Gate» existe ou morre? |
| A21 | pedido de template hitech (12:32) | base `audit/2026-11-16-ai` é da equipa |
| A22 | — | conflito PLANO.md de 15/11 vs D41 |
| A10, A11, A23, A28 | sem citação | ratificar em bloco ou rejeitar |
| A24 | 116 comentários no código «(Diogo, 2026-11-05…09)» | entram como histórico ou exige re-verificação? |
| A27 | sem citação («Visão do Diogo» na memória) | barra de cima só File/Edit/View/Help: confirmar |
| A14, A15, A17, A20, A25, A26 | têm citação, estado «ratificar» | ratificação em bloco |

### Do repo actual

| Pendente | Onde está escrito | Porque é pendente |
| --- | --- | --- |
| PROD-01…PROD-10 | [docs/DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md](docs/DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md) | regra em **D89**. Coberta por verbatim: «sem instrumentos próprios, só plugins de terceiros» (D86, coincide com D46/A01). **Continuam sem citação**: Pattern Maker + Tools como únicas superfícies, preservar catálogo/alinhamento, retirar `*-Core`, retirar Kick/Cymbal Design, retirar samples distribuídos, limpeza de menus, editor da instância exacta nos painéis de Tools, não limitar a VST3 |
| P-F01…P-F06 | [docs/FRONTEIRA-CORE-AICORE.md](docs/FRONTEIRA-CORE-AICORE.md) | propostas; o próprio documento diz-no (D77) |
| Proposta tudo-ou-nada como regra | docs/FRONTEIRA-CORE-AICORE.md (validação) | D40 fixa «Undo único» e verificação; recusa integral vs filtragem parcial não tem citação |
| Fio de controlo = pipe local, sem sockets | docs/FRONTEIRA-CORE-AICORE.md | A04 (pipes) é proposta da equipa; D50 exige IPC local mas não escolhe transporte |
| `CORE_BUILD_AI=OFF`, `ai_boundary_static`, Core 100% sem pacote | docs/FRONTEIRA-CORE-AICORE.md | D48(4)/D52 pedem os invariantes como testes; nomes e defaults concretos são desenho da equipa |
| **Re-land das superfícies UI/DSP** | D83 + [`MIGRACAO-LEDGER`](docs/historico/MIGRACAO-LEDGER.md) §4 | stash `refs/escriva/unratified-2026-11-17` (`1abade52`) + cópia em disco. As três superfícies partilhavam ficheiros, por isso a ordem sobre master vertical/selecção levou tudo: (1) master vertical + selecção de secção/intervalo, (2) refinamento visual + janela DSP Performance, (3) maqueta de interface. Cada uma precisa de decisão numerada: ratificar, re-land parcial, ou recusar |
| Item 6 do audit | [RESUMO](docs/relatorios/2026-11-17-RESUMO.md) §Falta fazer | «não tenho o texto; nunca me chegou» |
| Itens 16 e 17 (UI + som) | RESUMO | instrumentos próprios nos menus e «Randomize sound» sem nada para randomizar; com D86 cheiram a mal colocados, mas mexer em menus é UI (D48/D50) |
| Timeout do item 9 (5000 ms) | RESUMO | recusar com aviso, esperar mais, ou aplicar o que veio e dizer o que falta? |
| `docs/contracts/core-calls.md` | citado por `calls.hpp`/`bridge.hpp`, **não existe** | escreve-se no lado Core ou vem no ficheiro da separação? |
| CI deste repo | `.github/workflows` não importado | reconstruir precisa de token com scope `workflow`; com que label de gate |
| Preço da hash 3a | [audit](docs/relatorios/2026-11-17-auditoria-core.md) §3 | ≤64 KiB por ficheiro em cache por varredura: confirmas ou hash maior? |
| Alvos numéricos de performance | D86 («performance top grade», «sem crash») | teto de CPU, xruns, ΔRSS, soak, taxa de adopção sem crash: os números existentes (T27–T29) são proposta da equipa |
| Desenho da remodelação | D87 | o que entra, sai e muda de forma |
| Do lado `core-main` | RESUMO | PR #25 (fechar ou mesclar) e ratificar D58 — nada disto toca este repo |

### Colisão em aberto — pede resolução (regra COLISÕES de D72: registar e perguntar, não escolher)

| # | Colisão | Evidência verbatim | Fonte | Opções (não escolho) |
| --- | --- | --- | --- | --- |
| **C12** | Texto de decisões sobre sanitizers chegou com os números **D75 e D76**, que já estão ratificados hoje para outra coisa (D75 = namespace `BUG-`; D76 = folha do coordenador pendente). O primeiro vem escrito «75:» sem o `D`. | «75: ligo CORE_SANITIZE no CMake: address→MSVC (/fsanitize=address + /DEBUG); undefined/thread→só Clang/GCC, FATAL_ERROR se o compilador não suporta (nunca D9002 ignorado); exige Debug; instrumenta só os nossos targets (JUCE/miniaudio ficam de fora, declarado). Acrescento canários (asan/tsan/ubsan) que falham se o sanitizer estiver silencioso, e uma assertiva de cobertura que falha se algum ficheiro do scope não estiver no compile_commands.» · «D76: os três passos correm em WSL (zero instalações; TSan não tem caminho em Windows); no host fica só ASan MSVC como prova de Windows. Não instalo LLVM no host.» | esta sessão, `D--CORE/61b1abc0…jsonl` **L616**, ts `2026-11-17T19:30:21.934Z` (turno interrompido) | (a) entram como **D92/D93** (próximos livres) com esta citação e fonte; (b) pertencem a outro ledger/sessão e ficam fora deste; (c) o dono reescreve com número. **Nada foi implementado nem ratificado por mim a partir deste texto.** O relatório de pré-flight correspondente (`docs/relatorios/2026-11-17-preflight-sanitizadores-clang-tidy.md`) apareceu por comitar na árvore durante esta sessão — sinal de outra entrega a escrever no mesmo checkout; não o comitei |

---

## Invalidadas (por referência)

| Invalida | O que fica sem efeito | Fonte |
| --- | --- | --- |
| D47 + D53 | título do PR #4 «AI dentro do Core»; PR #17 e T05–T08 na forma embebida; `audit/2026-11-16-ai`; `teste/tudo` (PR #13); `bug/pr5-mais-ai` (PR #6); a frase «decisão anterior do painel AI» do contrato de 13:05; premissa de T11 (receitas SQLite no Core) e T12 (testes AI no gate do main); memória da equipa «Core's AI is self-contained» | A:24, A:48, A:54 |
| D47 (nota de proveniência) | a citação «a AI seria um plugin» atribuída à D45 — não existe no chat; valem as de 13:09/13:05 | A:46 |
| D46 (nota de proveniência) | o texto «DAW leve, feita para performance, robustez…» atribuído ao dono — não está no chat; valem as três de 10:25/10:51/09:33 | A:47 |
| D50 | todas as referências anteriores a «Sonus» | A:27, A:51 |
| D51 | D49 na parte em que passar o soak chegava para ligar o M5 | A:28, A:52 |
| D56 | a recusa «Feature da AI em ramo enquanto main continua a gatear 15»; T11/T12 antigos | A:33, A:57 |
| D44 | o documento «testes, CI e plano para o Guardian» (13/11, `api-w21-so-serum@1def47b`) como referência de suites actuais | A:20 |
| D88 + D46 + D86 | DSP/som próprio do Core: SHIFT, GLITCH, RETRIG, BODY, FILTER, R-Core, D-Core, METAL, SPACE, ECHO, SPIRAL, ZAP, FLUX e a linha W-Core↔Serum | transcrições 4454b067 L921 / e3a22b62 L1853 |
| Ordem de 14/11 10:08 (D88) | a ordem de 10:06 na parte «Ficam o R-Core e o D-Core» | L1853 vs L921 |
| Instrução registada na fronteira | a conclusão do audit de que «calls.cpp **e bridge.cpp** têm de ficar no Core» | docs/FRONTEIRA-CORE-AICORE.md §153 |
| **D74 + D82** | os números D36–D140 do predecessor B como decisões vivas deste repo; passam a `M-D##` e ficam arquivados | L335 |
| **D74** | a hipótese de renumerar D72/D73 como D141/D142 (opção (a) da colisão C1) | L335 |
| **D76** | D58–D66 escritos como decisões do coordenador | L335 |
| **D77** | «D-AI-fora» e «D-DEFER1» como decisões com efeito próprio; e A09 como decisão | L335 |
| **D79** | `docs/DECISOES.md` (AP-01…AP-35) como ledger vivo | L335 |
| **D80** | a leitura de `AGENTS.md`/`docs/relatorios/README.md` que dava a qualquer entrega o direito de editar ESTADO.md | L335 |
| **D81** | links para `github.com/dmo-app/qwen-core` como referência actual; SHAs anteriores a `b80017e` como estado presente | L335 |
| **D83** | as linhas «Master vertical e seleção», «Refinamento visual e DSP Performance» e «Maqueta da interface» da página anterior como trabalho **no binário** — ver [`ESTADO-2026-11-17-anterior.md`](docs/historico/ESTADO-2026-11-17-anterior.md) | L335 |
| **D90** | a linha anterior «gate Windows: sem artefacto local / delta a reconciliar» | L335 + [`2026-11-17-gate-windows.txt`](docs/relatorios/2026-11-17-gate-windows.txt) |
| **D91** | a leitura de «integraçao de ai atraves de um pllugin proprio» como VST3 de AI dentro de `Core.exe` | L335 |

---

## Estado medido

| Item | Valor | Estatuto |
| --- | --- | --- |
| Gate Linux | 15 testes / 113 casos / 1456 verificações, 0 falhas | **reportado** pelo audit de 17/11 sobre `59cf952`; não reexecutado nesta sessão |
| Percurso dos casos | 88 → 104 → 111 → 113, nunca desceu | reportado (`2026-11-17-auditoria-core.md` §1) |
| Gate Windows 18:43 | **VERDE — 15/15 `merge`** @ `b80017e` *com alterações por commitar*; build `D:/Core-build/gate-CORE`; 212 s de gate, 7.20 s de testes; por teste: 0.04–2.37 s | log guardado em [`2026-11-17-gate-windows.txt`](docs/relatorios/2026-11-17-gate-windows.txt) com SHA-256 dos originais (D90). **Não é gate de árvore limpa** |
| Gate Windows em árvore de código limpa (19:37) | **VERDE — 15/15 `merge`** @ `eb9d847`, pasta nova `D:/Core-build/gate-CORE-limpa`, build completo 925 passos, 228 s de gate / 6.90 s de testes, `rc=0` | **medido por mim** (D90). Ressalva do script «com alteracoes por commitar» explicada: `ci/gate.ps1:56` usa `git status --porcelain`, que conta docs por comitar e 2 ficheiros não comitados de outra entrega; `git diff` do código = vazio. Log e SHA-256 em [`2026-11-17-gate-windows.txt`](docs/relatorios/2026-11-17-gate-windows.txt) |
| Árvore depois de D83 | `git diff` sobre `app engine tests ci patterns` = **vazio**; código = `b80017e`; commit documental `eb9d847` (14 ficheiros, 712+/52−) | medido antes e depois do commit |
| Recuperação do que foi revertido | stash-commit `1abade528f6fe506152311183d55e38ada681288` em `refs/escriva/unratified-2026-11-17` (506+/129− em 11 ficheiros) + cópia em `D:\CORE\.escriva\unratified-2026-11-17\` (inclui `DspPerformance.*` e os 4 relatórios) | medido |
| Itens do audit não medidos | 7, 8, 15, 18, 27, 28, 31 (Windows-puro: só compilação/leitura) | reportado |
| Limites conhecidos | 3a = hash dos primeiros 64 KiB; 3b = revalida só o cabeçalho WAV; índice da biblioteca ficou na versão 2 de propósito | reportado (`RESUMO.md`) |
| Contagem de casos em Windows | **não existe no log** — 113/1456 são do ambiente Linux | medido (a ausência é o facto) |
| Runtime da app nesta sessão | build + 15 testes de gate corridos em Windows; **a app não foi aberta**, nenhum self-test desktop, nenhum áudio ouvido | medido por mim |

---

## Ligações

| Documento | Papel |
| --- | --- |
| [AGENTS.md](AGENTS.md) | regras de trabalho do repo (emendadas por D80) |
| [docs/relatorios/README.md](docs/relatorios/README.md) | índice de relatórios (emendado por D80) |
| [docs/historico/MIGRACAO-LEDGER.md](docs/historico/MIGRACAO-LEDGER.md) | **mapa old→new** e fontes arquivadas (D73/D81/D82) |
| [docs/FRONTEIRA-CORE-AICORE.md](docs/FRONTEIRA-CORE-AICORE.md) | fronteira Core ↔ AICORE; P-F01…P-F06 são propostas (D77) |
| [docs/DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md](docs/DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md) | PROD-01…PROD-10, pendentes por D89 |
| [audit](docs/relatorios/2026-11-17-auditoria-core.md) · [RESUMO](docs/relatorios/2026-11-17-RESUMO.md) | audit de 31 itens, números Linux |
| [docs/relatorios/2026-11-17-gate-windows.txt](docs/relatorios/2026-11-17-gate-windows.txt) | log do gate Windows ancorado por D90 |
| [docs/AUDITORIA.md](docs/AUDITORIA.md) · `docs/contracts/` | procedimento; `core-calls.md` **não existe** (pendente) |
| `dmo-app/core-main`, `dmo-app/CORE-MANUAL`, `dmo-app/core-create`, `dmo-app/AICORE` | outros repos; fora do âmbito deste ledger pela instrução «apenas sobre o core nada de aicore e fundmmentals» |

**SHAs.** Presente: `b80017e` (cânone). Históricos (D81): `59cf952` (audit Linux), `5445152`/`ecd2c7a`
(organização documental), `599d558` (snapshot da fronteira), `95f411a` (main auditado 16/11), `e9b1707`
(baseline de bugs), `8e816c1`, `b28520e` (Fase 4), `1def47b` (Guardian, pré-reset), `02921ea`/`2d438c9`
(core-create), `1abade52` (stash de D83).

**Fontes das decisões.** Predecessor A = `D:\Core Master\ESTADO.md` (SHA-256 `b65c965b96c1c90f…`, 254 linhas) —
governança, **cânone por número**. Predecessor B = `D:\Qwen work\ESTADO.md` (SHA-256
`a154a264728cd978…`, 1845 linhas; cópia byte-idêntica em `D:\Core Master\historico\ESTADO-2026-11-14.md`) —
motor/Serum, arquivado com prefixo `M-` (D74/D82).
