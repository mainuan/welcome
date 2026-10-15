# Shell e integração com o sistema

## Auditoria do shell legado

`bash -n rootfs/usr/share/welcome/welcome-cli.sh` passou. ShellCheck não está instalado no host, portanto não foi possível emitir um resultado ShellCheck. A revisão manual encontrou:

- `COLOR_MODE` e `ACCENT_COLOR` usam expansão aritmética `$((...))`, aparentando ser um erro de execução, não uma chamada de comando válida;
- `set -euo pipefail` aparece depois de trabalho e declarações, reduzindo a cobertura de falhas;
- `terminal()` monta texto externo em `bash -c`;
- `install()` e `remove()` passam IDs sem allowlist e sem quoting robusto;
- `sudo apt`, `sudo freshclam` e `sudo` dentro de terminal dão privilégios administrativos gerais ao fluxo;
- `accent` aplica o mesmo comando duas vezes;
- `theme` grava `AccentColor` com `dark`/`light`, valor inválido para uma cor;
- `qdbus6 ... loadLayout`, `Latte Dock` e `Bismuth` não são baseline confiável do Plasma 6.6;
- `xdg-open` e `telegram-desktop` são executados de forma fire-and-forget, sem feedback;
- scripts de codecs e antivírus exibem um terminal e esperam Enter, sem estado para a UI.

O script foi removido do runtime. Não restou shell necessário para a aplicação.

## Integração Qt escolhida

- `QProcess` com programa e lista de argumentos, nunca `system()`, `popen()`, `bash -c` ou concatenação de comando;
- `QDesktopServices::openUrl` para URLs `http`, `https` e `tg` validadas;
- KConfig (`KSharedConfig` + `KConfigWatcher`) para ler `kdeglobals`/`plasmarc` pela cascata do KDE e acompanhar mudanças;
- `QFileSystemWatcher` para reagir a alteração de tema sem polling agressivo;
- `QLocalServer`/`QLocalSocket` para single-instance;
- `QtMultimedia` para vídeos locais;
- Kirigami e ícones do tema para integração visual.

Não foi necessária uma chamada DBus direta para as funções presentes. O dispatcher antigo usava `qdbus6` para operações que não têm contrato estável para layouts no Plasma 6.6; essa integração foi deliberadamente substituída por estado explícito de incompatibilidade.
