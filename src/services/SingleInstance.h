#pragma once

#include <QLocalServer>
#include <QObject>

class SingleInstance final : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);
    bool tryAcquire();

signals:
    // `activationToken` is the XDG activation token the launcher gave the
    // second instance; Wayland compositors require it to raise the window.
    void activationRequested(const QString &activationToken);

private:
    QString serverName() const;
    QLocalServer m_server;
    QString m_name;
};
