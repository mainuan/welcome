// Proves that the real models and services are translated through the
// mainuan-welcome catalog: Portuguese with LANGUAGE=pt_BR, the English source
// text with LANGUAGE=en (CTest runs this binary once per language).
#include "CatalogLayoutDirection.h"
#include "models/LayoutModel.h"
#include "services/LayoutService.h"
#include "services/SystemReportService.h"

#include <KLocalizedString>

#include <QGuiApplication>
#include <QtTest>

#include <clocale>
#include <cstring>

class I18nTest : public QObject
{
    Q_OBJECT

private:
    bool portuguese() const { return qEnvironmentVariable("MAINUAN_EXPECT_LANGUAGE") == QStringLiteral("pt_BR"); }
    QString pick(const char *pt, const char *en) const { return QString::fromUtf8(portuguese() ? pt : en); }

private slots:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain(TRANSLATION_DOMAIN);
        const QString localeDir = qEnvironmentVariable("MAINUAN_LOCALE_DIR");
        QVERIFY2(!localeDir.isEmpty(), "MAINUAN_LOCALE_DIR must point to the compiled catalogs");
        QVERIFY(QFileInfo::exists(localeDir + QStringLiteral("/pt_BR/LC_MESSAGES/" TRANSLATION_DOMAIN ".mo")));
        KLocalizedString::addDomainLocaleDir(TRANSLATION_DOMAIN, localeDir);
        const QString expected = qEnvironmentVariable("MAINUAN_EXPECT_LANGUAGE");
        QVERIFY2(expected == QStringLiteral("pt_BR") || expected == QStringLiteral("en"),
                 "MAINUAN_EXPECT_LANGUAGE must be pt_BR or en");
        // gettext ignores LANGUAGE under the C/POSIX locale: fail loudly instead of
        // passing on untranslated text.
        const char *messages = std::setlocale(LC_MESSAGES, nullptr);
        QVERIFY2(messages != nullptr && std::strcmp(messages, "C") != 0 && std::strcmp(messages, "POSIX") != 0
                     && std::strncmp(messages, "C.", 2) != 0,
                 "A non-C locale is required (LC_ALL=pt_BR.UTF-8; install locales-all)");
    }

    void layoutNamesAndDescriptions()
    {
        QCOMPARE(LayoutService::displayName(QStringLiteral("plasma-default")), pick("Clássico — 5 barras", "Classic — 5 panels"));
        LayoutModel model;
        QCOMPARE(model.data(model.index(0), LayoutModel::LayoutIdRole).toString(), QStringLiteral("plasma-default"));
        QCOMPARE(model.data(model.index(0), LayoutModel::DescriptionRole).toString(),
                 pick("O visual do Mainuan: ilhas flutuantes na base com menu, tarefas, bandeja e relógio.",
                      "The Mainuan look: floating islands at the bottom with menu, tasks, system tray and clock."));
    }

    void pluralsAndContexts()
    {
        QCOMPARE(SystemReportService::humanDuration(2 * 3600 + 18 * 60), pick("2 horas e 18 minutos", "2 hours and 18 minutes"));
        QCOMPARE(SystemReportService::humanDuration(3600 + 60), pick("1 hora e 1 minuto", "1 hour and 1 minute"));
        QCOMPARE(SystemReportService::humanDuration(10), pick("menos de um minuto", "less than a minute"));
    }

    void applicationDisplayName()
    {
        // The same call main() makes for the window title.
        QCOMPARE(i18nc("@title application display name", "Mainuan — Welcome"), pick("Mainuan — Boas-vindas", "Mainuan — Welcome"));
    }

    void rightToLeftCatalogMirrorsLayout()
    {
        auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
        QVERIFY(app != nullptr);
        // No qtbase_ar translator in this process (LANGUAGE is pt_BR or en):
        // Qt by itself keeps the layout left to right.
        QVERIFY(QCoreApplication::translate("QGuiApplication", "QT_LAYOUT_DIRECTION") != QStringLiteral("RTL"));
        QCOMPARE(QGuiApplication::layoutDirection(), Qt::LeftToRight);

        // The application's own Arabic catalog is used: mirror (same call as main()).
        KLocalizedString::setLanguages({QStringLiteral("ar"), QStringLiteral("en")});
        QVERIFY(KLocalizedString::isApplicationTranslatedInto(QStringLiteral("ar")));
        QCOMPARE(CatalogLayoutDirection::effectiveLanguage(), QStringLiteral("ar"));
        CatalogLayoutDirection::apply(*app);
        QCOMPARE(QGuiApplication::layoutDirection(), Qt::RightToLeft);

        // Urdu is written right to left, but there is no mainuan-welcome catalog
        // for it: the interface falls back to English and stays left to right.
        app->setLayoutDirection(Qt::LeftToRight);
        KLocalizedString::setLanguages({QStringLiteral("ur"), QStringLiteral("en")});
        QCOMPARE(QLocale(QStringLiteral("ur")).textDirection(), Qt::RightToLeft);
        QVERIFY(!KLocalizedString::isApplicationTranslatedInto(QStringLiteral("ur")));
        QCOMPARE(CatalogLayoutDirection::effectiveLanguage(), QString());
        CatalogLayoutDirection::apply(*app);
        QCOMPARE(QGuiApplication::layoutDirection(), Qt::LeftToRight);

        KLocalizedString::clearLanguages();
        app->setLayoutDirection(Qt::LayoutDirectionAuto);
    }
};

QTEST_MAIN(I18nTest)
#include "tst_i18n.moc"
