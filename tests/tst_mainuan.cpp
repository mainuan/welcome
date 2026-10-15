#include "models/ApplicationModel.h"
#include "models/VideoModel.h"
#include "services/InstallProgressParser.h"
#include "services/AccentProfile.h"
#include "services/InstallService.h"
#include "services/LayoutService.h"
#include "services/PackageService.h"
#include "services/QrCode.h"
#include "services/StartupPreference.h"
#include "services/SystemReportService.h"
#include "services/SystemService.h"
#include "services/VisualStyle.h"

#include <QtTest>

#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <memory>
#include <unistd.h>

namespace {

QString fixture(const QString &name)
{
    return QStringLiteral(MAINUAN_TEST_FIXTURES "/") + name;
}

InstallService::Backend mockBackend()
{
    return {fixture(QStringLiteral("pkexec")), fixture(QStringLiteral("helper")),
            fixture(QStringLiteral("dpkg-query")), QStandardPaths::findExecutable(QStringLiteral("true"))};
}

const QStringList kClamAv = {QStringLiteral("clamav"), QStringLiteral("clamav-daemon"),
                             QStringLiteral("clamav-freshclam")};
const QString kClamUi = QStringLiteral("io.github.linx_systems.ClamUI");

// A simulated system: installed Debian packages and Flatpak applications.
class MockSystem
{
public:
    MockSystem(const QStringList &debPackages, const QStringList &flatpaks)
    {
        writeLines(QStringLiteral("deb"), debPackages);
        QStringList lines;
        for (const QString &flatpak : flatpaks) {
            lines << flatpak + QStringLiteral("\tsystem");
        }
        writeLines(QStringLiteral("flatpak"), lines);
        qputenv("MOCK_STATE_DIR", m_dir.path().toLocal8Bit());
    }

    QStringList lines(const QString &name) const
    {
        QFile file(m_dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }

private:
    void writeLines(const QString &name, const QStringList &lines)
    {
        QFile file(m_dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        for (const QString &line : lines) {
            file.write(line.toUtf8() + '\n');
        }
    }

    QTemporaryDir m_dir;
};

struct Services
{
    std::unique_ptr<PackageService> packages;
    std::unique_ptr<InstallService> installer;
};

Services makeServices(InstallService::Backend backend = mockBackend())
{
    Services services;
    services.packages = std::make_unique<PackageService>(nullptr, fixture(QStringLiteral("flatpak")));
    services.installer = std::make_unique<InstallService>(services.packages.get(), std::move(backend));
    return services;
}

QVariantMap stateOf(const InstallService &installer, const QString &profile)
{
    return installer.profiles().value(profile).toMap();
}

bool waitForDetection(const InstallService &installer)
{
    return QTest::qWaitFor([&installer]() {
        return stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("state")) != QStringLiteral("checking");
    }, 5000);
}

bool runToEnd(InstallService &installer, const QString &profile)
{
    QSignalSpy finished(&installer, &InstallService::finished);
    if (!installer.start(profile)) {
        return false;
    }
    return finished.count() > 0 || finished.wait(15000);
}

} // namespace

class MainuanTest final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    void acceptsOnlyHexAccentColors();
    void rejectsUnexpectedPackageIds();
    void acceptsOnlyKnownLayouts();
    void buildsLayoutScripts();
    void backsUpAndRestoresPanelConfiguration();
    void mapsVisualStylesToMainuanThemes();
    void detectsVisualStyleFromPlasmaConfig();
    void appliesVisualStyleProfiles();
    void reportsVisualStyleFailures();
    void switchesThemeWithinTheActiveStyle();
    void recordsTheChosenAccentColor();
    void exposesModelRolesAndLocalVideoCatalog();

    void parsesHelperProtocol();
    void parsesFlatpakProgress();
    void realHelperRejectsUntrustedInput();

    void installsMissingComponentsWithRealProgress();   // A
    void reportsAlreadyInstalledProfiles();             // B
    void treatsDismissedAuthenticationAsCancelled();    // C
    void reportsPackageManagerErrors();                 // D
    void explainsNetworkAndLockFailures();              // E + lock
    void rejectsConcurrentStarts();                     // F
    void detectsStateAgainOnRestart();                  // G
    void installsOnlyClamUiWhenClamAvExists();          // H
    void installsOnlyClamAvWhenClamUiExists();          // I
    void completesFirewallWithPlasmaIntegration();      // J
    void treatsDeniedFlatpakAuthorizationAsCancelled();
    void disablesAptProfilesOnNonDebianSystems();
    void rejectsUnknownProfiles();

    void rendersQrCodesLocally();
    void parsesSystemReportSources();
    void collectsSystemReportWithoutSecrets();

    void startupPreferenceDefaultsToShown();
    void startupPreferenceIsStoredPerUser();
    void startupPreferenceReadsPlasmaOverrides();
};

void MainuanTest::cleanup()
{
    qunsetenv("MOCK_SCENARIO");
    qunsetenv("MOCK_STATE_DIR");
}

void MainuanTest::acceptsOnlyHexAccentColors()
{
    QVERIFY(SystemService::isValidAccent(QStringLiteral("#3daee9")));
    QVERIFY(SystemService::isValidAccent(QStringLiteral("#AABBCC")));
    QVERIFY(!SystemService::isValidAccent(QStringLiteral("red")));
    QVERIFY(!SystemService::isValidAccent(QStringLiteral("#12345")));
    QVERIFY(!SystemService::isValidAccent(QStringLiteral("#123456789")));
}

void MainuanTest::rejectsUnexpectedPackageIds()
{
    QVERIFY(PackageService::isAllowedApplication(QStringLiteral("org.libreoffice.LibreOffice")));
    QVERIFY(PackageService::isAllowedApplication(QStringLiteral("com.brave.Browser")));
    QVERIFY(PackageService::isAllowedApplication(kClamUi));
    QVERIFY(!PackageService::isAllowedApplication(QStringLiteral("webapp.gdocs")));
    QVERIFY(!PackageService::isAllowedApplication(QStringLiteral("../../etc/passwd")));
    QVERIFY(!PackageService::isAllowedApplication(QStringLiteral("org.example.Unknown")));
}

void MainuanTest::acceptsOnlyKnownLayouts()
{
    QCOMPARE(LayoutService::layoutIds().size(), 6);
    for (const QString &id : LayoutService::layoutIds()) {
        QVERIFY(LayoutService::isAllowedLayout(id));
        QVERIFY(!LayoutService::displayName(id).isEmpty());
    }
    QVERIFY(!LayoutService::isAllowedLayout(QStringLiteral("../../tmp")));
    QVERIFY(!LayoutService::isAllowedLayout(QStringLiteral("unity\");panels().forEach(function(p){p.remove()});//")));
}

void MainuanTest::buildsLayoutScripts()
{
    const QString script = LayoutService::buildScript(QStringLiteral("unity"),
        {QStringLiteral("Chaac.Complete.Weather"), QStringLiteral("x\"];panels().forEach(function(p){p.remove()});//"),
         QStringLiteral("../escape"), QStringLiteral("com.mike.desktop")});
    QVERIFY(script.startsWith(QStringLiteral("var installedWidgets = [\"Chaac.Complete.Weather\",\"com.mike.desktop\"];\n")));
    QVERIFY(script.contains(QStringLiteral("function applyMainuanLayout(id)")));
    QVERIFY(script.trimmed().endsWith(QStringLiteral("applyMainuanLayout(\"unity\");")));
    QVERIFY(!script.contains(QStringLiteral("../escape")));
    QVERIFY(LayoutService::buildScript(QStringLiteral("evil"), {}).isEmpty());

    using Description = LayoutService::Description;
    Description description = LayoutService::parseDescription(QStringLiteral("floating|1|1\n"));
    QVERIFY(description.valid());
    QCOMPARE(description.layout, QStringLiteral("floating"));
    QCOMPARE(description.panels, 1);
    QCOMPARE(description.launchers, 1);
    description = LayoutService::parseDescription(QStringLiteral("|5|1"));
    QVERIFY(description.valid());
    QVERIFY(description.layout.isEmpty()); // custom layout, not one of the six
    QVERIFY(LayoutService::parseDescription(QStringLiteral("hacked|5|1")).layout.isEmpty());
    QVERIFY(!LayoutService::parseDescription(QStringLiteral("Error: TypeError")).valid());
    QVERIFY(!LayoutService::parseDescription(QStringLiteral("unity|x|1")).valid());
}

void MainuanTest::backsUpAndRestoresPanelConfiguration()
{
    QTemporaryDir config;
    QTemporaryDir backups;
    const auto write = [&config](const QString &name, const QByteArray &contents) {
        QFile file(config.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(contents);
    };
    const auto read = [&config](const QString &name) {
        QFile file(config.filePath(name));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    };
    LayoutService service(nullptr, config.path(), backups.path());
    QVERIFY(!service.hasBackup());
    QVERIFY(service.createBackup().isEmpty()); // nothing to back up yet
    QVERIFY(!service.hasBackup());

    write(QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc"), "[Containments][1]\nplugin=org.kde.panel\n");
    write(QStringLiteral("plasmashellrc"), "[PlasmaViews][Panel 1]\nfloating=1\n");
    const QString first = service.createBackup();
    QVERIFY(!first.isEmpty());
    QVERIFY(first.startsWith(backups.path()));
    QVERIFY(service.hasBackup());

    write(QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc"), "broken");
    write(QStringLiteral("plasmashellrc"), "broken");
    QVERIFY(service.restoreFiles(first));
    QCOMPARE(read(QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc")), QByteArray("[Containments][1]\nplugin=org.kde.panel\n"));
    QCOMPARE(read(QStringLiteral("plasmashellrc")), QByteArray("[PlasmaViews][Panel 1]\nfloating=1\n"));

    // Five backups are kept: the first one (the user's own panels) and the newest.
    QString newest;
    for (int i = 0; i < 7; ++i) {
        newest = service.createBackup();
        QVERIFY(!newest.isEmpty());
        QCOMPARE(service.backups().constLast(), newest); // ordered by sequence, not by clock
    }
    QCOMPARE(service.backups().size(), 5);
    QCOMPARE(service.backups().constFirst(), first);

    // A backup without the applets file is refused instead of "restored".
    QDir(backups.path()).mkpath(QStringLiteral("incomplete"));
    QVERIFY(!service.restoreFiles(backups.filePath(QStringLiteral("incomplete"))));
    QCOMPARE(read(QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc")), QByteArray("[Containments][1]\nplugin=org.kde.panel\n"));

    // Outside Plasma nothing is applied and no backup is taken.
    const int before = static_cast<int>(service.backups().size());
    service.apply(QStringLiteral("floating"));
    QVERIFY(!service.busy());
    QVERIFY(service.messageIsError());
    QCOMPARE(service.backups().size(), before);
    service.apply(QStringLiteral("rm -rf"));
    QCOMPARE(service.message(), QStringLiteral("Layout inválido."));
}

void MainuanTest::mapsVisualStylesToMainuanThemes()
{
    const VisualStyleProfile *dream = VisualStyles::find(QStringLiteral("blur"));
    const VisualStyleProfile *tahoe = VisualStyles::find(QStringLiteral("glass"));
    const VisualStyleProfile *breeze = VisualStyles::find(QStringLiteral("solid"));
    QVERIFY(dream && tahoe && breeze);
    QCOMPARE(dream->name, QStringLiteral("Dream"));
    QCOMPARE(dream->package(false), QStringLiteral("Dream-Light-Color-Global-6"));
    QCOMPARE(dream->package(true), QStringLiteral("Dream-Dark-Color-Global-6"));
    QCOMPARE(tahoe->name, QStringLiteral("Tahoe"));
    QCOMPARE(tahoe->package(false), QStringLiteral("com.github.vinceliuice.MacTahoe-Light"));
    QCOMPARE(tahoe->package(true), QStringLiteral("com.github.vinceliuice.MacTahoe-Dark"));
    QCOMPARE(breeze->name, QStringLiteral("Breeze"));
    QCOMPARE(breeze->package(false), QStringLiteral("org.kde.breeze.desktop"));
    QCOMPARE(breeze->package(true), QStringLiteral("org.kde.breezedark.desktop"));
    QCOMPARE(VisualStyles::profiles().size(), 3);

    // Each accent color carries the Mainuan wallpaper of the same color.
    const QList<QPair<QString, QString>> wallpapers = {
        {QStringLiteral("#d08040"), QStringLiteral("05marrom.png")}, {QStringLiteral("#e8177d"), QStringLiteral("03magenta.png")},
        {QStringLiteral("#3daee9"), QStringLiteral("01ciano.png")},  {QStringLiteral("#3dd425"), QStringLiteral("06lima.png")},
        {QStringLiteral("#aab6b9"), QStringLiteral("02cinza.png")},  {QStringLiteral("#a588cb"), QStringLiteral("04purpura.png")}};
    QCOMPARE(AccentProfiles::profiles().size(), wallpapers.size());
    for (const auto &[color, wallpaper] : wallpapers) {
        QVERIFY(AccentProfiles::find(color));
        QCOMPARE(AccentProfiles::find(color)->wallpaper, wallpaper);
    }
    QCOMPARE(AccentProfiles::find(QStringLiteral("#E8177D"))->name, QStringLiteral("Rosa"));
    QVERIFY(!AccentProfiles::find(QStringLiteral("#123456")));

    QVERIFY(SystemService::isAllowedVisualStyle(QStringLiteral("blur")));
    QVERIFY(SystemService::isAllowedVisualStyle(QStringLiteral("glass")));
    QVERIFY(SystemService::isAllowedVisualStyle(QStringLiteral("solid")));
    QVERIFY(!SystemService::isAllowedVisualStyle(QStringLiteral("Dream-Light-Color-Global-6")));
    QVERIFY(!SystemService::isAllowedVisualStyle(QStringLiteral("blur; plasmashell --replace")));
    QVERIFY(!SystemService::isAllowedVisualStyle(QStringLiteral("$(id)")));
    QVERIFY(!SystemService::isAllowedVisualStyle(QString()));
}

void MainuanTest::detectsVisualStyleFromPlasmaConfig()
{
    using VisualStyles::detect;
    QCOMPARE(detect(QStringLiteral("Dream-Light-Color-Global-6"), {}), QStringLiteral("blur"));
    QCOMPARE(detect(QStringLiteral("Dream-Dark-Color-Global-6"), QStringLiteral("Dream-Color-Plasma")), QStringLiteral("blur"));
    QCOMPARE(detect(QStringLiteral("com.github.vinceliuice.MacTahoe-Dark"), {}), QStringLiteral("glass"));
    QCOMPARE(detect(QStringLiteral("org.kde.breezedark.desktop"), {}), QStringLiteral("solid"));
    // Breeze is Plasma's default package and is not written to kdeglobals.
    QCOMPARE(detect({}, {}), QStringLiteral("solid"));
    QCOMPARE(detect({}, QStringLiteral("default")), QStringLiteral("solid"));
    // A fresh Mainuan install sets the Dream Plasma theme without a package id.
    QCOMPARE(detect({}, QStringLiteral("Dream-Color-Plasma")), QStringLiteral("blur"));
    QCOMPARE(detect({}, QStringLiteral("MacTahoe-Light")), QStringLiteral("glass"));
    // Other global themes are not one of the three styles.
    QVERIFY(detect(QStringLiteral("org.kde.oxygen"), {}).isEmpty());
    QVERIFY(detect({}, QStringLiteral("Sweet")).isEmpty());
}

namespace {

// A user home and a system data directory with the Mainuan themes, plus the
// simulated Plasma tools from tests/fixtures/plasma.
class PlasmaSandbox
{
public:
    PlasmaSandbox()
    {
        const QString data = m_root.filePath(QStringLiteral("data"));
        for (const VisualStyleProfile &profile : VisualStyles::profiles()) {
            for (const QString &package : {profile.lightPackage, profile.darkPackage}) {
                touch(data + QStringLiteral("/plasma/look-and-feel/") + package + QStringLiteral("/metadata.json"));
            }
        }
        for (const AccentProfile &accent : AccentProfiles::profiles()) {
            touch(data + QStringLiteral("/wallpapers/") + accent.wallpaper);
        }
        for (const QString &icons : {QStringLiteral("breeze"), QStringLiteral("breeze-dark"), QStringLiteral("kora-cyan")}) {
            touch(data + QStringLiteral("/icons/") + icons + QStringLiteral("/index.theme"));
        }
        // Cursor themes Mainuan ships (MacTahoe's are missing there too).
        touch(data + QStringLiteral("/icons/breeze_cursors/cursors/default"));
        touch(data + QStringLiteral("/icons/cyan-cursor/cursors/default"));

        qputenv("XDG_CONFIG_HOME", config().toLocal8Bit());
        qputenv("XDG_CONFIG_DIRS", m_root.filePath(QStringLiteral("xdg")).toLocal8Bit());
        qputenv("XDG_DATA_HOME", m_root.filePath(QStringLiteral("home-data")).toLocal8Bit());
        qputenv("XDG_DATA_DIRS", data.toLocal8Bit());
        qputenv("MOCK_LOG", log().toLocal8Bit());
        QDir().mkpath(config());
    }

    ~PlasmaSandbox()
    {
        for (const char *name : {"XDG_CONFIG_HOME", "XDG_CONFIG_DIRS", "XDG_DATA_HOME", "XDG_DATA_DIRS", "MOCK_LOG",
                                 "MOCK_LNF_FAIL", "MOCK_LNF_NOOP", "MOCK_WALLPAPER_FAIL"}) {
            qunsetenv(name);
        }
    }

    QString config() const { return m_root.filePath(QStringLiteral("config")); }
    QString log() const { return m_root.filePath(QStringLiteral("calls.log")); }
    QString data() const { return m_root.filePath(QStringLiteral("data")); }
    static QString tools() { return QStringLiteral(MAINUAN_TEST_FIXTURES "/plasma"); }

    QStringList takeCalls() const
    {
        QFile file(log());
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        const QStringList calls = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        file.close();
        file.remove();
        return calls;
    }

private:
    static void touch(const QString &path)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    QTemporaryDir m_root;
};

bool waitIdle(const SystemService &service)
{
    return QTest::qWaitFor([&service]() { return !service.busy(); }, 10000);
}

} // namespace

void MainuanTest::appliesVisualStyleProfiles()
{
    PlasmaSandbox sandbox;
    SystemService service;
    service.setToolSearchPaths({PlasmaSandbox::tools()});
    QVERIFY(service.visualStyleAvailable());
    QCOMPARE(service.installedVisualStyles(), (QStringList{QStringLiteral("blur"), QStringLiteral("glass"), QStringLiteral("solid")}));
    QCOMPARE(service.visualStyle(), QStringLiteral("solid")); // empty config: Plasma defaults

    service.setVisualStyle(QStringLiteral("blur"));
    QVERIFY(service.busy());
    QVERIFY(waitIdle(service));
    QCOMPARE(service.visualStyle(), QStringLiteral("blur"));
    QCOMPARE(service.lastMessage(), QStringLiteral("Estilo Dream aplicado."));
    // The style does not touch the wallpaper; it follows the accent color.
    QCOMPARE(sandbox.takeCalls(), (QStringList{QStringLiteral("lookandfeel Dream-Light-Color-Global-6")}));

    // Tahoe names icon and cursor themes Mainuan does not ship: Breeze fallbacks.
    service.setVisualStyle(QStringLiteral("glass"));
    QVERIFY(waitIdle(service));
    QCOMPARE(service.visualStyle(), QStringLiteral("glass"));
    QCOMPARE(service.lastMessage(), QStringLiteral("Estilo Tahoe aplicado."));
    QCOMPARE(sandbox.takeCalls(), (QStringList{
        QStringLiteral("lookandfeel com.github.vinceliuice.MacTahoe-Light"),
        QStringLiteral("plasma-changeicons breeze"),
        QStringLiteral("plasma-apply-cursortheme breeze_cursors")}));

    service.setVisualStyle(QStringLiteral("solid"));
    QVERIFY(waitIdle(service));
    QCOMPARE(service.visualStyle(), QStringLiteral("solid"));
    QCOMPARE(service.lastMessage(), QStringLiteral("Estilo Breeze aplicado."));
    QCOMPARE(sandbox.takeCalls(), (QStringList{QStringLiteral("lookandfeel org.kde.breeze.desktop")}));

    // Repeated switching keeps working and the state is read back by a new instance.
    for (const QString &style : {QStringLiteral("blur"), QStringLiteral("solid"), QStringLiteral("glass")}) {
        service.setVisualStyle(style);
        QVERIFY(waitIdle(service));
        QCOMPARE(service.visualStyle(), style);
    }
    SystemService reopened;
    QCOMPARE(reopened.visualStyle(), QStringLiteral("glass"));
}

void MainuanTest::reportsVisualStyleFailures()
{
    PlasmaSandbox sandbox;
    SystemService service;
    service.setToolSearchPaths({PlasmaSandbox::tools()});

    service.setVisualStyle(QStringLiteral("unknown"));
    QVERIFY(!service.busy());
    QCOMPARE(service.lastMessage(), QStringLiteral("Erro: Estilo visual inválido."));

    // The essential step fails: no success and the selection does not move.
    qputenv("MOCK_LNF_FAIL", "1");
    service.setVisualStyle(QStringLiteral("blur"));
    QVERIFY(waitIdle(service));
    QCOMPARE(service.visualStyle(), QStringLiteral("solid"));
    QVERIFY(service.lastMessage().startsWith(QStringLiteral("Erro: Não foi possível aplicar o estilo Dream.")));
    QVERIFY(sandbox.takeCalls().isEmpty());
    qunsetenv("MOCK_LNF_FAIL");

    // The tool reports success but Plasma's configuration does not change.
    qputenv("MOCK_LNF_NOOP", "1");
    service.setVisualStyle(QStringLiteral("glass"));
    QVERIFY(waitIdle(service));
    QCOMPARE(service.visualStyle(), QStringLiteral("solid"));
    QCOMPARE(service.lastMessage(), QStringLiteral("Erro: O Plasma não confirmou o estilo Tahoe."));
    qunsetenv("MOCK_LNF_NOOP");
    sandbox.takeCalls();

    service.setVisualStyle(QStringLiteral("blur"));
    QVERIFY(waitIdle(service));
    QCOMPARE(service.visualStyle(), QStringLiteral("blur"));

    // Missing package.
    QVERIFY(QFile::remove(sandbox.data() + QStringLiteral("/plasma/look-and-feel/org.kde.breeze.desktop/metadata.json")));
    service.setVisualStyle(QStringLiteral("solid"));
    QVERIFY(!service.busy());
    QCOMPARE(service.lastMessage(), QStringLiteral("Erro: O estilo Breeze não está instalado neste sistema."));
    service.refresh();
    QVERIFY(!service.installedVisualStyles().contains(QStringLiteral("solid")));

    // Missing plasma-apply-lookandfeel.
    QTemporaryDir noTools;
    service.setToolSearchPaths({noTools.path()});
    QVERIFY(!service.visualStyleAvailable());
    service.setVisualStyle(QStringLiteral("glass"));
    QVERIFY(!service.busy());
    QVERIFY(service.lastMessage().contains(QStringLiteral("plasma-apply-lookandfeel")));
    QCOMPARE(service.visualStyle(), QStringLiteral("blur"));
}

void MainuanTest::switchesThemeWithinTheActiveStyle()
{
    PlasmaSandbox sandbox;
    SystemService service;
    service.setToolSearchPaths({PlasmaSandbox::tools()});
    service.setVisualStyle(QStringLiteral("blur"));
    QVERIFY(waitIdle(service));
    QVERIFY(!service.darkTheme());
    sandbox.takeCalls();

    service.setTheme(QStringLiteral("dark"));
    QVERIFY(waitIdle(service));
    QVERIFY(service.darkTheme());
    QCOMPARE(service.visualStyle(), QStringLiteral("blur"));
    QCOMPARE(service.lastMessage(), QStringLiteral("Tema escuro aplicado."));
    // The theme switch keeps the wallpaper.
    QCOMPARE(sandbox.takeCalls(), (QStringList{QStringLiteral("lookandfeel Dream-Dark-Color-Global-6")}));

    // A style chosen in dark mode uses the dark variant.
    service.setVisualStyle(QStringLiteral("solid"));
    QVERIFY(waitIdle(service));
    QVERIFY(service.darkTheme());
    QCOMPARE(sandbox.takeCalls().constFirst(), QStringLiteral("lookandfeel org.kde.breezedark.desktop"));
}

void MainuanTest::recordsTheChosenAccentColor()
{
    PlasmaSandbox sandbox;
    SystemService service;
    service.setToolSearchPaths({PlasmaSandbox::tools()});

    service.setAccent(QStringLiteral("#E8177D"));
    QVERIFY(waitIdle(service));
    QCOMPARE(sandbox.takeCalls(), (QStringList{
        QStringLiteral("plasma-apply-colorscheme --accent-color #e8177d"),
        QStringLiteral("plasma-apply-wallpaperimage ") + sandbox.data() + QStringLiteral("/wallpapers/03magenta.png")}));
    QCOMPARE(service.lastMessage(), QStringLiteral("Cor Rosa e papel de parede aplicados."));
    QCOMPARE(service.accentColor(), QStringLiteral("#e8177d"));
    // Stored where System Settings stores it, so Plasma keeps it across global themes.
    QFile globals(sandbox.config() + QStringLiteral("/kdeglobals"));
    QVERIFY(globals.open(QIODevice::ReadOnly));
    QVERIFY(globals.readAll().contains("AccentColor=232,23,125"));

    // Each color applies its own wallpaper.
    for (const AccentProfile &accent : AccentProfiles::profiles()) {
        service.setAccent(accent.color);
        QVERIFY(waitIdle(service));
        QCOMPARE(sandbox.takeCalls().constLast(),
                 QStringLiteral("plasma-apply-wallpaperimage ") + sandbox.data() + QStringLiteral("/wallpapers/") + accent.wallpaper);
        QCOMPARE(service.accentColor(), accent.color);
    }

    // Missing wallpaper: the color is applied, the wallpaper is reported, not claimed.
    QVERIFY(QFile::remove(sandbox.data() + QStringLiteral("/wallpapers/06lima.png")));
    service.setAccent(QStringLiteral("#3dd425"));
    QVERIFY(waitIdle(service));
    QCOMPARE(sandbox.takeCalls(), (QStringList{QStringLiteral("plasma-apply-colorscheme --accent-color #3dd425")}));
    QCOMPARE(service.accentColor(), QStringLiteral("#3dd425"));
    QCOMPARE(service.lastMessage(), QStringLiteral("Erro: Cor Verde aplicada, mas o papel de parede 06lima.png não foi encontrado."));

    // The wallpaper tool fails.
    qputenv("MOCK_WALLPAPER_FAIL", "1");
    service.setAccent(QStringLiteral("#3daee9"));
    QVERIFY(waitIdle(service));
    QCOMPARE(service.lastMessage(), QStringLiteral("Erro: Cor Azul aplicada, mas não foi possível trocar o papel de parede."));
    qunsetenv("MOCK_WALLPAPER_FAIL");
    sandbox.takeCalls();

    service.setAccent(QStringLiteral("red; rm -rf ~"));
    QVERIFY(!service.busy());
    QCOMPARE(service.lastMessage(), QStringLiteral("Erro: Cor de destaque inválida."));
    QVERIFY(sandbox.takeCalls().isEmpty());
}

void MainuanTest::exposesModelRolesAndLocalVideoCatalog()
{
    PackageService packageService;
    const ApplicationEntry onlineEntry{
        QStringLiteral("gdocs"), QStringLiteral("Documentos"), QStringLiteral("Online"),
        QStringLiteral("webapp.gdocs"), QStringLiteral("qrc:/assets/webapp.gdocs.png"),
        QStringLiteral("https://docs.google.com/")};
    ApplicationModel applications(&packageService, {onlineEntry});

    QCOMPARE(applications.rowCount(), 1);
    const QModelIndex applicationIndex = applications.index(0, 0);
    QCOMPARE(applications.data(applicationIndex, ApplicationModel::NameRole).toString(),
             QStringLiteral("Documentos"));
    QVERIFY(applications.data(applicationIndex, ApplicationModel::AvailableRole).toBool());
    QCOMPARE(applications.data(applicationIndex, ApplicationModel::InstallStateRole).toString(),
             QStringLiteral("Online"));
    QCOMPARE(applications.data(applicationIndex, ApplicationModel::WebUrlRole).toString(),
             QStringLiteral("https://docs.google.com/"));
    QVERIFY(applications.data(applicationIndex, ApplicationModel::InstallProfileRole).toString().isEmpty());
    QCOMPARE(applications.roleNames().value(ApplicationModel::WebUrlRole), QByteArray("webUrl"));

    VideoModel videos;
    QCOMPARE(videos.rowCount(), 5);
    const QModelIndex videoIndex = videos.index(0, 0);
    QCOMPARE(videos.data(videoIndex, VideoModel::TitleRole).toString(),
             QStringLiteral("Conhecendo o sistema"));
    QCOMPARE(videos.data(videoIndex, VideoModel::IconSourceRole).toString(),
             QStringLiteral("qrc:/assets/icone-videoaulas_g204.svg"));
    QVERIFY(videos.data(videoIndex, VideoModel::FileSourceRole).toString().startsWith(
        QStringLiteral("file:///var/lib/curso-linux/videos/")));
}

void MainuanTest::parsesHelperProtocol()
{
    using InstallProgress::HelperEvent;
    using InstallProgress::parseHelperLine;

    HelperEvent event = parseHelperLine(QStringLiteral("dlstatus:3:41.2500:Retrieving file 3 of 29"));
    QCOMPARE(event.type, HelperEvent::Type::Download);
    QCOMPARE(event.percent, 41.25);
    int current = 0;
    int total = 0;
    QVERIFY(InstallProgress::parseAptFileCounter(event.text, &current, &total));
    QCOMPARE(current, 3);
    QCOMPARE(total, 29);

    event = parseHelperLine(QStringLiteral("pmstatus:clamav:12.6552:Unpacking clamav (amd64)"));
    QCOMPARE(event.type, HelperEvent::Type::Package);
    QCOMPARE(event.name, QStringLiteral("clamav"));
    QVERIFY(!event.configuring);

    event = parseHelperLine(QStringLiteral("pmstatus:ufw:60.1:Preparing to configure ufw (amd64)"));
    QVERIFY(event.configuring);

    event = parseHelperLine(QStringLiteral("pmerror:clamav-daemon:70:subprocess returned error: exit status 1"));
    QCOMPARE(event.type, HelperEvent::Type::PackageError);
    QCOMPARE(event.text, QStringLiteral("subprocess returned error: exit status 1"));

    QCOMPARE(parseHelperLine(QStringLiteral("@@phase signatures")).name, QStringLiteral("signatures"));
    QCOMPARE(parseHelperLine(QStringLiteral("@@error locked")).type, HelperEvent::Type::Error);
    QCOMPARE(parseHelperLine(QStringLiteral("@@error")).name, QStringLiteral("failed"));
    QCOMPARE(parseHelperLine(QStringLiteral("@@done")).type, HelperEvent::Type::Done);
    QCOMPARE(parseHelperLine(QStringLiteral("pmstatus:broken")).type, HelperEvent::Type::Unknown);
    QCOMPARE(parseHelperLine(QStringLiteral("dlstatus:1:abc:x")).type, HelperEvent::Type::Unknown);
    QCOMPARE(parseHelperLine(QStringLiteral("dlstatus:1:250:x")).percent, 100.0);
}

void MainuanTest::parsesFlatpakProgress()
{
    using InstallProgress::FlatpakProgress;
    using InstallProgress::parseFlatpakLine;

    FlatpakProgress state;
    state = parseFlatpakLine(QStringLiteral("Required runtime for app found in remote flathub"), state);
    QVERIFY(!state.changed);

    state = parseFlatpakLine(QStringLiteral("Installing 1/3…"), state);
    QVERIFY(state.changed);
    QCOMPARE(state.operation, 1);
    QCOMPARE(state.operations, 3);
    QCOMPARE(state.percent, -1);

    state = parseFlatpakLine(QStringLiteral("Installing 1/3… ████████▌            45%  1.2 MB/s  00:12"), state);
    QCOMPARE(state.percent, 45);

    state = parseFlatpakLine(QStringLiteral("Instalando 2/3…"), state);
    QCOMPARE(state.operation, 2);
    QCOMPARE(state.percent, -1);

    state = parseFlatpakLine(QStringLiteral("Instalando 2/3… ███ 100%  3,4 MB/s  00:00"), state);
    QCOMPARE(state.percent, 100);

    FlatpakProgress single;
    single = parseFlatpakLine(QStringLiteral("Installing…"), single);
    QCOMPARE(single.operations, 1);
    single = parseFlatpakLine(QStringLiteral("Installing… ██ 12%  800 kB/s"), single);
    QCOMPARE(single.percent, 12);
}

void MainuanTest::realHelperRejectsUntrustedInput()
{
    const auto run = [](const QStringList &arguments, QString *output) {
        QProcess helper;
        helper.start(QStringLiteral(MAINUAN_TEST_HELPER), arguments);
        helper.waitForFinished(10000);
        *output = QString::fromUtf8(helper.readAllStandardOutput()).trimmed();
        return helper.exitCode();
    };

    QString output;
    QCOMPARE(run({}, &output), 2);
    QCOMPARE(output, QStringLiteral("@@error invalid"));
    QCOMPARE(run({QStringLiteral("install"), QStringLiteral("firewall; id")}, &output), 2);
    QCOMPARE(output, QStringLiteral("@@error invalid"));
    QCOMPARE(run({QStringLiteral("install"), QStringLiteral("--help")}, &output), 2);
    QCOMPARE(run({QStringLiteral("remove"), QStringLiteral("ufw")}, &output), 2);
    QCOMPARE(run({QStringLiteral("install"), QStringLiteral("firewall"), QStringLiteral("extra")}, &output), 2);
    if (geteuid() != 0) {
        QCOMPARE(run({QStringLiteral("install"), QStringLiteral("firewall")}, &output), 1);
        QCOMPARE(output, QStringLiteral("@@error permission"));
    }
}

void MainuanTest::installsMissingComponentsWithRealProgress()
{
    MockSystem system({}, {});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(waitForDetection(installer));
    QCOMPARE(stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("state")).toString(),
             QStringLiteral("missing"));
    QVERIFY(stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("canInstall")).toBool());

    struct Sample { QString phase; int percent; int step; };
    QList<Sample> progress;
    connect(&installer, &InstallService::stateChanged, &installer, [&]() {
        progress.append({installer.phase(), installer.percent(), installer.step()});
    });

    QVERIFY(runToEnd(installer, QStringLiteral("antivirus")));
    QCOMPARE(installer.phase(), QStringLiteral("success"));
    QCOMPARE(installer.percent(), 100);
    QCOMPARE(installer.stepCount(), 2);
    QVERIFY(installer.resultMessage().contains(QStringLiteral("ClamAV e ClamUI")));

    // Phases are reported in order and stages without data stay indeterminate.
    QStringList phases;
    for (const Sample &sample : std::as_const(progress)) {
        if (phases.isEmpty() || phases.constLast() != sample.phase) {
            phases << sample.phase;
        }
        if (sample.phase == QStringLiteral("authenticating") || sample.phase == QStringLiteral("updating")
            || sample.phase == QStringLiteral("preparing")) {
            QCOMPARE(sample.percent, -1);
        }
    }
    for (const QString &expected : {QStringLiteral("authenticating"), QStringLiteral("updating"),
                                    QStringLiteral("downloading"), QStringLiteral("installing"),
                                    QStringLiteral("configuring"), QStringLiteral("success")}) {
        QVERIFY2(phases.contains(expected), qPrintable(expected + QStringLiteral(" missing from ") + phases.join(u',')));
    }

    // Every determinate download value comes from the backend: APT dlstatus
    // lines in step 1, Flatpak progress lines in step 2.
    const QList<int> aptReported = {0, 18, 41, 66, 88, 100};
    const QList<int> flatpakReported = {0, 25, 50, 75, 100};
    int aptSamples = 0;
    int flatpakSamples = 0;
    for (const Sample &sample : std::as_const(progress)) {
        if (sample.phase != QStringLiteral("downloading") || sample.percent < 0) {
            continue;
        }
        const QList<int> &expected = sample.step == 1 ? aptReported : flatpakReported;
        QVERIFY2(expected.contains(sample.percent),
                 qPrintable(QStringLiteral("step %1: %2").arg(sample.step).arg(sample.percent)));
        ++(sample.step == 1 ? aptSamples : flatpakSamples);
    }
    QVERIFY(aptSamples > 0);
    QVERIFY(flatpakSamples > 0);

    QVERIFY(QTest::qWaitFor([&]() {
        return stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("state")) == QStringLiteral("installed");
    }, 5000));
    QVERIFY(stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("canLaunch")).toBool());
    for (const QString &package : kClamAv) {
        QVERIFY(system.lines(QStringLiteral("deb")).contains(package));
    }
    QVERIFY(system.lines(QStringLiteral("flatpak")).constFirst().startsWith(kClamUi));
}

void MainuanTest::reportsAlreadyInstalledProfiles()
{
    MockSystem system(kClamAv, {kClamUi});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(QTest::qWaitFor([&]() {
        return stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("state")) == QStringLiteral("installed");
    }, 5000));
    QVERIFY(!stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("canInstall")).toBool());

    QVERIFY(runToEnd(installer, QStringLiteral("antivirus")));
    QCOMPARE(installer.phase(), QStringLiteral("success"));
    QCOMPARE(installer.stepCount(), 0);
}

void MainuanTest::treatsDismissedAuthenticationAsCancelled()
{
    MockSystem system({}, {});
    qputenv("MOCK_SCENARIO", "cancel");
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(waitForDetection(installer));

    QVERIFY(runToEnd(installer, QStringLiteral("firewall")));
    QCOMPARE(installer.phase(), QStringLiteral("cancelled"));
    QVERIFY(!installer.busy());
    QVERIFY(installer.resultMessage().contains(QStringLiteral("Nenhuma alteração")));
    QVERIFY(system.lines(QStringLiteral("deb")).isEmpty());

    qputenv("MOCK_SCENARIO", "auth");
    QVERIFY(runToEnd(installer, QStringLiteral("firewall")));
    QCOMPARE(installer.phase(), QStringLiteral("error"));
    QVERIFY(installer.resultMessage().contains(QStringLiteral("autenticação")));
}

void MainuanTest::reportsPackageManagerErrors()
{
    MockSystem system({}, {});
    qputenv("MOCK_SCENARIO", "apt-error");
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(waitForDetection(installer));

    QVERIFY(runToEnd(installer, QStringLiteral("codecs")));
    QCOMPARE(installer.phase(), QStringLiteral("error"));
    QVERIFY(!installer.resultMessage().contains(QStringLiteral("dpkg")));
    QVERIFY(installer.details().contains(QStringLiteral("post-installation script")));
    QVERIFY(installer.details().contains(QStringLiteral("Sub-process /usr/bin/dpkg returned an error code")));
    QVERIFY(stateOf(installer, QStringLiteral("codecs")).value(QStringLiteral("state")) != QStringLiteral("installed"));
}

void MainuanTest::explainsNetworkAndLockFailures()
{
    MockSystem system({}, {});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(waitForDetection(installer));

    qputenv("MOCK_SCENARIO", "network");
    QVERIFY(runToEnd(installer, QStringLiteral("firewall")));
    QCOMPARE(installer.phase(), QStringLiteral("error"));
    QVERIFY(installer.resultMessage().contains(QStringLiteral("conexão com a internet")));
    QVERIFY(installer.details().contains(QStringLiteral("Temporary failure resolving")));

    qputenv("MOCK_SCENARIO", "locked");
    QVERIFY(runToEnd(installer, QStringLiteral("firewall")));
    QCOMPARE(installer.phase(), QStringLiteral("error"));
    QVERIFY(installer.resultMessage().contains(QStringLiteral("Outro programa")));

    qputenv("MOCK_SCENARIO", "flatpak-network");
    MockSystem withClamAv(kClamAv, {});
    Services second = makeServices();
    QVERIFY(waitForDetection(*second.installer));
    QVERIFY(runToEnd(*second.installer, QStringLiteral("antivirus")));
    QCOMPARE(second.installer->phase(), QStringLiteral("error"));
    QVERIFY(second.installer->resultMessage().contains(QStringLiteral("conexão com a internet")));
}

void MainuanTest::rejectsConcurrentStarts()
{
    MockSystem system({}, {});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(waitForDetection(installer));

    QSignalSpy finished(&installer, &InstallService::finished);
    QVERIFY(installer.start(QStringLiteral("firewall")));
    QVERIFY(installer.busy());
    QVERIFY(!installer.start(QStringLiteral("firewall")));
    QVERIFY(!installer.start(QStringLiteral("codecs")));
    installer.reset(); // ignored while running
    QVERIFY(installer.busy());
    QVERIFY(finished.wait(15000));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(system.lines(QStringLiteral("deb")).count(QStringLiteral("ufw")), 1);
}

void MainuanTest::detectsStateAgainOnRestart()
{
    MockSystem system({}, {});
    {
        Services services = makeServices();
        QVERIFY(waitForDetection(*services.installer));
        QVERIFY(runToEnd(*services.installer, QStringLiteral("firewall")));
        QCOMPARE(services.installer->phase(), QStringLiteral("success"));
    }
    Services restarted = makeServices();
    QVERIFY(QTest::qWaitFor([&]() {
        return stateOf(*restarted.installer, QStringLiteral("firewall")).value(QStringLiteral("state"))
            == QStringLiteral("installed");
    }, 5000));
    QCOMPARE(restarted.installer->phase(), QStringLiteral("idle"));
}

void MainuanTest::installsOnlyClamUiWhenClamAvExists()
{
    MockSystem system(kClamAv, {});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(QTest::qWaitFor([&]() {
        return stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("state")) == QStringLiteral("partial");
    }, 5000));
    QCOMPARE(stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("summary")).toString(),
             QStringLiteral("Falta: ClamUI"));

    QVERIFY(runToEnd(installer, QStringLiteral("antivirus")));
    QCOMPARE(installer.phase(), QStringLiteral("success"));
    QCOMPARE(installer.stepCount(), 1);
    QVERIFY(!installer.details().contains(QStringLiteral("Pacotes a instalar")));
    QCOMPARE(system.lines(QStringLiteral("deb")), kClamAv);
}

void MainuanTest::installsOnlyClamAvWhenClamUiExists()
{
    MockSystem system({}, {kClamUi});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(QTest::qWaitFor([&]() {
        return stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("state")) == QStringLiteral("partial");
    }, 5000));

    QVERIFY(runToEnd(installer, QStringLiteral("antivirus")));
    QCOMPARE(installer.phase(), QStringLiteral("success"));
    QCOMPARE(installer.stepCount(), 1);
    QCOMPARE(system.lines(QStringLiteral("deb")), kClamAv);
    QCOMPARE(system.lines(QStringLiteral("flatpak")).size(), 1);
}

void MainuanTest::completesFirewallWithPlasmaIntegration()
{
    MockSystem system({QStringLiteral("ufw")}, {});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(QTest::qWaitFor([&]() {
        return stateOf(installer, QStringLiteral("firewall")).value(QStringLiteral("state")) == QStringLiteral("partial");
    }, 5000));

    QVERIFY(runToEnd(installer, QStringLiteral("firewall")));
    QCOMPARE(installer.phase(), QStringLiteral("success"));
    QVERIFY(installer.details().contains(QStringLiteral("Pacotes a instalar: plasma-firewall")));
    QCOMPARE(system.lines(QStringLiteral("deb")), (QStringList{QStringLiteral("ufw"), QStringLiteral("plasma-firewall")}));
}

void MainuanTest::treatsDeniedFlatpakAuthorizationAsCancelled()
{
    MockSystem system(kClamAv, {});
    qputenv("MOCK_SCENARIO", "flatpak-denied");
    Services services = makeServices();
    QVERIFY(waitForDetection(*services.installer));
    QVERIFY(runToEnd(*services.installer, QStringLiteral("antivirus")));
    QCOMPARE(services.installer->phase(), QStringLiteral("cancelled"));
}

void MainuanTest::disablesAptProfilesOnNonDebianSystems()
{
    MockSystem system({}, {});
    InstallService::Backend backend = mockBackend();
    backend.dpkgQuery = QStringLiteral("/nonexistent/dpkg-query");
    Services services = makeServices(backend);
    InstallService &installer = *services.installer;
    QVERIFY(waitForDetection(installer));
    QVERIFY(!installer.aptSupported());

    // Without dpkg the state comes from files on this host. Codecs are APT only:
    // unless the host already has them, they must be reported as unavailable.
    const QVariantMap codecs = stateOf(installer, QStringLiteral("codecs"));
    if (codecs.value(QStringLiteral("state")) != QStringLiteral("installed")) {
        QVERIFY(!codecs.value(QStringLiteral("canInstall")).toBool());
        QVERIFY(!codecs.value(QStringLiteral("unavailableReason")).toString().isEmpty());
        QVERIFY(runToEnd(installer, QStringLiteral("codecs")));
        QCOMPARE(installer.phase(), QStringLiteral("error"));
        QVERIFY(installer.resultMessage().contains(QStringLiteral("Debian ou Ubuntu")));
    }
    // Any profile that cannot be installed explains why; Flatpak-only gaps stay installable.
    for (const QString &profile : {QStringLiteral("antivirus"), QStringLiteral("firewall"), QStringLiteral("codecs")}) {
        const QVariantMap state = stateOf(installer, profile);
        if (state.value(QStringLiteral("state")) != QStringLiteral("installed")
            && !state.value(QStringLiteral("canInstall")).toBool()) {
            QVERIFY(!state.value(QStringLiteral("unavailableReason")).toString().isEmpty());
        }
    }
}

void MainuanTest::rejectsUnknownProfiles()
{
    MockSystem system({}, {});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(!installer.start(QStringLiteral("rm -rf /")));
    QVERIFY(!installer.start(QStringLiteral("app:org.example.Unknown")));
    QVERIFY(!installer.registerApplication(QStringLiteral("org.example.Unknown"), QStringLiteral("X"), {}, {}));
    QVERIFY(installer.registerApplication(QStringLiteral("com.brave.Browser"), QStringLiteral("Brave"), {}, {}));
    QVERIFY(!installer.registerApplication(QStringLiteral("com.brave.Browser"), QStringLiteral("Brave"), {}, {}));
    QVERIFY(!installer.busy());
    QVERIFY(!installer.launch(QStringLiteral("unknown")));
}

void MainuanTest::rendersQrCodesLocally()
{
    const QImage image = QrCode::render(QStringLiteral("mestresemlinux@gmail.com"), 4, 2);
    QVERIFY(!image.isNull());
    QCOMPARE(image.width(), image.height());
    QCOMPARE(image.width() % 4, 0);
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::white));              // quiet zone
    QCOMPARE(image.pixelColor(2 * 4, 2 * 4), QColor(Qt::black));      // finder pattern corner
    QVERIFY(QrCode::render(QString()).isNull());
    QVERIFY(QrCode::dataUrl(QStringLiteral("x")).startsWith(QStringLiteral("data:image/png;base64,")));
    QVERIFY(QrCode::dataUrl(QString()).isEmpty());
}

void MainuanTest::parsesSystemReportSources()
{
    const auto os = SystemReportService::parseOsRelease(QStringLiteral(
        "PRETTY_NAME=\"Mainuan 2026 LTS\"\nVERSION=\"2026 LTS (MAO Tite)\"\nUBUNTU_CODENAME=resolute\n# comment\nID=ubuntu\n"));
    QCOMPARE(os.value(QStringLiteral("PRETTY_NAME")), QStringLiteral("Mainuan 2026 LTS"));
    QCOMPARE(os.value(QStringLiteral("VERSION")), QStringLiteral("2026 LTS (MAO Tite)"));
    QCOMPARE(os.value(QStringLiteral("ID")), QStringLiteral("ubuntu"));

    const auto cpu = SystemReportService::parseCpuInfo(QStringLiteral(
        "processor\t: 0\nmodel name\t: AMD Ryzen   5 5600X\nphysical id\t: 0\ncore id\t: 0\n\n"
        "processor\t: 1\nmodel name\t: AMD Ryzen 5 5600X\nphysical id\t: 0\ncore id\t: 0\n\n"
        "processor\t: 2\nphysical id\t: 0\ncore id\t: 1\n"));
    QCOMPARE(cpu.model, QStringLiteral("AMD Ryzen 5 5600X"));
    QCOMPARE(cpu.threads, 3);
    QCOMPARE(cpu.cores, 2);

    const auto memory = SystemReportService::parseMemInfo(QStringLiteral(
        "MemTotal:       16000000 kB\nMemFree: 1 kB\nMemAvailable:    9000000 kB\nSwapTotal: 2000 kB\nSwapFree: 500 kB\n"));
    QCOMPARE(memory.totalKiB, 16000000);
    QCOMPARE(memory.availableKiB, 9000000);
    QCOMPARE(memory.swapTotalKiB, 2000);
    QCOMPARE(memory.swapFreeKiB, 500);

    QCOMPARE(SystemReportService::countInstalledPackages(QStringLiteral(
        "Package: a\nStatus: install ok installed\n\nPackage: b\nStatus: deinstall ok config-files\n\n"
        "Package: c\nStatus: install ok installed\n")), 2);

    QCOMPARE(SystemReportService::maskMac(QStringLiteral("52:54:00:12:34:56\n")), QStringLiteral("52:54:00:xx:xx:xx"));
    QVERIFY(SystemReportService::maskMac(QStringLiteral("garbage")).isEmpty());
    QCOMPARE(SystemReportService::publicInterfaceName(QStringLiteral("enx001122334455")), QStringLiteral("enx001122xxxxxx"));
    QCOMPARE(SystemReportService::publicInterfaceName(QStringLiteral("wlp2s0")), QStringLiteral("wlp2s0"));
    QCOMPARE(SystemReportService::publicMountPoint(QStringLiteral("/")), QStringLiteral("/"));
    QCOMPARE(SystemReportService::publicMountPoint(QStringLiteral("/boot/efi")), QStringLiteral("/boot/efi"));
    QCOMPARE(SystemReportService::publicMountPoint(QStringLiteral("/home")), QStringLiteral("/home"));
    QVERIFY(SystemReportService::publicMountPoint(QStringLiteral("/media/ana/PENDRIVE")).isEmpty());
    QVERIFY(SystemReportService::publicMountPoint(QStringLiteral("/run/media/ana/Fotos")).isEmpty());
    QVERIFY(SystemReportService::publicMountPoint(QStringLiteral("/home/ana/dados")).isEmpty());

    QCOMPARE(SystemReportService::humanDuration(30), QStringLiteral("menos de um minuto"));
    QCOMPARE(SystemReportService::humanDuration(60), QStringLiteral("1 minuto"));
    QCOMPARE(SystemReportService::humanDuration(2 * 3600 + 18 * 60), QStringLiteral("2 horas e 18 minutos"));
    QCOMPARE(SystemReportService::humanDuration(86400 + 3 * 3600), QStringLiteral("1 dia e 3 horas"));

    const auto pci = SystemReportService::parseLspci(QStringLiteral(
        "00:01.0 \"VGA compatible controller\" \"Red Hat, Inc.\" \"Virtio 1.0 GPU\" -r01 \"Red Hat, Inc.\" \"QEMU\"\n"
        "01:00.0 \"3D controller\" \"NVIDIA Corporation\" \"GA107M\" -ra1\n"));
    QCOMPARE(pci.value(QStringLiteral("00:01.0")), QStringLiteral("Red Hat, Inc. Virtio 1.0 GPU"));
    QCOMPARE(pci.value(QStringLiteral("01:00.0")), QStringLiteral("NVIDIA Corporation GA107M"));
}

void MainuanTest::collectsSystemReportWithoutSecrets()
{
    QTemporaryDir root;
    const auto write = [&root](const QString &relative, const QByteArray &contents) {
        const QString file = root.filePath(relative);
        QDir().mkpath(QFileInfo(file).absolutePath());
        QFile out(file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(contents);
    };
    write(QStringLiteral("etc/os-release"), "PRETTY_NAME=\"Mainuan 2026 LTS\"\nVERSION=\"2026 LTS (MAO Tite)\"\nID=ubuntu\nUBUNTU_CODENAME=resolute\n");
    write(QStringLiteral("proc/cpuinfo"), "processor : 0\nmodel name : Test CPU\n");
    write(QStringLiteral("proc/meminfo"), "MemTotal: 8000000 kB\nMemAvailable: 6000000 kB\nSwapTotal: 0 kB\nSwapFree: 0 kB\n");
    write(QStringLiteral("proc/uptime"), "8280.55 1000.00\n");
    write(QStringLiteral("var/lib/dpkg/status"), "Package: a\nStatus: install ok installed\n\nPackage: b\nStatus: deinstall ok config-files\n");
    write(QStringLiteral("var/log/installer/media-info"), "Mainuan 2026 \"MAO Tite\" - Release amd64\n");
    write(QStringLiteral("boot/vmlinuz-7.0.0-30-generic"), "");
    write(QStringLiteral("sys/class/net/eth0/address"), "52:54:00:12:34:56\n");
    write(QStringLiteral("sys/class/net/eth0/operstate"), "up\n");
    write(QStringLiteral("sys/class/net/eth0/device/vendor"), "0x1af4\n");

    SystemReportService report(nullptr, root.path());
    QSignalSpy changed(&report, &SystemReportService::changed);
    report.refresh();
    QVERIFY(changed.count() == 1 || changed.wait(5000));
    QVERIFY(report.ready());
    QVERIFY(!report.busy());

    const QString text = report.reportText();
    QVERIFY(text.startsWith(QStringLiteral("Mainuan System Report\n")));
    QVERIFY(text.contains(QStringLiteral("Distribuição: Mainuan 2026 LTS")));
    QVERIFY(text.contains(QStringLiteral("Base: Ubuntu resolute")));
    QVERIFY(text.contains(QStringLiteral("Mídia de instalação: Mainuan 2026 \"MAO Tite\" - Release amd64")));
    QVERIFY(text.contains(QStringLiteral("Data da instalação: ")));        // from the installer log
    QVERIFY(!text.contains(QStringLiteral("Data estimada")));
    QVERIFY(text.contains(QStringLiteral("Processador: Test CPU")));
    QVERIFY(text.contains(QStringLiteral("APT / dpkg: 1 pacotes")));
    QVERIFY(text.contains(QStringLiteral("Ligado há: 2 horas e 18 minutos")));
    QVERIFY(text.contains(QStringLiteral("Kernels instalados: 7.0.0-30-generic")));
    QVERIFY(text.contains(QStringLiteral("Snap: Snap não instalado")));
    QVERIFY(text.contains(QStringLiteral("eth0: Ethernet")));
    QVERIFY(text.contains(QStringLiteral("MAC 52:54:00:xx:xx:xx")));
    QVERIFY(!text.contains(QStringLiteral("12:34:56")));
    QVERIFY(!text.contains(QStringLiteral("Nome do computador")));
    QVERIFY(!text.contains(QSysInfo::machineHostName()));
    QVERIFY(!text.contains(QRegularExpression(QStringLiteral("\\b\\d{1,3}(\\.\\d{1,3}){3}\\b"))));  // no IPv4 addresses

    const QVariantList sections = report.sections();
    QStringList ids;
    for (const QVariant &section : sections) {
        ids << section.toMap().value(QStringLiteral("id")).toString();
    }
    QCOMPARE(ids, (QStringList{QStringLiteral("system"), QStringLiteral("desktop"), QStringLiteral("hardware"),
                               QStringLiteral("graphics"), QStringLiteral("memory"), QStringLiteral("storage"),
                               QStringLiteral("packages"), QStringLiteral("boot"), QStringLiteral("network")}));
    QCOMPARE(report.summary().value(QStringLiteral("os")).toString(), QStringLiteral("Mainuan 2026 LTS"));
    QVERIFY(!report.summary().value(QStringLiteral("installEstimated")).toBool());

    QTemporaryDir out;
    const QString saved = out.filePath(QStringLiteral("mainuan-system-report.txt"));
    QVERIFY(report.saveReport(QUrl::fromLocalFile(saved)));
    QFile file(saved);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(file.readAll()), text);
    QVERIFY(!report.saveReport(QUrl(QStringLiteral("https://example.com/report.txt"))));

    // Without the installer log the date is only an estimate, and says so.
    QVERIFY(QFile::remove(root.filePath(QStringLiteral("var/log/installer/media-info"))));
    QVERIFY(QDir(root.filePath(QStringLiteral("var/log/installer"))).removeRecursively());
    report.refresh();
    QVERIFY(!report.reportText().contains(QStringLiteral("Data da instalação:")));
    QVERIFY(report.reportText().contains(QStringLiteral("Data estimada da instalação:")));
}

void MainuanTest::startupPreferenceDefaultsToShown()
{
    QTemporaryDir home;
    StartupPreference preference(nullptr, home.path());
    QVERIFY(preference.showAtStartup());
    QCOMPARE(QFileInfo(preference.overridePath()).fileName(), QStringLiteral("org.mainuan.Welcome.desktop"));
    QVERIFY(!QFileInfo::exists(preference.overridePath()));
}

void MainuanTest::startupPreferenceIsStoredPerUser()
{
    QTemporaryDir firstUser;
    QTemporaryDir secondUser;
    StartupPreference preference(nullptr, firstUser.path());
    QSignalSpy changed(&preference, &StartupPreference::showAtStartupChanged);

    preference.setShowAtStartup(false);
    QVERIFY(!preference.showAtStartup());
    QCOMPARE(changed.count(), 1);
    QFile file(preference.overridePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray contents = file.readAll();
    QVERIFY(contents.contains("[Desktop Entry]"));
    QVERIFY(contents.contains("Hidden=true"));

    // The choice survives a restart and does not affect another user.
    QVERIFY(!StartupPreference(nullptr, firstUser.path()).showAtStartup());
    QVERIFY(StartupPreference(nullptr, secondUser.path()).showAtStartup());

    preference.setShowAtStartup(true);
    QVERIFY(preference.showAtStartup());
    QVERIFY(!QFileInfo::exists(preference.overridePath()));
    QVERIFY(StartupPreference(nullptr, firstUser.path()).showAtStartup());
    QCOMPARE(changed.count(), 2);

    preference.setShowAtStartup(true); // no-op
    QCOMPARE(changed.count(), 2);
}

void MainuanTest::startupPreferenceReadsPlasmaOverrides()
{
    QTemporaryDir home;
    StartupPreference preference(nullptr, home.path());
    QDir().mkpath(QFileInfo(preference.overridePath()).absolutePath());
    const auto write = [&preference](const QByteArray &contents) {
        QFile file(preference.overridePath());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(contents);
    };

    write("[Desktop Entry]\nExec=mainuan-welcome\nHidden = TRUE\n");
    preference.reload();
    QVERIFY(!preference.showAtStartup());

    write("[Desktop Entry]\nExec=mainuan-welcome\nHidden=false\n[Desktop Action x]\nHidden=true\n");
    preference.reload();
    QVERIFY(preference.showAtStartup());

    write("[Desktop Entry]\nHiddenSomething=true\n");
    preference.reload();
    QVERIFY(preference.showAtStartup());
}

QTEST_GUILESS_MAIN(MainuanTest)
#include "tst_mainuan.moc"
