# Arquitetura e manutenção — Mainuan Welcome 1.0.1

## Estrutura e recursos

`src/services/` integra o sistema por APIs Qt/KDE e processos com argumentos
separados. `src/models/` fornece os catálogos; `qml/` contém a interface Kirigami.
`src/layouts/` contém os scripts Plasma/KWin incorporados pelo CMake. As imagens
de `MAINUAN_ASSETS`, os ícones Lucide e os scripts são recursos Qt: não dependem
do diretório do desenvolvedor nem dos arquivos usados para capturar referências.

O módulo QML continua com API `1.0`; sua versão é independente da aplicação.
A versão pública vem de `project(... VERSION ...)` e chega à página Sobre por
`Qt.application.version`. CMake, changelog Debian e AppStream devem ser atualizados
juntos, sem números locais escritos manualmente no QML.

`rootfs/usr/share/welcome/index.html` e `main.js` são vestígios do protótipo,
preservados por precaução e sem referências no CMake, no binário ou nos testes
atuais. Não são instalados nem executados. Nenhum componente Electron faz parte
do produto nativo. Os recursos gráficos e demais arquivos de `rootfs` foram mantidos.

## Instalação e autenticação

`InstallService`, `PackageService`, `ApplicationModel`, `AppCard` e `InstallDialog`
compartilham o fluxo. O catálogo distingue Flatpak, Debian gerenciado e serviços
web. Google Docs e Office 365 continuam como URLs; a presença de URL não define
uma instalação. Aplicativos Flatpak usam a instalação existente do Flathub,
detectam o escopo e podem ser abertos/removidos pelo mecanismo já implementado.

Os perfis privilegiados são fechados: antivírus, firewall, codecs e FreeOffice.
O helper instalado em `/usr/libexec/mainuan-welcome/mainuan-welcome-helper` recebe
o identificador do perfil, nunca comandos, URLs ou caminhos arbitrários. A política
Polkit autoriza esse executável específico. O Welcome não coleta senhas.
APT aguarda locks, resolve dependências e usa `--no-remove`, sem apagar locks
ou remover pacotes para resolver conflitos. Os detalhes ficam no modal sob
solicitação e em `/var/log/mainuan-welcome/install.log` no lado privilegiado.

FreeOffice é o pacote `softmaker-freeoffice-2024`, não SoftMaker Office comercial.
Seu download HTTPS, versão, arquitetura, tamanho e SHA-256 estão fixados no perfil
incorporado e instalado. A validação ocorre antes da autenticação e novamente
após a transferência por stdin para armazenamento protegido pertencente ao root.
O helper instala o mesmo arquivo validado; não recebe arquivos por caminhos do
usuário. Atualizar o artefato autorizado exige revisão explícita do perfil e da
procedência, sem substituição silenciosa de produto/versão. Veja o documento [20](20-internal-application-installation.md).

O modal mostra produto, origem centralizada no perfil, uma barra real ou
indeterminada e o aviso de segurança. Um 100% intermediário de download não é
tratado como conclusão de toda a instalação. A confirmação de origem do FreeOffice
permanece antes do início, com origem SoftMaker e um aviso breve de autorização
administrativa. URL, hash, metadados e mensagens internas permanecem nos detalhes
de diagnóstico; após falhas, o usuário pode solicitar “Ver detalhes”.
Fechar normalmente o modal ou a janela não interrompe
APT, dpkg ou Flatpak. As origens são informativas, sem alterar os instaladores.

## Aparência, Kora e avatar

`AccentProfile` centraliza ID, esquema claro/escuro, cor do seletor, Kora,
wallpaper e origem `.face`. Os IDs são `brown`, `cyan`, `magenta`, `lime`, `gray`
e `violet`; os nomes são Marrom, Ciano, Magenta, Lima, Cinza e Violeta.
Os wallpapers são `05marrom.png`, `01ciano.png`, `03magenta.png`, `06lima.png`,
`02cinza.png` e `04violet.png`, respectivamente.

A seleção usa os doze esquemas Dream existentes no sistema, sem `--accent-color`
como substituto. O hexadecimal serve ao seletor visual. A detecção lê o estado
KDE, incluindo `kdeglobals`, por KConfig e sua cascata de defaults. A cascata
inclui explicitamente `~/.config/kdedefaults` mesmo se o lançador não exportar
esse diretório em `XDG_CONFIG_DIRS`: preferências locais têm precedência sobre
os defaults do tema, que têm precedência sobre os defaults do sistema. A leitura
não escreve configurações. O ID vem do esquema oficial, nunca do hexadecimal.
Os watchers KConfig mantêm a interface sincronizada com mudanças externas. Ao trocar
Dream/Tahoe/Breeze ou claro/escuro, o serviço aguarda o tema global e reaplica
os recursos da cor conhecida. Uma simples inicialização não substitui temas
personalizados. Claro/Escuro preserva a família reconhecida desde a inicialização;
recusa esquemas desconhecidos, não regrava o avatar e só restaura o wallpaper
quando o tema global o substitui. Falhas parciais são informadas individualmente.

`UserAvatar` grava `~/.face` do usuário atual sem root, valida a imagem, recusa
links inesperados e preserva um backup privado antes da primeira substituição.
As escritas usam descritores de diretório, sincronização e publicação atômica.
Um `.face.icon` independente não é sobrescrito; o link existente para `.face`
continua funcionando. Não há modificação privilegiada em AccountsService.
A atualização imediata de SDDM/KDE depende do tema, formato e cache; consulte [15](15-color-schemes-kora-avatar.md).

## Layouts e recuperação

Os sete IDs selecionáveis são `plasma-default`, `three-panels`, `single-bar`,
`tiling`, `panel-top`, `unity` e `minimal`, nessa ordem. O legado `floating` pode
ser reconhecido sem migração nem exclusão de painéis ao abrir o programa.
Todos os novos painéis usam espessura de 40 pixels lógicos; `height` representa
a espessura, inclusive em um painel vertical. `fit`, `fill` e `custom` mantêm
seus significados nativos. As referências pessoais já foram convertidas em dados
saneados: não são necessárias na compilação nem distribuídas no `.deb`.

O serviço verifica os widgets obrigatórios antes de alterar painéis. Lomiri exige
`Orbit.Start.Menu`; Painel superior exige `TahoeLauncher`. O Clássico verifica os
widgets de seus cinco segmentos, incluindo ChatAI, bandeja e KdeControlStation
na quarta barra. Não instala plasmoids automaticamente nem troca silenciosamente
esses lançadores por Kickoff. Widgets opcionais ausentes não inviabilizam os
demais painéis quando a estrutura obrigatória permanece válida.

O backup guarda os arquivos Plasma em
`$XDG_DATA_HOME/mainuan-welcome/layout-backups/`, mantendo a primeira cópia e as
mais recentes, até cinco. A aplicação usa scripting, sem edição direta dos arquivos
ativos. Quantidade, identidade e medidas dos painéis são verificadas antes do
sucesso. Falhas acionam o rollback; restaurar o backup pode reiniciar o Plasma,
mas aplicar normalmente um layout não exige reinício. Veja [17](17-layout-functional-fixes.md)
e o ajuste final de geometria em [19](19-classic-manual-spacing.md).

## Outras integrações e testes

O card Drivers adicionais procura somente o executável público
`mainuan-gestao-drivers` no PATH e o abre por QProcess, sem argumentos, shell ou
elevação. O lançador do projeto é `org.mainuan.GestaoDrivers.desktop`; sua presença
isolada não comprova que o executável está disponível. O Welcome não consulta
versões, tags ou downloads da ferramenta nem implementa operações de drivers.
PackageKit/PolicyKit e as alterações de drivers pertencem à própria ferramenta.
O pacote `mainuan-gestao-drivers` é instalado separadamente: não há Depends ou
Recommends novo, pois sua disponibilidade nas fontes APT não está garantida.
A ISO deve tratar os dois pacotes como componentes independentes. Sem a
ferramenta, o botão fica desabilitado e o card informa sua ausência.

O relatório da máquina evita identificadores sensíveis e coleta dados somente
quando solicitado. A página Pix lê `/etc/mainuan/welcome.conf`, com fallback na
configuração pessoal; o QR Code é gerado localmente por libqrencode. A preferência
de abrir no login é por usuário. Não há telemetria ou dependência de CDNs.

QtTest, QML e startup offscreen usam fixtures e diretórios temporários. Nenhum
teste deve chamar instaladores reais, modificar avatares/configurações pessoais
ou alcançar uma sessão Plasma. A compilação oficial mantém
`MAINUAN_DEVELOPER_MODE=OFF`; somente builds explicitamente de desenvolvimento
aceitam os overrides `MAINUAN_DEV_*` descritos nos fontes e nas fixtures.

Os temas, esquemas, plasmoids e recursos Mainuan são externos. Ausências e
limitações gráficas devem ser avaliadas na distribuição; os testes simulados não
provam a renderização final em todas as escalas, monitores e temas Wayland.
