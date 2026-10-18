#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

// One pinned vendor artifact, never an arbitrary .deb installer.
struct FreeOfficeProfile
{
    QUrl url;
    QString package;
    QString version;
    QString architecture;
    qint64 size = 0;
    QByteArray sha256;

    static FreeOfficeProfile official();
    bool isValid() const;
    bool acceptsUrl(const QUrl &candidate) const;
    bool matchesMetadata(const QByteArray &metadata) const;
};
