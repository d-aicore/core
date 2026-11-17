# Ledger canónico do Core — reconciliação executada, 17-11-2026

Âmbito: **só documentação e governação**. Nenhum código de produto foi alterado nesta entrega; o código da
árvore voltou a ser exactamente `b80017e` por ordem do dono (D83).

Autorização: confirmação única do dono em 17-11-2026 19:17 (D73), verbatim em
[`ESTADO.md`](../../ESTADO.md) e no mapa [`docs/historico/MIGRACAO-LEDGER.md`](../historico/MIGRACAO-LEDGER.md).
Fonte primária do turno: `D--CORE/61b1abc0-db5c-4d05-989e-4ff6c7788120.jsonl` **L335**,
ts `2026-11-17T19:17:54.706Z`.

## O que foi feito

| # | Acção | Ordem que a manda | Prova |
| --- | --- | --- | --- |
| 1 | `ESTADO.md` reescrito como ledger canónico com a estrutura ditada: Decisões/Proveniência, Pendentes (owner=Diogo), Invalidadas (por referência), Estado medido, Ligações | D72/D73 | 203 linhas; 36 entradas `D###`; numeração contínua D40→D91 sem buracos não explicados (verificado por script); 25 links relativos, **0 partidos** |
| 2 | D40–D57 carregadas do predecessor A com citação verbatim, linha-fonte e id de mensagem; reserva da D56 levantada | D84 | citações conferidas contra `D:\Core Master\ESTADO.md` (linhas 41–58); D56 confirmada por «confirmo que ENVIEI o rascunho, é meu» |
| 3 | Ratificações novas **D74…D91**, uma por linha da confirmação (C1–C11, §1.1, §1.2, definição do Core, remodelação, ordem de 14/11 sobre o DSP próprio, §2 PROD, §5 gate, Q9 hosting da AI) | D74 («próximas ratificações D74+») | cada linha cita L335 verbatim |
| 4 | `docs/historico/` criado: rascunho `DECISOES.md` arquivado, página anterior do ESTADO arquivada, mapa old→new com SHA-256 das fontes | D73/D79/D82 | `DECISOES-rascunho-2026-11-17.md` (`a45c875d…`, 85 linhas), `ESTADO-2026-11-17-anterior.md` (`ffa3a52e…`, 72 linhas), `MIGRACAO-LEDGER.md` |
| 5 | `AGENTS.md`, `docs/README.md` e `docs/relatorios/README.md` emendados: entregas **propõem** via pendentes+relatório; só o escrivão escreve `ESTADO.md` | D80 | secção «Ledger» nova em AGENTS.md; «Entrega obrigatória» deixou de mandar actualizar ESTADO.md |
| 6 | Work UI/DSP não ratificado retirado da árvore, **recuperável** | D83 | stash-commit `1abade528f6fe506152311183d55e38ada681288` em `refs/escriva/unratified-2026-11-17` (506+/129−, 11 ficheiros) + cópia em `D:\CORE\.escriva\unratified-2026-11-17\`; `git diff` sobre `app engine tests ci patterns` = **vazio** |
| 7 | Log do gate Windows guardado com proveniência | D90 | [`2026-11-17-gate-windows.txt`](2026-11-17-gate-windows.txt): SHA-256 `09ca8211…` (resumo, 180 bytes) e `88b163f5…` (gate.log, 96 698 bytes UTF-16LE), mtime 18:43 |
| 8 | Proveniência dos permalinks da fronteira declarada histórica | D81 | `git cat-file -t 599d558` → *Not a valid object name* (idem `59cf952`, `5445152`, `ecd2c7a`, `95f411a`, `e9b1707`, `b28520e`); só `b80017e` existe neste repo. Nota inserida em `docs/FRONTEIRA-CORE-AICORE.md` §4; **não** reescrevi os links para o novo nome porque dariam 404 |

## O que NÃO foi feito (dito à maneira da regra da honestidade)

- **Nada foi implementado.** Nenhuma linha de `app/`, `engine/`, `tests/`, `ci/` ou `patterns/` mudou.
- **À data da primeira escrita deste relatório, nenhum teste do produto tinha sido corrido.** Correcção datada
  (19:37, depois do commit `eb9d847`): o gate foi re-executado em pasta nova — **15/15 verdes**, ver adendum.
  Continua verdade que **a app não foi aberta** e que nenhum self-test desktop correu.
- Os relatórios de 17/11 sobre UI/DSP **não foram reescritos**: ficam como evidência do que foi feito, com o
  índice a dizer que o código está fora da árvore (D83).
- O `docs/relatorios/2026-11-17-preflight-sanitizadores-clang-tidy.md` apareceu por comitar na árvore durante
  esta sessão (outra entrega a escrever no mesmo checkout). **Não o comitei** e não o alterei.
- O chat primário das citações D40–D57 **não foi re-verificado**: valem pela Proveniência do predecessor A,
  como o dono autorizou em D84 («primário não re-verificado; reabre se contradito»).

## Colisão nova registada, não resolvida por mim

**C12** — chegou texto de decisões sobre sanitizers com os números **D75/D76**, já ocupados pelas ratificações
de 19:17 (namespace `BUG-` e folha do coordenador). Verbatim e fonte (L616, ts `19:30:21.934Z`) registados em
`ESTADO.md` → Pendentes → Colisão em aberto. Próximo número livre: **D92**. Nada foi ratificado nem
implementado a partir desse texto.

## Pendências que esta entrega deixa abertas (owner = Diogo)

1. Re-land das três superfícies UI/DSP em stash — cada uma precisa de decisão numerada (D83).
2. C12 acima.
3. As 8 perguntas do RESUMO do audit que continuam sem resposta (item 6, itens 16/17, timeout do item 9,
   autoria de `docs/contracts/core-calls.md`, CI, preço da hash 3a, PR #25/D58).
4. PROD-01…PROD-10 na parte sem verbatim (D89).

## Verificação das citações (feita por script, não por olho)

Cada fragmento entre «» de cada linha `D###` foi comparado por substring exacta com a fonte declarada:

| Bloco | Fragmentos | Ausentes na fonte |
| --- | --- | --- |
| D40–D57 (fonte: `D:\Core Master\ESTADO.md`) | 42 | **0** |
| D74–D91 (fonte: turno L335) | 15 | **0** |

**Correcção feita nesta entrega:** na primeira escrita, 5 fragmentos (D48 ×2, D49, D50 ×2) tinham perdido os
marcadores `**` da fonte ou trocado «declarem»/«declarar». Foram repostos exactamente como estão em A e
re-verificados. Registo aqui porque a regra é verbatim, e uma citação quase-verbatim é uma citação falsa.

Numeração: D40→D91 contínua; D43/D44 explicadas como não-decisões; D58–D71 reservados (D74). Buracos não
explicados: **0**. Links relativos do ESTADO: 25, partidos: **0**.

## Adendum — gate Windows em árvore de código limpa (medido às 19:37)

Pedido por D90 («re-correr em árvore limpa depois do commit»). Corrido **depois** do commit `eb9d847`, em pasta
de build nova para não reutilizar objetos da árvore suja das 18:43:

```
powershell -NoProfile -ExecutionPolicy Bypass -File ci/gate.ps1 -Build "D:/Core-build/gate-CORE-limpa"
```

| Medição | Valor |
| --- | --- |
| Resultado | **VERDE — 100% tests passed out of 15** (label `merge`), `rc=0` |
| Snapshot | `eb9d847` (código idêntico a `b80017e`; `git diff` sobre `app engine tests ci patterns` = vazio) |
| Build | completo, 925 passos, `D:/Core-build/gate-CORE-limpa` |
| Tempo | 228 s de gate; 6.90 s de execução de testes (6.27 sec*proc) |
| Por teste | 0.03–2.28 s; os mesmos 15 nomes das 18:43, todos `Passed` |
| Proveniência | `gate-resumo.txt` 186 bytes SHA-256 `c182ef1d…`; `gate.log` 96 770 bytes SHA-256 `43ce9015…` — texto integral em [`2026-11-17-gate-windows.txt`](2026-11-17-gate-windows.txt) |

**Ressalva que o script imprime e que não é do código:** «com alteracoes por commitar». `ci/gate.ps1:56` usa
`git status --porcelain`, que conta ficheiros não comitados. Nesse momento estavam por comitar as correcções
documentais desta entrega e dois ficheiros de outra origem (`.qoderignore`,
`docs/relatorios/2026-11-17-preflight-sanitizadores-clang-tidy.md`). O código estava limpo — medido.

**O que este gate não prova:** que a app abre, toca, guarda/reabre ou exporta. Não corri `-Desktop`, não abri o
Core, não ouvi áudio. Os itens 7/8/15/18/27/28/31 do audit continuam sem runtime medido.
