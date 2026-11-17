# Primeira leitura UI/UX — 17-11-2026

## Âmbito e base
Core, branch main, SHA b80017efdbb6b47bc65a82338c403548e095967f, Windows. Pedido: ler UI/UX e sugerir melhorias. Leitura estática; Core não consta das janelas abertas. Sem avaliação visual, build ou testes de utilização. Alteração pré-existente: .qoderignore não rastreado, preservado.

## Achados confirmados no código
- Theme.h:23–40 centraliza cantos, tipografia e espaçamentos. Base coerente a preservar; aplicação uniforme não auditada.
- MainComponent.cpp:1972–2005 oferece Arrange/Mix no topo e Arrangement/Mixer na barra lateral; Patterns fica na barra de ícones. Proposta: hierarquia única com Arrangement, Pattern Maker, Mixer e Tools identificados por texto.
- MainComponent.cpp:5539–5552 oculta nível de escuta, nome do projecto e posição conforme a largura disponível. Proposta: reorganizar grupos em janela estreita e garantir acesso explícito aos controlos recolhidos. Legibilidade e limites efectivos não testados.
- PatternsWindow.cpp:102,488 ainda refere P-Core em tooltips; documentação de produto exige Pattern Maker. Proposta: vocabulário único em nomes, ajuda e navegação.
- PatternsWindow.cpp:474–502 já distingue Preview, Load Template e Send to Arrange. Preservar separação entre ouvir, editar padrão e inserir no arrangement; tornar destino e alcance explícitos.

## Proposta de apresentação (não implementada)
1. Manter charcoal, cinza-azulado, cantos discretos e identidade de pista. Reforçar hierarquia por espaçamento e contraste, evitando multiplicar molduras.
2. Topo estável: projecto, transporte, BPM, posição e escuta. Navegação com texto; biblioteca recolhível à esquerda, arrangement ao centro, editor contextual em baixo.
3. Cabeçalho contextual persistente: nome/cor da pista, clip ou intervalo seleccionado. Tools mostra pistas, início/fim e instâncias afectadas antes de aplicar.
4. Chain com nome de instância, bypass e Open Plugin acessível em cada cartão. Parâmetros adicionais por expansão, preservando acesso directo aos editores.
5. Estados distintos para hover, seleccionado, activo, bypass e indisponível; não depender só de cor. Validar teclado e foco além de rato.
6. Priorizar navegação e contexto; depois dimensionamento/tipografia; por fim detalhes gráficos.

## Verificação e passagem
Leituras de código e inventário de janelas concluídos. Nenhum teste do produto executado. Não se afirma que truncamento ou perda de contexto tenha sido reproduzido. Próximo passo: observar build identificado e tarefas reais (criar padrão, enviar ao arrangement, aplicar Tool a intervalo, abrir plugin e voltar); depois comparar mockup e aprovar desenho antes de implementar.
