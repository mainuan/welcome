# Performance

## Instrumentação e decisões

- Não há Chromium, WebView, polling de 1 segundo ou loop permanente de atualização.
- Detecção Flatpak ocorre por processo assíncrono e apenas uma vez no startup/ao terminar operação.
- Estado de tema reage a `QFileSystemWatcher`.
- O player Qt Multimedia é lazy: seus objetos só são criados ao abrir um vídeo, reduzindo custo de startup e evitando inicialização de codecs quando o usuário não usa tutoriais.
- Cards e imagens usam recursos locais e carregamento assíncrono.

## Medições disponíveis nesta execução

| Medida | Resultado |
|---|---|
| Build Debug | concluído |
| Testes unitários | 0,07 s para 3 testes |
| Smoke startup offscreen sem player | permaneceu ativo por 3 s; `timeout` retornou 124 conforme esperado |
| RAM/CPU | não medido com processo persistente controlado |
| tempo até janela utilizável | não medido com relógio de sessão gráfica |
| troca de páginas | não medido com QML Profiler |
| tamanho release instalado | 796.464 bytes para o binário; assets ficam embutidos |
| Electron antigo | runtime não estava no repositório; o payload legado versionado tinha cerca de 300 KiB, mas não é comparação válida com Chromium |

O ambiente injeta `vkBasalt`/`liblsfg-vk`; criar `MediaPlayer` eagerly causou SIGSEGV dentro dessa camada Vulkan. O player foi tornado lazy e o smoke sem vídeo passou. A reprodução real deve ser medida em uma sessão limpa sem esse overlay.
