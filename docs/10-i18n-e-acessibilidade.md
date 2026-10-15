# Internacionalização e acessibilidade

## Estado atual

Os controles são Qt Quick Controls/Kirigami, têm foco nativo, nomes acessíveis nos botões e não dependem apenas de cor para os estados principais. A navegação usa botões reais, o player possui controles de play/pause, seek e volume, e itens indisponíveis ficam com texto explicativo.

O idioma inicial está em português do Brasil. A camada de tradução completa com `qsTr`/TS ainda é uma pendência objetiva: as strings da primeira migração continuam literais em QML/C++. Antes de publicar traduções adicionais, deve-se extrair as strings com `lupdate`, criar `i18n/mainuan-welcome_pt_BR.ts` e substituir mensagens de backend por `tr()`.

## Revisão necessária em sessão real

Ainda precisam de validação com Orca/KDE screen reader, teclado sem mouse, High DPI 100–250%, contraste personalizado, daltonismo, fontes grandes, preferência de menos animação e layouts de 1280×720 a 3840×2160. A estrutura usa layouts responsivos e a sidebar colapsa abaixo de 1060 px, mas isso não substitui teste visual em todas as escalas.
