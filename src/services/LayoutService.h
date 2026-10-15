#pragma once

#include <QDBusPendingCall>
#include <QDateTime>
#include <QObject>
#include <QStringList>

#include <functional>

// Applies the six desktop layouts of Aparência > Layout do desktop with
// Plasma desktop scripting (org.kde.PlasmaShell.evaluateScript), keeps
// per-user backups of the panel configuration and restores them.
class LayoutService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString activeLayout READ activeLayout NOTIFY stateChanged)
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool hasBackup READ hasBackup NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(bool messageIsError READ messageIsError NOTIFY stateChanged)

public:
    struct Description
    {
        QString layout; // id when every panel carries the same tag, else empty
        int panels = -1;
        int launchers = -1;
        bool valid() const { return panels >= 0; }
    };

    // `configDir` and `backupDir` override $XDG_CONFIG_HOME and the backup
    // location (tests only).
    explicit LayoutService(QObject *parent = nullptr, const QString &configDir = {}, const QString &backupDir = {});

    static const QStringList &layoutIds();
    static bool isAllowedLayout(const QString &id);
    static QString displayName(const QString &id);
    // The Plasma script applying `id`; `installedWidgets` are plasmoid ids
    // found on disk (anything else is dropped).
    static QString buildScript(const QString &id, const QStringList &installedWidgets);
    static Description parseDescription(const QString &output);
    static QStringList installedPlasmoids();

    QString activeLayout() const;
    bool available() const;
    bool busy() const;
    bool hasBackup() const;
    QString message() const;
    bool messageIsError() const;

    // Copies the panel configuration into a new backup; returns its directory.
    QString createBackup();
    QStringList backups() const;
    bool restoreFiles(const QString &backupDirectory) const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void apply(const QString &id);
    Q_INVOKABLE void restoreBackup();

signals:
    void stateChanged();

private:
    void describe(std::function<void(const Description &)> done);
    void evaluate(const QString &script, std::function<void(bool ok, const QString &output)> done);
    void applyTiling(std::function<void(bool ok)> done);
    void runTilingScript(const QString &path, std::function<void(bool ok)> done);
    QDBusPendingCall callKWin(const QString &path, const QString &interface, const QString &method,
                              const QVariantList &arguments);
    void controlPlasma(const QString &action, std::function<void(bool ok)> done);
    void restartPlasmaWithBackup(const QString &backupDirectory, std::function<void(bool ok)> done);
    void waitForPlasma(int attempts, std::function<void(bool ok)> done);
    void finish(const QString &message, bool error);
    void setMessage(const QString &message, bool error);
    void pruneBackups(const QString &keep);

    QString m_configDir;
    QString m_backupDir;
    QString m_activeLayout;
    QString m_message;
    bool m_available = false;
    bool m_busy = false;
    bool m_messageIsError = false;
};
