# MIGRAÇÃO do ledger — mapa old→new (D73, ratificada 17-11-2026 19:17)

Página do escrivão. Regista como os números antigos de três fontes diferentes passam a ler-se no ledger
canónico de [`ESTADO.md`](../../ESTADO.md). **Nada aqui apaga nada**: é mapa, e os ficheiros-fonte ficam onde
estão, arquivados por referência (caminho + SHA-256 + tamanho + data).

Ordens do dono que mandam nesta página, verbatim (17-11-2026 19:17, chat desta sessão,
`D--CORE/61b1abc0-db5c-4d05-989e-4ff6c7788120.jsonl` L335):

> «C1/C2: mantenho D72/D73 como ditei. Predecessor B (motor/Serum) = âmbito separado arquivado, prefixo M-;
> cânone por número = predecessor A. D58–D71 reservados como IDs de proposta/pendente com mapa documentado,
> nunca ratificados sem palavras minhas; próximas ratificações D74+.»
> «C10: predecessor B arquivado; governança vive em A; nada de B vivo por omissão.»
> «D73: novo ESTADO.md canónico por reconciliação, não reset. Numeração contínua ou mapa old→new documentado;
> invalidações e pendentes carregados; citações+fonte obrigatórias; SHAs re-ancorados ou históricos; antigos
> arquivados.»

---

## 1. Fontes arquivadas (medido em disco, 17-11-2026 19:20)

| Fonte | Caminho | SHA-256 | Tamanho / linhas | mtime | Destino |
| --- | --- | --- | --- | --- | --- |
| **Predecessor A** — ledger de governança (D40–D57 + Proveniência + A01–A29 + T01–T33) | `D:\Core Master\ESTADO.md` | `b65c965b96c1c90f5854c62c44477442a69f9ec0dc6316ba8009edb56ef8569d` | 37 254 bytes · 254 linhas | 2026-11-16 14:08 | **cânone por número**: D40–D57 carregados vivos; A/T/G → pendentes ou histórico |
| **Predecessor B** — ledger do motor/Serum (D01–D19 «trancadas», entradas até D140) | `D:\Qwen work\ESTADO.md` | `a154a264728cd978589b0ad4beccc91a668d5a8797867995a2047cb79329e32a` | 241 393 bytes · 1845 linhas | 2026-11-14 10:40 | âmbito separado **arquivado**, prefixo `M-`; nada vivo por omissão |
| Cópia do B dentro do A | `D:\Core Master\historico\ESTADO-2026-11-14.md` | `a154a264728cd978589b0ad4beccc91a668d5a8797867995a2047cb79329e32a` — **idêntica ao B** | 241 393 bytes · 1845 linhas | ver B | confirma que o B já estava arquivado dentro do A |
| ESTADO do repo antes da reconciliação | [`ESTADO-2026-11-17-anterior.md`](ESTADO-2026-11-17-anterior.md) | `ffa3a52eb3f5a7878dadc955dc1ded460347d8b0991e414eb42f9c0e91b1d1dc` | 8 638 bytes · 72 linhas | 2026-11-17 | arquivado; conteúdo carregado para o canónico, não apagado |
| Rascunho de decisões feito por mim antes do papel de escrivão (AP-01…AP-35) | [`DECISOES-rascunho-2026-11-17.md`](DECISOES-rascunho-2026-11-17.md) | `a45c875d1bb63f1236520109b57688219cefb80a711e7dca2a70e872c51d5413` | 11 617 bytes · 85 linhas | 2026-11-17 18:54 | **rascunho**, sem efeito de ledger (D79) |
| Colheita de turnos do dono (134 mensagens, 30/10→17/11) | `D:\CORE\.escriva\owner-turns-core.txt` | `e578b3a4a09c8b62…` | 59 673 bytes · 988 linhas | 2026-11-17 19:01 | prova de trabalho, fora do repo |
| Rascunho de reconciliação confirmado | `D:\CORE\.escriva\RECONCILIACAO-RASCUNHO.md` | — | — | 2026-11-17 | confirmado em 19:17; citado no canónico |

## 2. Mapa por namespace

| Origem | Números que existem | Como se lêem agora |
| --- | --- | --- |
| Predecessor A (governança) | D40–D57 | **vivos no canónico com o mesmo número** (D84) |
| Predecessor A (auditoria) | A01–A29 | A02/A03/A05/A07/A29 já resolvidos por D45/D52; resto em **Pendentes**; **A09 fica pendente** (D77) |
| Predecessor A (plano/guardian) | T01–T33, G01–G20 | histórico ligado ao snapshot, **não** constraint viva (D82) |
| Predecessor B (motor/Serum) | D01–D19, D36–D140 | `M-D01`…`M-D140`, âmbito separado arquivado (D74/D82) |
| Bugs de 16/11 | D1, D2, D4, D6 | `BUG-1`, `BUG-2`, `BUG-4`, `BUG-6` — namespace de bugs, não ocupam D (D75) |
| Coordenador | D58–D66 | **propostas pendentes**, sem efeito até ratificação (D76) |
| Reserva | D58–D71 | IDs de proposta/pendente; **nunca ratificados sem palavras do dono** (D74) |
| Dono, 17/11 | D72, D73 | mantidos como ditados (D74) |
| Dono, 17/11 19:17 | — | ratificações novas a partir de **D74** (D74…D91) |
| Repo actual | PROD-01…PROD-10 | âmbito de produto, **mapa para D### sem renumerar** (D78); pendentes excepto as partes com verbatim (D89) |
| Repo actual | P-F01…P-F06, D-AI-fora, D-DEFER1 | propostas/pendentes; os IDs sem formato `D###` **mapeiam** para D45/D47/D52/D53 (D77) |

## 3. Colisões resolvidas (as 11, uma linha cada)

| # | Resolução do dono (19:17) | Entrada no canónico |
| --- | --- | --- |
| C1/C2 | mantém D72/D73; B = `M-` arquivado; cânone por número = A; D58–D71 reservados; D74+ | D74 |
| C3 | bugs = namespace `BUG-` | D75 |
| C4 | D58–D66 = propostas pendentes | D76 |
| C5 | A09 pendente; D-AI-fora/D-DEFER1 → D45/D47/D52/D53 | D77 |
| C6 | PROD = âmbito de produto com mapa, não renumera D | D78 |
| C7 | `docs/DECISOES.md` arquivado como rascunho; ESTADO single writer | D79 |
| C8 | emendar AGENTS/README: entregas propõem via pendentes+relatório | D80 |
| C9 | re-ancorar links/SHAs a `d-aicore/core`; anteriores históricos | D81 |
| C10 | B arquivado; governança vive em A; nada de B vivo por omissão | D82 |
| C11 | master vertical/selecção = rollback/stash; re-land por decisão | D83 |

## 4. Superfícies UI/DSP retiradas da árvore (C11 → D83)

Medido antes da reversão (`git status --short` sobre `b80017e`): modificados `ESTADO.md`,
`app/desktop/CMakeLists.txt`, `src/EngineSession.{cpp,h}`, `src/InspectorPanel.{cpp,h}`,
`src/MainComponent.{cpp,h}`, `src/PatternEditor.{cpp,h}`, `docs/relatorios/README.md`; por comitar
`src/DspPerformance.{cpp,h}`, `.qoderignore`, `docs/DECISOES.md` e 4 relatórios de 17/11.

| Recuperação | Onde |
| --- | --- |
| commit de stash (diff dos ficheiros comitados) | `refs/escriva/unratified-2026-11-17` = `1abade528f6fe506152311183d55e38ada681288` (pai: `b80017e`) — 506 inserções / 129 deleções em 11 ficheiros |
| cópia em disco (inclui os ficheiros não comitados: `DspPerformance.*` e os 4 relatórios) | `D:\CORE\.escriva\unratified-2026-11-17\` (ex.: `desktop\src\MainComponent.cpp` = 403 213 bytes vs 384 744 em `b80017e`) |
| relatórios de 17/11 | ficam em `docs/relatorios/` como evidência; o índice diz que o código **não** está na árvore |

O `git stash` normal **não** foi usado (worktree partilhado): `git stash create` + `update-ref` num ref próprio.
