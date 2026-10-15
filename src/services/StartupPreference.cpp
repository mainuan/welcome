#include "StartupPreference.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcStartup, "mainuan.welcome.startup")

StartupPreference::StartupPreference(QObject *parent, const QString &configHome)
    : QObject(parent)
    , m_configHome(configHome.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                                        : configHome)
{
    m_showAtStartup = !readOverride();
}

QString StartupPreference::desktopFileName()
{
    return QStringLiteral("org.mainuan.Welcome.desktop");
}

QString StartupPreference::overridePath() const
{
    return QDir(m_configHome).filePath(QStringLiteral("autostart/") + desktopFileName());
}

bool StartupPreference::showAtStartup() const
{
    return m_showAtStartup;
}

void StartupPreference::reload()
{
    const bool show = !readOverride();
    if (show != m_showAtStartup) {
        m_showAtStartup = show;
        emit showAtStartupChanged();
    }
}

// True when the user's autostart directory hides the system-wide entry.
bool StartupPreference::readOverride() const
{
    QFile file(overridePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    bool inMainGroup = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inMainGroup = line == QStringLiteral("[Desktop Entry]");
        } else if (inMainGroup && line.section(QLatin1Char('='), 0, 0).trimmed() == QStringLiteral("Hidden")) {
            return line.section(QLatin1Char('='), 1).trimmed().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
        }
    }
    return false;
}

void StartupPreference::setShowAtStartup(bool show)
{
    if (show == m_showAtStartup) {
        return;
    }

    const QString path = overridePath();
    if (show) {
        if (QFileInfo::exists(path) && !QFile::remove(path)) {
            qCWarning(lcStartup) << "could not remove" << path;
            emit failed(QStringLiteral("Não foi possível alterar a preferência de inicialização."));
            emit showAtStartupChanged(); // restore the checkbox to the real state
            return;
        }
    } else {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile file(path);
        const QByteArray contents =
            "# Written by Mainuan Welcome: do not show the Welcome when this user logs in.\n"
            "# Delete this file, or tick the option in the Welcome, to show it again.\n"
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=Mainuan Welcome\n"
            "Exec=mainuan-welcome\n"
            "Hidden=true\n";
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() || !file.commit()) {
            qCWarning(lcStartup) << "could not write" << path << file.errorString();
            emit failed(QStringLiteral("Não foi possível alterar a preferência de inicialização."));
            emit showAtStartupChanged();
            return;
        }
    }

    m_showAtStartup = show;
    qCInfo(lcStartup) << "show at startup:" << show;
    emit showAtStartupChanged();
}
