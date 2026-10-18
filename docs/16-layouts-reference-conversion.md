# Oito layouts do Mainuan Welcome 2.3.1

> Registro histórico do desenvolvimento experimental. Os números 2.x, resultados de execução e caminhos de capturas descrevem aquele momento. A primeira versão oficial é **1.0**; consulte a [documentação atual](README.md) antes de seguir instruções antigas.

Este documento registra a implementação anterior. As correções posteriores, a lista de sete opções e os modos de comprimento atuais estão em [Layouts 2.3.1.1](17-layout-functional-fixes.md).

## Referências e escopo

Os dez arquivos de referência foram encontrados e lidos: `plasma-org.kde.plasma.desktop-appletsrc` e `plasmashellrc` nas pastas `3-barras`, `barra-unica`, `lomiri`, `painel-superior` e `barra-w7` de `/home/daigo/Github/mainuan-layout-references/`.

Somente a ordem de widgets, aplicativos fixados, configurações funcionais e características dos painéis foram convertidas. `src/layouts/reference-layouts.js` contém essa representação estática saneada, incorporada como recurso Qt. Os arquivos originais não são instalados. IDs de containments/applets/atividades, mapeamentos de telas, histórico de migração, tamanhos de diálogos e caminhos da sessão capturada não foram transportados. O lançador do Jitsi foi normalizado para `applications:org.jitsi.jitsi-meet.desktop`, independente da revisão do Flatpak.

As funcionalidades de aplicativos, cores KDE, Kora, wallpapers e avatares da versão local anterior foram preservadas. O script KWin de Tiling e os corpos das implementações Clássico, Flutuante e Tiling permanecem iguais aos anteriores.

## Ordem definitiva

| Ordem | Nome | ID | Painéis por tela | Lançador obrigatório |
|---|---|---|---|---|
| 1 | Clássico — 5 barras | `plasma-default` | 5* | Kickoff ou Kicker |
| 2 | 3 barras | `three-panels` | 3 | Kickoff ou Kicker |
| 3 | Barra única | `single-bar` | 1 | Kickoff ou Kicker |
| 4 | Tiling | `tiling` | 1 | Kickoff ou Kicker |
| 5 | Painel superior | `panel-top` | 1 | `TahoeLauncher` |
| 6 | Flutuante | `floating` | 1 | Kickoff ou Kicker |
| 7 | Lomiri | `unity` | 2 | `Orbit.Start.Menu` |
| 8 | Barra W7 | `minimal` | 1 | Kickoff ou Kicker |

*O Clássico conserva seu comportamento anterior: quando faltam tanto `com.mike.desktop` quanto `Chaac.Complete.Weather`, omite o segmento informativo vazio e mantém quatro segmentos funcionais. Com os widgets do Mainuan, conserva cinco. Não foi redesenhado nesta etapa.

A ordem vem de `LayoutService::layoutIds()`, utilizada pelo modelo; não depende de estado temporário nem da ordenação de uma tabela hash.

## Conversão dos widgets

- **3 barras:** esquerda: Kickoff, Chaac, Milou, informações `com.mike.desktop`; centro: tarefas com seis aplicativos fixados; direita: ChatAI, bandeja, KdeControlStation, relógio, exibição do desktop e espaçador fixo. As três barras continuam presentes mesmo se widgets opcionais estiverem ausentes.
- **Barra única:** espaçador, Kickoff, Chaac, Milou, informações, espaçador expansível, tarefas, espaçador expansível, ChatAI, bandeja, controles, relógio, desktop e espaçador final. Painel inferior flutuante personalizado com margens laterais.
- **Painel superior:** ordem da referência com TahoeLauncher e espaçador fixo antes das tarefas, painel superior flutuante personalizado. A miniatura anterior foi preservada.
- **Lomiri:** topo: menu global, espaçadores expansíveis, relógio, ChatAI, bandeja, controles e desktop. Lateral esquerda: espaçador fixo, Orbit, tarefas, espaçador expansível, lixeira e espaçador final. Ambos não flutuantes, conforme a referência.
- **Barra W7:** espaçador, Kickoff, espaçador, Milou, informações, tarefas, ChatAI, bandeja, controles, relógio, desktop e espaçador final. Painel inferior de largura total, sem flutuação e sem ocultação.

Plugins opcionais das referências: `Chaac.Complete.Weather`, `org.kde.milou`, `com.mike.desktop`, `ChatAI-Plasmoid`, `KdeControlStation`, `org.kde.plasma.icontasks`, `org.kde.plasma.systemtray`, `org.kde.plasma.digitalclock`, `org.kde.plasma.powershowdesktop`, `org.kde.plasma.panelspacer`, `org.kde.plasma.appmenu` e `org.kde.plasma.trash`. Clássico/Flutuante/Tiling também mantêm seus plugins anteriores, como `org.kde.plasma.marginsseparator`, `org.kde.plasma.pager`, `org.kde.plasma.showdesktop` e `org.kde.plasma.kdeaichat`. A bandeja recebe a lista de itens extras/ocultos da referência; a disponibilidade de cada item depende da instalação do sistema. Nenhum plugin é instalado pelo Welcome.

Configurações úteis de menus, previsão, pílulas de informações, bandeja, relógio, controles, ChatAI e espaçadores foram preservadas. Dados de migração/favoritos antigos, registros de itens conhecidos e posições de janelas não são reproduzidos. A aparência de fontes, ícones e bordas depende dos recursos instalados e do tema Plasma.

## Geometria e API Plasma 6

A implementação utiliza somente scripting do Plasma; não escreve diretamente nos arquivos ativos da sessão. As propriedades foram conferidas na [API de scripting do KDE](https://develop.kde.org/docs/plasma/scripting/api/) e no [código de Panel do Plasma 6.6](https://invent.kde.org/plasma/plasma-workspace/-/blob/Plasma/6.6/shell/scripting/panel.cpp).

As medidas de `screenGeometry()` são tratadas como pixels lógicos, sem aplicar a escala física novamente. A criação é repetida em cada tela; gerenciadores de tarefas secundários mostram somente as janelas da respectiva tela.

Nas três barras, margens e intervalos usam aproximadamente 1,8% da largura lógica, com mínimos de 10 e 12 pixels. Os segmentos laterais têm comprimentos iguais e o central ocupa o restante. Os laterais se ancoram nas extremidades com margem; o centro permanece com deslocamento zero. Barra única e Painel superior deixam a mesma margem lateral adaptativa. Espessuras seguem as referências: 38/40 pixels nos três segmentos, 40 nas barras únicas, 34 no topo e 52 na lateral do Lomiri.

O modo `fit` do [PanelView do Plasma 6.6](https://invent.kde.org/plasma/plasma-workspace/-/blob/Plasma/6.6/shell/panelview.cpp) ignora deslocamentos e limitações de comprimento. Por isso, os três segmentos e a lateral Lomiri usam `custom` com comprimentos calculados e limites iguais, evitando importar os deslocamentos fixos das capturas. O topo do Lomiri e a Barra W7 usam `fill`. O dock Lomiri fica abaixo do painel superior, com margens verticais. No código do Plasma 6.6, o alinhamento `left` de um painel vertical corresponde à ancoragem superior. Nenhuma geometria histórica por resolução é copiada.

Essas dimensões são recalculadas quando o layout é aplicado. Mudanças posteriores de resolução/monitores usam o comportamento nativo do Plasma; não foi adicionado um serviço permanente de reposicionamento. Arredondamento e eventuais margens adicionais de painéis flutuantes ficam a cargo de Dream, Tahoe ou Breeze. O ajuste visual final e os limites intrínsecos de widgets de terceiros precisam ser conferidos na sessão real.

## Aplicação, identificação e recuperação

1. Validar o ID em uma lista fechada.
2. Consultar o Plasma assincronamente para verificar o lançador exigido e a geometria das telas. Orbit e Tahoe nunca recebem substituição silenciosa por Kickoff.
3. Guardar o backup existente antes de remover painéis.
4. Repetir a verificação imediatamente antes da remoção e criar os painéis pela API.
5. Consultar os painéis reais, verificando tags, papéis dos segmentos, quantidade por tela, borda, flutuação e identidade/quantidade de lançadores. A conclusão da chamada D-Bus isoladamente não confirma sucesso.
6. Restaurar o backup se a aplicação ou a validação falhar. O reinício do Plasma continua restrito à recuperação/restauração, como antes. A aplicação normal não reinicia o Plasma.

A descrição reconhece Orbit e Tahoe, além dos menus Plasma tradicionais. Ao reabrir, o Welcome consulta novamente a estrutura; layouts antigos que tenham somente a tag, mas não correspondam à nova estrutura/lançador, não recebem confirmação indevida de aplicação. O fechamento normal da janela é adiado enquanto uma aplicação/restauração estiver em andamento.

O backend injetável é exclusivamente uma interface C++ para testes; não expõe execução arbitrária ao QML, variáveis de ambiente ou scripts do usuário. IDs e plugins interpolados são validados e codificados como JSON. Não há shell, sudo ou instalação de widgets no fluxo de layout.

## Testes e validação manual

`tst_layouts` executa os scripts reais em QJSEngine com um Plasma simulado e configurações temporárias. Cobre oito opções/modelo/nomes, contagem por tela, menus customizados, widgets/configurações, lista de aplicativos fixados, geometrias em 1280×720, 1366×768, 1920×1080 e 960×540 lógicos, duas telas, widgets ausentes, identificação ao reabrir, falhas D-Bus/estrutura, ordem de backup e rollback/restauração. O serviço KWin real nunca é chamado pelos testes de layout.

`layouts_qml` instancia a página real com serviços simulados, verifica ordenação dos cards, nomes acessíveis, botão Aplicar, indicador de seleção e duas novas miniaturas. Roda offscreen sem barramento/display da sessão. O teste é opcional quando qmltestrunner ou os módulos de teste não estão instalados. `tst_mainuan` e `smoke_startup` preservam a cobertura existente.

Ainda requerem validação gráfica: renderização de Orbit/Tahoe e demais plugins de terceiros, bandeja e seus itens, fontes, encaixe visual em Dream/Tahoe/Breeze, hotplug/escala de monitores e comportamento KWin em Wayland. Não foi aplicado nenhum layout na sessão real do desenvolvedor.

Somente as miniaturas novas de 3 barras e Barra única foram adicionadas. As outras foram mantidas. Aproximar futuramente a miniatura Flutuante da borda inferior continua uma pendência deliberada.

## Entrega e arquivos desta etapa

Arquivos modificados: `CMakeLists.txt`, `README.md`, `debian/changelog`, `qml/Main.qml`, `qml/pages/AppearancePage.qml`, `qml/components/LayoutPreview.qml`, `src/layouts/desktop-layouts.js`, `src/models/LayoutModel.cpp`, `src/models/LayoutModel.h`, `src/services/LayoutService.cpp`, `src/services/LayoutService.h` e `tests/tst_mainuan.cpp`.

Arquivos criados: `src/layouts/reference-layouts.js`, `tests/tst_layouts.cpp`, `tests/fixtures/plasma-layout-mock.js`, `tests/layouts-qml.sh`, `tests/qml/tst_Layouts.qml` e este documento. As demais alterações locais exibidas por `git status` já existiam antes desta etapa.

Validação em 17/10/2026:

- `cmake -S . -B build` e `cmake --build build -j"$(nproc)"`: sucesso.
- `ctest --test-dir build --output-on-failure`: quatro testes aprovados, sem falhas ou testes ignorados.
- `dpkg-buildpackage -us -uc -b`: sucesso, com os quatro testes repetidos no build Release. Totais QtTest: 45 em `tst_mainuan`, 9 em `tst_layouts` e 4 na suíte QML, incluindo inicialização/finalização.
- `dpkg-deb --info/--contents/--extract`: versão 2.3.1, amd64, recursos e metadados inspecionados. O pacote não contém arquivos pessoais, configurações originais das referências, temas Kora, esquemas `.colors` ou avatares externos.
- Os bytes dos três scripts Plasma/KWin e dos ícones FreeOffice/Vivaldi/Zen foram comparados com os recursos registrados pelo executável extraído do pacote: todos idênticos aos fontes. A sonda de validação termina antes de executar `main()` e não integra o pacote.
- `lintian`: saída vazia, sem erros ou alertas. `dpkg-shlibdeps` emitiu somente uma observação sobre a ligação já existente com QtQuick; não houve mudança de dependências para contorná-la.
- `git diff --check`: sucesso, sem saída. O repositório permanece com alterações locais e arquivos novos, incluindo os anteriores; nenhum commit foi criado.
- Os hashes das configurações Plasma/KDE e de `.face` monitorados antes/depois permaneceram iguais.

Pacote: `/home/daigo/Github/mainuan-welcome_2.3.1_amd64.deb` (533418 bytes). SHA-256: `1b95f7ee37f02d2ba5da91e6c61c5af56d8e41a3d5fe375e991d2b4d19e60b58`.

O binário foi construído com Qt 6.10.2 e os metadados incluem as ABIs privadas Qt dessa versão, determinadas automaticamente pelo empacotamento existente. A instalação de destino precisa satisfazer essas dependências.

Instalação ou reinstalação manual da mesma versão:

```bash
sudo apt install --reinstall /home/daigo/Github/mainuan-welcome_2.3.1_amd64.deb
```

Logs, conteúdo do pacote, recursos conferidos e `git status --short` completo estão em `build/validation-layouts-2.3.1/`. Os intermediários do empacotamento foram limpos com `debian/rules clean`; o pacote e os registros foram preservados. Nada foi instalado, commitado, enviado ou publicado.
