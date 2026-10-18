# Publicação e migração — Mainuan Welcome

## Política de versão

A primeira versão oficial é **1.0**, tag prevista **v1.0**, pacote
`mainuan-welcome_1.0_amd64.deb`. A série 2.x foi experimental; seu histórico foi
mantido em `debian/changelog` e nos relatórios identificados como históricos.
CMake, a primeira entrada do changelog e a primeira release AppStream precisam
ter exatamente a mesma versão. O módulo QML tem versionamento de API próprio.
Não há geração automática de números, tags, epochs ou commits.

### Consequência Debian

`dpkg --compare-versions 2.3.1.5 gt 1.0` e a comparação com `2.2.0` retornam zero.
Não se trata de um upgrade Debian. O usuário precisa solicitar explicitamente
o arquivo local 1.0, conferi-lo e executar:

```bash
sudo apt install --allow-downgrades ./mainuan-welcome_1.0_amd64.deb
```

Não se acrescentou epoch para contornar a ordem. Um APT que continue oferecendo
2.x poderá preferir novamente essa versão. Usuários que permanecerem em 2.x não
receberão automaticamente 1.x inferiores; os canais que distribuíram versões
experimentais precisam de um plano explícito do mantenedor. Não são criadas regras
de pinning nem removidas fontes APT pelo Welcome ou por esta preparação.

Na consulta pública de preparação em 17/10/2026, `mainuan/welcome` não tinha tags
nem Releases; o changelog da branch pública ainda declarava **2.2.0**. Isso
comprova a publicação do código com esse número, mas não a distribuição de um
`.deb`. Não é possível excluir distribuições via ISO, repositório APT ou outros
canais apenas examinando o GitHub. O mantenedor deve conferir esses canais antes
do anúncio. A máquina local tinha 2.3.1 registrado somente no status do dpkg,
sem candidato adicional identificado em suas fontes APT configuradas.

## O que cada ação faz

| Ação | Resultado |
|---|---|
| Push de código para `main` | CI, testes e artefato temporário; nenhuma Release |
| Pull Request para `main` | Mesmas verificações e artefato, sem publicação |
| `workflow_dispatch` | Compilação manual, sem publicação, mesmo se selecionada uma tag |
| Push de tag `v1.0` | Testa e gera pacote 1.0; depois publica a Release correspondente |

O workflow usa `ubuntu-26.04`, dependências de `debian/control` via
`apt-get build-dep ./` e `dpkg-checkbuilddeps`. Inclui compilação CMake,
QtTest/CTest/QML, rejeição de testes ignorados, empacotamento com
`dpkg-buildpackage -us -uc -b`, conferência do conteúdo/versão/arquitetura,
lintian e SHA256SUMS. Compilar e testar roda como usuário normal.

As Actions oficiais estão fixadas por SHA de commit. Para atualizar, confirme a
release e seu commit nos repositórios oficiais antes de alterar as referências.
`actionlint` 1.7.12 ainda não reconhece o label hospedado `ubuntu-26.04` em seu
catálogo interno; `.github/actionlint.yaml` acrescenta esse label validado na
[imagem oficial](https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2604-Readme.md).
Essa configuração do linter não muda os jobs para runners self-hosted.

O job `build` tem somente `contents: read` e não persiste credenciais Git. O job
`release`, dependente do sucesso do build e restrito a push de tag no repositório
oficial, tem `contents: write`, sem checkout nem execução de scripts do projeto.
Baixa o artefato pelo ID da mesma execução, verifica checksum, versão e commit
atual da tag e chama `gh release create --verify-tag`. Não usa `--clobber`, não
cria tags e falha se a Release já existir. Uma publicação parcial exige revisão
manual; reexecutar não substitui assets silenciosamente.

Os artefatos comuns ficam disponíveis por 30 dias na execução em Actions.
Releases contêm o `.deb`, SHA256SUMS e uma descrição. Nenhum desses serviços é um
repositório APT; assinatura, índices e distribuição APT são um processo separado.

## Revisar e enviar o código

Estes comandos são sugestões para o mantenedor. **Não foram executados nesta preparação.**
Primeiro confirme branch, remoto e cada mudança local, incluindo os arquivos novos
das funcionalidades já validadas. Não descarte arquivos movidos pelo usuário.

```bash
git status --short
git diff --stat
git diff --check
git diff
git remote -v
git branch --show-current
```

O destino deve ser `https://github.com/mainuan/welcome`. Se o remoto existente
estiver errado, altere-o conscientemente antes do push; esta tarefa não o altera.
Após revisar o inventário e as regras de ignore:

```bash
git add -p
git add .github .gitignore LICENSE CMakeLists.txt README.md debian doc docs qml src rootfs tests
git diff --cached --check
git diff --cached --stat
git diff --cached
git commit -m "Prepare Mainuan Welcome 1.0 and Debian CI"
git push origin main
```

Revise especialmente arquivos não rastreados antes de `git add`. `build/`, `dist/`
e os intermediários Debian são ignorados; nenhuma limpeza automática de arquivos
de trabalho é executada. O push em `main` gera somente o artefato de teste.

## Criar a publicação oficial depois da validação

Espere a primeira execução remota aprovada, baixe e confira o artefato, e valide
o pacote na sessão Mainuan. Releia a política de downgrade e confira os canais
externos que possam conter 2.x. Depois, somente quando decidir publicar:

```bash
git switch main
git pull --ff-only origin main
git status --short
python3 .github/scripts/check_release.py metadata --tag v1.0
git tag -a v1.0 -m "Mainuan Welcome 1.0"
git push origin v1.0
```

Não crie a tag com mudanças pendentes nem mova uma tag já publicada. O push da tag
inicia outra compilação; a publicação ocorre apenas após todos os gates. Para
versões futuras, atualize os três metadados e use a tag correspondente, como
`v1.1`. A primeira execução real do Actions só pode ser avaliada após o usuário
enviar estes arquivos; validação local de YAML não equivale a execução remota.
