# Arquitetura Qt

```text
src/
├── main.cpp
├── services/
│   ├── SystemService.*       tema, diagnósticos, URLs, clipboard e ações
│   ├── PackageService.*      Flatpak assíncrono e allowlist
│   └── SingleInstance.*      socket local privado
└── models/
    ├── ApplicationModel.*    Office e Navegadores
    ├── VideoModel.*          vídeos locais
    └── LayoutModel.*         matriz de compatibilidade
qml/
├── Main.qml
├── components/               botões, linhas de estado e cards
└── pages/                    Início, apps, tutoriais, layouts, Sobre, Contribuir
tests/tst_mainuan.cpp
```

## Contratos

C++ expõe apenas `Q_PROPERTY`, sinais e `Q_INVOKABLE` pequenos. Processos são assíncronos; nenhuma operação de Flatpak, tema, firewall ou versão do Plasma bloqueia a thread da interface. Modelos são `QAbstractListModel` e fornecem roles nomeadas para QML.

QML concentra apresentação, navegação e interação. Não executa shell, não acessa caminhos arbitrários, não parseia comandos nem constrói privilégio. Assets são compilados em recursos Qt, permitindo execução sem internet e sem `docs/`.

Kirigami é usado para `ApplicationWindow`, `Page`, `Heading` e `Icon`. O código não usa APIs específicas de Plasma 6.7/6.8. CMake procura Kirigami opcionalmente para link/import estático, mas o mínimo de Qt é 6.5; a dependência runtime do pacote deve fornecer o módulo Kirigami do Plasma alvo.
