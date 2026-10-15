#include "AccentProfile.h"

namespace AccentProfiles {

const QList<AccentProfile> &profiles()
{
    // File names as shipped by Mainuan 2026 in /usr/share/wallpapers.
    static const QList<AccentProfile> list = {
        {QStringLiteral("#d08040"), QStringLiteral("Laranja"), QStringLiteral("05marrom.png")},
        {QStringLiteral("#e8177d"), QStringLiteral("Rosa"), QStringLiteral("03magenta.png")},
        {QStringLiteral("#3daee9"), QStringLiteral("Azul"), QStringLiteral("01ciano.png")},
        {QStringLiteral("#3dd425"), QStringLiteral("Verde"), QStringLiteral("06lima.png")},
        {QStringLiteral("#aab6b9"), QStringLiteral("Cinza"), QStringLiteral("02cinza.png")},
        {QStringLiteral("#a588cb"), QStringLiteral("Lilás"), QStringLiteral("04purpura.png")},
    };
    return list;
}

const AccentProfile *find(const QString &color)
{
    const QString normalized = color.trimmed().toLower();
    for (const AccentProfile &profile : profiles()) {
        if (profile.color == normalized) {
            return &profile;
        }
    }
    return nullptr;
}

} // namespace AccentProfiles
