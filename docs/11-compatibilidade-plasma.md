# Compatibilidade Plasma

| Ambiente | Projeto | Evidência/estado |
|---|---|---|
| Plasma 6.6 Wayland | obrigatório | APIs usadas são Qt/Kirigami públicos; ainda não há host 6.6 para teste final |
| Plasma 6.6 X11 | desejável | não disponível neste host |
| Plasma 6.7 Wayland | teste de desenvolvimento | host detectado 6.7.4; smoke offscreen passou; inspeção gráfica foi feita em XCB sob Wayland |
| Plasma 6.8 | forward compatibility | não disponível |

## APIs e baseline

| API | Uso | Motivo de compatibilidade |
|---|---|---|
| Qt Quick Controls 2 | botões, sliders, scroll | Qt 6.5+ |
| Kirigami ApplicationWindow/Page/Icon | integração visual | módulo fornecido pelo KDE; sem API específica do Plasma |
| KConfig + kdeglobals/plasmarc | leitura de tema, accent e estilo visual (inclui `~/.config/kdedefaults`) | API estável do KF6 |
| `plasma-apply-colorscheme` | aplicação solicitada pelo usuário | executável externo validado e assíncrono |
| QProcess | processos opcionais | contrato Qt estável |
| QDesktopServices | links | API Qt, sem shell |
| QLocalServer | single-instance | API Qt, socket com `UserAccessOption` |
| Qt Multimedia | vídeo | plugin runtime do Qt; player lazy |

Não são usadas APIs internas de plasmashell, edição forçada de `plasmashellrc`, Latte Dock, Bismuth, `loadLayout` sem contrato ou hacks X11. A aplicação informa incompatibilidade para os layouts legados.
