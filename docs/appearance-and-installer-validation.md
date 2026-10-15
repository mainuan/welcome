# Aparência e instalador universal — validação

Executada em 02/10/2026 no host de desenvolvimento (BigLinux/Manjaro, Plasma 6.7.4, Wayland, Qt 6.11.2) e em contêineres Podman Ubuntu 24.04, Ubuntu 26.04 e Debian 13. O plano está em `appearance-and-installer-plan.md`.

## Alterações realizadas

| Área | Resultado |
|---|---|
| Navegação | Início, **Aparência**, Office, Navegadores, Tutoriais, Sobre, Contribuir. “Layouts” saiu da navegação. |
| Aparência | `AppearancePage.qml`: “Aparência do sistema” (Estilo visual, Tema, Cor de destaque) em um único cartão e “Layout do desktop” com os cards do `LayoutModel` em grade responsiva (1–4 colunas). `LayoutsPage.qml` removida. |
| Início | “Hardware & Mídia” (Drivers adicionais → Codecs de mídia) e depois “Segurança & Privacidade” (Antivírus, Firewall). Nenhum controle de aparência permanece no Início. |
| Antivírus/Firewall/Codecs | Estado detectado (“✓ Instalado”, “Falta: …”, “Não instalado”, “Indisponível neste sistema”), botão **Instalar** enquanto algo falta e **Configurar** quando o configurador existe. |
| Instalador | `InstallService` (C++), `InstallProgressParser`, `InstallDialog.qml`, helper `mainuan-welcome-helper` + ação polkit `org.mainuan.welcome.install`. Office e Navegadores também instalam pelo mesmo modal. |
| Estilo visual | Aplica os temas globais Dream, Tahoe e Breeze (ver “Estilo visual no Mainuan” abaixo). |
| Componentes QML | `SettingRow`, `SegmentedChoice`, `MessageBanner`; `StatusRow` reescrito com selo de estado e duas ações. |

## Problemas encontrados e corrigidos

| Problema | Origem | Correção |
|---|---|---|
| `flatpak install --user flathub` falha quando o Flathub é remote só de sistema (caso do host) | pré-existente | escopo escolhido conforme `flatpak remotes`; sem Flathub, adiciona remote `--user` |
| `kdeglobals` nunca era lido: `QSettings` trata `[General]` como chaves de nível superior, e `beginGroup("General")` não encontrava nada | pré-existente | leitura sem grupo; tema escuro pela cor real `[Colors:Window] BackgroundNormal`; `AccentColor` “r,g,b” lido como lista |
| `layoutModel: layoutModel` ligava a propriedade a si mesma (grade de layouts vazia) | pré-existente | propriedade renomeada para `layouts` |
| Estado “Removendo…” nunca aparecia (`operationStarted` não conectado) | pré-existente | sinal conectado |
| Firewall sempre “Não foi possível verificar” (`ufw status` exige root) | pré-existente | leitura de `ENABLED` em `/etc/ufw/ufw.conf` |
| `kcmshell6` mantinha `busy` enquanto o módulo ficava aberto | pré-existente | configuradores abertos com `startDetached` |
| Sem rede, `apt-get update` sai com 0 e o helper relatava “pacotes indisponíveis” | encontrado no teste | falta de candidato é classificada pelo log (→ `network`) |
| Leitor do pipe morto quebrava o apt (`Write error 32: Broken pipe`) | encontrado no teste | `trap '' PIPE`, laço de retransmissão tolerante e saída do dpkg em log root |
| Plasma 6.6.0–6.7.4: `panel.opacity` por script é ignorado em painéis com tela (o setter chama `QWindow::setOpacity`, corrigido só no ramo 6.7) | upstream | motivo adicional para abandonar a abordagem por opacidade: os estilos agora aplicam temas globais |
| Texto “Esta janela fecha assim que…” prometia fechamento automático | encontrado na revisão visual | texto corrigido |

## Testes executados

| Teste | Comando/método | Resultado |
|---|---|---|
| Build Debug com modo desenvolvedor | `cmake -S . -B build-dev -DMAINUAN_DEVELOPER_MODE=ON && cmake --build build-dev` | PASS, 0 warnings (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow`) |
| Build Release | `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr` | PASS, 0 warnings |
| Testes unitários | `ctest --test-dir build-dev` e `build-release` | PASS: 23 funções; 10 repetições consecutivas sem falha |
| Staging | `DESTDIR=stage-release cmake --install build-release` | binário, helper `0755` em `/usr/libexec/mainuan-welcome/`, policy com `exec.path` correto |
| Policy | `xmllint --noout` | PASS |
| Modo desenvolvedor ausente no Release | `strings` no binário Release | 0 ocorrências de `MAINUAN_DEV_` |
| Shell | `bash -n` + `shellcheck` no helper e nas fixtures | PASS |
| QML | `qmllint` em todos os arquivos | PASS, sem avisos |
| Smoke | `QT_QPA_PLATFORM=offscreen` e `xcb`, 4 s | PASS, log vazio |
| Desktop file | `desktop-file-validate` | PASS |
| JS legado | `node --check rootfs/usr/share/welcome/main.js` | PASS; `preload.js` e `welcome-cli.sh` já estavam apagados antes desta tarefa |

### Helper real em contêineres (APT real, sem tocar o host)

| Cenário | Ambiente | Resultado |
|---|---|---|
| Firewall do zero | Ubuntu 26.04 | `@@phase preparing → updating → installing → done`; 1.528 linhas `dlstatus` e 2.553 `pmstatus`; `ufw` e `plasma-firewall` instalados; `ENABLED=no` (firewall não ativado); log `0640 root` |
| Firewall já instalado | Ubuntu 26.04 | apenas `preparing` e `done` |
| Antivírus do zero | Ubuntu 26.04 | `clamav`, `clamav-daemon`, `clamav-freshclam`, `flatpak` instalados; `freshclam` baixou e testou `main.cvd`, `daily.cvd`, `bytecode.cvd` (103 s) |
| Sem rede (`--network none`) | Ubuntu 26.04 | `@@error network` |
| Lock do dpkg mantido por outro processo | Debian 13 | `@@phase waiting`, instalação após liberação; arquivo de lock preservado |
| Leitor morre após 5 linhas | Debian 13 | instalação concluída, `dpkg --audit` consistente, 0 “Broken pipe” no log |
| Argumentos inválidos (`install "firewall; rm -rf /"`, `remove ufw`, vazio) | Ubuntu 26.04 + teste unitário | `@@error invalid`, exit 2; sem root → `@@error permission` |

### Cenários do modal (backends simulados em `tests/fixtures/`, testes automatizados)

| Caso | Teste | Resultado |
|---|---|---|
| A — não instalado → progresso → sucesso | `installsMissingComponentsWithRealProgress` | PASS; fases na ordem; porcentagens exibidas são exatamente as emitidas pelo backend; etapas sem dado ficam em −1 (indeterminado) |
| B — já instalado | `reportsAlreadyInstalledProfiles` | PASS; sucesso sem nenhuma etapa |
| C — autenticação cancelada (`pkexec` 126) | `treatsDismissedAuthenticationAsCancelled` | PASS; “Instalação cancelada”; 127 → erro de autenticação |
| D — erro do APT | `reportsPackageManagerErrors` | PASS; mensagem amigável sem “dpkg”; detalhes técnicos disponíveis |
| E — sem internet / lock / Flatpak sem rede | `explainsNetworkAndLockFailures` | PASS |
| F — clique duplo | `rejectsConcurrentStarts` | PASS; segundo `start` recusado, um único job |
| G — reabrir o Welcome | `detectsStateAgainOnRestart` | PASS; estado vem do sistema |
| H — ClamAV presente, ClamUI ausente | `installsOnlyClamUiWhenClamAvExists` | PASS; só a etapa Flatpak, sem helper |
| I — ClamUI presente, ClamAV ausente | `installsOnlyClamAvWhenClamUiExists` | PASS; só a etapa APT |
| J — UFW presente, plasma-firewall ausente | `completesFirewallWithPlasmaIntegration` | PASS; instala apenas `plasma-firewall` |
| Autorização do Flatpak negada | `treatsDeniedFlatpakAuthorizationAsCancelled` | PASS |
| Sistema sem dpkg | `disablesAptProfilesOnNonDebianSystems` | PASS; instalação indisponível com motivo |
| Perfis desconhecidos | `rejectsUnknownProfiles` | PASS |

### Validação visual (build de desenvolvimento, backends simulados)

Executada em XWayland (com `xdotool`) e em Wayland nativo (captura com Spectacle).

- Início e Aparência na ordem pedida; Layout ausente da barra lateral; tamanho mínimo 900×620 sem sobreposição.
- Modal: indeterminado em “Atualizando a lista…”, “Baixando ClamAV (arquivo 3 de 6)… 41%”, “Instalando ClamAV… 33%”, “Etapa 2 de 2 · componente 1 de 2” no Flatpak, sucesso com **Configurar** e **Fechar**; codecs com apenas **Fechar**; LibreOffice com **Abrir** e **Fechar**.
- Erro com “Ver detalhes” (ativado por teclado e mouse); cancelamento com “Tentar novamente” e “Fechar”.
- Esc e pedido de fechamento da janela durante a instalação: a janela permanece aberta, o job continua e o aviso é destacado.
- Tema claro com cor de destaque rosa (configuração isolada via `XDG_CONFIG_HOME`) e tema escuro real.
- Estilo visual: a primeira implementação (blur/opacidade) foi testada no host e depois substituída; ver a seção seguinte.

## Não executado

- Fluxo polkit real (diálogo do agente, senha, `auth_admin_keep`): o host não é Debian e a ação polkit não está instalada nele; o mapeamento 126/127 segue a documentação do `pkexec` e foi testado com simulação.
- Instalação real do ClamUI pelo Flathub, abertura real do ClamUI e do `kcm_firewall` após instalação real.
- Troca real de tema claro/escuro e de cor de destaque no host (alteraria o desktop do desenvolvedor; `DreamGray*Color` não existe no host).
- Leitores de tela e escala 150–250%.

## Pendências reais

1. Validar em Kubuntu/Mainuan com Plasma 6.6 real: polkit, ClamUI pelo Flathub, `kcmshell6 kcm_firewall` e `kubuntu-driver-manager`.
2. Os esquemas `Dream*Color.colors` do Mainuan têm valores como `0,130,140 ; #00828C`, que o KConfig não interpreta: a cor de seleção do Dream cai no azul padrão. Corrigir no pacote de temas do Mainuan (remover os comentários `; #…`).
3. `appstreamcli validate` falha por `app-description-required` no metainfo criado na migração Qt (anterior a esta tarefa).
4. `ttf-mscorefonts-installer` (EULA) e `libavcodec-extra` (exige remover `libavcodec*`) não fazem parte do perfil de codecs.
5. Se o processo do Welcome for morto durante a instalação, o helper conclui em segundo plano; ao reabrir, o estado é detectado novamente, mas o progresso daquela execução não é recuperado.

## Estilo visual no Mainuan (Dream, Tahoe, Breeze)

Validado em 02/10/2026 na VM de testes Mainuan 2026 (base Ubuntu 26.04, Plasma 6.6.6, Wayland), com o pacote `.deb` construído e instalado na própria VM e a interface acionada por AT-SPI.

### Perfis encontrados no sistema

| | Dream | Tahoe | Breeze |
|---|---|---|---|
| LookAndFeelPackage | `Dream-Light-Color-Global-6` / `Dream-Dark-Color-Global-6` | `com.github.vinceliuice.MacTahoe-Light` / `-Dark` | `org.kde.breeze.desktop` / `org.kde.breezedark.desktop` |
| ColorScheme | `DreamCyanLightColor` / `DreamCyanDarkColor` | `MacTahoeLight` / `MacTahoeDark` | `BreezeLight` / `BreezeDark` |
| Ícones | `kora-cyan` | `MacTahoe-light`/`-dark` (ausentes → `breeze`/`breeze-dark`) | `breeze` / `breeze-dark` |
| Tema Plasma | `Dream-Color-Plasma` | `MacTahoe-Light` / `-Dark` | `default` |
| Decoração | Aurorae `Lavanda-Light` / `-Dark` | Aurorae `MacTahoe-Light` / `-Dark` | Breeze |
| Papel de parede | `01ciano.png` | `01ciano.png` | `02cinza.png` |

### Por que a versão anterior não funcionava

- Interpretava os estilos como intensidade de blur do KWin e opacidade dos painéis; isso não troca esquema de cores, tema Plasma, decoração nem ícones, e o Plasma 6.6 ignora `panel.opacity` por script em painéis com tela.
- Detectava o estilo pelo blur, então nunca reconhecia Dream, Tahoe ou Breeze.
- Lia `~/.config/kdeglobals` com `QSettings`, sem a cascata do KDE: temas globais gravam em `~/.config/kdedefaults/`, que ficava invisível.

### Comparação com `change-theme.sh`

| Passo do script | No Welcome |
|---|---|
| `kwriteconfig` de ColorScheme, Icons e LookAndFeelPackage | substituído por `plasma-apply-lookandfeel --apply <id>`, que grava esses valores (em `kdedefaults`) e notifica o Plasma |
| `plasma-apply-lookandfeel --apply "org.kde.$2.desktop"` | mantido com os ids reais; o prefixo `org.kde.` do script não existe para Dream/Tahoe e a chamada falhava em silêncio |
| papel de parede (appletsrc + `plasma-apply-wallpaperimage`) | `plasma-apply-wallpaperimage` com o arquivo do perfil |
| ícones como parâmetro | ícones do pacote; se não estiverem instalados, `plasma-changeicons` com o fallback Breeze (idem cursor com `plasma-apply-cursortheme`) |
| `kbuildsycoca6` | não necessário: nenhum serviço ou `.desktop` muda |
| `plasmashell --replace` | não necessário: o Plasma aplica tudo ao vivo (PID do `plasmashell` inalterado em todos os testes) |

### Resultados

| Teste | Resultado |
|---|---|
| Breeze claro → Desfocado | PASS: Dream aplicado em 1,9 s; painel ciano translúcido, decoração Lavanda, `01ciano.png` |
| Fechar e reabrir | PASS: Desfocado selecionado |
| Desfocado → Vítreo | PASS: Tahoe em 2,9 s; botões de janela estilo macOS, ícones Breeze no lugar dos MacTahoe ausentes |
| Fechar e reabrir | PASS: Vítreo selecionado |
| Vítreo → Sólido | PASS: Breeze em 2,1 s; painel sólido, decoração Breeze, `02cinza.png` |
| Fechar e reabrir | PASS: Sólido selecionado |
| Ciclo Dream → Tahoe → Breeze → Dream → Breeze → Tahoe | PASS: 6/6 confirmados no Plasma, sem reiniciar o `plasmashell` |
| Escuro com Tahoe; Desfocado e Sólido no escuro; Claro | PASS: variantes `-Dark`/`breezedark` e volta ao claro, mantendo o estilo |
| Cor de destaque + troca de estilo | PASS: cor gravada em `AccentColor` e mantida pelo Plasma ao trocar o estilo |
| Troca externa (`plasma-apply-lookandfeel` com o Welcome aberto) | PASS: seleção atualizada ao vivo via `KConfigWatcher` |
| Journal durante os testes | sem avisos do Welcome nem crash do Plasma/KWin |
| Regressão (Início, Office, Navegadores, Tutoriais, Sobre, Contribuir) | PASS: páginas abrem e controles respondem |

### Problemas encontrados e corrigidos nesta etapa

| Problema | Correção |
|---|---|
| Opções de estilo, tema e cor ignoravam a ação de acessibilidade (o AT-SPI altera `checked` sem `clicked`): leitores de tela não aplicavam nada | os controles reagem à mudança de `checked` e voltam a seguir o estado real |
| Opções não expunham qual estava selecionada aos leitores de tela | botões `checkable` ligados ao estado do sistema |
| Cor escolhida não aparecia marcada (`plasma-apply-colorscheme` só tinge a seleção) e se perdia ao trocar o estilo | `AccentColor` gravado como no Configurações do Sistema |
| Cores lidas via conversão `QColor` do KConfig, que depende do KConfigGui carregado por acaso | cores interpretadas explicitamente |
| Tahoe deixava o botão do menu de aplicativos sem ícone (tema MacTahoe ausente) | fallback para ícones e cursor Breeze |
