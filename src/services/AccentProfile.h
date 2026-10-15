#pragma once

#include <QList>
#include <QString>

// The six accent colors offered by Aparência > Cor de destaque, each paired
// with the Mainuan wallpaper in the same color (share/wallpapers).
struct AccentProfile
{
    QString color;     // lower-case #rrggbb, as sent by QML
    QString name;      // label shown to the user
    QString wallpaper; // file name in share/wallpapers
};

namespace AccentProfiles {

const QList<AccentProfile> &profiles();
// Profile for `color` (case-insensitive #rrggbb), or nullptr.
const AccentProfile *find(const QString &color);

} // namespace AccentProfiles
