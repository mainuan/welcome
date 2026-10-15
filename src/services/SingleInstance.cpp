#include "SingleInstance.h"

#include <QDir>
#include <QLocalSocket>
#include <QStandardPaths>

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection, this, [this]() {
        while (m_server.hasPendingConnections()) {
            QLocalSocket *socket = m_server.nextPendingConnection();
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
                // "activate" optionally followed by " <token>"; tokens are short ASCII.
                const QByteArray message = socket->readAll().left(512).trimmed();
                QString token;
                if (message.startsWith("activate ")) {
                    token = QString::fromLatin1(message.mid(9)).trimmed();
                }
                emit activationRequested(token);
                socket->disconnectFromServer();
            });
            connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        }
    });
}

QString SingleInstance::serverName() const
{
    // $XDG_RUNTIME_DIR is private to the user and cleared at logout.
    QString directory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (directory.isEmpty()) {
        directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    return QDir(directory).filePath(QStringLiteral("mainuan-welcome.sock"));
}

bool SingleInstance::tryAcquire()
{
    m_name = serverName();
    QDir().mkpath(QFileInfo(m_name).absolutePath());

    QLocalSocket client;
    client.connectToServer(m_name);
    if (client.waitForConnected(150)) {
        const QByteArray token = qgetenv("XDG_ACTIVATION_TOKEN");
        client.write(token.isEmpty() ? QByteArray("activate") : QByteArray("activate ") + token);
        client.waitForBytesWritten(100);
        client.disconnectFromServer();
        return false;
    }

    QLocalServer::removeServer(m_name);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(m_name)) {
        // Failing to create the local socket must not prevent a user from opening the app.
        return true;
    }
    return true;
}
