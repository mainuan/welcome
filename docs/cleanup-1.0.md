# Auditoria e limpeza para o Mainuan Welcome 1.0

## Inspeção anterior às mudanças

Foram executados `git status --short`, `git status --ignored --short`,
`git ls-files`, `git diff --stat` e `git diff --check`. O estado inicial tinha
35 arquivos rastreados modificados, além de código, recursos e testes locais
ainda não rastreados. Essas alterações anteriores foram preservadas.

Foram inventariados os fontes, recursos, documentação, configurações Debian e
todo o conteúdo de `build/`, com finalidade, tamanho e SHA-256 dos arquivos.
O diretório antigo continha **915 arquivos gerados**, somando **228.572.340 bytes**,
em **32 entradas de primeiro nível**. Não havia arquivo rastreado dentro dele,
nem links simbólicos. O inventário completo e os comandos originais estão em
`build/release-validation/old-build-inventory.json` e `git-*-before.txt`, ignorados
pelo Git como registros locais desta preparação.

## A — necessários, preservados

- Todos os 38 arquivos C++/headers/scripts de `src`, incluindo os novos serviços
  FreeOffice e UserAvatar e os layouts convertidos.
- Todos os 46 arquivos QML/ícones de `qml`; somente a identificação de versão e
  o endereço oficial na página Sobre foram atualizados nesta etapa.
- Todos os 28 arquivos de `rootfs`, incluindo as três imagens personalizadas,
  helper, manifesto FreeOffice e integração do sistema. Apenas URLs/metadados
  oficiais foram atualizados nos arquivos AppStream e Polkit.
- Todos os 19 arquivos de testes e fixtures; nenhum teste funcional foi removido.
- Configurações Debian, manpage, créditos, licença Lucide e autoria/maintainer.
  A licença GPL já declarada foi disponibilizada também como `LICENSE`.
- Documentação e as oito capturas em `docs/screenshots`, mantidas como referência
  histórica, sem tratá-las como evidência visual da nova Release.

## B — produtos temporários removidos

Após conferir seu conteúdo, foram removidas somente estas entradas antigas
dentro de `build/`, recursivamente quando eram diretórios:

```text
.qt/                         .rcc/                       CMakeFiles/
CMakeCache.txt               CTestTestfile.cmake          Makefile
cmake_install.cmake          Testing/                    Mainuan/
bin/                         meta_types/                 qmltypes/
mainuan-core_autogen/        mainuan-welcome_autogen/     tst_mainuan_autogen/
tst_layouts_autogen/         tst_appinstallation_autogen/
libmainuan-core.a            mainuan-welcome_qmltyperegistrations.cpp
org.mainuan.welcome.policy   tst_mainuan                 tst_layouts
tst_appinstallation
validation/                 validation-2.3.1/
validation-wallpapers-2.3.1/ validation-layouts-2.3.1/
validation-layout-fixes-2.3.1.1/
validation-classic-tray-2.3.1.2/
validation-classic-spacing-2.3.1.3/
validation-app-install-2.3.1.4/
validation-installer-ui-2.3.1.5/
```

Isso inclui objetos, moc/rcc, executáveis, caches, relatórios e logs de execuções,
staging de pacotes extraídos, cópias temporárias de fontes e pacotes, arquivos
de comparação de geometria e o download de pesquisa do FreeOffice de 160 MiB.
O pequeno harness de inspeção dos recursos foi conservado nos registros novos
para conferir o executável final. O conhecimento permanente está nos documentos
de arquitetura e nos relatórios preservados; esses produtos não eram inputs do
CMake, do empacotamento ou dos testes.

Uma compilação limpa recria intermediários em `build/`. Os intermediários Debian
desta nova compilação são limpos por `debian/rules clean`; o `.deb` final fica
fora do repositório e os registros locais novos ficam em `build/release-validation/`.
O `.gitignore` cobre esses locais e padrões específicos, sem ignorar fontes,
testes, documentação, recursos ou os arquivos-fonte de `debian/`.

## C — documentação histórica, preservada e consolidada

Não foi excluído relatório Markdown. Os documentos `00` a `20` e
`appearance-and-installer-*.md` receberam uma identificação de histórico
experimental, preservando o conteúdo e os links. Instruções antigas de Discover,
destaque hexadecimal, oito layouts ou números 2.x não representam a versão
oficial. A referência atual está em [arquitetura e manutenção](architecture-and-maintenance.md),
[publicação](releasing.md) e no [índice](README.md).

Foram conservadas especialmente as explicações de Polkit, proteção do FreeOffice,
backup/rollback de layouts, segurança dos avatares, mapeamentos de recursos e
limitações gráficas. Logs de uma execução anterior não são confundidos com essas
explicações permanentes. Caminhos de capturas mencionados no histórico não foram
restaurados nem convertidos em dependências de build.

## D — obsoletos ou incertos, mantidos por precaução

- `rootfs/usr/share/welcome/index.html` e `main.js`: protótipo Electron sem uso no
  produto nativo; preservados porque fazem parte dos originais de `rootfs`.
  Não entram no CMake, no executável ou no pacote Debian.
- Fixtures de Discover: continuam referenciadas pelos testes da API existente;
  não são duplicatas descartáveis, mesmo que os três cards agora usem instalação interna.
- Todos os fontes novos não rastreados: são necessários ao build e devem entrar
  no commit revisado pelo mantenedor, não ser eliminados como resíduos.

Não se procurou nem removeu conteúdo de diretórios externos. A pasta pessoal
de referências não foi alterada e nenhum pacote anterior em `/home/daigo/Github/`
foi apagado. Arquivos ausentes não foram presumidos como exclusões acidentais.

## Preparação da versão e integração contínua

A versão técnica e pública é 1.0, sem epoch, com histórico experimental mantido.
Homepage, Vcs-Git, Vcs-Browser, AppStream, URL Polkit, Sobre e documentação apontam
para `mainuan/welcome`; autoria, licenças e Maintainer foram preservados.
Foi confirmada com dpkg a ordem `2.3.1.5 > 1.0` e `2.2.0 > 1.0`.
O downgrade e as consequências para APT estão documentados, sem migração automática.

O workflow tem jobs separados `build` e `release`; somente o segundo recebe
permissão de escrita, somente para push de tag no repositório oficial e após
aprovação de todos os gates. PRs não recebem permissão de publicação.
Os scripts de validação não executam conteúdo do pacote: conferem versão,
arquitetura, dependências, caminhos, propriedade e permissões do tar Debian.
A publicação exige tag já existente, mesma versão e mesmo commit; nunca sobrescreve assets.

A primeira execução remota depende de um futuro push deliberado do mantenedor.
A validação local de YAML/actionlint não é apresentada como aprovação remota.

## Resultados da validação local

- CMake Release, com modo de desenvolvimento desativado: compilação limpa aprovada.
- CTest: seis suítes aprovadas, sem falhas nem testes ignorados, incluindo QtTest,
  QML e startup offscreen. O gate confirmou os seis nomes obrigatórios no JUnit.
- Gates de publicação: seis testes Python aprovados, incluindo rejeição de tag,
  metadados divergentes, testes ignorados e renomeação artificial de pacote.
- YAML/GitHub Actions: actionlint 1.7.12 aprovado com o label declarado na configuração.
- `dpkg-checkbuilddeps` e simulação das dependências: aprovados, sem instalar pacotes.
- `dpkg-buildpackage -us -uc -b`: aprovado, repetindo as seis suítes em Release.
- `dpkg-deb`: pacote `mainuan-welcome`, versão interna `1.0`, arquitetura `amd64`;
  dependências, arquivos obrigatórios, propriedade e permissões aprovados.
- Lintian: nenhum apontamento. O empacotador emitiu o aviso esperado de versão
  1.0 inferior a 2.3.1.5 e a observação preexistente de ligação QtQuick; nenhum
  desses avisos foi ocultado ou contornado com epoch/alteração funcional.
- Recursos do executável extraído: scripts de layout, ícones personalizados,
  perfil FreeOffice e modal idênticos aos fontes; helper e perfil instalados conferidos.
- 128 arquivos funcionais protegidos permaneceram idênticos por SHA-256, incluindo
  todos os C++/headers, scripts de layout, instaladores, testes e recursos não metadados.
- Não foi encontrada referência ao diretório pessoal do desenvolvedor no executável;
  o pacote não contém arquivos pessoais, instaladores externos ou fontes/intermediários.
- `git diff --check`: sem erros. Nenhuma mudança foi adicionada ao índice ou commitada.

O pacote principal foi gerado em
`/home/daigo/Github/mainuan-welcome_1.0_amd64.deb`. A cópia de distribuição e o
manifesto estão em `dist/`, ignorado pelo Git. SHA-256:

```text
3f1a705f99817f5cf209f389a64c87f90f0cd368ae1ee7031713e8b7581fbd7e  mainuan-welcome_1.0_amd64.deb
```

Os intermediários novos de CMake/Qt e Debian também foram removidos após a
validação. Permanecem somente os registros novos em `build/release-validation/`
e a distribuição em `dist/`, ambos ignorados. O estado Git completo está no
registro local `build/release-validation/git-status.txt`; ele inclui as alterações
anteriores, ainda não publicadas, além da preparação desta etapa.

Nenhum pacote foi instalado. Não foram executados commit, push, criação/envio de
tag ou publicação. A instalação real de 1.0, o downgrade voluntário e a primeira
execução do Actions continuam sob decisão do mantenedor, conforme [publicação](releasing.md).
