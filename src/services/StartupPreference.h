#pragma once

#include <QObject>
#include <QString>

// Per-user "show at login" preference built on XDG Autostart: the package
// installs a system-wide entry in /etc/xdg/autostart and a user opts out with
// an entry of the same name in $XDG_CONFIG_HOME/autostart containing
// Hidden=true, which is also what Plasma's Autostart settings write.
class StartupPreference final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool showAtStartup READ showAtStartup WRITE setShowAtStartup NOTIFY showAtStartupChanged)

public:
    // `configHome` overrides $XDG_CONFIG_HOME (used by tests).
    explicit StartupPreference(QObject *parent = nullptr, const QString &configHome = {});

    static QString desktopFileName();

    bool showAtStartup() const;
    void setShowAtStartup(bool show);
    QString overridePath() const;

    Q_INVOKABLE void reload();

signals:
    void showAtStartupChanged();
    void failed(const QString &message);

private:
    bool readOverride() const;

    QString m_configHome;
    bool m_showAtStartup = true;
};
