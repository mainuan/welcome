#pragma once

#include <QList>
#include <QString>

// One source of truth for the six Mainuan appearance choices. The swatch is
// presentation only; KDE scheme identifiers determine the selected color.
struct AccentProfile
{
    QString id;
    QString name;
    QString color;
    QString lightScheme;
    QString darkScheme;
    QString iconTheme;
    QString wallpaper;

    QString scheme(bool dark) const { return dark ? darkScheme : lightScheme; }
    QString avatar() const { return QStringLiteral("icons/") + iconTheme + QStringLiteral("/status/scalable/.face"); }
};

namespace AccentProfiles {

const QList<AccentProfile> &profiles();
const AccentProfile *find(const QString &id);
const AccentProfile *fromScheme(const QString &scheme);

} // namespace AccentProfiles
