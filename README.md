<div align="center">

<img src="https://raw.githubusercontent.com/mainuan/.github/main/profile/assets/mainuan-logo.svg" width="96" alt="Logotipo do Mainuan">

# Mainuan Welcome

Tela de boas-vindas do **Mainuan Linux**: aparência, layout do painel, aplicativos essenciais e informações do sistema num só lugar.

[![CI](https://github.com/mainuan/welcome/actions/workflows/build-deb.yml/badge.svg)](https://github.com/mainuan/welcome/actions/workflows/build-deb.yml)
[![Release](https://img.shields.io/github/v/release/mainuan/welcome)](https://github.com/mainuan/welcome/releases/latest)
[![Licença](https://img.shields.io/github/license/mainuan/welcome)](LICENSE)
![Plataforma](https://img.shields.io/badge/Kubuntu%2026.04%20%7C%20Plasma%206.6-1d99f3)

**Português** · [English](README.en.md)

</div>

---

## O que é

O Mainuan Welcome abre no primeiro login e ajuda a deixar o sistema do jeito do usuário: estilo visual, cores, layout dos painéis do Plasma e instalação de codecs, antivírus, firewall, suítes de escritório e navegadores. Cada usuário pode desativar a abertura automática.

É um aplicativo nativo em C++17, Qt 6/QML e Kirigami, sem Electron, Node.js, Chromium embarcado ou WebView. As ações administrativas passam por um helper com perfis fechados, autorizado pelo Polkit; o Welcome nunca pede nem guarda senhas e não tem telemetria.

Os temas, esquemas Dream, ícones Kora, wallpapers, avatares e plasmoides personalizados pertencem ao Mainuan e não são duplicados no pacote. Sem esses recursos, os serviços tratam a ausência, mas instalar o Welcome num Kubuntu comum não reproduz toda a personalização do Mainuan.

## Funcionalidades

- Estilos Dream, Tahoe e Breeze, com variantes clara e escura e seis cores (Marrom, Ciano, Magenta, Lima, Cinza e Violeta), cada uma com esquema KDE, ícones Kora, wallpaper e avatar correspondentes.
- Sete layouts do Plasma: Clássico (5 barras), 3 barras, Barra única, Tiling, Painel superior, Lomiri e Barra W7, com verificação de widgets, backup automático e restauração.
- Instalação de suítes de escritório e navegadores (entre eles FreeOffice, Vivaldi e Zen) via Flatpak ou pacote Debian, com progresso, mensagens de erro acessíveis e autenticação do sistema quando necessária.
- Perfis de antivírus, firewall e codecs; atalhos para Google Docs e Office 365.
- Atalho para o [Mainuan Gestão de Drivers](https://github.com/mainuan/mainuan-gestao-drivers), quando instalado.
- Relatório da máquina (coletado só quando pedido, sem identificadores sensíveis), tutoriais locais, página de contribuição por Pix com QR Code gerado localmente e opção de abrir no login.

## Instalação

O Welcome já vem instalado na ISO do Mainuan. Para instalar ou atualizar manualmente, baixe `mainuan-welcome_1.0.1_amd64.deb` e `SHA256SUMS` da [última release](https://github.com/mainuan/welcome/releases/latest) e, na pasta dos dois arquivos:

```bash
sha256sum --check --ignore-missing SHA256SUMS
sudo apt install ./mainuan-welcome_1.0.1_amd64.deb
```

Para reinstalar a mesma versão, acrescente `--reinstall`. As Releases do GitHub não são um repositório APT: atualizações não chegam sozinhas por `apt upgrade`.

> **Vindo de uma versão experimental 2.x?** A 1.0 foi a primeira versão oficial e é numericamente inferior à série 2.x (`dpkg --compare-versions 2.3.1.5 gt 1.0` retorna zero). Confira a versão instalada com `dpkg-query -W -f='${Version}\n' mainuan-welcome` e autorize o downgrade explicitamente com `sudo apt install --allow-downgrades ./mainuan-welcome_1.0.1_amd64.deb`. Não há epoch nem migração automática; se uma fonte APT ainda oferecer 2.x, o APT pode voltar a preferi-la. Detalhes em [docs/releasing.md](docs/releasing.md).

## Uso

Abra **Mainuan Welcome** no menu de aplicativos (categoria Configurações) ou execute:

```bash
mainuan-welcome
```

O comando não tem opções. Iniciar uma segunda cópia traz a janela aberta para a frente. A janela abre em 870 × 600 (mínimo 760 × 520) e se adapta a telas menores.

| Arquivo | Função |
|---|---|
| `/etc/xdg/autostart/org.mainuan.Welcome.desktop` | abre o Welcome no login, para todos os usuários |
| `~/.config/autostart/org.mainuan.Welcome.desktop` | criado com `Hidden=true` quando o usuário desmarca "Mostrar esta tela ao iniciar o sistema" |
| `/etc/mainuan/welcome.conf` | chave Pix da página Contribuir (com configuração pessoal como alternativa) |
| `/usr/libexec/mainuan-welcome/mainuan-welcome-helper` | helper privilegiado, executado por `pkexec`; aceita só perfis fixos |
| `/var/log/mainuan-welcome/install.log` | saída da última instalação de pacote (legível pelo root) |
| `~/.local/share/mainuan-welcome/layout-backups/` | backups dos arquivos do Plasma antes de trocar o layout (até cinco) |

Página de manual: `man mainuan-welcome`.

## Informações técnicas

| Item | Valor |
|---|---|
| Linguagem | C++17 e QML |
| Toolkit | Qt 6, Kirigami, KF6 (KConfig, KI18n), libqrencode |
| Base | Kubuntu 26.04 LTS, KDE Plasma 6.6, Wayland |
| Pacote | `mainuan-welcome` (Debian, `amd64`) |
| Dependências principais | módulos QML do Qt 6 e Kirigami, `pkexec`, `psmisc` (lista completa em [`debian/control`](debian/control)) |
| Arquitetura | amd64, x86-64 básico (v1/SSE2), sem exigir AVX/AVX2 |

O pacote é auditado para rodar em x86-64 básico: o CI compila num contêiner Ubuntu 26.04 sem variantes `amd64v3`, confere as flags de compilação e as notas ELF do pacote e das bibliotecas que ele carrega, e inicia o `.deb` sob QEMU com CPU sem AVX/AVX2. A ISO e suas bibliotecas também precisam respeitar esse alvo; veja [os gates de CPU](docs/cpu-compatibility.md).

Mais documentação:

- [Arquitetura, segurança, backups e isolamento dos testes](docs/architecture-and-maintenance.md)
- [Publicação, tags, checksums e migração Debian](docs/releasing.md)
- [Consolidação da 1.0.1](docs/22-final-1.0.1-ux-decoration.md) e [correção de Claro/Escuro](docs/21-initial-theme-detection.md)
- [Índice da documentação e histórico](docs/README.md)

## Compilação e testes

Num Mainuan ou Kubuntu 26.04, com as dependências de [`debian/control`](debian/control). São os mesmos comandos do workflow [Build Debian package](.github/workflows/build-deb.yml):

```bash
git clone https://github.com/mainuan/welcome.git
cd welcome
sudo apt-get install build-essential dpkg-dev fakeroot lintian
sudo apt-get build-dep --no-install-recommends ./
dpkg-checkbuilddeps

# metadados, versão e scripts de verificação
python3 -m unittest discover -s .github/scripts -p 'test_*.py'
python3 .github/scripts/check_release.py metadata --tag v1.0.1

# catálogos de tradução
po/i18n.sh check-pot && po/i18n.sh validate && po/i18n.sh coverage

# compilação e testes QtTest, QML e de inicialização (offscreen)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMAINUAN_DEVELOPER_MODE=OFF
cmake --build build --parallel 2
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ctest --test-dir build --output-on-failure --output-junit release-tests.xml
python3 .github/scripts/check_release.py tests build/release-tests.xml

# pacote
dpkg-buildpackage -us -uc -b
python3 .github/scripts/check_release.py package ../mainuan-welcome_1.0.1_amd64.deb --version 1.0.1
lintian --fail-on error ../mainuan-welcome_1.0.1_amd64.deb
```

Para rodar sem instalar: `./build/bin/mainuan-welcome`. Os testes usam diretórios temporários, ferramentas simuladas e um D-Bus privado; não instalam aplicativos nem alteram a sessão Plasma.

O CI também confere a CPU básica (`.github/scripts/check_cpu.py` e `cpu_smoke.py`) e instala o `.deb` num contêiner Ubuntu 26.04 limpo: confere os arquivos, abre o app como usuário comum, testa o helper, reinstala, remove e expurga sem sobras. A Release só é publicada por tag `v*` igual à versão do pacote e nunca substitui uma Release existente.

## Traduções

A interface está em 33 idiomas (catálogos em [`po/`](po), com inglês como idioma-fonte e `pt_BR` como interface brasileira): ar, bn, cs, da, de, el, es, fa, fi, fr, he, hi, hu, id, it, ja, ko, nb, nl, pl, pt, pt_BR, ro, ru, sv, sw, th, tr, uk, ur, vi, zh_CN e zh_TW. O idioma segue o do Plasma (*Região e idioma*).

Como traduzir e manter os catálogos: [docs/i18n/README.md](docs/i18n/README.md).

## Contribuição

Relate problemas e sugestões em [Issues](https://github.com/mainuan/welcome/issues). Pull requests para `main` passam pelo CI completo (compilação, testes, CPU, lintian e teste de instalação limpa) antes da revisão. Ao mudar textos da interface, atualize os catálogos com `po/i18n.sh extract` e `po/i18n.sh update`.

## Créditos

- **Natanael Barbosa Santos** ([@natanael-art](https://github.com/natanael-art))
- **Rafael Ruscher** ([@ruscher](https://github.com/ruscher)), mantenedor do pacote Debian
- **Daigo** ([@daigoasuka](https://github.com/daigoasuka))

Ícones de linha do [Lucide](https://lucide.dev), sob licença ISC ([qml/icons/LICENSE.lucide](qml/icons/LICENSE.lucide)).

## Licença

[GPL-3.0-or-later](LICENSE). Detalhes por arquivo em [`debian/copyright`](debian/copyright).
