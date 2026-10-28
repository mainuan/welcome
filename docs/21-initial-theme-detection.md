# Correção de Claro/Escuro — Mainuan Welcome 1.0.1

## Diagnóstico e reprodução

O código 1.0 lia `kdeglobals` e `plasmarc` assumindo que o ambiente sempre
incluía `~/.config/kdedefaults` em `XDG_CONFIG_DIRS`. O Plasma guarda ali os
seletores dos temas globais; o arquivo local pode conter a paleta sem uma chave
`General/ColorScheme`. Um lançador sem essa variável completa deixava `accentId`
vazio e podia identificar Breeze em vez do estilo herdado. Selecionar uma cor
no Welcome gravava o seletor local e tornava as operações seguintes funcionais.

A configuração local examinada, somente para leitura, tinha `ColorScheme` em
`kdedefaults`; o ambiente do terminal já incluía esse diretório. Não alteramos a
sessão real para forçar o erro. No `PlasmaSandbox`, antes da correção, o caso
Ciano Claro/Dream passou com o seletor local e com herança explicitamente
exportada, mas falhou com o mesmo seletor herdado sem essa exportação:
`accentId` era vazio em vez de `cyan`. Também falhou o teste que exige preservar
um esquema externo quando o estilo global é conhecido.

Os testes antigos de alternância chamavam `setAccent()` antes de `setTheme()`;
essa preparação escrevia justamente a chave cuja ausência era relevante.
O comportamento de defaults do Plasma está documentado no código do
[KLookAndFeelManager](https://invent.kde.org/plasma/plasma-workspace/-/blob/master/libklookandfeel/klookandfeelmanager.cpp),
que grava os defaults e reverte os seletores locais.

## Correção

A leitura usa KConfig com fontes explícitas, nesta precedência:

1. Configuração local do usuário.
2. `kdedefaults` do mesmo diretório de configuração.
3. Demais diretórios XDG, na ordem configurada.

A detecção não grava arquivos, não altera variáveis da sessão e não infere a
identidade pelo hexadecimal. Os IDs e as doze variantes continuam centralizados
em `AccentProfile`. Os nomes relativos dos configs preservam notificações D-Bus
KConfig; watchers adicionais acompanham alterações somente nos defaults.

`setTheme()` relê o estado efetivo, recusa esquemas desconhecidos e preserva a
família reconhecida antes de iniciar a operação. A conclusão do tema global
continua sendo aguardada e verificada antes da reaplicação do esquema solicitado.
O estado final é relido também em caso de erro. A troca apenas de modo não
reescreve o avatar ou seu backup, mantém o wallpaper se ele não foi substituído
e restaura Kora quando o tema global o altera. A seleção explícita de cor e a
troca explícita de estilo conservam seus fluxos existentes.

## Cobertura e limites

A matriz de regressão tem 108 casos: seis cores, três estilos, duas variantes
iniciais e três fontes de configuração. Cada caso alterna nos dois sentidos,
recria o serviço e repete sem chamar `setAccent()`. As fixtures também simulam
um tema global que escreve seletores em `kdedefaults`, como o Plasma real.

Outros testes cobrem precedência local/tema/sistema, notificações externas no
arquivo local e nos defaults, esquemas desconhecidos, destino ausente, falhas
dos comandos e preservação do avatar/wallpaper. Nenhum teste usa a sessão real,
instala programas ou altera configurações pessoais.

A versão da aplicação, Debian e AppStream é 1.0.1; a API QML continua 1.0.
O workflow e os gates x86-64 básico permanecem sem alterações. A tag `v1.0`
não é movida e a futura `v1.0.1` depende da decisão do mantenedor.

A simulação de recriação do serviço não substitui um logout/login real. Ainda
é necessário validar na sessão Mainuan/Wayland: abrir o Welcome com cada cor
já definida nas Configurações do Sistema, alternar diretamente Claro/Escuro,
reabrir o aplicativo e repetir após login. Conferir Dream/Tahoe/Breeze, paleta,
seletores, ícones e preservação do wallpaper/avatar personalizados. Os temas e
recursos Mainuan não fazem parte do ambiente de testes isolado.
