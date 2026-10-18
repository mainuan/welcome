# Correções funcionais dos layouts — 2.3.1.1

> Registro histórico do desenvolvimento experimental. Os números 2.x, resultados de execução e caminhos de capturas descrevem aquele momento. A primeira versão oficial é **1.0**; consulte a [documentação atual](README.md) antes de seguir instruções antigas.

## Diagnóstico e mudanças

A implementação anterior reproduzia as referências parcialmente: `three-panels` recebia três larguras proporcionais com limites iguais, impedindo ajuste ao conteúdo; o Clássico acrescentava `org.kde.plasma.kdeaichat`, usava ajuste ao conteúdo no quarto segmento e aceitava quatro painéis quando os widgets informativos faltavam. O Lomiri tinha dock com comprimento personalizado e espessuras de 34/52 pixels. Tiling e os segmentos laterais de três barras também tinham espessuras diferentes. O gerador igualava mínimo/máximo em todos os casos personalizados e ainda impunha `panel.length`, que representa o comprimento medido dos conteúdos.

Foram lidos os dois arquivos da nova pasta `classico-5-barras`, além das referências de `3-barras`, `barra-unica` e `lomiri`. A referência Clássico agora integra o mesmo mapeamento saneado de `reference-layouts.js`. Não são transportados IDs, atividades, históricos, geometrias por resolução ou caminhos pessoais. O arquivo original contém um caminho de downloads do ChatAI: ele foi explicitamente excluído da conversão.

A ordem selecionável é: Clássico — 5 barras (`plasma-default`), 3 barras (`three-panels`), Barra única (`single-bar`), Tiling (`tiling`), Painel superior (`panel-top`), Lomiri (`unity`) e Barra W7 (`minimal`). `floating` foi removido do modelo, da geração e das miniaturas. Ele permanece reconhecido somente para leitura, sem migrar, excluir ou redimensionar painéis na inicialização. A altura antiga de um painel legado é preservada até uma escolha explícita de outro layout.

## Clássico e comprimento dos painéis

A ordem dos cinco segmentos vem da referência corrigida. A ordem interna segue `AppletOrder`, inclusive o separador antes do Kickoff no primeiro segmento:

1. Separador e Kickoff (ou a alternativa Kicker já suportada).
2. Chaac.Complete.Weather e com.mike.desktop.
3. Gerenciador de tarefas com os cinco aplicativos fixados da referência.
4. ChatAI-Plasmoid, org.kde.plasma.systemtray, KdeControlStation.
5. Relógio e org.kde.plasma.powershowdesktop.

Não há adição de `org.kde.plasma.kdeaichat`. Todos os widgets do Clássico são obrigatórios para esta opção: a ausência de qualquer um cancela a troca antes do backup/remoção. A descrição posterior também verifica presença e quantidade exata de cinco segmentos por tela. Não há mais sucesso com quatro painéis ou com um segmento necessário vazio.

| Layout/segmento | Modo de comprimento |
|---|---|
| Clássico: menu, tarefas, relógio | `fit` |
| Clássico: informações | `custom`, intervalo da referência ajustado de 38 para 40 pixels de espessura |
| Clássico: ChatAI/bandeja/controles | `custom`, intervalo nominal 251–339 ajustado pela razão 40/38 e limitado ao espaço da tela |
| 3 barras: todos os três painéis | `fit`, alinhados à esquerda, centro e direita |
| Barra única / Painel superior | `custom`, centralizados com margens laterais adaptativas |
| Tiling / Barra W7 | `fill` horizontal |
| Lomiri: topo e lateral | `fill`, horizontal e vertical respectivamente |

A constante `PANEL_THICKNESS = 40` controla a espessura de todos os painéis novos; em painéis laterais, `height` significa largura. Não há alteração de escala ou DPI.

`minimumLength`, `maximumLength` e `offset` são escritos somente para segmentos `custom`; `panel.length` não é imposto. Mínimo e máximo distintos permitem redimensionamento real no Clássico. Os offsets dos segmentos informativo e da bandeja são calculados a partir dos comprimentos medidos dos painéis vizinhos; o máximo da bandeja também respeita o espaço reservado, permitindo crescimento sem invadir as tarefas dentro desse intervalo.

Após uma aplicação explícita do Clássico ou de 3 barras, o serviço aguarda assincronamente duas amostras de medidas iguais e valida a separação. São até dez consultas espaçadas por 200 ms. Se não houver espaço ou estabilização, utiliza o rollback existente; a interface permanece responsiva. Essa etapa não é executada ao abrir o Welcome ou consultar o estado. Os alinhamentos nativos de `fit` são mantidos, sem offsets compensatórios nem comprimentos artificiais por resolução.

As medidas de conteúdo vêm de `panel.length`; o tema ainda pode acrescentar moldura/margem flutuante. Os testes simulam variações de medidas e verificam espaço de segurança, mas não equivalem à renderização do compositor. O código não monitora permanentemente mudanças posteriores dos conteúdos (por exemplo, muitas novas tarefas) ou troca de monitores; essas situações dependem também do comportamento nativo do Plasma e precisam de teste visual.

Semântica verificada na [API pública de scripting](https://develop.kde.org/docs/plasma/scripting/api/) e no [PanelView do Plasma 6.6](https://invent.kde.org/plasma/plasma-workspace/-/blob/Plasma/6.6/shell/panelview.cpp). `fit` deriva a extensão do conteúdo, `fill` ocupa a dimensão disponível e `custom` limita a extensão a um intervalo. `fit` não oferece deslocamento útil na implementação Wayland consultada; não foram adicionados limites para fingir esse comportamento.

## Ícones: opções suportadas e limites

| Widget | Ajuste ou limitação |
|---|---|
| Bandeja do sistema | `General/scaleIconsToFit=true`, para acompanhar a espessura; `iconSpacing=1` reduz espaçamento excessivo. |
| Tarefas somente com ícones | `General/maxStripes=1` e `iconSpacing=1`; `fill=false` quando o painel usa `fit`, evitando expansão do widget. O ícone segue a espessura/métricas nativas. |
| Kickoff | Não oferece uma opção separada para tamanho do ícone compacto no esquema consultado. Recebe o painel de 40 pixels; não foram inventadas chaves. |
| TahoeLauncher | A representação compacta segue o espaço do painel. `appsIconSize` configura o menu aberto, não o ícone do painel; não foi usado para tentar redimensioná-lo. |
| Orbit.Start.Menu | A representação compacta preenche seu contêiner. `appIconSize`/`appsIconSize` herdados da referência não existem no esquema instalado e foram removidos. Não há chave de tamanho compacto. |
| ChatAI-Plasmoid | Usa `Kirigami.Units.iconSizes.medium` e o espaço disponível. Oferece escolha da imagem/estilo, sem tamanho compacto independente; configuração funcional preservada. |
| KdeControlStation | O ícone compacto preenche a representação. A opção `Appearance/scale` afeta o popup e não foi alterada para simular tamanho do ícone. |
| Pesquisa/Milou | Representação nativa, sem tamanho independente de ícone no esquema consultado. |
| Chaac.Complete.Weather | Oferece tamanho de fonte, sem controle independente de tamanho do ícone compacto. Fonte e funcionamento preservados. |
| com.mike.desktop | Configura pílulas, números e textos; não é um ícone equivalente a um lançador. Seus parâmetros da referência foram preservados. |

As opções nativas foram conferidas nos esquemas oficiais de [taskmanager](https://invent.kde.org/plasma/plasma-desktop/-/blob/Plasma/6.6/applets/taskmanager/main.xml) e [systemtray](https://invent.kde.org/plasma/plasma-workspace/-/blob/Plasma/6.6/applets/systemtray/main.xml). Os esquemas e representações compactas dos widgets de terceiros foram lidos nos respectivos diretórios instalados em `/usr/share/plasma/plasmoids/`. Nenhum arquivo interno desses widgets foi modificado. Não há chave universal `iconSize`, mudança de DPI, de ícones do Dolphin ou de tema Kora.

## Segurança e testes

Backup, retenção e restauração continuam no mesmo serviço. A verificação de widgets precede qualquer remoção; validação de estrutura e medidas precede a confirmação. Rollback e restauração reiniciam o Plasma apenas pelos mecanismos existentes. Todos os testes usam o backend simulado e arquivos temporários; não aplicam painéis, temas ou avatares na sessão real.

A suíte cobre sete opções em ordem, cinco segmentos completos, ordem do quarto segmento e ausência de kdeaichat, intervalos personalizados, três barras em fit sem escrita de mínimos/máximos/offset, todas as espessuras em 40, Lomiri em fill, menus Orbit/Tahoe, ícones suportados, estado legado floating sem modificação, rollback por erro D-Bus/estrutura/sobreposição e responsividade enquanto aguarda medidas. As simulações incluem 1280×720, 1366×768 e 1920×1080, duas telas e variações no tamanho intrínseco dos ícones. QML verifica modelo, nomes, seleção, acessibilidade, sete cards e ausência da antiga miniatura.

**Validação gráfica ainda obrigatória:** testar os sete layouts em Wayland, especialmente margens e flutuação em Dream/Tahoe/Breeze, extensões reais de relógio/bandeja/informações, carregamento tardio dos plasmoids, variações na quantidade de tarefas e telas com escalas diferentes. Confirmar que o quarto segmento do Clássico redimensiona dentro do intervalo e que as três barras acompanham o conteúdo. A harmonização não garante ícones visualmente idênticos entre plasmoids com métricas próprias. Os testes automatizados não encerram essa validação funcional.

## Versões incrementais dos pacotes de teste

A base continua 2.3.1. Cada novo pacote de teste recebe `2.3.1.N`, começando em `2.3.1.1`. Antes de gerar outro pacote, conferir o maior sufixo já utilizado nos metadados dos `.deb`, nas versões instaladas e no changelog, e incrementar N. Preservar os artefatos anteriores, atualizar CMake e changelog juntos e conferir o resultado com `dpkg-deb -f arquivo.deb Version`. Nunca renomear um pacote para simular versão diferente.

Para esta entrega, foram pesquisados os pacotes em `/home/daigo/Github` e `/home/daigo/Downloads`, além da versão instalada e do changelog: somente 2.3.1 estava em uso. O pacote anterior e seu hash foram registrados para verificar sua preservação.

## Resultado desta entrega

- `cmake -S . -B build`: sucesso.
- `cmake --build build -j"$(nproc)"`: sucesso.
- `ctest --test-dir build --output-on-failure`: 4/4 aprovados, sem falhas ou testes ignorados.
- `dpkg-buildpackage -us -uc -b`: sucesso; repetiu os quatro testes no build Release. QtTest registrou 45 resultados em tst_mainuan, 12 em tst_layouts e 4 em QML, incluindo inicialização/finalização.
- `dpkg-deb -f`: `mainuan-welcome`, versão **2.3.1.1**, arquitetura **amd64**. CMake, changelog e versão do pacote coincidem.
- Conteúdo do pacote inspecionado: sem configurações pessoais, arquivos originais de referência, temas Kora, esquemas ou avatares externos. Os bytes dos scripts e dos ícones FreeOffice/Vivaldi/Zen incorporados ao executável extraído foram conferidos contra os fontes.
- `lintian`: sem saída, sucesso. `dpkg-shlibdeps` manteve uma observação sobre a ligação existente com QtQuick, sem impedir o empacotamento.
- `git diff --check`: sem erros. O repositório permanece com alterações locais anteriores e as desta etapa, sem commits.
- Hashes dos serviços protegidos, script KWin, configurações pessoais e avatar: idênticos aos anteriores. O pacote **2.3.1** anterior continua presente, com o mesmo hash.

Pacote final: `/home/daigo/Github/mainuan-welcome_2.3.1.1_amd64.deb` (536694 bytes).

O empacotamento existente calcula dependências contra Qt 6.10.2, incluindo as ABIs privadas Qt dessa versão. O sistema de destino precisa satisfazer os metadados registrados em `package-fields.txt`.

Instalação manual:

```bash
sudo apt install /home/daigo/Github/mainuan-welcome_2.3.1.1_amd64.deb
```

Se essa mesma versão já estiver instalada, acrescentar `--reinstall` ao comando. Não houve instalação automática, commit, push ou publicação. Logs e verificações completos em `build/validation-layout-fixes-2.3.1.1/`.

## Arquivos modificados nesta etapa

- `CMakeLists.txt`
- `README.md`
- `debian/changelog`
- `qml/components/LayoutPreview.qml`
- `src/layouts/desktop-layouts.js`
- `src/layouts/reference-layouts.js`
- `src/models/LayoutModel.cpp`
- `src/models/LayoutModel.h`
- `src/services/LayoutService.cpp`
- `src/services/LayoutService.h`
- `tests/fixtures/plasma-layout-mock.js`
- `tests/qml/tst_Layouts.qml`
- `tests/tst_layouts.cpp`
- `tests/tst_mainuan.cpp`
- `docs/16-layouts-reference-conversion.md`
- `docs/17-layout-functional-fixes.md`
