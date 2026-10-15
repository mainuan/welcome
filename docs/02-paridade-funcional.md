# Matriz de paridade funcional

| Funcionalidade atual | Implementação Qt | Implementação concluída | Teste/resultado |
|---|---|---|---|
| Single instance | `QLocalServer`/`QLocalSocket` com socket privado do usuário | Sim | Teste unitário/build; foco Wayland requer sessão manual |
| Navegação entre páginas | `Kirigami.ApplicationWindow` + `StackLayout` + botões acessíveis | Sim | QML compilado e smoke offscreen PASS |
| Tema claro/escuro | leitura de `kdeglobals` + `plasma-apply-colorscheme` com argumentos separados | Sim | API validada; alteração real não executada para não modificar o desktop do teste |
| Cor de destaque | validação `QColor` + `plasma-apply-colorscheme --accent-color` | Sim | allowlist/Qt Test PASS; comando real não executado |
| Antivírus | diagnóstico local de `clamd`, Discover como fluxo de configuração | Sim | ausência é tratada; instalação não automática |
| Firewall | consulta assíncrona `ufw status`, abertura de `kcmshell6 kcm_firewall` | Sim | comando depende do host; estado de ausência tratado |
| Codecs | detecção de ffmpeg/GStreamer, Discover para configuração | Sim | detecção local implementada |
| Drivers | abertura do `kubuntu-driver-manager` se existente | Sim | ausência tratada |
| Office | `ApplicationModel` C++ com os 2 Flatpaks confirmados e 2 serviços online | Sim | IDs confirmados com `flatpak remote-info`; instalação não executada |
| Navegadores | `ApplicationModel` C++ com 6 itens legados e estados | Sim | igual a Office |
| Detecção Flatpak | `flatpak list --app --columns=application,installation` assíncrono | Sim | ausência de Flatpak tratada; comando de consulta no startup |
| Instalar/remover | `QProcess` com argumentos fixos, `--user` para instalação e polkit do Flatpak para remoção de sistema | Sim | não executado contra estado do usuário |
| Paginação | removida: catálogo atual tem 10 itens, cabe em layout responsivo | Justificada | não havia necessidade real do limite 20 |
| Tutoriais | `VideoModel`, `QtMultimedia`, seek, play/pause, volume e tela cheia | Parcialmente | player lazy e QML compilado; vídeos não existem no host, reprodução real pendente |
| Vídeo ausente | item desabilitado com “Arquivo não encontrado” | Sim | comportamento derivado de `QFileInfo` |
| Layouts | página reativada; perfil padrão informado, legados marcados incompatíveis | Substituída com justificativa | não promete APIs obsoletas |
| Sobre | QML, créditos verificáveis, sem fotos externas | Sim | QML compilado |
| Contribuir | GitHub, issue, Telegram, Pix copiar/estado ausente | Sim | URLs validadas por allowlist; abertura real não executada |
| Offline | assets PNG/SVG embutidos; nenhuma CDN | Sim | busca no runtime Qt sem HTML/CDN; validação de rede real pendente |
