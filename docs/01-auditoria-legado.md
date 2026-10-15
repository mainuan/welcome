# Auditoria do legado

## Inventário

| Arquivo | Responsabilidade | Tecnologia | Funcionalidades | Dependências | Problemas | Destino |
|---|---|---|---|---|---|---|
| `index.html` | Interface completa | HTML/CSS/JS inline | Início, Office, Navegadores, Layouts comentados, Tutoriais, Sobre, Contribuir | Electron, CDNs Lucide/Tailwind/Google Fonts, picsum | WebView, online, dados fictícios, `require` no fallback, duplicações no JS, estado sem feedback | Removido; páginas reescritas em QML |
| `main.js` | Janela e single-instance | Electron/Node | BrowserWindow, lock de instância | Electron, Node | Chromium, `nodeIntegration: true`, sem foco da segunda instância | Removido; `SingleInstance` + `QQuickWindow` |
| `preload.js` | Ponte para comandos | Electron/Node | CLI assíncrona/síncrona, Flatpak por caminho fixo | `child_process`, fs, shell | processo síncrono bloqueante, caminho incompleto do Flatpak, API stringificada | Removido; serviços C++ tipados |
| `package.json` | Manifesto de execução | npm | `electron .` | npm/Electron 30 | runtime proibido no produto final | Removido; CMake |
| `welcome-cli.sh` | Ações de sistema | Bash | Tema, accent, firewall, codecs, drivers, apps, links, layouts | `sudo`, apt, kcmshell, qdbus, Latte, Bismuth, Konsole | injection via `bash -c`, sudo geral, IDs sem allowlist, tema bugado, layouts obsoletos | Removido do runtime; lógica segura em C++ |
| PNG de aplicativos | Ícones/cards | PNG | Cards Office/Navegadores | nenhum | dependência local válida | Embutidos em recursos Qt |
| SVG `icone-videoaulas_*` | Cards de tutoriais | SVG | thumbnails locais | nenhum | metadados de máquina no SVG, sem vídeos no repo | Embutidos; thumbnails preservados |

## Funcionalidades descobertas

O legado oferece 6 páginas visíveis: Início, Office, Navegadores, Tutoriais, Sobre e Contribuir. A página Layouts está comentada em `CFG.pages`, mas possui dados, previews SVG e dispatcher shell completo. Ela foi reativada na aplicação Qt com estado explícito: somente o perfil Plasma padrão é reconhecido; Latte Dock, Bismuth e os perfis que dependem de edição interna do Plasma 6.6 aparecem como incompatíveis, nunca como sucesso falso.

Os seis IDs Flatpak de navegadores e os dois de Office nativo foram confirmados com `flatpak remote-info flathub` no host. `webapp.gdocs` e `webapp.office365` foram rejeitados pelo próprio Flatpak como IDs inválidos (nomes precisam de pelo menos dois pontos); a UI os mantém como ações online explícitas para `docs.google.com` e `office.com`, sem tentar instalar pacotes falsos.

## Problemas de dados

Os nomes Ana Ferreira, Carlos Mendes, Priya Nair, Lucas Oliveira, Marina Costa, Diego Rocha, Sofia Lima, Fernanda Castro e Gustavo Melo, junto com os avatares `picsum.photos`, aparecem apenas no protótipo e não são confirmados por metadados do projeto. Foram removidos, substituídos por créditos verificáveis ao projeto Mainuan, KDE e Ubuntu e por um link para o repositório.

O QR Code Pix era explicitamente placeholder e a chave estava vazia. A versão Qt mostra “não configurada” e só habilita copiar quando `/etc/mainuan/welcome.conf` ou `~/.config/mainuan/welcome.conf` tiver `[Contribute] PixKey`.
