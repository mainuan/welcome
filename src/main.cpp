#include "WindowGeometry.h"
#include "models/ApplicationModel.h"
#include "models/LayoutModel.h"
#include "models/VideoModel.h"
#include "services/InstallService.h"
#include "services/LayoutService.h"
#include "services/PackageService.h"
#include "services/SingleInstance.h"
#include "services/StartupPreference.h"
#include "services/SystemReportService.h"
#include "services/SystemService.h"

#include <KLocalizedQmlContext>
#include <KLocalizedString>
// After KLocalizedQmlContext: KLocalizedString defines i18nd() as a macro.
#include "CatalogLayoutDirection.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QScreen>
#include <QQuickWindow>
#include <QDebug>
#include <QTimer>
#include <QUrl>
#include <cstdio>

int main(int argc, char *argv[])
{
    // The catalog domain is the Debian package name; set before any i18n() call.
    KLocalizedString::setApplicationDomain(TRANSLATION_DOMAIN);

    // QApplication rather than QGuiApplication: Plasma's platform integration only
    // offers its native file dialogs (used by "Save Report") to widget apps.
    QApplication app(argc, argv);

    // Installed: /usr/bin/../share/locale is the system catalog directory. From a
    // build tree, <build>/bin/../share/locale holds the catalogs compiled by CMake.
    // MAINUAN_LOCALE_DIR overrides both (development only).
    const QString localeDir = qEnvironmentVariableIsSet("MAINUAN_LOCALE_DIR")
        ? qEnvironmentVariable("MAINUAN_LOCALE_DIR")
        : QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../share/locale"));
    if (QFileInfo(localeDir).isDir()) {
        KLocalizedString::addDomainLocaleDir(TRANSLATION_DOMAIN, localeDir);
    }
    // Mirror the interface for a right-to-left catalog even without qtbase_<lang>.
    CatalogLayoutDirection::apply(app);
    app.setApplicationName(QStringLiteral("Mainuan Welcome"));
    app.setApplicationDisplayName(i18nc("@title application display name", "Mainuan — Welcome"));
    app.setOrganizationName(QStringLiteral("Mainuan"));
    app.setOrganizationDomain(QStringLiteral("mainuan.org"));
    app.setApplicationVersion(QStringLiteral(MAINUAN_VERSION));
    app.setDesktopFileName(QStringLiteral("org.mainuan.Welcome"));

    SingleInstance singleInstance;
    if (!singleInstance.tryAcquire()) {
        return 0;
    }

#ifdef MAINUAN_DEVELOPER_MODE
    // Developer builds only: point the installer at simulated backends.
    const auto devOverride = [](const char *name, const QString &fallback) {
        const QString value = qEnvironmentVariable(name);
        return value.isEmpty() ? fallback : value;
    };
    InstallService::Backend backend = InstallService::Backend::system();
    backend.pkexec = devOverride("MAINUAN_DEV_PKEXEC", backend.pkexec);
    backend.helper = devOverride("MAINUAN_DEV_HELPER", backend.helper);
    backend.dpkgQuery = devOverride("MAINUAN_DEV_DPKG_QUERY", backend.dpkgQuery);
    backend.aptGet = devOverride("MAINUAN_DEV_APT_GET", backend.aptGet);
    SystemService systemService;
    PackageService packageService(nullptr, qEnvironmentVariable("MAINUAN_DEV_FLATPAK"));
    InstallService installService(&packageService, backend);
#else
    SystemService systemService;
    PackageService packageService;
    InstallService installService(&packageService);
#endif

    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &packageService,
                     [&installService](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            installService.refresh();
        }
    });

    const auto asset = [](const QString &fileName) {
        return QStringLiteral("qrc:/assets/") + fileName;
    };
    ApplicationModel officeModel(&packageService, {
        {QStringLiteral("libreoffice"), QStringLiteral("LibreOffice"),
         i18nc("@info application description", "Complete suite: Writer, Calc and Impress"), QStringLiteral("org.libreoffice.LibreOffice"),
         asset(QStringLiteral("org.libreoffice.LibreOffice.png")), QString()},
        {QStringLiteral("wps"), QStringLiteral("WPS Office"),
         i18nc("@info application description", "Interface familiar to Microsoft Office users"), QStringLiteral("com.wps.Office"),
         asset(QStringLiteral("com.wps.Office.png")), QString()},
        {QStringLiteral("freeoffice"), QStringLiteral("FreeOffice"),
         i18nc("@info application description", "Free office suite with word processor, spreadsheet and presentation editors."), QStringLiteral("softmaker-freeoffice-2024"),
         asset(QStringLiteral("freeOffice.png")), QString(), ApplicationAction::InternalDebian},
        {QStringLiteral("gdocs"), QStringLiteral("Google Docs"),
         i18nc("@info application description", "Online documents with collaboration"), QString(),
         asset(QStringLiteral("webapp.gdocs.png")), QStringLiteral("https://docs.google.com/"), ApplicationAction::WebPage},
        {QStringLiteral("office365"), QStringLiteral("Office 365 Online"),
         i18nc("@info application description", "Microsoft suite right in the browser"), QString(),
         asset(QStringLiteral("webapp.office365.png")), QStringLiteral("https://www.office.com/"), ApplicationAction::WebPage}
    });

    ApplicationModel browserModel(&packageService, {
        {QStringLiteral("chrome"), QStringLiteral("Google Chrome"),
         i18nc("@info application description", "Google's web browser"), QStringLiteral("com.google.Chrome"),
         asset(QStringLiteral("com.google.Chrome.png")), QString()},
        {QStringLiteral("edge"), QStringLiteral("Microsoft Edge"),
         i18nc("@info application description", "Compatibility with Microsoft applications"), QStringLiteral("com.microsoft.Edge"),
         asset(QStringLiteral("com.microsoft.Edge.png")), QString()},
        {QStringLiteral("brave"), QStringLiteral("Brave"),
         i18nc("@info application description", "Built-in ad blocking"), QStringLiteral("com.brave.Browser"),
         asset(QStringLiteral("com.brave.Browser.png")), QString()},
        {QStringLiteral("vivaldi"), QStringLiteral("Vivaldi"),
         i18nc("@info application description", "Browser with advanced customization, tab management and privacy features."),
         QStringLiteral("com.vivaldi.Vivaldi"), asset(QStringLiteral("vivaldi.png")), QString(), ApplicationAction::InternalFlatpak},
        {QStringLiteral("zen"), QStringLiteral("Zen Browser"),
         i18nc("@info application description", "Browser focused on privacy, productivity and vertical tab organization."),
         QStringLiteral("app.zen_browser.zen"), asset(QStringLiteral("zenBrowser.png")), QString(), ApplicationAction::InternalFlatpak},
        {QStringLiteral("librewolf"), QStringLiteral("LibreWolf"),
         i18nc("@info application description", "Privacy-focused browser"), QStringLiteral("io.gitlab.librewolf-community"),
         asset(QStringLiteral("io.gitlab.librewolf-community.png")), QString()},
        {QStringLiteral("opera"), QStringLiteral("Opera GX"),
         i18nc("@info application description", "Browser with resource controls"), QStringLiteral("com.opera.opera-gx"),
         asset(QStringLiteral("com.opera.opera-gx.png")), QString()},
        {QStringLiteral("tor"), QStringLiteral("Tor Browser"),
         i18nc("@info application description", "Browsing through the Tor network"), QStringLiteral("org.torproject.torbrowser-launcher"),
         asset(QStringLiteral("org.torproject.torbrowser-launcher.png")), QString()}
    });

    officeModel.setInstallService(&installService);
    browserModel.setInstallService(&installService);

    StartupPreference startupPreference;
    SystemReportService systemReport;
    LayoutService layoutService;
    layoutService.refresh();
    VideoModel videoModel;
    LayoutModel layoutModel;

    QQmlApplicationEngine engine;
    // i18n(), i18nc(), i18np()… in QML, resolved against the same domain.
    KLocalization::setupLocalizedContext(&engine);
    engine.rootContext()->setContextProperty(QStringLiteral("systemService"), &systemService);
    engine.rootContext()->setContextProperty(QStringLiteral("packageService"), &packageService);
    engine.rootContext()->setContextProperty(QStringLiteral("installService"), &installService);
    engine.rootContext()->setContextProperty(QStringLiteral("startupPreference"), &startupPreference);
    engine.rootContext()->setContextProperty(QStringLiteral("layoutService"), &layoutService);
    engine.rootContext()->setContextProperty(QStringLiteral("systemReport"), &systemReport);
    engine.rootContext()->setContextProperty(QStringLiteral("officeModel"), &officeModel);
    engine.rootContext()->setContextProperty(QStringLiteral("browserModel"), &browserModel);
    engine.rootContext()->setContextProperty(QStringLiteral("videoModel"), &videoModel);
    engine.rootContext()->setContextProperty(QStringLiteral("layoutModel"), &layoutModel);

    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
                         for (const QQmlError &warning : warnings) {
                             qWarning().noquote() << warning.toString();
                         }
                     });

    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qt/qml/Mainuan/Welcome/qml/Main.qml")));
    if (component.isError()) {
        for (const QQmlError &error : component.errors()) {
            std::fprintf(stderr, "%s\n", error.toString().toLocal8Bit().constData());
        }
        qWarning() << "Não foi possível carregar o módulo QML Mainuan.Welcome.";
        return 1;
    }
    QObject *rootObject = component.create(engine.rootContext());
    if (rootObject == nullptr) {
        for (const QQmlError &error : component.errors()) {
            std::fprintf(stderr, "%s\n", error.toString().toLocal8Bit().constData());
        }
        return 1;
    }
    rootObject->setParent(&engine);

    if (auto *window = qobject_cast<QQuickWindow *>(rootObject)) {
        QScreen *screen = window->screen();
        if (screen == nullptr) {
            screen = QGuiApplication::primaryScreen();
        }
        if (screen != nullptr) {
            const QRect initial = WelcomeWindow::initialGeometry(screen->availableGeometry());
            window->setMinimumSize(WelcomeWindow::minimumSize(initial.size()));
            window->setGeometry(initial);
        }
        window->show();
    }

    QObject::connect(&singleInstance, &SingleInstance::activationRequested, &app,
                     [rootObject](const QString &activationToken) {
        if (auto *window = qobject_cast<QQuickWindow *>(rootObject)) {
            if (!activationToken.isEmpty()) {
                // Qt's Wayland plugin consumes this variable in requestActivate().
                qputenv("XDG_ACTIVATION_TOKEN", activationToken.toUtf8());
            }
            window->showNormal();
            if (QGuiApplication::platformName() != QStringLiteral("offscreen")
                && QGuiApplication::platformName() != QStringLiteral("minimal")) {
                window->raise();
            }
            window->requestActivate();
        }
    });

    return app.exec();
}
