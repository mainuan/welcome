#pragma once

#include <KLazyLocalizedString>

#include <QList>
#include <QString>

// One source of truth for the six Mainuan appearance choices. The swatch is
// presentation only; KDE scheme identifiers determine the selected color.
struct AccentProfile
{
    QString id;
    KLazyLocalizedString name; // translated with name.toString()
    QString color;
    QString lightScheme;
    QString darkScheme;
    QString iconTheme;
    QString wallpaper;

    QString scheme(bool dark) const { return dark ? darkScheme : lightScheme; }
    // Official avatar of this color, relative to /usr/share (mainuan-identity).
    QString avatar() const { return QStringLiteral("mainuan/identity/avatars/") + id + QStringLiteral(".svg"); }
};

namespace AccentProfiles {

const QList<AccentProfile> &profiles();
const AccentProfile *find(const QString &id);
const AccentProfile *fromScheme(const QString &scheme);

} // namespace AccentProfiles
