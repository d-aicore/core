# Master vertical, seleção e dependências de backend

## Âmbito

Core desktop para Windows. Alterações locais sobre `main` em `b80017efdbb6b47bc65a82338c403548e095967f`. Referências visuais fornecidas pelo dono. Menu lateral, páginas recolhíveis e distinção entre Tools e plugins continuam a orientar o desenho.

## Implementado

- `MainComponent.cpp` e `.h` passam o Master do rodapé para uma faixa vertical entre o Inspector e o Arrangement. O fader e o valor em dB editam o ganho do projeto. O botão FX abre diretamente o menu existente dos efeitos do Master. No Mixer usa-se a faixa própria do Master.
- Clicar no nome de uma secção na régua seleciona a secção inteira. Shift+arrastar na régua seleciona um intervalo independente do loop. `InspectorPanel.cpp` apresenta início, fim, tamanho em compassos e duração, integrada através do mapa de tempo do projeto.
- As cores das pistas identificam notas, clips e cabeçalhos selecionados. No piano roll, Shift+arrastar na régua seleciona um intervalo e as notas que o intersectam. O intervalo tem preenchimento translúcido e dois limites visíveis.

## Backend disponível e trabalho em falta

| Área | Disponível | Trabalho em falta |
| --- | --- | --- |
| Master | O documento e o motor partilham `masterGainDb`, `masterEffects` e picos estéreo na snapshot. O ganho faz parte do histórico e do projeto guardado. | Nenhuma alteração de backend necessária para a faixa introduzida. O medidor usa picos, sem medição de loudness ou retenção dedicada de clipping. |
| Intervalo e secção no Arrangement | `Section` tem ID, início e comprimento. `doc::secondsBetween` integra as mudanças no mapa de tempo. | Nenhuma alteração de backend para mostrar tempos. A seleção é estado da interface. Se passar a alimentar Tools, definir o contexto explícito de pistas e limites em ticks, validar plugins por instância, e aplicar a operação como uma edição com Undo. |
| Clip e Track no piano roll | As notas pertencem a padrões. Clips referenciam padrões e têm posição, comprimento e offset. O editor atual edita o padrão ativo. | Para uma vista Track que edite várias ocorrências, projetar as notas de cada clip para a timeline, levando em conta offsets, repetições e cortes. Manter identidade de clip, padrão e nota para mapear a edição de volta. Definir se editar uma ocorrência altera o padrão partilhado ou torna o clip independente. Testar clips repetidos e partilhados, drag através de limites e Undo. A vista multi-clip em si ainda não existe. |
| Duração de uma seleção no piano roll | O intervalo existe em ticks locais do padrão. | Mostrar no Inspector como tempo local do padrão ou identificar a ocorrência do clip a usar. Um padrão pode ser usado várias vezes com BPM diferente, pelo que não existe uma única duração no relógio do projeto sem essa referência. |

## Verificação

- Release x64 em `D:/Core-build/core-master-vertical`. `ctest --test-dir D:/Core-build/core-master-vertical --output-on-failure --no-tests=error` terminou com 16/16 testes, incluindo 15 merge e 1 desktop. A repetição isolada de `desktop_self_test` também passou.
- O teste desktop verificou ganho do Master e Undo, seleção de tempo sem alterar o loop, reprodução com dispositivo nulo, gravação e reabertura, exportação WAV e abertura do Mixer. A captura `tests/self-test.png` mostra o Master, o intervalo no Arrangement e a duração no Inspector. Edição manual de texto em dB e seleção de notas por intervalo não foram testadas separadamente.
- Builds incrementais anteriores falharam com `0xc0000374`. `ninja -t deps` mostrou zero dependências no objeto de `Main.cpp`. A saída localizada de `/showIncludes` não coincidia com o prefixo guardado em `rules.ninja`. Recompilar todos os ficheiros desktop eliminou a falha nas duas execuções finais. Para outra build incremental, confirmar que o Ninja regista as dependências dos headers antes de confiar nos objetos reutilizados.
