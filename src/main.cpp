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

#include <QApplication>
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
    // QApplication rather than QGuiApplication: Plasma's platform integration only
    // offers its native file dialogs (used by "Salvar relatório") to widget apps.
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Mainuan Welcome"));
    app.setApplicationDisplayName(QStringLiteral("Mainuan — Bem-vindo"));
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

    const auto asset = [](const QString &fileName) {
        return QStringLiteral("qrc:/assets/") + fileName;
    };
    ApplicationModel officeModel(&packageService, {
        {QStringLiteral("libreoffice"), QStringLiteral("LibreOffice"),
         QStringLiteral("Suite completa: Writer, Calc e Impress"), QStringLiteral("org.libreoffice.LibreOffice"),
         asset(QStringLiteral("org.libreoffice.LibreOffice.png")), QString()},
        {QStringLiteral("wps"), QStringLiteral("WPS Office"),
         QStringLiteral("Interface familiar ao Microsoft Office"), QStringLiteral("com.wps.Office"),
         asset(QStringLiteral("com.wps.Office.png")), QString()},
        {QStringLiteral("gdocs"), QStringLiteral("Google Docs"),
         QStringLiteral("Documentos online com colaboração"), QString(),
         asset(QStringLiteral("webapp.gdocs.png")), QStringLiteral("https://docs.google.com/")},
        {QStringLiteral("office365"), QStringLiteral("Office 365 Online"),
         QStringLiteral("Suite Microsoft direto no navegador"), QString(),
         asset(QStringLiteral("webapp.office365.png")), QStringLiteral("https://www.office.com/")}
    });

    ApplicationModel browserModel(&packageService, {
        {QStringLiteral("chrome"), QStringLiteral("Google Chrome"),
         QStringLiteral("Navegador do Google"), QStringLiteral("com.google.Chrome"),
         asset(QStringLiteral("com.google.Chrome.png")), QString()},
        {QStringLiteral("edge"), QStringLiteral("Microsoft Edge"),
         QStringLiteral("Compatibilidade com aplicações Microsoft"), QStringLiteral("com.microsoft.Edge"),
         asset(QStringLiteral("com.microsoft.Edge.png")), QString()},
        {QStringLiteral("brave"), QStringLiteral("Brave"),
         QStringLiteral("Bloqueio de anúncios integrado"), QStringLiteral("com.brave.Browser"),
         asset(QStringLiteral("com.brave.Browser.png")), QString()},
        {QStringLiteral("librewolf"), QStringLiteral("LibreWolf"),
         QStringLiteral("Navegador focado em privacidade"), QStringLiteral("io.gitlab.librewolf-community"),
         asset(QStringLiteral("io.gitlab.librewolf-community.png")), QString()},
        {QStringLiteral("opera"), QStringLiteral("Opera GX"),
         QStringLiteral("Navegador com controles de recursos"), QStringLiteral("com.opera.opera-gx"),
         asset(QStringLiteral("com.opera.opera-gx.png")), QString()},
        {QStringLiteral("tor"), QStringLiteral("Tor Browser"),
         QStringLiteral("Navegação pela rede Tor"), QStringLiteral("org.torproject.torbrowser-launcher"),
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
        window->show();
        QTimer::singleShot(0, window, [window]() {
            QScreen *screen = window->screen();
            if (screen == nullptr) {
                screen = QGuiApplication::primaryScreen();
            }
            if (screen != nullptr) {
                const QRect available = screen->availableGeometry();
                const int x = available.x() + (available.width() - window->width()) / 2;
                const int y = available.y() + (available.height() - window->height()) / 2;
                window->setPosition(QPoint(x, y));
            }
        });
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
