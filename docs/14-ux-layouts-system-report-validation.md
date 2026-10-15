# UX, layouts e relatório do sistema — validação

Executada em 02/10/2026 no host de desenvolvimento (BigLinux, Plasma 6.7, Qt 6.11.2) e na VM de testes Mainuan 2026 LTS (base Ubuntu 26.04, Plasma 6.6.6, KDE Frameworks 6.24, Qt 6.10.2, Wayland, 1 monitor, 4 áreas de trabalho). O plano e a auditoria estão em `13-ux-layouts-system-report-plan.md`.

## Alterações realizadas

| Área | Resultado |
|---|---|
| Cor → papel de parede | `AccentProfile` (tabela única) liga cada cor de destaque a um papel de parede do Mainuan; `setAccent` aplica a cor, grava `AccentColor` e aplica o papel de parede com `plasma-apply-wallpaperimage` (todas as telas). |
| Estilo visual | Os perfis Dream/Tahoe/Breeze não definem mais papel de parede. Se o tema global trocar o papel de parede, o Welcome reaplica o da cor de destaque (ou o anterior, sem cor registrada). |
| Sidebar | Sete ícones de linha (Lucide, ISC, `qml/icons/`) desenhados como máscara na cor do texto/destaque: 26 px expandida, 28 px recolhida; aparência idêntica em qualquer tema de ícones. |
| Largura | `PageContainer.qml` (rolagem + coluna centralizada, máximo 820 px, margem 24 px) em todas as páginas. |
| Sobre | Logo, versão, chips (Plasma, kernel, arquitetura), “Este computador” (processador, gráficos, memória, armazenamento, instalação, tempo ligado), créditos e botão **Relatório da máquina**. |
| Relatório da máquina | `SystemReportService` (C++) + `ReportDialog.qml`: resumo no estilo fastfetch e nove seções; **Copiar relatório**, **Salvar relatório** (diálogo nativo do KDE), **Atualizar**. |
| Contribuir | Cartão Pix com QR Code gerado localmente (libqrencode), chave copiável com confirmação, três cartões de ação (código, problema, comunidade) e nota de que nada é enviado. |
| Layouts | Seis layouts nativos do Plasma 6 com prévia, “✓ Em uso”, cópia de segurança antes de cada troca, reversão automática e **Restaurar layout anterior**. |
| Testes | 9 testes novos/atualizados em `tst_mainuan` e o teste de fumaça `smoke_startup` (abre a interface real e falha em qualquer erro de QML). |

## Papéis de parede por cor

| Cor | Hex | Arquivo em `/usr/share/wallpapers/` |
|---|---|---|
| Laranja | `#d08040` | `05marrom.png` |
| Rosa | `#e8177d` | `03magenta.png` |
| Azul | `#3daee9` | `01ciano.png` |
| Verde | `#3dd425` | `06lima.png` |
| Cinza | `#aab6b9` | `02cinza.png` |
| Lilás | `#a588cb` | `04purpura.png` |

Os nomes reais são `/usr/share/wallpapers` (plural) e `03magenta.png`. Arquivo ausente: a cor é aplicada e a mensagem diz qual papel de parede faltou; falha da ferramenta: a cor é aplicada e a mensagem informa o erro.

## Layouts

Script do Plasma (`src/layouts/desktop-layouts.js`) executado por `org.kde.PlasmaShell.evaluateScript`. O script remove os painéis, cria os do layout em cada tela (`showOnlyCurrentScreen` nas telas extras) e marca cada painel com `MainuanLayout=<id>`. Widgets que não estão instalados são omitidos; menu: `kickoff`, senão `kicker`.

| Layout | Painéis |
|---|---|
| Plasma padrão | Layout de fábrica do Mainuan: cinco ilhas flutuantes de 40 px na base (menu; clima + áreas de trabalho; tarefas com os lançadores do Mainuan; bandeja com KdeControlStation, KDE AI Chat e ChatAI; relógio + mostrar área de trabalho). |
| Painel superior | Barra de 36 px no topo: menu, áreas de trabalho, tarefas, bandeja, relógio. |
| Flutuante | Dock central flutuante de 52 px na base. |
| Minimalista | Barra fina de 32 px na base que desvia das janelas. |
| Unity-like | Barra superior de 28 px (menu global, bandeja, relógio) + lançador vertical de 56 px à esquerda. |
| Tiling | Barra superior compacta de 30 px + blocos nativos do KWin (`workspace.rootTile`): um bloco grande e dois empilhados em cada tela e área de trabalho. Sem Bismuth/Polonium. |

- **Cópia de segurança:** `plasma-org.kde.plasma.desktop-appletsrc` e `plasmashellrc` copiados para `$XDG_DATA_HOME/mainuan-welcome/layout-backups/<sequência>-<hora UTC>/` antes de cada troca (a sequência mantém a ordem mesmo se o relógio voltar). Ficam 5 cópias: a primeira (os painéis do usuário antes de qualquer troca, nunca apagada) e as mais recentes. A restauração troca cada arquivo de forma atômica (`QSaveFile`) e recusa uma cópia sem o arquivo de applets.
- **Verificação e reversão:** depois do script, `describeMainuanLayout()` precisa devolver a marca do layout, ao menos um painel e um menu de aplicativos; senão (ou com erro/tempo-limite de 20 s) o Welcome restaura a cópia.
- **Restauração:** `systemctl --user stop plasma-plasmashell.service`, copia, `start`. `dumpCurrentLayoutJS`/`loadSerializedLayout` foram testados e descartados: no Plasma 6.6 alteram alturas, comprimento e ordem dos widgets.
- **Detecção:** a marca `MainuanLayout` dos painéis; sem marca (painéis feitos à mão), nenhum card aparece como “Em uso”.
- **Referência BigLinux** (`biglinux-session-and-themes`): aproveitado o conceito de layout completo por perfil e a cópia por usuário antes de trocar; não aproveitados os `appletsrc` prontos com ids fixos, os widgets do BigLinux (`windowbuttons`, `panel.colorizer`), Latte e a edição de preferências de navegadores.

## Relatório da máquina

| Seção | Fontes |
|---|---|
| Sistema | `/etc/os-release`, `/var/log/installer/media-info`, `QSysInfo`, `QLocale`, data e hora |
| Área de trabalho | versão do Plasma via D-Bus (`org.kde.plasmashell /MainApplication applicationVersion`), `dpkg-query` (`libkf6coreaddons6`), `qVersion()`, `XDG_SESSION_TYPE`, `XDG_CURRENT_DESKTOP` |
| Hardware | `/sys/class/dmi/id/{sys_vendor,product_name}`, `/proc/cpuinfo`, `/sys/firmware/efi` (UEFI e Secure Boot via efivars) |
| Gráficos | `/sys/class/drm/card*/device` + driver, `lspci -mm` para os nomes, `glxinfo -B` (renderizador OpenGL), `QScreen` (resolução, taxa, escala) |
| Memória | `/proc/meminfo` (RAM e swap) |
| Armazenamento | `QStorageInfo` (volumes montados, sem loop/snap), `lsblk -J -b` (discos, modelo, SSD/HDD, barramento) |
| Pacotes | `/var/lib/dpkg/status` (contagem de `install ok installed`), `/var/lib/flatpak/app` + Flatpak do usuário, `/snap` |
| Inicialização | `/boot/vmlinuz-*` (kernels instalados), kernel em uso, `/proc/uptime` |
| Rede | `/sys/class/net` (só interfaces físicas): tipo, estado, velocidade, driver, MAC mascarado (`aa:bb:cc:xx:xx:xx`) |

- Ferramentas externas rodam de forma assíncrona com `LC_ALL=C` e tempo-limite de 6 s; sem a ferramenta, o item correspondente é omitido.
- **Privacidade:** sem IPs, SSID, números de série, UUIDs, nome de usuário ou tokens. O nome do computador aparece na janela, mas fica fora do texto copiado/salvo (o padrão do instalador inclui o nome do usuário). O texto termina com “Sem endereços de rede, números de série nem nome do computador.”
- **Data da instalação:** data de modificação de `/var/log/installer/media-info` (escrito pelo instalador), senão a da pasta `/var/log/installer`; sem registro do instalador, data de criação (`birthTime`) do sistema de arquivos raiz, rotulada “Data estimada da instalação”.

## Contribuir

- Chave em `/etc/mainuan/welcome.conf` (`[Contribute] PixKey`, instalado como conffile) e, quando ele não tem chave, `~/.config/mainuan/welcome.conf`. `PixPayload` opcional (BR Code “copia e cola”) tem precedência no QR; sem ele, o QR contém a chave e o texto explica como usá-la no app do banco.
- QR gerado com libqrencode (nível M) e entregue ao QML como `data:` PNG: nenhuma API remota.
- Links: repositório, abertura de issue e comunidade no Telegram, abertos com `QDesktopServices`.

## Problemas encontrados e corrigidos

| Problema | Correção |
|---|---|
| Restauração por `loadSerializedLayout` deformava os painéis (40 → 56 px, comprimento, ordem) | cópia de arquivos + reinício do `plasmashell` |
| Gerenciador de tarefas com “¿?” quando os lançadores iam como texto separado por vírgulas | lista JavaScript |
| `Chaac.Complete.Weather` ausente de `knownWidgetTypes` | C++ injeta a lista de plasmoides instalados |
| `layoutService: layoutService` ligava a propriedade a si mesma | propriedade renomeada `layoutManager` |
| `spacing` (ScrollView) e `icon` (AbstractButton) são propriedades FINAL: a janela não abria na VM, e o qmllint não acusava | `contentSpacing` / `iconName`; criado o teste `smoke_startup`, que falha com essa regressão |
| `FileDialog` do Qt 6.10: “Cannot set as a selected file” | nome sugerido definido ao abrir o diálogo |
| Diálogo de salvar não nativo e sem nome sugerido | `QApplication` (Qt6::Widgets) habilita o diálogo do KDE |
| Nome do computador contém o nome do usuário | fora do relatório copiado/salvo |
| Cor de destaque ficava “uma troca atrasada” nos controles do Qt e no Plasma (o `plasma-apply-colorscheme` anuncia a paleta antes de `AccentColor` ser gravado, e a `kdeglobals` em cache do Welcome não era relida) | após gravar, `reparseConfiguration()` e `org.kde.KGlobalSettings.notifyChange(PaletteChanged)` |
| Prévias de layout no tema claro: barras brancas sobre fundo claro, quase invisíveis | barras em cinza-ardósia no tema claro |
| Cards de layout da mesma linha com alturas diferentes | cards preenchem a linha; botão alinhado à base |
| `waitForFinished` no tiling bloqueava a interface | chamadas encadeadas assíncronas |
| Revisão: ponto de montagem de mídia removível (`/media/<usuário>/<rótulo>`) ia para o relatório | só locais do sistema; os demais viram “Outro volume N” |
| Revisão: interfaces nomeadas pelo MAC (`enx001122334455`) expunham o MAC inteiro | parte do dispositivo mascarada (`enx001122xxxxxx`) |
| Revisão: com o relógio voltando, a poda podia apagar a cópia recém-criada, e a reversão “restaurava” uma pasta vazia | nomes com sequência, poda que preserva a nova cópia e a original, restauração que falha sem o arquivo de applets |
| Revisão: restauração apagava o arquivo antes de copiar (disco cheio = Plasma sem painéis) | `QSaveFile` (troca atômica) |
| Revisão: `systemctl` sem tratamento de falha ao iniciar nem tempo-limite podia deixar o Welcome “ocupado” | auxiliar único com `errorOccurred` e tempo-limite de 60 s |
| Revisão: disponibilidade do Plasma lida só na abertura | relida sempre que a Aparência aparece |
| Revisão: o teste de fumaça usava o barramento D-Bus da sessão real | roda sem barramento nem display (um barramento privado com `dbus-run-session` deixava portais ativados rodando após o build) |
| Após o merge: `plasmashell --version` (versão do Plasma) iniciava um segundo `plasmashell`, que quebrava (SIGSEGV + core dump) quando herdava a plataforma offscreen dos testes em cada `ctest`/build do pacote | versão lida do D-Bus (`/MainApplication applicationVersion`), sem processo |
| Após o merge: `cmake --install build --prefix stage` tentava gravar em `/usr/local/etc` | destino de `/etc` relativo ao prefixo (absoluto só com prefixo `/usr` ou `CMAKE_INSTALL_SYSCONFDIR` absoluto) |

## Testes executados

| Teste | Método | Resultado |
|---|---|---|
| Build Release limpo | `cmake -S . -B <novo> -DCMAKE_BUILD_TYPE=Release && cmake --build` | PASS, 0 warnings (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow`) |
| Testes unitários | `ctest` (`tst_mainuan`: 36 casos) | PASS no host e no build do pacote na VM |
| Teste de fumaça | `ctest` (`smoke_startup`: abre a interface offscreen por 4 s) | PASS; FAIL confirmado com a regressão `icon`/`spacing` |
| Pacote | `dpkg-buildpackage -us -uc -b` + `apt-get install` na VM | PASS (os testes rodam no build) |
| Seis cores | cada cor pela interface; `org.kde.PlasmaShell.wallpaper(0)` e captura de tela | PASS: os seis arquivos da tabela |
| Cor mantida ao trocar estilo | Azul → Dream → Tahoe → Breeze; Rosa → Dream | PASS: `01ciano`/`03magenta` mantidos |
| Cor ao vivo | Laranja → Azul pela interface, sem reiniciar nada | PASS: Welcome, caixas de seleção do Qt e paginador do Plasma acompanham |
| Seis layouts | cada um pela interface; marca, painéis e menu via script de descrição | PASS; cópias limitadas a 5 |
| Tiling | blocos do KWin em todas as 4 áreas de trabalho | PASS: 50% + 25%/25% |
| Restaurar layout anterior | após o Tiling | PASS: layout anterior de volta, uma instância do `plasmashell` |
| Persistência | fechar e reabrir o Welcome | PASS: estilo, tema, cor, layout e papel de parede detectados |
| Sobre / relatório | dados conferidos com `dpkg-query`, `flatpak list`, `lsblk`, `/proc` | PASS (2492 pacotes Debian, 4 Flatpak) |
| Copiar / salvar relatório | área de transferência e diálogo nativo em `~/Documentos` | PASS: 58 linhas, sem IP/SSID/nome do computador |
| Atualizar relatório 5× | contagem de descritores do processo | PASS: 27 antes e depois |
| Pix | área de transferência; `zbarimg` sobre a captura do QR | PASS: decodifica a chave configurada |
| Resoluções | 1360×768 (sidebar expandida e recolhida), 1920×1080, temas claro e escuro | PASS |
| Escala | 125% e 150% (`kscreen-doctor`): Aparência, Sobre, relatório, Contribuir | PASS: a 150% a sidebar recolhe sozinha e o corpo do relatório rola |
| Regressão | instalação e remoção do Google Chrome pelo modal; linhas de antivírus/firewall/codecs; instância única; autostart | PASS |
| Journal | `journalctl --user` durante os testes | sem erros do Welcome; avisos só de plasmoides de terceiros |

Capturas em `docs/screenshots/` (sem dados pessoais; o nome do computador está desfocado na captura do relatório).

## Limitações conhecidas

- O KWin não tem tiling automático: o layout Tiling cria os blocos, e as janelas são encaixadas arrastando com Shift.
- Widgets de terceiros do layout padrão (clima, KdeControlStation, KDE AI Chat, ChatAI) são recriados com as configurações padrão deles.
- Restaurar um layout reinicia o `plasmashell` (alguns segundos sem painéis). “Restaurar layout anterior” volta sempre para a cópia mais recente; a cópia original fica guardada na pasta de cópias, mas ainda não há botão para ela.
- Multimonitor coberto pelo script (`screens()`, `showOnlyCurrentScreen`, `plasma-apply-wallpaperimage` em todos os desktops), mas não testado com monitores físicos reais.
- Os esquemas de cores Dream trazem comentários na mesma linha dos valores (`0,167,181 ; #00A7B5`), e o KConfig avisa ao lê-los; o Welcome lê as cores manualmente e não é afetado.
- No modal do instalador, a barra chega a 100% quando o download termina, e a instalação ainda leva alguns segundos (comportamento anterior a esta mudança).
