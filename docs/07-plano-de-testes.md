# Plano de testes

> Registro histórico do desenvolvimento experimental. Os números 2.x, resultados de execução e caminhos de capturas descrevem aquele momento. A primeira versão oficial é **1.0**; consulte a [documentação atual](README.md) antes de seguir instruções antigas.

| Área | Teste |
|---|---|
| Configuração | `cmake -S . -B build-test` em diretório limpo |
| Compilação | `cmake --build build-test -j"$(nproc)"` com warnings fortes |
| Unit | `ctest --test-dir build-test --output-on-failure` |
| QML | `qmllint` em `qml/Main.qml`, `qml/components/*.qml`, `qml/pages/*.qml` |
| Desktop/AppStream | `desktop-file-validate` e XML parser |
| Startup | Wayland real, X11, e `QT_QPA_PLATFORM=offscreen` apenas como smoke |
| Single-instance | abrir duas vezes, foco/restauração em Wayland |
| Páginas | navegar por todas as 7 páginas com mouse e teclado |
| Tema | trocar claro/escuro e observar watcher |
| Accent | testar paleta e esquema personalizado |
| Flatpak | Flatpak ausente, app user/system, install/remove, erro e offline |
| Vídeo | pasta ausente, vídeo válido, play/pause/seek/volume/fullscreen |
| Falhas | DBus/Plasma helper ausente, timeout, exit code diferente de zero |
| Acessibilidade | foco por teclado, screen reader, 100–250% de escala |
| Layout | confirmar que legados não são reportados como aplicados |
| Instalação | `cmake --install` em staging e validar arquivos FHS |
