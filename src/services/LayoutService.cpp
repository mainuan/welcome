#include "LayoutService.h"

#include <KLazyLocalizedString>
#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#include <memory>

Q_LOGGING_CATEGORY(lcLayout, "mainuan.welcome.layout")

namespace {

// Sentence for a failed preflightMainuanLayout() check. The script runs inside
// plasmashell, where this application's catalog is not loaded, so it returns a
// stable code (plus parameters) and the sentence is composed here.
QString preflightMessage(const QJsonObject &result)
{
    const QString code = result.value(QStringLiteral("code")).toString();
    if (code == QStringLiteral("invalid-layout")) {
        return i18n("Invalid layout.");
    }
    if (code == QStringLiteral("missing-launcher")) {
        QStringList launchers;
        for (const QJsonValue &launcher : result.value(QStringLiteral("launchers")).toArray()) {
            launchers << launcher.toString();
        }
        // i18n: %1 is one or more Plasma widget ids joined by " or ".
        return i18n("Required launcher missing: %1. The previous panels were preserved.",
                    launchers.join(i18nc("separator between alternative widget ids", " or ")));
    }
    if (code == QStringLiteral("missing-widget")) {
        // i18n: %1 is a Plasma widget id, e.g. org.kde.plasma.kickoff.
        return i18n("Required widget missing: %1. The previous panels were preserved.",
                    result.value(QStringLiteral("widget")).toString());
    }
    if (code == QStringLiteral("no-screen")) {
        return i18n("No screen available in Plasma.");
    }
    if (code == QStringLiteral("screen-geometry")) {
        return i18n("Could not query the screen geometry.");
    }
    return i18n("Could not check the required widgets; the previous panels were preserved.");
}

// Files that hold the panels and their widgets.
const QStringList kLayoutFiles = {QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc"),
                                  QStringLiteral("plasmashellrc")};
constexpr int kKeptBackups = 5;
constexpr int kScriptTimeoutMs = 20000;
const QString kTilingScriptName = QStringLiteral("mainuan-welcome-tiling");
const QString kScripting = QStringLiteral("org.kde.kwin.Scripting");

QDBusMessage plasmaCall(const QString &method)
{
    return QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
                                          QStringLiteral("org.kde.PlasmaShell"), method);
}

QString resourceText(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

QString scriptLibrary()
{
    const QString reference = resourceText(QStringLiteral(":/layouts/reference-layouts.js"));
    const QString desktop = resourceText(QStringLiteral(":/layouts/desktop-layouts.js"));
    return reference.isEmpty() || desktop.isEmpty() ? QString() : reference + QLatin1Char('\n') + desktop;
}

QString widgetPrelude(const QStringList &installedWidgets)
{
    static const QRegularExpression pluginId(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$"));
    QJsonArray widgets;
    for (const QString &widget : installedWidgets) {
        if (pluginId.match(widget).hasMatch()) widgets.append(widget);
    }
    return QStringLiteral("var installedWidgets = %1;\n")
        .arg(QString::fromUtf8(QJsonDocument(widgets).toJson(QJsonDocument::Compact)));
}

} // namespace

LayoutService::LayoutService(QObject *parent, const QString &configDir, const QString &backupDir, Backend backend)
    : QObject(parent)
    , m_configDir(configDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) : configDir)
    , m_backend(std::move(backend))
    , m_backupDir(backupDir.isEmpty()
                      ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                            + QStringLiteral("/mainuan-welcome/layout-backups")
                      : backupDir)
{
}

const QStringList &LayoutService::layoutIds()
{
    static const QStringList ids = {QStringLiteral("plasma-default"), QStringLiteral("three-panels"),
                                    QStringLiteral("single-bar"), QStringLiteral("tiling"),
                                    QStringLiteral("panel-top"),
                                    QStringLiteral("unity"), QStringLiteral("minimal")};
    return ids;
}

bool LayoutService::isAllowedLayout(const QString &id)
{
    return layoutIds().contains(id);
}

QString LayoutService::displayName(const QString &id)
{
    if (id == QStringLiteral("unity")) {
        return QStringLiteral("Lomiri"); // product name, never translated
    }
    static const QHash<QString, KLazyLocalizedString> names = {
        {QStringLiteral("plasma-default"), kli18nc("desktop layout name", "Classic — 5 panels")},
        {QStringLiteral("three-panels"), kli18nc("desktop layout name", "3 panels")},
        {QStringLiteral("single-bar"), kli18nc("desktop layout name", "Single panel")},
        {QStringLiteral("panel-top"), kli18nc("desktop layout name", "Top panel")},
        {QStringLiteral("floating"), kli18nc("desktop layout name", "Floating")},
        // i18n: "W7" refers to a Windows 7 style taskbar.
        {QStringLiteral("minimal"), kli18nc("desktop layout name", "W7 panel")},
        {QStringLiteral("tiling"), kli18nc("desktop layout name", "Tiling")}};
    const auto name = names.constFind(id);
    return name == names.constEnd() ? QString() : name->toString();
}

QString LayoutService::buildScript(const QString &id, const QStringList &installedWidgets)
{
    if (!isAllowedLayout(id)) {
        return {};
    }
    const QString library = scriptLibrary();
    if (library.isEmpty()) {
        return {};
    }
    // Both values are JSON-encoded; `id` is also from the allowlist above.
    return widgetPrelude(installedWidgets) + QStringLiteral("%1\napplyMainuanLayout(%2);\n")
        .arg(library,
             QString::fromUtf8(QJsonDocument(QJsonArray{id}).toJson(QJsonDocument::Compact)).mid(1).chopped(1));
}

QString LayoutService::buildPreflightScript(const QString &id, const QStringList &installedWidgets)
{
    QString script = buildScript(id, installedWidgets);
    if (script.isEmpty()) return {};
    const QString action = QStringLiteral("applyMainuanLayout(\"") + id + QStringLiteral("\");\n");
    script.chop(action.size());
    return script + QStringLiteral("print(JSON.stringify(preflightMainuanLayout(\"") + id + QStringLiteral("\")));\n");
}

LayoutService::Description LayoutService::parseDescription(const QString &output)
{
    Description description;
    if (output.trimmed().startsWith(QLatin1Char('{'))) {
        const QJsonDocument document = QJsonDocument::fromJson(output.toUtf8());
        if (!document.isObject()) return description;
        const QJsonObject object = document.object();
        if (!object.value(QStringLiteral("structureOk")).isBool()) return description;
        description.panels = object.value(QStringLiteral("panels")).toInt(-1);
        description.launchers = object.value(QStringLiteral("launchers")).toInt(-1);
        description.screens = object.value(QStringLiteral("screens")).toInt(-1);
        description.matchingLaunchers = object.value(QStringLiteral("matchingLaunchers")).toInt(-1);
        description.structureOk = object.value(QStringLiteral("structureOk")).toBool();
        const QString id = object.value(QStringLiteral("layout")).toString();
        description.layout = isAllowedLayout(id) || id == QStringLiteral("floating") ? id : QString();
        if (description.panels < 0 || description.launchers < 0 || description.screens < 1 || description.matchingLaunchers < 0) return {};
        return description;
    }
    // Accept old three-field diagnostics, but they cannot confirm an application.
    const QStringList fields = output.trimmed().split(QLatin1Char('|'));
    if (fields.size() != 3) {
        return description;
    }
    bool panelsOk = false;
    bool launchersOk = false;
    const int panels = fields.at(1).toInt(&panelsOk);
    const int launchers = fields.at(2).toInt(&launchersOk);
    if (!panelsOk || !launchersOk || panels < 0 || launchers < 0) {
        return description;
    }
    description.layout = isAllowedLayout(fields.at(0)) || fields.at(0) == QStringLiteral("floating") ? fields.at(0) : QString();
    description.panels = panels;
    description.launchers = launchers;
    return description;
}

bool LayoutService::matchesLayout(const Description &description, const QString &id)
{
    if ((!isAllowedLayout(id) && id != QStringLiteral("floating")) || !description.valid() || description.layout != id || !description.structureOk
        || description.screens < 1 || description.matchingLaunchers != description.screens) return false;
    const qint64 screens = description.screens;
    const int perScreen = id == QStringLiteral("three-panels") ? 3 : id == QStringLiteral("unity") ? 2 : 1;
    if (id == QStringLiteral("plasma-default")) return description.panels == 5 * screens;
    return description.panels == perScreen * screens;
}

QStringList LayoutService::installedPlasmoids()
{
    QStringList ids;
    const QStringList roots = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("plasma/plasmoids"),
                                                        QStandardPaths::LocateDirectory);
    for (const QString &root : roots) {
        ids << QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    }
    ids.removeDuplicates();
    return ids;
}

QString LayoutService::activeLayout() const { return m_activeLayout; }
bool LayoutService::available() const { return m_available; }
bool LayoutService::busy() const { return m_busy; }
QString LayoutService::message() const { return m_message; }
bool LayoutService::messageIsError() const { return m_messageIsError; }

bool LayoutService::hasBackup() const
{
    return !backups().isEmpty();
}

QStringList LayoutService::backups() const
{
    QStringList names = QDir(m_backupDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QStringList complete;
    for (const QString &name : std::as_const(names)) {
        if (QFile::exists(m_backupDir + QLatin1Char('/') + name + QLatin1Char('/') + kLayoutFiles.constFirst())) {
            complete << m_backupDir + QLatin1Char('/') + name;
        }
    }
    return complete; // oldest first
}

// Backups are named "<sequence>-<UTC time>": the sequence keeps them in order
// even when the clock goes backwards (dual boot with the RTC in local time).
QString LayoutService::createBackup()
{
    const QStringList existing = backups();
    const qulonglong sequence = existing.isEmpty()
        ? 1
        : QFileInfo(existing.constLast()).fileName().section(QLatin1Char('-'), 0, 0).toULongLong() + 1;
    const QString directory = m_backupDir + QLatin1Char('/') + QStringLiteral("%1-%2")
        .arg(sequence, 8, 10, QLatin1Char('0'))
        .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddTHHmmsszzz")));
    if (QFileInfo::exists(directory) || !QDir().mkpath(directory)) {
        return {};
    }
    for (const QString &name : kLayoutFiles) {
        const QString source = m_configDir + QLatin1Char('/') + name;
        if (QFile::exists(source) && !QFile::copy(source, directory + QLatin1Char('/') + name)) {
            QDir(directory).removeRecursively();
            return {};
        }
    }
    if (!QFile::exists(directory + QLatin1Char('/') + kLayoutFiles.constFirst())) {
        QDir(directory).removeRecursively();
        return {};
    }
    pruneBackups(directory);
    qCInfo(lcLayout) << "layout backup" << directory;
    return directory;
}

// Keeps the first backup (the panels the user had before ever using the
// Welcome), the one just made and the newest others, up to kKeptBackups.
void LayoutService::pruneBackups(const QString &keep)
{
    QStringList existing = backups();
    if (existing.size() <= kKeptBackups) {
        return;
    }
    const QString original = existing.takeFirst();
    existing.removeAll(keep);
    qsizetype excess = existing.size() + 1 + (keep != original ? 1 : 0) - kKeptBackups;
    while (excess-- > 0 && !existing.isEmpty()) {
        QDir(existing.takeFirst()).removeRecursively();
    }
}

// Each file is replaced atomically (QSaveFile), so a full disk never leaves
// Plasma without its panel configuration. A backup without the applets file
// is not a backup.
bool LayoutService::restoreFiles(const QString &backupDirectory) const
{
    if (!QFile::exists(backupDirectory + QLatin1Char('/') + kLayoutFiles.constFirst())) {
        return false;
    }
    bool ok = true;
    for (const QString &name : kLayoutFiles) {
        QFile source(backupDirectory + QLatin1Char('/') + name);
        if (!source.exists()) {
            continue;
        }
        QSaveFile target(m_configDir + QLatin1Char('/') + name);
        const bool copied = source.open(QIODevice::ReadOnly) && target.open(QIODevice::WriteOnly)
            && target.write(source.readAll()) == source.size() && target.commit();
        if (!copied) {
            target.cancelWriting();
            qCWarning(lcLayout) << "could not restore" << name << target.errorString();
        }
        ok = copied && ok;
    }
    return ok;
}

void LayoutService::setMessage(const QString &message, bool error)
{
    m_message = message;
    m_messageIsError = error;
    emit stateChanged();
}

void LayoutService::finish(const QString &message, bool error)
{
    m_busy = false;
    if (error) {
        qCWarning(lcLayout).noquote() << message;
    } else {
        qCInfo(lcLayout).noquote() << message;
    }
    setMessage(message, error);
}

void LayoutService::evaluate(const QString &script, std::function<void(bool, const QString &)> done)
{
    if (m_backend.evaluate) {
        m_backend.evaluate(script, std::move(done));
        return;
    }
    QDBusMessage call = plasmaCall(QStringLiteral("evaluateScript"));
    call << script;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call, kScriptTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [done](QDBusPendingCallWatcher *pending) {
        const QDBusPendingReply<QString> reply = *pending;
        pending->deleteLater();
        if (reply.isError()) {
            done(false, reply.error().message());
        } else {
            done(true, reply.value());
        }
    });
}

void LayoutService::describe(std::function<void(const Description &)> done)
{
    const QString library = scriptLibrary();
    evaluate(widgetPrelude(installedPlasmoids()) + library + QStringLiteral("\ndescribeMainuanLayout();\n"), [done](bool ok, const QString &output) {
        done(ok ? parseDescription(output) : Description());
    });
}

void LayoutService::refresh()
{
    if (m_busy) return;
    describe([this](const Description &description) {
        if (m_busy) return; // Ignore a refresh started before an application.
        m_available = description.valid();
        m_activeLayout = matchesLayout(description, description.layout) ? description.layout : QString();
        emit stateChanged();
    });
}

void LayoutService::settleLayout(const QString &id, int attempts, const QString &previous,
                                std::function<void(bool, const QString &)> done)
{
    // Let Plasma measure the applets before normalizing every panel's
    // thickness and positioning neighboring custom segments. Two equal
    // samples avoid declaring success on initial sizes.
    QTimer::singleShot(200, this, [this, id, attempts, previous, done]() {
        const QString script = widgetPrelude(installedPlasmoids()) + scriptLibrary()
            + QStringLiteral("\nprint(JSON.stringify(positionMainuanLayout(\"%1\")));\n").arg(id);
        evaluate(script, [this, id, attempts, previous, done](bool ok, const QString &output) {
            const QJsonObject result = QJsonDocument::fromJson(output.toUtf8()).object();
            const QString signature = result.value(QStringLiteral("signature")).toString();
            if (!ok || signature.isEmpty()) {
                done(false, i18n("Could not measure the layout segments."));
            } else if (result.value(QStringLiteral("ok")).toBool() && signature == previous) {
                done(true, {});
            } else if (attempts > 1) {
                settleLayout(id, attempts - 1, signature, done);
            } else {
                done(false, i18n("The segments did not settle without overlapping on this screen."));
            }
        });
    });
}

void LayoutService::apply(const QString &id)
{
    if (!isAllowedLayout(id)) {
        setMessage(i18n("Invalid layout."), true);
        return;
    }
    if (m_busy) {
        return;
    }
    if (!m_available) {
        setMessage(i18n("Plasma did not respond; open the Welcome inside a KDE Plasma session."), true);
        return;
    }

    const QString name = displayName(id);
    const QStringList widgets = installedPlasmoids();
    m_busy = true;
    setMessage(i18n("Checking the launcher of the %1 layout…", name), false);
    const QString preflight = buildPreflightScript(id, widgets);
    if (preflight.isEmpty()) {
        finish(i18n("Could not load the layout scripts; nothing was changed."), true);
        return;
    }
    evaluate(preflight, [this, id, name, widgets](bool checked, const QString &output) {
        const QJsonObject result = QJsonDocument::fromJson(output.toUtf8()).object();
        if (!checked || !result.value(QStringLiteral("ok")).toBool()) {
            finish(preflightMessage(checked ? result : QJsonObject()), true);
            return;
        }
        const QString backup = createBackup();
        if (backup.isEmpty()) {
            finish(i18n("Could not save a copy of the current layout; nothing was changed."), true);
            return;
        }
        setMessage(i18n("Applying the %1 layout…", name), false);
        const auto rollBack = [this, backup](const QString &reason) {
            qCWarning(lcLayout).noquote() << reason << "- restoring" << backup;
            restartPlasmaWithBackup(backup, [this, reason](bool restored) {
                // i18n: %1 is a complete sentence explaining why the layout failed.
                finish(restored ? i18n("%1 The previous layout was restored.", reason)
                    // i18n: “Restore Previous Layout” is the button on the Appearance page.
                    : i18n("%1 Could not restore automatically; use “Restore Previous Layout”.", reason), true);
                refresh();
            });
        };
        evaluate(buildScript(id, widgets), [this, id, name, rollBack](bool ok, const QString &applied) {
            const QString prefix = QStringLiteral("MAINUAN_PREFLIGHT_ERROR:");
            if (ok && applied.trimmed().startsWith(prefix)) {
                finish(preflightMessage(QJsonDocument::fromJson(applied.trimmed().mid(prefix.size()).toUtf8()).object()), true);
                return;
            }
            if (!ok) {
                // i18n: %2 is the raw error reported by Plasma.
                rollBack(i18n("Could not apply the %1 layout (%2).", name, applied.trimmed()));
                return;
            }
            settleLayout(id, 10, {}, [this, id, name, rollBack](bool settled, const QString &reason) {
                if (!settled) {
                    rollBack(reason);
                    return;
                }
                describe([this, id, name, rollBack](const Description &description) {
                    if (!matchesLayout(description, id)) {
                        rollBack(i18n("The %1 layout was not completed.", name));
                        return;
                    }
                    m_activeLayout = id;
                    if (id != QStringLiteral("tiling")) {
                        finish(i18n("%1 layout applied.", name), false);
                        return;
                    }
                    applyTiling([this, name](bool tilesOk) {
                        finish(tilesOk ? i18n("%1 layout applied. Drag windows while holding Shift to snap them into the tiles.", name)
                            : i18n("%1 layout applied, but the KWin tiles could not be configured.", name), !tilesOk);
                    });
                });
            });
        });
    });
}

QDBusPendingCall LayoutService::callKWin(const QString &path, const QString &interface, const QString &method,
                                         const QVariantList &arguments)
{
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), path, interface, method);
    call.setArguments(arguments);
    return QDBusConnection::sessionBus().asyncCall(call, 5000);
}

void LayoutService::applyTiling(std::function<void(bool)> done)
{
    // KWin reads scripts from a file: keep a private copy in the runtime dir.
    const QString path = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
        + QStringLiteral("/mainuan-welcome-tiling.js");
    QFile file(path);
    const QByteArray script = resourceText(QStringLiteral(":/layouts/kwin-tiling.js")).toUtf8();
    if (script.isEmpty() || !file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(script) != script.size()) {
        done(false);
        return;
    }
    file.close();

    // Unload a copy left by an earlier run first; loadScript refuses a loaded name.
    auto *unload = new QDBusPendingCallWatcher(
        callKWin(QStringLiteral("/Scripting"), kScripting, QStringLiteral("unloadScript"), {kTilingScriptName}), this);
    connect(unload, &QDBusPendingCallWatcher::finished, this, [this, path, done](QDBusPendingCallWatcher *pending) {
        pending->deleteLater();
        runTilingScript(path, done);
    });
}

void LayoutService::runTilingScript(const QString &path, std::function<void(bool)> done)
{
    auto *load = new QDBusPendingCallWatcher(
        callKWin(QStringLiteral("/Scripting"), kScripting, QStringLiteral("loadScript"), {path, kTilingScriptName}), this);
    connect(load, &QDBusPendingCallWatcher::finished, this, [this, path, done](QDBusPendingCallWatcher *pending) {
        const QDBusPendingReply<int> reply = *pending;
        pending->deleteLater();
        if (reply.isError() || reply.value() < 0) {
            qCWarning(lcLayout) << "KWin did not load the tiling script:" << reply.error().message();
            QFile::remove(path);
            done(false);
            return;
        }
        auto *run = new QDBusPendingCallWatcher(
            callKWin(QStringLiteral("/Scripting/Script%1").arg(reply.value()), QStringLiteral("org.kde.kwin.Script"),
                     QStringLiteral("run"), {}),
            this);
        connect(run, &QDBusPendingCallWatcher::finished, this, [this, path, done](QDBusPendingCallWatcher *running) {
            const bool ok = !running->isError();
            running->deleteLater();
            // The script runs once; unload it shortly after so it can be loaded again.
            QTimer::singleShot(1500, this, [this, path]() {
                callKWin(QStringLiteral("/Scripting"), kScripting, QStringLiteral("unloadScript"), {kTilingScriptName});
                QFile::remove(path);
            });
            done(ok);
        });
    });
}

void LayoutService::restoreBackup()
{
    const QStringList existing = backups();
    if (m_busy || existing.isEmpty()) {
        return;
    }
    m_busy = true;
    setMessage(i18n("Restoring the previous layout; Plasma will be restarted…"), false);
    restartPlasmaWithBackup(existing.constLast(), [this](bool ok) {
        finish(ok ? i18n("Previous layout restored.")
                  : i18n("Could not restore the previous layout."),
               !ok);
        refresh();
    });
}

// `systemctl --user <action> plasma-plasmashell.service`; `done` runs exactly
// once, also when systemctl cannot start or hangs (killed after 60 s).
void LayoutService::controlPlasma(const QString &action, std::function<void(bool ok)> done)
{
    if (m_backend.controlPlasma) {
        m_backend.controlPlasma(action, std::move(done));
        return;
    }
    const QString systemctl = QStandardPaths::findExecutable(QStringLiteral("systemctl"));
    if (systemctl.isEmpty()) {
        done(false);
        return;
    }
    auto *process = new QProcess(this);
    auto finished = std::make_shared<bool>(false);
    const auto complete = [process, finished, done](bool ok) {
        if (*finished) {
            return;
        }
        *finished = true;
        process->deleteLater();
        done(ok);
    };
    process->setStandardInputFile(QProcess::nullDevice());
    connect(process, &QProcess::finished, this, [complete](int exitCode, QProcess::ExitStatus status) {
        complete(status == QProcess::NormalExit && exitCode == 0);
    });
    connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            complete(false);
        }
    });
    QTimer::singleShot(60000, process, [process]() { process->kill(); });
    process->start(systemctl, {QStringLiteral("--user"), action, QStringLiteral("plasma-plasmashell.service")});
}

// Plasma keeps the panel configuration in memory and rewrites the files, so a
// file restore needs plasmashell stopped, the files copied and Plasma started.
void LayoutService::restartPlasmaWithBackup(const QString &backupDirectory, std::function<void(bool)> done)
{
    controlPlasma(QStringLiteral("stop"), [this, backupDirectory, done](bool stopped) {
        const bool copied = stopped && restoreFiles(backupDirectory);
        qCInfo(lcLayout) << "restored files from" << backupDirectory << copied;
        // Start Plasma again even if copying failed, so the session keeps a shell.
        controlPlasma(QStringLiteral("start"), [this, copied, done](bool started) {
            if (!started) {
                done(false);
                return;
            }
            waitForPlasma(15, [copied, done](bool running) { done(copied && running); });
        });
    });
}

void LayoutService::waitForPlasma(int attempts, std::function<void(bool)> done)
{
    describe([this, attempts, done](const Description &description) {
        if (description.valid() && description.panels > 0) {
            done(true);
        } else if (attempts <= 1) {
            done(false);
        } else {
            QTimer::singleShot(1000, this, [this, attempts, done]() { waitForPlasma(attempts - 1, done); });
        }
    });
}
