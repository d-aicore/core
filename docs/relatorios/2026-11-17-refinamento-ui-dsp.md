# Refinamento visual e DSP Performance

## Âmbito

Desktop Windows sobre `main` em `b80017e`, com o Master vertical e a seleção acrescentados na intervenção anterior. A referência do Bitwig orientou o painel de diagnóstico, a densidade das pistas e a separação entre régua e grelha.

## Alterações

- O cabeçalho de pista passa de 374 para 300 px lógicos. Nome, FX, mute, solo, ganho e dois medidores mantêm acesso direto. A grelha ocupa todo o Arrangement e continua por baixo das pistas. Compasso, secção, loop e seleção mantêm hierarquia visual própria.
- O Inspector apresenta a duração selecionada num campo de leitura principal. O valor em dB do Master ganha um campo delimitado. O piano roll tem uma barra de edição própria e usa a cor da pista para identificar o contexto.
- **Audio > DSP Performance** e o botão **DSP** da barra inferior abrem uma janela redimensionável. O gráfico recebe a carga média e de pico dos callbacks de `engine.cpu`; 100% representa o orçamento de processamento. Mostra os últimos 30 segundos, bloco nominal, frequência de amostragem, orçamento nominal do bloco e duração estimada do buffer de saída. A janela e o Mixer partilham o pedido assíncrono. O gráfico aberto pede dados a cada 250 ms, com um pedido de cada vez. Com a janela fechada, vigora o ritmo do Mixer quando este está aberto.

## Backend em falta

### Tonalidade e escala do projeto

A deteção existente em `doc::songRoot`/`doc::followSongKey` estima apenas a tónica pelas notas. O seletor da referência precisa de uma escolha explícita que sobreviva a guardar, reabrir e Undo. Ainda não há esse controlo no Core.

1. Definir no documento um estado opcional com tónica em classe de altura de 0 a 11 e ID estável de escala. Projetos antigos sem o campo mantêm o comportamento atual. Validar campos desconhecidos e preservar dados no percurso de guardar/reabrir.
2. Definir o catálogo de escalas com intervalos em semitons, nomes apresentados e IDs estáveis. Separar realce visual no piano roll, snap voluntário de novas notas e operações explícitas sobre notas existentes. A escolha de escala por si só não deve reescrever notas.
3. Fazer o seletor alterar o estado através do histórico. Fazer a grelha ler esse estado e destacar tónica e graus da escala. Definir quais Tools usam a preferência explícita e quais continuam a detetar a tónica.
4. Verificar os 12 centros tonais, mudança e Undo, projetos legados e partilhados, e igualdade das notas e do áudio ao mudar apenas a referência visual.

### Métricas adicionais do DSP

- `engine.cpu` disponibiliza carga média e pico por intervalo. Para mostrar tempos médios e máximos exatos em milissegundos, exportar duração medida do callback por janela com contagem de callbacks e máximo em nanossegundos. Multiplicar a percentagem pelo bloco nominal pode dar tempos errados quando o tamanho real varia.
- Para medir jitter no dispositivo, recolher intervalos entre callbacks, tratar callbacks agrupados e expor a distribuição e a unidade no protocolo. Os contadores atuais de atraso e a estimativa de drift servem a outros fins.
- Para latência de entrada e latência de saída medida, distinguir configuração do buffer, latência reportada pelo backend e latência física. O Core atual tem estimativa do buffer de saída. A janela identifica essa estimativa como **Output buffer**.
- Ao adicionar telemetria, manter o callback livre de I/O e alocações. Recolher em contadores adequados ao tempo real e publicar janelas no processo de controlo. Comparar monitor aberto e fechado em WASAPI para medir custo e detetar quebras.

## Verificação

- Release Windows x64. `ctest --test-dir D:/Core-build/gate-CORE --output-on-failure --no-tests=error` terminou com código 0 nas duas builds, incluindo na final. Passaram 15 testes merge e 1 teste desktop. O self-test recebeu amostras DSP durante a reprodução com dispositivo nulo e verificou também Master, Undo, seleção de tempo, guardar/reabrir e exportar WAV.
- Inspeção da captura do Arrangement e verificação visual da janela DSP na build isolada `D:/Core-build/core-ui-refined/Core.exe`. O botão DSP abriu o gráfico em WASAPI com 480 amostras, 48 kHz, orçamento nominal de 10 ms, estimativa de buffer de saída de 20 ms e carga média/pico recebida do motor. A abertura em WASAPI comprova a ligação de dados e disposição, sem benchmark do custo do painel sob carga.
- A captura do Arrangement está em `D:/Core-build/core-ui-refined/arrangement.png`. O executável copiado tem o mesmo SHA-256 do executável testado.
