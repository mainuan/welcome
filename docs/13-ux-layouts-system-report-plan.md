# UX, layouts e relatório do sistema — auditoria e plano

Escrito em 02/10/2026 sobre a `main` em `28cf04a`, antes da implementação. Ambiente de referência: VM Mainuan 2026 (base Ubuntu 26.04, Plasma 6.6.6, Wayland, 1 monitor, 4 áreas de trabalho virtuais).

## Estado encontrado

| Área | Situação |
|---|---|
| Cor de destaque | aplica `plasma-apply-colorscheme --accent-color` e grava `AccentColor`; não troca o papel de parede |
| Papel de parede | `VisualStyleProfile` define um papel de parede por estilo (Dream/Tahoe → `01ciano.png`, Breeze → `02cinza.png`) |
| Sidebar | ícones do tema (`go-home`, `preferences-desktop-theme`, …), coloridos e de estilos diferentes, 20 px; o tema de ícones muda com o estilo (kora, Breeze), então a navegação muda de aparência |
| Largura das páginas | cada página define a própria largura: Início 880, Aparência 1080, Sobre 820, Office/Navegadores/Tutoriais sem limite |
| Sobre | texto e dois links; nenhuma informação do sistema |
| Contribuir | lista de botões; Pix só aparece com chave em `/etc/mainuan/welcome.conf`, que o pacote não instala |
| Layouts | `LayoutModel` declara só “Plasma padrão” disponível; os outros cinco aparecem como “Incompatível/Legado”, e “Aplicar” do padrão não faz nada |

## Sistema real

- Papéis de parede em `/usr/share/wallpapers/` (plural; `/usr/share/wallpaper/` não existe): `01ciano.png`, `02cinza.png`, `03magenta.png` (não “magenda”), `04purpura.png`, `05marrom.png`, `06lima.png`. `plasma-apply-wallpaperimage` percorre `desktops()`, isto é, todos os monitores.
- Layout de fábrica do Mainuan: cinco “ilhas” flutuantes na base (menu; clima + áreas de trabalho; tarefas; bandeja com KdeControlStation, KDE AI Chat e ChatAI; relógio), altura 40 px, comprimento “fit”/personalizado.
- `org.kde.PlasmaShell` oferece `evaluateScript`, `dumpCurrentLayoutJS` e `setWallpaper`. KWin oferece `org.kde.kwin.Scripting.loadScript` e, no script, `workspace.rootTile(tela, área)` com `split()`/`remove()`.
- `dumpCurrentLayoutJS` + `loadSerializedLayout` **não** restauram fielmente no Plasma 6.6: alturas mudam (40 → 56), comprimento vira “fill” e a ordem dos widgets muda (testado na VM e revertido).
- `Chaac.Complete.Weather` não aparece em `knownWidgetTypes` (metadata sem `KPackageStructure`), mas o Plasma o carrega.
- Instalação: `/var/log/installer/` (legível: `media-info`, `initial-status.gz`) é criado pelo instalador.
- Disponíveis: `lspci`, `lsblk`, `glxinfo`, `libqrencode4` (dev no arquivo: `libqrencode-dev`).

## Referência `biglinux-session-and-themes`

`big-theme-plasma --apply <layout>` para o `plasmashell`, faz cópia por usuário do layout atual em `~/.kdebiglinux/<layout anterior>/`, copia `plasma-org.kde.plasma.desktop-appletsrc`/`plasmashellrc` prontos de `/usr/share/biglinux/kdebiglinux/<layout>/` (ou os salvos do usuário), aplica ajustes (`kwin_left`, bordas em janelas maximizadas, preferências de navegadores) e reinicia o `plasmashell`. O layout “kunity” usa painel superior (`location=3`) + painel vertical esquerdo (`location=5`) com `icontasks`, `windowbuttons` e `panel.colorizer` (de terceiros).

Aproveitado como conceito: layout completo por perfil, cópia de segurança por usuário antes de trocar, restauração. Não reaproveitado: arquivos `appletsrc` com ids de contêiner fixos e widgets do BigLinux, parada do Plasma para aplicar, edição de preferências de Firefox/Chromium.

## Plano

1. **Cor → papel de parede.** Tabela única `AccentProfile` (cor, nome, arquivo). `setAccent` aplica a cor, grava `AccentColor` e aplica o papel de parede com `plasma-apply-wallpaperimage` (todos os monitores). O estilo visual deixa de definir papel de parede; se a troca de estilo alterar o papel de parede, o da cor é reaplicado.
2. **Sidebar.** Sete ícones SVG de linha (Lucide, licença ISC; `currentColor`, traço uniforme, 24×24) desenhados com `Kirigami.Icon { isMask: true }`, iguais em qualquer tema de ícones; 26 px expandida, 28 px recolhida.
3. **Largura.** Componente `PageContainer` (rolagem + coluna centralizada, máximo 820 px, margem 24 px) usado pelas sete páginas.
4. **Sobre + relatório.** `SystemReportService` (C++) coleta de `/etc/os-release`, `/proc`, `/sys`, Qt, `QStorageInfo` e, com tempo-limite, `lspci`, `lsblk`, `glxinfo`, `flatpak`; seções tipadas para QML, texto para copiar/salvar. Data da instalação pelo registro do instalador; sem ele, data de criação do sistema de arquivos raiz marcada como estimada. MAC mascarado, sem IPs, sem números de série.
5. **Contribuir.** QR Code gerado localmente com libqrencode; `PixPayload` opcional (BR Code) e, sem ele, QR da chave rotulado como tal. O pacote instala `/etc/mainuan/welcome.conf` com a chave Pix do projeto.
6. **Layouts.** Script do Plasma (`src/layouts/desktop-layouts.js`, recurso Qt) cria os painéis de cada layout em todos os monitores com propriedades explícitas e marca cada painel com `MainuanLayout=<id>` para a detecção. Antes de aplicar, cópia de `plasma-org.kde.plasma.desktop-appletsrc` e `plasmashellrc` em `$XDG_DATA_HOME/mainuan-welcome/layout-backups/`; se o script falhar ou não sobrar painel com menu de aplicativos, restauração automática (para o `plasmashell`, copia, inicia). Botão “Restaurar layout anterior”. Tiling: barra superior compacta e blocos nativos do KWin (um grande e dois empilhados) em todas as telas e áreas de trabalho; sem Bismuth.
7. Testes automatizados, validação na VM, documentação, PR e teste da `main`.
