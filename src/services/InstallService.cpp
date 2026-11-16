#include "InstallService.h"

#include "InstallProgressParser.h"
#include "PackageService.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include <KLocalizedString>

#include <algorithm>
#include <utility>

#ifndef MAINUAN_HELPER_PATH
#define MAINUAN_HELPER_PATH "/usr/libexec/mainuan-welcome/mainuan-welcome-helper"
#endif

namespace {

const QString kClamUiId = QStringLiteral("io.github.linx_systems.ClamUI");
const QString kAppPrefix = QStringLiteral("app:");
const QString kFlathubUrl = QStringLiteral("https://dl.flathub.org/repo/flathub.flatpakrepo");
constexpr int kMaxDetailLines = 400;

bool isExecutableFile(const QString &path)
{
    const QFileInfo info(path);
    return !path.isEmpty() && info.isFile() && info.isExecutable();
}

QString lastLines(const QString &text, int count)
{
    QStringList lines = text.split(QLatin1Char('\n'));
    while (!lines.isEmpty() && lines.constLast().trimmed().isEmpty()) {
        lines.removeLast();
    }
    if (lines.size() > count) {
        lines = lines.mid(lines.size() - count);
    }
    return lines.join(QLatin1Char('\n'));
}

QString friendlyError(const QString &kind)
{
    if (kind == QStringLiteral("locked")) {
        return i18n("Another program is installing or updating the system right now. Wait for it to finish and try again.");
    }
    if (kind == QStringLiteral("network")) {
        return i18n("Could not download the files. Check your internet connection and try again.");
    }
    if (kind == QStringLiteral("space")) {
        return i18n("There is not enough free disk space to complete the installation.");
    }
    if (kind == QStringLiteral("unavailable")) {
        return i18n("The required components were not found in the repositories configured on this system.");
    }
    if (kind == QStringLiteral("interrupted")) {
        return i18n("A previous installation was interrupted and must be repaired before new programs can be installed.");
    }
    if (kind == QStringLiteral("dependencies")) {
        return i18n("The package system found a conflict that prevents the installation without removing other programs.");
    }
    if (kind == QStringLiteral("auth")) {
        return i18n("Authentication failed or your user does not have administrator privileges.");
    }
    if (kind == QStringLiteral("unsupported")) {
        return i18n("Automatic installation of these components requires a Debian- or Ubuntu-based system.");
    }
    if (kind == QStringLiteral("flatpak-missing")) {
        return i18n("Flatpak is not available on this system.");
    }
    if (kind == QStringLiteral("freeoffice-download")) {
        return i18n("Could not download FreeOffice from the authorized official source. Check the connection and try again. Redirects and other versions are refused.");
    }
    if (kind == QStringLiteral("freeoffice-invalid")) {
        return i18n("The FreeOffice file does not match the authorized package (integrity, size, name, version or architecture). The installation was stopped.");
    }
    if (kind == QStringLiteral("verification")) {
        return i18n("The installer finished, but it could not be confirmed that the application is installed. Check the details and try again.");
    }
    // i18n: “Show Details” must match the label of the details button in the installation dialog.
    return i18n("The system could not install all components. Try again; if the problem persists, use “Show Details” to get the technical information.");
}

QString classifyFlatpakError(const QString &output)
{
    static const QRegularExpression auth(QStringLiteral("not allowed for user|Not authorized|AccessDenied|authentication"),
                                         QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression network(
        QStringLiteral("Could not resolve|Couldn't resolve|Unable to connect|Could not connect|Failed to connect|"
                       "Timeout was reached|Network is unreachable|Temporary failure"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression space(QStringLiteral("No space left|not enough space"),
                                          QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression missing(QStringLiteral("Nothing matches|No remote refs found|not found"),
                                            QRegularExpression::CaseInsensitiveOption);
    if (auth.match(output).hasMatch()) {
        return QStringLiteral("cancelled");
    }
    if (network.match(output).hasMatch()) {
        return QStringLiteral("network");
    }
    if (space.match(output).hasMatch()) {
        return QStringLiteral("space");
    }
    if (missing.match(output).hasMatch()) {
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("failed");
}

QString phaseName(InstallService::Phase phase)
{
    switch (phase) {
    case InstallService::Phase::Idle: return QStringLiteral("idle");
    case InstallService::Phase::Preparing: return QStringLiteral("preparing");
    case InstallService::Phase::Authenticating: return QStringLiteral("authenticating");
    case InstallService::Phase::Waiting: return QStringLiteral("waiting");
    case InstallService::Phase::Updating: return QStringLiteral("updating");
    case InstallService::Phase::Downloading: return QStringLiteral("downloading");
    case InstallService::Phase::Installing: return QStringLiteral("installing");
    case InstallService::Phase::Configuring: return QStringLiteral("configuring");
    case InstallService::Phase::Success: return QStringLiteral("success");
    case InstallService::Phase::Error: return QStringLiteral("error");
    case InstallService::Phase::Cancelled: return QStringLiteral("cancelled");
    }
    return QStringLiteral("idle");
}

// "A e B" (and "A e B e C" for longer lists), as the steps were always worded.
QString joinWithAnd(const QStringList &labels)
{
    QString result = labels.value(0);
    for (qsizetype i = 1; i < labels.size(); ++i) {
        // i18n: joins the names of two components installed in the same step, e.g. "ClamAV and Flatpak".
        result = i18nc("@item:intext two components", "%1 and %2", result, labels.at(i));
    }
    return result;
}

bool isRunningPhase(InstallService::Phase phase)
{
    return phase != InstallService::Phase::Idle && phase != InstallService::Phase::Success
        && phase != InstallService::Phase::Error && phase != InstallService::Phase::Cancelled;
}

} // namespace

InstallService::Backend InstallService::Backend::system()
{
    const QStringList systemDirs = {QStringLiteral("/usr/bin"), QStringLiteral("/bin"), QStringLiteral("/usr/sbin")};
    return {QStandardPaths::findExecutable(QStringLiteral("pkexec"), systemDirs),
            QStringLiteral(MAINUAN_HELPER_PATH),
            QStringLiteral("/usr/bin/dpkg-query"),
            QStringLiteral("/usr/bin/apt-get")};
}

InstallService::InstallService(PackageService *packages, Backend backend, QObject *parent)
    : QObject(parent)
    , m_packages(packages)
    , m_backend(std::move(backend))
{
    Q_ASSERT(m_packages != nullptr);

    Profile antivirus;
    antivirus.id = QStringLiteral("antivirus");
    antivirus.name = i18n("Antivirus");
    antivirus.source = i18n("Configured repositories / Flathub");
    antivirus.description = i18n("ClamAV protection with the ClamUI graphical interface.");
    antivirus.iconName = QStringLiteral("security-high");
    antivirus.runningTitle = i18n("Installing ClamAV + ClamUI");
    antivirus.successTitle = i18n("Antivirus installed successfully");
    antivirus.successMessage = i18n("ClamAV and ClamUI are ready to use.");
    antivirus.actionLabel = i18nc("@action:button open the settings of the installed component", "Configure");
    antivirus.launcher = Launcher::ClamUi;
    antivirus.launchTarget = kClamUiId;
    antivirus.usesHelper = true;
    antivirus.components = {
        {QStringLiteral("ClamAV"),
         {QStringLiteral("clamav"), QStringLiteral("clamav-daemon"), QStringLiteral("clamav-freshclam")},
         QString(),
         {QStringLiteral("clamscan"), QStringLiteral("freshclam"), QStringLiteral("clamd")}},
        {QStringLiteral("ClamUI"), {}, kClamUiId, {QStringLiteral("clamui")}}
    };
    antivirus.packageLabels = {{QStringLiteral("clamav"), QStringLiteral("ClamAV")},
                               {QStringLiteral("libclamav"), QStringLiteral("ClamAV")},
                               {QStringLiteral("clamdscan"), QStringLiteral("ClamAV")},
                               {QStringLiteral("flatpak"), QStringLiteral("Flatpak")}};

    Profile firewall;
    firewall.id = QStringLiteral("firewall");
    firewall.name = i18n("Firewall");
    firewall.source = i18n("Configured repositories");
    firewall.description = i18n("UFW with the KDE Plasma firewall module.");
    firewall.iconName = QStringLiteral("security-medium");
    firewall.runningTitle = i18n("Installing Firewall");
    firewall.successTitle = i18n("Firewall installed successfully");
    firewall.successMessage = i18n("UFW and the KDE Plasma integration are installed. The firewall was not enabled "
                                   "automatically: use Configure to enable it and choose the rules.");
    firewall.actionLabel = i18nc("@action:button open the settings of the installed component", "Configure");
    firewall.launcher = Launcher::FirewallKcm;
    firewall.usesHelper = true;
    firewall.components = {
        {QStringLiteral("UFW"), {QStringLiteral("ufw")}, QString(), {QStringLiteral("ufw")}},
        {i18nc("@item:intext component label", "KDE Plasma integration"), {QStringLiteral("plasma-firewall")}, QString(),
         {QStringLiteral("kcm:kcm_firewall")}}
    };
    firewall.packageLabels = {{QStringLiteral("ufw"), QStringLiteral("UFW")},
                              {QStringLiteral("plasma-firewall"), i18nc("@item:intext component label", "KDE Plasma integration")}};

    Profile codecs;
    codecs.id = QStringLiteral("codecs");
    codecs.name = i18n("Media Codecs");
    codecs.source = i18n("Configured repositories");
    codecs.description = i18n("Support for the most common audio and video formats.");
    codecs.iconName = QStringLiteral("applications-multimedia");
    codecs.runningTitle = i18n("Installing media codecs");
    codecs.successTitle = i18n("Codecs installed successfully");
    codecs.successMessage = i18n("Audio and video in the most common formats can now be played.");
    codecs.usesHelper = true;
    codecs.components = {
        {i18nc("@item:intext component label", "media codecs"),
         {QStringLiteral("gstreamer1.0-libav"), QStringLiteral("gstreamer1.0-plugins-ugly"),
          QStringLiteral("gstreamer1.0-plugins-bad"), QStringLiteral("ffmpeg")},
         QString(),
         {QStringLiteral("ffmpeg"), QStringLiteral("gst-launch-1.0")}}
    };
    codecs.packageLabels = {{QStringLiteral("gstreamer"), i18nc("@item:intext component label", "GStreamer codecs")},
                            {QStringLiteral("ffmpeg"), QStringLiteral("FFmpeg")},
                            {QStringLiteral("libav"), QStringLiteral("FFmpeg")}};

    Profile freeoffice;
    freeoffice.id = QStringLiteral("freeoffice");
    freeoffice.name = QStringLiteral("FreeOffice");
    freeoffice.source = QStringLiteral("SoftMaker");
    freeoffice.description = i18n("Free office suite from SoftMaker.");
    freeoffice.iconSource = QStringLiteral("qrc:/assets/freeOffice.png");
    freeoffice.runningTitle = i18n("Installing FreeOffice");
    freeoffice.successTitle = i18n("FreeOffice installed successfully");
    freeoffice.successMessage = i18n("FreeOffice is installed and available in the application menu.");
    freeoffice.sourceNotice = i18n("This package is provided by SoftMaker and may ask for administrator authorization.");
    freeoffice.usesHelper = true;
    freeoffice.components = {{QStringLiteral("FreeOffice"), {QStringLiteral("softmaker-freeoffice-2024")}, {}, {}}};
    freeoffice.packageLabels = {{QStringLiteral("softmaker-freeoffice-2024"), QStringLiteral("FreeOffice")}};
    m_profiles = {antivirus, firewall, codecs, freeoffice};
    m_network = m_backend.network != nullptr ? m_backend.network : new QNetworkAccessManager(this);

    connect(m_packages, &PackageService::installedApplicationsChanged, this, &InstallService::profilesChanged);
    connect(m_packages, &PackageService::flatpakAvailableChanged, this, &InstallService::profilesChanged);
    refresh();
}

InstallService::~InstallService()
{
    if (m_process != nullptr) {
        // A privileged helper cannot be signalled by this user and keeps running;
        // it no longer depends on our pipe, so just stop listening to it.
        m_process->disconnect(this);
    }
}

bool InstallService::busy() const { return isRunningPhase(m_phase); }
QString InstallService::profileId() const { return m_profileId; }
QString InstallService::phase() const { return phaseName(m_phase); }
int InstallService::percent() const { return m_percent; }
QString InstallService::message() const { return m_message; }
QString InstallService::resultMessage() const { return m_resultMessage; }
QString InstallService::details() const { return m_details; }
InstallService::Phase InstallService::currentPhase() const { return m_phase; }

int InstallService::step() const
{
    return m_stepIndex < 0 ? 0 : std::min(m_stepIndex + 1, static_cast<int>(m_steps.size()));
}

int InstallService::stepCount() const
{
    return static_cast<int>(m_steps.size());
}

bool InstallService::aptSupported() const
{
    return isExecutableFile(m_backend.dpkgQuery) && isExecutableFile(m_backend.aptGet)
        && isExecutableFile(m_backend.pkexec) && QFileInfo(m_backend.helper).isFile();
}

bool InstallService::isSystemProfile(const QString &profileId)
{
    return profileId == QStringLiteral("antivirus") || profileId == QStringLiteral("firewall")
        || profileId == QStringLiteral("codecs");
}

bool InstallService::registerApplication(const QString &flatpakId, const QString &name, const QString &description,
                                         const QString &iconSource)
{
    if (!PackageService::isAllowedApplication(flatpakId)
        || findProfile(kAppPrefix + flatpakId) != nullptr) {
        return false;
    }
    Profile profile;
    profile.id = kAppPrefix + flatpakId;
    profile.name = name;
    profile.source = QStringLiteral("Flathub");
    profile.description = description;
    profile.iconSource = iconSource;
    profile.runningTitle = i18nc("@title %1 is an application name", "Installing %1", name);
    profile.successTitle = i18nc("@title %1 is an application name", "%1 installed successfully", name);
    profile.successMessage = i18n("The application is now available in the application menu.");
    profile.actionLabel = i18nc("@action:button open the installed application", "Open");
    profile.launcher = Launcher::FlatpakApp;
    profile.launchTarget = flatpakId;
    profile.components = {{name, {}, flatpakId, {}}};
    m_profiles.append(profile);
    emit profilesChanged();
    return true;
}

const InstallService::Profile *InstallService::findProfile(const QString &profileId) const
{
    for (const Profile &profile : m_profiles) {
        if (profile.id == profileId) {
            return &profile;
        }
    }
    return nullptr;
}

bool InstallService::probeExists(const QString &probe) const
{
    if (probe.startsWith(QStringLiteral("kcm:"))) {
        const QString fileName = probe.mid(4) + QStringLiteral(".so");
        QStringList roots = QCoreApplication::libraryPaths();
        roots.prepend(QLibraryInfo::path(QLibraryInfo::PluginsPath));
        for (const QString &root : std::as_const(roots)) {
            if (QFileInfo::exists(root + QStringLiteral("/plasma/kcms/systemsettings/") + fileName)) {
                return true;
            }
        }
        return false;
    }
    if (probe.startsWith(QLatin1Char('/'))) {
        return QFileInfo::exists(probe);
    }
    return !QStandardPaths::findExecutable(probe).isEmpty();
}

bool InstallService::componentInstalled(const Component &component) const
{
    if (!component.aptPackages.isEmpty()) {
        if (m_dpkgAvailable) {
            for (const QString &package : component.aptPackages) {
                if (!m_debInstalled.value(package)) {
                    return false;
                }
            }
            return true;
        }
        if (component.probes.isEmpty()) {
            return false;
        }
        for (const QString &probe : component.probes) {
            if (!probeExists(probe)) {
                return false;
            }
        }
        return true;
    }

    if (m_packages->isInstalled(component.flatpakId)) {
        return true;
    }
    for (const QString &probe : component.probes) {
        if (probeExists(probe)) {
            return true;
        }
    }
    return false;
}

bool InstallService::flatpakComponentNeedsRuntime(const Profile &profile) const
{
    return profile.usesHelper && flatpakExecutable().isEmpty();
}

QString InstallService::flatpakExecutable() const
{
    return m_packages->flatpakExecutable();
}

QVariantMap InstallService::profileState(const Profile &profile) const
{
    QStringList missing;
    bool installPossible = true;
    bool needsApt = false;
    bool needsFlatpak = false;
    int installedCount = 0;
    for (const Component &component : profile.components) {
        if (componentInstalled(component)) {
            ++installedCount;
            continue;
        }
        missing << component.label;
        if (!component.aptPackages.isEmpty()) {
            needsApt = true;
        } else {
            needsFlatpak = true;
        }
    }
    if (needsFlatpak && flatpakExecutable().isEmpty()) {
        needsApt = needsApt || profile.usesHelper;
        installPossible = profile.usesHelper;
    }
    if (needsApt && !aptSupported()) {
        installPossible = false;
    }
    if (profile.id == QStringLiteral("freeoffice")) {
        installPossible = installPossible && m_backend.freeOffice.isValid()
            && isExecutableFile(m_backend.dpkgDeb) && QSysInfo::buildCpuArchitecture() == QStringLiteral("x86_64");
    }

    const bool hasAptComponents = std::any_of(profile.components.cbegin(), profile.components.cend(),
                                              [](const Component &c) { return !c.aptPackages.isEmpty(); });
    QString state;
    QString summary;
    if (hasAptComponents && !m_detected) {
        state = QStringLiteral("checking");
        summary = i18nc("@info:status installation state", "Checking…");
    } else if (missing.isEmpty()) {
        state = QStringLiteral("installed");
        summary = i18nc("@info:status installation state", "Installed");
    } else if (installedCount > 0) {
        state = QStringLiteral("partial");
        // i18n: %1 is the name of the only component that is not installed yet.
        summary = missing.size() == 1 ? i18nc("@info:status one component", "Missing: %1", missing.constFirst())
                                      // i18n: %1 is a comma-separated list of components that are not installed yet.
                                      : i18nc("@info:status several components", "Missing: %1", missing.join(QStringLiteral(", ")));
    } else {
        state = QStringLiteral("missing");
        summary = i18nc("@info:status installation state", "Not installed");
    }

    QString unavailableReason;
    if (!installPossible && !missing.isEmpty()) {
        unavailableReason = needsApt ? friendlyError(QStringLiteral("unsupported"))
                                     : friendlyError(QStringLiteral("flatpak-missing"));
    }

    bool canLaunch = false;
    switch (profile.launcher) {
    case Launcher::ClamUi:
        canLaunch = state == QStringLiteral("installed")
            && (m_packages->isInstalled(kClamUiId) || probeExists(QStringLiteral("clamui")));
        break;
    case Launcher::FirewallKcm:
        canLaunch = state == QStringLiteral("installed") && probeExists(QStringLiteral("kcm:kcm_firewall"))
            && (probeExists(QStringLiteral("kcmshell6")) || probeExists(QStringLiteral("systemsettings")));
        break;
    case Launcher::FlatpakApp:
        canLaunch = state == QStringLiteral("installed") && m_packages->isInstalled(profile.launchTarget);
        break;
    case Launcher::None:
        break;
    }

    return {{QStringLiteral("state"), state},
            {QStringLiteral("summary"), summary},
            {QStringLiteral("installed"), state == QStringLiteral("installed")},
            {QStringLiteral("canInstall"), installPossible && !missing.isEmpty() && state != QStringLiteral("checking")},
            {QStringLiteral("unavailableReason"), unavailableReason},
            {QStringLiteral("canLaunch"), canLaunch},
            {QStringLiteral("running"), busy() && m_profileId == profile.id}};
}

QVariantMap InstallService::profiles() const
{
    QVariantMap result;
    for (const Profile &profile : m_profiles) {
        result.insert(profile.id, profileState(profile));
    }
    return result;
}

QVariantMap InstallService::profileInfo(const QString &profileId) const
{
    const Profile *profile = findProfile(profileId);
    if (profile == nullptr) {
        return {};
    }
    return {{QStringLiteral("id"), profile->id},
            {QStringLiteral("name"), profile->name},
            {QStringLiteral("source"), profile->source},
            {QStringLiteral("description"), profile->description},
            {QStringLiteral("iconName"), profile->iconName},
            {QStringLiteral("iconSource"), profile->iconSource},
            {QStringLiteral("runningTitle"), profile->runningTitle},
            {QStringLiteral("successTitle"), profile->successTitle},
            {QStringLiteral("sourceNotice"), profile->sourceNotice},
            {QStringLiteral("actionLabel"), profile->actionLabel}};
}

void InstallService::refresh()
{
    detect({});
    m_packages->refresh();
}

void InstallService::detect(std::function<void()> done)
{
    if (done) {
        m_detectCallbacks.append(std::move(done));
    }
    if (m_detectProcess != nullptr) {
        return;
    }

    const auto complete = [this](bool dpkgAvailable, const QHash<QString, bool> &installed) {
        m_dpkgAvailable = dpkgAvailable;
        m_debInstalled = installed;
        m_detected = true;
        emit profilesChanged();
        const auto callbacks = std::exchange(m_detectCallbacks, {});
        for (const auto &callback : callbacks) {
            callback();
        }
    };

    if (!isExecutableFile(m_backend.dpkgQuery)) {
        QTimer::singleShot(0, this, [complete]() { complete(false, {}); });
        return;
    }

    QStringList arguments = {QStringLiteral("-W"), QStringLiteral("-f=${Package}\\t${db:Status-Abbrev}\\n")};
    for (const Profile &profile : std::as_const(m_profiles)) {
        for (const Component &component : profile.components) {
            arguments << component.aptPackages;
        }
    }
    arguments.removeDuplicates();

    auto *process = new QProcess(this);
    m_detectProcess = process;
    process->setStandardInputFile(QProcess::nullDevice());
    connect(process, &QProcess::finished, this, [this, process, complete](int, QProcess::ExitStatus status) {
        // dpkg-query exits with 1 when some package is unknown; its output is still valid.
        QHash<QString, bool> installed;
        const QStringList lines = QString::fromUtf8(process->readAllStandardOutput()).split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            const QStringList fields = line.split(QLatin1Char('\t'));
            if (fields.size() >= 2) {
                installed.insert(fields.at(0).section(QLatin1Char(':'), 0, 0).trimmed(),
                                 fields.at(1).startsWith(QStringLiteral("ii")));
            }
        }
        process->deleteLater();
        m_detectProcess = nullptr;
        complete(status == QProcess::NormalExit, installed);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            m_detectProcess = nullptr;
            complete(false, {});
        }
    });
    process->start(m_backend.dpkgQuery, arguments);
}

bool InstallService::start(const QString &profileId)
{
    if (busy() || m_packages->busy()) {
        return false;
    }
    if (findProfile(profileId) == nullptr) {
        return false;
    }

    m_profileId = profileId;
    m_steps.clear();
    m_stepIndex = -1;
    m_details.clear();
    m_warning.clear();
    m_errorKind.clear();
    m_resultMessage.clear();
    m_flatpakScope.clear();
    setPhase(Phase::Preparing, i18n("Preparing the installation…"));
    emit profilesChanged();
    detect([this]() { planJob(); });
    return true;
}

void InstallService::reset()
{
    if (busy()) {
        return;
    }
    m_profileId.clear();
    m_steps.clear();
    m_stepIndex = -1;
    m_resultMessage.clear();
    m_details.clear();
    setPhase(Phase::Idle, QString());
}

void InstallService::planJob()
{
    const Profile *profile = findProfile(m_profileId);
    if (profile == nullptr || m_phase != Phase::Preparing) {
        return;
    }

    QStringList helperLabels;
    QList<Step> flatpakSteps;
    for (const Component &component : profile->components) {
        if (componentInstalled(component)) {
            continue;
        }
        if (!component.aptPackages.isEmpty()) {
            helperLabels << component.label;
        } else {
            flatpakSteps.append({StepKind::Flatpak, component.flatpakId, component.label});
        }
    }
    if (!flatpakSteps.isEmpty() && flatpakExecutable().isEmpty()) {
        if (!flatpakComponentNeedsRuntime(*profile)) {
            finishError(QStringLiteral("flatpak-missing"));
            return;
        }
        helperLabels << QStringLiteral("Flatpak");
    }

    if (!helperLabels.isEmpty()) {
        if (!profile->usesHelper || !aptSupported()) {
            finishError(QStringLiteral("unsupported"));
            return;
        }
        if (profile->id == QStringLiteral("freeoffice")) {
            if (!m_backend.freeOffice.isValid() || !isExecutableFile(m_backend.dpkgDeb)
                || QSysInfo::buildCpuArchitecture() != QStringLiteral("x86_64")) {
                finishError(QStringLiteral("freeoffice-invalid"));
                return;
            }
            m_steps.append({StepKind::FreeOfficeDownload, {}, QStringLiteral("FreeOffice")});
        }
        m_steps.append({StepKind::Helper, QString(), joinWithAnd(helperLabels)});
    }
    m_steps.append(flatpakSteps);

    if (m_steps.isEmpty()) {
        finishSuccess();
        return;
    }
    runNextStep();
}

void InstallService::runNextStep()
{
    ++m_stepIndex;
    if (m_stepIndex >= m_steps.size()) {
        if (m_profileId == QStringLiteral("freeoffice")) {
            verifyFreeOfficeInstalled();
        } else if (m_profileId == QStringLiteral("app:com.vivaldi.Vivaldi")
                   || m_profileId == QStringLiteral("app:app.zen_browser.zen")) {
            verifyBrowserInstalled();
        } else {
            finishSuccess();
        }
        return;
    }
    const Step step = m_steps.at(m_stepIndex);
    if (step.kind == StepKind::FreeOfficeDownload) {
        downloadFreeOffice();
    } else if (step.kind == StepKind::Helper) {
        runHelperStep();
    } else {
        runFlatpakStep(step);
    }
}

void InstallService::downloadFreeOffice()
{
    const auto &profile = m_backend.freeOffice;
    if (!profile.acceptsUrl(profile.url)) {
        finishError(QStringLiteral("freeoffice-invalid"));
        return;
    }
    m_downloadFile.close();
    m_downloadDirectory = std::make_unique<QTemporaryDir>();
    if (!m_downloadDirectory->isValid()) {
        finishError(QStringLiteral("space"), m_downloadDirectory->errorString());
        return;
    }
    m_downloadFile.setFileName(m_downloadDirectory->filePath(QStringLiteral("freeoffice.deb")));
    if (!m_downloadFile.open(QIODevice::ReadWrite | QIODevice::NewOnly)) {
        finishError(QStringLiteral("space"), m_downloadFile.errorString());
        return;
    }
    m_downloadFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    m_downloadHash.reset();
    setPhase(Phase::Downloading, i18n("Downloading FreeOffice from SoftMaker…"));
    // i18n: technical details: %1 URL, %2 package name, %3 version, %4 architecture, %5 size in bytes, %6 checksum.
    appendDetails(i18n("Authorized source: %1\nPackage: %2 · version %3 · %4 · %5 bytes\nPinned SHA-256: %6",
        profile.url.toString(), profile.package, profile.version, profile.architecture,
        QString::number(profile.size), QString::fromLatin1(profile.sha256)));

    QNetworkRequest request(profile.url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Accept-Encoding", "identity");
    request.setTransferTimeout(30000);
    auto *reply = m_network->get(request);
    reply->setReadBufferSize(256 * 1024);
    auto failure = std::make_shared<QString>();
    const auto drain = [this, reply, failure]() {
        if (!failure->isEmpty()) {
            return;
        }
        const auto bytes = reply->readAll();
        if (m_downloadFile.size() + bytes.size() > m_backend.freeOffice.size) {
            *failure = i18n("The file exceeds the authorized size.");
        } else if (m_downloadFile.write(bytes) != bytes.size()) {
            *failure = m_downloadFile.errorString();
        } else {
            m_downloadHash.addData(bytes);
        }
        if (!failure->isEmpty()) {
            reply->abort();
        }
    };
    connect(reply, &QNetworkReply::readyRead, this, drain);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        const int progress = total == m_backend.freeOffice.size
            ? static_cast<int>(std::min<qint64>(100, received * 100 / total)) : -1;
        setProgress(progress, i18n("Downloading FreeOffice from SoftMaker…"));
    });
    // Bound the complete transfer as well as individual network stalls.
    QTimer::singleShot(15 * 60 * 1000, reply, [reply]() { if (!reply->isFinished()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, failure, drain]() {
        drain();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool transportOk = reply->error() == QNetworkReply::NoError && status == 200
            && m_backend.freeOffice.acceptsUrl(reply->url())
            && !reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid();
        const QString technical = QStringLiteral("HTTP %1: %2\n%3").arg(status).arg(reply->errorString(), *failure);
        reply->deleteLater();
        if (!transportOk || !failure->isEmpty()) {
            finishError(failure->isEmpty() ? QStringLiteral("freeoffice-download") : QStringLiteral("freeoffice-invalid"), technical);
            return;
        }
        if (!m_downloadFile.flush() || m_downloadFile.size() != m_backend.freeOffice.size
            || m_downloadHash.result().toHex() != m_backend.freeOffice.sha256) {
            finishError(QStringLiteral("freeoffice-invalid"), i18n("Size or SHA-256 mismatch."));
            return;
        }
        validateFreeOffice();
    });
}

void InstallService::validateFreeOffice()
{
    setPhase(Phase::Preparing, i18n("Validating the FreeOffice Debian package…"));
    auto *process = new QProcess(this);
    process->setStandardInputFile(QProcess::nullDevice());
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        const QByteArray metadata = process->readAllStandardOutput();
        const QString technical = QString::fromUtf8(process->readAllStandardError());
        process->deleteLater();
        if (code != 0 || status != QProcess::NormalExit || !m_backend.freeOffice.matchesMetadata(metadata)) {
            finishError(QStringLiteral("freeoffice-invalid"), QString::fromUtf8(metadata) + technical);
            return;
        }
        runNextStep();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            finishError(QStringLiteral("freeoffice-invalid"), process->errorString());
        }
    });
    QTimer::singleShot(30000, process, [process]() { if (process->state() != QProcess::NotRunning) process->kill(); });
    process->start(m_backend.dpkgDeb, {QStringLiteral("--show"),
        QStringLiteral("--showformat=${Package}\\n${Version}\\n${Architecture}\\n"), m_downloadFile.fileName()});
}

void InstallService::transferFreeOffice()
{
    if (m_process == nullptr || m_process->state() != QProcess::Running || !m_process->isWritable()) {
        return;
    }
    // Backpressure keeps the 160 MiB package out of QProcess's in-memory queue.
    while (m_process->bytesToWrite() < 256 * 1024 && !m_downloadFile.atEnd()) {
        const QByteArray chunk = m_downloadFile.read(64 * 1024);
        if (chunk.isEmpty() || m_process->write(chunk) != chunk.size()) {
            m_process->closeWriteChannel();
            return; // The helper rejects any incomplete transfer before invoking APT.
        }
    }
    if (m_downloadFile.atEnd()) {
        m_process->closeWriteChannel();
    }
}

void InstallService::verifyFreeOfficeInstalled()
{
    setPhase(Phase::Preparing, i18n("Confirming the FreeOffice installation…"));
    detect([this]() {
        if (m_dpkgAvailable && m_debInstalled.value(QStringLiteral("softmaker-freeoffice-2024"))) {
            finishSuccess();
        } else {
            finishError(QStringLiteral("verification"));
        }
    });
}

void InstallService::verifyBrowserInstalled()
{
    setPhase(Phase::Preparing, i18n("Confirming the browser installation…"));
    m_process = new QProcess(this);
    m_process->setStandardInputFile(QProcess::nullDevice());
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const auto output = m_process->readAllStandardOutput().trimmed();
        const auto error = QString::fromUtf8(m_process->readAllStandardError());
        m_process->deleteLater();
        m_process = nullptr;
        const auto id = m_profileId.mid(kAppPrefix.size()).toUtf8();
        if (code == 0 && status == QProcess::NormalExit && output.startsWith("app/" + id + '/')) {
            finishSuccess();
        } else {
            finishError(QStringLiteral("verification"), error);
        }
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_process != nullptr) {
            m_process->deleteLater();
            m_process = nullptr;
            finishError(QStringLiteral("verification"));
        }
    });
    m_process->start(flatpakExecutable(), {QStringLiteral("info"), m_flatpakScope, QStringLiteral("--show-ref"),
        m_profileId.mid(kAppPrefix.size())});
}

void InstallService::runHelperStep()
{
    m_receivedOutput = false;
    m_helperDone = false;
    m_errorKind.clear();
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    setPhase(Phase::Authenticating, i18n("Waiting for administrator authorization…"));

    m_process = new QProcess(this);
    if (m_profileId == QStringLiteral("freeoffice")) {
        if (!m_downloadFile.isOpen() || !m_downloadFile.seek(0)) {
            m_process->deleteLater();
            m_process = nullptr;
            finishError(QStringLiteral("freeoffice-invalid"));
            return;
        }
        connect(m_process, &QProcess::started, this, &InstallService::transferFreeOffice);
        connect(m_process, &QProcess::bytesWritten, this, &InstallService::transferFreeOffice);
    } else {
        m_process->setStandardInputFile(QProcess::nullDevice());
    }
    connect(m_process, &QProcess::readyReadStandardOutput, this, &InstallService::handleHelperOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        m_stderrBuffer.append(m_process->readAllStandardError());
        if (m_stderrBuffer.size() > 128 * 1024) {
            m_stderrBuffer = m_stderrBuffer.right(64 * 1024);
        }
    });
    connect(m_process, &QProcess::finished, this, &InstallService::finishStep);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_process != nullptr) {
            const QString technical = i18nc("@info %1 is a program path, %2 the system error",
                                                 "Could not run %1: %2", m_backend.pkexec, m_process->errorString());
            m_process->deleteLater();
            m_process = nullptr;
            finishError(QStringLiteral("unsupported"), technical);
        }
    });
    m_process->start(m_backend.pkexec, {m_backend.helper, QStringLiteral("install"), m_profileId});
}

void InstallService::handleHelperOutput()
{
    if (m_process == nullptr) {
        return;
    }
    m_stdoutBuffer.append(m_process->readAllStandardOutput());
    qsizetype newline = m_stdoutBuffer.indexOf('\n');
    while (newline >= 0) {
        const QString line = QString::fromUtf8(m_stdoutBuffer.left(newline));
        m_stdoutBuffer.remove(0, newline + 1);
        handleHelperLine(line);
        newline = m_stdoutBuffer.indexOf('\n');
    }
}

QString InstallService::labelForPackage(const QString &package) const
{
    const Profile *profile = findProfile(m_profileId);
    if (profile != nullptr) {
        for (const auto &label : profile->packageLabels) {
            if (package.startsWith(label.first)) {
                return label.second;
            }
        }
    }
    return i18nc("@item:intext generic component label", "required components");
}

void InstallService::handleHelperLine(const QString &line)
{
    using InstallProgress::HelperEvent;
    if (line.trimmed().isEmpty()) {
        return;
    }
    m_receivedOutput = true;
    const HelperEvent event = InstallProgress::parseHelperLine(line);
    const QString stepLabel = m_steps.value(m_stepIndex).label;

    switch (event.type) {
    case HelperEvent::Type::Phase:
        if (event.name == QStringLiteral("preparing")) {
            setPhase(Phase::Preparing, i18n("Checking the required components…"));
        } else if (event.name == QStringLiteral("waiting")) {
            setPhase(Phase::Waiting, i18n("Waiting for another program to finish installing or updating the system…"));
        } else if (event.name == QStringLiteral("updating")) {
            setPhase(Phase::Updating, i18n("Updating the list of available programs…"));
        } else if (event.name == QStringLiteral("installing")) {
            setPhase(Phase::Downloading, i18nc("@info:status %1 is a component label", "Preparing the download of %1…", stepLabel));
        } else if (event.name == QStringLiteral("signatures")) {
            setPhase(Phase::Configuring, i18n("Updating the virus definitions…"));
        }
        break;
    case HelperEvent::Type::Packages:
        appendDetails(i18nc("@info %1 is a list of package names", "Packages to install: %1", event.text));
        break;
    case HelperEvent::Type::Download: {
        if (m_phase == Phase::Updating) {
            // Index downloads report file counts, not size: keep the bar indeterminate.
            break;
        }
        int current = 0;
        int total = 0;
        QString text = i18nc("@info:status %1 is a component label", "Downloading %1…", stepLabel);
        if (InstallProgress::parseAptFileCounter(event.text, &current, &total)) {
            text = i18nc("@info:status %1 is a component label", "Downloading %1 (file %2 of %3)…", stepLabel, current, total);
        }
        if (m_phase != Phase::Downloading) {
            setPhase(Phase::Downloading, text, static_cast<int>(event.percent));
        } else {
            setProgress(static_cast<int>(event.percent), text);
        }
        break;
    }
    case HelperEvent::Type::Package: {
        const Phase target = event.configuring ? Phase::Configuring : Phase::Installing;
        QString text = m_message;
        if (event.name != QStringLiteral("dpkg-exec")) {
            const QString label = labelForPackage(event.name);
            text = event.configuring ? i18nc("@info:status %1 is a component label", "Configuring %1…", label)
                                     : i18nc("@info:status %1 is a component label", "Installing %1…", label);
        }
        if (m_phase != target) {
            setPhase(target, text, static_cast<int>(event.percent));
        } else {
            setProgress(static_cast<int>(event.percent), text);
        }
        break;
    }
    case HelperEvent::Type::PackageError:
        appendDetails(QStringLiteral("%1: %2").arg(event.name, event.text));
        break;
    case HelperEvent::Type::Error:
        m_errorKind = event.name;
        break;
    case HelperEvent::Type::Warning:
        if (event.name == QStringLiteral("signatures")) {
            m_warning = i18n("The virus definitions could not be downloaded now; ClamAV will retry "
                             "automatically in the background.");
            appendDetails(i18n("freshclam did not finish updating the definitions."));
        }
        break;
    case HelperEvent::Type::Done:
        m_helperDone = true;
        break;
    case HelperEvent::Type::Unknown:
        break;
    }
}

void InstallService::runFlatpakStep(const Step &step)
{
    const QString flatpak = flatpakExecutable();
    if (flatpak.isEmpty()) {
        finishError(QStringLiteral("flatpak-missing"));
        return;
    }

    if (m_flatpakScope.isEmpty()) {
        // Install where Flathub is configured: --user needs no administrator,
        // --system is authorized by Flatpak's own polkit helper.
        setPhase(Phase::Preparing, i18n("Checking Flathub…"));
        auto *remotes = new QProcess(this);
        remotes->setStandardInputFile(QProcess::nullDevice());
        connect(remotes, &QProcess::finished, this, [this, remotes, step, flatpak](int exitCode, QProcess::ExitStatus) {
            bool user = false;
            bool system = false;
            const QStringList lines = QString::fromUtf8(remotes->readAllStandardOutput()).split(QLatin1Char('\n'));
            for (const QString &line : lines) {
                const QStringList fields = line.split(QLatin1Char('\t'));
                if (fields.value(0).trimmed() == QStringLiteral("flathub")) {
                    const QString options = fields.value(1);
                    user = user || options.contains(QStringLiteral("user"));
                    system = system || options.contains(QStringLiteral("system"));
                }
            }
            remotes->deleteLater();
            if (exitCode == 0 && (user || system)) {
                m_flatpakScope = user ? QStringLiteral("--user") : QStringLiteral("--system");
                runFlatpakStep(step);
                return;
            }

            setPhase(Phase::Preparing, i18n("Setting up Flathub…"));
            auto *add = new QProcess(this);
            add->setStandardInputFile(QProcess::nullDevice());
            connect(add, &QProcess::finished, this, [this, add, step](int addExit, QProcess::ExitStatus) {
                const QString output = QString::fromUtf8(add->readAllStandardError());
                add->deleteLater();
                if (addExit != 0) {
                    appendDetails(output);
                    const QString kind = classifyFlatpakError(output);
                    finishError(kind == QStringLiteral("cancelled") ? QStringLiteral("failed") : kind);
                    return;
                }
                m_flatpakScope = QStringLiteral("--user");
                runFlatpakStep(step);
            });
            add->start(flatpak, {QStringLiteral("remote-add"), QStringLiteral("--user"),
                                 QStringLiteral("--if-not-exists"), QStringLiteral("flathub"), kFlathubUrl});
        });
        remotes->start(flatpak, {QStringLiteral("remotes"), QStringLiteral("--columns=name,options")});
        return;
    }

    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_flatpakOperation = 0;
    m_flatpakOperations = 0;
    m_flatpakPercent = -1;
    setPhase(Phase::Downloading, i18nc("@info:status %1 is a component label", "Downloading %1…", step.label));

    m_process = new QProcess(this);
    m_process->setStandardInputFile(QProcess::nullDevice());
    connect(m_process, &QProcess::readyReadStandardOutput, this, &InstallService::handleFlatpakOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        m_stderrBuffer.append(m_process->readAllStandardError());
    });
    connect(m_process, &QProcess::finished, this, &InstallService::finishStep);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_process != nullptr) {
            m_process->deleteLater();
            m_process = nullptr;
            finishError(QStringLiteral("flatpak-missing"));
        }
    });
    // No --noninteractive: it switches Flatpak to a quiet transaction without progress.
    m_process->start(flatpak, {QStringLiteral("install"), m_flatpakScope, QStringLiteral("-y"),
                               QStringLiteral("flathub"), step.flatpakId});
}

void InstallService::handleFlatpakOutput()
{
    if (m_process == nullptr) {
        return;
    }
    m_stdoutBuffer.append(m_process->readAllStandardOutput());
    // Flatpak ends progress lines with "\n" when stdout is not a terminal; accept "\r" too.
    m_stdoutBuffer.replace('\r', '\n');
    qsizetype newline = m_stdoutBuffer.indexOf('\n');
    while (newline >= 0) {
        const QString line = QString::fromUtf8(m_stdoutBuffer.left(newline));
        m_stdoutBuffer.remove(0, newline + 1);
        handleFlatpakLine(line);
        newline = m_stdoutBuffer.indexOf('\n');
    }
}

void InstallService::handleFlatpakLine(const QString &line)
{
    if (line.trimmed().isEmpty()) {
        return;
    }
    InstallProgress::FlatpakProgress state;
    state.operation = m_flatpakOperation;
    state.operations = m_flatpakOperations;
    state.percent = m_flatpakPercent;
    state = InstallProgress::parseFlatpakLine(line, state);
    if (!state.changed) {
        appendDetails(line.trimmed());
        return;
    }
    m_flatpakOperation = state.operation;
    m_flatpakOperations = state.operations;
    m_flatpakPercent = state.percent;

    const QString label = m_steps.value(m_stepIndex).label;
    const QString text = m_flatpakOperations > 1
        ? i18nc("@info:status %1 is a component label", "Downloading and installing %1 (component %2 of %3)…",
                label, m_flatpakOperation, m_flatpakOperations)
        : i18nc("@info:status %1 is a component label", "Downloading and installing %1…", label);
    setProgress(m_flatpakPercent, text);
}

void InstallService::finishStep(int exitCode, QProcess::ExitStatus status)
{
    if (m_process == nullptr) {
        return;
    }
    const Step step = m_steps.value(m_stepIndex);
    if (step.kind == StepKind::Helper) {
        handleHelperOutput();
        if (!m_stdoutBuffer.isEmpty()) {
            handleHelperLine(QString::fromUtf8(std::exchange(m_stdoutBuffer, {})));
        }
    } else {
        handleFlatpakOutput();
    }
    m_stderrBuffer.append(m_process->readAllStandardError());
    const QString errorOutput = QString::fromUtf8(m_stderrBuffer).trimmed();
    m_process->deleteLater();
    m_process = nullptr;

    const QString technical = i18nc("@info technical detail", "Exit code: %1", exitCode)
        + (errorOutput.isEmpty() ? QString() : QStringLiteral("\n") + lastLines(errorOutput, 80));

    if (status == QProcess::CrashExit) {
        finishError(QStringLiteral("failed"), technical);
        return;
    }

    if (step.kind == StepKind::Helper) {
        if (exitCode == 0 && m_helperDone) {
            // The helper may have installed Flatpak itself.
            m_packages->refresh();
            runNextStep();
        } else if (!m_receivedOutput && exitCode == 126) {
            // pkexec: the authentication dialog was dismissed.
            finishCancelled();
        } else if (!m_receivedOutput && exitCode == 127) {
            // pkexec: not authorized, authentication failed or no agent.
            finishError(QStringLiteral("auth"), technical);
        } else {
            finishError(m_errorKind.isEmpty() ? QStringLiteral("failed") : m_errorKind, technical);
        }
        return;
    }

    if (exitCode == 0 || errorOutput.contains(QStringLiteral("already installed"))) {
        runNextStep();
        return;
    }
    const QString kind = classifyFlatpakError(errorOutput);
    if (kind == QStringLiteral("cancelled")) {
        appendDetails(technical);
        finishCancelled();
    } else {
        finishError(kind, technical);
    }
}

void InstallService::setPhase(Phase phase, const QString &message, int percent)
{
    const bool wasBusy = busy();
    m_phase = phase;
    m_message = message;
    m_percent = percent;
    emit stateChanged();
    if (wasBusy != busy()) {
        emit busyChanged();
    }
}

void InstallService::setProgress(int percent, const QString &message)
{
    if (percent == m_percent && message == m_message) {
        return;
    }
    m_percent = percent;
    m_message = message;
    emit stateChanged();
}

void InstallService::appendDetails(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        return;
    }
    if (!m_details.isEmpty()) {
        m_details.append(QLatin1Char('\n'));
    }
    m_details.append(text.trimmed());
    if (m_details.count(QLatin1Char('\n')) > kMaxDetailLines) {
        m_details = lastLines(m_details, kMaxDetailLines);
    }
}

void InstallService::finishSuccess()
{
    const Profile *profile = findProfile(m_profileId);
    m_resultMessage = profile != nullptr ? profile->successMessage : QString();
    if (!m_warning.isEmpty()) {
        m_resultMessage += QStringLiteral("\n\n") + m_warning;
    }
    setPhase(Phase::Success, QString(), 100);
    endJob();
}

void InstallService::finishError(const QString &kind, const QString &technical)
{
    m_resultMessage = friendlyError(kind);
    appendDetails(i18nc("@info technical detail; %3 is an internal error code", "Step: %1 of %2 · reason: %3", step(), stepCount(), kind));
    appendDetails(technical);
    setPhase(Phase::Error, QString());
    endJob();
}

void InstallService::finishCancelled()
{
    m_resultMessage = m_stepIndex > 0 && m_profileId != QStringLiteral("freeoffice")
        ? i18n("The authorization was canceled. The components installed in the previous steps were kept.")
        : i18n("The authorization was canceled. No changes were made to the system.");
    setPhase(Phase::Cancelled, QString());
    endJob();
}

void InstallService::endJob()
{
    const QString completed = m_profileId;
    const bool success = m_phase == Phase::Success;
    m_downloadFile.close();
    m_downloadDirectory.reset();
    refresh();
    emit profilesChanged();
    emit finished(completed, success);
}

bool InstallService::launch(const QString &profileId)
{
    const Profile *profile = findProfile(profileId);
    if (profile == nullptr) {
        return false;
    }
    if (!profileState(*profile).value(QStringLiteral("canLaunch")).toBool()) {
        emit launchFailed(i18nc("@info %1 is an application or component name",
                                "Cannot open %1: the required components are not installed or the tool is unavailable.",
                                profile->name));
        return false;
    }

    QString program;
    QStringList arguments;
    switch (profile->launcher) {
    case Launcher::ClamUi:
        if (m_packages->isInstalled(kClamUiId) && !flatpakExecutable().isEmpty()) {
            program = flatpakExecutable();
            arguments = {QStringLiteral("run"), kClamUiId};
        } else {
            program = QStandardPaths::findExecutable(QStringLiteral("clamui"));
        }
        break;
    case Launcher::FirewallKcm:
        program = QStandardPaths::findExecutable(QStringLiteral("kcmshell6"));
        if (program.isEmpty()) {
            program = QStandardPaths::findExecutable(QStringLiteral("systemsettings"));
        }
        arguments = {QStringLiteral("kcm_firewall")};
        break;
    case Launcher::FlatpakApp:
        if (m_packages->isInstalled(profile->launchTarget)) {
            program = flatpakExecutable();
            arguments = {QStringLiteral("run"), profile->launchTarget};
        }
        break;
    case Launcher::None:
        return false;
    }

    if (program.isEmpty() || !QProcess::startDetached(program, arguments)) {
        emit launchFailed(i18nc("@info %1 is an application or component name", "Could not open %1.", profile->name));
        return false;
    }
    return true;
}
