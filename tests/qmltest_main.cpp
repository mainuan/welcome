// QML test runner with the same localization setup as the application:
// KI18n's i18n()/i18nc()/i18np() are available to the components under test
// and resolve against the mainuan-welcome catalog. Replaces qmltestrunner,
// which knows nothing about KI18n. Accepts the same -input/-o arguments.
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest>

#include <clocale>

class Setup : public QObject
{
    Q_OBJECT

public slots:
    void applicationAvailable()
    {
        KLocalizedString::setApplicationDomain(TRANSLATION_DOMAIN);
        const QString localeDir = qEnvironmentVariable("MAINUAN_LOCALE_DIR");
        if (!localeDir.isEmpty()) {
            KLocalizedString::addDomainLocaleDir(TRANSLATION_DOMAIN, localeDir);
        }
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        KLocalization::setupLocalizedContext(engine);
        // Language the i18n test expects ("pt_BR" or "en"); empty elsewhere.
        engine->rootContext()->setContextProperty(QStringLiteral("expectedLanguage"),
                                                  qEnvironmentVariable("MAINUAN_EXPECT_LANGUAGE"));
        // gettext ignores LANGUAGE under the C locale; the i18n test checks this.
        const char *messages = std::setlocale(LC_MESSAGES, nullptr);
        engine->rootContext()->setContextProperty(QStringLiteral("messagesLocale"),
                                                  QString::fromLatin1(messages != nullptr ? messages : "C"));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(mainuan_qml, Setup)

#include "qmltest_main.moc"
