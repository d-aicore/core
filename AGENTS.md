# Regras de trabalho — core (d-aicore/core)

## Autoridade e âmbito

- Ler README.md, ESTADO.md e os documentos de produto/fronteira antes de trabalhar.
- A instrução actual do dono prevalece. Documento histórico, comentário de código, PR ou sugestão de coordenador não ratifica uma decisão.
- Produto: docs/DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md. Processos/dependências/persistência/AI: docs/FRONTEIRA-CORE-AICORE.md.
- PROD-01…PROD-10 são âmbito de produto com mapa para D### e **não renumeram** decisões (D78); entram como pedidos **pendentes** excepto a parte coberta pela definição do Core de 17/11 (D86/D89). P-F* são propostas. D58–D66 são propostas do coordenador sem efeito até ratificação (D76); D67–D71 estão reservados (D74).
- Ratificada só com citação verbatim do dono + data + fonte. Silêncio ≠ aprovação (D48/D50).
- Hosting da AI: processo separado + painel cliente fino, descoberto como add-on por `addon.json`; **não** VST3 in-process; «plugin próprio» = pacote fora do binário (D91).
- Arquitectura e desenho final de novas superfícies exigem decisão explícita numerada. Não voltar a pedir decisões de produto já expressas; concretizar apenas os detalhes ainda abertos.
- Não reintroduzir instrumentos/designers próprios ou AI no Core. Não mover código entre Core/AICORE sem decisão.
- Master vertical/selecção, refinamento visual, janela DSP e maqueta de interface estão **fora da árvore** por ordem do dono (D83); re-land só por decisão numerada. Não os reescrever por iniciativa.
- Preservar catálogo de padrões, alinhamento kick/baixo/pratos, projectos e áudio do utilizador.

## Language

- Owner's instruction, verbatim (2026-11-18 02:23 Europe/Lisbon, DSH coordinator session `session-2ff54f03-8344-40bb-8cb4-54df0601eb69`): «sempre tudo em ingles, meter como rule nos fecheiros necessarios».
- Everything written into this repository is in English: source code, identifiers, comments, test names and test output, log and error messages, script and tool output, commit messages, and new documentation.
- Build tools emit English where the toolchain has it. The gate asks MSVC for English (`VSLANG=1033`, ignored when the English resources are not installed, as on the owner's PC) and switches to the UTF-8 code page (`chcp 65001`): localized compiler output in a mismatched code page broke Ninja's header dependency tracking and produced false-green gates.
- User-visible app text is English in the source; translations live only in `lang/*.json`.
- Not translated: the owner's verbatim quotes (always kept in the original language) and the existing ledger and history (`ESTADO.md`, `docs/relatorios/`, `docs/historico/`), which are never edited.
- Existing non-English code, test names and tool messages are converted to English when they are touched; do not mix a translation sweep into an unrelated fix.

## Engine quality bar

- Owner, verbatim (2026-11-18, DSH coordinator session `session-2ff54f03-8344-40bb-8cb4-54df0601eb69`): «eu quero melhor motor que possamos fazer»; and, about the five criteria below: «deixa isso como criterio a chegar».
- These are targets to reach, not claims about the current state. Each one counts as met only when it is proven by measurement:
  1. Never loses work.
  2. A failing plugin never takes the song down.
  3. Zero audio dropouts: no allocation, lock, I/O or blocking call on the audio thread, proven by tooling rather than by reading.
  4. Handles many plugins: the multicore path proven identical to the serial path, measured with real plugins under load.
  5. Exact sound: automation and latency compensation correct at every sample rate and block size, proven by null tests.
- When they conflict, the owner's order applies: data > real time > performance > compatibility.

## Ledger (D72/D73/D79/D80)

- `ESTADO.md` é escrito **só pelo escrivão do ledger**. Qualquer outra entrega propõe linhas: descreve-as no relatório e abre/actualiza a pendência — não edita `ESTADO.md`.
- Numeração contínua, sem reset; entradas nunca editadas nem apagadas; invalidações **por referência**; `docs/historico/MIGRACAO-LEDGER.md` é o mapa old→new.
- Bugs usam o namespace `BUG-` (D75); não ocupam números D.

## Execução

- Identificar branch/SHA e alterações existentes antes de editar; preservar trabalho alheio.
- Auditoria/documentação não autoriza implementação. Implementação autorizada deve ter âmbito explícito.
- Não portar PRs antigos integralmente: comparar com a baseline e trazer apenas o que falta e serve a direcção actual.
- Não apagar código, dados, testes ou história apenas por parecerem antigos; mapear dependências. Limpeza de samples distribuídos não é limpeza dos ficheiros do utilizador.
- Correcção de bug: regressão significativa e vermelho medido no estado anterior quando executável, depois verde. Se não executável, declarar a limitação.
- Contagens de testes por suite/âmbito; não reduzir cobertura ou relaxar critérios para obter verde. Flaky é defeito a investigar.
- Cada milestone de implementação mantém Core compilável, a abrir, tocar, guardar/reabrir e exportar; não declarar estes gates sem execução.
- Usar testes/gates registados no checkout. Linux não prova runtime Windows. Sem tokens novos para agentes ou activação de CI nesta organização documental.
- Não enviar mensagens, fechar PRs nem arquivar outro repo por inferência de um plano.

## Entrega obrigatória

Seguir docs/AUDITORIA.md. Deixar relatório em docs/relatorios/AAAA-MM-DD-assunto.md, indexá-lo e **propor** as
linhas de ESTADO.md que a entrega pediria: escrever no relatório o que deve entrar em Decisões/Estado medido,
com citação e fonte, e abrir a pendência correspondente. Só o escrivão escreve `ESTADO.md` (D72/D80).
Distinguir confirmado no código / comprovado por teste / apenas reportado / não verificado.
Nunca apresentar plano como implementado, teste existente como corrido, ou build como teste de UI.
