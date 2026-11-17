# Relatórios

Cada relatório pertence ao snapshot/ambiente que identifica. Para o presente, consultar [ESTADO](../../ESTADO.md). Procedimento: [AUDITORIA](../AUDITORIA.md); formato: [MODELO](MODELO.md).

| Data | Assunto | Evidência |
| --- | --- | --- |
| 2026-11-17 | [Organização do repo](2026-11-17-organizacao-repo.md) | Documental; sem testes do produto |
| 2026-11-17 | [Arredura profunda](2026-11-17-auditoria-core.md) | Medições Linux reportadas; limitações Windows explícitas |
| 2026-11-17 | [Resumo da arredura](2026-11-17-RESUMO.md) | Resumo histórico do relatório anterior |
| 2026-11-17 | [Criação de música → core-create](2026-11-17-core-create.md) | Notas datadas mudam a entrega: 1) markdown apenas, 0 JSON (erro meu no `INDICE.md` registado lá); 2) **a fonte certa é o CORE-MANUAL `8dc3a33`, não o core-main** — repo refeito como técnicas e métodos por estilo, psytrance incluído, `core-create main` = `02921ea`; extracção anterior protegida na tag `antes-core-manual`; 3) correcção de números do ponto 2 (psytrance são 12 páginas, e as 1595 linhas/141 KB são só as 62 páginas de conteúdo) |
| 2026-11-17 | [Pré-flight do plano tidy + ASan/UBSan/TSan](2026-11-17-preflight-sanitizadores-clang-tidy.md) | Medido em sondas: `CORE_SANITIZE` não existe no build, `cl.exe` ignora `undefined`/`thread` com exit 0, clang-tidy ausente no host, `juce_host` fora do banco com app OFF, tidy cobria 49/65 TUs. Nenhum passo do plano executado; sem implementação |
| 2026-11-17 | [Gate Windows](2026-11-17-gate-windows.txt) | Log guardado por ordem do dono (D90): VERDE 15/15 `merge` @ `b80017e`, SHA-256 dos ficheiros originais. **Com alterações por commitar** — não é gate de árvore limpa |
| 2026-11-17 | [Primeira leitura UI/UX](2026-11-17-ui-ux-primeira-leitura.md) | Leitura estática em `b80017e`, propostas; sem implementação nem testes |
| 2026-11-17 | [Master vertical e seleção](2026-11-17-master-vertical-selecao.md) | Reporta build Release 16/16 e captura; **o código descrito está fora da árvore** — stash `refs/escriva/unratified-2026-11-17` por ordem do dono (D83). Re-land só por decisão numerada |
| 2026-11-17 | [Refinamento visual e DSP Performance](2026-11-17-refinamento-ui-dsp.md) | Reporta 16/16 em Windows e gráfico em WASAPI; **código fora da árvore** (D83, mesma ordem e mesmo stash) |
| 2026-11-17 | [Maqueta da interface](2026-11-17-maqueta-interface.md) | Captura e oito percursos em Edge headless; proposta, **sem código na árvore** (D83) |
| 2026-11-17 | [Ledger canónico — reconciliação](2026-11-17-ledger-canonico.md) | Só documentação/governação: ESTADO canónico (D40→D91, 57 citações verificadas verbatim), arquivo em `docs/historico/`, AGENTS/README emendados (D80), work UI/DSP em stash recuperável (D83), log do gate ancorado (D90) e **gate re-executado: 15/15 verdes @ `eb9d847`**. Nenhum código de produto alterado; colisão C12 registada |

Acrescentar novas entradas aqui; não substituir relatórios antigos. Relatório é evidência ligada ao snapshot que
identifica: manter o texto e os números originais, corrigir por cima com linha datada se algo se revelou falso.

**ESTADO.md não se edita aqui (D72/D80).** O índice e o relatório são a proposta; as linhas de
[`ESTADO.md`](../../ESTADO.md) escreve-as só o escrivão do ledger. O que uma entrega quer visto escreve-se
no relatório e pede-se como pendência (owner = Diogo).
