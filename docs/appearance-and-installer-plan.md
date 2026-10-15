# Aparência e instalador universal — plano

Planejamento escrito em 02/10/2026, antes da implementação, sobre a árvore de trabalho de `main` (HEAD `11d3547`).

## Qual aplicação é alterada

O pedido original descreve a versão Electron (`index.html`, `preload.js`, `main.js`, `welcome-cli.sh`, `package.json`). Na árvore de trabalho, essa versão já foi substituída pela aplicação nativa Qt 6/QML/C++ (`CMakeLists.txt`, `src/`, `qml/`): `preload.js`, `welcome-cli.sh` e `package.json` estão apagados, e `index.html`/`main.js` não fazem mais parte do runtime nem do build (ver `docs/12-relatorio-final.md`).

Decisão: implementar na aplicação Qt, que é a que roda. Ressuscitar o Electron desfaria a migração. Os requisitos do pedido para Electron (sem `exec` genérico, allowlist, `execFile` com arrays, IPC limitado) têm equivalentes diretos em Qt: `Q_INVOKABLE` específicos, `QProcess::start(programa, argumentos)` e perfis fechados em C++.

## Arquitetura atual (relevante)

| Área | Onde está | Observação |
|---|---|---|
| Navegação | `qml/Main.qml` (`navigationModel` + `StackLayout`) | Início, Office, Navegadores, Tutoriais, Layouts, Sobre, Contribuir |
| Tema e cor de destaque | `HomePage.qml` → `SystemService::setTheme/setAccent` | `plasma-apply-colorscheme` com argumentos separados |
| Estilo visual | inexistente no Qt | no Electron, os botões Desfocado/Vítreo/Sólido chamavam `look-and-feel`, ação que o `welcome-cli.sh` **nunca implementou** |
| Layouts | `LayoutsPage.qml` + `LayoutModel` + `SystemService::applyLayout` | apenas “Plasma padrão” disponível; os demais são marcados como incompatíveis |
| Antivírus/codecs | `SystemService::performAction` | abre o Discover; não instala nada |
| Firewall | `SystemService::performAction("firewall")` | `kcmshell6 kcm_firewall` via `runCommand`, que mantém `busy` enquanto o KCM está aberto |
| Office/Navegadores | `ApplicationModel` + `PackageService` | `flatpak install --user flathub <id>` em `QProcess`, sem progresso |

## Problemas encontrados

1. `PackageService` instala com `--user flathub`, mas em muitos sistemas (inclusive o host de desenvolvimento) o Flathub existe apenas como remote **system**: a instalação falha com “remote não encontrado”.
2. `runCommand` espera o `kcmshell6` terminar: com o módulo de firewall aberto, os botões de tema e cor ficam desabilitados.
3. Não há indicação de progresso nas instalações, nem tratamento de cancelamento/erros além do texto de saída.
4. Antivírus e codecs dependem do Discover, que não existe em todas as instalações.
5. “Estilo visual” existia só na interface do protótipo, sem backend; o `change-theme.sh` do Mainuan era a referência de comportamento.

## Decisões

### Navegação e páginas

- Nova página **Aparência** (índice 1, logo abaixo de Início), com duas seções: “Aparência do sistema” (Estilo visual, Tema, Cor de destaque) e “Layout do desktop” (os cards do `LayoutModel`, inalterados em comportamento).
- `LayoutsPage.qml` é absorvida por `AppearancePage.qml`; a entrada “Layouts” sai da navegação. `LayoutModel` e `applyLayout` permanecem.
- Início: seção “Hardware & Mídia” (Drivers adicionais → Codecs de mídia) e depois “Segurança & Privacidade” (Antivírus, Firewall).

### Estilo visual

Cada opção aplica um tema global (pacote Look-and-Feel) do Mainuan, na variante clara ou escura conforme o “Tema do sistema”:

| Opção | Perfil | Pacote claro / escuro | Papel de parede |
|---|---|---|---|
| Desfocado | Dream | `Dream-Light-Color-Global-6` / `Dream-Dark-Color-Global-6` | `01ciano.png` |
| Vítreo | Tahoe | `com.github.vinceliuice.MacTahoe-Light` / `-Dark` | `01ciano.png` |
| Sólido | Breeze | `org.kde.breeze.desktop` / `org.kde.breezedark.desktop` | `02cinza.png` |

A primeira versão desta tela tentou representar os estilos com intensidade do blur do KWin e opacidade dos painéis; no Mainuan isso não produzia os visuais Dream/Tahoe/Breeze e foi substituído (correção registrada em `appearance-and-installer-validation.md`).

### Instalador universal

```text
QML                          C++ (InstallService)                     root
openInstallDialog(id) ──▶ start(profileId) ─┬─ APT ─▶ pkexec helper install <perfil> ─▶ apt-get
                                            └─ Flatpak ─▶ flatpak install (escopo do Flathub)
        ◀── propriedades: phase, percent, message, step, error, details
```

- **Perfis fechados em C++**: `antivirus`, `firewall`, `codecs` e `app:<id Flatpak>` (somente IDs da allowlist de `PackageService`). Cada perfil tem componentes (APT ou Flatpak), textos, ícone e ação pós-instalação (Configurar/Abrir).
- **Helper privilegiado** `mainuan-welcome-helper` (bash, instalado em `libexec`), executado via `pkexec` com ação polkit própria. Aceita apenas `install antivirus|firewall|codecs`, decide os pacotes internamente, instala somente o que falta, usa `--no-remove`, nunca recebe nomes de pacote do frontend.
- **Progresso real**: APT via `-o APT::Status-Fd=3` (`dlstatus`/`pmstatus`); Flatpak via linhas de progresso do CLI em modo não-tty (`Installing 1/3… 45%`). Etapas sem porcentagem (autenticação, lista de pacotes, assinaturas do ClamAV) usam barra indeterminada.
- **Máquina de estados**: `idle → preparing → authenticating → (waiting | updating) → downloading → installing → configuring → success | error | cancelled`.
- **Detecção de estado** no startup, local e assíncrona: `dpkg-query -W` para pacotes Debian e a listagem já existente do Flatpak. Em sistemas sem dpkg, há detecção por arquivos (executáveis/KCM) e a instalação APT fica indisponível com explicação.
- **Robustez**: um único job por vez; o helper aguarda locks do dpkg (`fuser`, `DPkg::Lock::Timeout`) sem nunca removê-los; o fluxo de status é retransmitido por um laço que tolera a morte do leitor, e a saída do dpkg vai para log root em `/var/log/mainuan-welcome/`, de modo que fechar o Welcome não interrompe o dpkg no meio.
- **Fechamento**: durante o job, o modal não fecha (Esc mostra a explicação) e o fechamento da janela é adiado com aviso.

### Pacotes (validados em contêineres Ubuntu 24.04, Ubuntu 26.04 e Debian 13)

| Perfil | APT | Flatpak |
|---|---|---|
| Antivírus | `clamav`, `clamav-daemon`, `clamav-freshclam`, `flatpak` (pré-requisito do ClamUI) | `io.github.linx_systems.ClamUI` (Flathub) |
| Firewall | `ufw`, `plasma-firewall` | — |
| Codecs | `gstreamer1.0-libav`, `gstreamer1.0-plugins-ugly`, `gstreamer1.0-plugins-bad`, `ffmpeg` | — |

- ClamUI via Flathub: método recomendado pelo upstream, com atualização automática; o `.deb` upstream não tem repositório APT. O ClamUI usa o ClamAV do host via `flatpak-spawn --host`, por isso o ClamAV é instalado por APT.
- Após instalar o ClamAV, o helper atualiza as assinaturas (`freshclam`, com o serviço `clamav-freshclam` parado durante a execução) e inicia o `clamav-daemon`. Falha nesse passo vira aviso, não erro: os pacotes estão instalados e o serviço tenta de novo.
- Firewall: o helper não habilita o UFW nem cria regras. `plasma-firewall` instala `kcm_firewall` (`kcmshell6 kcm_firewall`).
- Codecs: `ttf-mscorefonts-installer` (EULA interativa) e `libavcodec-extra` (exige remover `libavcodec*`, bloqueado por `--no-remove`) ficam fora.

## Arquivos envolvidos

- Novos: `src/services/InstallService.{h,cpp}`, `src/services/InstallProgressParser.{h,cpp}`, `qml/pages/AppearancePage.qml`, `qml/components/InstallDialog.qml`, `qml/components/SettingRow.qml`, `rootfs/usr/libexec/mainuan-welcome/mainuan-welcome-helper`, `rootfs/usr/share/polkit-1/actions/org.mainuan.welcome.policy.in`.
- Alterados: `CMakeLists.txt`, `src/main.cpp`, `SystemService`, `PackageService`, `ApplicationModel`, `qml/Main.qml`, `HomePage.qml`, `StatusRow.qml`, `AppCard.qml`, `AppsPage.qml`, `tests/tst_mainuan.cpp`, `README.md`.
- Removido: `qml/pages/LayoutsPage.qml` (conteúdo movido).

## Riscos

| Risco | Mitigação |
|---|---|
| Host de desenvolvimento é BigLinux/Arch (sem dpkg) | APT testado em contêineres Debian/Ubuntu; app testado com backends simulados |
| Fluxo polkit real depende de uma sessão com agente | helper e mapeamento de códigos do `pkexec` (126/127) testados com mocks |
| Download grande do ClamUI (runtime GNOME) | progresso real por operação do Flatpak, sem estimativa inventada |
| `freshclam` limitado pela CDN | aviso não fatal; o serviço tenta novamente |
| Scripting do Plasma bloqueado (widgets travados) | erro exibido, sem alteração parcial silenciosa |
