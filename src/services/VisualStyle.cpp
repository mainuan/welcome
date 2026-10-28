#include "VisualStyle.h"

namespace VisualStyles {

const QList<VisualStyleProfile> &profiles()
{
    // Package ids and themes as shipped by Mainuan 2026.
    static const QList<VisualStyleProfile> list = {
        {QStringLiteral("blur"), QStringLiteral("Dream"),
         QStringLiteral("Dream-Light-Color-Global-6"), QStringLiteral("Dream-Dark-Color-Global-6"),
         QStringLiteral("Dream-Color-Plasma"),
         QStringLiteral("kora-cyan"), QStringLiteral("kora-cyan"), QStringLiteral("breeze_cursors"),
         {QStringLiteral("org.kde.kwin.aurorae.v2"), QStringLiteral("__aurorae__svg__Lavanda-Light")},
         {QStringLiteral("org.kde.kwin.aurorae.v2"), QStringLiteral("__aurorae__svg__Lavanda-Dark")}},
        {QStringLiteral("glass"), QStringLiteral("Tahoe"),
         QStringLiteral("com.github.vinceliuice.MacTahoe-Light"), QStringLiteral("com.github.vinceliuice.MacTahoe-Dark"),
         QStringLiteral("MacTahoe"),
         QStringLiteral("breeze"), QStringLiteral("breeze-dark"), QStringLiteral("breeze_cursors"),
         {QStringLiteral("org.kde.kwin.aurorae"), QStringLiteral("__aurorae__svg__MacTahoe-Light")},
         {QStringLiteral("org.kde.kwin.aurorae"), QStringLiteral("__aurorae__svg__MacTahoe-Dark")}},
        {QStringLiteral("solid"), QStringLiteral("Breeze"),
         QStringLiteral("org.kde.breeze.desktop"), QStringLiteral("org.kde.breezedark.desktop"),
         QStringLiteral("default"),
         QStringLiteral("breeze"), QStringLiteral("breeze-dark"), QStringLiteral("breeze_cursors"),
         {QStringLiteral("org.kde.breeze"), QStringLiteral("Breeze")},
         {QStringLiteral("org.kde.breeze"), QStringLiteral("Breeze")}},
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

bool isKnownDecoration(const WindowDecoration &decoration)
{
    if (decoration.library.isEmpty() && decoration.theme.isEmpty()) return true;
    if (decoration.library == QStringLiteral("org.kde.breeze")) {
        return decoration.theme.isEmpty() || decoration.theme == QStringLiteral("Breeze");
    }
    if (decoration.library != QStringLiteral("org.kde.kwin.aurorae")
        && decoration.library != QStringLiteral("org.kde.kwin.aurorae.v2")) return false;
    QString name = decoration.theme;
    const QString prefix = QStringLiteral("__aurorae__svg__");
    if (name.startsWith(prefix)) name.remove(0, prefix.size());
    // Legacy Dream defaults shipped with Mainuan refer to uninstalled Cyan
    // decorations. Lavanda is installed as Mainuan-Light/Mainuan-Dark instead.
    return QStringList{QStringLiteral("Cyan-Light"), QStringLiteral("Cyan-Dark"),
                       QStringLiteral("Lavanda-Light"), QStringLiteral("Lavanda-Dark"),
                       QStringLiteral("MacTahoe-Light"), QStringLiteral("MacTahoe-Dark")}.contains(name);
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
