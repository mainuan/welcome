#pragma once

#include <QList>
#include <QString>

// The three visual styles offered by Aparência > Estilo visual. Each one is a
// complete Mainuan look: a Plasma global theme (Look-and-Feel package) in a
// light and a dark variant plus the icon/cursor themes to use when the package
// refers to ones that are missing. The wallpaper follows the accent color
// (AccentProfile), not the style.
struct VisualStyleProfile
{
    QString id;           // identifier used by QML: "blur", "glass" or "solid"
    QString name;         // "Dream", "Tahoe" or "Breeze"
    QString lightPackage; // Look-and-Feel package ids
    QString darkPackage;
    QString plasmaTheme;  // Plasma theme the package sets, used to recognise
                          // installs where LookAndFeelPackage was never written
    QString fallbackLightIcons;
    QString fallbackDarkIcons;
    QString fallbackCursor;

    QString package(bool dark) const { return dark ? darkPackage : lightPackage; }
};

namespace VisualStyles {

const QList<VisualStyleProfile> &profiles();
const VisualStyleProfile *find(const QString &id);

// Style id ("blur", "glass", "solid") for the active configuration, or an
// empty string when another global theme is in use. `lookAndFeelPackage` is
// [KDE] LookAndFeelPackage from kdeglobals and `plasmaTheme` is [Theme] name
// from plasmarc, both read through the KDE configuration cascade.
QString detect(const QString &lookAndFeelPackage, const QString &plasmaTheme);

} // namespace VisualStyles
