# Consolidação da versão 1.0.1: interface, janela e decoração

## Preservação da correção inicial

A leitura explícita de `kdedefaults` e suas notificações KConfig, descritas no
[diagnóstico anterior](21-initial-theme-detection.md), permanecem intactas.
Os 108 casos de cores continuam obrigatórios, com verificações adicionais de
decoração. Os instaladores, layouts, recursos Kora e mecanismos de avatar e
backup não foram substituídos.

## Interface e janela

Os perfis Antivírus, Firewall e Codecs ocultam somente os badges de ausência ou
instalação parcial quando `canInstall` é verdadeiro. A detecção e os textos
internos do instalador não mudam. O próprio layout exclui o indicador invisível;
verificação, instalação, conclusão, indisponibilidade e erros úteis continuam
visíveis. “Instalado” indica presença dos componentes, não uma verificação de
antivírus em execução. O estado adicional do UFW é identificado como “início”:
`ufw.conf` descreve a preferência de inicialização, não uma prova de proteção
ativa neste instante.

O tamanho normal inicial é 870 × 600, com mínimo 760 × 520 e sem tamanho máximo
imposto. A referência é a geometria do
[TigerOS Welcome](https://github.com/Tiger-OperatingSystem/welcome-qt/blob/main/Welcome.ui);
a restrição máxima do original não foi copiada. Antes de mostrar a janela, a
geometria é limitada à área útil do monitor em pixels lógicos, com margem para
a moldura e mínimo reduzido somente quando necessário. Coordenadas negativas de
monitores adicionais são respeitadas. A posição inicial é uma sugestão; no
Wayland o compositor decide o posicionamento final.

AppsPage e Tutoriais calculam colunas e altura conforme a largura disponível.
SettingRow passa os controles para baixo quando precisam de espaço. O modal de
instalação limita sua altura à janela e permite rolagem, inclusive dos detalhes
opcionais e botões, preservando confirmação e proteção contra interrupções.

## Diagnóstico da decoração no sistema Mainuan

A inspeção foi somente para leitura. Os defaults instalados de Dream indicavam:

- `library=org.kde.kwin.aurorae`;
- `theme=Cyan-Light` ou `theme=Cyan-Dark`.

Esses diretórios Cyan não estavam em `/usr/share/aurorae/themes/` nem no diretório
de temas do usuário. As variantes instaladas eram **Lavanda-Light** e
**Lavanda-Dark**, com esses valores em `X-KDE-PluginInfo-Name`, mas nomes visíveis
**Mainuan-Light** e **Mainuan-Dark**. A preferência local de KWin já usava
`org.kde.kwin.aurorae.v2` e `__aurorae__svg__Lavanda-Dark`, enquanto o default
herdado ainda apontava para Cyan-Dark. Aplicar o tema global podia, portanto,
restaurar uma referência inexistente. Este é um erro persistente de configuração;
a inspeção de arquivos não comprova a renderização atual do compositor.

A nomenclatura foi conferida com o
[Lavanda original](https://github.com/vinceliuice/Lavanda-kde) e os recursos locais.
O prefixo SVG e a leitura do identificador são definidos pelo
[Aurorae v2](https://invent.kde.org/plasma/aurorae/-/blob/master/v2/decoration.cpp).
A matriz centralizada em VisualStyle usa:

| Estilo | Biblioteca | Claro | Escuro |
|---|---|---|---|
| Dream | `org.kde.kwin.aurorae.v2` | `__aurorae__svg__Lavanda-Light` | `__aurorae__svg__Lavanda-Dark` |
| Tahoe | `org.kde.kwin.aurorae` | `__aurorae__svg__MacTahoe-Light` | `__aurorae__svg__MacTahoe-Dark` |
| Breeze | `org.kde.breeze` | `Breeze` | `Breeze` |

Tahoe conserva a biblioteca declarada por seus temas globais e seus recursos
MacTahoe. Lavanda não é imposto a Tahoe ou Breeze. Nenhum recurso do sistema
foi copiado ou alterado; o Welcome referencia os temas já instalados.

## Ordem, validação e falhas

Antes de aplicar o tema global, o serviço valida o plugin de decoração e, para
Aurorae, diretório, ID, metadata.desktop, arquivo rc e SVG/SVGZ legíveis. Também
verifica comunicação com o KWin sem ativar serviços automaticamente. Depois da
conclusão e confirmação do tema global, reaplica a cor e os ajustes já previstos,
finaliza somente os campos `library` e `theme` de `kwinrc` com KConfig e relê o
resultado efetivo pela cascata.

A aplicação usa o
[D-Bus do KWin](https://invent.kde.org/plasma/kwin/-/blob/master/src/org.kde.KWin.xml):
`/KWin`, interface `org.kde.KWin`, método `reconfigure`. Ele é Q_NOREPLY;
não se espera uma resposta que o método não oferece. Uma consulta assíncrona
`supportInformation` verifica a comunicação após a solicitação, sem guardar seu
conteúdo. Não há shell, privilégios, temporizadores arbitrários ou reinício do
KWin. A leitura final de configuração e essa comunicação não são prova visual.

Uma decoração externa desconhecida é preservada na alternância Claro/Escuro,
inclusive se um tema global sobrescrever temporariamente seus seletores. Escolher
explicitamente outro estilo aplica a identidade desse estilo. Erros anteriores
à aplicação preservam a aparência; falhas posteriores são informadas como parciais,
e os seletores anteriores são restaurados tanto quanto possível, sem apagar outras
preferências de kwinrc. Não se anuncia conclusão completa quando a decoração ou
a comunicação falham.

## Testes e validação prática

As fixtures incluem recursos Aurorae e plugins simulados (nunca carregados), um
KWin D-Bus privado, sobrescrita de decoração pelo tema global, preferências locais
e herdadas, recursos/IDs ausentes e falha de comunicação após a solicitação.
Os 108 casos de cores são preservados. Testes QML cobrem os estados dos três
perfis, sete páginas, grades, rolagem, instalação/confirmacão, relatório,
restauração de layout, maximização e restauração da janela.

O runner QML executa a mesma cobertura nas escalas 100%, 125%, 150% e 200%, com
home, cache, configurações e runtime temporários, sem barramento Plasma real.
As dimensões incluem 870 × 600, 1024 × 768, 1280 × 720, 1366 × 768,
1920 × 1080, 760 × 520 e uma área reduzida 616 × 320. Esses testes verificam
geometria e acessibilidade; não simulam as sombras reais de cada decoração.

Ainda é obrigatório validar em Mainuan/Wayland: primeira alternância sem escolher
cor, troca entre os três estilos, cor selecionada nas Configurações do Sistema,
reabertura e logout/login; conferir Lavanda claro/escuro, MacTahoe e Breeze
visíveis, margens da janela, escala fracionária e colocação em múltiplos monitores.
Conferir também avisos de instalação e acesso aos botões após abrir detalhes.
A nova compilação mantém 1.0.1 e gera um checksum diferente da anterior, que deve
ser preservada. A tag v1.0 e a Release publicada permanecem intactas.
