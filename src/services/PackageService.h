#pragma once

#include <QProcess>
#include <QObject>
#include <QHash>
#include <QStringList>

class PackageService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool flatpakAvailable READ flatpakAvailable NOTIFY flatpakAvailableChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(QString currentApplication READ currentApplication NOTIFY operationChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY operationChanged)

public:
    // `flatpakProgram` overrides the executable found in PATH (used by tests).
    explicit PackageService(QObject *parent = nullptr, const QString &flatpakProgram = {});

    bool flatpakAvailable() const;
    QString lastMessage() const;
    QString currentApplication() const;
    bool busy() const;

    static bool isAllowedApplication(const QString &applicationId);

    bool isInstalled(const QString &applicationId) const;
    QString installation(const QString &applicationId) const;
    QString flatpakExecutable() const;

    // Installation goes through InstallService, which reports progress.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool remove(const QString &applicationId);

signals:
    void flatpakAvailableChanged();
    void installedApplicationsChanged();
    void lastMessageChanged();
    void operationChanged();
    void operationStarted(const QString &applicationId);
    void operationFinished(const QString &applicationId, bool success, const QString &message);

private:
    void setMessage(const QString &message);

    QProcess *m_process = nullptr;
    QString m_flatpakOverride;
    QHash<QString, QString> m_installations;
    QString m_lastMessage;
    QString m_currentApplication;
    bool m_flatpakAvailable = false;
};
