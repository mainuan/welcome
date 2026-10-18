# Esquemas KDE, ícones Kora e avatar — 2.3.1

> Registro histórico do desenvolvimento experimental. Os números 2.x, resultados de execução e caminhos de capturas descrevem aquele momento. A primeira versão oficial é **1.0**; consulte a [documentação atual](README.md) antes de seguir instruções antigas.

O Welcome 2.3.0 aplicava apenas um destaque hexadecimal e gravava `General/AccentColor`.
Isso tingia o esquema atual, não aplicava o arquivo Dream completo e não preservava a
identidade da cor quando um tema global substituía as configurações do KDE.

A versão 2.3.1 centraliza os recursos em `AccentProfile`. O hexadecimal serve apenas
para desenhar o botão. A seleção é reconhecida por `General/ColorScheme` em
`kdeglobals`, inclusive após reiniciar; `Icons/Theme` é lido separadamente. Um
esquema externo não seleciona arbitrariamente um dos seis botões. A abertura do
Welcome não sincroniza nem modifica a aparência ou o avatar.

| ID | Nome | Esquema claro | Esquema escuro | Ícones | Wallpaper |
|---|---|---|---|---|---|
| brown | Marrom | DreamBrownLightColor | DreamBrownDarkColor | kora-brown | 05marrom.png |
| cyan | Ciano | DreamCyanLightColor | DreamCyanDarkColor | kora-cyan | 01ciano.png |
| magenta | Magenta | DreamMagentaLightColor | DreamMagentaDarkColor | kora-magenta | 03magenta.png |
| lime | Lima | DreamLimeLightColor | DreamLimeDarkColor | kora-lima | 06lima.png |
| gray | Cinza | DreamGrayLightColor | DreamGrayDarkColor | kora-grey | 02cinza.png |
| violet | Violeta | DreamVioletLightColor | DreamVioletDarkColor | kora-violet | 04violet.png |

Os esquemas são procurados como `color-schemes/<identificador>.colors` nos diretórios
XDG, normalmente `/usr/share/color-schemes/`. O Welcome valida o arquivo e consulta
assincronamente `plasma-apply-colorscheme --list-schemes` antes de aplicar o ID sem
extensão. Não cria esquemas nem utiliza `--accent-color` como alternativa.

## Sequência de aplicação

Ao selecionar uma cor: esquema KDE → ícones Kora → wallpaper → avatar → leitura
final da configuração. Falhas de cada recurso são informadas separadamente; uma
falha de wallpaper ou avatar não transforma o resultado em sucesso total.

Ao trocar Dream, Tahoe ou Breeze, ou sua variante, o serviço captura a identidade
da cor antes de iniciar `plasma-apply-lookandfeel`, aguarda o término, confirma o
estilo e a variante e só então reaplica o esquema e seus recursos. Novas operações
de aparência são recusadas durante essa sequência, e o fechamento normal do
Welcome também aguarda a operação. Para preservar uma escolha
conhecida, a ausência do esquema ou do Kora necessário impede a troca do tema
global antes de ele modificar a aparência. Na seleção direta de uma cor, um Kora
ausente ou com `index.theme` inválido mantém os ícones anteriores e gera erro parcial.

A seleção explícita remove o destaque personalizado local. Se a configuração XDG
herdada ainda fornecer um destaque, utiliza a configuração transparente nativa do
KDE, `0,0,0,0`, para usar as cores do esquema, sem modificar os arquivos de defaults.
Preferências externas não relacionadas são preservadas. As chaves alteradas são
restauradas se a aplicação falhar. Como o CLI pula esquemas já selecionados, o
seletor é temporariamente esvaziado para permitir uma recarga completa. O código
oficial do [CLI](https://github.com/KDE/plasma-workspace/blob/Plasma/6.6/kcms/colors/plasma-apply-colorscheme.cpp)
e do [aplicador KDE](https://github.com/KDE/plasma-workspace/blob/Plasma/6.6/kcms/colors/colorsapplicator.cpp)
documenta esses comportamentos.

Um estilo externo com um esquema Dream conhecido pode alternar a variante do
esquema sem trocar o tema global. Para um esquema externo não reconhecido não há
uma variante correspondente confiável: é preciso escolher uma cor Mainuan antes
de alternar claro/escuro fora dos três estilos conhecidos.

## Avatar e compatibilidade

A origem é `icons/<tema Kora>/status/scalable/.face`. O destino é `.face` no diretório
pessoal obtido pelo Qt. Os arquivos Kora examinados são SVG; os bytes originais são
copiados sem conversão. `qt6-svg-plugins` é necessário para validar esses SVGs com
`QImageReader`, inclusive nos testes de compilação.

`UserAvatar` valida a imagem e limita seu tamanho. Abre arquivos sem seguir links
simbólicos e ancora todas as escritas em um descritor do diretório pessoal do
usuário atual. Não opera como root, em um diretório pertencente a outro usuário,
através de um link de diretório pessoal, ou em diretórios graváveis por grupo/outros.

Antes da primeira substituição de um `.face` existente, cria
`~/.face.mainuan-welcome-backup`, privado (0600), sem sobrescrever um backup anterior.
O backup e o novo avatar são publicados apenas após a gravação completa e `fsync`.
A substituição usa rename atômico. Mantém leitura apropriada do avatar, sem conceder
escrita a grupo/outros; um avatar anterior 0600 permanece 0600. Não segue `.face`
ou backup que sejam links, nem sobrescreve destinos não regulares. Em caso de erro,
a mensagem informa a etapa que falhou.

A leitura, validação da imagem, cópia e sincronização em disco são executadas
em uma tarefa separada da interface, sem acrescentar bibliotecas de concorrência.
O serviço só libera uma nova mudança após receber o resultado dessa tarefa.

Um `.face.icon` existente é preservado, incluindo o link para `.face`. Se não existir,
é criado um link relativo `.face.icon -> .face`, sem sobrescrever entradas concorrentes.
Arquivos `.face.icon` independentes e avatares personalizados do AccountsService
não são substituídos. Não há chamada `SetIconFile`, escrita em
`/var/lib/AccountsService/`, elevação de privilégios ou conversão SVG para PNG.
A interface D-Bus instalada do AccountsService especifica PNG para `SetIconFile`.

No sistema examinado, `.face.icon` já aponta para `.face`, e a propriedade `IconFile`
do AccountsService também aponta para `.face`. O SDDM pode consultar `.face.icon`
quando `EnableAvatars` está habilitado, mas a apresentação depende do tema, das
permissões, do suporte ao formato e dos caches dos componentes. A atualização
imediata no Plasma ou na tela de login não é garantida; requer teste gráfico real.
Veja o [manual do SDDM](https://github.com/sddm/sddm/blob/develop/data/man/sddm.conf.rst.in).

## Recursos ausentes no sistema examinado

Foram encontrados e reconhecidos pelo CLI onze dos doze IDs obrigatórios.
**`DreamLimeDarkColor.colors` não está instalado.** Existe `Lima-Dark.colors`, que não
é utilizado como substituto. A aplicação de Lima no escuro (ou a troca para escuro
com Lima selecionado) informa o recurso ausente e preserva a configuração anterior.
Nenhum arquivo de esquema ou Kora foi criado, renomeado ou modificado no sistema.
Os testes geram os doze esquemas e os seis avatares somente em diretórios temporários.

Os esquemas já instalados também provocaram avisos do CLI sobre algumas cores com
comentários na mesma linha. A correção desses recursos da distribuição está fora
do pacote Welcome e nenhum deles foi alterado.

## Validação e pacote

A suíte QtTest usa ferramentas simuladas, configurações XDG temporárias e um home
injetado apenas pelo construtor C++. Não modifica avatar, tema ou configurações
pessoais. Inclui os doze esquemas, as seis correspondências Kora, persistência,
ordem assíncrona, overrides herdados, falhas e ausência de recursos, validação de
SVG/PNG, backup único, permissões, links inesperados e falhas de gravação.

O pacote 2.3.1 contém somente o Welcome, seus recursos anteriores e a lógica de
sincronização. Não inclui esquemas, temas Kora, arquivos `.face`, backups ou dados
do diretório pessoal. Os recursos de FreeOffice, Vivaldi e Zen da versão 2.3.0 são
preservados. A instalação e a validação gráfica permanecem manuais.
