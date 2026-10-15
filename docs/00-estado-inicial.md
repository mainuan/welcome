# Estado inicial

Auditoria realizada em 14/09/2026, antes da implementação, em `/home/ruscher/Documentos/Git/welcome5`.

| Item | Resultado |
|---|---|
| Branch | `main` |
| HEAD | `94e3de5 Update welcome-cli.sh` |
| Estado Git | limpo (`## main...origin/main`) |
| Histórico observado | cinco commits recentes concentrados em `welcome-cli.sh`, `index.html` e `preload.js` |
| Arquivos versionados | 20: 10 PNG, 6 SVG, HTML, 2 JS, JSON e shell |
| Build system | inexistente |
| Interface | HTML/CSS/JavaScript com CDNs |
| Backend | Electron preload + shell script |

Nenhuma alteração existente do usuário foi descartada. A árvore inicial não continha `docs/`, `src/`, `qml/`, `CMakeLists.txt`, testes, desktop file ou metadata.

## Versões detectadas

No host de desenvolvimento:

| Componente | Versão/resultado |
|---|---|
| Qt 6 via `qmake6`/pkg-config | 6.11.2 |
| Qt 5 via `qmake` | 5.15.19 (não usado) |
| Plasma shell | 6.7.4 |
| CMake | 4.4.3 |
| GCC/G++ | 16.2.1 |
| Flatpak | 1.18.2 |
| Kirigami/KF6 | QML e CMake disponíveis; Kirigami detectado como 6.29.0 |
| `qmllint` | disponível |
| `desktop-file-validate` | disponível |
| `shellcheck` | não disponível |
| `clang-tidy` | disponível; não executado sobre a primeira versão |
| Sessão | Wayland |

O host não é Plasma 6.6. Por isso o projeto não exige Qt 6.11 nem APIs exclusivas do Plasma 6.7+: o CMake declara Qt 6.5 como mínimo e a compatibilidade do baseline é tratada por APIs genéricas Qt/KDE.
