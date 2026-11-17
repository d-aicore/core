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
