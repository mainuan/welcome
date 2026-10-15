# Relatório final

## Resumo

O protótipo Electron foi reconstruído como aplicação Qt 6 nativa com C++/QML/Kirigami. A aplicação agora possui páginas Início, Office, Navegadores, Tutoriais, Layouts, Sobre e Contribuir, com recursos locais embutidos.

## Arquitetura final

CMake moderno, Qt 6.5+ baseline, C++17, `QAbstractListModel`, `QProcess` assíncrono, `QLocalServer`, Qt Multimedia lazy e QML compilável/cacheável. Kirigami é usado na janela e páginas; não há WebView.

## Removido

`index.html`, `main.js`, `preload.js`, `package.json` e `welcome-cli.sh` foram removidos do runtime. Não há dependência de Electron, Node.js, npm, Chromium, Tailwind, Lucide CDN, Google Fonts CDN ou imagens remotas.

## Funcionalidades migradas

Tema, accent, diagnósticos, firewall, drivers, codecs, antivírus, Flatpak, Office, navegadores, vídeos ausentes, controles de reprodução, layouts auditados, créditos, links GitHub/issue/Telegram e Pix configurável.

## Melhorias

Allowlist de IDs/URLs/cores/layouts, sem `sudo`/shell injection, feedback de operação, tratamento de dependências ausentes, single-instance Qt, Wayland-first, fontes/ícones do sistema e operação offline.

A revisão visual também corrigiu o posicionamento multi-monitor, o contraste da paleta escura, o dimensionamento dos `Flow` dentro de layouts, a largura dos botões e a identificação de vídeos ausentes.

## Testes

Build, CTest, qmllint, desktop-file-validate e smoke QML passaram. A tabela completa está em `docs/08-resultados-dos-testes.md`.

## Performance

O binário Release instalado mede 796.464 bytes; não há medição honesta de RAM/CPU/startup ainda. O ambiente antigo não trazia runtime Electron instalado, então não há comparação numérica de Chromium/processos. O player é lazy e processos de sistema são assíncronos.

## Compatibilidade

Desenhado para Plasma 6.6 Wayland com Qt 6.5+; desenvolvido em Plasma 6.7.4/Wayland. X11, Plasma 6.6 real, 6.8 e reprodução com vídeos presentes ainda precisam de sessões de validação.

## Pendências reais

1. Validar em Plasma 6.6 Wayland e X11.
2. Executar instalação/remoção Flatpak em ambiente de teste.
3. Reproduzir uma aula existente em `/var/lib/curso-linux/videos`.
4. Extrair strings para TS/i18n e revisar com screen reader/High DPI.
5. Medir Release com RAM/CPU/startup.
