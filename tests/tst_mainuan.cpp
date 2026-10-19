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
#include "services/UserAvatar.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QImage>
#include <QBuffer>

#include <QtTest>

#include <QFile>
#include <QDesktopServices>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

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
        setFlatpaks(flatpaks);
        qputenv("MOCK_STATE_DIR", m_dir.path().toLocal8Bit());
    }

    void setFlatpaks(const QStringList &flatpaks)
    {
        QStringList lines;
        for (const QString &flatpak : flatpaks) {
            lines << flatpak + QStringLiteral("\tsystem");
        }
        writeLines(QStringLiteral("flatpak"), lines);
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

class UrlReceiver final : public QObject
{
    Q_OBJECT
public:
    QUrl received;
public slots:
    void receive(const QUrl &url) { received = url; }
};

class MainuanTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void acceptsOnlyKnownAccentIds();
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
    void detectsAppearanceWithoutChangingUserResources();
    void reportsPartialAccentFailures();
    void preservesAccentOverridesOnFailure();
    void backsUpAndSafelyReplacesAvatars();
    void rejectsUnsafeAvatarFilesAndWrites();
    void exposesModelRolesAndLocalVideoCatalog();
    void distinguishesManagedApplicationActions();
    void refreshesExternalFlatpaksAsynchronously();
    void opensSpecificDiscoverPagesAndHandlesErrors();
    void requestsExternalDownloadThroughDesktopServices();
    void opensIndependentDriverManagerVersions_data();
    void opensIndependentDriverManagerVersions();
    void reportsMissingOrBrokenDriverManager();

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

void MainuanTest::initTestCase()
{
    const auto bus = QDBusConnection::sessionBus();
    QVERIFY2(bus.isConnected(), "Run through CTest: appearance fixtures need a connected private D-Bus session.");
    const QDBusReply<bool> plasma = bus.interface()->isServiceRegistered(QStringLiteral("org.kde.plasmashell"));
    QVERIFY(plasma.isValid());
    QVERIFY2(!plasma.value(), "Tests must not connect to a real Plasma session.");
}

void MainuanTest::cleanup()
{
    qunsetenv("MOCK_SCENARIO");
    qunsetenv("MOCK_STATE_DIR");
    qunsetenv("MOCK_LIST_DELAY");
    qunsetenv("MOCK_DISCOVER_LOG");
    qunsetenv("MOCK_DISCOVER_EXIT");
}

void MainuanTest::acceptsOnlyKnownAccentIds()
{
    for (const QString &id : {QStringLiteral("brown"), QStringLiteral("cyan"), QStringLiteral("magenta"),
                             QStringLiteral("lime"), QStringLiteral("gray"), QStringLiteral("violet")}) {
        QVERIFY(SystemService::isValidAccent(id));
    }
    for (const QString &id : {QStringLiteral("#3daee9"), QStringLiteral("red"), QStringLiteral("CYAN"),
                             QStringLiteral("../cyan"), QStringLiteral("cyan; id"), QString()}) {
        QVERIFY(!SystemService::isValidAccent(id));
    }
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
    QCOMPARE(LayoutService::layoutIds().size(), 7);
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
    QVERIFY(description.layout.isEmpty()); // custom layout, not one of the seven
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
    service.apply(QStringLiteral("single-bar"));
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

    const QStringList ids = {"brown", "cyan", "magenta", "lime", "gray", "violet"};
    const QStringList names = {"Marrom", "Ciano", "Magenta", "Lima", "Cinza", "Violeta"};
    const QStringList stems = {"DreamBrown", "DreamCyan", "DreamMagenta", "DreamLime", "DreamGray", "DreamViolet"};
    const QStringList icons = {"kora-brown", "kora-cyan", "kora-magenta", "kora-lima", "kora-grey", "kora-violet"};
    const QStringList wallpapers = {"05marrom.png", "01ciano.png", "03magenta.png", "06lima.png", "02cinza.png", "04violet.png"};
    QCOMPARE(AccentProfiles::profiles().size(), 6);
    for (int i = 0; i < ids.size(); ++i) {
        const AccentProfile *profile = AccentProfiles::find(ids[i]);
        QVERIFY(profile);
        QCOMPARE(profile->name, names[i]);
        QCOMPARE(profile->lightScheme, stems[i] + "LightColor");
        QCOMPARE(profile->darkScheme, stems[i] + "DarkColor");
        QCOMPARE(profile->iconTheme, icons[i]);
        QCOMPARE(profile->wallpaper, wallpapers[i]);
        QCOMPARE(profile->avatar(), "icons/" + icons[i] + "/status/scalable/.face");
        QCOMPARE(AccentProfiles::fromScheme(profile->scheme(false)), profile);
        QCOMPARE(AccentProfiles::fromScheme(profile->scheme(true)), profile);
    }
    QVERIFY(!AccentProfiles::fromScheme(QStringLiteral("ExternalScheme")));
    QVERIFY(!AccentProfiles::find(QStringLiteral("#e8177d")));

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
        if (!m_root.isValid()) {
            qFatal("Cannot create an isolated appearance test directory");
        }
        for (const char *name : environmentNames()) {
            m_environment.insert(name, {qEnvironmentVariableIsSet(name), qgetenv(name)});
        }
        for (const VisualStyleProfile &profile : VisualStyles::profiles()) {
            for (const QString &package : {profile.lightPackage, profile.darkPackage}) {
                write(data() + "/plasma/look-and-feel/" + package + "/metadata.json", "{}");
            }
        }
        for (const AccentProfile &accent : AccentProfiles::profiles()) {
            write(data() + "/wallpapers/" + accent.wallpaper, "wallpaper fixture");
            write(data() + "/icons/" + accent.iconTheme + "/index.theme", "[Icon Theme]\nName=Kora\nDirectories=status/scalable\n");
            write(data() + "/" + accent.avatar(),
                  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\"><rect width=\"32\" height=\"32\" fill=\""
                  + accent.color.toUtf8() + "\"/></svg>");
            for (bool dark : {false, true}) {
                write(data() + "/color-schemes/" + accent.scheme(dark) + ".colors",
                    "[General]\nName=" + accent.name.toUtf8() + "\n[Colors:Window]\nBackgroundNormal="
                    + (dark ? QByteArray("30,30,46") : QByteArray("239,240,241"))
                    // Swatch values deliberately differ from the selection color.
                    + "\n[Colors:Selection]\nBackgroundNormal=17,99,155\n");
            }
        }
        for (const QString &icons : {QStringLiteral("breeze"), QStringLiteral("breeze-dark")}) {
            write(data() + "/icons/" + icons + "/index.theme", "[Icon Theme]\nName=Breeze\nDirectories=actions\n");
        }
        write(data() + "/icons/breeze_cursors/cursors/default", {});
        write(data() + "/icons/cyan-cursor/cursors/default", {});
        QDir().mkpath(config());
        QDir().mkpath(home());
        QFile::setPermissions(home(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        qputenv("XDG_CONFIG_HOME", config().toUtf8());
        qputenv("XDG_CONFIG_DIRS", m_root.filePath("xdg").toUtf8());
        qputenv("XDG_DATA_HOME", m_root.filePath("home-data").toUtf8());
        qputenv("XDG_DATA_DIRS", data().toUtf8());
        qputenv("MOCK_LOG", log().toUtf8());
    }
    ~PlasmaSandbox()
    {
        for (auto it = m_environment.cbegin(); it != m_environment.cend(); ++it) {
            if (it.value().first) qputenv(it.key().constData(), it.value().second);
            else qunsetenv(it.key().constData());
        }
    }
    QString config() const { return m_root.filePath("config"); }
    QString home() const { return m_root.filePath("home"); }
    QString log() const { return m_root.filePath("calls.log"); }
    QString data() const { return m_root.filePath("data"); }
    static QString tools() { return QStringLiteral(MAINUAN_TEST_FIXTURES "/plasma"); }
    static void write(const QString &path, const QByteArray &bytes)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(bytes), bytes.size());
    }
    static QByteArray read(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }
    void appearance(const QString &scheme, const QString &icons = "breeze", const QString &style = "org.kde.breeze.desktop")
    {
        write(config() + "/kdeglobals", "[General]\nColorScheme=" + scheme.toUtf8()
              + "\n[Icons]\nTheme=" + icons.toUtf8() + "\n[KDE]\nLookAndFeelPackage=" + style.toUtf8()
              + "\n[Colors:Window]\nBackgroundNormal=" + (scheme.contains("Dark") ? QByteArray("30,30,46") : QByteArray("239,240,241")) + "\n");
    }
    QStringList takeCalls() const
    {
        const QStringList calls = QString::fromUtf8(read(log())).split('\n', Qt::SkipEmptyParts);
        QFile::remove(log());
        return calls;
    }
private:
    static QList<const char *> environmentNames()
    {
        return {"XDG_CONFIG_HOME", "XDG_CONFIG_DIRS", "XDG_DATA_HOME", "XDG_DATA_DIRS", "MOCK_LOG",
                "PATH", "MOCK_DRIVERS_LOG", "WAYLAND_DISPLAY", "DISPLAY", "XDG_SESSION_TYPE",
                "MOCK_LNF_FAIL", "MOCK_LNF_NOOP", "MOCK_LNF_DELAY", "MOCK_WALLPAPER_FAIL", "MOCK_ICONS_FAIL",
                "MOCK_ICONS_NOOP", "MOCK_SCHEME_FAIL", "MOCK_SCHEME_NOOP", "MOCK_SCHEME_HIDDEN"};
    }
    QTemporaryDir m_root;
    QHash<QByteArray, QPair<bool, QByteArray>> m_environment;
};

bool waitIdle(const SystemService &service)
{
    return QTest::qWaitFor([&service]() { return !service.busy(); }, 10000);
}

} // namespace

void MainuanTest::opensIndependentDriverManagerVersions_data()
{
    QTest::addColumn<QByteArray>("toolVersion");
    QTest::newRow("compatible-tool-1") << QByteArray("1.0");
    QTest::newRow("compatible-tool-2") << QByteArray("2.4");
}

void MainuanTest::opensIndependentDriverManagerVersions()
{
    QFETCH(QByteArray, toolVersion);
    PlasmaSandbox sandbox;
    const QString bin = sandbox.home() + "/bin";
    const QString executable = bin + "/mainuan-gestao-drivers";
    PlasmaSandbox::write(executable,
        "#!/bin/bash\nprintf '%s\\n' '" + toolVersion
        + "' \"$#\" \"$UID\" \"$EUID\" \"$WAYLAND_DISPLAY\" \"$DISPLAY\" \"$XDG_SESSION_TYPE\" "
          "\"$XDG_RUNTIME_DIR\" \"$DBUS_SESSION_BUS_ADDRESS\" >\"$MOCK_DRIVERS_LOG\"\n");
    QVERIFY(QFile::setPermissions(executable, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    for (const QString &other : {QString("kubuntu-driver-manager"), QString("software-properties-qt"),
         QString("plasma-discover"), QString("pkexec"), QString("sudo")}) {
        const QString rejected = bin + '/' + other;
        PlasmaSandbox::write(rejected, "#!/bin/bash\nprintf 'unexpected launcher\\n' >\"$MOCK_DRIVERS_LOG\"\n");
        QVERIFY(QFile::setPermissions(rejected, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    }
    qputenv("PATH", bin.toUtf8());
    qputenv("MOCK_DRIVERS_LOG", sandbox.log().toUtf8());
    qputenv("WAYLAND_DISPLAY", "mainuan-test-wayland");
    qputenv("DISPLAY", "mainuan-test-display");
    qputenv("XDG_SESSION_TYPE", "wayland");
    SystemService service(nullptr, sandbox.home());
    QVERIFY(service.driversAvailable());
    QCOMPARE(service.driversStatus(), QStringLiteral("Gestão de Drivers disponível"));
    QVERIFY(!QFile::exists(sandbox.log())); // Opening Welcome does not launch the tool.
    service.performAction(QStringLiteral("drivers"));
    QByteArray expected = toolVersion + "\n0\n" + QByteArray::number(getuid()) + '\n' + QByteArray::number(geteuid()) + '\n';
    for (const char *key : {"WAYLAND_DISPLAY", "DISPLAY", "XDG_SESSION_TYPE", "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS"}) {
        expected += qgetenv(key) + '\n';
    }
    // No version probe, command-line options, elevation or session replacement.
    QTRY_COMPARE(PlasmaSandbox::read(sandbox.log()), expected);
    QVERIFY(!service.lastMessage().startsWith(QStringLiteral("Erro:")));
}

void MainuanTest::reportsMissingOrBrokenDriverManager()
{
    PlasmaSandbox sandbox;
    const QString bin = sandbox.home() + "/bin";
    const QString legacy = bin + "/kubuntu-driver-manager";
    PlasmaSandbox::write(legacy, "#!/bin/bash\nprintf 'legacy called\\n' >\"$MOCK_DRIVERS_LOG\"\n");
    QVERIFY(QFile::setPermissions(legacy, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    // A desktop entry alone cannot make an absent executable available.
    PlasmaSandbox::write(sandbox.data() + "/applications/org.mainuan.GestaoDrivers.desktop",
        "[Desktop Entry]\nType=Application\nExec=mainuan-gestao-drivers\nName=Gestão de Drivers\n");
    qputenv("PATH", bin.toUtf8());
    qputenv("MOCK_DRIVERS_LOG", sandbox.log().toUtf8());
    SystemService service(nullptr, sandbox.home());
    QVERIFY(!service.driversAvailable());
    QCOMPARE(service.driversStatus(), QStringLiteral("Gestão de Drivers não instalada"));
    service.performAction(QStringLiteral("drivers"));
    QVERIFY(service.lastMessage().contains(QStringLiteral("Gestão de Drivers não está instalada")));
    QVERIFY(!QFile::exists(sandbox.log()));
    const QString executable = bin + "/mainuan-gestao-drivers";
    PlasmaSandbox::write(executable, "#!/nonexistent/mainuan-drivers-interpreter\n");
    QVERIFY(QFile::setPermissions(executable, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    service.refresh();
    QVERIFY(service.driversAvailable());
    service.performAction(QStringLiteral("drivers"));
    QVERIFY(service.lastMessage().contains(QStringLiteral("Não foi possível abrir a Gestão de Drivers")));
    QVERIFY(QFile::remove(executable));
    service.refresh();
    QVERIFY(!service.driversAvailable());
    QVERIFY(!QFile::exists(sandbox.log()));
}

void MainuanTest::appliesVisualStyleProfiles()
{
    PlasmaSandbox sandbox;
    sandbox.appearance("DreamBrownDarkColor", "kora-brown");
    SystemService service(nullptr, sandbox.home());
    service.setToolSearchPaths({PlasmaSandbox::tools()});
    QCOMPARE(service.accentId(), QStringLiteral("brown"));
    QCOMPARE(service.iconTheme(), QStringLiteral("kora-brown"));
    QCOMPARE(service.accentProfiles().size(), 6);
    for (const QString &style : {QStringLiteral("blur"), QStringLiteral("glass"), QStringLiteral("solid"), QStringLiteral("blur")}) {
        qputenv("MOCK_LNF_DELAY", "0.05");
        service.setVisualStyle(style);
        QVERIFY(service.busy());
        service.setAccent("cyan"); // Must not race the theme application.
        QVERIFY(waitIdle(service));
        QCOMPARE(service.visualStyle(), style);
        QVERIFY(service.darkTheme());
        QCOMPARE(service.accentId(), QStringLiteral("brown"));
        QCOMPARE(service.colorScheme(), QStringLiteral("DreamBrownDarkColor"));
        QCOMPARE(service.iconTheme(), QStringLiteral("kora-brown"));
        const QStringList calls = sandbox.takeCalls();
        const int theme = calls.indexOf("lookandfeel " + VisualStyles::find(style)->darkPackage);
        const int scheme = calls.indexOf("plasma-apply-colorscheme DreamBrownDarkColor");
        const int icons = calls.indexOf("plasma-changeicons kora-brown");
        QVERIFY(theme >= 0 && scheme > theme && icons > scheme);
        QVERIFY(!calls.contains(QStringLiteral("plasma-apply-colorscheme DreamCyanDarkColor")));
        QVERIFY2(!service.lastMessage().startsWith("Erro:"), qPrintable(service.lastMessage()));
        QCOMPARE(PlasmaSandbox::read(sandbox.home() + "/.face"), PlasmaSandbox::read(sandbox.data() + "/" + AccentProfiles::find("brown")->avatar()));
    }
    SystemService reopened(nullptr, sandbox.home());
    QCOMPARE(reopened.visualStyle(), QStringLiteral("blur"));
    QCOMPARE(reopened.accentId(), QStringLiteral("brown"));
    QCOMPARE(reopened.iconTheme(), QStringLiteral("kora-brown"));

    service.setAccent("violet");
    QVERIFY(waitIdle(service));
    sandbox.takeCalls();
    for (const QString &style : {QStringLiteral("glass"), QStringLiteral("solid"), QStringLiteral("blur")}) {
        service.setVisualStyle(style);
        QVERIFY(waitIdle(service));
        QCOMPARE(service.accentId(), QStringLiteral("violet"));
        QVERIFY(sandbox.takeCalls().contains("plasma-apply-wallpaperimage " + sandbox.data() + "/wallpapers/04violet.png"));
    }
}

void MainuanTest::reportsVisualStyleFailures()
{
    PlasmaSandbox sandbox;
    sandbox.appearance("DreamVioletLightColor", "kora-violet");
    SystemService service(nullptr, sandbox.home());
    service.setToolSearchPaths({PlasmaSandbox::tools()});
    service.setVisualStyle("unknown");
    QVERIFY(!service.busy());
    QVERIFY(service.lastMessage().contains("inválido"));
    qputenv("MOCK_LNF_FAIL", "1");
    service.setVisualStyle("blur");
    QVERIFY(waitIdle(service));
    QCOMPARE(service.visualStyle(), QStringLiteral("solid"));
    QVERIFY(service.lastMessage().contains("Não foi possível aplicar o estilo Dream"));
    qunsetenv("MOCK_LNF_FAIL");
    qputenv("MOCK_LNF_NOOP", "1");
    service.setVisualStyle("glass");
    QVERIFY(waitIdle(service));
    QVERIFY(service.lastMessage().contains("não confirmou o estilo Tahoe"));
    qunsetenv("MOCK_LNF_NOOP");
    QVERIFY(QFile::remove(sandbox.data() + "/color-schemes/DreamVioletDarkColor.colors"));
    sandbox.takeCalls();
    service.setTheme("dark");
    QVERIFY(waitIdle(service));
    QVERIFY(service.lastMessage().contains("DreamVioletDarkColor.colors"));
    QCOMPARE(service.colorScheme(), QStringLiteral("DreamVioletLightColor"));
    QVERIFY(sandbox.takeCalls().isEmpty());
    QVERIFY(QFile::remove(sandbox.data() + "/icons/kora-violet/index.theme"));
    service.setVisualStyle("blur");
    QVERIFY(!service.busy());
    QCOMPARE(service.iconTheme(), QStringLiteral("kora-violet"));
    QVERIFY(service.lastMessage().contains("ícones kora-violet"));
    QVERIFY(QFile::remove(sandbox.data() + "/plasma/look-and-feel/org.kde.breeze.desktop/metadata.json"));
    service.setVisualStyle("solid");
    QVERIFY(!service.busy());
    QVERIFY(service.lastMessage().contains("não está instalado"));
    QTemporaryDir noTools;
    service.setToolSearchPaths({noTools.path()});
    service.setVisualStyle("glass");
    QVERIFY(!service.busy());
    QVERIFY(service.lastMessage().contains("plasma-apply-lookandfeel"));
}

void MainuanTest::switchesThemeWithinTheActiveStyle()
{
    PlasmaSandbox sandbox;
    SystemService service(nullptr, sandbox.home());
    service.setToolSearchPaths({PlasmaSandbox::tools()});
    for (const QString &style : {QStringLiteral("blur"), QStringLiteral("glass"), QStringLiteral("solid")}) {
        service.setVisualStyle(style);
        QVERIFY(waitIdle(service));
        for (const QString &id : {QStringLiteral("cyan"), QStringLiteral("violet")}) {
            service.setAccent(id);
            QVERIFY(waitIdle(service));
            for (const QString &variant : {QStringLiteral("dark"), QStringLiteral("light")}) {
                service.setTheme(variant);
                QVERIFY(waitIdle(service));
                QCOMPARE(service.visualStyle(), style);
                QCOMPARE(service.darkTheme(), variant == "dark");
                QCOMPARE(service.accentId(), id);
                QCOMPARE(service.colorScheme(), AccentProfiles::find(id)->scheme(variant == "dark"));
                QCOMPARE(service.iconTheme(), AccentProfiles::find(id)->iconTheme);
                QVERIFY(!service.lastMessage().startsWith("Erro:"));
            }
        }
    }
}

void MainuanTest::recordsTheChosenAccentColor()
{
    PlasmaSandbox sandbox;
    SystemService service(nullptr, sandbox.home());
    service.setToolSearchPaths({PlasmaSandbox::tools()});
    for (const AccentProfile &profile : AccentProfiles::profiles()) {
        for (bool dark : {false, true}) {
            sandbox.appearance(profile.scheme(dark));
            service.refresh();
            service.setAccent(profile.id);
            QVERIFY(waitIdle(service));
            QCOMPARE(service.accentId(), profile.id);
            QCOMPARE(service.colorScheme(), profile.scheme(dark));
            QCOMPARE(service.iconTheme(), profile.iconTheme);
            QCOMPARE(service.accentColor(), QStringLiteral("#11639b")); // Identity does not come from the hex swatch.
            const QStringList expected = {"plasma-apply-colorscheme " + profile.scheme(dark),
                "plasma-changeicons " + profile.iconTheme,
                "plasma-apply-wallpaperimage " + sandbox.data() + "/wallpapers/" + profile.wallpaper};
            QCOMPARE(sandbox.takeCalls(), expected);
            QVERIFY(!PlasmaSandbox::read(sandbox.config() + "/kdeglobals").contains("AccentColor="));
            QCOMPARE(PlasmaSandbox::read(sandbox.home() + "/.face"), PlasmaSandbox::read(sandbox.data() + "/" + profile.avatar()));
            QVERIFY(!service.lastMessage().startsWith("Erro:"));
            SystemService reopened(nullptr, sandbox.home());
            QCOMPARE(reopened.accentId(), profile.id);
        }
    }
    service.setAccent("red; rm -rf ~");
    QVERIFY(!service.busy());
    QVERIFY(sandbox.takeCalls().isEmpty());
}

void MainuanTest::detectsAppearanceWithoutChangingUserResources()
{
    PlasmaSandbox sandbox;
    sandbox.appearance("ExternalScheme", "custom-icons", "org.example.custom");
    const QString globals = sandbox.config() + "/kdeglobals";
    PlasmaSandbox::write(globals, PlasmaSandbox::read(globals) + "\n[General]\nAccentColor=232,23,125\nCustomPreference=keep\n");
    const QByteArray before = PlasmaSandbox::read(globals);
    PlasmaSandbox::write(sandbox.home() + "/.face", "custom avatar");
    SystemService service(nullptr, sandbox.home());
    service.setToolSearchPaths({PlasmaSandbox::tools()});
    QVERIFY(service.accentId().isEmpty()); // Even a matching legacy hex cannot select a profile.
    QCOMPARE(service.iconTheme(), QStringLiteral("custom-icons"));
    QCOMPARE(PlasmaSandbox::read(globals), before);
    QCOMPARE(PlasmaSandbox::read(sandbox.home() + "/.face"), QByteArray("custom avatar"));
    QVERIFY(!QFileInfo::exists(sandbox.home() + "/.face.mainuan-welcome-backup"));
    QVERIFY(sandbox.takeCalls().isEmpty());
    service.setTheme("dark");
    QVERIFY(!service.busy());
    QVERIFY(service.lastMessage().contains("esquema externo"));
    QCOMPARE(PlasmaSandbox::read(globals), before);
    // A known Dream scheme is recognized without copying anything on startup.
    for (const AccentProfile &profile : AccentProfiles::profiles()) {
        sandbox.appearance(profile.darkScheme, profile.iconTheme);
        service.refresh();
        QCOMPARE(service.accentId(), profile.id);
        QCOMPARE(service.iconTheme(), profile.iconTheme);
        QCOMPARE(PlasmaSandbox::read(sandbox.home() + "/.face"), QByteArray("custom avatar"));
    }
}

void MainuanTest::reportsPartialAccentFailures()
{
    PlasmaSandbox sandbox;
    sandbox.appearance("BreezeLight", "breeze");
    SystemService service(nullptr, sandbox.home());
    service.setToolSearchPaths({PlasmaSandbox::tools()});
    const QString index = sandbox.data() + "/icons/kora-magenta/index.theme";
    QVERIFY(QFile::remove(index));
    service.setAccent("magenta");
    QVERIFY(waitIdle(service));
    QCOMPARE(service.colorScheme(), QStringLiteral("DreamMagentaLightColor"));
    QCOMPARE(service.iconTheme(), QStringLiteral("breeze"));
    QVERIFY(service.lastMessage().contains("ícones kora-magenta"));
    QVERIFY(!sandbox.takeCalls().contains("plasma-changeicons kora-magenta"));
    QVERIFY(QFileInfo::exists(sandbox.home() + "/.face"));
    PlasmaSandbox::write(index, "[Icon Theme]\nName=Broken\n");
    service.setAccent("magenta");
    QVERIFY(waitIdle(service));
    QCOMPARE(service.iconTheme(), QStringLiteral("breeze"));
    QVERIFY(service.lastMessage().contains("index.theme inválido"));
    QVERIFY(QFile::remove(sandbox.data() + "/wallpapers/01ciano.png"));
    service.setAccent("cyan");
    QVERIFY(waitIdle(service));
    QCOMPARE(service.colorScheme(), QStringLiteral("DreamCyanLightColor"));
    QCOMPARE(service.iconTheme(), QStringLiteral("kora-cyan"));
    QVERIFY(service.lastMessage().contains("papel de parede 01ciano.png ausente"));
    qputenv("MOCK_WALLPAPER_FAIL", "1");
    service.setAccent("violet");
    QVERIFY(waitIdle(service));
    QCOMPARE(service.accentId(), QStringLiteral("violet"));
    QVERIFY(service.lastMessage().startsWith("Erro:"));
    QVERIFY(service.lastMessage().contains("papel de parede"));
    qunsetenv("MOCK_WALLPAPER_FAIL");
    QVERIFY(QFile::remove(sandbox.data() + "/" + AccentProfiles::find("brown")->avatar()));
    service.setAccent("brown");
    QVERIFY(waitIdle(service));
    QCOMPARE(service.colorScheme(), QStringLiteral("DreamBrownLightColor"));
    QVERIFY(service.lastMessage().contains("avatar:"));
    QCOMPARE(PlasmaSandbox::read(sandbox.home() + "/.face"), PlasmaSandbox::read(sandbox.data() + "/" + AccentProfiles::find("violet")->avatar()));
    qputenv("MOCK_ICONS_NOOP", "1");
    service.setAccent("gray");
    QVERIFY(waitIdle(service));
    QCOMPARE(service.iconTheme(), QStringLiteral("kora-brown"));
    QVERIFY(service.lastMessage().contains("não confirmou os ícones kora-grey"));
}

void MainuanTest::preservesAccentOverridesOnFailure()
{
    PlasmaSandbox sandbox;
    sandbox.appearance("DreamCyanLightColor", "breeze");
    const QString globals = sandbox.config() + "/kdeglobals";
    PlasmaSandbox::write(globals, PlasmaSandbox::read(globals) + "\n[General]\nAccentColor=232,23,125\nCustomPreference=keep\n");
    PlasmaSandbox::write(sandbox.home() + "/.face", "old avatar");
    SystemService service(nullptr, sandbox.home());
    service.setToolSearchPaths({PlasmaSandbox::tools()});
    qputenv("MOCK_SCHEME_HIDDEN", "DreamBrownLightColor");
    service.setAccent("brown");
    QVERIFY(waitIdle(service));
    QVERIFY(service.lastMessage().contains("não reconheceu"));
    QCOMPARE(service.colorScheme(), QStringLiteral("DreamCyanLightColor"));
    qunsetenv("MOCK_SCHEME_HIDDEN");
    for (const char *flag : {"MOCK_SCHEME_FAIL", "MOCK_SCHEME_NOOP"}) {
        qputenv(flag, "1");
        service.setAccent("brown");
        QVERIFY(waitIdle(service));
        QCOMPARE(service.colorScheme(), QStringLiteral("DreamCyanLightColor"));
        const KConfigGroup general(KSharedConfig::openConfig("kdeglobals"), "General");
        QCOMPARE(general.readEntry("AccentColor", QString()), QStringLiteral("232,23,125"));
        QCOMPARE(general.readEntry("CustomPreference", QString()), QStringLiteral("keep"));
        QCOMPARE(PlasmaSandbox::read(sandbox.home() + "/.face"), QByteArray("old avatar"));
        QVERIFY(service.lastMessage().startsWith("Erro:"));
        qunsetenv(flag);
    }
    // Same-scheme selection must fully reload, clearing the old tint.
    service.setAccent("cyan");
    QVERIFY(waitIdle(service));
    QVERIFY2(!service.lastMessage().startsWith("Erro:"), qPrintable(service.lastMessage()));
    QVERIFY(!PlasmaSandbox::read(globals).contains("AccentColor="));
    QVERIFY(PlasmaSandbox::read(globals).contains("CustomPreference=keep"));
    QCOMPARE(service.accentColor(), QStringLiteral("#11639b"));
    // An inherited custom tint must not leak into the chosen scheme either.
    const QString defaults = QDir(qEnvironmentVariable("XDG_CONFIG_DIRS")).filePath("kdeglobals");
    PlasmaSandbox::write(defaults, "[General]\nAccentColor=255,0,0\n");
    service.setAccent("magenta");
    QVERIFY(waitIdle(service));
    QVERIFY2(!service.lastMessage().startsWith("Erro:"), qPrintable(service.lastMessage()));
    QCOMPARE(PlasmaSandbox::read(defaults), QByteArray("[General]\nAccentColor=255,0,0\n"));
    QVERIFY(!PlasmaSandbox::read(globals).contains("AccentColor=255,0,0"));
}

void MainuanTest::backsUpAndSafelyReplacesAvatars()
{
    PlasmaSandbox sandbox;
    const QString face = sandbox.home() + "/.face";
    const QString backup = sandbox.home() + "/.face.mainuan-welcome-backup";
    PlasmaSandbox::write(face, "original personal avatar bytes");
    QVERIFY(QFile::setPermissions(face, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    QVERIFY(QFile::link(face, sandbox.home() + "/.face.icon"));
    for (const AccentProfile &profile : AccentProfiles::profiles()) {
        const QString source = sandbox.data() + "/" + profile.avatar();
        QVERIFY2(UserAvatar::update(source, sandbox.home()).isEmpty(), qPrintable(profile.id));
        QCOMPARE(PlasmaSandbox::read(face), PlasmaSandbox::read(source));
        QCOMPARE(PlasmaSandbox::read(backup), QByteArray("original personal avatar bytes"));
        QVERIFY(QFileInfo(sandbox.home() + "/.face.icon").isSymLink());
        QCOMPARE(QFileInfo(sandbox.home() + "/.face.icon").symLinkTarget(), face);
        QVERIFY(!(QFile::permissions(face) & (QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::WriteGroup | QFileDevice::WriteOther)));
        QVERIFY(!(QFile::permissions(backup) & (QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::WriteGroup | QFileDevice::WriteOther)));
    }
    // Original PNG bytes and independent .face.icon files remain unchanged.
    const QString png = sandbox.data() + "/avatar.png";
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(Qt::cyan);
    QVERIFY(image.save(png, "PNG"));
    QVERIFY(QFile::remove(sandbox.home() + "/.face.icon"));
    PlasmaSandbox::write(sandbox.home() + "/.face.icon", "independent avatar");
    QVERIFY(UserAvatar::update(png, sandbox.home()).isEmpty());
    QCOMPARE(PlasmaSandbox::read(face), PlasmaSandbox::read(png));
    QCOMPARE(PlasmaSandbox::read(sandbox.home() + "/.face.icon"), QByteArray("independent avatar"));
    QVERIFY(QFile::remove(sandbox.home() + "/.face.icon"));
    QVERIFY(UserAvatar::update(png, sandbox.home()).isEmpty());
    QCOMPARE(QFileInfo(sandbox.home() + "/.face.icon").symLinkTarget(), face);
    QVERIFY(QDir(sandbox.home()).entryList({".mainuan-avatar-*"}, QDir::Files | QDir::Hidden).isEmpty());
}

void MainuanTest::rejectsUnsafeAvatarFilesAndWrites()
{
    PlasmaSandbox sandbox;
    const QString source = sandbox.data() + "/" + AccentProfiles::find("cyan")->avatar();
    const QString face = sandbox.home() + "/.face";
    const QString external = sandbox.home() + "/external";
    PlasmaSandbox::write(external, "must remain unchanged");
    QVERIFY(QFile::link(external, face));
    QVERIFY(!UserAvatar::update(source, sandbox.home()).isEmpty());
    QCOMPARE(PlasmaSandbox::read(external), QByteArray("must remain unchanged"));
    QVERIFY(QFile::remove(face));
    PlasmaSandbox::write(face, "existing avatar");
    const QString backup = sandbox.home() + "/.face.mainuan-welcome-backup";
    QVERIFY(QFile::link(external, backup));
    QVERIFY(!UserAvatar::update(source, sandbox.home()).isEmpty());
    QCOMPARE(PlasmaSandbox::read(face), QByteArray("existing avatar"));
    QVERIFY(QFile::remove(backup));
    QVERIFY(!UserAvatar::update(sandbox.data() + "/missing", sandbox.home()).isEmpty());
    const QString invalid = sandbox.data() + "/invalid";
    PlasmaSandbox::write(invalid, "not an image");
    QVERIFY(!UserAvatar::update(invalid, sandbox.home()).isEmpty());
    const QString alias = sandbox.data() + "/alias";
    QVERIFY(QFile::link(source, alias));
    QVERIFY(!UserAvatar::update(alias, sandbox.home()).isEmpty());
    QVERIFY(QFile::setPermissions(source, {}));
    QVERIFY(!UserAvatar::update(source, sandbox.home()).isEmpty());
    QVERIFY(QFile::setPermissions(source, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    QVERIFY(QFile::setPermissions(sandbox.home(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    QVERIFY(!UserAvatar::update(source, sandbox.home()).isEmpty());
    QCOMPARE(PlasmaSandbox::read(face), QByteArray("existing avatar"));
    QVERIFY(!QFileInfo::exists(backup));
    QVERIFY(QFile::setPermissions(sandbox.home(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    const QString homeAlias = sandbox.data() + "/home-alias";
    QVERIFY(QFile::link(sandbox.home(), homeAlias));
    QVERIFY(!UserAvatar::update(source, homeAlias).isEmpty());
    QVERIFY(QFile::setPermissions(sandbox.home(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::WriteOther));
    QVERIFY(!UserAvatar::update(source, sandbox.home()).isEmpty());
    QFile::setPermissions(sandbox.home(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QCOMPARE(PlasmaSandbox::read(external), QByteArray("must remain unchanged"));
}

void MainuanTest::exposesModelRolesAndLocalVideoCatalog()
{
    PackageService packageService;
    const ApplicationEntry onlineEntry{
        QStringLiteral("gdocs"), QStringLiteral("Documentos"), QStringLiteral("Online"),
        QStringLiteral("webapp.gdocs"), QStringLiteral("qrc:/assets/webapp.gdocs.png"),
        QStringLiteral("https://docs.google.com/"), ApplicationAction::WebPage};
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

void MainuanTest::distinguishesManagedApplicationActions()
{
    MockSystem system({}, {});
    PackageService packages(nullptr, QStringLiteral("/nonexistent/flatpak"));
    InstallService installer(&packages, mockBackend());
    ApplicationModel model(&packages, {
        {"freeoffice", "FreeOffice", {}, "softmaker-freeoffice-2024", "qrc:/assets/freeOffice.png", {}, ApplicationAction::InternalDebian},
        {"vivaldi", "Vivaldi", {}, "com.vivaldi.Vivaldi", {}, {}},
        {"zen", "Zen Browser", {}, "app.zen_browser.zen", {}, {}},
        {"libreoffice", "LibreOffice", {}, "org.libreoffice.LibreOffice", {}, {}}
    });
    model.setInstallService(&installer);
    QVERIFY(waitForDetection(installer));
    QVERIFY(model.data(model.index(0), ApplicationModel::AvailableRole).toBool());
    QCOMPARE(model.data(model.index(0), ApplicationModel::ActionTypeRole).toString(), QStringLiteral("debian"));
    QCOMPARE(model.data(model.index(0), ApplicationModel::InstallProfileRole).toString(), QStringLiteral("freeoffice"));
    QVERIFY(!model.remove(0));
    for (const QString &id : {QStringLiteral("com.vivaldi.Vivaldi"), QStringLiteral("app.zen_browser.zen")}) {
        QVERIFY(PackageService::isAllowedApplication(id));
        QVERIFY(installer.profiles().contains(QStringLiteral("app:") + id));
        QVERIFY(!installer.registerApplication(id, id, {}, {})); // No duplicate profiles.
    }
    QVERIFY(installer.profiles().contains(QStringLiteral("app:org.libreoffice.LibreOffice")));
    for (int row = 1; row < 4; ++row) {
        QVERIFY(!model.data(model.index(row), ApplicationModel::AvailableRole).toBool());
        QCOMPARE(model.data(model.index(row), ApplicationModel::ActionTypeRole).toString(), QStringLiteral("flatpak"));
    }
}

void MainuanTest::refreshesExternalFlatpaksAsynchronously()
{
    MockSystem system({}, {});
    qputenv("MOCK_LIST_DELAY", "0.1");
    PackageService packages(nullptr, fixture(QStringLiteral("flatpak")));
    ApplicationModel model(&packages, {
        {"vivaldi", "Vivaldi", {}, "com.vivaldi.Vivaldi", {}, {}, ApplicationAction::InternalFlatpak},
        {"zen", "Zen Browser", {}, "app.zen_browser.zen", {}, {}, ApplicationAction::InternalFlatpak}
    });
    QSignalSpy changed(&packages, &PackageService::installedApplicationsChanged);
    bool responsive = false;
    QTimer::singleShot(0, &packages, [&responsive]() { responsive = true; });
    packages.refresh(); // A return to Welcome during an ongoing query queues another query.
    QTRY_VERIFY(responsive);
    QTRY_VERIFY(changed.count() >= 2);
    QVERIFY(!model.data(model.index(0), ApplicationModel::InstalledRole).toBool());
    system.setFlatpaks({QStringLiteral("com.vivaldi.Vivaldi"), QStringLiteral("app.zen_browser.zen")});
    packages.refresh();
    QTRY_VERIFY(model.data(model.index(0), ApplicationModel::InstalledRole).toBool());
    QTRY_VERIFY(model.data(model.index(1), ApplicationModel::InstalledRole).toBool());
    QCOMPARE(model.data(model.index(1), ApplicationModel::InstallStateRole).toString(), QStringLiteral("Instalado"));
    system.setFlatpaks({});
    packages.refresh();
    QTRY_VERIFY(!model.data(model.index(0), ApplicationModel::InstalledRole).toBool());
    QTRY_VERIFY(!model.data(model.index(1), ApplicationModel::InstalledRole).toBool());
}

void MainuanTest::opensSpecificDiscoverPagesAndHandlesErrors()
{
    QTemporaryDir temp;
    SystemService service;
    PackageService packages(nullptr, QStringLiteral("/nonexistent/flatpak"));
    ApplicationModel model(&packages, {
        {"zen", "Zen Browser", {}, "app.zen_browser.zen", {}, {}, ApplicationAction::InternalFlatpak}
    });
    service.setToolSearchPaths({temp.path()});
    QVERIFY(!service.openDiscover(QStringLiteral("com.vivaldi.Vivaldi")));
    QVERIFY(service.lastMessage().contains(QStringLiteral("Discover não foi encontrado")));
    service.setToolSearchPaths({fixture(QStringLiteral("discover"))});
    QVERIFY(!service.openDiscover(QStringLiteral("com.vivaldi.Vivaldi; touch /tmp/untrusted")));
    const QString log = temp.filePath(QStringLiteral("arguments"));
    qputenv("MOCK_DISCOVER_LOG", log.toUtf8());
    for (const QString &id : {QStringLiteral("com.vivaldi.Vivaldi"), QStringLiteral("app.zen_browser.zen")}) {
        QVERIFY(service.openDiscover(id));
        const QByteArray expected = "--application\nappstream://" + id.toUtf8() + '\n';
        QTRY_VERIFY([&]() {
            QFile file(log);
            return file.open(QIODevice::ReadOnly) && file.readAll() == expected;
        }());
    }
    qputenv("MOCK_DISCOVER_EXIT", "1");
    QFile::remove(log);
    QVERIFY(service.openDiscover(QStringLiteral("app.zen_browser.zen")));
    QTRY_VERIFY(QFile::exists(log));
    QVERIFY(!model.data(model.index(0), ApplicationModel::InstalledRole).toBool());
    QCOMPARE(model.data(model.index(0), ApplicationModel::InstallStateRole).toString(), QStringLiteral("Indisponível"));
    QFile::remove(log);
    QVERIFY(service.openDiscover(QStringLiteral("app.zen_browser.zen"))); // Retry stays available.
    QTRY_VERIFY(QFile::exists(log));
    // An executable with a missing interpreter passes discovery but cannot start.
    QFile broken(temp.filePath(QStringLiteral("plasma-discover")));
    QVERIFY(broken.open(QIODevice::WriteOnly));
    broken.write("#!/nonexistent/discover-interpreter\n");
    broken.close();
    QVERIFY(broken.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    service.setToolSearchPaths({temp.path()});
    QVERIFY(!service.openDiscover(QStringLiteral("app.zen_browser.zen")));
    QVERIFY(service.lastMessage().startsWith(QStringLiteral("Erro:")));
}

void MainuanTest::requestsExternalDownloadThroughDesktopServices()
{
    UrlReceiver receiver;
    SystemService service;
    const QString url = QStringLiteral("https://www.softmaker.net/down/softmaker-freeoffice-2024_1238-01_amd64.deb");
    QDesktopServices::setUrlHandler(QStringLiteral("https"), &receiver, "receive");
    const bool opened = service.openUrl(url);
    QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
    QVERIFY(opened);
    QCOMPARE(receiver.received, QUrl(url));
    QCOMPARE(service.lastMessage(), QStringLiteral("Abertura solicitada ao navegador."));
    QVERIFY(!service.openUrl(QStringLiteral("file:///tmp/unsafe.deb")));
    QVERIFY(service.lastMessage().startsWith(QStringLiteral("Erro:")));
    // With no GUI application/default-browser integration, a rejected handoff is reported.
    QVERIFY(!service.openUrl(url));
    QVERIFY(service.lastMessage().contains(QStringLiteral("Não foi possível abrir a URL")));
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
    // A hostname such as "Mainuan" may also be part of the distribution name.
    // Reject it as a complete exported field value, rather than a substring.
    QVERIFY(!text.contains(QRegularExpression(QStringLiteral("(?:^|\\n)[^:\\n]+: *")
        + QRegularExpression::escape(QSysInfo::machineHostName()) + QStringLiteral("(?:\\n|$)"))));
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
