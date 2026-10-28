#pragma once

#include <KConfigWatcher>

#include <QList>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <functional>
#include <memory>

struct VisualStyleProfile;
struct AccentProfile;

class SystemService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool darkTheme READ darkTheme NOTIFY darkThemeChanged)
    Q_PROPERTY(QString accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(QString accentId READ accentId NOTIFY accentColorChanged)
    Q_PROPERTY(QString colorScheme READ colorScheme NOTIFY accentColorChanged)
    Q_PROPERTY(QString iconTheme READ iconTheme NOTIFY iconThemeChanged)
    Q_PROPERTY(QVariantList accentProfiles READ accentProfiles CONSTANT)
    Q_PROPERTY(QString visualStyle READ visualStyle NOTIFY visualStyleChanged)
    Q_PROPERTY(bool visualStyleAvailable READ visualStyleAvailable NOTIFY visualStyleChanged)
    Q_PROPERTY(QStringList installedVisualStyles READ installedVisualStyles NOTIFY visualStyleChanged)
    Q_PROPERTY(QString sessionType READ sessionType CONSTANT)
    Q_PROPERTY(QString logoSource READ logoSource CONSTANT)
    Q_PROPERTY(QString plasmaVersion READ plasmaVersion NOTIFY plasmaVersionChanged)
    Q_PROPERTY(QString pixKey READ pixKey NOTIFY pixKeyChanged)
    // QR image (data URL) of PixPayload when configured, otherwise of the key.
    Q_PROPERTY(QString pixQrSource READ pixQrSource NOTIFY pixKeyChanged)
    Q_PROPERTY(bool pixQrIsPayload READ pixQrIsPayload NOTIFY pixKeyChanged)
    Q_PROPERTY(QString firewallStatus READ firewallStatus NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString driversStatus READ driversStatus NOTIFY diagnosticsChanged)
    Q_PROPERTY(bool driversAvailable READ driversAvailable NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit SystemService(QObject *parent = nullptr, const QString &userHome = {});

    bool darkTheme() const;
    QString accentColor() const;
    QString accentId() const;
    QString colorScheme() const;
    QString iconTheme() const;
    QVariantList accentProfiles() const;
    QString visualStyle() const;
    bool visualStyleAvailable() const;
    QStringList installedVisualStyles() const;
    QString sessionType() const;
    // file:// URL of the official Mainuan logo, empty when it is not installed.
    QString logoSource() const;
    QString plasmaVersion() const;
    QString pixKey() const;
    QString pixQrSource() const;
    bool pixQrIsPayload() const;
    QString firewallStatus() const;
    QString driversStatus() const;
    bool driversAvailable() const;
    QString lastMessage() const;
    bool busy() const;

    static bool isValidAccent(const QString &value);
    static bool isAllowedVisualStyle(const QString &value);

    // Directories searched for the Plasma tools instead of PATH (tests only).
    void setToolSearchPaths(const QStringList &paths);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setTheme(const QString &theme);
    Q_INVOKABLE void setAccent(const QString &accent);
    Q_INVOKABLE void setVisualStyle(const QString &style);
    Q_INVOKABLE void performAction(const QString &action);
    Q_INVOKABLE bool openUrl(const QString &url);
    Q_INVOKABLE bool openDiscover(const QString &applicationId);
    Q_INVOKABLE bool copyText(const QString &text);

signals:
    void darkThemeChanged();
    void accentColorChanged();
    void iconThemeChanged();
    void visualStyleChanged();
    void plasmaVersionChanged();
    void pixKeyChanged();
    void diagnosticsChanged();
    void lastMessageChanged();
    void busyChanged();

private:
    struct Command
    {
        QString program;
        QStringList arguments;
        QString label; // what failed, for partial-failure messages
    };

    void readDesktopState();
    void readVisualStyle();
    void readPixKey();
    void refreshDiagnostics();
    void readPlasmaVersion();
    void setMessage(const QString &message, bool error = false);
    void setBusy(bool busy);
    QString findTool(const QString &name) const;
    void runTool(const QString &program, const QStringList &arguments,
                 std::function<void(bool ok, const QString &output)> done);
    void applyStyleProfile(const VisualStyleProfile &profile, bool dark, const QString &successMessage,
                           bool themeOnly = false);
    // `replacedWallpaper` is the image the package replaced, empty if it kept it.
    void finishStyleProfile(const VisualStyleProfile &profile, bool dark, const QString &successMessage,
                            const QString &replacedWallpaper, const QString &accentId, bool themeOnly);
    // Image of the first desktop's wallpaper (file URL), empty when unknown.
    void readWallpaper(std::function<void(const QString &image)> done);
    void validateScheme(const AccentProfile &profile, bool dark, std::function<void(const QString &)> done);
    void applyAccentScheme(const AccentProfile &profile, bool dark, std::function<void(const QString &)> done);
    void applyAccentExtras(const AccentProfile &profile, const QString &successMessage, QList<Command> steps = {});
    void finishAppearance(const QString &successMessage, const QStringList &failures = {});
    void completeDecoration(const QString &successMessage, QStringList failures);
    void checkKWin(std::function<void(const QString &error)> done);
    void runStyleFollowUps(QList<Command> steps, QStringList failures,
                           std::function<void(const QStringList &failures)> done);

    KConfigWatcher::Ptr m_globalsWatcher;
    KConfigWatcher::Ptr m_plasmaWatcher;
    KConfigWatcher::Ptr m_defaultGlobalsWatcher;
    KConfigWatcher::Ptr m_defaultPlasmaWatcher;
    struct DecorationChange;
    std::shared_ptr<DecorationChange> m_decorationChange;
    QStringList m_toolPaths;
    QStringList m_installedVisualStyles;
    QString m_accentColor;
    QString m_accentId;
    QString m_colorScheme;
    QString m_iconTheme;
    QString m_userHome;
    QString m_visualStyle;
    QString m_plasmaVersion;
    QString m_pixKey;
    QString m_pixPayload;
    QString m_pixQrSource;
    QString m_firewallStatus;
    QString m_driversStatus;
    QString m_lastMessage;
    QString m_sessionType;
    QString m_logoSource;
    bool m_darkTheme = false;
    bool m_visualStyleAvailable = false;
    bool m_driversAvailable = false;
    bool m_busy = false;
};
