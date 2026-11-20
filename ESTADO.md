# ESTADO canónico — d-aicore/core

Único ponto de passagem do Core. Escrito **só pelo escrivão do ledger** (D72/D73/D79/D80); outras entregas
propõem linhas novas como **Pendentes** + relatório em [`docs/relatorios/`](docs/relatorios/README.md), nunca
editam esta página. Regras de trabalho em [AGENTS.md](AGENTS.md).

| | |
| --- | --- |
| Repo | `D:\CORE\CORE` · remote `https://github.com/d-aicore/core.git` (nome antigo `dmo-app/qwen-core` → histórico, D81) |
| HEAD local | `7ad8ccaa72cadd132b2f19c33142c4b3eed85e68` — medido 17-11 **21:18** por `git rev-parse HEAD` |
| `origin/main` | `7ad8ccaa72cadd132b2f19c33142c4b3eed85e68` — medido 17-11 **21:18** por `git rev-parse origin/main` **e** `git ls-remote origin main`; `git status -sb` = `## main...origin/main`, **ahead 0** |
| Nota datada (21:18) | `8850edd` (ledger D92/D93) **já está publicado** como ancestral de HEAD (`git merge-base --is-ancestor 8850edd HEAD` → sim). A linha anterior deste cabeçalho dava `4cc08b4` como «HEAD / origin/main» e estava **desactualizada**: esse era o remoto, não o HEAD local. Base de produto: `b80017efdbb6b47bc65a82338c403548e095967f` («Core: estado inicial do Pattern Maker + Tools») + wiring de sanitizers `54addba` (D92) + launcher `3097c83`. Commits documentais do escrivão: `eb9d847`, `1bf65f3`, `e502be9`, `9d87065`, `8850edd` |
| Nota datada (17-11 ~23:05, ronda D98 + FL-1 + ART) | **Medido por mim antes de escrever:** HEAD local = `origin/main` = **`e436ede25661d329bbbd57ae8c48ea657bf76ef3`** (`git rev-parse HEAD` e `git rev-parse origin/main`), `git status -sb` = `## main...origin/main` → **ahead 0**. Working tree: `M docs/relatorios/README.md` + `?? docs/relatorios/2026-11-17-triagem-art.md` (relatório da triagem, **por commitar**; vai no commit desta ronda, que não é meu) + `?? .qoderignore` — bate com a medição do coordenador às 22:36. As três linhas acima («HEAD local», «`origin/main`», «Nota datada (21:18)») passam a **históricas**: davam `7ad8cca` às 21:18 e não conhecem `7211c73` (ledger A-009) nem `e436ede` (FL-1). Nenhum comando git de escrita foi corrido nesta ronda; `ESTADO.md` é o único ficheiro que escrevi |
| Nota datada (18-11 ~15:50, ronda D99–D114 + estado de `main`) | **Medido por mim:** o checkout local estava em `ed4642e` (18/11 03:39:48Z) e `origin/main` em **`f6b86dc`** — **0 à frente / 83 atrás**; `git ls-remote origin refs/heads/main` = `f6b86dc878b78326f1b2fdcfc80e749f36ae96a7`. Desde o último commit que tocou este ledger (`7211c73`, 17/11 22:00:38Z) há **106 commits** em `main`, e o blob de `ESTADO.md` era **idêntico** em `e436ede`/`ed4642e`/`origin/main` (`bfef2c2…`, 61 700 bytes): nada de 18/11 estava registado. **Git desta ronda:** `git merge --ff-only origin/main` (autorizado por **D107**) → HEAD `f6b86dc`, com o `ESTADO.md` por commitar preservado (`134 0`, 97 339 bytes antes desta ronda). O `docs/relatorios/README.md` tinha **2 linhas de outra sessão** por commitar: guardadas em stash etiquetado (`ledger-scribe-18nov-README-outra-sessao`), conflito resolvido por **união exacta** (33 linhas de índice de HEAD + 2 = 35; 0 perdidas de qualquer lado; delta de bytes = exactamente as 2 linhas) e devolvidas ao working tree **fora do commit**. Os 2 relatórios não rastreados dessa sessão e `.qoderignore` ficaram intactos. **O commit desta entrega leva só `ESTADO.md`** |
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
| D92 | 17/11 20:24 | «D92/D93 (ratifico já)» — resposta a «Que número uso no `Autorizado-por`?» | sessão Qoder `a571968b-ec84-4479-9d7e-de29f9340301`, `C:\Users\vk_do\.qoder\projects\D--CORE-CORE\a571968b-ec84-4479-9d7e-de29f9340301.jsonl` **L861**, `tool_result` do AskUserQuestion, ts `2026-11-17T20:24:49.174Z`. Conteúdo proposto em L616 desta sessão (19:30) | build, sanitizers, testes — **âmbito definido pela opção escolhida (proposta da equipa), não por palavras ditadas**: wiring `CORE_SANITIZE` + canários + assertiva de cobertura. A citação verbatim é só a resposta acima | ratificada | ocupa os números que C12 disputava (resolução por referência abaixo); implementação em `54addba` |
| D93 | 17/11 20:24 | «Alinhar primeiro (recomendado)» — resposta a «E os quatro desvios ao teu texto do wiring?»; ratificação no mesmo registo: «D92/D93 (ratifico já)» | mesma fonte: `a571968b…jsonl` **L861**, ts `2026-11-17T20:24:49.174Z` | toolchain, âmbito de execução — **âmbito da opção escolhida, não ditado**: os três passos (tidy + ASan/UBSan + TSan) correm em WSL, zero instalações; no host fica só ASan MSVC como prova de Windows; não se instala LLVM no host | ratificada | alinha os quatro desvios ao texto do wiring antes de commitar; materializado em `54addba` + `4cc08b4` |

### Ratificadas por endosso explícito de texto do executor (17-11 ~21:45)

**Proveniência, sem disfarçar.** O texto das duas entradas **não foi dito pelo dono**: é recomendação do
executor (Qoder) que o dono **confirmou por endosso explícito** às ~21:45, na sessão do coordenador, colada com
os slugs `D-SanLauncher` e `D-JuceAideAscii`. Palavras do dono são o endosso e a ordem de registo, não o corpo.
Por isso ficam nesta subsecção e **não** na tabela acima, que é do bloco confirmado em L335. Pelo precedente de
D77, um slug fora do formato `D###` não é decisão até ganhar número. **Números:** o pedido de ledger propunha
D94/D95; esses dois estão **ocupados** — a linha «D94, D95» da Reserva de números e a fila de 20:24 atribuem-nos
`D-LinuxHost` e `D-TransportSplit`, ainda **pendentes e não ratificadas**. As decisões entram então como os
dois livres seguintes, **D96 e D97**, pelo mesmo critério com que C12 foi resolvida. `grep` por `D96`/`D97`
nesta página antes de escrever: **0 ocorrências**.

| D### | Data | Citação (texto endossado, não ditado) | Fonte | Âmbito/gatilho | Estado | Supersede / invalida |
| --- | --- | --- | --- | --- | --- | --- |
| D96 | 17/11 ~21:45 | «CORE_SANITIZE não é exposto no launcher GUI; sanitizers correm só pelo fluxo de auditoria (canários + gate de scope + verbose). O wiring permanece no CMake. Launcher mantém-se simples (repo/build dir/tipo/app/tests/JUCE); follow-up remove o combo de sanitize.» | sessão do coordenador de 17-11 (~21:45), texto colado pelo dono com o slug `D-SanLauncher`; corpo = recomendação do executor (Qoder); o que o dono dá é o endosso explícito — **não é verbatim ditado** | launcher `ci/compilar.bat` + wiring `CORE_SANITIZE` (`54addba`, D92) — ferramenta de build; nada no caminho de áudio, nenhuma dependência nova | **ratificada por endosso**, com a correcção datada **CO-1** abaixo | não invalida nada; fecha a pergunta «expor `CORE_SANITIZE`?» no sentido **não expor**; **não** abre trabalho de código (CO-1) |
| D97 | 17/11 ~21:45 | «Limitação do juceaide com caminhos não-ASCII aceite e documentada como limitação JUCE; não se patcha o vendored. Launcher e gate validam build dir ASCII (e sem espaços) e recusam com mensagem clara. Report upstream ao JUCE opcional, sem bloquear nada.» | mesma sessão e mesmo estatuto (slug `D-JuceAideAscii`). Delta de versão: na colagem das ~21:45 a última frase era «Report upstream opcional»; no pedido de ledger é «Report upstream ao JUCE opcional, sem bloquear nada» — registada a forma mais completa, o sentido não muda | launcher `ci/compilar.bat` + `ci/gate.ps1`, caminhos de build, JUCE **sem patch** (não está no repo: `FetchContent` de 9.0.5 fixado por versão e hash em `CMakeLists.txt:64-66`, ou cópia local via `FETCHCONTENT_SOURCE_DIR_JUCE`; os únicos vendored em `engine/third_party/` são `json.hpp` e `miniaudio.h`) | **ratificada por endosso**, com a ressalva medida **CO-2** abaixo; **consequência de código por entregar** | fecha a pergunta «caminhos não-ASCII (`juceaide`)»; proibir caminhos com espaços é default que muda comportamento → gatilho D50, satisfeito por esta entrada numerada |

**CO-1 — correcção datada à D96 (medido 17-11 ~21:55).** A cláusula «follow-up remove o combo de sanitize»
**não tem objecto**: o launcher comitado **nunca expôs** sanitizers. `ci/compilar.bat` (401 linhas, `3097c83`) tem
**0** ocorrências de `CORE_SANITIZE`, `sanitize`, `asan`, `ubsan` e `tsan` (grep case-insensitive, corrido nesta
ronda); em todo `ci/`, as ocorrências de `CORE_SANITIZE` também são **0**. O dropdown existia na proposta do
Qwen **recusada** — QW-02, relatório do launcher `:16` — e nunca chegou à árvore. Regista-se a **regra**
(sanitizers fora do launcher; wiring no CMake) e **não se promete remover código que não existe**. Detalhe
medido do «mantém-se simples»: os campos actuais são Repo (`:64`), Build (`:67`), JUCE (`:70`), Tipo (`:75`),
**Jobs** (`:78`), APP (`:84`), TESTES (`:85`) e o checkbox **«Gate também com a app»** (`:86`) — os dois últimos
não vêm no inciso da D96, que lista «repo/build dir/tipo/app/tests/JUCE»; ficam **fora desse inciso sem
que a decisão os condene**, por isso não se apagam sem pergunta.

**CO-2 — ressalva medida à D97 (17-11 ~21:55).** A parte «sem espaços» **não se apoia em falhanço medido**: o
mesmo launcher mediu pasta de build **com espaços** a funcionar — `Configurar` em `D:\Core-build\prova espaco`
→ **exit 0, 24 s** (relatório JU-01 `:23`, Verificação passo 6 `:42`, recapitulada nesta página na linha
«Build com espaços | exit 0, 24 s» da ronda do launcher). O que falha é **não-ASCII**: exit 1, `Running juceaide
failed`, `JUCEUtils.cmake:630`. Fica a decisão **como está** — é a palavra do dono, e a recusa antecipada com
mensagem clara custa menos que um falhanço a meio do configure do JUCE — mas a limitação real do JUCE é só a
dos caracteres não-ASCII, e quem reler esta linha não deve procurar aí um bug que não foi medido.

**O que D96/D97 não fazem.** Nada foi implementado nesta ronda: o pedido era **só** `ESTADO.md`, e `ESTADO.md`
é o único ficheiro que escrevi. A validação de build dir em `ci/compilar.bat` e `ci/gate.ps1` e a documentação
da limitação do `juceaide` ficam como **trabalho consequente por entregar**; o âmbito já está fixado pela D97,
mas nenhuma entrega o executou até ~21:55. Report upstream ao JUCE: opcional, não bloqueia nada.

### Ratificadas por verbatim do dono — sessão do coordenador (17-11 22:02:31 UTC)

**Proveniência reaberta na fonte, não copiada do pedido.** Antes de numerar, descompactei o log citado e li o
registo dele: **160 frames zstd / 338 registos** (o 1.º frame contém só o cabeçalho da sessão, por isso um
`zstdDecompressSync` do ficheiro inteiro devolve 188 bytes e nada mais — quem reabrir isto precisa de percorrer
os frames). O registo existe, é do dono, e a grafia é a que o pedido dava.

| D### | Data | Citação verbatim | Fonte | Âmbito/gatilho | Estado | Supersede / invalida |
| --- | --- | --- | --- | --- | --- | --- |
| D98 | 17/11/2026 **22:02:31 UTC** (o registo traz `2026-11-17T22:02:31.431Z`, `clientTimeZone = Europe/Lisbon`, que em Novembro é UTC+0 — local e UTC coincidem) | «eu quero que faças as decisoes percebes mais que eu disto, so quero a app funcionar bem» — **grafia conservada como dita**: sem acentos em «decisoes» e «so», sem vírgulas | sessão DSH do coordenador `session-2ff54f03-8344-40bb-8cb4-54df0601eb69` (`cwd` `D:\CORE`), registo `user/message` **seq 90**, `id` `d721c86a-e0ef-444b-99ea-fe28b58020b2`, `source.kind = user`; log `C:\Users\vk_do\.dsh\sessions\--D-CORE--\session-2ff54f03-8344-40bb-8cb4-54df0601eb69\session.v4.jsonl.zstd`. **Contexto verificado no mesmo log:** resposta ao `assistant/message` **seq 83** (22:01:33.385Z), cuja última frase literal é «Qual das opções queres, e onde está o lote (ficheiro, ou sessão Qoder e `.jsonl`)?» com as opções A/B/C para correr a triagem dos 16 achados do lote A-real-time — o pedido de ledger abreviava essa frase; tinha sido precedida pelo pedido de decisão «Próxima decisão: a triagem dos 16 corre agora com Fable?» | governação — **delegação de decisões técnicas no coordenador** | **ratificada** (palavras do dono, verificadas na fonte) | não revoga nem nomeia D48/D50/D51/D76/D83/D86/D91; **não invalida nada** |

**O que as palavras dizem e o que não dizem.** Dizem: o dono quer que o coordenador decida, porque confia na
competência técnica dele («percebes mais que eu disto»), e fixam o objectivo — «so quero a app funcionar bem».
**Não dizem** que categorias de escolha ficam delegadas, nem que valor de ledger tem uma decisão escrita pelo
coordenador, nem como se forma uma entrada numerada. Isso é leitura do coordenador; fica à parte, e é o ponto em
colisão. **Número atribuído: D98** — ver a Reserva; D94/D95 continuam ocupados pelas propostas `D-LinuxHost` e
`D-TransportSplit` (não ratificadas), D96/D97 pelas duas do launcher, e `grep` por `D98`/`D99` dava **0**
ocorrências antes de escrever.

**LEITURA DO COORDENADOR — não palavras do dono, e sem efeito de ledger.** Transcrita do pedido de ledger desta
ronda, porque o próprio pedido a pede separada:

- **Cobre**, segundo o coordenador: escolhas técnicas — que achados corrigir e por que ordem; a abordagem mínima de
  cada correcção, incluindo correcções no caminho de áudio, sempre com teste vermelho→verde; que agente/modelo corre
  o quê; aceitar ou recusar entregas.
- **Regra proposta:** cada uma dessas escolhas entra no ledger como decisão numerada «origem: coordenador, por
  delegação D98», **antes** do código.
- **Não cobre**, e continua a exigir palavras do dono: ligar o M5 (D51); âmbito de produto, funcionalidades e novas
  superfícies de UI; mover código Core ↔ AICORE; novas dependências, processos ou persistência; o que apague ou
  altere projectos e áudio do utilizador. Esta lista de exclusões **não colide** — é repetição fiel de D48/D50/D51/D83/D86/D91.

**Colisão com D48/D50/D76 — não resolvo, abro pendência.** A regra proposta choca com texto ratificado: D48 «Só
palavras explícitas tuas criam decisões.»; D50 «Registem como decisões numeradas com `origem: Diogo`, citando esta
mensagem.» e «Arquitectura só com aprovação explícita **antes** do código.» e «Silêncio ≠ aprovação.»; D76 «D58–D66
do coordenador = propostas pendentes, sem efeito até eu ratificar.» — exactamente o tipo de entrada que a leitura
quer numerar e aterrar antes do código. O precedente de hoje vai no mesmo sentido: D92/D93 (20:24) e D96/D97
(~21:45) só viraram decisões com palavra do dono (ratificação ou endosso explícito), e o texto era de terceiro nos
dois casos. Registo portanto **só a citação** como decisão (D98) e a leitura como leitura; a questão está em
**P-D98** em Pendentes, com três opções e sem escolha minha.

**Nada nesta ronda dependeu desta leitura:** o FL-1 entrou autorizado pela **D51 já ratificada** (não por delegação
nenhuma) e a triagem ART foi **só leitura**, sem código nem execução. O que a leitura alteraria, se tiver efeito, são
as correcções do lote ART que tocam `graph.cpp`/`engine.cpp`, **segundo a última coluna da tabela §2 do relatório**:
ART-002, ART-005 (só se medir mal), ART-007, ART-008, ART-010(b). Nessa coluna ART-003, ART-004, ART-011, ART-012,
ART-013, ART-015, ART-016, ART-017 e ART-018 estão a «não»; ART-006 e ART-014 a «—» (refutadas). A lista que aqui
estava (ART-003 e ART-018 incluídos, ART-008 e ART-010(b) omitidos) era minha e não era a do relatório — corrigida.

### Ratificadas por verbatim do dono — 18/11 (duas sessões, dois canais de entrada)

**Proveniência, verificada por mim antes de numerar.** As palavras do dono de 18/11 estão em **duas** sessões:
(a) DSH `session-2ff54f03-8344-40bb-8cb4-54df0601eb69` — log reaberto por frames (**957 frames zstd / 1988 registos**;
um `zstdDecompressSync` do ficheiro inteiro devolve só o 1.º frame), 34 mensagens `user/message` com
`data.source.kind = user` em 18/11; (b) Qoder `613ad19e-456c-4336-8bc7-6496bb5c7a2b`
(`C:\Users\vk_do\.qoder\projects\D--CORE-CORE\613ad19e-456c-4336-8bc7-6496bb5c7a2b.jsonl`, 2804 linhas) — **82
mensagens do dono em dois canais**: `type=user` com `humanInput` (47) **e** `type=attachment` com
`attachment.prompt[]` / `origin.kind = bridge` (`queued_command`, 35). Ler só o primeiro canal perde metade das
directivas: foi o que aconteceu aos relatórios dos ramos UX.

**Delta de atribuição encontrado (medido):** o relatório `2026-11-18-ux-piano-roll.md` (ramo `flawless-ux-piano`)
cita «metam as janelas assim [Pulse], implementem os conceitos que tem, deixem o ui como esta no core. usem
dropdowns em vez de muitos botoes» como verbatim do dono na **sessão DSH `session-2ff54f03`** (o id aparece 2×);
o `2026-11-18-ux-detachable-windows.md` (ramo `flawless-ux-windows`) cita a mesma fusão, com «para escolhar» no
fim, atribuindo-a a «coordinator dispatch» sem id. Medido por mim: essa cadeia fundida tem **0 ocorrências nos
15 logs DSH** e **0 nos 47 `humanInput`** da sessão Qoder. As palavras existem, mas são **duas mensagens
separadas** no canal `queued_command` da sessão **Qoder `613ad19e`**, L2332/L2334, **12:53:48.632Z / .740Z**, e a
grafia real é «https://github.com/nalexand/pulse metam as janelas assim, implementem os conceitos que tem,
deixem o ui como esta no core» + «usem dropdowns em vez de muitos botoes para escolhar». A substância está
autorizada; **a fonte citada no relatório do piano está errada** e a citação é uma fusão de duas mensagens.
Registo abaixo a grafia e a fonte medidas.

| D### | Data (UTC) | Citação verbatim | Fonte | Âmbito/gatilho | Estado | Supersede / invalida |
| --- | --- | --- | --- | --- | --- | --- |
| D99 | 18/11 **01:40:41.322Z** | «Quero que melhores a app, motor, perceber conceito corrigir todos os erros bugs melhorar performance. Quero este daw funcionar flawless. remove codigo morto, funcionamento que ja nao para ser utilizado. Ideia é ter performance top notch e conseguir lidar com varios plugins sejam vst3 ou vst2» | DSH `session-2ff54f03`, `user/message` **seq 879**, `id f4b81495-97ff-44d7-8846-7b9c890aa7db` | mandato da ronda «flawless»: corrigir bugs, performance, muitos plugins VST3/VST2, remover código morto | **ratificada** | não revoga D54 (a ordem data > tempo real > performance > compatibilidade mantém-se); dá âmbito à ronda de 18/11 |
| D100 | 18/11 **02:28:14.711Z** e **02:30:23.853Z** | «eu quero melhor motor que possamos fazer» · «https://github.com/alexisvassquez/audiomix deixa isso como criterio a chegar» | DSH `session-2ff54f03`, seq **1371** (`id b7425641-6c36-4695-ace3-dd286616e0ea`) e seq **1395** (`id 6ddee51c-81e4-4fab-a73c-2009c09fe99d`) | os cinco critérios de qualidade do motor são **alvos a chegar**, não estado actual; cada um só conta como atingido com medição | **ratificada** | escrita em `AGENTS.md` «Engine quality bar» no commit `db20d67`; não invalida nada |
| D101 | 18/11 **02:23:28.146Z** | «sempre tudo em ingles, meter como rule nos fecheiros necessarios» | DSH `session-2ff54f03`, seq **1330**, `id c526cf49-057f-47e4-83d5-4387e292cef2` | tudo o que se escreve no repo em inglês (código, testes, logs, mensagens de commit, documentação nova) | **ratificada** | escrita em `AGENTS.md` no commit `5cb1ec5`. **Excepções que a própria regra fixa:** verbatim do dono, e o ledger/histórico (`ESTADO.md`, `docs/relatorios/`, `docs/historico/`) não se traduzem — por isso esta página continua em PT-PT |
| D102 | 18/11 **01:42:36.614Z → 12:53:48.740Z** (várias mensagens) | «https://github.com/nalexand/pulse quero adoptar muito deste ui, como rato funciona no piano, a questao de as janelas estarem todas separadas poder mexer, scales do piano, generate do piano. como rato trabalho no piano» · «quero praticamernte esse ui mas com menus botoes e cor do core,» · «channel rack expansibel» · «manter as cores e design do core mas implementar esse ux» · «nao o core tem look» · «tem corees e menus que sao para ficar» · «apenas quero implementar o que ja tem no core para ser igual ao exemplo» · «meter janelas a sair, do limited da pahoma principal» · «quero que esteja refinidado como este programa,mixer,  fill pattern» · «aqui quero que lado direito do rto continue com as mesmas funçoes do core» · «façam assim , metam o core como phaser, e depois eu arranjo a minha maneira.» · «pulse, como o pulse» · «https://github.com/nalexand/pulse metam as janelas assim, implementem os conceitos que tem, deixem o ui como esta no core» · «usem dropdowns em vez de muitos botoes para escolhar» · «a partir dai depois eu refino» | DSH seq **926** (`id b03caf12`), **970** (`98cc3d64`), **987** (`201a11a1`), **1014** (`4f4c8b9e`); Qoder `613ad19e` **L386** 04:30:19.778Z, **L396/L398** 04:30:52.577Z/.685Z, **L475/L477** 04:35:43.859Z/.963Z, **L490** 04:36:53.233Z, **L2314** 12:51:18.925Z, **L2322** 12:52:21Z, **L2332/L2334/L2336** 12:53:48.632Z/.740Z/… (canal `queued_command` salvo L2314, que é `humanInput`) | **UX = conceitos do Pulse, look/menus/cores do Core**: janelas destacáveis, dropdowns em vez de muitos botões, channel rack expansível, mixer e fill pattern refinados, lado direito mantém as funções do Core. Referência AGPL: **comportamentos/conceitos, nunca código transcrito**. O refinamento final é do dono | **ratificada** | **supersede por referência** o item 11 de `docs/plan/README.md` («The owner has not approved the design yet: no UI code before he does») — passou a haver palavras explícitas. Não altera D83 (o que está fora da árvore só re-entra por decisão numerada: esta é a decisão para o UX Pulse) |
| D103 | 18/11 **01:48:42.729Z → 04:53:00.497Z** | «piano que da para ver tudo» · «apenas muda no piano» · «piano deve conseguir expandir» · «no piano ctrl+scroll keys+ e keys» · «quero uma pequena janela com isto , ou sej um botao no piano que me da estas opçoes, incluindo as do baixo kbbb que ja existem» | DSH seq **999** (`id f5b9eb89`); Qoder `613ad19e` **L492** 04:36:53.341Z, **L532** 04:40:28.607Z (bridge), **L563** 04:41:52.626Z, **L662** 04:53:00.497Z (`humanInput`, com captura anexa) | âmbito do piano: ver tudo/expandir, `ctrl+scroll` para keys+ e keys−, botão que abre uma janela pequena de note tools incluindo as opções do baixo kbbb que já existem. **«apenas muda no piano»** limita o alcance desta fatia | **ratificada** | concretiza D102 para o piano; não autoriza mexer no resto do UI por esta via |
| D104 | 18/11 **06:09:20.650Z**, **12:55:31.718Z/.821Z**, **13:16:12.605Z** | «core doesnt hhave any instruments, effects,oscilator or filters» · «atençao que o core nao tem instrumentos, certos menus ficam preenchidos depois de existirem plugins» · «core nao tem sampler tambem.» · «nao btemos instrumentos, just in case tivesse informaçao boa» | Qoder `613ad19e` **L1295**, **L2350** (com captura), **L2352** (bridge), **L2504** (`humanInput`) | fronteira reafirmada: o Core **não tem** instrumentos, efeitos, osciladores, filtros nem sampler próprios; menus que dependem de plugins ficam preenchidos quando existirem plugins. As referências de DSP são conhecimento, não licença para criar instrumentos | **ratificada** | confirma D86/D91 e a remoção de 16/11; **não** reabre o silêncio dos builtins como defeito do motor (ver Estado medido) |
| D105 | 18/11 **12:45:52.745Z → 12:48:44.501Z** (e 01:18–01:19 na DSH) | «limpa tudo» · «remove templates com efeitos» · «se existirem templates com efeitos antigos remove» · «o que esta a causar erros limpa, w-core, d-core f-core, tudo isso ja nao entra» · «apenas o pattern maker,mixer, piano, tools.» | Qoder `613ad19e` **L2268** (`humanInput`), **L2275**, **L2289**, **L2287**, **L2285** (bridge). Contexto anterior na DSH: seq **821** «remover tudo» (01:18:49.589Z), **840** «instrumentos templates feiitos» (01:19:04.566Z), **861** «codigo morto» (01:19:13.172Z) — respostas curtas ao resumo de pendentes do seq 814, que listava «templates antigos»; **o âmbito operativo é o das mensagens explícitas de 12:45–12:48** | limpeza: remover templates com efeitos (incl. antigos), o que causa erros e os resíduos w-core/d-core/f-core; superfícies que ficam: pattern maker, mixer, piano, tools | **ratificada** | dá âmbito ao L10 (`flawless-clean`, por integrar) e ao CLEAN-07 já em main. **Preserva** catálogo de padrões, alinhamento kick/baixo/pratos, projectos e áudio do utilizador (regra de `AGENTS.md`); «limpa tudo» não autoriza apagar dados do dono |
| D106 | 18/11 **12:50:22.981Z** | «se tivermos de alterar os testes alteramos e removemos pq nao estao a provar nada, se nao existe» | Qoder `613ad19e` **L2302** (bridge) | testes de funcionalidades **removidas** podem ser alterados ou removidos: não provam nada se o sujeito não existe | **ratificada** | **não** relaxa a regra de `AGENTS.md` para testes de comportamento vivo (não reduzir cobertura nem relaxar critérios para obter verde); aplica-se ao que deixou de existir |
| D107 | 18/11 **07:09:13.544Z**, **07:20:28.459Z**, **07:21:47.378Z** | «consolida e envia» · «ok voces tem de ir fazendo pushes nem que sejam branches» · «sim» | Qoder `613ad19e` **L1741**, **L1826**, **L1841** (`humanInput`). O «sim» de L1841 responde a **L1834** (07:21:24.614Z), que propunha pushes por branch e o merge para `main` quando o tip integrado estiver provado («e tu autorizares o ff, ou faço-o eu se disseres») | consolidação dos ramos de fixes + **pushes incrementais**, mesmo por branch; ff para `main` autorizado quando o tip integrado estiver verde | **ratificada** | é a autorização da ronda de consolidação que produziu `fcf38e0` → `main f6b86dc`. Não delega decisões de produto (isso continua D48/D50; ver P-D98) |
| D108 | 18/11 **04:05:59.075Z** e **10:46:03.300Z** | «A ordem para o Sonus fica: BUG-SWEEP-01, BUG-SWEEP-04, depois o "Try again" (05), a mensagem da exportação (03) e o 06. Juntos são um lote pequeno.» · «fixes motor primeiro, depois melhoramenros sur» | Qoder `613ad19e` **L187** e **L2019** (`humanInput`) | ordem de trabalho: primeiro os fixes do motor, depois as melhorias SUR; dentro do lote Sonus, 01 → 04 → 05 → 03 → 06 | **ratificada** | concretiza D54/D99 para esta ronda; não muda a ordem de prioridades do dono (data > tempo real > performance > compatibilidade) |
| D109 | 18/11 **01:49:58.461Z** e **01:50:40.838Z**; referência 04:27:17.567Z | «é para o futuro ou em casos especificos» (cauda da mensagem dele, colada a texto do coordenador sobre VST2) · «quero implementar vst2 tambem» · «https://github.com/Xaymar/vst2sdk» | DSH seq **1030** (`id 71de23ea`) e seq **1045** (`id f347d9f7`); Qoder `613ad19e` **L345** (bridge) | VST2 entra no âmbito desejado, para o futuro/casos específicos; `Xaymar/vst2sdk` é referência de ABI | **ratificada** | não revoga nada. **Sequência e forma (depois dos itens 1–4, atrás de flag de build, cabeçalho de compatibilidade) são leitura do coordenador no plano, não palavras do dono** — ficam como proposta enquanto ele não disser |
| D110 | 18/11 **12:58:21.699Z** | «10: sampler = host de samples do utilizador, sem fontes de som próprias.» · «11: randomize sobre parâmetros dos plugins dos papéis (undoável); escondido quando não há plugins.» · «21: split em clip congelado = recusa com mensagem + oferta de unfreeze (um Undo); nunca meio-congelado.» · «30: escala de envelope consistente em ALL base-writers via helper partilhado (ratifica a política para o follow-up avançar).» · «26: preservar swing ao guardar; unswing só em opção explícita de export.» · «28: manter semântica parte-primeiro (patterns pertencem à parte).» · «31: wording "Restore autosave / Discard"; Discard apaga só o autosave, nunca o projecto; .bak usado quando o primário está corrupto.» (**7 linhas na origem; aqui separadas por ·**) | Qoder `613ad19e` **L2379** (`humanInput`) | políticas de produto ratificadas, pelos números que o dono usou (10/11/21/30/26/28/31) — mapeiam para PROD-01…PROD-10 e para os lotes 26/28/30/31 | **ratificada** | ratifica a política do 30 «para o follow-up avançar»; fecha as dúvidas 26/28 que o relatório `pattern-26-28` deixou abertas. **Não renumera** decisões (D78): os números 10/11/… são os do dono, não `D###` |
| D111 | 18/11 **13:04:26.323Z** | «humanize/vary/velocity-ramp/follow/pattern_rack) + 26-adjust (preservar swing no save + unswing export explícito) isto vai ser tudo usado no channel rack. basicamente o pcore» | Qoder `613ad19e` **L2401** (`humanInput`, com captura anexa) | o channel rack é o **P-Core**: humanize/vary/velocity-ramp/follow/pattern_rack + 26-adjust vivem aí | **ratificada** | concretiza D102 («channel rack expansibel») e D110(26); âmbito de produto, não autorização de UI nova por si só |
| D112 | 18/11 **13:32:04.454Z** (e referências 13:06–13:29) | «tentar usar isso para nosso beneficio , vou trabalhar continuem» | Qoder `613ad19e` **L2579** (`humanInput`); referências coladas por ele: **L2420** pulse/src, **L2448** pulse/tests, **L2463/L2470** OpenDaw (+src/engine), **L2487** klang, **L2537** iPlug2, **L2544** dspguide, **L2546** thinkdsp.pdf, **L2548** PortAudio, **L2514** Roche slides, **L2563** 28247361.pdf; na DSH: seq **1233/1248/1271** (melatonin: testes DSP, dicas JUCE, `melatonin_perfetto`), **1353** libsonare | as referências externas são para **aproveitar conceitos em benefício do Core**, não para transcrever código (copyleft fora) nem para criar instrumentos próprios (D104) | **ratificada** | não altera a fronteira; `melatonin_perfetto` fica como opção de build desligada por omissão (proposta do plano, não decisão) |
| D113 | 18/11 **06:33:30.196Z** | «sim» — resposta a **L1484** (06:30:22.870Z): «**Continua a precisar de ti:** item 1-D — autorização para o patch vendored no JUCE pinned (deadlock do `restartComponent`). Tudo o resto avança sem bloqueio.» | Qoder `613ad19e` **L1491** (`humanInput`), pergunta em **L1484** | autorização explícita para **patch vendored no JUCE pinned** (item 1-D: deadlock do `restartComponent` no thread de mensagens) | **ratificada** | é a autorização de uma alteração a dependência (D48/D50 exigiam palavra do dono). Implementada em `cc5b874` + `ci/patches/juce-failed-plugin-restart.patch`. **Pendência de infraestrutura:** o patch vive como modificação da árvore do JUCE, nunca commitada lá |
| D114 | 18/11 **05:37:42.694Z** | «ok» — resposta a **L1000** (05:36:10.200Z), que propunha «no `ci/gate.ps1` (linha 37: `core-gate-<pid>.bat` em vez do path fixo) — fix CI permanente que elimina esta classe de colisão para os dois streams, mesmo sem lock. Implemento assim que disseres.» | Qoder `613ad19e` **L1007** (`humanInput`), proposta em **L1000** | fix do gate: bat único por invocação, para gates concorrentes não se corromperem | **ratificada** | implementado em `fffbb41` (e cherry-picks). Não é activação de CI nem tokens novos (proibidos por `AGENTS.md`); é o script local de gate |
| D115 | 20/11 ~00:20 | «Core — Passagem para o coordenador dos Qwens»: «Quero que organizes o trabalho para tornar o Core fiável em projetos reais. A ordem que deves usar para decidir prioridades é **dados > tempo real > performance > compatibilidade**.» + contratos explícitos S11/S1/S5/S4/S2/S3/S6/S7 (cada um com prova vermelho→verde) + «Faz o commit apenas se já estiver autorizado» (etapa 0) + «Mantém esta investigação [Debug 259] numa tarefa própria» | DSH sessão lead `a69b320fbfab`, dispatch do dono colado 2026-11-20 | mandato da ronda S: prioridades D54 reafirmadas; âmbito explícito por item (S11 JUCE reproduzível, S1 exportação incompleta, S5 samples do utilizador, S4 WAV float, S2/S3 estado de plugins, S6/S7 motor RT); ENV-03 tarefa própria | **ratificada** | governa a ronda S 2026-11-20 (entregue abaixo). Q1–Q8 do plano original `1c6238b` não consultáveis (clone não localizável local nem remote — busca exaustiva arquivada); os contratos explícitos do dispatch prevalecem como instrução atual (AGENTS.md) |
| D116 | 20/11 **12:52** | «acaba o que tem feito para fazer commit» + «update ao estado o que foi feito, testes criados a sua finalidade, proximo agente a pegar no projecto tem de saber tudo» | DSH sessão lead `a69b320fbfab`, mensagens do dono 2026-11-20 12:52 e ~13:00 | autorização para **commitar o trabalho entregue** da ronda S e para **o coordenador escrever esta atualização do ESTADO** (instrução direta do dono que dispensa o escrivão-only D72/D80 nesta entrega) | **ratificada** | commits `0d7d8ce` (build), `587bf5b` (dados), `234f956` (motor), `aacc6d1` (docs) + este commit do ledger; re-gate version==sha após commit. Trabalho de outras sessões preservado por commitar (2 linhas README 17/11 + relatórios; `.qoderignore`/`environment.yml` pré-existentes) |

**Números atribuídos nesta ronda: D99–D114** (16). Método: maior `D` vivo = D98 (esta página, por commitar até
esta entrega); D94/D95 continuam ocupados pelas propostas não ratificadas; `grep` por `D99`…`D114` dava **0**
ocorrências antes de escrever. **Nenhuma destas decisões depende de P-D98**: são palavras do dono, não escolhas
do coordenador por delegação.

**O que fica de fora desta numeração, por não ser decisão:** «too many subagents» (DSH seq 1515, 02:34:10.516Z),
«temos 50€ make it worth» (seq 1552, 02:35:28.508Z), «ainda tens 32€» (seq 1704, 03:07:12.251Z), «12€» (seq 1758,
03:38:08.224Z), «mete mais agents a trabalhar, uns a fazer comiit outros preencher o estado ctests» (Qoder L766,
05:01:31.262Z), «quero uma equipa a correr, varios bug serem corrigidos» (L823, 05:07:48.762Z) — são instruções
operacionais/de orçamento, registadas aqui como contexto, não como decisões de produto. Os «ok»/«sim» curtos sem
pergunta identificada (L136 «ok manda», L1007 já usado acima, L1491 já usado) **não se numeram** sem o contexto
verificado: silêncio ≠ aprovação, e um «sim» sem pergunta não é âmbito (D48/D50).

### Reserva de números (D74)

| Intervalo | Estatuto |
| --- | --- |
| D58–D66 | folha do coordenador — **propostas pendentes**, sem efeito (D76) |
| D67–D71 | reservados, sem conteúdo atribuído; não usar como decisão |
| D72, D73 | mantidas como ditadas pelo dono |
| D74+ | sequência de ratificações, em uso desde esta entrega |
| D92, D93 | **ratificadas** em 17-11 20:24 (sanitizers: wiring e toolchain) |
| D94, D95 | **pendentes**, não ratificadas: `D-LinuxHost` e `D-TransportSplit` (coladas às 20:13) |
| D96, D97 | **ratificadas** em 17-11 ~21:45 **por endosso explícito de texto do executor** (launcher: `CORE_SANITIZE` fora da GUI; limitação não-ASCII do `juceaide` aceite e documentada). O pedido de ledger chamava-lhes D94/D95; não se usaram porque esses números estão ocupados pela linha de cima — ver a subsecção «Ratificadas por endosso» |
| D98 | **ratificada** 17-11 **22:02:31 UTC** — delegação do dono ao coordenador, verificada por mim no log da própria sessão DSH (`session-2ff54f03-8344-40bb-8cb4-54df0601eb69`, `user/message` seq 90). Atribuído nesta ronda como o livre seguinte a D96/D97. **A numeração não decide a questão do alcance:** a regra que o coordenador quer derivar dela fica em **P-D98**, pendente, porque colide com D48/D50/D76 |
| D99–D114 | **ratificadas** em 18/11 por verbatim do dono verificado na fonte em duas sessões (DSH `session-2ff54f03` e Qoder `613ad19e`, canais `humanInput` **e** `queued_command`): mandato flawless, critérios do motor, inglês, UX Pulse, piano, fronteira sem instrumentos/sampler, limpeza, testes de funcionalidades removidas, consolidação/pushes, ordem motor→SUR, VST2, políticas 10/11/21/30/26/28/31, channel rack = P-Core, referências, patch vendored JUCE, bat único do gate. **Livres antes de escrever:** `grep D99…D114` no `ESTADO.md` de HEAD = **0**. Os `D140`/`D141`/`D142` que aparecem no texto são o arquivo `M-D##` do predecessor B e a hipótese de renumeração **recusada** na colisão C1 — não são números vivos |

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
| **Launcher: expor `CORE_SANITIZE`?** | [relatório do launcher](docs/relatorios/2026-11-17-compilar-gui.md) §Pendências, `:58` | Pergunta ao dono, **não decisão**. Hoje o launcher não expõe sanitizers de propósito (pertencem ao fluxo de auditoria, não ao de compilar). **Premissa corrigida:** o texto da pergunta diz «o wiring existe apenas por commitar» — já não está por commitar, está em `54addba` (D92). Se quiseres o dropdown, passa a ser um campo a mais, com os `FATAL_ERROR` do wiring a protegerem os falsos verdes. **Não decidi nenhuma** |
| **Caminhos de build não-ASCII (`juceaide`)** | mesmo relatório `:59`, resultado JU-01 | Limitação **medida do JUCE** (`Running juceaide failed`, `JUCEUtils.cmake:630`), não do launcher: aceitar e documentar, ou investigar um workaround no JUCE (fora do âmbito daquele pedido)? **Não decidi nenhuma** |
| **Null-check defensivo em `graph.cpp:867` e `:870`** | [relatório A-009](docs/relatorios/2026-11-17-a009-instrumento-retirado.md) §Pendências `:123-126`, código conferido por mim em `d667527` | **Proposta da equipa, sem decisão nenhuma.** Medido: `liveNote` (`:867`, `t.instrument->noteOn`/`noteOff`) e `allNotesOff` (`:870`, `t.instrument->allNotesOff()`) desreferenciam **sem guarda**, e `makeInstrument` devolve `nullptr` para um nome fora do registo (`engine/src/dsp/instruments.cpp:35`). Hoje a única defesa real é a validação em `project.cpp:524-529`. **Delta ao relatório, medido agora:** o relatório diz que `server.cpp:202` é «o único sítio de construção» do `Graph` — não é exacto; há pelo menos **quatro** (`server.cpp:113` export, `server.cpp:202` `rebuild`, `render_job.cpp:290`, `measure.cpp:221`). Se todos os `Project` dessas vias vêm de `fromJson` validado é o que falta provar, e é precisamente o risco residual que a proposta quer tapar: uma via que contorne `trackFromJson` reintroduz o crash. Recomendado pela entrega: **assert em debug + guarda em release com a faixa marcada falhada**, sem comportamento novo audível. Mexer em `graph.cpp` é **caminho de áudio** → gatilho D48/D50; **não se avança sem palavras tuas** |
| **Ligar o M5 (continua OFF)** | [relatório FL-1](docs/relatorios/2026-11-17-fl1-m5-default.md) §Pendências `:119-122`; D51 nesta página `:39` (A:52) | **Não é decisão nova — é a D51 em vigor.** O default está agora **0 (serial)**, que é o que a D51 manda; para passar a **ON** falta a medição que ela pede: **soak 8 h + bit-a-bit vs serial** no PC do dono com `CORE_LIVE_THREADS=4` — **não verificado / não feito** em nenhuma entrega até agora (a correcção FL-1 correu backend **null**, headless; não é prova de WASAPI real). Duas notas que evitam releituras: `engine.hpp:74` mantém o texto histórico «0 = serial, the default until measured» (fora do âmbito da correcção, não tocado), e o default do próprio `Engine` **já era 0** (`engine.hpp:76`, `explicit Engine(double sampleRate, int liveThreads = 0)`) — a divergência vivia só no `Server`. **Adiada por D51, sem prazo; e a D98 não muda isto: ligar o M5 está explicitamente fora da delegação** |

### Pendências do launcher — **fechadas por referência** (D96/D97, 17-11 ~21:45)

As duas linhas da tabela «Do repo actual» («Launcher: expor `CORE_SANITIZE`?» e «Caminhos de build não-ASCII
(`juceaide`)») deixaram de estar em aberto. Ficam escritas como foram, sem edição nem apagamento (D72); a
resolução é por referência.

| Pergunta (como estava escrita) | Resolução | O que fica a fazer |
| --- | --- | --- |
| «Launcher: expor `CORE_SANITIZE`?» | **D96** — não exposto; sanitizers só no fluxo de auditoria (canários + gate de scope + verbose); o wiring permanece no CMake. Ver **CO-1**: não existe combo para remover, portanto a regra não gera commit | nada de código; é regra de desenho do launcher |
| «Caminhos de build não-ASCII (`juceaide`)» | **D97** — limitação aceite e documentada **como limitação JUCE**; não se patcha o JUCE (fetched/local, fora do repo); launcher e gate passam a validar build dir ASCII e sem espaços, com recusa clara. Ver **CO-2**: a parte «sem espaços» não tem falhanço medido por trás | implementar a validação nos dois scripts + documentar a limitação em relatório próprio; report upstream ao JUCE **opcional**, não bloqueia |

Continuam **sem resposta** nas duas decisões, e portanto por exercitar no launcher: tipo **Debug**, os diálogos
«…» (`FolderBrowserDialog`), a guarda de gerador diferente de Ninja (relatório do launcher `:23` e `:58-59`).
Isto não reabre as duas perguntas fechadas acima; é a lista que a própria entrega declarou em aberto.

### P-D98 — Delegação D98: o alcance está em colisão com D48/D50/D76 (aberta 17-11 ~23:05)

O dono delegou as decisões técnicas (**D98**, verificada na fonte) mas **não disse** que as escolhas do coordenador
entram no ledger como decisões numeradas com `origem: coordenador`. Não resolvo — três opções, nenhuma escolhida por mim:

| Opção | Efeito no ledger |
| --- | --- |
| **(a)** | A D98 autoriza: cada escolha técnica do coordenador entra como decisão numerada «origem: coordenador, por delegação D98», **antes** do código; fica um *carve-out* documentado de D48/D50/D76 para esse tipo de entrada |
| **(b)** | A D98 é confiança para promover e decidir no prompt, **sem efeito de ledger**: cada escolha continua proposta pendente (D76/D77) até palavra do dono — verbatim (precedente D92/D93, 20:24) ou endosso explícito de texto de terceiro (precedente D96/D97, ~21:45) |
| **(c)** | Meio-termo: entram numeradas com `origem: coordenador, por delegação D98` **só** a ordem de ataque, a ferramenta, o agente/modelo de cada entrega e aceitar/recusar entregas; caminho de áudio, defaults, produto e arquitectura continuam a exigir palavra do dono **por item** (D48/D50) |

Consequência prática de (a), para ponderares antes de escolher: D48 fixa «Todo o PR leva `Autorizado-por: D###`».
Com (a) esse `D###` pode ser uma decisão sem palavras tuas — que é exactamente o que D48/D50 hoje excluem. Enquanto
não responderes: nada de código avança por delegação. As correcções que **tocam `graph.cpp`/`engine.cpp`** — última
coluna da tabela §2 do relatório: ART-002, ART-005 (só se medir mal), ART-007, ART-008, ART-010(b) — ficam
**pendentes** na tabela abaixo; as restantes também não avançam sem autorização (D48/D50/D76), só não caem nessa
coluna. A lista que aqui estava (com ART-003 e ART-018, sem ART-008 nem ART-010(b)) era minha, não do relatório.

### Triagem ART (lote A-real-time) — pendências abertas pela triagem de 17-11 22:17

Abertas por **leitura de um subagente, um único verificador, nada executado** ([relatório](docs/relatorios/2026-11-17-triagem-art.md),
**por commitar** à hora desta ronda). Nenhum item tem decisão nem implementação. A última coluna é a do relatório
(§2, «correcção toca graph.cpp/engine.cpp?»), **transcrita** — não é avaliação minha. «Não» nessa coluna **não é
autorização** para escrever código: o gatilho continua a ser D48/D50 (default, comportamento, arquitectura) e D76.
«Medição primeiro» não é decisão, é o que falta para a decisão fazer sentido. FL-2 e FL-3 estão no §5 do relatório,
fora da tabela §2, e por isso a coluna deles diz «fora do §2».

| Pendente | O que está em aberto (relatório + o que conferi eu) | Toca graph.cpp/engine.cpp? (última coluna do §2) |
| --- | --- | --- |
| **ART-003** — notificação de dispositivo | **Em `7211c73`/`e436ede`**, conferido por mim: `device.cpp` não define `notificationCallback` nem consulta estado — 0 ocorrências no ficheiro, `dataCallback` `:131`, `pUserData` `:132`, `setDeviceBuffer` só em `:162`. **A árvore de trabalho moveu-se durante esta correcção, por outra mão e sem commitar** (HEAD continua `e436ede`): `device.cpp` 183 → 208 linhas, `notificationCallback` 0 → 1 ocorrência, getter `deviceNotification()` presente; `server.cpp` +4/-1, `server.hpp` +5, `EngineSession.cpp` +27/-1, `EngineSession.h` +2, `tests/engine_tests.cpp` +32 (`git diff --numstat`). **Resolvido por referência (18/11):** o ART-003 foi commitado em `e7968c8` («Motor/App: a saida de audio que morre e reportada (ART-003)», `Autorizado-por: D98`) e está em `main`; gate reportado **15/15, 116 casos / 1469 verificações / 0 falhas**, vermelho 4 → verde (5 verif.), e o relatório `2026-11-17-art003-dispositivo.md` entrou nesse commit. Runtime WASAPI e reacção da app medidos depois pela sessão de teste real (10 min 54 s, 0 overruns, `deviceNotification` 0, **sem alarme falso**). As citações de `device.cpp`/`EngineSession.cpp` desta tabela valem para `7211c73`/`e436ede`; em `main` o `device.cpp` já tem `notificationCallback` e as linhas deslocaram-se. A correcção que o relatório propõe (§3 `:114`) é `notificationCallback` → flags atómicas → evento de estado → a app reabre, com teste manual (desligar USB; mudar o dispositivo por omissão para 44,1 kHz). **Nota minha, fora do relatório:** o `Engine` **já tem** getter de ritmo (`engine.hpp:146` `sampleRate()`) e de workers (`:148` `liveThreads()`) — o que falta é **estado do dispositivo**, não um getter de sample rate. Runtime por medir: USB retirado; default mudado para 44,1 kHz. As cinco citações de `miniaudio.h` do relatório (`:22961-22962`, `:22969`, `:22970`, `:41017`, `:41065`) **batem exactas** na árvore fixada `7211c73` (verificado por mim: o ficheiro tem **93 468** linhas; `:22961-22962` é o comentário «automatic stream routing … Otherwise we want to stop the device outright and let the application handle it», `:22969-22970` põe `allowPlaybackAutoStreamRouting = MA_TRUE` só com `pDeviceID == NULL`; o campo nasce em `:7868` e é lido em `:20938`/`:21047`) | **não** (§2) |
| **ART-018** — áudio partido ao carregar/exportar | O relatório diz (§3 `:225-233`): WAV float com NaN/Inf, ou amostras × ganho acima de **256** (ganho do clip até +24 dB, `project.cpp:592`), é **calado em sub-blocos de 32 amostras**, ao vivo e na exportação, **sem contador nem aviso** — `graph.cpp:1247` zera o sub-bloco quando `nativeOutputBad`, `audio_file.cpp:139` deixa o float entrar cru e o `AssetStore` não verifica, e o teste de nível partido da exportação (`render_job.cpp:179` «  if (level.broken)») já só vê o silêncio porque usa o mesmo grafo (`:290`). **Direcção da correcção:** verificar ao carregar em `AssetStore::get` e reportar como áudio em falta; a exportação **já aborta** nesse caso (`render_job.cpp:298` «        throw std::runtime_error("Missing audio, nothing exported: " + list);»). Teste proposto pelo relatório: WAV float com 100 amostras a 1000 → `missingAssets` e `render.start` falha. A parte que **cai**: «o instrumento de plugin da mesma faixa também é calado» — `project.cpp:580` só permite clips em faixas de áudio. Por verificar, pelo próprio relatório: se a importação da app saneia. As citações de código batem na árvore `e436ede` (conferidas por mim) | **não** (§2) |
| **ART-002** — política do bus em falha | `graph.cpp:246` zera e o bus fica calado (linha conferida por mim), `graph.cpp:303`/`:361` metem o plugin no bus com o id do dono (conferidas), e o comportamento está **escrito** na ajuda `server.cpp:54`: «each is silent (its track mutes; on the master the mix goes through) until plugin.retry…». O relatório diz (§3 `:106`) que a correcção é **decisão do dono**: o bus passa o sinal seco, como o master? Teste proposto: 3 faixas para um bus com processador falso que lança; a saída master não pode ser zero depois da falha. Cai a «incoerência com os efeitos nativos» — já não existem (`effects.cpp:10` devolve `nullptr`). Mexe em `graph.cpp` | **sim (graph.cpp)** (§2); é decisão de produto |
| **FL-2** — `plugin.retry` | O relatório diz (§5 `:263`): a app nunca envia `plugin.retry` (em `app/` só existe `EngineSession.cpp:385` «            client_->command("plugins.release");»), o host devolve a mesma instância em cada reconstrução (`juce_host.cpp:306`) e o aviso de `MainComponent.cpp:6345` é falso para plugins; não verificou se alguma edição muda o `stateId`. **Confirmado por mim:** 0 ocorrências de `plugin.retry` em `app/` @ `7211c73`, e `MainComponent.cpp:6345` «        notice_ = u8("Stopped after a fault, silent until your next change: ") + u8(failedNow);» — aviso que promete o que o produto não oferece. É mensagem/UI, não DSP | fora do §2 (FL-2, §5); **não** — é mensagem/UI em `MainComponent.cpp` |
| **FL-3** — restauro quando o `deviceOpen` falha | O relatório diz (§5 `:264`): só há motor guardado quando `otherEngine` (`server.cpp:338` «    if (otherEngine) keptEngine = std::move(engine_);»); se o dispositivo escolher outra taxa (`:359` substitui o motor) e o segundo arranque falhar, `:373` «      sampleRate_ = keptRate;» fica desalinhado com o motor. **Citações exactas**, conferidas por mim em `7211c73`. Estrutura que conferi eu à volta: `:331-334` põem de lado output/info/args/rate **antes** do `try` (`:339`); `:338` também está **antes** do try; `:344` e `:359` constroem motores novos dentro dele; `:367` `keptEngine.reset()` no sucesso; o `catch` (`:371`) repõe `sampleRate_` (`:373`) e o motor guardado **se existir** (`:374` «      if (keptEngine) engine_ = std::move(keptEngine);»), e repõe o output (`:376`/`:378`) ou `outputInfo_ = std::move(keptInfo)` (`:386`). O buraco é o caso `otherEngine == false` com taxa escolhida pelo dispositivo: `keptEngine` vazio, `engine_` fica o de `:359` e `sampleRate_` volta a `keptRate`. **Retiro duas frases minhas** que aqui estavam: «os três `move`s estão dentro do try» (`:338` está antes; `:374` está no catch) e «o `else if (device_->isOpen())` (`:341`) é um `stop()`» — `:341` é `listenDb_ = std::clamp(...)` e **não há** `isOpen` em `server.cpp` (0 ocorrências). Em `e436ede` estas linhas deslocam-se **+1**. Falta teste que parta `outputInfo_` sem excepção | fora do §2 (FL-3, §5); **não** — vive em `server.cpp` |
| **Baixos** ART-004/008/010(b)/011/012/015/017 | Ordem sugerida pela triagem §4, cada um com teste próprio. ART-010(b) **só se o M5 voltar** (D51); ART-008 pede teste de carga antes | ART-008 **sim (engine.cpp)**; ART-010(b) **sim**; ART-004/011/012/015/017 **não** (§2) |
| **ART-016** — colisão de id «master» | O relatório diz (§3 `:204-215`): a colisão **existe no motor** — `project.cpp:514` (o regex de ids aceita «master»), `graph.cpp:333` «  anyMasterEffect_ = buildChain(p.masterEffects, masterEffects_, "master");» e `juce_host.cpp:306` (mesma chave + identidade + sample rate → a mesma instância). **A app impede-a**: `document.cpp:632` «  renameMasterTrack(d);» (definida em `:522`) e `edit.cpp:305` «  for (int n = 2; doc.findTrack(id) \|\| id == "master"; ++n) id = root + "-" + std::to_string(n);»; há teste em `tests/audit_tests.cpp:84` «  CHECK(track != "master");» (não corrido pelo subagente). **Alcance real: só por CLI** (`core-engine render`/`soak`/`loopcheck`) ou outro cliente do protocolo. O relatório acrescenta um efeito sem id (`graph.cpp:301` usa `std::to_string(chain.size())`, que pode coincidir com «0»/«1»; `project.cpp:464` só deduplica ids não vazios) e declara não ter verificado se a app dá sempre id. Citações conferidas por mim em `e436ede`, nos paths reais: `engine/src/plugins/juce_host.cpp`, `app/domain/src/document.cpp`, `app/domain/src/edit.cpp` | **não** (§2) |
| **ART-005** e **ART-007** — os dois «não decidíveis por leitura» | **Só se decidem medindo.** O relatório diz: **ART-005** (§3 `:127-133`) é o custo do `adopt` em cada edição a tocar — `engine.cpp:228` «      next->adopt(*current_);» e `:230`, com a app a mandar cada edição como `project.load` inteiro (`EngineSession.cpp:539` **em `e436ede`**; na árvore de trabalho, entretanto alterada por outra mão, `:539` já é `void EngineSession::sync(...)` — ficheiro 633 → 659 linhas); o limite por leitura são ~3 repetições (`project.cpp:404` recusa notas fora de `1..patternTicks`), a conversão é em tempo constante (`tempo_map.cpp:44`/`:48`) e o `adopt` dos efeitos é vazio (`graph.cpp:249` «  void adopt(Effect&) override {}  // the instance itself goes on playing»); estimativa **não medida**: 30 faixas × 64 notas ≈ 10⁴ conversões, fracção de ms. Medição em falta: `core-engine soak <canção mais pesada> --block 128` e ler `overruns`, `maxLoadPct`, `maxRenderMs`. **ART-007** (§3 `:142-149`) é a **dobra** do teste de finitos: `graph.cpp:120` testa `isfinite` + `kRunawayLevel` e `:121` volta a testar `isfinite`, contra a regra de `finite.hpp:2-3`; mesmo dobrado, o NaN não chega às colunas (`finiteBits` em `graph.cpp:1283`/`:1232`/`:1097`, e `output_guard.cpp:49`). O que muda é o **rótulo e a política**: a falha conta como «unit» e `server.cpp:434` «    if (!it->processor->failed()) return {{"retried", false}};» deixa o retry sem efeito. Medição: o teste do auditor em Release MSVC, ou o código gerado de `checkOutput`; correcção proposta: `finiteBits` em `graph.cpp:120-121`. **Retiro o que aqui estava** (`alignas(64)`, MXCSR por thread, `getCurrentThreadMxcsr()`, ~100 µs com `masterGain != 1` e `timeoutsEnabled`): **0 ocorrências** de cada uma dessas cadeias no relatório, e `getCurrentThreadMxcsr` não existe no repo (0 ocorrências em `engine/`, `app/`, `tests/`; o modo vive em `float_mode.hpp` — `floatMode()`, `setFloatMode()`, `setAudioFloatMode()`) | ART-005 **sim, se medir mal**; ART-007 **sim (2 linhas)** (§2) |
| **Observação do escrivão nesta ronda, fora do relatório ART, não triada** — publicação da exportação | Não é o ART-018 e não vem do relatório (`publishRenderOutputs` e `writeWav`: **0 ocorrências** nele; o próprio relatório declara em `:268` que não leu `wav.cpp`). Conferido por mim no código, com as linhas literais: `engine/src/core/server.cpp:696` «  try {», `:697` «    writeWav(path + ".part", r.L, r.R, (int)sr, bits, channels);», `:701` «    publishRenderOutputs(outputs, published);», `:702` «  } catch (...) {», `:706` «      if (std::filesystem::is_regular_file(part, ignored)) std::filesystem::remove(part, ignored);» (em `7211c73`; em `e436ede` **+1**: `:697/:698/:702/:703/:707`). `engine/src/io/wav.hpp:9` «void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sampleRate, int bits = 24,» — é `void`, não devolve bool; `WavWriter` **lança** (`wav.cpp:71/:75/:81/:98/:108`). `engine/src/core/render_job.cpp:72` «void publishRenderOutputs(const std::vector<RenderOutput>& outputs, int& published, const std::function<void()>& beforePlace) {» renomeia progressivamente (`:114` «      fs::rename(partOf(outputs[f].path), utf8(outputs[f].path), e);», `:116` «      placed[f] = 1;») mas **desfaz** o que colocou: `:96` «      if (placed[f]) { fs::remove(utf8(outputs[f].path), ignored); if (ignored) ++left; }» e `:99` «    published = left;». **A frase que eu tinha escrito — «o que já foi publicado não é desfeito pela limpeza» — é falsa e fica retirada**: o undo remove os ficheiros já colocados e `published` conta só os que não conseguiu remover (`:139` «  int published = 0;  // files put in place (all of them on success; on a failure only those that could not be undone)»). Não triado: ninguém avaliou se há aqui defeito | fora do §2; **não** — `server.cpp`/`render_job.cpp`/`wav.*` |

A pendência do **null-check em `graph.cpp:867`/`:870`** mantém-se escrita como está na tabela «Do repo actual»:
**adiada** — a triagem não a fecha, não a reabre e não lhe mudou o âmbito.

### Ronda de 18/11 — pendências abertas (o que falta e quem decide)

Abertas pelo estado de `main` `f6b86dc` e pelos 17 relatórios de 18/11. **Nada aqui é decisão nova**: o que tem
palavras do dono está numerado D99–D114; o resto é execução, medição ou pergunta. Estatuto: os gates dos ramos
são **reportados pelas entregas**, não re-executados por mim.

| Pendente | O que está em aberto | Quem decide / estatuto |
| --- | --- | --- |
| **Round 2 de consolidação — em voo** | `integration/flawless-round2` estava a mergear durante esta ronda (`caeb980` @ 15:18:23Z, 9 s antes da medição; já com `flawless-l9`, `flawless-crashreset`, `flawless-m5`). **15 ramos por integrar em `main`:** batch2 `b64e95e`, builtins `e035eb7`, clean `e5f2fca`, crashreset `7ad1b72`, exportpart `dff5ba9`, l9 `e7037da`, m5 `569d995`, modules `e6ead02`, ratified `c7612c3`, roll2 `2718c64`, samplehost `4a508b3`, state `81da46e`, ux-piano `8ce28fc`, ux-windows `d23704a`, integration2. Gates próprios reportados: batch2 **209/3115**, m5 **205/3084** (um intermitente declarado), crashreset **203/3069** | execução de **D99/D107/D108**; não é decisão. O tip integrado exige gate verde + união provada antes de ff para `main` |
| **L10 — limpeza final + teste de amplitude** | `flawless-clean` `e5f2fca` por integrar: 21 templates mortos removidos, `conversionNotes` reportados no render/measure, **teste de amplitude no gate** (demo pico 0→0.7434 com áudio gerado/test-synth), app abre vazia. Em `main` confirmei eu: **21** `*.core-template.json` ainda presentes e o teste de amplitude do demo **não** está lá | autorizado por **D105/D106**; falta integrar e gatear. **Não verificado por mim** |
| **UX — integrar e refinar com o dono** | `flawless-ux-piano` `8ce28fc` (+552/−51 em `PatternEditor.cpp`, +145/−1 em `MainComponent.cpp`) e `flawless-ux-windows` `d23704a` (+678/−92 em `MainComponent.cpp`, `PanelWindows.*` novos), ambos por integrar. Autorizados por **D102/D103**; o dono disse «a partir dai depois eu refino» | falta **o dono ver** e o refino. Verificação visual não feita por mim; os relatórios citam a fonte errada para o verbatim (ver o delta na subsecção das decisões) |
| **FL-2 — plugin falhado nunca é retentado** | medido por mim em `main`: **0** ocorrências de `plugin.retry` em `app/`; o aviso `MainComponent.cpp:6345` continua a prometer «silent until your next change». O motor já tem o comando. Falta: «Try again» em menu/aviso existente + texto correcto; **export reporta sucesso com plugin calado/em falta** (`render_job.cpp:295-299` só vê áudio em falta); `plugins.missing` não é anunciado; `relinkPlugins` nunca chamado; estado inválido ignorado em silêncio | âmbito **D99/D108** (item 4 do plano). Mexer em menus/avisos e no desfecho do export = **D48/D50 por item** |
| **M5 — continua OFF (D51)** | `server.cpp:102` `return 0` confirmado por mim em `main`. Soak 8 h + bit-a-bit **não feito**. O plano diz que M5 **não tem teste algum** e exige teste determinista serial-vs-M5 com provider falso antes de soak; fixes RT no ramo `flawless-m5` (`da91dab` re-arm após stall, `88aa357` watchdog de reset, `0e45d58` held notes em swap) por integrar | **ligar exige palavras do dono (D51)**; D98/P-D98 não mudam isto |
| **Patch vendored no JUCE — infraestrutura** | **D113** autorizou o patch, aplicado em `cc5b874`. O patch vive como **modificação da árvore do JUCE pinned** (`D:/Core-deps/JUCE-9.0.5`), nunca commitada lá; está tracked em `ci/patches/juce-failed-plugin-restart.patch` (10 hunks). Qualquer re-fetch/clone do JUCE perde-o e o gate pode ficar verde sem ele | falta decidir **como** o bootstrap aplica o patch (proposta da equipa, não decisão). Risco de falsa-verde registado |
| **Estado de plugins por edição — a raiz** | o limite 8 MB→256 MB (`0090460`) trata o sintoma; medidos **157 ms por edição com 32 faixas**. Falta a projecção enviar só `stateId`, `project.load` responder que chaves precisam de estado, `plugins.states` aceitar lista, export ler do live (`server.cpp`, `juce_host.cpp`, `EngineSession.cpp`, `render_job.cpp`, `history.cpp`). Ramo `flawless-state` `81da46e` por integrar | item 2 do plano, âmbito **D99/D100**. **Não verificado na app** com plugins reais >8 MB |
| **Null-check defensivo `graph.cpp`** | confirmei em `main`: `Graph::allNotesOff` e `liveNote` continuam a desreferenciar `t.instrument` **sem guarda**. A pendência acima mantém-se **adiada**, com o mesmo âmbito | proposta da equipa, sem decisão (D48/D50) |
| **Detalhes do BUG-SWEEP-30 e do 26** | D110 ratificou as políticas; ficam por responder: factor realizado vs nominal, piso de endpoint zero, Normalize flatten-vs-shape; e a limitação de delay baked-in no fresh-apply do 26 | **perguntas ao dono**, não decisões |
| **VST2 — forma por decidir** | **D109** dá o âmbito («quero implementar vst2 tambem», «é para o futuro ou em casos especificos»). Cabeçalhos Steinberg ausentes neste PC; `Xaymar/vst2sdk` é referência de ABI, não drop-in. Sequência (depois dos itens 1–4) e flag de build são **leitura do plano** | falta o dono dizer se tem SDK antigo ou aceita cabeçalho de compatibilidade escrito de raiz |
| **Critérios D100 — por medir** | os cinco alvos não estão provados: falta teste de amplitude no gate (L10), prova multicore=serial com plugins reais sob carga, null tests de automação/compensação de latência a todos os sample rates e block sizes, e tooling que prove ausência de alloc/lock/IO no thread de áudio | **D100** fixa-os como alvos; os números concretos continuam pendentes (linha «Alvos numéricos de performance») |
| **PROD-01…PROD-10 e itens 16/17 do RESUMO** | **D110/D111** cobrem por referência as políticas 10/11/21/30/26/28/31 e o channel rack = P-Core; **D104/D105** cobrem «instrumentos próprios nos menus» e o «Randomize sound» sem nada para randomizar (passa a randomize sobre parâmetros de plugins, escondido sem plugins). O resto de PROD-01…10 continua pendente | coberto **por referência** onde há verbatim; o restante mantém-se (D89) |
| **Relatórios de outra sessão, por commitar** | no checkout principal: `?? docs/relatorios/2026-11-17-teste-real-app.md`, `?? …plano-interface-chain-abas-laterais.md`, `?? …evidencia/2026-11-17-teste-real-app/` e as 2 linhas de índice no `README.md`. **Não são meus, não os commitei**; o README foi resolvido por união exacta para devolver as 2 linhas | da sessão que os escreveu; o framing «silêncio P1» do primeiro está contextualizado em Estado medido |
| **P-D98 — continua aberta** | o alcance da delegação (D98) vs D48/D50/D76, com as três opções (a)/(b)/(c). **D99–D114 não dependem dela**: são palavras do dono | **owner = Diogo** |

### Colisão C12 — **resolvida por referência** (D92/D93, 17-11 20:24)

Registada e perguntada como manda a regra COLISÕES de D72; o dono resolveu. A linha fica, com a resolução datada.

| # | Colisão | Evidência verbatim | Fonte | Opções (não escolho) |
| --- | --- | --- | --- | --- |
| **C12** | Texto de decisões sobre sanitizers chegou com os números **D75 e D76**, que já estão ratificados hoje para outra coisa (D75 = namespace `BUG-`; D76 = folha do coordenador pendente). O primeiro vem escrito «75:» sem o `D`. | «75: ligo CORE_SANITIZE no CMake: address→MSVC (/fsanitize=address + /DEBUG); undefined/thread→só Clang/GCC, FATAL_ERROR se o compilador não suporta (nunca D9002 ignorado); exige Debug; instrumenta só os nossos targets (JUCE/miniaudio ficam de fora, declarado). Acrescento canários (asan/tsan/ubsan) que falham se o sanitizer estiver silencioso, e uma assertiva de cobertura que falha se algum ficheiro do scope não estiver no compile_commands.» · «D76: os três passos correm em WSL (zero instalações; TSan não tem caminho em Windows); no host fica só ASan MSVC como prova de Windows. Não instalo LLVM no host.» | esta sessão, `D--CORE/61b1abc0…jsonl` **L616**, ts `2026-11-17T19:30:21.934Z` (turno interrompido) | (a) entram como **D92/D93** (próximos livres) com esta citação e fonte; (b) pertencem a outro ledger/sessão e ficam fora deste; (c) o dono reescreve com número. **Nada foi implementado nem ratificado por mim a partir deste texto.** O relatório de pré-flight correspondente (`docs/relatorios/2026-11-17-preflight-sanitizadores-clang-tidy.md`) apareceu por comitar na árvore durante esta sessão — sinal de outra entrega a escrever no mesmo checkout; não o comitei. **Medido depois (19:41):** essa entrega implementou o texto na árvore — `CMakeLists.txt` com `CORE_SANITIZE` e `FATAL_ERROR` em vez de D9002 (mtime 19:37), `tests/CMakeLists.txt` e `tests/canary.cpp` (19:38). O conteúdo está a ser executado; **os números D75/D76 continuam em colisão** com as ratificações de 19:17. Não comitei nem alterei nenhum desses ficheiros. **RESOLVIDA por referência em 17-11 20:24 (D92/D93):** o dono ratificou os dois próximos livres — «D92/D93 (ratifico já)» — e os números ficam para D92 (wiring `CORE_SANITIZE` + canários + assertiva) e D93 (três passos em WSL, host só ASan MSVC, sem LLVM). A colisão de numeração deixa de existir: o texto de L616 é o conteúdo, D92/D93 são as entradas ratificadas. **Correcção datada da frase «Nada foi implementado»:** continua verdadeira quanto a mim (não implementei nada), mas está desactualizada quanto ao repo — o trabalho **está implementado, medido e commitado** em `54addba` (build) e `4cc08b4` (docs+evidência), e verifiquei que `origin/main` = `4cc08b4f44b87ddf71dc3f8da8dfb373b5d32ef7`. A opção (b) «pertence a outro ledger» ficou descartada |

### Propostas coladas nesta sessão, por ratificar (IDs fora do formato `D###`)

Chegaram em 20:13 com os IDs `D-LinuxHost` e `D-TransportSplit`. Pelo precedente de D77, IDs fora do formato não
são decisões: ou mapeiam para as já ratificadas, ou ganham número quando o dono ratificar. **Fila de números (actualizada 20:24):** C12 foi resolvida — **D92/D93 ratificadas** e ocupadas. Estas duas
(20:13) seguem como **D94/D95, pendentes, não ratificadas**. Nada foi implementado a partir delas.

**Actualização da fila (17-11 ~21:55).** Às ~21:45 chegaram duas decisões com os slugs `D-SanLauncher` e
`D-JuceAideAscii`, e o pedido de ledger propunha registá-las como D94/D95. **Não puderam usar esses números**:
D94/D95 estão ocupados pelas duas propostas acima, que continuam pendentes e não ratificadas. As decisões
ratificadas usam os dois livres seguintes, **D96/D97** (ver a subsecção «Ratificadas por endosso» em Decisões),
pelo mesmo critério aplicado a C12. A fila D94/D95 **mantém-se** onde estava, com os mesmos números, à espera de
palavras do dono; nada foi implementado a partir dela.

| Proposta | Citação verbatim | Fonte | Verificação que fiz ao código (medido, não assumido) | O que falta |
| --- | --- | --- | --- | --- |
| **D-LinuxHost** | «D-LinuxHost: relaxar o gate AND WIN32 só para o alvo core_plugin_host (JUCE compila headless em Linux), permitindo que WSL tidy+ASan/UBSan/TSan cubram juce_host.cpp. Desktop continua WIN32. Fecha a lacuna do juce_host barato.» | esta sessão, `D--CORE/61b1abc0…jsonl` **L989**, ts `2026-11-17T20:13:58.746Z` | **A premissa do mecanismo não bate com a árvore.** `core_plugin_host` **não tem guarda WIN32**: é criado em `engine/CMakeLists.txt:45` sob `if(TARGET juce::juce_audio_processors)`. O `AND WIN32` está em `CMakeLists.txt:56` e gateia o **FetchContent do JUCE** (e em `:72` o desktop/installer/selftest-plugins). Em Linux ou com app OFF o JUCE não é disponibilizado → o alvo não existe → `engine/src/plugins/juce_host.cpp` nunca compila → fica fora do tidy/sanitizers. **A lacuna é real; a descrição do que a fecha está errada.** «Desktop continua WIN32» é compatível com `CMakeLists.txt:72` | O que teria de mudar é disponibilizar JUCE em Linux (opção nova ou mover o FetchContent para fora da guarda) = **nova dependência/superfície de build**, gatilho D48/D50. E `core_plugin_host` liga `juce::juce_gui_basics` (`engine/CMakeLists.txt:49`): o custo em Linux/WSL **não foi medido** — não fiz sonda, não verifiquei os pacotes de sistema que o módulo GUI exige. «compila headless» e «barato» ficam por provar antes de ratificar |
| **D-TransportSplit** | «D-TransportSplit: partir engine_client.cpp em protocolo/state-machine cross-platform (TSan-testável em WSL) + transporte Win32 fino (revido + stress em Windows). Alternativa: aceitar e registar a lacuna de races no transporte. Recomendo o split — é o que torna o IPC provável sem TSan em Windows.» | esta sessão, **L989**, mesmo ts | **A premissa bate.** `app/engine-client/src/engine_client.cpp` = 375 linhas, `#error` fora de Windows (linhas 4–6), `windows.h`; `app/engine-client/CMakeLists.txt:2` salta o alvo em não-Windows. O protocolo **já está separado** (`engine_protocol.hpp`, 72 linhas). O que está misturado no .cpp: transporte/processo (`start`:60, `close`:144, `killForTest`:165, `writeLoop`:215, `errorLoop`:238, `readLoop`:254) e protocolo/estado (`enqueue`:174, `command`:204, `onLine`:299, `disconnect`:354) com `mutex_`/`connected_` | **A voz é de recomendação** («Recomendo o split»), não de decisão: por D48/D50/D72 isto é proposta até o dono a ratificar por palavras dele. Autoria por confirmar — pode ser texto de outro actor que tu colaste. A alternativa que o próprio texto dá (aceitar e registar a lacuna de races no transporte) é opção legítima; **não escolho**. Partir o ficheiro é refactor de arquitectura → gatilho D48/D50, e D50 exige «uma variável de cada vez» |

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
| **D92/D93** (resolução de C12) | a colisão de numeração C12 e as suas três opções; o texto de L616 passa a ser o conteúdo das entradas D92/D93 | `a571968b…jsonl` L861, ts 20:24:49.174Z |
| **D92/D93 + evidência §7** | a linha «Contagem de casos em Windows: não existe no log» como afirmação sobre o estado actual — existe 113/1456 em ASan MSVC | [`2026-11-17-sanitizadores-evidencia.txt`](docs/relatorios/2026-11-17-sanitizadores-evidencia.txt) §7 |
| **`54addba` + `4cc08b4`** | «o trabalho dos sanitizers está por comitar / nada foi implementado» como descrição do repo — está implementado, medido e em `origin/main` | `git log`, `git ls-remote origin main` |
| **Triagem ART (subagente `triagem-art`, 17-11 22:17, leitura de `7211c73`)** — sem força de decisão: **nenhum dos três é `D`** | ficam sem efeito, **como defeitos em aberto nesta linhagem**, as três linhas do lote A-real-time. **ART-006** (padrões fora de limites → milhares de repetições): a validação está em `project.cpp:552` e `:570` — `if (t.patternTicks < kPpq / 8 \|\| t.patternTicks > 64LL * 4 * kPpq) throw … "invalid pattern length."`, com kPpq = 960 o mínimo é 120; e o ciclo de `sourceOns` (`graph.cpp:479`, `for (int64_t k = 0; origin + k * clip.patternTicks < clip.end; ++k)`) só cresce com um projecto escrito à mão para isso e corre no thread de controlo → gravidade nula, não alcançável por omissão. **ART-013** (NaN a entrar por `listenDb`/`db`): os chamadores dão valores finitos — `server.cpp:392` exige `is_number` **antes** de `:393`, `:341` já usa `std::clamp`, `output_guard.cpp:23` é `if (!(db == db)) return;  // NaN: ignored` (guarda por baixo do `clamp`), e o JSON do produto não tem gramática para NaN → o alcance cai, a linha opcional (`finiteBits`) é de higiene. **ART-014** (a loudness antes do guard envenena o estado com NaN): a ordem é real (`engine.cpp:397` `loudness_.process(L, R, frames);` antes de `:400` `guard_.process(...)  // last: what the speakers get is never dangerously loud`) e o estado recursivo sem reset também (`loudness.hpp:44` `z1 = b1 * x - a1 * y + z2;`, `analyzer.cpp:51` `ll_ = ll_ * decay_ + a * a;`), **mas o NaN não chega lá**: todas as saídas de unidade passam por `nativeOutputBad` (definido `graph.cpp:132`, usado `:1097`/`:1232`/`:1247`/`:1283`) e a protecção nunca é desligada — `setNativeProtection` (`graph.hpp:186`, `protectNative_ = true` em `:370`) tem **0 chamadas** fora da definição. **Reabre ART-014** se alguém passar a chamar `setNativeProtection(false)`. O que **se mantém** dos três são os factos de código acima, que conferi nesta ronda | relatório [triagem ART](docs/relatorios/2026-11-17-triagem-art.md) §2 `:85`/`:91`/`:92` e §3 `:135-139`/`:185-195`. **Estatuto: leitura de um único verificador, nada executado.** Os ~14 números de linha e as 0 chamadas a `setNativeProtection` foram verificados **por mim** contra `7211c73`; nenhum teste prova o fecho de nenhum dos três |
| **Código `project.cpp:524-529` (desde `b80017e`) + teste `engine_tests.cpp:482` (`c1d86b0`)** — sem força de decisão: **A-009 não é `D`**, é um facto verificado | A linha do lote **A-real-time** que trata A-009 («canção antiga com instrumento retirado crasha»; «`project.cpp:520` lê sem validar») como **defeito em aberto nesta linhagem**. Fica sem efeito nessa formulação: a conversão para `none` com nota já existe na raiz do produto e o `git diff b80017e HEAD -- engine/src/core/project.cpp` = **0 linhas** não deixa de pé a hipótese alternativa «veio de outra linhagem/snapshot» para este ficheiro. O que **se mantém de pé** é o mecanismo do crash descrito (sem a validação, `makeInstrument` devolve `nullptr` e `graph.cpp:870` desreferencia-o) — visível no controlo de não-vacuidade, exit 139. O relatório pedia número `BUG-` (D75) para o registo: **não atribuído**, porque não há defeito confirmado nesta árvore e o pedido de ledger desta ronda diz expressamente «sem número D»; se quiseres um `BUG-` para o refutamento, é dizer | [relatório A-009](docs/relatorios/2026-11-17-a009-instrumento-retirado.md) §Resultados `:24-28` e §Pendências `:114-126`; medições da linha A-009 em Estado medido |
| **D102 + D103** (18/11, verbatim do dono) | o item 11 de `docs/plan/README.md` — «**The owner has not approved the design yet: no UI code before he does**» — e, nas pendências «Do repo actual», a parte de **UX Pulse** da linha «Desenho da remodelação» (D87). Passou a haver palavras explícitas do dono para adoptar os conceitos do Pulse mantendo o look/menus/cores do Core, com janelas destacáveis, dropdowns, channel rack expansível e a fatia do piano. **O que continua pendente:** o refinamento («a partir dai depois eu refino») e os números/desenho da remodelação que não são UX Pulse | Qoder `613ad19e` L333/L386/L396/L398/L475/L477/L490/L492/L532/L563/L662/L2314/L2322/L2332/L2334/L2336; DSH seq 926/970/987/999/1014 — ver a subsecção «Ratificadas por verbatim do dono — 18/11» |
| **D110** (18/11 12:58:21.699Z) | as «owner flags open» do relatório [pattern-26-28](docs/relatorios/2026-11-18-pattern-26-28.md) **na parte de política**: 26 (preservar swing ao guardar; unswing só em opção explícita de export) e 28 (semântica parte-primeiro, patterns pertencem à parte). Ficam ratificadas; o que sobra é técnico (limitação de delay baked-in no fresh-apply), não decisão | Qoder `613ad19e` **L2379**, itens 26 e 28 verbatim |
| **D105 + D106** (18/11 12:45–12:50) | a condição do plano item 7 «**The 5 visible templates stay until the owner says otherwise**» para os templates **com efeitos**: o dono disse «remove templates com efeitos» e «se existirem templates com efeitos antigos remove». O L10 (`flawless-clean`, por integrar) remove os 21 e os testes que deixaram de provar nada. **Não invalida** a preservação do catálogo de padrões, alinhamento kick/baixo/pratos, projectos e áudio do utilizador | Qoder `613ad19e` **L2268/L2275/L2289/L2287/L2285/L2302**; DSH seq 821/840/861 (contexto) |
| **D114** (18/11 05:37:42.694Z) | a causa técnica da regra «nunca correr dois gates ao mesmo tempo» — o `%TEMP%\core-gate.bat` partilhado. A partir de `fffbb41` cada invocação tem bat único (`core-gate-<guid>.bat`), provado com 2 gates concorrentes VERDE. O `GATE-LOCK` mantém-se como prática de coordenação entre streams, não como necessidade técnica do bat | Qoder `613ad19e` **L1007** («ok») à proposta **L1000**; [relatório gate-unique-bat](docs/relatorios/2026-11-18-gate-unique-bat.md) |
| **D104 + medição do coordenador** (18/11) | a leitura «o silêncio dos builtins é defeito do motor» onde quer que apareça: o Core não tem instrumentos/efeitos/osciladores/filtros/sampler próprios (palavras do dono) e o coordenador mediu som com dois sintetizadores reais — «O "silêncio total" era o teste, não o Core». **Não é entrada `D` deste ledger** que caia; fica sem efeito como diagnóstico, e a lacuna real (teste de amplitude no gate) é o L10 | DSH `session-2ff54f03` seq **814** (01:17:48.408Z); Qoder `613ad19e` **L1295/L2350/L2352/L2504**; `docs/plan/README.md:14-16` |

---

## Estado medido

| Item | Valor | Estatuto |
| --- | --- | --- |
| Gate Linux | 15 testes / 113 casos / 1456 verificações, 0 falhas | **reportado** pelo audit de 17/11 sobre `59cf952`; não reexecutado nesta sessão |
| Percurso dos casos | 88 → 104 → 111 → 113, nunca desceu | reportado (`2026-11-17-auditoria-core.md` §1) |
| Gate Windows 18:43 | **VERDE — 15/15 `merge`** @ `b80017e` *com alterações por commitar*; build `D:/Core-build/gate-CORE`; 212 s de gate, 7.20 s de testes; por teste: 0.04–2.37 s | log guardado em [`2026-11-17-gate-windows.txt`](docs/relatorios/2026-11-17-gate-windows.txt) com SHA-256 dos originais (D90). **Não é gate de árvore limpa** |
| Gate Windows em árvore de código limpa (19:37) | **VERDE — 15/15 `merge`** @ `eb9d847`, pasta nova `D:/Core-build/gate-CORE-limpa`, build completo 329 passos (328 executados), 228 s de gate / 6.90 s de testes, `rc=0` | **medido por mim** (D90). Ressalva do script «com alteracoes por commitar» explicada: `ci/gate.ps1:56` usa `git status --porcelain`, que conta docs por comitar e 2 ficheiros não comitados de outra entrega; `git diff` do código = vazio. Log e SHA-256 em [`2026-11-17-gate-windows.txt`](docs/relatorios/2026-11-17-gate-windows.txt) |
| Árvore depois de D83 | `git diff` sobre `app engine tests ci patterns` = **vazio**; código = `b80017e`; commit documental `eb9d847` (14 ficheiros, 712+/52−) | medido antes e depois do commit |
| Recuperação do que foi revertido | stash-commit `1abade528f6fe506152311183d55e38ada681288` em `refs/escriva/unratified-2026-11-17` (506+/129− em 11 ficheiros) + cópia em `D:\CORE\.escriva\unratified-2026-11-17\` (inclui `DspPerformance.*` e os 4 relatórios) | medido |
| Itens do audit não medidos | 7, 8, 15, 18, 27, 28, 31 (Windows-puro: só compilação/leitura) | reportado |
| Limites conhecidos | 3a = hash dos primeiros 64 KiB; 3b = revalida só o cabeçalho WAV; índice da biblioteca ficou na versão 2 de propósito | reportado (`RESUMO.md`) |
| Contagem de casos/verificações em Windows | **Correcção 20:5x — existe.** A ronda ASan MSVC 19.51 no host dá **113 casos / 1456 verificações / 0 falhas**, com contagem por suite (evidência §7). O que era verdade: os logs do gate de 18:43 e 19:37 **não traziam** contagens, só 15/15 | medido por mim no ficheiro commitado [`2026-11-17-sanitizadores-evidencia.txt`](docs/relatorios/2026-11-17-sanitizadores-evidencia.txt); somas conferidas (113 e 1456) |
| Concurrent writer no mesmo checkout | Outra entrega escreveu `CMakeLists.txt` (19:37), `tests/CMakeLists.txt` e `tests/canary.cpp` (19:38) — o trabalho de sanitizers do texto L616 (C12). **Não comitei nem alterei esses ficheiros.** **Correcção 20:5x:** esse trabalho **está commitado** em `54addba` (build) e `4cc08b4` (docs+evidência), e `origin/main` = `4cc08b4f44b87ddf71dc3f8da8dfb373b5d32ef7` — o que arrasta também os meus 4 commits documentais (`eb9d847`, `1bf65f3`, `e502be9`, `9d87065`), que eu não tinha pushed. **Nota datada 21:18:** `origin/main` é agora **`7ad8ccaa72cadd132b2f19c33142c4b3eed85e68`** — a entrega do launcher (`3097c83` + `7ad8cca`, 21:14) foi commitada e publicada depois, arrastando o `8850edd`. O `4cc08b4` nesta linha fica como o valor medido às 20:5x, não como o remoto actual | medido (`git log`, `git rev-parse origin/main`, `git ls-remote origin main`; re-medido 21:18) |
| O gate das 19:37 não apanhou esses edits (**histórico, mantém-se**) | `CMakeCache.txt` e `build.ninja` da pasta nova têm mtime **19:34** e **0** ocorrências de `CORE_SANITIZE`, `canary` ou `fsanitize` (cache, `build.ninja` e `gate.log`); o configure foi anterior às edições | medido por contagem nos três ficheiros |
| Terceira execução do gate por mim | **não feita** — construiria o trabalho em curso de outra entrega e disputaria locks de build. **Correcção 20:5x:** a ronda D92/D93 correu quatro gates (abaixo), por isso a re-execução pedida por D90 está satisfeita mais do que uma vez | decisão de método, registada |
| Runtime da app nesta sessão | build + 15 testes de gate corridos em Windows; **a app não foi aberta**, nenhum self-test desktop, nenhum áudio ouvido | medido por mim |
| **A-009 (lote A-real-time) — desfecho: REFUTADO nesta linhagem** | A premissa «`project.cpp:520` lê o instrumento sem validar» não corresponde à árvore: 520 lê e **524-529 validam** contra o registo (`plugin`/`audio`/`bus`/`none`, `engine/src/dsp/instruments.cpp:29`), empurram nota para `loadNotes` e convertem para `none`. O bloco **já estava na raiz `b80017e`** — e `b80017e` **é** a raiz desta linhagem na árvore (`git log --all -S ownInstrumentLeft -- engine/src/core/project.cpp` devolve **só** `b80017e`), pelo que não há commit anterior onde ele pudesse faltar. **Data corrigida vs o pedido de ledger:** o commit é de **17-11 17:48 UTC**; o «16/11» vem do comentário do próprio bloco («Core's own instruments left the engine (2026-11-16)»), que é a data da ordem D88/D46, não a do commit. `git diff b80017e HEAD -- engine/src/core/project.cpp` = **0 linhas** — `instruments.cpp` idem; o lote fixou `8850edd`, que **é ancestral de `main`** (`git merge-base --is-ancestor` → sim). Logo: **é o mesmo código**, não veio de outra linhagem nem de outro snapshot. Guarda de regressão entregue: `tests/engine_tests.cpp:482`, **+37 linhas** em `c1d86b0` (1 caso / 5 verificações / 0 falhas / exit 0). Controlo de não-vacuidade da entrega: bloco removido → **3 `FALHOU` + segfault exit 139** em `Graph::allNotesOff` (`graph.cpp:870`), reposto byte-a-byte (`git diff` vazio). Gate `merge` **15/15 VERDE**, **114 casos / 1461 verificações / 0 falhas** = **+1 caso / +5 verificações** vs a baseline `113/1456` da ronda sanitizers; nenhuma cobertura reduzida. **Sem correcção de código**: o único ficheiro alterado é o teste; `graph.cpp` ficou por tocar. Número: **sem `D`** — A-009 não é decisão (ver Invalidadas) | Medido por **outra entrega** ([relatório](docs/relatorios/2026-11-17-a009-instrumento-retirado.md); gate corrido em `7ad8cca` com o teste ainda por commitar, hoje `c1d86b0` = seu filho em `main`). **Conferido por mim** nos ficheiros commitados em `d667527`: bloco 524-529 presente e **byte-igual desde `b80017e`**, registo em `instruments.cpp:29`, `makeInstrument` devolve `nullptr` para nome fora do registo (`:35`), teste em `engine_tests.cpp:482`, derefs **sem guarda** em `graph.cpp:867` e `:870`, `b80017e`/`8850edd` ancestrais de HEAD. **Não re-executei nada**: o segfault do controlo e as contagens 114/1461 ficam como **reportado pela entrega** |
| **FL-1 (lote A-real-time / triagem) — M5 ligado por omissão: corrigido** | O `Server` arrancava o motor com **M5 ligado por omissão**, contra a D51: `liveThreadsOrDefault` em `server.cpp:100` @ `7211c73` = `if (v.empty()) return WorkerPool::forThisMachine(4);`, ou seja `min(4, núcleos-1)` = **4** na máquina do dono (16 lógicos). O `Engine` **já tinha** default 0 (`engine.hpp:76` `explicit Engine(double sampleRate, int liveThreads = 0)`; getter `:148`) — a divergência vivia só no `Server`; a app nunca enviava `liveThreads` (0 ocorrências em `app/`) e nenhum teste fixava o default (0 em `tests/` antes da correcção). **Correcção = uma linha funcional:** `server.cpp:101` @ `e436ede` `if (v.empty()) return 0;  // D51: M5 OFF by default; opt-in by CORE_LIVE_THREADS or {liveThreads}`; ajuda `:23` passa a «default 0 = serial, D51; opt-in by argument or CORE_LIVE_THREADS» e os comentários `:89-91`/`:325` idem. Opt-in por `{liveThreads}` e por `CORE_LIVE_THREADS` **intacto**; exportação (`CORE_EXPORT_THREADS`/`WorkerPool::forExport`) por tocar; `forThisMachine` continua usado por `forExport` — não ficou órfão. Regressão: `tests/engine_tests.cpp:483` (+48 linhas) com guarda RAII de `CORE_LIVE_THREADS` (`_dupenv_s`/`_putenv_s` em MSVC, `setenv`/`unsetenv` POSIX) e **três** verificações: default → 0, `{liveThreads:2}` → 2, `CORE_LIVE_THREADS=4` → 4. **Vermelho medido 4 / esperado 0** @ `7211c73`, verde após a correcção. Gate `merge` **15/15 VERDE**, **115 casos / 1464 verificações / 0 falhas** (engine 22/89) = **+1 caso / +3 verificações** vs `114/1461` da ronda A-009 — somas por suite conferidas por mim (22+9+21+8+10+2+7+5+14+7+10 = 115; 89+64+216+115+49+25+65+66+649+82+44 = 1464). **Enquadramento (D50):** «Facto implementado sem decisão = "erro da equipa, a corrigir"» — foi isso: o default contradizia uma decisão ratificada (`ESTADO.md:39`) e **está corrigido sem decisão nova**, porque a autorização é a própria D51. **Commit:** `e436ede` `Engine: M5 OFF por omissao (FL-1)`, pai `7211c73`, 4 ficheiros (`132/0` relatório + `1/0` índice + `6/5` `server.cpp` + `48/0` teste), data de **`git log -1 --format=%cI e436ede` = 2026-11-17T22:31:28Z** (não de comentários) | **Medido pela entrega FL-1** ([relatório](docs/relatorios/2026-11-17-fl1-m5-default.md), commitado em `e436ede`). **Conferido por mim** nos dois SHAs: `server.cpp:100` em `7211c73` e `:101` em `e436ede`, ajuda `:23`, `engine.hpp:76`/`:148`, numstat e `%cI` do commit, e as somas do gate. O **vermelho/verde e o 15/15** são execução **reportada pela entrega — não re-executei** nesta ronda. Backend **null** (headless): não prova WASAPI real. **Soak 8 h + bit-a-bit vs serial com `CORE_LIVE_THREADS=4`** (condição da D51 para ligar): **não verificado / não feito** |
| **Triagem do lote A-real-time (ART) — 16 achados, desfecho por achado** | **5 confirmados** — ART-002 (política M3b, decisão de produto) · ART-003 (runtime por medir) · ART-008 (probabilidade baixa) · ART-015 (latente) · ART-018 (uma parte cai). **6 parciais** — ART-004 · ART-010 (premissa «M5 desligado» falsa; o FL-1 tinha-a refutado antes, e foi corrigido em `e436ede`) · ART-011 · ART-012 · ART-016 · ART-017. **3 refutados** — ART-006 · ART-013 · ART-014 (alcance; ver Invalidadas). **2 não decidíveis por leitura** — ART-005 · ART-007 (dobra). Total 16: o lote do auditor tinha 18, A-001 e A-009 já estavam refutados. **Contagem divergente dentro do próprio relatório:** a frase de abertura `:41` diz «3 confirmados, 7 parciais, 4 refutados (um só quanto ao alcance) e 2 não decidíveis»; **vale a tabela do §2** — contei as duas fontes nesta ronda de forma independente: as 16 linhas do §2 dão 5/6/3/2 e os 16 títulos do §3 dão o mesmo. **Numeração:** `ART-0NN` substitui `A-0NN` deste lote em todas as referências futuras (era `A-002`…`A-018`, com ART-001 e ART-009 fora porque A-001/A-009 já estavam refutados) → resolve a colisão com o lote A anterior. **Drift e fiabilidade das citações, medidos por mim:** as linhas de `server.cpp`/`graph.cpp`/`device.cpp`/`project.cpp` citadas ancoram a `7211c73` (declarado no relatório `:21`); em `e436ede` deslocam-se **+1** a partir de `:101` (verificado em `output_.reset()` 372→373, `writeWav(outputs` 699→700, `u8path` 705→706 — o FL-1 deu +1 linha líquida: 861 → 862). Citações conferidas por mim contra `7211c73`, **batem certas** (device.cpp `:116/:126/:127/:131/:132/:162`, server.cpp `:54/:323/:337/:341/:349/:378/:392/:393`, graph.cpp `:246/:303/:361/:479/:1097/:1232/:1247/:1283`, engine.cpp `:397/:400`, project.cpp `:552/:570`, loudness.hpp `:44`, analyzer.cpp `:51`, graph.hpp `:186/:370`, miniaudio.h `:22961-22962/:22969-22970/:41017/:41065` com o ficheiro a ter **93 468** linhas) e **fica sem efeito a atribuição das «7 citações que não batem»**: uma versão anterior desta célula, escrita nesta ronda e nunca commitada, atribuía ao relatório sete citações que ele não contém — **erro de atribuição meu**, confirmado por contagem de ocorrências no ficheiro (coordenador e eu, iguais): `publishRenderOutputs`, `setSampleRate`, `setLiveSampleRate`, `writeWav`, `try/catch`, `:610`, `:623`, `:239`, `:697`, `:149`, `:156` → **0** cada; o relatório declara em `:268` que nem leu `wav.cpp`. O que eu tinha lido no código sobre `publishRenderOutputs`/`.part`/`writeWav` não pertence ao ART-018 e passa a linha própria em Pendentes («observação do escrivão nesta ronda, fora do relatório ART, não triada»); a frase «o que já foi publicado não é desfeito pela limpeza» era **falsa** (`render_job.cpp:96` remove os ficheiros já colocados). ART-018 e ART-016 ficam descritos pelo que o relatório diz, nas pendências abaixo | **Por leitura, um único verificador** — subagente `triagem-art`, sessão filha DSH `86671b31-8f18-4756-95bf-f99192578e98`, modelo medido no log da filha: `anthropic/claude-fable-5-2` (116 mensagens), entregue 22:17 Europe/Lisbon; árvore `7211c73`, lote fixado pelo auditor em `8850edd` (`git diff 8850edd 7211c73 -- engine/` vazio declarado no relatório). **Nada foi compilado, executado nem escrito pelo subagente**; nada é «comprovado por teste» excepto o FL-1. Eu **não conferi linha a linha o relatório inteiro**: conferí as contagens (duas fontes), a proveniência no log, o drift entre SHAs, as citações listadas nesta célula e as que sustentam os três refutamentos; nesta correcção, as de ART-016/ART-018/ART-005/ART-007/FL-2/FL-3 e a **ausência** das sete que eu tinha atribuído ao relatório. Relatório **por commitar** à hora desta ronda (`??`) |

### Triagem do lote A-real-time (ids ART) — 17-11 22:17, **por leitura, um único verificador, nada executado**

Fonte: [docs/relatorios/2026-11-17-triagem-art.md](docs/relatorios/2026-11-17-triagem-art.md) (o cabeçalho desse
ficheiro é do coordenador; o corpo está lá verbatim). Subagente `triagem-art`, sessão filha DSH
`86671b31-8f18-4756-95bf-f99192578e98`, `anthropic/claude-fable-5-2`, 116 mensagens. Árvore `7211c73`, lote do
auditor externo fixado em `8850edd` e recebido na sessão DSH `session-f917643e-3439-450a-856b-db4965395354`
(seq 475). Tabela do §2 transcrita como entregue — **não é estado medido por teste**; a gravidade é de leitura e
«por omissão» quer dizer alcançável sem argumento especial.

| id | veredicto | gravidade real | alcançável por omissão? | correcção toca graph.cpp/engine.cpp? |
| --- | --- | --- | --- | --- |
| ART-002 | CONFIRMADO (política M3b, decisão de produto) | médio | sim | sim (graph.cpp) |
| ART-003 | CONFIRMADO no código; runtime por medir | médio | sim | não |
| ART-004 | PARCIAL | baixo | corte audível: sim; intervalo: não | não |
| ART-005 | NÃO DECIDÍVEL POR LEITURA | não medido (estimativa baixa) | sim (o caminho corre) | sim, se medir mal |
| ART-006 | REFUTADO | nulo | não | — |
| ART-007 | NÃO DECIDÍVEL POR LEITURA (dobra) | baixo | sim | sim (2 linhas) |
| ART-008 | CONFIRMADO por leitura; probabilidade baixa | baixo | sim | sim (engine.cpp) |
| ART-010 | PARCIAL (premissa «M5 desligado» falsa) | baixo | **sim** | por omissão: não (server.cpp); (b): sim |
| ART-011 | PARCIAL (mecanismo sim, alcance por medir) | baixo | só com plugin ≥ 32 canais | não |
| ART-012 | PARCIAL | baixo | só se um plugin mexer no MXCSR | não |
| ART-013 | REFUTADO (alcance) | nulo | não | não |
| ART-014 | REFUTADO (alcance) | nulo | não | — |
| ART-015 | CONFIRMADO como latente | baixo | não | não |
| ART-016 | PARCIAL | baixo (nulo pela app) | não pela app; sim por CLI | não |
| ART-017 | PARCIAL | baixo | sim | não |
| ART-018 | CONFIRMADO (uma parte cai) | baixo | sim, com ficheiro partido | não |

**Ordem de ataque proposta pelo relatório §4 — ordem de um verificador por leitura, não ratificada por ti.** Sem
grafos primeiro: FL-1/ART-010 (**corrigido** em `e436ede`), ART-003, ART-018, ART-016, ART-004 + FL-3,
ART-011 + ART-012, ART-015, ART-017. Depois o caminho de áudio: ART-007 (duas linhas, sem efeito em áudio finito), ART-002 (só depois de
decisão de produto, com FL-2), ART-005 (medir antes), ART-008 (teste de carga antes), ART-010(b) (só se o M5
voltar — D51). **Fechar sem acção:** ART-006, ART-013, ART-014 (ver Invalidadas). Quem executa cada item e com que
modelo depende da resposta a **P-D98**; nenhum agente tem autorização por delegação enquanto essa pergunta estiver
aberta.

### Ronda sanitizers + clang-tidy (D92/D93) — medida noutra entrega, conferida por mim nos ficheiros commitados

Fonte: `54addba` (build) e `4cc08b4` (docs+evidência), ambos em `origin/main`. Evidência ancorada em
[`2026-11-17-sanitizadores-evidencia.txt`](docs/relatorios/2026-11-17-sanitizadores-evidencia.txt) (169 linhas),
relatório [`2026-11-17-preflight-sanitizadores-clang-tidy.md`](docs/relatorios/2026-11-17-preflight-sanitizadores-clang-tidy.md)
(PF-01…PF-15 + três adenda) e [`tidy-round1.txt`](docs/relatorios/tidy-round1.txt). **Não re-executei nada**:
conferi totais, somas e contagens por leitura independente.

| Corrida `merge` | Ambiente | Testes | Casos / verificações | Tempo | Reports de sanitizer |
| --- | --- | --- | --- | --- | --- |
| ASan + UBSan | WSL GCC 13 | **15/15** | 113 / 1456, 0 falhas | 116.17 s | ASan=0 · runtime error=0 · LSan=0 · TSan=0 · FATAL=0 |
| TSan | WSL GCC 13 | **15/15** | 113 / 1456, 0 falhas | 228.23 s | idem, todos 0 |
| Normal (sem `CORE_SANITIZE`) | WSL GCC 13 | **15/15** | 113 / 1456, 0 falhas | 71.46 s | idem, todos 0 |
| ASan MSVC 19.51 | **host Windows** | **15/15** | 113 / 1456, 0 falhas | 303.41 s | ASan=0 · runtime error=0 |

Somas por suite conferidas por mim: 20+9+21+8+10+2+7+5+14+7+10 = **113**; 81+64+216+115+49+25+65+66+649+82+44 = **1456**.

| Item | Valor medido | Estatuto |
| --- | --- | --- |
| Instrumentação por árvore | ASan **65/66** TUs · MSVC **65/66** · TSan **64/65**; único de fora: `miniaudio_impl.c`. Tidy 51 TUs no banco (0 instrumentados, build normal); plain 64 TUs | evidência §3; JUCE e `CoreSetupData` fora, declarado |
| Canários (fora do `ctest`) | asan **3/3** exit 1 · ubsan **3/3** exit 1 · tsan **5/5** exit 66, com reports reais (`heap-use-after-free` canary.cpp:10, `signed integer overflow` :20, `data race` :38). `ctest -N`: Total Tests **15**, ocorrências de canary **0** | evidência §2 e §8 |
| Defeito próprio da ronda | o desenho anterior do canário TSan era **flaky** (falhou 3 de 5, exit 0 sem report) → substituído por 200 000 acessos de cada lado libertados por um atomic; binários ligados ao libtsan (`ldd` verificado). Segundo defeito: o `--require` do gate não falhava e escrevia um ficheiro com o nome do fonte exigido | registado e corrigido na própria entrega (commit `4cc08b4`) |
| Recusas do wiring | **5** `FATAL_ERROR` exit 1: MSVC `address,undefined` · MSVC `thread` · mistura `address+thread` · `Release` em vez de Debug · valor desconhecido `bogus` (WSL) | evidência §1 — o D9002 silencioso não volta |
| Gate de scope (`ci/tidy_scope.py`) | exit 0; 52 pedidos → **50 analisados, 2 excluídos, 0 em falta**; controlo negativo **exit 1** (tem de ser 1). Excluídos declarados: `app/engine-client/src/engine_client.cpp` (`#error` no :5, alvo Windows-only) e `engine/src/plugins/juce_host.cpp` (JUCE ausente pela guarda `CORE_BUILD_APP`/`WIN32`) | evidência §9. **São exactamente as duas lacunas que D94/D95 propõem fechar** |
| clang-tidy | WSL LLVM 18.1.3: **766 avisos / 0 erros**; **456 nossos + 310 `json.hpp` vendored**; `tidy-round1.txt` = **6551 linhas**. Top nosso: `graph.cpp` 49, `project.hpp` 44, `edit.cpp` 32, `clip_grains.hpp` 27, `song_clock.hpp` 24 | **conferido por mim**: contagem independente de `warning:`/`error:` e das linhas com `json.hpp` dá 766/0/310 → 456 |
| O que continua não provado | nenhum destes gates abre a app, toca áudio ou prova o runtime Windows dos itens 7/8/15/18/27/28/31; TSan não corre no host (D93), por isso o transporte Win32 fica sem prova de races | limite declarado |

### Ronda do launcher `ci/compilar.bat` — entrega de outro actor, **reportado pela entrega**

Commitada em **`3097c83`** (build: `ci/compilar.bat`, novo, **401 linhas**) e **`7ad8cca`** (docs: relatório +
evidência). Evidência: `docs/relatorios/evidencia/2026-11-17-compilar-gui/`, **14 ficheiros** (8 PNG +
`gate-resumo.txt` + 5 logs), contados por `git ls-tree`. Relatório:
[`2026-11-17-compilar-gui.md`](docs/relatorios/2026-11-17-compilar-gui.md). **Não re-executei nada desta ronda.**

**Correcção ao estado da árvore descrito no relatório:** o §5 e a linha proposta `:63` dizem «wiring de
sanitizers **por commitar**». Isso **já não é verdade** — esse wiring está commitado em `54addba`/`4cc08b4`
(D92/D93). As medições foram feitas sobre a árvore de trabalho em `9d87065` com essas alterações ainda por
commitar, isto é, sobre o conteúdo que hoje é `54addba`. Não copiei a linha `:63` verbatim por causa disto.

| Percurso (conduzido por botões da GUI, Win32 `BM_CLICK`, não por cliques humanos) | Resultado reportado |
| --- | --- |
| Configurar (pasta nova) | exit 0, 24 s, VERDE |
| Config+Compilar | exit 0, **196 s**, ninja **[329/329]**, `Core Setup.exe` ligado |
| Compilar (no-op) | exit 0, 1 s |
| Gate (merge) | exit 0, 10 s, **15/15** VERDE |
| Gate merge+desktop | exit 0, 26 s, **16/16** (`desktop` 1 teste, 16.70 s; `merge` 15 testes, 6.06 s) |
| Parar aos 15 s | exit 1, 17 s, VERMELHO, `cl/ninja restantes: 0` |
| Build com espaços | exit 0, 24 s |
| Build não-ASCII | exit 1, 24 s, `Running juceaide failed`, `JUCEUtils.cmake:630` — limitação do JUCE, não do launcher |
| Guardas GU-01 / GU-02 | recusam sem criar nada e sem tocar na cache alheia |

Contagens por âmbito: `merge` = 15 testes ctest, `desktop` = 1. **Os 113 casos / 1456 verificações não foram
re-medidos aqui** — são da ronda sanitizers acima. Por exercitar: tipo **Debug**, diálogos «…», guarda de
gerador diferente de Ninja. Duas incidências registadas pela própria entrega: capturas de 850 bytes descartadas
(janela minimizada por actor externo) e uma ronda em que o driver de prova fechou por engano uma janela do
Explorador intitulada «CORE» (sem perda de dados; ronda refeita identificando o processo).

Estatuto: **reportado pela entrega** (`3097c83`/`7ad8cca`). Verificado por mim: os SHAs, a contagem de evidência
(14) e o facto do repo sobre labels. Divergência menor: o relatório §29 diz «7 capturas + 3 logs»; a pasta
commitada tem 8 PNG + 6 txt.

### Testes novos e o gate de merge — **facto do repo**, não do launcher (adendum `:68-91`)

Medido pela entrega numa **cópia** da árvore (`cp -a`, 24 MB, em scratch removido no fim; o repo partilhado não
foi tocado), passos T1–T5 conduzidos pelos botões da GUI:

| Passo | Acção | Resultado reportado |
| --- | --- | --- |
| T1 | `prova_novo` (`LABELS merge`) acrescentado antes do configure + Config+Compilar | exit 0, 233 s, VERDE |
| T2 | Gate | **17/17** = 16 merge (15 + `prova_novo`) + `desktop_self_test`; o checkbox «Gate também com a app» tinha ficado memorizado ligado |
| T3 | `prova_dois` (merge) e `prova_sem_label` (sem label) acrescentados **depois** do configure; só `Compilar` | exit 0, 2 s; `tests/CTestTestfile.cmake` passa a conter os dois → o Ninja re-configurou sozinho |
| T4 | Gate (checkbox ligado = todos) | **19/19**, inclui `prova_sem_label` (#19) e `desktop_self_test` (#16) |
| T5 | checkbox desligado; Gate (merge) | **17/17** = 15 + `prova_novo` + `prova_dois`; **sem** `prova_sem_label`, **sem** desktop |

**Conclusões.** (1) Testes novos entram **sem actualizar o launcher** — por Configurar, por Config+Compilar e
até por Compilar sozinho: `build.ninja` tem `RERUN_CMAKE` dependente de `tests/CMakeLists.txt`, e o botão Gate
chama `ci/gate.ps1`, que configura explicitamente em todas as execuções; não há contagens fixas no launcher.
(2) **O gate de merge só corre testes com `LABELS merge`**: um teste novo sem label fica **fora do gate de
merge** (entra no gate com a app, que corre todos). **É desenho actual do repo, não do launcher** — confirmado
por mim em `tests/CMakeLists.txt:90-91` (lista explícita de 15 nomes com `LABELS merge`) e `:97`
(`desktop_self_test` com `LABELS desktop`). Quem acrescentar testes fora dessa lista fica fora do gate de merge.

Evidência: `log-t2.txt`, `log-t4.txt`, `log-t5.txt`, `22-testes-novos.png`. Estatuto: T1–T5 **reportados pela
entrega**; o mecanismo e a lista de labels **confirmados por leitura do código**.

---

### Ronda «flawless» de 18/11 — o que está em `main` (`f6b86dc`): **medido pelas entregas, não re-executado por mim**

Âmbito: **D99** (mandato), ordem **D108**, consolidação/pushes **D107**. Desde `7211c73` (último commit deste
ledger) há **106 commits** em `main` e **17 relatórios novos** no índice, todos com «linhas de ESTADO propostas,
não escritas». Os números abaixo são **reportados pelas entregas** (cada uma com log/evidência em
`docs/relatorios/evidencia/…` e `version == sha`, sem `.modified`); **nada foi compilado, corrido nem gateado por
mim nesta ronda**. O que conferi directamente em `main` está separado no fim.

| Entrega | Commits | O que corrige / prova (vermelho → verde) | Gate reportado | Estatuto |
| --- | --- | --- | --- | --- |
| **FL-1 + ART-003** (17/11, já acima) | `e436ede`, `e7968c8` | M5 OFF por omissão (D51); dispositivo que morre é reportado e reaberto uma vez | 15/15, 115/1464; 15/15, **116/1469** | comprovado por teste (reportado); runtime WASAPI do ART-003 medido depois pela sessão de teste real |
| **Stream dados + estado de plugins + FL-3** (`7211c73..ed4642e`, 23 commits) | D1 `49b8cc3`, D2 `458f621`, D3 `dce6605`, D4 `a48d084`, D5 `c9b83ef`; inglês `5cb1ec5`, critérios `db20d67`; gate Ninja `3ccee9f`; crash em `processBlock` `ffed453`; limite 8 MB→256 MB `0090460`; undo ≥50 passos `31d6486`, tecto 1 GB `88e1d5e`, undo/redo não repõe estado de plugin vivo `d0a31ac`; autosave→documento `fbdbfaa`; estado antes de remover plugin `4360215`; `pluginsChanged` `69bc814`; **FL-3** `47282b5` | «Remove unused» não leva áudio de outras canções; recuperação com PID reutilizado; Save pergunta antes de sobrescrever; `ReplaceFileW` meio-falhado não perde a versão; índice de samples ilegível posto de lado; plugin crashado não pendura o motor; estados grandes não matam o motor; `engine.start` não responde ok sem dispositivo depois de uma troca falhada | merge **17/17, 145 casos / 1741 verif / 0 falhas** @ `ed4642e`; desktop **1/1** (49 passos); full **18/18** | comprovado por teste **de domínio**; **não verificado na app**: diálogos/fluxos D1–D5, Save/Export com plugins reais >8 MB, FL-3 com WASAPI real |
| **Gate: bat único por invocação** (**D114**) | `fffbb41` + cherry-picks (`8bce608`, `cca31d1`, `615dd0d`, `5143390`, `a574da0`, `c0accc1`, `3d5b2a2`, `c064e46`, `20e3cb8`) | `core-gate-<guid>.bat` elimina a colisão de gates concorrentes; incidente medido: 2 gates do lote corrompidos pelo stream SUR, descartados (logs guardados) | 2 gates **concorrentes** VERDE 17/17 (`gate-proof-a/b`) | comprovado por execução (reportada) |
| **Consolidação round 1 — 17 ramos** (**D107**) | 14 `--no-ff` → `29ec5e0`; +3 (`flawless-crash` D `075b6dd`, `flawless-writesafety` `b035584`, `flawless-deadcode2` `3542bba`) → tip final **`fcf38e0`**; `main` = `f6b86dc` (docs: convenção de passos do self-test, 51 transições desde o passo 0) | conflitos de ficheiros partilhados resolvidos preservando todos os lados: **53 ficheiros single-owner byte-idênticos**, **14 hotspots com união verificada**, 3 remoções intencionais de previews | merge **20/20** + full **21/21**, **202 casos / 3059 verificações / 0 falhas** (tip de 14 ramos: 196/2947) | comprovado por teste (reportado), sob `GATE-LOCK` |
| **BUG-SWEEP-01 / 04** | `93ae1bf`, `5a5fe49` | save nunca trunca/move/apaga cópia de recuperação `.saving-N` retida; `addon.json` malformado é recusado com razão, nunca throw | red 2 testes/4 falhas (63 de 64 seeds) → green 21/410; red 2/11 → green 12/69; merge 17/17 @ `8bce608`, **149/2028/0** | comprovado por teste (reportado) |
| **BUG-SWEEP-07 — tempo round-trip** | `43a0d20` | `setTempo` 145→300→145 em edições separadas repõe a canção exactamente; contrato ms-gate (Diogo 2026-11-04) preservado; sound-neutral | red **1692 ticks / 3 patterns / equal=0** (18 falhas/42 checks) → green 3500/1/equal=1; domain 26/190; merge 17/17, **152/1783/0** | comprovado por teste (reportado) |
| **BUG-SWEEP-14 — dev zip** | `6267728` | `core-package` deixava ficheiros `lang`/`patterns`/`pcore` só-no-destino; `remove_directory` antes de cada cópia | teste novo `cli_package_stale`: red @ `ed4642e` (3 seeds sobreviviam) → green; gate **18/18**, 145/1741/0 | comprovado por teste (reportado) |
| **BUG-SWEEP-16 — write safety** | `7ef828d` (gate `3d5b2a2`) | MIDI export / preset / template / roles passam por `writeFileSafely` (read-back + `.bak`); já não respondem sucesso com ficheiro vazio nem sobrescrevem sem backup | red **22 checks em 5 testes** → green 10/129 + 25/168; merge 17/17, **150/1785/0** | comprovado por teste (reportado); Save/autosave por tocar |
| **BUG-SWEEP-17 / 18 — preservação** | `7544b3f`, `e105488` | limpeza lê as cópias `.saving-N` retidas (ilegível = dúvida, nada vai); UTF-8 inválido em nome MIDI não torna a canção inguardável | red 8 falhas → `bug_tests` 24/153; red 15 falhas (`.dump()` lançava) → `midi_tests` 10/150; merge 17/17, **149/1791/0** | comprovado por teste (reportado) |
| **BUG-SWEEP-22…25 — timeline/tempo** | `de72561`, `bea172a`, `9d8083c`, `7ab1859` (só `edit.cpp` + `bug_tests.cpp`) | duplicar pina o início efectivo e parte ramps na fronteira; trocar secções move o tempo com a música; compassos vazios inseridos HOLD o tempo; encurtar deriva endpoint dentro do alcance | red medido por achado (**58 falhas** no total) → green após cada um; merge 17/17 @ `c064e46`, **149/1851/0**; som renderizado medido (sha256 + delta dB) | comprovado por teste (reportado) |
| **BUG-SWEEP-26…28 — patterns** | `21153c3`, `9ae7feb`, `9fd18ed` (só `pattern_rack.cpp`) | Load Template reaplica o swing do slot; metadata malformada toma defaults no consumidor (sem throw); `matchRows` parte-primeiro | red 240→300/330; throw→no-throw; bass 84/lead 36→36/84; merge 17/17, **153/1804/0** (+8/+63 em `bug_tests`) | comprovado por teste (reportado). **D110(26/28) ratifica as políticas**; UI e desfecho de crash não verificados |
| **BUG-SWEEP-29 — Randomize** | `577a0d8` (gate `5143390`) | Randomize só-de-tempo apagava ideias não arranjadas da janela Patterns; used-check agora vê o rack (`edit.cpp:560`) | red 5 falhas (ideia apagada, 0 refs, 0 notas ao reabrir) → green 20 checks (undo repõe exacto; save/reopen mantém); domain 19/148→20/168; merge 17/17, **146/1761/0** | comprovado por teste (reportado) |
| **BUG-SWEEP-30 — roll velocity** | `133430e` (gate `a574da0`) | edições de velocidade escalam o envelope tocado de um roll multi-hit pelo factor realizado da base, proporcional, clamp [0,1] | red **14 falhas** + MIDI exportado byte-idêntico após Louder (hits 32/64/95/127) → green 44/44 (36/66/97/127); merge 17/17, **147/1785/0** (+2/+44); WAVs byte-idênticos (0.00 dB) | comprovado por teste (reportado). **Flags do dono por responder:** factor realizado, piso de endpoint zero, Normalize flatten-vs-shape — D110(30) ratifica o helper partilhado, não estes três detalhes |
| **BUG-SWEEP-32 — ids de menu** | `ca70435` (gate `c0accc1`) | ids do catálogo de plugins disjuntos dos ids de acção por construção (bandas desde 4000, placeholder 99999→−1), ambos via `MenuIds.h`; catálogo NÃO limitado | red **56 falhas** → green **7 casos / 366 checks**, bandas disjuntas 500…5000; gate **18/18**, **152/2107/0** | comprovado por teste (reportado); **clique real** no menu com catálogo 501+ não verificado (prova headless) |
| **Dead code L1 / L2** | `9289b19`, `ed61cea` (**443 remoções / 1 inserção**) | API de domínio sem chamadores (modulator/ducking, A/B TrackSound, sidechain/loop/param helpers, browseSampleRoles, sampleFitInRegion) | suite de caracterização **9 casos / 169 checks verde antes e depois**; gate 18/18 merge, **154/1910/0**, full 19/19; null render **bit-exact** `db9bcbd6…`; 5 templates visíveis + catálogo + alinhamento kick/baixo/pratos preservados | comprovado por teste (reportado); colisão de `git stash` entre worktrees documentada com recuperação |
| **CLEAN-07 + BUG-LEGACY-RISE** | `5a5eced`, `08ddc65` | preview de instrumentos nativos retirado (clips desenham as notas MIDI); acção inactiva «Rise on build-ups» do Stage Core removida | probe Windows reproduzido @ `ed4642e` (`waveform empty=1; notes=4`); null renders bit-exact antes/depois (audio + test-synth, sha256 + cmp); merge 17/17, **145/1792/0**; desktop 49 passos; full 18/18 | comprovado por teste (reportado); **engine/DSP por tocar** |
| **Plugin crash A–D** (**D113** para o D) | A `d434720`, B `81f835a`, C `8fcce35`, D `cc5b874` | A: testes de crash fora de `processBlock` (8); B: guards em getter de parâmetro e janela (red `0xC0000005` → green, 9 testes); C: Restart oferecido após timeouts repetidos (self-test passos 112–114); D: deadlock do `restartComponent` no thread de mensagens — **patch vendored no JUCE pinned** (10 hunks, `ci/patches/juce-failed-plugin-restart.patch`), `markCrashed()`→`markCrashedByHost()`, teste `CORE_TEST_CRASH_RESTART` | D 1/17/0 exit 0; `engine_process` 10/121/0; gate limpo **ambos os modos** VERDE (merge 17/17 + desktop) | comprovado por teste (reportado). **O patch vive como modificação da árvore do JUCE, nunca commitada lá** — pendência de infraestrutura abaixo |
| **Mapas ingleses** | `7bed196` | só documentação: tradução integral dos 5 estudos PT de `e7968c8` para `docs/plan/` | **sem gate** (nenhum código mudou); preservação medida por script: guillemets 32/12/8/35/114, `path:line` 41/52/35/114/9, headings 17/6/8/16/18, multiconjuntos de citações/referências idênticos; verbatim do dono intacto | confirmado por leitura + medição de preservação (reportada); números de linha ancoram `e7968c8`, **não re-ancorados** |
| **Estado medido dos ctests** | `269a7d8`, `3a25a71` | só medição, sem `gate.ps1`: ctest directo nos dirs de gate idle | merge @ `ed4642e` **17/17, 145/1741/0**; @ `eef8db0` (SUR-01) **17/17, 146/1771/0** (engine 26/160, +1/+30); desktop **1/1** em ambos (49 passos, null device, build IDs `ed4642e61b`/`eef8db0b3a`); full **18/18** | comprovado por execução (reportada). **Corrige por referência:** `baseline-suite-counts.txt` engine_process 8/93 é o estado do item-1 (`d434720`+`81f835a`), não `ed4642e` (4/49); `C-selftest-green.txt` é `81f835a.modified` |

**Progressão do denominador — cada número pertence ao seu SHA, não se comparam entre si:**
17/11 sanitizers **15 suites / 113 casos / 1456 verif** → A-009 15/114/1461 → FL-1 15/115/1464 → ART-003
15/116/1469 → 18/11 `ed4642e` **17/145/1741** (+2 suites são testes ctest CLI, +32 casos/+285 verif, **sem
cobertura perdida**) → `eef8db0` 17/146/1771 → tip integrado `fcf38e0` **20 suites / 202 casos / 3059 verif**
(full 21/21). O salto de 15→17→20 suites é reconciliado pelo relatório `estado-ctests`, não é relaxamento.

**O que conferi eu em `main` (`f6b86dc`), por leitura/medição directa nesta ronda:**
`server.cpp:102` «  if (v.empty()) return 0;  // D51: M5 OFF by default…» → **M5 continua OFF**;
`instruments.cpp` com `Silent::render(float*, int) override {}` → **builtins vazios**; **0** ocorrências de
`plugin.retry` em `app/` → **FL-2 aberto**; **21** ficheiros `*.core-template.json` ainda em `main` → a remoção
do L10 está por integrar; `Graph::allNotesOff`/`liveNote` continuam a desreferenciar `t.instrument` **sem guarda**
→ a pendência adiada do null-check mantém-se; há assertions de pico para clips de áudio
(`tests/engine_tests.cpp:267`, `tests/legacy_guard_tests.cpp:200`) mas **o teste de amplitude do demo/measure
(L10) não está em `main`**; **15 ramos por integrar** e `integration/flawless-round2` em movimento
(`caeb980` @ 15:18:23Z, **9 segundos** antes da minha medição); **35 worktrees** + clone separado
`D:/CORE/core-chain-abas`.

**Silêncio dos builtins — contexto, não defeito do motor.** O relatório **não rastreado**
`docs/relatorios/2026-11-17-teste-real-app.md` (outra sessão, por commitar) classifica «o motor não produz áudio»
como P1 crítico. O handoff [docs/plan/README.md](docs/plan/README.md) `:14-16` e a medição do coordenador na DSH
(seq **814**, 18/11 01:17:48.408Z: «**O motor faz som** — medido com dois sintetizadores reais, com os picos
registados. O "silêncio total" era o teste, não o Core») contextualizam: canções/ferramentas que ainda usam
builtins antigos são silenciosas **por construção** desde 16/11, e **D104** reafirma a fronteira. A lacuna real é
**não haver teste de amplitude no gate** para o demo/measure — é o L10 (`flawless-clean` `e5f2fca`, por integrar).
**Não reabrir como P1 do motor.** O que esse relatório mede de válido e fica registado: app abre/Ready com WASAPI
«Auscultadores (USB-C Audio)», 10 min 54 s da canção mais pesada (15 faixas/63 efeitos/31 samples) com
**0 overruns / 0 late callbacks / 0 erros**, 20 stop/play + 3 cargas sem crash/leak, e **ART-003 sem alarme falso**
(`deviceNotification` 0, sem reabertura espontânea).

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
| [docs/relatorios/2026-11-17-fl1-m5-default.md](docs/relatorios/2026-11-17-fl1-m5-default.md) | prova do **FL-1** (default do M5 → 0, vermelho 4/verde 0, gate 115/1464); commitado em `e436ede` 22:31:28Z |
| [docs/relatorios/2026-11-17-triagem-art.md](docs/relatorios/2026-11-17-triagem-art.md) | triagem do lote A-real-time em ids `ART-0NN` — 16 achados por leitura, nada executado; **relatório por commitar** à hora desta ronda (ia `??`), vai no commit desta entrega que não é meu |
| `C:\Users\vk_do\.dsh\sessions\--D-CORE--\session-2ff54f03-8344-40bb-8cb4-54df0601eb69\session.v4.jsonl.zstd` | **fonte da D98** (delegação do dono): `user/message` seq 90, `2026-11-17T22:02:31.431Z`. Ficheiro = 160 frames zstd concatenados, 338 registos; descomprimir só o primeiro frame devolve 188 bytes e induz em erro. Sessões filha citadas: triagem `86671b31-8f18-4756-95bf-f99192578e98`, lote do auditor `session-f917643e-3439-450a-856b-db4965395354` (seq 475). **Relido em 18/11:** o ficheiro cresceu para **957 frames / 1988 registos**, com **34** mensagens do dono em 18/11 — fonte de **D99/D100/D101/D109** e do contexto de **D102/D103/D105** (seq 814, 821, 840, 861, 879, 926, 970, 987, 999, 1014, 1030, 1045, 1330, 1371, 1395) |
| `dmo-app/core-main`, `dmo-app/CORE-MANUAL`, `dmo-app/core-create`, `dmo-app/AICORE` | outros repos; fora do âmbito deste ledger pela instrução «apenas sobre o core nada de aicore e fundmmentals» |
| [docs/plan/README.md](docs/plan/README.md) + os cinco mapas ingleses (`data-loss-map.md`, `plugin-hosting-map.md`, `realtime-map.md`, `dead-code-inventory.md`, `ux-pulse-spec.md`) | **handoff** da ronda de 18/11 e os mapas traduzidos (`7bed196`, em `main`). Mandato do dono, o que está feito, e a ordem de trabalho. Os `path:line` ancoram **`e7968c8`** e não foram re-ancorados. O item 11 («design não aprovado») está **superseded por D102/D103** |
| [docs/relatorios/2026-11-18-consolidation-round.md](docs/relatorios/2026-11-18-consolidation-round.md) | prova da consolidação (**D107**): 14+3 ramos, união verificada (53 byte-idênticos / 14 hotspots), tip final `fcf38e0`, gate **202/3059/0** merge 20/20 + full 21/21 |
| [docs/relatorios/2026-11-18-estado-ctests.md](docs/relatorios/2026-11-18-estado-ctests.md) | **denominadores medidos**: 17/145/1741 @ `ed4642e`, 17/146/1771 @ `eef8db0`, desktop 1/1 (49 passos), full 18/18; reconcilia o 15/113/1456 de 17/11 e corrige por referência `baseline-suite-counts.txt` e `C-selftest-green.txt` |
| [índice de relatórios](docs/relatorios/README.md) — 17 entradas de 18/11 | uma por entrega (gate-unique-bat, maps-english, estado-ctests, sweep-01/04, devzip-14, preservation-17/18, dead-code-L1-L2, tempo-07, timeline-22-25, randomize-29, pattern-26-28, roll-30, menu-32, plugin-crash-followups, consolidation, write-safety-16, preview-rise), cada uma com `evidencia/` verbatim. **Todas propõem linhas de ESTADO; esta ronda escreve-as** |
| `C:\Users\vk_do\.qoder\projects\D--CORE-CORE\613ad19e-456c-4336-8bc7-6496bb5c7a2b.jsonl` | **fonte de D102–D114** (sessão Qoder do coordenador, 2804 linhas): 82 mensagens do dono em **dois canais** — `type=user` com `humanInput` (47) e `type=attachment` com `attachment.prompt[]`/`origin.kind=bridge` (35). Ler só o primeiro perde as directivas UX de 12:53. As linhas citadas neste ledger (L333, L386, …) são números de linha **desse ficheiro** |

**SHAs.** Presente: `b80017e` (cânone). Históricos (D81): `59cf952` (audit Linux), `5445152`/`ecd2c7a`
(organização documental), `599d558` (snapshot da fronteira), `95f411a` (main auditado 16/11), `e9b1707`
(baseline de bugs), `8e816c1`, `b28520e` (Fase 4), `1def47b` (Guardian, pré-reset), `02921ea`/`2d438c9`
(core-create), `1abade52` (stash de D83).

**Fontes das decisões.** Predecessor A = `D:\Core Master\ESTADO.md` (SHA-256 `b65c965b96c1c90f…`, 254 linhas) —
governança, **cânone por número**. Predecessor B = `D:\Qwen work\ESTADO.md` (SHA-256
`a154a264728cd978…`, 1845 linhas; cópia byte-idêntica em `D:\Core Master\historico\ESTADO-2026-11-14.md`) —
motor/Serum, arquivado com prefixo `M-` (D74/D82).

---

## Ronda S de 2026-11-20 — coordenador `lead` (DSH `a69b320fbfab`), autorizado por D115/D116

**Escrito pelo coordenador por instrução direta do dono (D116: «update ao estado… proximo agente a pegar no
projecto tem de saber tudo»), dispensando o escrivão-only D72/D80 nesta entrega.** Append-only: nada acima foi
editado. O escrivão pode reconciliar D115/D116 na tabela principal e este bloco no formato canónico.

### Ponto de partida e commits

- Base: `main @ a9a038430d781959829f8510067ce59afffaad62` (= `origin/main`, pull ff de 86 commits desde
  `8c6837e8`, 19/11). Ambiente consolidado + validação SHA-bound + retoma provada em sessão nova (etapa 0,
  relatório `2026-11-19-pull-a9a0384-environment-refresh.md`).
- **Commits desta ronda** (por commitar → commitados sob D116): `0d7d8ce` Build (ENV-01 + D113 JUCE +
  `ci/review-env`), `587bf5b` Data (S1/S4/S5/S2/S3), `234f956` Engine (S6/S7/S9/timing + registos),
  `aacc6d1` Docs (17 relatórios + evidência + índice). Este commit do ledger segue.
- **Gate final integrado** (`ci/gate.ps1 -Desktop`, Release, pasta nova `gate-final`, D113 verificado):
  **merge 29/29 + desktop 1/1 = 30/30 VERDE**, 531 s. A única falha no run de todos os labels é
  `s7_rt_strict_tests` (label `rt_strict`, **fora de merge**), VERMELHA POR DESIGN = porta de decisão do dono
  (locks MidiEventList, ver abaixo), padrão ENV-03. **version==sha**: re-gate em árvore limpa após o commit
  (pendente no momento desta escrita; o gate acima correu com `.modified` por commitar).
- Denominador: `a9a0384` 21 merge + 1 desktop → **29 merge + 1 desktop** (+8 suites novas; nada removido ou
  enfraquecido; os 18 nomes originais da lista merge preservados; `cli_plugin_crash_render` reescrito para o
  contrato de recusa S1 — mudança sancionada, o assert antigo codificava o defeito).

### O que foi entregue (vermelho→verde, cada um com relatório + evidência)

| Item | O que corrige/prova | Vermelho medido → Verde | Relatório |
| --- | --- | --- | --- |
| **S11** JUCE reproduzível | CMake aplica o patch D113 à cópia descarregada (idempotente) e verifica a local (falha cedo, nunca modifica); prova de patch completo `git apply --check -R` | fetch limpo + local sem patch → C2039 `markCrashedByHost` juce_host.cpp:136; 5 casos verdes, Release crash/restart 3/3+3/3+1/1 | `2026-11-20-s11-juce-reproducible-build.md` |
| **S1** exportação | recusa export com plugin necessário falhado (load/render), nome/pista/razão; bypass≠falha; stems conjunto todo; WAV anterior intacto | export degradado byte-idêntico ao saudável publicado como sucesso; **WAV anterior substituído + backup apagado no sucesso**; regressão 5/54 33→0 + CLI real 5 casos | `2026-11-20-s1-export-refusal.md` |
| **S5** samples | propriedade por hash de conteúdo (cópia E desinstalação); legacy conservador; Audit-27 preservado | editado sobrescrito; ficheiro do utilizador sobrescrito+apagado; manifesto apaga conteúdo do utilizador; 70 checks 29→70/70 ×2 | `2026-11-20-s5-installer-sample-protection.md` |
| **S4** WAV float | rejeição NaN/±Inf/overflow por bit-patterns inteiros à prova de /fp:fast (Debug+Release); mensagem nomeia ficheiro/frame; nada registado | NaN/±Inf raw, overflow f64→float, asset totalmente cached; 16/360 22→0 Debug E Release | `2026-11-20-s4-float-wav-rejection.md` |
| **S2/S3** estado plugins | estado inválido recusado com why via canal missing (original nunca sobrescrito); aviso missing name+why; relinkPlugins ligado (open+rescan, estado+automação preservados) | perda do estado original provada byte-a-byte (captureStates→applyPluginStates no Save); 7/68 16→0 | `2026-11-20-s2s3-state-recovery.md` |
| **3A** ENV-03 | **causa PROVADA** (não é hang): motor sai sempre código 0; Debug ~2.7–3.3 s no leak dump automático do CRT em exit (instâncias leaked por design M1); cliente espera 2000 ms hardcoded → 259 | Debug 3/3 «259», Release 3/3 passa; stacks ao vivo em `_CrtDumpMemoryLeaks`; controlos 0/1/2 leaks | `2026-11-20-env03-debug-259.md` |
| **S6** motor RT | serial≡M5(2)≡M5(4) **bit-a-bit** (4 blocos, um SHA); paralelo provado (workerJobs); watchdog medido | matriz 2/129 verde; watchdog Stop 2205 ms, reset+3000 ms 3008 ms (limite documentado) | `2026-11-20-s6-engine-rt-proofs.md` |
| **S7** motor RT | instrumentação alloc/lock/IO com controlos negativos que disparam; baseline dispositivo real | **0 overruns ≤72 instâncias @64**, rutura em 96; M5 p99 2.3× melhor; **locks ~3.7 pares CS/processBlock + realloc raro no RT** (critério 3 não cumprido) | `2026-11-20-s7-rt-measurement.md` |
| **S8** undo | **medição**: serialização ≈98% do commit (151 ms @32×318KB → ~3.3 ms possível); ceiling só preço dumps (memória real ~1.7×) | probe 10 configs × 3 reps, integridade 0 falhas; proposta shared-blob (não implementada) | `2026-11-20-s8-undo-cost.md` |
| **S9** mute/bloco | **medição**: mute paga CPU total; bloco-128 = latência de gather constante +128 (não chamadas extra); quatro coberturas | 5/194 verde; propostas mute-suspend/block-match (não implementadas) | `2026-11-20-s9-mute-block.md` |
| **Timing** | PDC compensa atraso conhecido exatamente (48k dev 480→0); MIDI sample-accurate (dev 0 ambos SR); automação no grid; paralelo M7a alinhado; mudança de latência re-alinha | 8/577 verde; **defeito clip-offset descoberto** (ver abaixo) | `2026-11-20-timing-latency-proof.md` |
| **VST3 host** | EchoBoy/Decapitator 5.5.5: scan/load/editor/bypass/render/automação/cauda PASS; **defeito recall** (ver abaixo); VST2 recusado pelo engine | cauda 5.49 s/11 ecos; recall headless→defaults, editor-init restaura | `2026-11-20-vst3-host-soundtoys.md` |
| **Chain série** | série EB→DEC ≠ inversa; bypass isola; save/reopen bit-idêntico; duplicados independentes; paralelo M7a alinhado; sidechain M7c dormente | **defeito clique em edições live** (ver abaixo) | `2026-11-20-chain-series.md` |
| **CSV dono** | 10 achados confirmados @ a9a0384 (9 confirmados + 1 parcial) | só leitura; mapeiam para S1/S2/S3/S4/S5/S6/S8/S9/S11 | `2026-11-20-csv-10-findings-confirmation.md` |

Coordenação completa: `2026-11-20-round1-coordination.md` (gate, mapa de commits, decisões consolidadas,
manutenção de disco, addenda datados).

### Testes criados — finalidade e como correr (o próximo agente precisa disto)

Todos em `tests/`, registados em `tests/CMakeLists.txt` (LABELS merge salvo indicação). Correr:
`ctest --test-dir <build> -L merge -V` (gate: `ci/gate.ps1 -Build <dir> [-Desktop]`). Build de referência:
`D:\Core-build\gate-final` (Release) ou `D:\Core-build\s1-fix\build` (Debug integrado).

| Suite | Finalidade | Contagens | Notas |
| --- | --- | --- | --- |
| `export_plugin_failure_tests` | S1: recusar export com plugin necessário falhado (load E render), nome/pista/razão; bypass≠falha; stems conjunto todo; WAV anterior byte-idêntico na recusa | 5/54 | in-process offline (fake providers) + CLI plugins reais via `cli_plugin_crash_render` |
| `wav_float_tests` | S4: rejeitar NaN/±Inf/overflow float WAV na importação, mensagem com ficheiro/frame/canal/tipo; AssetStore nada regista; válidos bit-idênticos | 16/360 | **corre em Debug E Release** (/fp:fast incondicional); fixtures gerados no teste |
| `installer_samples` | S5: fluxo silent real do `Core Setup.exe` — samples editados/do utilizador protegidos na reinstalação E desinstalação; propriedade por hash; legacy sem source nunca apaga; Audit-27 | 70 checks | teste cmake (`installer_samples.cmake`), RUN_SERIAL, guard WIN32+APP+CoreSetup |
| `state_recovery_tests` | S2/S3: aviso missing name+why; relink preserva estado+automação/no-match recuperável/duplicados; estado inválido reportado + original preservado através de save/reopen | 7/68 | serve harness + EngineSession snapshot + save/reopen |
| `m5_bitexact_tests` | S6: serial≡M5(2)≡M5(4) bit-a-bit em blocos 128/333/512/1024; prova de uso do paralelo (workerJobs); controlo de determinismo | 2/129 | M5 opt-in RAII (D51 default OFF intacto); plugins reais determinísticos |
| `s7_rt_probe_tests` | S7: wiring do detetor alloc/lock/IO + 3 controlos negativos (TÊM de disparar) + caracterização do caminho do motor (contagens como dados) | 6/77 | seam `core::rtProbe` (device.hpp, null por omissão = 1 relaxed load); IAT hooks + new/delete overrides |
| `s7_rt_strict_tests` | S7: porta ESTRITA 0/0/0 (locks/allocs/IO no audio thread) = **decisão do dono** | 1/12, **VERMELHO por design** | **label `rt_strict`, FORA de merge**; verde só quando os locks MidiEventList forem removidos ou aceites |
| `s9_mute_block_tests` | S9: mute paga CPU total (gate só no fader); bloco-128 = latência gather +128 constante; quatro coberturas (cauda drena/sidechain dormente/PDC invariante/retoma bit-exact warm sem clique) | 5/194 | caracterização; propostas mute-suspend/block-match pendentes |
| `timing_proof_tests` | timing: PDC antes/depois (dev 480→0), independência de bloco, serial≡paralelo, MIDI sample-accurate (dev 0 ambos SR), automação grid 32, paralelo M7a alinhado, mudança de latência re-alinha, determinismo; **+ pin OPEN do defeito clip-offset** | 8/577 | o caso PDC estrito é 48k; o clip-offset 44.1k é pin nomeado (flip quando corrigido) |
| `engine_process_tests` (+3 casos) | S6 watchdog: Stop com plugin preso 2205 ms (bounded), reset+300 ms 332 ms, reset+3000 ms 3008 ms (limite: não interrompe código de plugin dentro do reset) | +3 casos | Debug: caso crash/retry falha com ENV-03 259 (pré-existente, causa provada, correção pendente) |
| `cli_plugin_crash_render` (reescrito) | S1: contrato de recusa (anti-hang TIMEOUT + prova de crash real + controlo saudável) | 1 | mudança de contrato sancionada (o assert antigo codificava o defeito) |

Ambiente (fora do ctest): `ci/review-env/{run-all.ps1,run-wsl.sh,verify.ps1}` — validação SHA-bound
(fp1 sha+local+juce+script+cfg), gate de checkout WSL rígido, provas de reuso/invalidação. Ver
`ci/review-env/README.md` e `2026-11-19-pull-a9a0384-environment-refresh.md`.

### Defeitos descobertos (propostos, NÃO corrigidos — decisão do dono)

- **BUG-RT-LOCKS** (S7): `juce::MidiEventList` `Array<Vst::Event,CriticalSection>` (juce_VST3Common.h:1289,
  JUCE estática) → ~3.7 pares CS/processBlock + realloc raro no audio thread/workers. Critério 3 da barra
  («no lock on the audio thread») não cumprido como escrito. CS não-contendido = fast path user-mode.
  Proposta: patch vendored (array single-threaded/lock-free + capacidade pré-reservada) ou aceitar. Porta
  `rt_strict` vermelha até decisão. Precedente D113 para vendored JUCE.
- **BUG-TIMING-CLIP-OFFSET**: clip `offsetSeconds=0` em start fracionário → `c.offset=llround(0.5)=1`
  (graph.cpp:438) salta frame 0 do WAV (impulso @44.1k silencioso). PDC correto; é o rounding do offset.
  Proposta: offset 0 quando offsetSeconds==0, ou floor. Pin OPEN no `timing_proof_tests`.
- **BUG-CHAIN-EDIT-CLICK**: edições live reorder/bypass trocam o caminho sem ramp/crossfade
  (graph.cpp:641-649/:868) → transiente (tailMaxDiff 0.340); estado final correto (bit-idêntico). Proposta:
  ramp/crossfade como no mute. Sem gesto reorder na app hoje.
- **BUG-VST3-ST-RECALL**: estado Soundtoys capturado sem move com editor aberto → defaults no reopen (ambos
  os plugins); editor-init restaura; fluxo Core são (controlo bit-idêntico). Atribuição suspeita plugin-side,
  **não confirmada** (segundo host por fazer — Bitwig/Ableton/Cakewalk disponíveis). Impacto: configuração
  só host-side (macros/randomize/live writes) perde settings. 4 mitigações propostas.
- **BUG-S8-MEM-ACCOUNTING**: ceiling do undo preço só os dumps dos passos; memória real ~1.7× o nominal
  (gap 158 MB @t64_s318128, peak WS 1727.6 MB vs 1 GB). Proposta shared-blob (COW + contadores) com as 3
  provas (independência/integridade/limite). Conservação do ceiling verificada (D-a refutado).
- **ENV-03** (3A): causa provada (CRT leak dump ~3 s > espera 2 s do cliente, engine_client.cpp:333).
  Proposta A (1 linha: esperar closeTimeout — recomendada), A2 (default app 2000 ms), B (leak-check off Debug).

### Decisões pendentes do dono (nenhuma bloqueia provas; todas para implementação)

D-commit (feito sob D116; falta re-gate version==sha + push) · ENV-03 A/A2/B · RT-LOCKS (patch vendored vs
aceitar) · M5 default-ON (soak 8 h + bit-a-bit no PC do dono, D51; bit-a-bit offline já provado) · S8
shared-blob · S9 mute-suspend (política de cauda) + block-match (latência vs amortização) · VST3-RECALL
(mitigação + segundo host) · CHAIN-EDIT-CLICK (ramp) · CLIP-OFFSET (offset 0/floor) · **D-CHAIN-ARCH**
(sidechain M7c para plugins não implementado — decisão de arquitetura numerada; paralelo M7a bus já existe e
alinha) · Chain parâmetros (Delay Drive → Mix+Drive, sistema de macros) · S10 VST2 (caso concreto + SDK/licença;
engine recusa vst2, project.cpp:387) · S12 restos/Q8 (sample_host dormente; projeto antigo + render de
referência antes de remover) · S13 UX (revisão do dono à slice A) · menores: retenção de backup no sucesso do
publish, `.part` legacy CLI (SWEEP-02), stems crash-atómicos, starvation exclusive (hardware), subnormais
f64→f32, modified() dirty-flag (08).

### Ambiente e reprodução (o próximo agente)

- Scripts no repo: `ci/review-env/` (cópia canónica de trabalho `D:\Core-build\core-review`).
  `powershell -NoProfile -ExecutionPolicy Bypass -File ci\review-env\run-all.ps1` (Windows),
  `wsl.exe -e bash ci/review-env/run-wsl.sh` (WSL), `verify.ps1` (health check read-only, NÃO é suite).
- Builds out-of-tree: `D:\Core-build\core-review\{debug,asan}`; WSL `~/core-review-build/{debug,asanubsan,tsan,tidy}`.
  Gate: `ci/gate.ps1 -Build <dir> -Desktop` (Release, merge+desktop). Builds desta ronda em
  `D:\Core-build\{gate-final,s1-fix,s2s3,s6,s7,s9,timing(C:),chain-series,vst3-host}` (árvores superseded
  libertadas — 56 GB, manutenção registada na coordenação).
- JUCE: `D:\Core-deps\JUCE-9.0.5` com patch D113 aplicado (CMake aplica/verifica desde S11).
- Pré-requisitos: VS 18 BuildTools (vcvars64), CMake ≥4.4, Ninja; WSL2 Ubuntu 24.04 GCC 13/clang-tidy 18
  (sem gdb, sem rede apt); TSan precisa `setarch -R` (automático no run-wsl.sh).
- Evidência verbatim: `docs/relatorios/evidencia/2026-11-20-*/` (logs, CSVs, probes, WAVs, stacks, transcripts).

### Retoma — por onde o próximo agente começa

1. Este bloco + `docs/relatorios/2026-11-20-round1-coordination.md` (mapa de commits, decisões, addenda).
2. `ci/review-env/README.md` + `verify.ps1` para o ambiente; `ci/gate.ps1` para o gate.
3. Relatórios por item (tabela acima) para o detalhe vermelho→verde de cada correção/prova.
4. **Decisões pendentes** (acima) — implementar só com autorização do dono (D48/D50); arquitetura nova exige
   decisão numerada. Os defeitos descobertos estão medidos e propostos, não corrigidos.
5. Trabalho de outras sessões por commitar preservado: 2 linhas README 17/11 (Plano interface / Teste real) +
   relatórios/evidência 17/11; `.qoderignore`, `environment.yml` pré-existentes. Não commitar sem dono.
6. O que NÃO foi feito/verificado: dispositivo real sob ASan/VST3 reais; soak M5 8 h; UI click-through das
   correções (headless + código); segundo host VST3; sidechain/Chain-parâmetros (decisão); S10/S12/S13.
   Linux não prova Windows e vice-versa (AGENTS.md).

