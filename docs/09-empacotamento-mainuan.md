# Empacotamento Mainuan

## Arquivos instalados

| Destino | Conteúdo |
|---|---|
| `/usr/bin/mainuan-welcome` | executável Qt nativo |
| `/usr/share/applications/mainuan-welcome.desktop` | entrada de menu/launcher |
| `/usr/share/metainfo/org.mainuan.Welcome.metainfo.xml` | metadata AppStream |
| recursos Qt do executável | PNG/SVG locais; não dependem de `/usr/share/welcome` em runtime |

O diretório legado `/usr/share/welcome` deixa de receber HTML, JS, JSON ou shell. Os arquivos gráficos existentes são apenas fontes de recursos no build e são embutidos pelo `qt_add_resources`.

## Dependências

- Build: `cmake`, compilador C++17, Qt6 Core/Gui/Qml/Quick/QuickControls2/Network/DBus/Multimedia/Test.
- Runtime obrigatório: Qt6, Qt Quick Controls 2, Kirigami 6 e plugins gráficos/multimedia correspondentes.
- Runtime opcional: Flatpak, Discover, UFW, `kcmshell6`, `kubuntu-driver-manager`.

O pacote deve declarar as bibliotecas Qt/Kirigami correspondentes à versão do Kubuntu/Mainuan alvo, sem fixar Qt 6.11. A instalação deve rodar `desktop-file-validate` e verificar que o AppStream referencia o desktop ID instalado.
