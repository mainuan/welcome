#include "SystemService.h"

#include "AccentProfile.h"
#include "PlasmaInfo.h"
#include "QrCode.h"
#include "VisualStyle.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QColor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPalette>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <memory>

Q_LOGGING_CATEGORY(lcAppearance, "mainuan.welcome.appearance")

namespace {

// Shipped by the Mainuan artwork; referenced in place instead of bundling a copy.
const QString kMainuanLogo = QStringLiteral("/usr/share/plasma/avatars/logoMainuan.png");

// Reads a KDE configuration file through the whole cascade
// ($XDG_CONFIG_HOME, then $XDG_CONFIG_DIRS, which in a Plasma session starts
// with ~/.config/kdedefaults where global themes write their values).
KSharedConfig::Ptr freshConfig(const QString &name)
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(name);
    config->reparseConfiguration();
    return config;
}

// plasma-apply-colorscheme announces the new palette before AccentColor is
// written, so running applications (Plasma and the Welcome itself) kept the
// previous accent until the next change. Announce it again, as System Settings
// does, once everything is on disk.
void announcePaletteChange()
{
    KSharedConfig::openConfig(QStringLiteral("kdeglobals"))->reparseConfiguration();
    KSharedConfig::openConfig()->reparseConfiguration();
    QDBusMessage signal = QDBusMessage::createSignal(QStringLiteral("/KGlobalSettings"), QStringLiteral("org.kde.KGlobalSettings"),
                                                     QStringLiteral("notifyChange"));
    signal << 0 << 0; // KGlobalSettings::PaletteChanged
    QDBusConnection::sessionBus().send(signal);
}

bool lookAndFeelInstalled(const QString &package)
{
    const QString base = QStringLiteral("plasma/look-and-feel/") + package;
    return !QStandardPaths::locate(QStandardPaths::GenericDataLocation, base + QStringLiteral("/metadata.json")).isEmpty()
        || !QStandardPaths::locate(QStandardPaths::GenericDataLocation, base + QStringLiteral("/metadata.desktop")).isEmpty();
}

bool iconThemeInstalled(const QString &theme)
{
    return !QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                   QStringLiteral("icons/") + theme + QStringLiteral("/index.theme")).isEmpty();
}

bool cursorThemeInstalled(const QString &theme)
{
    return !QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                   QStringLiteral("icons/") + theme + QStringLiteral("/cursors"),
                                   QStandardPaths::LocateDirectory).isEmpty();
}

// plasma-changeicons is installed in plasma-workspace's libexec directory.
QStringList libexecDirectories()
{
    QStringList directories = {QStringLiteral("/usr/libexec"), QStringLiteral("/usr/lib/libexec")};
    const QStringList multiarch = QDir(QStringLiteral("/usr/lib")).entryList({QStringLiteral("*-linux-gnu*")}, QDir::Dirs);
    for (const QString &triplet : multiarch) {
        directories << QStringLiteral("/usr/lib/") + triplet + QStringLiteral("/libexec");
    }
    return directories;
}

// KDE stores colors as "r,g,b" (or a name). Parsed here instead of through
// KConfigGui, which is not linked. Values Plasma cannot parse either, such as
// "r,g,b ; #hex", are rejected so the Welcome reports the colors in effect.
QColor configColor(const KConfigGroup &group, const char *key)
{
    const QString raw = group.readEntry(key, QString()).trimmed();
    const QStringList channels = raw.split(QLatin1Char(','));
    if (channels.size() == 3) {
        int rgb[3];
        for (int i = 0; i < 3; ++i) {
            bool ok = false;
            rgb[i] = channels.at(i).trimmed().toInt(&ok);
            if (!ok || rgb[i] < 0 || rgb[i] > 255) {
                return {};
            }
        }
        return QColor(rgb[0], rgb[1], rgb[2]);
    }
    return raw.startsWith(QLatin1Char('#')) ? QColor(raw) : QColor();
}

QString lastLine(const QString &text)
{
    const QStringList lines = text.trimmed().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    return lines.isEmpty() ? QString() : lines.constLast().trimmed();
}

bool isDarkScheme(const QString &scheme)
{
    const QString lower = scheme.toLower();
    return lower.contains(QStringLiteral("dark")) || lower.contains(QStringLiteral("breeze-dark"))
        || lower.contains(QStringLiteral("night"));
}

}

SystemService::SystemService(QObject *parent)
    : QObject(parent)
    , m_sessionType(qEnvironmentVariable("XDG_SESSION_TYPE", QStringLiteral("unknown")).toLower())
    , m_logoSource(QFileInfo(kMainuanLogo).isReadable() ? QUrl::fromLocalFile(kMainuanLogo).toString() : QString())
{
    // Plasma writes appearance settings with KConfig::Notify; follow changes made
    // elsewhere (System Settings, another tool) without polling.
    const auto onAppearanceChanged = [this](const KConfigGroup &, const QByteArrayList &) {
        readDesktopState();
        readVisualStyle();
    };
    m_globalsWatcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kdeglobals")));
    m_plasmaWatcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("plasmarc")));
    connect(m_globalsWatcher.data(), &KConfigWatcher::configChanged, this, onAppearanceChanged);
    connect(m_plasmaWatcher.data(), &KConfigWatcher::configChanged, this, onAppearanceChanged);

    refresh();
}

bool SystemService::darkTheme() const
{
    return m_darkTheme;
}

QString SystemService::accentColor() const
{
    return m_accentColor;
}

QString SystemService::visualStyle() const
{
    return m_visualStyle;
}

bool SystemService::visualStyleAvailable() const
{
    return m_visualStyleAvailable;
}

QStringList SystemService::installedVisualStyles() const
{
    return m_installedVisualStyles;
}

void SystemService::setToolSearchPaths(const QStringList &paths)
{
    m_toolPaths = paths;
    readVisualStyle();
}

QString SystemService::findTool(const QString &name) const
{
    if (!m_toolPaths.isEmpty()) {
        return QStandardPaths::findExecutable(name, m_toolPaths);
    }
    const QString inPath = QStandardPaths::findExecutable(name);
    return inPath.isEmpty() ? QStandardPaths::findExecutable(name, libexecDirectories()) : inPath;
}

QString SystemService::sessionType() const
{
    return m_sessionType;
}

QString SystemService::logoSource() const
{
    return m_logoSource;
}

QString SystemService::plasmaVersion() const
{
    return m_plasmaVersion;
}

QString SystemService::pixKey() const
{
    return m_pixKey;
}

QString SystemService::firewallStatus() const
{
    return m_firewallStatus;
}

QString SystemService::driversStatus() const
{
    return m_driversStatus;
}

bool SystemService::driversAvailable() const
{
    return m_driversAvailable;
}

QString SystemService::lastMessage() const
{
    return m_lastMessage;
}

bool SystemService::busy() const
{
    return m_busy;
}

bool SystemService::isValidAccent(const QString &value)
{
    const QColor color(value.trimmed());
    return color.isValid() && value.trimmed().startsWith(QLatin1Char('#'))
        && (value.trimmed().size() == 7 || value.trimmed().size() == 9);
}

bool SystemService::isAllowedVisualStyle(const QString &value)
{
    return VisualStyles::find(value) != nullptr;
}

void SystemService::refresh()
{
    readDesktopState();
    readVisualStyle();
    readPixKey();
    readPlasmaVersion();
    refreshDiagnostics();
}

void SystemService::readDesktopState()
{
    const KSharedConfig::Ptr globals = freshConfig(QStringLiteral("kdeglobals"));

    // Prefer the window color of the active scheme; its name is a weaker hint.
    const QColor window = configColor(KConfigGroup(globals, QStringLiteral("Colors:Window")), "BackgroundNormal");
    const QString scheme = KConfigGroup(globals, QStringLiteral("General")).readEntry("ColorScheme", QString());
    // The palette fallbacks need a QGuiApplication (headless tests only have a
    // QCoreApplication; qGuiApp is a static_cast and cannot tell).
    const bool guiApplication = qobject_cast<QGuiApplication *>(QCoreApplication::instance()) != nullptr;
    bool newDark = guiApplication && QGuiApplication::palette().window().color().lightness() < 128;
    if (window.isValid()) {
        newDark = window.lightness() < 128;
    } else if (!scheme.isEmpty()) {
        newDark = isDarkScheme(scheme);
    }
    if (newDark != m_darkTheme) {
        m_darkTheme = newDark;
        emit darkThemeChanged();
    }

    // AccentColor is written by System Settings and by setAccent(); when only
    // plasma-apply-colorscheme --accent-color was used, the tinted selection
    // color is the accent in effect.
    QColor accent = configColor(KConfigGroup(globals, QStringLiteral("General")), "AccentColor");
    if (!accent.isValid()) {
        accent = configColor(KConfigGroup(globals, QStringLiteral("Colors:Selection")), "BackgroundNormal");
    }
    if (!accent.isValid() && guiApplication) {
        accent = QGuiApplication::palette().highlight().color();
    }
    const QString newAccent = accent.name(QColor::HexRgb).toLower();
    if (newAccent != m_accentColor) {
        m_accentColor = newAccent;
        emit accentColorChanged();
    }
}

// [Contribute] PixKey (and optional PixPayload, a complete BR Code) from the
// distribution's /etc/mainuan/welcome.conf, or from the user's copy.
void SystemService::readPixKey()
{
    const QStringList files = {QStringLiteral("/etc/mainuan/welcome.conf"),
                               QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
                                   .filePath(QStringLiteral("mainuan/welcome.conf"))};
    QString newKey;
    QString newPayload;
    for (const QString &file : files) {
        QSettings settings(file, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("Contribute"));
        newKey = settings.value(QStringLiteral("PixKey")).toString().trimmed();
        newPayload = settings.value(QStringLiteral("PixPayload")).toString().trimmed();
        if (!newKey.isEmpty()) {
            break;
        }
    }

    if (newKey != m_pixKey || newPayload != m_pixPayload) {
        m_pixKey = newKey;
        m_pixPayload = newPayload;
        m_pixQrSource = QrCode::dataUrl(m_pixPayload.isEmpty() ? m_pixKey : m_pixPayload);
        emit pixKeyChanged();
    }
}

QString SystemService::pixQrSource() const
{
    return m_pixQrSource;
}

bool SystemService::pixQrIsPayload() const
{
    return !m_pixPayload.isEmpty();
}

void SystemService::readPlasmaVersion()
{
    PlasmaInfo::queryVersion(this, [this](const QString &version) {
        // Same text as `plasmashell --version`, which the pages expect.
        const QString text = version.isEmpty() ? QString() : QStringLiteral("plasmashell ") + version;
        if (text != m_plasmaVersion) {
            m_plasmaVersion = text;
            emit plasmaVersionChanged();
        }
    });
}

void SystemService::refreshDiagnostics()
{
    m_driversAvailable = !QStandardPaths::findExecutable(QStringLiteral("kubuntu-driver-manager")).isEmpty();
    m_driversStatus = m_driversAvailable ? QStringLiteral("Gerenciador disponível")
                                         : QStringLiteral("Gerenciador indisponível");

    // `ufw status` requires root; the boot-time switch in ufw.conf is world-readable.
    QFile ufwConfig(QStringLiteral("/etc/ufw/ufw.conf"));
    if (!ufwConfig.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_firewallStatus.clear();
    } else {
        m_firewallStatus = QStringLiteral("Inativo");
        while (!ufwConfig.atEnd()) {
            const QString line = QString::fromUtf8(ufwConfig.readLine()).trimmed();
            if (line.startsWith(QStringLiteral("ENABLED="))) {
                const QString value = line.mid(8).remove(QLatin1Char('"')).trimmed().toLower();
                m_firewallStatus = value == QStringLiteral("yes") ? QStringLiteral("Ativo") : QStringLiteral("Inativo");
            }
        }
    }
    emit diagnosticsChanged();
}

void SystemService::readVisualStyle()
{
    const QString lookAndFeel = KConfigGroup(freshConfig(QStringLiteral("kdeglobals")), QStringLiteral("KDE"))
                                    .readEntry("LookAndFeelPackage", QString());
    const QString plasmaTheme = KConfigGroup(freshConfig(QStringLiteral("plasmarc")), QStringLiteral("Theme"))
                                    .readEntry("name", QString());
    const QString style = VisualStyles::detect(lookAndFeel, plasmaTheme);
    const bool available = !findTool(QStringLiteral("plasma-apply-lookandfeel")).isEmpty();

    QStringList installed;
    for (const VisualStyleProfile &profile : VisualStyles::profiles()) {
        if (lookAndFeelInstalled(profile.lightPackage) && lookAndFeelInstalled(profile.darkPackage)) {
            installed << profile.id;
        }
    }

    if (style != m_visualStyle || available != m_visualStyleAvailable || installed != m_installedVisualStyles) {
        if (style != m_visualStyle) {
            qCInfo(lcAppearance).nospace() << "visual style: " << (style.isEmpty() ? QStringLiteral("other") : style)
                                           << " (LookAndFeelPackage=" << lookAndFeel << ", plasma theme=" << plasmaTheme << ')';
        }
        m_visualStyle = style;
        m_visualStyleAvailable = available;
        m_installedVisualStyles = installed;
        emit visualStyleChanged();
    }
}

void SystemService::setMessage(const QString &message, bool error)
{
    const QString prefix = error ? QStringLiteral("Erro: ") : QString();
    const QString newMessage = prefix + message;
    if (newMessage != m_lastMessage) {
        m_lastMessage = newMessage;
        emit lastMessageChanged();
    }
}

void SystemService::setBusy(bool busy)
{
    if (busy != m_busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void SystemService::runCommands(QList<Command> commands, const QString &successMessage, const QString &errorPrefix,
                                std::function<void()> onSuccess)
{
    if (commands.isEmpty()) {
        if (onSuccess) {
            onSuccess();
        } else {
            setMessage(successMessage);
            setBusy(false);
            refresh();
        }
        return;
    }
    if (!m_busy) {
        setBusy(true);
        setMessage(QStringLiteral("Aplicando alteração…"));
    }

    const Command command = commands.takeFirst();
    QProcess *process = new QProcess(this);
    process->setStandardInputFile(QProcess::nullDevice());
    const auto failedToStart = std::make_shared<bool>(false);
    connect(process, &QProcess::errorOccurred, this, [this, process, failedToStart](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            *failedToStart = true;
            setMessage(QStringLiteral("comando não encontrado"), true);
            setBusy(false);
            process->deleteLater();
        }
    });
    connect(process, &QProcess::finished, this,
            [this, process, commands, successMessage, errorPrefix, onSuccess, failedToStart](int exitCode, QProcess::ExitStatus) {
                if (*failedToStart) {
                    return;
                }
                const QString errorOutput = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
                process->deleteLater();
                if (exitCode != 0) {
                    setMessage(errorPrefix + (errorOutput.isEmpty() ? QStringLiteral("código %1").arg(exitCode)
                                                                    : errorOutput), true);
                    setBusy(false);
                    return;
                }
                runCommands(commands, successMessage, errorPrefix, onSuccess);
            });
    process->start(command.program, command.arguments);
}

void SystemService::setTheme(const QString &theme)
{
    if (theme != QStringLiteral("dark") && theme != QStringLiteral("light")) {
        setMessage(QStringLiteral("Tema inválido."), true);
        return;
    }

    if (m_busy) {
        setMessage(QStringLiteral("Aguarde a operação atual terminar."), true);
        return;
    }

    // With a Mainuan style active, switch to its light or dark variant so the
    // theme and the style stay consistent.
    if (const VisualStyleProfile *profile = VisualStyles::find(m_visualStyle)) {
        applyStyleProfile(*profile, theme == QStringLiteral("dark"),
                          theme == QStringLiteral("dark") ? QStringLiteral("Tema escuro aplicado.")
                                                          : QStringLiteral("Tema claro aplicado."));
        return;
    }

    const QString executable = findTool(QStringLiteral("plasma-apply-colorscheme"));
    if (executable.isEmpty()) {
        setMessage(QStringLiteral("plasma-apply-colorscheme não está disponível."), true);
        return;
    }

    const QString scheme = theme == QStringLiteral("dark") ? QStringLiteral("DreamGrayDarkColor")
                                                              : QStringLiteral("DreamGrayLightColor");
    runCommands({{executable, {scheme}, QStringLiteral("tema")}}, QStringLiteral("Tema aplicado."),
                QStringLiteral("Não foi possível aplicar o tema: "));
}

void SystemService::setAccent(const QString &accent)
{
    const QString normalized = accent.trimmed().toLower();
    if (!isValidAccent(normalized)) {
        setMessage(QStringLiteral("Cor de destaque inválida."), true);
        return;
    }

    const QString executable = findTool(QStringLiteral("plasma-apply-colorscheme"));
    if (executable.isEmpty()) {
        setMessage(QStringLiteral("plasma-apply-colorscheme não está disponível."), true);
        return;
    }

    runCommands({{executable, {QStringLiteral("--accent-color"), normalized}, QStringLiteral("cor de destaque")}},
                QStringLiteral("Cor de destaque aplicada."),
                QStringLiteral("Não foi possível aplicar a cor: "),
                [this, normalized]() {
                    // plasma-apply-colorscheme only tints the current scheme. Record the
                    // choice like System Settings does, so Plasma keeps it when the
                    // global theme changes and the Welcome shows the exact color.
                    KConfigGroup general(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), QStringLiteral("General"));
                    const QColor color(normalized);
                    general.writeEntry("AccentColor",
                                       QStringLiteral("%1,%2,%3").arg(color.red()).arg(color.green()).arg(color.blue()),
                                       KConfig::Notify);
                    general.sync();
                    announcePaletteChange();
                    readDesktopState();
                    applyAccentWallpaper(normalized, QStringLiteral("Cor de destaque aplicada."));
                });
}

// Sets the Mainuan wallpaper that matches `accent` on every desktop
// (plasma-apply-wallpaperimage walks all desktops, so all monitors).
void SystemService::applyAccentWallpaper(const QString &accent, const QString &colorMessage)
{
    const AccentProfile *profile = AccentProfiles::find(accent);
    if (profile == nullptr) {
        setMessage(colorMessage);
        setBusy(false);
        return;
    }
    const QString wallpaper = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                     QStringLiteral("wallpapers/") + profile->wallpaper);
    const QString tool = findTool(QStringLiteral("plasma-apply-wallpaperimage"));
    if (wallpaper.isEmpty() || tool.isEmpty()) {
        qCWarning(lcAppearance) << "wallpaper not applied:" << profile->wallpaper
                                << (wallpaper.isEmpty() ? "file not found" : "plasma-apply-wallpaperimage missing");
        setMessage(QStringLiteral("Cor %1 aplicada, mas o papel de parede %2 não foi encontrado.")
                       .arg(profile->name, profile->wallpaper),
                   true);
        setBusy(false);
        return;
    }
    runTool(tool, {wallpaper}, [this, profile](bool ok, const QString &errorOutput) {
        if (ok) {
            qCInfo(lcAppearance) << "accent" << profile->name << "wallpaper" << profile->wallpaper;
            setMessage(QStringLiteral("Cor %1 e papel de parede aplicados.").arg(profile->name));
        } else {
            qCWarning(lcAppearance) << "plasma-apply-wallpaperimage failed:" << errorOutput.trimmed();
            setMessage(QStringLiteral("Cor %1 aplicada, mas não foi possível trocar o papel de parede.").arg(profile->name),
                       true);
        }
        setBusy(false);
    });
}

void SystemService::readWallpaper(std::function<void(const QString &)> done)
{
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
                                                       QStringLiteral("org.kde.PlasmaShell"), QStringLiteral("wallpaper"));
    call << 0u;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [done](QDBusPendingCallWatcher *pending) {
        const QDBusPendingReply<QVariantMap> reply = *pending;
        pending->deleteLater();
        done(reply.isError() ? QString() : reply.value().value(QStringLiteral("Image")).toString());
    });
}

void SystemService::setVisualStyle(const QString &style)
{
    const VisualStyleProfile *profile = VisualStyles::find(style);
    if (profile == nullptr) {
        setMessage(QStringLiteral("Estilo visual inválido."), true);
        return;
    }
    if (m_busy) {
        setMessage(QStringLiteral("Aguarde a operação atual terminar."), true);
        return;
    }
    applyStyleProfile(*profile, m_darkTheme, QStringLiteral("Estilo %1 aplicado.").arg(profile->name));
}

void SystemService::runTool(const QString &program, const QStringList &arguments,
                            std::function<void(bool, const QString &)> done)
{
    auto *process = new QProcess(this);
    process->setStandardInputFile(QProcess::nullDevice());
    connect(process, &QProcess::finished, this, [process, done](int exitCode, QProcess::ExitStatus status) {
        const QString errorOutput = QString::fromLocal8Bit(process->readAllStandardError());
        process->deleteLater();
        done(status == QProcess::NormalExit && exitCode == 0, errorOutput);
    });
    connect(process, &QProcess::errorOccurred, this, [process, done](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            const QString message = process->errorString();
            process->deleteLater();
            done(false, message);
        }
    });
    process->start(program, arguments);
}

// Applies the Look-and-Feel package of `profile` (the essential step), then the
// fallbacks for icon/cursor themes the package names but Mainuan lacks. The
// wallpaper belongs to the accent color: if the package changed it, the
// accent's wallpaper (or the previous image) is put back. Success is reported
// only if Plasma's configuration then identifies the requested style.
void SystemService::applyStyleProfile(const VisualStyleProfile &profile, bool dark, const QString &successMessage)
{
    const QString lookAndFeelTool = findTool(QStringLiteral("plasma-apply-lookandfeel"));
    if (lookAndFeelTool.isEmpty()) {
        setMessage(QStringLiteral("O Plasma deste sistema não oferece plasma-apply-lookandfeel; o estilo não pode ser trocado."), true);
        return;
    }
    const QString package = profile.package(dark);
    if (!lookAndFeelInstalled(package)) {
        setMessage(QStringLiteral("O estilo %1 não está instalado neste sistema.").arg(profile.name), true);
        return;
    }

    setBusy(true);
    setMessage(QStringLiteral("Aplicando estilo %1…").arg(profile.name));
    qCInfo(lcAppearance) << "applying" << profile.name << package;

    readWallpaper([this, lookAndFeelTool, package, profile, dark, successMessage](const QString &wallpaperBefore) {
        // `package` comes from the closed profile table, never from QML.
        runTool(lookAndFeelTool, {QStringLiteral("--apply"), package},
                [this, profile, dark, successMessage, wallpaperBefore](bool ok, const QString &errorOutput) {
                    if (!ok) {
                        qCWarning(lcAppearance) << "plasma-apply-lookandfeel failed:" << errorOutput.trimmed();
                        const QString detail = lastLine(errorOutput);
                        setMessage(QStringLiteral("Não foi possível aplicar o estilo %1.").arg(profile.name)
                                       + (detail.isEmpty() ? QString() : QStringLiteral(" (") + detail + QLatin1Char(')')),
                                   true);
                        readDesktopState();
                        readVisualStyle();
                        setBusy(false);
                        return;
                    }
                    readWallpaper([this, profile, dark, successMessage, wallpaperBefore](const QString &wallpaperAfter) {
                        finishStyleProfile(profile, dark, successMessage, wallpaperBefore != wallpaperAfter
                                                                               ? wallpaperBefore : QString());
                    });
                });
    });
}

void SystemService::finishStyleProfile(const VisualStyleProfile &profile, bool dark, const QString &successMessage,
                                       const QString &replacedWallpaper)
{
    QList<Command> followUps;
    const QString icons = KConfigGroup(freshConfig(QStringLiteral("kdeglobals")), QStringLiteral("Icons"))
                              .readEntry("Theme", QString());
    const QString fallbackIcons = dark ? profile.fallbackDarkIcons : profile.fallbackLightIcons;
    if (!icons.isEmpty() && !iconThemeInstalled(icons) && iconThemeInstalled(fallbackIcons)) {
        followUps.append({findTool(QStringLiteral("plasma-changeicons")), {fallbackIcons}, QStringLiteral("ícones")});
    }
    const QString cursor = KConfigGroup(freshConfig(QStringLiteral("kcminputrc")), QStringLiteral("Mouse"))
                               .readEntry("cursorTheme", QString());
    if (!cursor.isEmpty() && !cursorThemeInstalled(cursor) && cursorThemeInstalled(profile.fallbackCursor)) {
        followUps.append({findTool(QStringLiteral("plasma-apply-cursortheme")), {profile.fallbackCursor},
                          QStringLiteral("cursor")});
    }
    if (!replacedWallpaper.isEmpty()) {
        const AccentProfile *accent = AccentProfiles::find(m_accentColor);
        const QString wallpaper = accent != nullptr
            ? QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("wallpapers/") + accent->wallpaper)
            : QUrl(replacedWallpaper).toLocalFile();
        qCInfo(lcAppearance) << "the style changed the wallpaper; restoring" << wallpaper;
        followUps.append({wallpaper.isEmpty() ? QString() : findTool(QStringLiteral("plasma-apply-wallpaperimage")),
                          {wallpaper}, QStringLiteral("papel de parede")});
    }

    runStyleFollowUps(followUps, {}, [this, profile, successMessage](const QStringList &failures) {
        readDesktopState();
        readVisualStyle();
        if (m_visualStyle != profile.id) {
            qCWarning(lcAppearance) << "style not confirmed after applying" << profile.name;
            setMessage(QStringLiteral("O Plasma não confirmou o estilo %1.").arg(profile.name), true);
        } else if (!failures.isEmpty()) {
            setMessage(successMessage + QStringLiteral(" Não foi possível ajustar: ")
                           + failures.join(QStringLiteral(", ")) + QLatin1Char('.'),
                       true);
        } else {
            setMessage(successMessage);
        }
        setBusy(false);
    });
}

void SystemService::runStyleFollowUps(QList<Command> steps, QStringList failures,
                                      std::function<void(const QStringList &)> done)
{
    if (steps.isEmpty()) {
        done(failures);
        return;
    }
    const Command step = steps.takeFirst();
    if (step.program.isEmpty()) {
        qCWarning(lcAppearance) << "no tool or file for" << step.label;
        runStyleFollowUps(steps, failures << step.label, done);
        return;
    }
    runTool(step.program, step.arguments,
            [this, steps, failures, step, done](bool ok, const QString &errorOutput) mutable {
        if (!ok) {
            qCWarning(lcAppearance) << step.program << "failed:" << errorOutput.trimmed();
            failures << step.label;
        }
        runStyleFollowUps(steps, failures, done);
    });
}

void SystemService::performAction(const QString &action)
{
    if (action == QStringLiteral("drivers")) {
        const QString executable = QStandardPaths::findExecutable(QStringLiteral("kubuntu-driver-manager"));
        if (executable.isEmpty()) {
            setMessage(QStringLiteral("O gerenciador de drivers não está instalado."), true);
            return;
        }
        if (!QProcess::startDetached(executable, {})) {
            setMessage(QStringLiteral("Não foi possível abrir o gerenciador de drivers."), true);
        }
        return;
    }

    setMessage(QStringLiteral("Ação desconhecida."), true);
}

bool SystemService::openUrl(const QString &url)
{
    const QUrl parsed(url);
    if (!parsed.isValid() || (parsed.scheme() != QStringLiteral("https")
                              && parsed.scheme() != QStringLiteral("http")
                              && parsed.scheme() != QStringLiteral("tg"))) {
        setMessage(QStringLiteral("URL bloqueada por segurança."), true);
        return false;
    }
    return QDesktopServices::openUrl(parsed);
}

bool SystemService::copyText(const QString &text)
{
    if (text.isEmpty()) {
        setMessage(QStringLiteral("Não há chave Pix configurada."), true);
        return false;
    }
    if (QGuiApplication::clipboard() == nullptr) {
        setMessage(QStringLiteral("Clipboard indisponível."), true);
        return false;
    }
    QGuiApplication::clipboard()->setText(text);
    setMessage(QStringLiteral("Chave Pix copiada."));
    return true;
}
