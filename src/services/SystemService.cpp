#include "SystemService.h"

#include "AccentProfile.h"
#include "PackageService.h"
#include "PlasmaInfo.h"
#include "QrCode.h"
#include "UserAvatar.h"
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
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>
#include <memory>

Q_LOGGING_CATEGORY(lcAppearance, "mainuan.welcome.appearance")

namespace {

// Shipped by the Mainuan artwork; referenced in place instead of bundling a copy.
const QString kMainuanLogo = QStringLiteral("/usr/share/plasma/avatars/logoMainuan.png");
// Stable public launcher; the driver tool manages its own version and permissions.
const QString kDriverManager = QStringLiteral("mainuan-gestao-drivers");

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
    const QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
        QStringLiteral("icons/") + theme + QStringLiteral("/index.theme"));
    if (file.isEmpty() || !QFileInfo(file).isReadable()) {
        return false;
    }
    const KConfig config(file, KConfig::SimpleConfig);
    const KConfigGroup group(&config, QStringLiteral("Icon Theme"));
    return !group.readEntry("Name", QString()).trimmed().isEmpty()
        && !group.readEntry("Directories", QStringList()).isEmpty();
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
    if (channels.size() == 3 || channels.size() == 4) {
        int rgb[4] = {0, 0, 0, 255};
        for (int i = 0; i < channels.size(); ++i) {
            bool ok = false;
            rgb[i] = channels.at(i).trimmed().toInt(&ok);
            if (!ok || rgb[i] < 0 || rgb[i] > 255) {
                return {};
            }
        }
        return QColor(rgb[0], rgb[1], rgb[2], rgb[3]);
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

SystemService::SystemService(QObject *parent, const QString &userHome)
    : QObject(parent)
    , m_userHome(userHome.isEmpty() ? QDir::homePath() : userHome)
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

QString SystemService::accentId() const { return m_accentId; }
QString SystemService::colorScheme() const { return m_colorScheme; }
QString SystemService::iconTheme() const { return m_iconTheme; }

QVariantList SystemService::accentProfiles() const
{
    QVariantList result;
    for (const AccentProfile &profile : AccentProfiles::profiles()) {
        result.append(QVariantMap{{QStringLiteral("id"), profile.id}, {QStringLiteral("name"), profile.name},
                                  {QStringLiteral("color"), profile.color}});
    }
    return result;
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
    return AccentProfiles::find(value) != nullptr;
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

    const AccentProfile *profile = AccentProfiles::fromScheme(scheme);
    const QString newId = profile != nullptr ? profile->id : QString();
    QColor accent = configColor(KConfigGroup(globals, QStringLiteral("General")), "AccentColor");
    if (!accent.isValid() || accent.alpha() == 0) {
        accent = configColor(KConfigGroup(globals, QStringLiteral("Colors:Selection")), "BackgroundNormal");
    }
    if (!accent.isValid() && guiApplication) {
        accent = QGuiApplication::palette().highlight().color();
    }
    const QString newAccent = accent.name(QColor::HexRgb).toLower();
    if (newAccent != m_accentColor || newId != m_accentId || scheme != m_colorScheme) {
        m_accentColor = newAccent;
        m_accentId = newId;
        m_colorScheme = scheme;
        emit accentColorChanged();
    }
    const QString icons = KConfigGroup(globals, QStringLiteral("Icons")).readEntry("Theme", QString());
    if (icons != m_iconTheme) {
        m_iconTheme = icons;
        emit iconThemeChanged();
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
    m_driversAvailable = !QStandardPaths::findExecutable(kDriverManager).isEmpty();
    m_driversStatus = m_driversAvailable ? QStringLiteral("Gestão de Drivers disponível")
                                         : QStringLiteral("Gestão de Drivers não instalada");

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

    readDesktopState();
    readVisualStyle();
    // With a Mainuan style active, switch to its light or dark variant so the
    // theme and the style stay consistent.
    if (const VisualStyleProfile *profile = VisualStyles::find(m_visualStyle)) {
        applyStyleProfile(*profile, theme == QStringLiteral("dark"),
                          theme == QStringLiteral("dark") ? QStringLiteral("Tema escuro aplicado.")
                                                          : QStringLiteral("Tema claro aplicado."));
        return;
    }

    if (const AccentProfile *accent = AccentProfiles::find(m_accentId)) {
        setBusy(true);
        validateScheme(*accent, theme == QStringLiteral("dark"), [this, accent, theme](const QString &error) {
            if (!error.isEmpty()) {
                finishAppearance({}, {error});
                return;
            }
            applyAccentScheme(*accent, theme == QStringLiteral("dark"), [this, accent](const QString &failure) {
                if (!failure.isEmpty()) {
                    finishAppearance({}, {failure});
                    return;
                }
                applyAccentExtras(*accent, QStringLiteral("Tema e cor %1 aplicados.").arg(accent->name));
            });
        });
    } else {
        setMessage(QStringLiteral("O esquema externo atual não possui uma variante conhecida. Selecione uma cor Mainuan primeiro."), true);
    }
}

void SystemService::setAccent(const QString &id)
{
    const AccentProfile *profile = AccentProfiles::find(id);
    if (profile == nullptr) {
        setMessage(QStringLiteral("Cor de destaque inválida."), true);
        return;
    }
    if (m_busy) {
        setMessage(QStringLiteral("Aguarde a operação atual terminar."), true);
        return;
    }
    readDesktopState();
    const bool dark = m_darkTheme;
    setBusy(true);
    setMessage(QStringLiteral("Aplicando cor %1…").arg(profile->name));
    validateScheme(*profile, dark, [this, profile, dark](const QString &error) {
        if (!error.isEmpty()) {
            finishAppearance({}, {error});
            return;
        }
        applyAccentScheme(*profile, dark, [this, profile](const QString &failure) {
            if (!failure.isEmpty()) {
                finishAppearance({}, {failure});
                return;
            }
            applyAccentExtras(*profile, QStringLiteral("Cor %1, ícones, papel de parede e avatar aplicados.").arg(profile->name));
        });
    });
}

void SystemService::validateScheme(const AccentProfile &profile, bool dark, std::function<void(const QString &)> done)
{
    const QString scheme = profile.scheme(dark);
    const QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
        QStringLiteral("color-schemes/") + scheme + QStringLiteral(".colors"));
    if (file.isEmpty() || !QFileInfo(file).isReadable()) {
        done(QStringLiteral("Esquema %1.colors ausente ou ilegível.").arg(scheme));
        return;
    }
    const KConfig config(file, KConfig::SimpleConfig);
    if (KConfigGroup(&config, QStringLiteral("General")).readEntry("Name", QString()).trimmed().isEmpty()
        || !configColor(KConfigGroup(&config, QStringLiteral("Colors:Window")), "BackgroundNormal").isValid()) {
        done(QStringLiteral("Esquema %1.colors inválido.").arg(scheme));
        return;
    }
    const QString tool = findTool(QStringLiteral("plasma-apply-colorscheme"));
    if (tool.isEmpty()) {
        done(QStringLiteral("plasma-apply-colorscheme não está disponível."));
        return;
    }
    runTool(tool, {QStringLiteral("--list-schemes")}, [scheme, done](bool ok, const QString &output) {
        const QRegularExpression listed(QStringLiteral("(?:^|\\n)\\s*\\*\\s+") + QRegularExpression::escape(scheme)
                                        + QStringLiteral("(?:\\s|$)"));
        done(ok && listed.match(output).hasMatch() ? QString()
             : QStringLiteral("O Plasma não reconheceu o esquema %1.colors.").arg(scheme));
    });
}

void SystemService::applyAccentScheme(const AccentProfile &profile, bool dark, std::function<void(const QString &)> done)
{
    const QString scheme = profile.scheme(dark);
    const QString tool = findTool(QStringLiteral("plasma-apply-colorscheme"));
    const auto globals = freshConfig(QStringLiteral("kdeglobals"));
    KConfigGroup general(globals, QStringLiteral("General"));
    // Snapshot only the two keys this operation owns, including whether they
    // were inherited. No other user preferences are removed or rewritten.
    const KConfig local(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                        + QStringLiteral("/kdeglobals"), KConfig::SimpleConfig);
    const KConfigGroup localGeneral(&local, QStringLiteral("General"));
    const bool hadScheme = localGeneral.hasKey("ColorScheme");
    const QString oldScheme = localGeneral.readEntry("ColorScheme", QString());
    const bool hadAccent = localGeneral.hasKey("AccentColor");
    const QString oldAccent = localGeneral.readEntry("AccentColor", QString());
    const auto restore = [globals, hadScheme, oldScheme, hadAccent, oldAccent]() {
        globals->reparseConfiguration();
        KConfigGroup group(globals, QStringLiteral("General"));
        if (hadScheme) group.writeEntry("ColorScheme", oldScheme, KConfig::Notify);
        else group.revertToDefault("ColorScheme", KConfig::Notify);
        if (hadAccent) group.writeEntry("AccentColor", oldAccent, KConfig::Notify);
        else group.revertToDefault("AccentColor", KConfig::Notify);
        group.sync();
    };
    // The CLI deliberately skips an already-selected scheme. Clear its selector
    // temporarily to force a full reload, including after removing a custom tint.
    // Startup never changes these keys.
    general.deleteEntry("AccentColor");
    general.writeEntry("ColorScheme", QString());
    bool prepared = general.sync();
    KConfigGroup effective(freshConfig(QStringLiteral("kdeglobals")), QStringLiteral("General"));
    // Deleting a local override can expose an inherited custom accent. KDE's
    // transparent setting disables tinting without modifying the defaults file.
    if (effective.hasKey("AccentColor")) {
        effective.writeEntry("AccentColor", QStringLiteral("0,0,0,0"));
        prepared = effective.sync() && prepared;
    }
    const KConfigGroup checked(freshConfig(QStringLiteral("kdeglobals")), QStringLiteral("General"));
    const QColor remaining = configColor(checked, "AccentColor");
    if (!prepared || !checked.readEntry("ColorScheme", QString()).isEmpty()
        || (checked.hasKey("AccentColor") && (!remaining.isValid() || remaining.alpha() > 0))) {
        restore();
        done(QStringLiteral("Não foi possível preparar a configuração de cores do KDE."));
        return;
    }
    runTool(tool, {scheme}, [this, scheme, restore, done](bool ok, const QString &output) {
        const KConfigGroup applied(freshConfig(QStringLiteral("kdeglobals")), QStringLiteral("General"));
        const QColor override = configColor(applied, "AccentColor");
        if (!ok || applied.readEntry("ColorScheme", QString()) != scheme || (override.isValid() && override.alpha() > 0)) {
            qCWarning(lcAppearance) << "scheme not confirmed:" << scheme << "process succeeded:" << ok
                                   << "reported scheme:" << applied.readEntry("ColorScheme", QString())
                                   << "custom accent:" << override << "tool result:" << lastLine(output);
            restore();
            done(QStringLiteral("O Plasma não confirmou o esquema %1 sem substituição de cor.").arg(scheme));
            return;
        }
        announcePaletteChange();
        readDesktopState();
        done({});
    });
}

void SystemService::applyAccentExtras(const AccentProfile &profile, const QString &successMessage, QList<Command> steps)
{
    QStringList failures;
    if (!iconThemeInstalled(profile.iconTheme)) {
        failures << QStringLiteral("tema de ícones %1 ausente ou index.theme inválido (ícones anteriores preservados)").arg(profile.iconTheme);
    } else {
        steps.prepend({findTool(QStringLiteral("plasma-changeicons")), {profile.iconTheme}, QStringLiteral("ícones ") + profile.iconTheme});
    }
    const QString wallpaper = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
        QStringLiteral("wallpapers/") + profile.wallpaper);
    if (wallpaper.isEmpty()) {
        failures << QStringLiteral("papel de parede %1 ausente").arg(profile.wallpaper);
    } else {
        steps.append({findTool(QStringLiteral("plasma-apply-wallpaperimage")), {wallpaper}, QStringLiteral("papel de parede")});
    }
    runStyleFollowUps(steps, failures, [this, profile, successMessage](QStringList errors) {
        readDesktopState();
        if (iconThemeInstalled(profile.iconTheme) && m_iconTheme != profile.iconTheme) {
            errors << QStringLiteral("o Plasma não confirmou os ícones %1").arg(profile.iconTheme);
        }
        // External changes during the async operation must not select an avatar
        // for a scheme that is no longer active.
        if (AccentProfiles::fromScheme(m_colorScheme) == AccentProfiles::find(profile.id)) {
            const QString source = QStandardPaths::locate(QStandardPaths::GenericDataLocation, profile.avatar());
            const auto result = std::make_shared<QString>();
            // Image decoding, copying and fsync can be slow on a network home.
            // The worker owns only values; it never touches QML or this service.
            auto *worker = QThread::create([source, home = m_userHome, result]() {
                *result = UserAvatar::update(source, home);
            });
            connect(worker, &QThread::finished, worker, &QObject::deleteLater);
            connect(worker, &QThread::finished, this, [this, successMessage, errors, result]() mutable {
                if (!result->isEmpty()) errors << QStringLiteral("avatar: ") + *result;
                finishAppearance(successMessage, errors);
            });
            worker->start();
            return;
        } else {
            errors << QStringLiteral("esquema alterado durante a operação; avatar preservado");
        }
        finishAppearance(successMessage, errors);
    });
}

void SystemService::finishAppearance(const QString &successMessage, const QStringList &failures)
{
    readDesktopState();
    readVisualStyle();
    setMessage(failures.isEmpty() ? successMessage
               : QStringLiteral("Não foi possível concluir todos os ajustes: ") + failures.join(QStringLiteral("; ")) + QLatin1Char('.'),
               !failures.isEmpty());
    setBusy(false);
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
    readDesktopState();
    applyStyleProfile(*profile, m_darkTheme, QStringLiteral("Estilo %1 aplicado.").arg(profile->name));
}

void SystemService::runTool(const QString &program, const QStringList &arguments,
                            std::function<void(bool, const QString &)> done)
{
    auto *process = new QProcess(this);
    process->setStandardInputFile(QProcess::nullDevice());
    connect(process, &QProcess::finished, this, [process, done](int exitCode, QProcess::ExitStatus status) {
        const bool ok = status == QProcess::NormalExit && exitCode == 0;
        const QString output = QString::fromLocal8Bit(ok ? process->readAllStandardOutput() : process->readAllStandardError());
        process->deleteLater();
        done(ok, output);
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
    // Snapshot the identity before KConfigWatcher sees the global theme's defaults.
    const QString accentId = m_accentId;
    const AccentProfile *accent = AccentProfiles::find(accentId);
    if (accent != nullptr && (!iconThemeInstalled(accent->iconTheme)
                             || findTool(QStringLiteral("plasma-changeicons")).isEmpty())) {
        setMessage(QStringLiteral("Não foi possível preservar o tema de ícones %1. Estilo anterior preservado.").arg(accent->iconTheme), true);
        return;
    }
    setBusy(true);
    setMessage(QStringLiteral("Aplicando estilo %1…").arg(profile.name));
    const auto apply = [this, lookAndFeelTool, package, profile, dark, successMessage, accentId](const QString &error) {
        if (!error.isEmpty()) {
            finishAppearance({}, {error});
            return;
        }
        readWallpaper([this, lookAndFeelTool, package, profile, dark, successMessage, accentId](const QString &wallpaperBefore) {
            runTool(lookAndFeelTool, {QStringLiteral("--apply"), package},
                    [this, profile, dark, successMessage, wallpaperBefore, accentId](bool ok, const QString &output) {
                if (!ok) {
                    finishAppearance({}, {QStringLiteral("Não foi possível aplicar o estilo %1.").arg(profile.name) + lastLine(output)});
                    return;
                }
                // The process has finished; confirm the global theme before any
                // restoration. No timers race a later look-and-feel process.
                readDesktopState();
                readVisualStyle();
                if (m_visualStyle != profile.id || m_darkTheme != dark) {
                    finishAppearance({}, {QStringLiteral("O Plasma não confirmou o estilo %1.").arg(profile.name)});
                    return;
                }
                readWallpaper([this, profile, dark, successMessage, wallpaperBefore, accentId](const QString &wallpaperAfter) {
                    finishStyleProfile(profile, dark, successMessage,
                        wallpaperBefore != wallpaperAfter ? wallpaperBefore : QString(), accentId);
                });
            });
        });
    };
    if (accent != nullptr) validateScheme(*accent, dark, apply);
    else apply({});
}

void SystemService::finishStyleProfile(const VisualStyleProfile &profile, bool dark, const QString &successMessage,
                                      const QString &replacedWallpaper, const QString &accentId)
{
    QList<Command> followUps;
    const AccentProfile *accent = AccentProfiles::find(accentId);
    const QString icons = KConfigGroup(freshConfig(QStringLiteral("kdeglobals")), QStringLiteral("Icons"))
                              .readEntry("Theme", QString());
    const QString fallbackIcons = dark ? profile.fallbackDarkIcons : profile.fallbackLightIcons;
    if (accent == nullptr && !icons.isEmpty() && !iconThemeInstalled(icons) && iconThemeInstalled(fallbackIcons)) {
        followUps.append({findTool(QStringLiteral("plasma-changeicons")), {fallbackIcons}, QStringLiteral("ícones")});
    }
    const QString cursor = KConfigGroup(freshConfig(QStringLiteral("kcminputrc")), QStringLiteral("Mouse"))
                               .readEntry("cursorTheme", QString());
    if (!cursor.isEmpty() && !cursorThemeInstalled(cursor) && cursorThemeInstalled(profile.fallbackCursor)) {
        followUps.append({findTool(QStringLiteral("plasma-apply-cursortheme")), {profile.fallbackCursor}, QStringLiteral("cursor")});
    }
    if (accent != nullptr) {
        applyAccentScheme(*accent, dark, [this, accent, successMessage, followUps](const QString &error) {
            if (!error.isEmpty()) {
                finishAppearance({}, {error});
                return;
            }
            applyAccentExtras(*accent, successMessage, followUps);
        });
        return;
    }
    if (!replacedWallpaper.isEmpty()) {
        const QString wallpaper = QUrl(replacedWallpaper).toLocalFile();
        followUps.append({wallpaper.isEmpty() ? QString() : findTool(QStringLiteral("plasma-apply-wallpaperimage")),
                          {wallpaper}, QStringLiteral("papel de parede")});
    }
    runStyleFollowUps(followUps, {}, [this, successMessage](const QStringList &failures) {
        finishAppearance(successMessage, failures);
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
        const QString executable = QStandardPaths::findExecutable(kDriverManager);
        if (executable.isEmpty()) {
            setMessage(QStringLiteral("A Gestão de Drivers não está instalada."), true);
            return;
        }
        if (!QProcess::startDetached(executable, {})) {
            setMessage(QStringLiteral("Não foi possível abrir a Gestão de Drivers."), true);
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
    const bool opened = QDesktopServices::openUrl(parsed);
    setMessage(opened ? QStringLiteral("Abertura solicitada ao navegador.")
                      : QStringLiteral("Não foi possível abrir a URL no navegador padrão."), !opened);
    return opened;
}

bool SystemService::openDiscover(const QString &applicationId)
{
    if (!PackageService::isDiscoverApplication(applicationId)) {
        setMessage(QStringLiteral("Aplicativo não permitido para abertura no Discover."), true);
        return false;
    }
    const QString executable = findTool(QStringLiteral("plasma-discover"));
    if (executable.isEmpty()) {
        setMessage(QStringLiteral("O Discover não foi encontrado. Instale o KDE Discover para abrir este aplicativo."), true);
        return false;
    }
    // Discover owns the confirmation and installation, and must remain open
    // even if the user closes the Welcome. A successful launch is not an install.
    if (!QProcess::startDetached(executable, {QStringLiteral("--application"), QStringLiteral("appstream://") + applicationId})) {
        setMessage(QStringLiteral("Não foi possível abrir a página no Discover. Tente novamente."), true);
        return false;
    }
    setMessage(QStringLiteral("Abertura da página solicitada ao Discover. Confirme a instalação no Discover; "
                              "se o aplicativo não for encontrado, verifique o Flathub e tente novamente."));
    return true;
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
