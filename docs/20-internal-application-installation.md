# Instalação interna dos três aplicativos — 2.3.1.4

> Registro histórico do desenvolvimento experimental. Os números 2.x, resultados de execução e caminhos de capturas descrevem aquele momento. A primeira versão oficial é **1.0**; consulte a [documentação atual](README.md) antes de seguir instruções antigas.

Vivaldi (`com.vivaldi.Vivaldi`) e Zen Browser (`app.zen_browser.zen`) usam agora
os mesmos perfis `app:<id>` do InstallService, a instalação existente do Flathub,
o InstallDialog, a detecção assíncrona e a abertura/remoção Flatpak. O catálogo
não os encaminha mais ao Discover. Um `flatpak info --show-ref` confirma a
instalação desses dois navegadores antes de apresentar sucesso.

FreeOffice usa a ação explícita `InternalDebian` e o perfil fechado `freeoffice`.
Google Docs e Office 365 continuam como `WebPage`. O estado Debian vem de
`dpkg-query` para `softmaker-freeoffice-2024`, inclusive ao retornar à janela;
não há remoção APT neste perfil, nem tentativa de remoção via Flatpak.

## Procedência verificada

Em 17/10/2026, uma consulta manual HTTPS, com validação do certificado e sem
seguir redirecionamentos, recebeu HTTP 200 da URL exata autorizada:

https://www.softmaker.net/down/softmaker-freeoffice-2024_1238-01_amd64.deb

A página oficial de instalação Linux do FreeOffice também indica esse arquivo:
https://www.freeoffice.com/en/support/installation/linux

`dpkg-deb --info` identificou:

| Campo | Valor |
| --- | --- |
| Package | `softmaker-freeoffice-2024` |
| Version Debian | `3714` |
| Revisão comercial do FreeOffice 2024 | `1238` |
| Architecture | `amd64` |
| Tamanho do arquivo | `167839604` bytes |
| Maintainer | `SoftMaker GmbH <support@softmaker.com>` |
| SHA-256 medido | `fddd0d829982d4bb0fd080fa26118134a110f0ae39a0e5902b2609172c84387a` |

Não foi localizado um checksum publicado pelo fabricante para esse arquivo nas
páginas oficiais consultadas. O hash acima foi calculado do arquivo recebido da
origem oficial por HTTPS e fixado pelo Welcome; **não é apresentado como uma
assinatura ou checksum publicado pela SoftMaker**. O pacote contém scripts
`postinst` e `prerm`; eles não foram executados durante esta verificação.

## Fluxo e fronteira de privilégios

1. O modal padrão informa a origem SoftMaker, a versão e a execução de scripts
   do fabricante como administrador, e oferece **Instalar** antes de iniciar.
2. O InstallService baixa assincronamente com Qt Network, valida TLS normalmente
   e recusa todos os redirecionamentos. A URL é exata, sem parâmetros, credenciais
   ou outra origem. Há limites de tamanho e tempo. O percentual vem dos bytes
   recebidos quando o total informado corresponde ao tamanho autorizado; nas
   outras fases sem percentual confiável o progresso é indeterminado.
3. O download fica em um diretório temporário privado. Tamanho e SHA-256 são
   conferidos durante o recebimento; `dpkg-deb --show` valida nome, versão e
   arquitetura antes de qualquer pedido de autenticação.
4. `pkexec` chama somente `mainuan-welcome-helper install freeoffice`. A interface
   não transmite URL, caminho, nome de pacote ou hash. O arquivo é transmitido
   pelo stdin com uma fila limitada; não é carregado integralmente na RAM.
5. O helper cria um diretório imprevisível pertencente ao root em `/var/tmp/`,
   recebe no máximo o tamanho autorizado mais um byte, com prazo de 180 segundos,
   e repete a verificação de SHA-256 e metadados usando seu perfil instalado.
   Arquivos truncados, maiores, links e conteúdo diferente são recusados.
6. O arquivo validado fica somente para leitura, em diretório onde apenas root
   pode modificar entradas. `_apt` recebe acesso de leitura, sem permissão de
   substituir o pacote. O mesmo arquivo é passado ao APT, evitando troca entre
   validação e instalação. A limpeza ocorre ao terminar o helper.
7. A infraestrutura existente aguarda os locks e usa `apt-get install -y
   --no-remove /var/tmp/.../freeoffice.deb`, com resolução de dependências e
   preservação de configurações. Nenhum lock é apagado e nenhuma remoção é
   autorizada para resolver conflitos.
8. O helper exige o estado `ii` no dpkg, e o InstallService consulta novamente
   `dpkg-query` antes de apresentar sucesso. Erros, autenticação cancelada e
   conclusão sem instalação confirmada mantêm a opção de tentar novamente.

O único manifesto é `rootfs/usr/share/mainuan-welcome/freeoffice.profile`,
incorporado nos recursos Qt e instalado em `/usr/share/mainuan-welcome/` para o
helper. É dado, nunca interpretado como script. A versão, origem, arquitetura,
tamanho e hash ficam restritos a esse artefato. Se o fabricante mudar seu
conteúdo ou endereço, o Welcome falha de forma explícita até uma atualização
revisada do perfil; não escolhe outro produto ou versão.

## Validação e limites

Os testes usam respostas Qt Network simuladas, pacotes mínimos criados em
diretórios temporários, banco dpkg simulado, helper e Flatpak simulados. Não
baixam nem instalam os três aplicativos reais. Testam inclusive o validador
real do helper sem executar seu caminho privilegiado, recusa de argumentos
arbitrários, integridade, arquitetura/nome/versão, cancelamento de autenticação,
falso sucesso, atualização do catálogo e ícones nas telas QML.

A instalação efetiva via Polkit, a resolução das dependências do pacote do
fabricante e a abertura dos programas devem ser confirmadas manualmente em
Mainuan/Wayland. O pacote Debian do Welcome contém somente o aplicativo, o helper,
o manifesto e os recursos habituais; não incorpora o instalador do FreeOffice
nem os navegadores. Layouts, temas, ícones Kora, avatares e wallpapers não foram
alterados nesta implementação.

A verificação manual adicional `apt-get --simulate --no-remove install` com o
arquivo oficial identificou somente `softmaker-freeoffice-2024` como novo pacote
na máquina de desenvolvimento, sem upgrades nem remoções. Isso valida a
resolução local das dependências disponíveis, sem executar a instalação ou os
scripts do fabricante.

## Arquivos desta alteração

Modificados:

- `CMakeLists.txt`, `debian/changelog`, `README.md`.
- `src/main.cpp`.
- `src/models/ApplicationModel.cpp`, `src/models/ApplicationModel.h`.
- `src/services/InstallService.cpp`, `src/services/InstallService.h`.
- `src/services/PackageService.cpp`.
- `qml/components/AppCard.qml`, `qml/components/InstallDialog.qml`.
- `qml/pages/AppsPage.qml`.
- `rootfs/usr/libexec/mainuan-welcome/mainuan-welcome-helper`.
- `tests/tst_mainuan.cpp`, `tests/fixtures/flatpak`, `tests/fixtures/helper`.

Criados:

- `src/services/FreeOfficeProfile.cpp`, `src/services/FreeOfficeProfile.h`.
- `rootfs/usr/share/mainuan-welcome/freeoffice.profile`.
- `tests/tst_appinstallation.cpp`, `tests/apps-qml.sh`.
- `tests/qml/tst_Applications.qml`.
- `docs/20-internal-application-installation.md`.

Os outros arquivos já modificados ou não rastreados no Git pertencem às
alterações locais anteriores e foram preservados.
