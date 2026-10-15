#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QVariantMap>

#include <functional>

class PackageService;

// Installs the allowlisted profiles of the Welcome (system components through
// the polkit helper and Flatpak applications) and reports real progress.
// QML only passes profile identifiers; programs, arguments and package lists
// are defined here and in the helper.
class InstallService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString profileId READ profileId NOTIFY stateChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(int percent READ percent NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(int step READ step NOTIFY stateChanged)
    Q_PROPERTY(int stepCount READ stepCount NOTIFY stateChanged)
    Q_PROPERTY(QString resultMessage READ resultMessage NOTIFY stateChanged)
    Q_PROPERTY(QString details READ details NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap profiles READ profiles NOTIFY profilesChanged)

public:
    enum class Phase {
        Idle,
        Preparing,
        Authenticating,
        Waiting,
        Updating,
        Downloading,
        Installing,
        Configuring,
        Success,
        Error,
        Cancelled
    };

    struct Backend
    {
        QString pkexec;
        QString helper;
        QString dpkgQuery;
        QString aptGet;
        static Backend system();
    };

    explicit InstallService(PackageService *packages, Backend backend = Backend::system(),
                            QObject *parent = nullptr);
    ~InstallService() override;

    bool busy() const;
    QString profileId() const;
    QString phase() const;
    int percent() const;
    QString message() const;
    int step() const;
    int stepCount() const;
    QString resultMessage() const;
    QString details() const;
    QVariantMap profiles() const;
    Phase currentPhase() const;

    bool aptSupported() const;
    static bool isSystemProfile(const QString &profileId);

    // Adds an "app:<id>" profile for an allowlisted Flatpak application.
    bool registerApplication(const QString &flatpakId, const QString &name, const QString &description,
                             const QString &iconSource);

    Q_INVOKABLE bool start(const QString &profileId);
    Q_INVOKABLE void reset();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap profileInfo(const QString &profileId) const;
    Q_INVOKABLE bool launch(const QString &profileId);

signals:
    void busyChanged();
    void stateChanged();
    void profilesChanged();
    void finished(const QString &profileId, bool success);
    void launchFailed(const QString &message);

private:
    struct Component
    {
        QString label;
        QStringList aptPackages;
        QString flatpakId;
        QStringList probes; // executables, absolute paths or "kcm:<name>" when dpkg is absent
    };

    enum class Launcher { None, ClamUi, FirewallKcm, FlatpakApp };

    struct Profile
    {
        QString id;
        QString name;
        QString description;
        QString iconName;
        QString iconSource;
        QString runningTitle;
        QString successTitle;
        QString successMessage;
        QString actionLabel;
        Launcher launcher = Launcher::None;
        QString launchTarget;
        bool usesHelper = false;
        QList<Component> components;
        QList<QPair<QString, QString>> packageLabels; // package prefix → label
    };

    enum class StepKind { Helper, FlatpakRemote, Flatpak };

    struct Step
    {
        StepKind kind = StepKind::Helper;
        QString flatpakId;
        QString label;
    };

    const Profile *findProfile(const QString &profileId) const;
    bool componentInstalled(const Component &component) const;
    bool flatpakComponentNeedsRuntime(const Profile &profile) const;
    bool probeExists(const QString &probe) const;
    QVariantMap profileState(const Profile &profile) const;

    void detect(std::function<void()> done);
    void planJob();
    void runNextStep();
    void runHelperStep();
    void runFlatpakRemoteStep(const Step &step);
    void runFlatpakStep(const Step &step);
    void handleHelperOutput();
    void handleHelperLine(const QString &line);
    void handleFlatpakOutput();
    void handleFlatpakLine(const QString &line);
    void finishStep(int exitCode, QProcess::ExitStatus status);
    QString labelForPackage(const QString &package) const;

    void setPhase(Phase phase, const QString &message, int percent = -1);
    void setProgress(int percent, const QString &message);
    void appendDetails(const QString &text);
    void finishSuccess();
    void finishError(const QString &kind, const QString &technical = {});
    void finishCancelled();
    void endJob();
    QString flatpakExecutable() const;

    PackageService *m_packages = nullptr;
    Backend m_backend;
    QList<Profile> m_profiles;

    // Detected system state.
    bool m_detected = false;
    bool m_dpkgAvailable = false;
    QHash<QString, bool> m_debInstalled;
    QPointer<QProcess> m_detectProcess;
    QList<std::function<void()>> m_detectCallbacks;

    // Current job.
    QString m_profileId;
    Phase m_phase = Phase::Idle;
    int m_percent = -1;
    QString m_message;
    QString m_resultMessage;
    QString m_details;
    QString m_warning;
    QString m_errorKind;
    QList<Step> m_steps;
    int m_stepIndex = -1;
    QProcess *m_process = nullptr;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
    bool m_receivedOutput = false;
    bool m_helperDone = false;
    QString m_flatpakScope;
    int m_flatpakOperation = 0;
    int m_flatpakOperations = 0;
    int m_flatpakPercent = -1;
};
