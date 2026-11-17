# Maqueta da interface do Core — 17-11-2026

## O que foi medido

Base `main` em `b80017e`, com alterações locais de UI e DSP já presentes. Leitura do código JUCE, documentação de produto e captura Windows em `D:/Core-build/core-ui-refined/arrangement.png`. A maqueta usa um projeto fictício.

## Resultados

| Resultado | Evidência | Classificação |
| --- | --- | --- |
| Navegação no topo e na lateral, com Inspector e Master antes da régua | Captura de Arrangement; `MainComponent.cpp`, método `resized()` | Observado |
| Transporte fixo, navegação única, Browser/Inspector alternáveis, Master estreito à direita e editor de contexto inferior | HTML local `Core-UI.html` | Proposta |
| Seleção de clip e secção, acesso ao editor do plugin, destino de Tools e envio de um padrão para Arrangement | Asserções em Edge headless | Validado na maqueta |

## Alterações

HTML autónomo com interface em inglês, captura de 1440 × 920 e interações simuladas. O código e os executáveis do Core ficaram intactos.

## Verificação

Edge headless renderizou a captura e confirmou oito percursos por asserção: fechar Inspector, filtrar Browser, selecionar clip de áudio, abrir editor de plugin simulado, transferir intervalo para Tools, colocar padrão, mute e abrir DSP. Áudio, integração VST3, desempenho e fiabilidade do Core requerem build nativa e sessão Windows.

## Pendências e passagem

Validar a hierarquia com um projeto realista e dimensões típicas do utilizador. Uma eventual implementação deve começar no layout existente de transporte, navegação, Arrangement e editor inferior, reaproveitando a seleção e as ações JUCE. Verificar abertura, reprodução, guardar/reabrir, exportação e carga do DSP em Windows.

## Adenda visual — 17-11-2026

A revisão da maqueta adota um menu vertical, gradação por altura no piano roll, forma de onda ilustrativa nos kicks e fader retangular no Master. Captura revista em Edge a 1440 × 920.
