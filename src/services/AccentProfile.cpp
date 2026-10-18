#include "AccentProfile.h"

namespace AccentProfiles {

const QList<AccentProfile> &profiles()
{
    static const QList<AccentProfile> list = {
        {QStringLiteral("brown"), QStringLiteral("Marrom"), QStringLiteral("#d08040"),
         QStringLiteral("DreamBrownLightColor"), QStringLiteral("DreamBrownDarkColor"), QStringLiteral("kora-brown"), QStringLiteral("05marrom.png")},
        {QStringLiteral("cyan"), QStringLiteral("Ciano"), QStringLiteral("#3daee9"),
         QStringLiteral("DreamCyanLightColor"), QStringLiteral("DreamCyanDarkColor"), QStringLiteral("kora-cyan"), QStringLiteral("01ciano.png")},
        {QStringLiteral("magenta"), QStringLiteral("Magenta"), QStringLiteral("#e8177d"),
         QStringLiteral("DreamMagentaLightColor"), QStringLiteral("DreamMagentaDarkColor"), QStringLiteral("kora-magenta"), QStringLiteral("03magenta.png")},
        {QStringLiteral("lime"), QStringLiteral("Lima"), QStringLiteral("#3dd425"),
         QStringLiteral("DreamLimeLightColor"), QStringLiteral("DreamLimeDarkColor"), QStringLiteral("kora-lima"), QStringLiteral("06lima.png")},
        {QStringLiteral("gray"), QStringLiteral("Cinza"), QStringLiteral("#aab6b9"),
         QStringLiteral("DreamGrayLightColor"), QStringLiteral("DreamGrayDarkColor"), QStringLiteral("kora-grey"), QStringLiteral("02cinza.png")},
        {QStringLiteral("violet"), QStringLiteral("Violeta"), QStringLiteral("#a588cb"),
         QStringLiteral("DreamVioletLightColor"), QStringLiteral("DreamVioletDarkColor"), QStringLiteral("kora-violet"), QStringLiteral("04violet.png")},
    };
    return list;
}

const AccentProfile *find(const QString &id)
{
    for (const AccentProfile &profile : profiles()) {
        if (profile.id == id) {
            return &profile;
        }
    }
    return nullptr;
}

const AccentProfile *fromScheme(const QString &scheme)
{
    for (const AccentProfile &profile : profiles()) {
        if (profile.lightScheme == scheme || profile.darkScheme == scheme) {
            return &profile;
        }
    }
    return nullptr;
}

} // namespace AccentProfiles
