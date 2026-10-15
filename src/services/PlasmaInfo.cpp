#include "PlasmaInfo.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

namespace PlasmaInfo {

void queryVersion(QObject *context, std::function<void(const QString &version)> done)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        done({});
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/MainApplication"),
                                                       QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    call << QStringLiteral("org.qtproject.Qt.QCoreApplication") << QStringLiteral("applicationVersion");
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call, 3000), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [watcher, done]() {
        const QDBusPendingReply<QDBusVariant> reply = *watcher;
        watcher->deleteLater();
        done(reply.isValid() ? reply.value().variant().toString().trimmed() : QString());
    });
}

}
