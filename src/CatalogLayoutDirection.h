#pragma once

#include <KLocalizedString>

#include <QGuiApplication>
#include <QLocale>

// Qt mirrors the interface only when a loaded qtbase_<lang> translator says
// "RTL"; Urdu has no qtbase catalog and qtbase may not be installed at all.
// The interface language is the first preferred language that has a catalog
// for the application's own domain: when that language is written right to
// left, mirror the application explicitly. Without such a catalog the
// interface falls back to English and keeps Qt's own direction.
namespace CatalogLayoutDirection {

inline QString effectiveLanguage()
{
    const QStringList languages = KLocalizedString::languages();
    for (const QString &language : languages) {
        if (language == QLatin1String("en") || language == QLatin1String("en_US") || language == QLatin1String("C")) {
            break;
        }
        if (KLocalizedString::isApplicationTranslatedInto(language)) {
            return language;
        }
    }
    return QString();
}

inline void apply(QGuiApplication &app)
{
    const QString language = effectiveLanguage();
    if (!language.isEmpty() && QLocale(language).textDirection() == Qt::RightToLeft) {
        app.setLayoutDirection(Qt::RightToLeft);
    }
}

} // namespace CatalogLayoutDirection
