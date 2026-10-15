#include "VisualStyle.h"

namespace VisualStyles {

const QList<VisualStyleProfile> &profiles()
{
    // Package ids and themes as shipped by Mainuan 2026.
    static const QList<VisualStyleProfile> list = {
        {QStringLiteral("blur"), QStringLiteral("Dream"),
         QStringLiteral("Dream-Light-Color-Global-6"), QStringLiteral("Dream-Dark-Color-Global-6"),
         QStringLiteral("Dream-Color-Plasma"),
         QStringLiteral("kora-cyan"), QStringLiteral("kora-cyan"), QStringLiteral("breeze_cursors")},
        {QStringLiteral("glass"), QStringLiteral("Tahoe"),
         QStringLiteral("com.github.vinceliuice.MacTahoe-Light"), QStringLiteral("com.github.vinceliuice.MacTahoe-Dark"),
         QStringLiteral("MacTahoe"),
         QStringLiteral("breeze"), QStringLiteral("breeze-dark"), QStringLiteral("breeze_cursors")},
        {QStringLiteral("solid"), QStringLiteral("Breeze"),
         QStringLiteral("org.kde.breeze.desktop"), QStringLiteral("org.kde.breezedark.desktop"),
         QStringLiteral("default"),
         QStringLiteral("breeze"), QStringLiteral("breeze-dark"), QStringLiteral("breeze_cursors")},
    };
    return list;
}

const VisualStyleProfile *find(const QString &id)
{
    for (const VisualStyleProfile &profile : profiles()) {
        if (profile.id == id) {
            return &profile;
        }
    }
    return nullptr;
}

QString detect(const QString &lookAndFeelPackage, const QString &plasmaTheme)
{
    const QString package = lookAndFeelPackage.trimmed();
    if (!package.isEmpty()) {
        for (const VisualStyleProfile &profile : profiles()) {
            if (package == profile.lightPackage || package == profile.darkPackage) {
                return profile.id;
            }
        }
        return {};
    }

    // No package recorded: Plasma's default (Breeze), unless the image set a
    // Plasma theme directly, as Mainuan does for Dream on a fresh install.
    const QString theme = plasmaTheme.trimmed();
    if (theme.isEmpty() || theme == QStringLiteral("default") || theme.startsWith(QStringLiteral("breeze"))) {
        return QStringLiteral("solid");
    }
    for (const VisualStyleProfile &profile : profiles()) {
        if (profile.plasmaTheme != QStringLiteral("default") && theme.startsWith(profile.plasmaTheme)) {
            return profile.id;
        }
    }
    return {};
}

} // namespace VisualStyles
