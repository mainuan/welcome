# Internacionalização do Mainuan Welcome

Contrato comum: `docs/03-arquitetura-i18n-kde.md` no repositório `mainuan`.

| Item | Valor |
|---|---|
| API | KF6 **KI18n** (`i18n()`, `i18nc()`, `i18np()`; `kli18nc()` em tabelas estáticas) em C++ e QML |
| Domínio | `mainuan-welcome` (nome do pacote Debian; `TRANSLATION_DOMAIN` definido pelo CMake) |
| Idioma-fonte | inglês (msgid); a interface brasileira está em `po/pt_BR.po` |
| Layout | plano: `po/<lang>.po`, `po/mainuan-welcome.pot`, `po/LINGUAS`, `po/POTFILES.in` |
| Instalação | `/usr/share/locale/<lang>/LC_MESSAGES/mainuan-welcome.mo` |

## Como funciona

* `main.cpp` chama `KLocalizedString::setApplicationDomain()` antes do
  `QApplication` e `KLocalization::setupLocalizedContext(&engine)` para que o
  QML use `i18n()`. O idioma vem de `LANGUAGE`/`LC_*`/`LANG` (Plasma →
  *Região e idioma*); nada é forçado.
* O CMake compila cada catálogo de `po/LINGUAS` em
  `<build>/share/locale/<lang>/LC_MESSAGES/` e o instala. O executável procura
  também `<dir do binário>/../share/locale`, então `build/bin/mainuan-welcome`
  já sai traduzido; `MAINUAN_LOCALE_DIR=<dir>` força outro diretório (só
  desenvolvimento).
* `.desktop` (aplicativos e autostart), metainfo e polkit são templates
  (`*.desktop.in`, `*.metainfo.xml.in`, `*.policy.in`); o build gera as versões
  traduzidas com `msgfmt --desktop`/`--xml`. O polkit passa primeiro por
  `configure_file` (caminho do helper) e depois pelo `msgfmt`; em seguida
  `po/polkit-lang.cmake` troca `xml:lang="pt-BR"` por `pt_BR`, que é o que o
  polkit procura. As regras ITS ficam em `po/its/` (via `GETTEXTDATADIRS`).
  Notas de versão e nome do desenvolvedor no metainfo têm `translate="no"`.
* Os scripts `src/layouts/*.js` rodam dentro do plasmashell, sem o catálogo:
  devolvem códigos (`invalid-layout`, `missing-widget`…) e o `LayoutService`
  monta a frase traduzida.

## Tarefas

```bash
po/i18n.sh extract          # regenera o POT a partir de po/POTFILES.in
po/i18n.sh update           # msgmerge do POT em cada catálogo
po/i18n.sh validate         # msgfmt --check + placeholders (gate de CI)
po/i18n.sh check-pot        # falha se o POT commitado estiver desatualizado
po/i18n.sh coverage         # estatística por idioma
po/i18n.sh add es           # novo idioma (Plural-Forms verificado) + LINGUAS
```

Arquivo novo com texto traduzível: acrescente-o a `po/POTFILES.in`, rode
`extract` e `update`. Não use `qsTr()`; use `i18n()`/`i18nc()` com frases
completas e `%1`, nunca concatenação.

## Testes

`ctest` roda:

* `i18n_pt_BR`/`i18n_en` (`tests/tst_i18n.cpp`) e `i18n_qml_pt_BR`/`i18n_qml_en`
  (`tests/qml/tst_I18n.qml` com o `AppCard` real): com `LANGUAGE=pt_BR` sai o
  português, com `LANGUAGE=en` o inglês. Rodam com `LC_ALL=pt_BR.UTF-8`, porque
  a glibc ignora `LANGUAGE` sob o locale `C`; por isso `locales-all` está nos
  Build-Depends (`<!nocheck>`).
* `i18n_metadata`: confere `.desktop`, autostart, metainfo e polkit gerados.
* `i18n_catalogs`: `check-pot` + `validate`.
* As suítes funcionais (`tst_*`, `layouts_qml`, `applications_qml`) comparam o
  texto inglês e rodam com `LANGUAGE=en`. Os testes QML usam
  `tests/qmltest_main.cpp` no lugar do `qmltestrunner`, para ter `i18n()`.

Para ver a interface em outro idioma a partir do build:
`LANGUAGE=pt_BR build/bin/mainuan-welcome` (ou `LANGUAGE=en`).

## Particularidades

* O relatório do sistema (copiar/salvar) é traduzido: é lido pelo usuário. O
  título `Mainuan System Report` fica fixo em inglês, como antes; datas usam
  `QLocale` (formato curto do locale de formatos).
* Não são traduzidos: logs (`qCWarning`…), saída de ferramentas, nomes de
  marca/estilo (Dream, Tahoe, Breeze, Lomiri, Kora), IDs, caminhos, URLs,
  o parser de progresso do Flatpak/APT (que lê a saída dessas ferramentas).
* `AppCard.qml` compara `installState` com `i18n("Available")` e
  `InstallDialog.qml` compara o rótulo com `i18n("Open")`: o C++ precisa usar
  exatamente esses msgid (sem contexto em `"Available"`).
