#include "FreeOfficeProfile.h"

#include <QFile>
#include <QRegularExpression>

FreeOfficeProfile FreeOfficeProfile::official()
{
    Q_INIT_RESOURCE(installation_profiles);
    QFile file(QStringLiteral(":/installation/freeoffice.profile"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QList<QByteArray> fields = file.readAll().trimmed().split('\n');
    if (fields.size() != 6) {
        return {};
    }
    return {QUrl(QString::fromUtf8(fields[0])), QString::fromUtf8(fields[1]), QString::fromUtf8(fields[2]),
            QString::fromUtf8(fields[3]), fields[4].toLongLong(), fields[5]};
}

bool FreeOfficeProfile::isValid() const
{
    return url == QUrl(QStringLiteral("https://www.softmaker.net/down/softmaker-freeoffice-2024_1238-01_amd64.deb"))
        && package == QStringLiteral("softmaker-freeoffice-2024") && version == QStringLiteral("3714")
        && architecture == QStringLiteral("amd64") && size > 0 && size < 512LL * 1024 * 1024
        && QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(QString::fromLatin1(sha256)).hasMatch();
}

bool FreeOfficeProfile::acceptsUrl(const QUrl &candidate) const
{
    // The current official URL returns 200 directly. Redirects are deliberately refused.
    return isValid() && candidate == url && candidate.userInfo().isEmpty()
        && candidate.fragment().isEmpty() && candidate.query().isEmpty();
}

bool FreeOfficeProfile::matchesMetadata(const QByteArray &metadata) const
{
    return metadata == package.toUtf8() + '\n' + version.toUtf8() + '\n' + architecture.toUtf8() + '\n';
}
