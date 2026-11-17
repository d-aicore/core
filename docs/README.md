# Documentação

## Em vigor

| Documento | Função |
| --- | --- |
| [Regras](../AGENTS.md) | Como trabalhar e entregar |
| [Estado](../ESTADO.md) | Ledger canónico de decisões, pendentes, invalidadas e estado medido |
| [Migração do ledger](historico/MIGRACAO-LEDGER.md) | Mapa old→new e fontes arquivadas (D73/D81/D82) |
| [Produto](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md) | Pattern Maker, Tools, limpeza, designers e formatos |
| [Fronteira](FRONTEIRA-CORE-AICORE.md) | Ownership, travessias, migração e gates |
| [Auditoria](AUDITORIA.md) | Procedimento e entrega |
| [Relatórios](relatorios/README.md) | Evidência por data/assunto/SHA |

## Referência histórica

[Plano anterior](PLANO-CORE-PLUGINS.md) e [triagem](../TRIAGEM-TESTES.md) conservam contexto e medições dos respectivos snapshots; não definem por si só a direcção actual. Caminhos mantidos para não quebrar referências.

Não criar resumos concorrentes do ESTADO. **Só o escrivão do ledger escreve `ESTADO.md`** (D72/D79/D80):
entregas propõem linhas via relatório + pendência, nunca editam a página. Resultados pertencem a relatórios;
decisões entram no ledger com citação verbatim do dono, data e fonte. Ficheiros temporários, logs brutos e
builds ficam fora do repo salvo evidência pequena, necessária e sem segredos.
