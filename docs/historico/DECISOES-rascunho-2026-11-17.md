# Ledger de decisões do Core

Actualizado em 17-11-2026. Checkout: `main` = `b80017efdbb6b47bc65a82338c403548e095967f`, com alterações locais na árvore de trabalho (ver §6).

**Âmbito: apenas o Core** — o binário DAW, a sua UI, os seus dados musicais, o build e o pacote. Este ficheiro não regista decisões cujo objecto é o projecto AICORE nem o conteúdo musical do CORE-FUNDAMENTALS. Quando uma instrução do dono restringe o que pode viver no Core (p. ex. AI fora do binário), o que fica aqui é o efeito no Core, não a matéria do lado de fora.

A razão de ser do ficheiro está na [fronteira](FRONTEIRA-CORE-AICORE.md): «não foi encontrado um ledger dessas decisões neste snapshot do qwen-core e não se inventa a sua ratificação histórica» ([FRONTEIRA-CORE-AICORE.md:13](FRONTEIRA-CORE-AICORE.md#L13)). Este é esse registo, feito por leitura do estado actual. Não é um segundo [ESTADO](../ESTADO.md) e não substitui os documentos responsáveis: direcção de produto na [direcção](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md), ownership/transporte/persistência na [fronteira](FRONTEIRA-CORE-AICORE.md), resultados em [relatórios](relatorios/README.md).

## 1. Como apontar

- IDs **AP-nn** são entradas deste registo. Não ocupam nem renumeram as decisões **D** do ledger do dono, nem **PROD-nn**, nem **P-Fnn** ([DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md:9](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L9)).
- Uma entrada só entra como **ratificada** com instrução explícita do dono. Documento histórico, comentário de código, PR, relatório ou sugestão de coordenador não ratificam ([AGENTS.md](../AGENTS.md), «Autoridade e âmbito»).
- Toda a entrada liga ao sítio onde a decisão está escrita, com linha. Sem fonte verificada, vai para §4 (proposta) ou §5 (em aberto).
- Uma decisão que mude não se reescreve a perder o anterior: acrescenta-se linha em §6 com data e o que passou a valer.
-改变… não: este ficheiro não regista trabalho feito nem números de testes; isso são relatórios.

## 2. Ratificadas pelo dono

| AP | Decisão | Fonte |
| --- | --- | --- |
| AP-01 | Só **Pattern Maker e Tools** ficam como ferramentas criativas próprias do Core. | [DIRECÇÃO:4](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L4), [:33](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L33) |
| AP-02 | O som criativo vem de **plugins externos instalados pelo utilizador** e de áudio importado/gravado; o Core não introduz gerador de som próprio. | [DIRECÇÃO:5](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L5), [:28](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L28), [FRONTEIRA:40](FRONTEIRA-CORE-AICORE.md#L40) |
| AP-03 | **AI fora do Core por agora.** Modelo, chaves, conhecimento, memória de receitas, SQLite, chats e export JSONL não pertencem ao Core.exe; a ponte AICORE é externa e opcional. | [FRONTEIRA:3-7](FRONTEIRA-CORE-AICORE.md#L3) |
| AP-04 | **Preservar o catálogo de padrões** (45 JSON: 39 com origem AICORE, 6 DnB com outras fontes, todos `status: hypothesis`) e o **alinhamento de pratos com kick/baixo**. Origem AICORE não autoriza apagar padrões. | [DIRECÇÃO:35-44](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L35), [FRONTEIRA:240-242](FRONTEIRA-CORE-AICORE.md#L240) |
| AP-05 | Retirar da oferta os instrumentos/efeitos criativos próprios e os módulos `*-Core` como superfícies autónomas, incluindo **D-Core e F-Core**. | [DIRECÇÃO:52](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L52), [:163](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L163) |
| AP-06 | **Kick Design e Cymbal Design** entram explicitamente na remoção; não basta改名 para Tools. | [DIRECÇÃO:141-145](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L141) |
| AP-07 | Retirar os **samples antigos distribuídos** do pacote, menus, omissões, presets/demo e cópia automática — **sem apagar** samples pessoais, áudio gravado/importado nem referências de projectos do utilizador. | [DIRECÇÃO:62-68](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L62) |
| AP-08 | **Limpeza extensa dos menus**: uma operação com um lugar primário claro, poucos passos, profundidade acessível; alvo sempre explícito; nada de opção aparentemente activa que não faz nada. | [DIRECÇÃO:70-85](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L70) |
| AP-09 | O painel de Tools mostra os plugins da chain e abre o **editor da instância exacta** usada — não outra instância nem uma cópia nova; fechar/reabrir não recria nem repõe parâmetros. | [DIRECÇÃO:126](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L126) |
| AP-10 | O produto **não se limita a VST3**: adaptar a diferentes tipos/formatos. Abrangência é objectivo, **não** promessa de compatibilidade universal; matriz de suporte antes de implementar. | [DIRECÇÃO:151-159](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L151) |
| AP-11 | Nome de produto visível: **Pattern Maker**. Não manter P-Core como segunda ferramenta equivalente. | [DIRECÇÃO:33](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L33) |
| AP-12 | Tools operam sobre **pistas/partes seleccionadas e intervalo em compassos** (início/fim), com resultado editável e Undo; chain e parâmetros definidos pelo utilizador. | [DIRECÇÃO:19-25](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L19) |
| AP-13 | O **DAW permanece**: transporte, arrangement, mixer, hosting, automação, routing, buses, gravação, import/export, persistência e recuperação. Gain, pan, metering, resampling e protecção de saída são DSP infraestrutural permitido. | [DIRECÇÃO:54-56](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L54), [FRONTEIRA:40](FRONTEIRA-CORE-AICORE.md#L40) |
| AP-14 | Exportar pistas/stems do projecto é operação do DAW; **separar uma mistura em stems por AI** não é trabalho do Core. | [FRONTEIRA:42](FRONTEIRA-CORE-AICORE.md#L42) |
| AP-15 | Alteração composta **tudo-ou-nada**: recusa integral antes de commit, sem Undo vazio e sem efeitos laterais; deixar de filtrar chamadas inválidas e aplicar as restantes. | [FRONTEIRA:89](FRONTEIRA-CORE-AICORE.md#L89), [:145-147](FRONTEIRA-CORE-AICORE.md#L145) |
| AP-16 | Fio futuro de controlo é **pipe local** — sem sockets, HTTP, WebSocket, servidor de rede ou fallback remoto; «localhost» continua a ser socket. | [FRONTEIRA:62](FRONTEIRA-CORE-AICORE.md#L62) |
| AP-17 | **Windows 11 x64 é P0**; uso privado, local-only, sem subscrição. | [FRONTEIRA:38](FRONTEIRA-CORE-AICORE.md#L38) |
| AP-18 | Ordem de prioridade do produto: integridade e fiabilidade > real-time > performance > compatibilidade de plugins > clareza de workflow > quantidade de funcionalidades. | [FRONTEIRA:44](FRONTEIRA-CORE-AICORE.md#L44) |
| AP-19 | AI desactivada significa **ausência efectiva** de activação/polling/lançamento, não esconder um botão. | [FRONTEIRA:68](FRONTEIRA-CORE-AICORE.md#L68) |
| AP-20 | **Gatilho de arquitectura**: qualquer mudança de dependência, processo, persistência ou âmbito exige decisão numerada explícita antes da implementação. Migração de dados é não-destrutiva e versionada. | [FRONTEIRA:69](FRONTEIRA-CORE-AICORE.md#L69) |
| AP-21 | Selecção de secção na régua: clicar no nome selecciona a secção inteira; Shift+arrastar cria **intervalo independente do loop**. Desenho orientado pelas referências visuais do dono, ainda sem ID de produto. | [relatório master vertical:9-11](relatorios/2026-11-17-master-vertical-selecao.md#L9) |

## 3. Ratificadas como requisito e ainda não provadas no código

A instrução existe; a entrega não está demonstrada. Não usar nenhuma destas como facto consumido.

| AP | Requisito | Estado observado | Fonte |
| --- | --- | --- | --- |
| AP-30 | `CORE_BUILD_AI=OFF` por defeito | Opção não encontrada no CMake raiz nem na configuração desktop/control examinada | [FRONTEIRA:27](FRONTEIRA-CORE-AICORE.md#L27) |
| AP-31 | Gate `ai_boundary_static` | Não registado em `tests/CMakeLists.txt` deste snapshot | [FRONTEIRA:29](FRONTEIRA-CORE-AICORE.md#L29) |
| AP-32 | Core 100% funcional sem pacote AI | Critério por demonstrar em Windows P0 | [FRONTEIRA:30](FRONTEIRA-CORE-AICORE.md#L30) |
| AP-33 | Painel/badge AI dormentes | Ligados no código; `aiControl` com default `true` na saída real | [FRONTEIRA:28](FRONTEIRA-CORE-AICORE.md#L28) |
| AP-34 | Bridge fora do binário principal | `core_control` ainda contém `calls.cpp` e `bridge.cpp`; painel e polling activos | [FRONTEIRA:80](FRONTEIRA-CORE-AICORE.md#L80), [:134](FRONTEIRA-CORE-AICORE.md#L134) |
| AP-35 | Recusa integral da proposta | Código actual filtra partes e pode aplicar as restantes (X12) | [FRONTEIRA:145](FRONTEIRA-CORE-AICORE.md#L145) |

## 4. Não ratificadas — registo para que ninguém as use como autorização

- **Folha D58–D66 do coordenador**: não adoptada em bloco; não executar como autorização ([DIRECÇÃO:9](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L9), [ESTADO:55](../ESTADO.md#L55)). A proposta D58 de F-Core como superfície autónoma não é para importar ([DIRECÇÃO:86](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L86)).
- **Referências ao painel/memória da PR #4 como decisão aprovada**: invalidadas pela D47 tal como comunicada pelo dono ([FRONTEIRA:15](FRONTEIRA-CORE-AICORE.md#L15)).
- **P-F01…P-F06**: propostas de decisão, não decisões aprovadas nem substituto do ledger do dono ([FRONTEIRA:227](FRONTEIRA-CORE-AICORE.md#L227)).
- **D63**: foi sugestão. A política final por operação (save/replace/remove/export) não está ratificada ([ESTADO:31](../ESTADO.md#L31)).
- **Port/fecho do PR #25 e arquivo do core-main**: não executados nem autorizados por esta limpeza ([ESTADO:55](../ESTADO.md#L55)).
- **Nomes reservados Windows no export Bitwig/MIDI** (`CON`, `NUL`): texto recebido, revalidação e teste em Windows não executados ([ESTADO:33](../ESTADO.md#L33)).
- **Tail/hash total da cache**: sugestões do audit, não implementação comprovada ([ESTADO:48](../ESTADO.md#L48)).

## 5. Em aberto — precisa de decisão numerada antes de implementar

Vem de [DIRECÇÃO §6](DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md#L161) e [FRONTEIRA §7 M0](FRONTEIRA-CORE-AICORE.md#L163):

1. Desenho final dos painéis simplificados de Tools e a relação macro/automação/editor de cada instância.
2. Matriz e prioridade de formatos de plugin para além de VST3.
3. Inventário exacto de Kick Design/Cymbal Design e dos samples/recursos legados a retirar do pacote.
4. Quais receitas antigas sobrevivem em Tools e o mapeamento dos seus parâmetros para plugins externos.
5. Semântica dos limites temporais: curvas e caudas após o fim, restauração de parâmetros, várias tools sobre a mesma instância.
6. Tratamento de projectos antigos sem o plugin substituto instalado.
7. Allowlist exacta do catálogo de acções (manter `mix`/`open`? que operações de estrutura entram?) e classificação das acções actualmente construídas por `buildFunctions`.
8. Topologia e local do endpoint dos pipes e o que fica do lado Core como receptor mínimo.
9. Política de estado por operação (save/replace/remove/export) — o lugar de D63.
10. Projecto antigo editado numa vista Track: alterar a ocorrência muda o padrão partilhado ou torna o clip independente; duração de uma selecção no piano roll (um padrão pode correr a BPMs diferentes).

## 6. Movimentos

| Data | O que mudou | Baseline |
| --- | --- | --- |
| 2026-11-17 | Ledger criado por apontamento do dono («vas apontando decisões sobre o Core num ficheiro»). Conteúdo derivado por leitura da direcção, da fronteira, do ESTADO e dos relatórios; nada novo foi decidido nesta entrega. | `b80017e`, árvore de trabalho com `ESTADO.md`, `app/desktop/src/*` e `docs/relatorios/README.md` alterados e dois relatórios novos por comitar |
