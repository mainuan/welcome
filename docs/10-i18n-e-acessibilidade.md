# Internacionalização e acessibilidade

> Registro histórico do desenvolvimento experimental. Os números 2.x, resultados de execução e caminhos de capturas descrevem aquele momento. A primeira versão oficial é **1.0**; consulte a [documentação atual](README.md) antes de seguir instruções antigas.

## Estado atual

Os controles são Qt Quick Controls/Kirigami, têm foco nativo, nomes acessíveis nos botões e não dependem apenas de cor para os estados principais. A navegação usa botões reais, o player possui controles de play/pause, seek e volume, e itens indisponíveis ficam com texto explicativo.

A interface usa KI18n com msgid em inglês e o catálogo `po/pt_BR.po`; veja [i18n/README.md](i18n/README.md). (O plano antigo com `qsTr`/TS foi substituído.)

## Revisão necessária em sessão real

Ainda precisam de validação com Orca/KDE screen reader, teclado sem mouse, High DPI 100–250%, contraste personalizado, daltonismo, fontes grandes, preferência de menos animação e layouts de 1280×720 a 3840×2160. A estrutura usa layouts responsivos e a sidebar colapsa abaixo de 1060 px, mas isso não substitui teste visual em todas as escalas.
