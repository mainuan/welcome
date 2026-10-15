#include "LayoutService.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#include <memory>

Q_LOGGING_CATEGORY(lcLayout, "mainuan.welcome.layout")

namespace {

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

} // namespace

LayoutService::LayoutService(QObject *parent, const QString &configDir, const QString &backupDir)
    : QObject(parent)
    , m_configDir(configDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) : configDir)
    , m_backupDir(backupDir.isEmpty()
                      ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                            + QStringLiteral("/mainuan-welcome/layout-backups")
                      : backupDir)
{
}

const QStringList &LayoutService::layoutIds()
{
    static const QStringList ids = {QStringLiteral("plasma-default"), QStringLiteral("panel-top"),
                                    QStringLiteral("floating"),       QStringLiteral("minimal"),
                                    QStringLiteral("unity"),          QStringLiteral("tiling")};
    return ids;
}

bool LayoutService::isAllowedLayout(const QString &id)
{
    return layoutIds().contains(id);
}

QString LayoutService::displayName(const QString &id)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("plasma-default"), QStringLiteral("Plasma padrão")},
        {QStringLiteral("panel-top"), QStringLiteral("Painel superior")},
        {QStringLiteral("floating"), QStringLiteral("Flutuante")},
        {QStringLiteral("minimal"), QStringLiteral("Minimalista")},
        {QStringLiteral("unity"), QStringLiteral("Unity-like")},
        {QStringLiteral("tiling"), QStringLiteral("Tiling")}};
    return names.value(id);
}

QString LayoutService::buildScript(const QString &id, const QStringList &installedWidgets)
{
    if (!isAllowedLayout(id)) {
        return {};
    }
    static const QRegularExpression pluginId(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$"));
    QJsonArray widgets;
    for (const QString &widget : installedWidgets) {
        if (pluginId.match(widget).hasMatch()) {
            widgets.append(widget);
        }
    }
    const QString library = resourceText(QStringLiteral(":/layouts/desktop-layouts.js"));
    if (library.isEmpty()) {
        return {};
    }
    // Both values are JSON-encoded; `id` is also from the allowlist above.
    return QStringLiteral("var installedWidgets = %1;\n%2\napplyMainuanLayout(%3);\n")
        .arg(QString::fromUtf8(QJsonDocument(widgets).toJson(QJsonDocument::Compact)), library,
             QString::fromUtf8(QJsonDocument(QJsonArray{id}).toJson(QJsonDocument::Compact)).mid(1).chopped(1));
}

LayoutService::Description LayoutService::parseDescription(const QString &output)
{
    Description description;
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
    description.layout = isAllowedLayout(fields.at(0)) ? fields.at(0) : QString();
    description.panels = panels;
    description.launchers = launchers;
    return description;
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
    const QString library = resourceText(QStringLiteral(":/layouts/desktop-layouts.js"));
    evaluate(library + QStringLiteral("\ndescribeMainuanLayout();\n"), [done](bool ok, const QString &output) {
        done(ok ? parseDescription(output) : Description());
    });
}

void LayoutService::refresh()
{
    describe([this](const Description &description) {
        m_available = description.valid();
        m_activeLayout = description.layout;
        emit stateChanged();
    });
}

void LayoutService::apply(const QString &id)
{
    if (!isAllowedLayout(id)) {
        setMessage(QStringLiteral("Layout inválido."), true);
        return;
    }
    if (m_busy) {
        return;
    }
    if (!m_available) {
        setMessage(QStringLiteral("O Plasma não respondeu; abra o Welcome dentro de uma sessão do KDE Plasma."), true);
        return;
    }

    const QString name = displayName(id);
    const QString backup = createBackup();
    if (backup.isEmpty()) {
        setMessage(QStringLiteral("Não foi possível guardar uma cópia do layout atual; nada foi alterado."), true);
        return;
    }

    m_busy = true;
    setMessage(QStringLiteral("Aplicando layout %1…").arg(name), false);
    qCInfo(lcLayout) << "applying layout" << id;

    const auto rollBack = [this, backup](const QString &reason) {
        qCWarning(lcLayout).noquote() << reason << "- restoring" << backup;
        restartPlasmaWithBackup(backup, [this, reason](bool restored) {
            finish(reason + (restored ? QStringLiteral(" O layout anterior foi restaurado.")
                                      : QStringLiteral(" Não foi possível restaurar automaticamente; use “Restaurar layout anterior”.")),
                   true);
            refresh();
        });
    };

    evaluate(buildScript(id, installedPlasmoids()), [this, id, name, rollBack](bool ok, const QString &output) {
        if (!ok) {
            rollBack(QStringLiteral("Não foi possível aplicar o layout %1 (%2).").arg(name, output.trimmed()));
            return;
        }
        describe([this, id, name, rollBack](const Description &description) {
            // Never leave the user without a panel holding an application launcher.
            if (!description.valid() || description.layout != id || description.panels < 1 || description.launchers < 1) {
                rollBack(QStringLiteral("O layout %1 não ficou completo.").arg(name));
                return;
            }
            m_activeLayout = id;
            if (id != QStringLiteral("tiling")) {
                finish(QStringLiteral("Layout %1 aplicado.").arg(name), false);
                return;
            }
            applyTiling([this, name](bool tilesOk) {
                finish(tilesOk ? QStringLiteral("Layout %1 aplicado. Arraste janelas com Shift pressionado para encaixá-las nos blocos.").arg(name)
                               : QStringLiteral("Layout %1 aplicado, mas os blocos do KWin não puderam ser configurados.").arg(name),
                       !tilesOk);
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
    setMessage(QStringLiteral("Restaurando o layout anterior; o Plasma será reiniciado…"), false);
    restartPlasmaWithBackup(existing.constLast(), [this](bool ok) {
        finish(ok ? QStringLiteral("Layout anterior restaurado.")
                  : QStringLiteral("Não foi possível restaurar o layout anterior."),
               !ok);
        refresh();
    });
}

// `systemctl --user <action> plasma-plasmashell.service`; `done` runs exactly
// once, also when systemctl cannot start or hangs (killed after 60 s).
void LayoutService::controlPlasma(const QString &action, std::function<void(bool ok)> done)
{
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
