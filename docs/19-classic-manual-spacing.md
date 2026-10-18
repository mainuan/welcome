# Espaçamento manual do Clássico — 2.3.1.3

> Registro histórico do desenvolvimento experimental. Os números 2.x, resultados de execução e caminhos de capturas descrevem aquele momento. A primeira versão oficial é **1.0**; consulte a [documentação atual](README.md) antes de seguir instruções antigas.

## Referência ativa

Foram lidos os dois arquivos de `classico-5-barras-espacamento`. A seleção usou exclusivamente os containments com `MainuanLayout=plasma-default` e os papéis `launcher`, `info`, `tasks`, `tray` e `clock`. As seções antigas não marcadas foram ignoradas. Os números da captura serviram apenas à extração e não foram incorporados ao gerador.

| Papel | Modo | Âncora | Comprimento lógico | Offset nominal |
|---|---|---|---|---|
| launcher | fit | esquerda | conteúdo | nenhum |
| info | custom | esquerda | 217–221 | 65 |
| tasks | fit | centro | conteúdo | nenhum |
| tray | custom | direita | 203–273 | 111 |
| clock | fit | direita | conteúdo | nenhum |

Todos permanecem inferiores, flutuantes e com 40 pixels lógicos de espessura. A faixa 203–273 do quarto segmento é a da nova captura ativa; os limites antigos de 251–339 pertencem à referência anterior e às seções residuais. A nova referência substitui esses dados anteriores.

`CLASSIC_PANEL_GEOMETRY`, em `reference-layouts.js`, centraliza somente os modos, alinhamentos, limites e offsets saneados. O mapeamento dos widgets e suas configurações permaneceu idêntico, byte a byte: menu/separador; previsão/informações; tarefas; ChatAI/bandeja/KdeControlStation; relógio/desktop. Não há kdeaichat, novos espaçadores, alteração de ícones ou ampliação artificial de painéis.

## Causa da distância excessiva

O gerador anterior colocava o segmento informativo no meio do espaço livre entre menu e tarefas e fazia o mesmo com os controles entre tarefas e relógio. Quanto maior a tela, maiores ficavam esses deslocamentos. Esse cálculo distribuía os painéis pela tela, em vez de reproduzir os grupos próximos às bordas ajustados manualmente pelo usuário.

Agora o segmento informativo parte do offset 65 da borda esquerda, e os controles do offset 111 da borda direita. As tarefas continuam centralizadas. Menu e relógio seguem suas âncoras fit originais.

## Adaptação e integridade

Os offsets são distâncias em pixels lógicos às bordas, portanto não precisam ser multiplicados pela largura da tela. Em cada tela, após a medição assíncrona já existente:

```text
posição das informações = max(65, ceil(comprimento do menu + 16))
posição dos controles   = max(111, ceil(comprimento do relógio + 16))
```

A reserva de 16 pixels é uma proteção lógica de implementação para a vizinhança dos painéis fit, e não uma distância física informada pela captura. Ela evita voltar à distribuição pelo espaço vazio. Um menu ou relógio maior só desloca seu vizinho na medida necessária.

O espaço restante é calculado até a borda real das tarefas centrais, a partir da largura lógica da respectiva tela e do comprimento medido das tarefas. Os máximos customizados podem ser reduzidos se esse espaço exigir, preservando os mínimos. Se os mínimos não couberem, a validação falha e o rollback existente restaura os painéis anteriores. A verificação de overflow observável dos três widgets do quarto segmento também foi preservada.

Nos cenários compatíveis com a captura, os offsets 65/111 e as faixas 217–221 e 203–273 se mantêm em 1280×720, 1366×768 e 1920×1080. Os testes usam comprimentos fit simulados de 48 para o menu, 95 para o relógio e 240 para tarefas. Esses números são entradas dos mocks; a captura não especifica as larguras físicas dos painéis fit.

Também foram simulados menu com 90 e relógio com 170 pixels: os offsets se ajustam somente para 106 e 186, sem crescer proporcionalmente à resolução. Tarefas exageradamente largas são rejeitadas, mantendo a recuperação existente.

## Semântica da API

O arquivo salvo usa `alignment=1` para esquerda e `alignment=2` para direita. As tarefas sem override mantêm o centro padrão. O scripting recebe os textos `left`, `right` e `center`.

As implementações oficiais de [Panel](https://invent.kde.org/plasma/plasma-workspace/-/blob/Plasma/6.6/shell/scripting/panel.cpp) e [PanelView](https://invent.kde.org/plasma/plasma-workspace/-/blob/Plasma/6.6/shell/panelview.cpp) confirmam que offsets são úteis para as âncoras laterais em modo custom. Em fit, o conteúdo e a âncora determinam o posicionamento; o gerador não escreve offsets nem limites nesses segmentos. Também não tenta deslocar o painel central por um offset ignorado no Wayland.

`panel.length` pode expressar uma preferência muito grande, principalmente em painéis custom com conteúdo expansível; o cálculo mantém os limites customizados e a verificação adicional da geometria dos widgets. A API não expõe aqui uma medição universal de todas as sombras e molduras de cada tema.

## Escopo e validação

`LayoutService.cpp` foi examinado e permaneceu intacto: já aguarda duas medidas estáveis antes da confirmação, com consultas assíncronas. Backup, rollback, identificação, requisitos dos widgets, ordem dos sete layouts e demais serviços foram preservados.

Os testes específicos verificam modos, âncoras, offsets antes/depois de consultas repetidas, limites, crescimento de menu/relógio, ausência de sobreposição no modelo, reserva para margens simuladas, duas telas, preservação dos widgets e espessura. A comparação dos scripts anterior/atual confirmou que os outros seis layouts produzem configurações idênticas em 1280, 1366 e 1920. Os demais fontes, recursos, configurações pessoais e pacotes anteriores foram monitorados por hash.

A inspeção da sessão real foi somente de leitura; ela estava em outro painel personalizado. Nenhum teste aplicou layouts ou alterou arquivos ativos do usuário.

**Pendente de validação visual em Wayland:** reaplicar explicitamente o Clássico e conferir o agrupamento nas bordas, molduras, margens flutuantes e sombras em Dream, Tahoe e Breeze. A reserva simulada não prova igualdade física entre todos os temas, fontes e escalas. Os espaços naturais até as tarefas centrais continuam dependendo da largura da tela, pois centro e âncoras das extremidades foram preservados.

## Arquivos desta etapa

- `src/layouts/desktop-layouts.js`: posicionamento do Clássico pelas âncoras e medições.
- `src/layouts/reference-layouts.js`: geometria saneada dos cinco painéis ativos.
- `tests/tst_layouts.cpp`: expectativas da nova captura e testes de espaçamento.
- `CMakeLists.txt`: versão 2.3.1.3.
- `debian/changelog`: entrada 2.3.1.3.
- `docs/19-classic-manual-spacing.md`: este relatório.

## Resultado e pacote

- `cmake -S . -B build` e `cmake --build build -j"$(nproc)"`: sucesso.
- `ctest --test-dir build --output-on-failure`: quatro testes aprovados, sem falhas ou testes ignorados; inclui QtTest, QML e inicialização offscreen.
- `dpkg-buildpackage -us -uc -b`: sucesso, repetindo os quatro testes em Release. QtTest registrou 45 resultados no teste geral, 20 no de layouts e 4 em QML, incluindo inicialização/finalização.
- `dpkg-deb -f`: versão interna **2.3.1.3**, arquitetura **amd64**. CMake e changelog sincronizados.
- `lintian`: saída vazia, sucesso. Permanece a observação já existente de dpkg-shlibdeps sobre ligação com QtQuick, sem impedir a geração.
- Recursos incorporados conferidos no executável extraído: scripts e ícones anteriores idênticos aos fontes. Nenhum arquivo pessoal nem cópia dos arquivos de referência foi empacotado.
- `git diff --check`: sem erros. Alterações locais anteriores preservadas; nenhum commit criado.
- Configurações pessoais, avatar, LayoutService, demais serviços e pacotes 2.3.1.1/2.3.1.2 preservados por hash.

Pacote final: `/home/daigo/Github/mainuan-welcome_2.3.1.3_amd64.deb` (537332 bytes).

```bash
sudo apt install /home/daigo/Github/mainuan-welcome_2.3.1.3_amd64.deb
```

O pacote não foi instalado. Não houve commit, push ou publicação. Os intermediários de empacotamento foram limpos; logs, comparação de escopo, geometria saneada da referência, inspeção do pacote e checksums permanecem em `build/validation-classic-spacing-2.3.1.3/`.
