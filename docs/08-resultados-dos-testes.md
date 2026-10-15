# Resultados dos testes

Executados em 15/09/2026 no host Plasma 6.7.4/Wayland.

| Teste | Comando | Esperado | Obtido | Resultado |
|---|---|---|---|---|
| Configuração | `cmake -S . -B build-test -DCMAKE_BUILD_TYPE=Debug` | configurar | configurou | PASS |
| Build | `cmake --build build-test -j$(nproc)` | compilar | binário e teste gerados | PASS |
| Unit | `ctest --test-dir build-pristine --output-on-failure` | 4 testes funcionais | 6/6 incluindo init/cleanup, 0,06 s | PASS |
| QML lint | `qmllint -I build-test/Mainuan/Welcome qml/Main.qml qml/components/*.qml qml/pages/*.qml` | sem erro | sem saída/erro | PASS |
| Desktop file | `desktop-file-validate rootfs/usr/share/applications/mainuan-welcome.desktop` | válido | exit 0 | PASS |
| Shell syntax legado | `bash -n rootfs/usr/share/welcome/welcome-cli.sh` antes da remoção | sintaxe válida | passou | PASS histórico |
| ShellCheck | `shellcheck ...` | análise | executável ausente | NÃO EXECUTADO |
| Smoke QML final | `env -u QT_LOGGING_RULES QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software timeout 2s ./build-pristine/bin/mainuan-welcome` | permanecer ativo e sem warnings | timeout 124, log vazio; assets e páginas carregaram | PASS |
| Smoke XCB | `env -u QT_LOGGING_RULES QT_QPA_PLATFORM=xcb QT_QUICK_BACKEND=software timeout 2s ./build-pristine/bin/mainuan-welcome` | iniciar sem warnings | timeout 124, log vazio | PASS técnico; não substitui sessão X11 |
| Inspeção visual XCB | janela em três monitores; Home e Tutoriais | conteúdo visível, centralizado e legível | corrigidos posicionamento, contraste, grids sem altura e botões comprimidos; cartões de tutoriais ausentes ficaram identificados | PASS visual no backend XCB |
| Build limpo final | `cmake -S . -B build-pristine && cmake --build build-pristine && ctest --test-dir build-pristine` | sem resíduos de build | Release e CTest 1/1 passaram | PASS |
| Smoke inicial com MediaPlayer eager | mesmo comando antes do lazy load | sem crash | SIGSEGV em `libvkbasalt`/Vulkan | CORRIGIDO |
| Vídeos reais | procurar `/var/lib/curso-linux/videos` | reproduzir | arquivos não presentes no host | NÃO EXECUTADO |
| Instalação Flatpak | operação install/remove | alterar sistema | não executado para não alterar estado do usuário | NÃO EXECUTADO |
| IDs Flatpak | `flatpak remote-info flathub <id>` | confirmar catálogo | 8 IDs reais encontrados; `webapp.*` rejeitados como inválidos | PASS |
| Plasma 6.6/X11 | sessão dedicada | abrir e integrar | Plasma 6.6/X11 real não disponível; XCB foi exercitado | NÃO EXECUTADO |
| QML Profiler/perf | profiling | métricas | não executado | NÃO EXECUTADO |

Não foram marcados como PASS testes que exigem uma sessão Plasma 6.6, X11, vídeos ou alteração real de pacotes.
