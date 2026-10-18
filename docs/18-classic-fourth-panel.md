# Quarta barra do Clássico — 2.3.1.2

> Registro histórico do desenvolvimento experimental. Os números 2.x, resultados de execução e caminhos de capturas descrevem aquele momento. A primeira versão oficial é **1.0**; consulte a [documentação atual](README.md) antes de seguir instruções antigas.

## Causa e correção restrita

O modo `custom` já era aplicado corretamente. O erro confirmado estava nos limites calculados pelo Welcome: a largura mínima de 251 era multiplicada por 40/38, tornando-se 264, e o máximo combinava essa mesma escala com 27% da largura da tela. Além disso, o posicionamento podia reduzir esse máximo posteriormente, sem reservar antes todo o intervalo. Espessura e comprimento são grandezas independentes: passar o painel de 38 para 40 pixels não exige alargar seus widgets nessa proporção.

A alteração afeta somente o perfil `tray` do Clássico e sua reserva de espaço. A composição permanece ChatAI-Plasmoid, org.kde.plasma.systemtray e KdeControlStation, nessa ordem, sem espaçadores e sem kdeaichat. `reference-layouts.js`, configurações de ícones, demais perfis, serviços, QML e os outros quatro segmentos não foram modificados.

O mínimo passa a 251 pixels lógicos. O máximo é calculado por:

```text
expansão = limitar((largura_da_tela - 1280) / (1366 - 1280), 0, 1)
máximo = arredondar(251 + (339 - 251) × expansão)
```

| Largura lógica testada | Mínimo | Máximo nominal |
|---|---:|---:|
| 1280 | 251 | 251 |
| 1323 | 251 | 295 |
| 1366 | 251 | 339 |
| 1920 | 251 | 339 |
| 2560 | 251 | 339 |

Isso reproduz os dois pontos da captura, interpola entre eles e conserva o intervalo compacto em monitores maiores. A espessura continua em 40 pixels. O máximo é um limite de redimensionamento, não a promessa de que a largura desenhada será sempre 339. O conteúdo, as molduras do tema e os limites do Plasma determinam o tamanho efetivo dentro do intervalo.

Somente o offset da quarta barra pode ser aproximado do relógio para reservar seu intervalo completo antes da limitação já existente entre vizinhos. Essa limitação ainda pode reduzir o máximo se a geometria real exigir. Se não houver espaço para a composição, as verificações existentes recusam a aplicação e utilizam o rollback. Também foi acrescentada uma verificação local das larguras efetivamente alocadas aos três widgets: uma soma que exceda o máximo não recebe confirmação de sucesso.

## Referência e API

Foram consultados os dois arquivos de `classico-5-barras`, usando o painel 83 somente para localizar os dados. Nenhum ID da captura foi incorporado ao código.

Há uma divergência entre o texto do pedido e o arquivo fornecido: este registra `alignment=2`, correspondente a `Qt::AlignRight`; `center` corresponde a `Qt::AlignCenter`. Foi preservada a ancoragem direita do segmento entre tarefas e relógio. Centralizá-lo na tela inteira o sobreporia à barra de tarefas e o deslocamento de um painel centralizado não reproduz essa posição no Wayland consultado.

A semântica foi conferida no [wrapper de Panel do Plasma 6.6](https://invent.kde.org/plasma/plasma-workspace/-/blob/Plasma/6.6/shell/scripting/panel.cpp) e em [PanelView](https://invent.kde.org/plasma/plasma-workspace/-/blob/Plasma/6.6/shell/panelview.cpp). `custom` limita o tamanho de conteúdo por mínimo/máximo; os deslocamentos usados aqui são suportados para a ancoragem direita. Não foi introduzida escrita direta em configurações ativas.

A inspeção da sessão em execução foi somente de leitura, consultando modo, limites e geometrias dos widgets pela API. Ela estava em outro painel personalizado, sem o Clássico ativo. Portanto, não foi possível atribuir a um widget específico uma largura mínima real da quarta barra. Essa inspeção também mostrou que `panel.length` pode retornar uma preferência não limitada (16777215), e não uma medição física do painel desenhado; a verificação adicional usa `widget.geometry.width`. Não foram alteradas preferências dos widgets para tentar corrigir isso.

## Testes, escopo e validação visual

A suíte QtTest adiciona cinco cenários de largura e uma segunda tela de 1280. Verifica limites antes/depois da estabilização, modo custom, espessura 40, ordem/configurações preservadas, medidas com duas variações de ícones, espaço para vizinhos, repetição sem redução inesperada do intervalo e rejeição de overflow observável.

Uma auditoria separada executou os scripts anterior e corrigido em QJSEngine com o mesmo Plasma simulado. Em 1280, 1366 e 1920, os seis outros layouts e os quatro demais segmentos do Clássico produziram propriedades e configurações idênticas. Os hashes dos demais fontes e dos recursos também foram conferidos. Nenhum layout foi aplicado na sessão real por esses testes.

**Pendente de validação visual:** reaplicar o Clássico explicitamente em Wayland e conferir o espaço, moldura flutuante, carregamento dos três widgets e quantidade real de itens da bandeja. Os limites da tabela foram verificados em simulação; não são medições de uma nova aplicação gráfica real.

## Arquivos desta etapa

- `src/layouts/desktop-layouts.js`: somente limites, posicionamento e verificação da quarta barra.
- `tests/tst_layouts.cpp`: testes específicos dos limites e de overflow.
- `CMakeLists.txt`: versão 2.3.1.2.
- `debian/changelog`: entrada 2.3.1.2.
- `docs/18-classic-fourth-panel.md`: este relatório.

## Resultado e pacote

- CMake e compilação solicitados: sucesso.
- CTest: 4/4 aprovados, incluindo QtTest, QML e smoke de inicialização, sem testes ignorados.
- Empacotamento `dpkg-buildpackage -us -uc -b`: sucesso, repetindo os quatro testes. QtTest: 45 resultados no teste geral, 17 no de layouts e 4 em QML, incluindo inicialização/finalização.
- `dpkg-deb -f`: versão interna **2.3.1.2**, arquitetura **amd64**; CMake e changelog sincronizados.
- `lintian`: sem saída, sucesso. A observação já existente de dpkg-shlibdeps sobre ligação com QtQuick não impede o pacote e não foi tratada fora deste escopo.
- Conteúdo e recursos do executável extraído conferidos. Os três scripts e os ícones anteriores incorporados correspondem aos fontes. Nenhuma configuração pessoal ou referência original entrou no pacote.
- `git diff --check`: sucesso, sem saída. Alterações locais anteriores preservadas; sem commit.
- Hashes dos arquivos pessoais e do pacote anterior 2.3.1.1: preservados.

Pacote: `/home/daigo/Github/mainuan-welcome_2.3.1.2_amd64.deb` (537248 bytes).

```bash
sudo apt install /home/daigo/Github/mainuan-welcome_2.3.1.2_amd64.deb
```

Nada foi instalado automaticamente. Não houve commit, push ou publicação. Os registros completos, comparação de escopo, inspeção somente de leitura e checksums estão em `build/validation-classic-tray-2.3.1.2/`. Os intermediários do empacotamento foram limpos, preservando o pacote final e os anteriores existentes.
