#include "InstallService.h"

#include "InstallProgressParser.h"
#include "PackageService.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

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
        return QStringLiteral("Outro programa está instalando ou atualizando o sistema agora. "
                              "Aguarde ele terminar e tente novamente.");
    }
    if (kind == QStringLiteral("network")) {
        return QStringLiteral("Não foi possível baixar os arquivos. Verifique sua conexão com a internet e tente novamente.");
    }
    if (kind == QStringLiteral("space")) {
        return QStringLiteral("Não há espaço livre suficiente no disco para concluir a instalação.");
    }
    if (kind == QStringLiteral("unavailable")) {
        return QStringLiteral("Os componentes necessários não foram encontrados nos repositórios configurados neste sistema.");
    }
    if (kind == QStringLiteral("interrupted")) {
        return QStringLiteral("Uma instalação anterior foi interrompida e precisa ser reparada antes de instalar novos programas.");
    }
    if (kind == QStringLiteral("dependencies")) {
        return QStringLiteral("O sistema de pacotes encontrou um conflito que impede a instalação sem remover outros programas.");
    }
    if (kind == QStringLiteral("auth")) {
        return QStringLiteral("A autenticação falhou ou o seu usuário não tem permissão de administrador.");
    }
    if (kind == QStringLiteral("unsupported")) {
        return QStringLiteral("A instalação automática destes componentes requer um sistema baseado em Debian ou Ubuntu.");
    }
    if (kind == QStringLiteral("flatpak-missing")) {
        return QStringLiteral("O Flatpak não está disponível neste sistema.");
    }
    return QStringLiteral("O sistema não conseguiu instalar todos os componentes. Tente novamente; se o problema "
                          "continuar, use “Ver detalhes” para obter as informações técnicas.");
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
    antivirus.name = QStringLiteral("Antivírus");
    antivirus.description = QStringLiteral("Proteção ClamAV com a interface gráfica ClamUI.");
    antivirus.iconName = QStringLiteral("security-high");
    antivirus.runningTitle = QStringLiteral("Instalando ClamAV + ClamUI");
    antivirus.successTitle = QStringLiteral("Antivírus instalado com sucesso");
    antivirus.successMessage = QStringLiteral("ClamAV e ClamUI estão prontos para uso.");
    antivirus.actionLabel = QStringLiteral("Configurar");
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
    firewall.name = QStringLiteral("Firewall");
    firewall.description = QStringLiteral("UFW com o módulo de firewall do KDE Plasma.");
    firewall.iconName = QStringLiteral("security-medium");
    firewall.runningTitle = QStringLiteral("Instalando Firewall");
    firewall.successTitle = QStringLiteral("Firewall instalado com sucesso");
    firewall.successMessage = QStringLiteral("O UFW e a integração com o KDE Plasma estão instalados. O firewall não foi "
                                             "ativado automaticamente: use Configurar para ativá-lo e escolher as regras.");
    firewall.actionLabel = QStringLiteral("Configurar");
    firewall.launcher = Launcher::FirewallKcm;
    firewall.usesHelper = true;
    firewall.components = {
        {QStringLiteral("UFW"), {QStringLiteral("ufw")}, QString(), {QStringLiteral("ufw")}},
        {QStringLiteral("integração com o KDE Plasma"), {QStringLiteral("plasma-firewall")}, QString(),
         {QStringLiteral("kcm:kcm_firewall")}}
    };
    firewall.packageLabels = {{QStringLiteral("ufw"), QStringLiteral("UFW")},
                              {QStringLiteral("plasma-firewall"), QStringLiteral("integração com o KDE Plasma")}};

    Profile codecs;
    codecs.id = QStringLiteral("codecs");
    codecs.name = QStringLiteral("Codecs de mídia");
    codecs.description = QStringLiteral("Suporte aos formatos de áudio e vídeo mais usados.");
    codecs.iconName = QStringLiteral("applications-multimedia");
    codecs.runningTitle = QStringLiteral("Instalando codecs de mídia");
    codecs.successTitle = QStringLiteral("Codecs instalados com sucesso");
    codecs.successMessage = QStringLiteral("Áudio e vídeo nos formatos mais comuns já podem ser reproduzidos.");
    codecs.usesHelper = true;
    codecs.components = {
        {QStringLiteral("codecs de mídia"),
         {QStringLiteral("gstreamer1.0-libav"), QStringLiteral("gstreamer1.0-plugins-ugly"),
          QStringLiteral("gstreamer1.0-plugins-bad"), QStringLiteral("ffmpeg")},
         QString(),
         {QStringLiteral("ffmpeg"), QStringLiteral("gst-launch-1.0")}}
    };
    codecs.packageLabels = {{QStringLiteral("gstreamer"), QStringLiteral("codecs GStreamer")},
                            {QStringLiteral("ffmpeg"), QStringLiteral("FFmpeg")},
                            {QStringLiteral("libav"), QStringLiteral("FFmpeg")}};

    m_profiles = {antivirus, firewall, codecs};

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
    return m_stepIndex < 0 ? 0 : m_stepIndex + 1;
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
    if (!PackageService::isAllowedApplication(flatpakId) || findProfile(kAppPrefix + flatpakId) != nullptr) {
        return false;
    }
    Profile profile;
    profile.id = kAppPrefix + flatpakId;
    profile.name = name;
    profile.description = description;
    profile.iconSource = iconSource;
    profile.runningTitle = QStringLiteral("Instalando %1").arg(name);
    profile.successTitle = QStringLiteral("%1 instalado com sucesso").arg(name);
    profile.successMessage = QStringLiteral("O aplicativo já está disponível no menu de aplicativos.");
    profile.actionLabel = QStringLiteral("Abrir");
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

    const bool hasAptComponents = std::any_of(profile.components.cbegin(), profile.components.cend(),
                                              [](const Component &c) { return !c.aptPackages.isEmpty(); });
    QString state;
    QString summary;
    if (hasAptComponents && !m_detected) {
        state = QStringLiteral("checking");
        summary = QStringLiteral("Verificando…");
    } else if (missing.isEmpty()) {
        state = QStringLiteral("installed");
        summary = QStringLiteral("Instalado");
    } else if (installedCount > 0) {
        state = QStringLiteral("partial");
        summary = missing.size() == 1 ? QStringLiteral("Falta: %1").arg(missing.constFirst())
                                      : QStringLiteral("Faltam: %1").arg(missing.join(QStringLiteral(", ")));
    } else {
        state = QStringLiteral("missing");
        summary = QStringLiteral("Não instalado");
    }

    QString unavailableReason;
    if (!installPossible && !missing.isEmpty()) {
        unavailableReason = needsApt ? friendlyError(QStringLiteral("unsupported"))
                                     : friendlyError(QStringLiteral("flatpak-missing"));
    }

    bool canLaunch = false;
    switch (profile.launcher) {
    case Launcher::ClamUi:
        canLaunch = m_packages->isInstalled(kClamUiId) || probeExists(QStringLiteral("clamui"));
        break;
    case Launcher::FirewallKcm:
        canLaunch = probeExists(QStringLiteral("kcm:kcm_firewall"))
            && (probeExists(QStringLiteral("kcmshell6")) || probeExists(QStringLiteral("systemsettings")));
        break;
    case Launcher::FlatpakApp:
        canLaunch = m_packages->isInstalled(profile.launchTarget);
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
            {QStringLiteral("description"), profile->description},
            {QStringLiteral("iconName"), profile->iconName},
            {QStringLiteral("iconSource"), profile->iconSource},
            {QStringLiteral("runningTitle"), profile->runningTitle},
            {QStringLiteral("successTitle"), profile->successTitle},
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
    connect(process, &QProcess::finished, this, [process, complete](int, QProcess::ExitStatus) {
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
        complete(true, installed);
    });
    connect(process, &QProcess::errorOccurred, this, [process, complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            complete(false, {});
        }
    });
    process->start(m_backend.dpkgQuery, arguments);
}

bool InstallService::start(const QString &profileId)
{
    if (busy()) {
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
    setPhase(Phase::Preparing, QStringLiteral("Preparando a instalação…"));
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
        m_steps.append({StepKind::Helper, QString(), helperLabels.join(QStringLiteral(" e "))});
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
        finishSuccess();
        return;
    }
    const Step step = m_steps.at(m_stepIndex);
    if (step.kind == StepKind::Helper) {
        runHelperStep();
    } else {
        runFlatpakStep(step);
    }
}

void InstallService::runHelperStep()
{
    m_receivedOutput = false;
    m_helperDone = false;
    m_errorKind.clear();
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    setPhase(Phase::Authenticating, QStringLiteral("Aguardando autorização do administrador…"));

    m_process = new QProcess(this);
    m_process->setStandardInputFile(QProcess::nullDevice());
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
            const QString technical = QStringLiteral("Não foi possível executar %1: %2")
                                          .arg(m_backend.pkexec, m_process->errorString());
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
    return QStringLiteral("componentes necessários");
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
            setPhase(Phase::Preparing, QStringLiteral("Verificando os componentes necessários…"));
        } else if (event.name == QStringLiteral("waiting")) {
            setPhase(Phase::Waiting, QStringLiteral("Aguardando outro programa terminar de instalar ou atualizar o sistema…"));
        } else if (event.name == QStringLiteral("updating")) {
            setPhase(Phase::Updating, QStringLiteral("Atualizando a lista de programas disponíveis…"));
        } else if (event.name == QStringLiteral("installing")) {
            setPhase(Phase::Downloading, QStringLiteral("Preparando o download de %1…").arg(stepLabel));
        } else if (event.name == QStringLiteral("signatures")) {
            setPhase(Phase::Configuring, QStringLiteral("Atualizando as definições de vírus…"));
        }
        break;
    case HelperEvent::Type::Packages:
        appendDetails(QStringLiteral("Pacotes a instalar: %1").arg(event.text));
        break;
    case HelperEvent::Type::Download: {
        if (m_phase == Phase::Updating) {
            // Index downloads report file counts, not size: keep the bar indeterminate.
            break;
        }
        int current = 0;
        int total = 0;
        QString text = QStringLiteral("Baixando %1…").arg(stepLabel);
        if (InstallProgress::parseAptFileCounter(event.text, &current, &total)) {
            text = QStringLiteral("Baixando %1 (arquivo %2 de %3)…").arg(stepLabel).arg(current).arg(total);
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
            text = event.configuring ? QStringLiteral("Configurando %1…").arg(label)
                                     : QStringLiteral("Instalando %1…").arg(label);
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
            m_warning = QStringLiteral("As definições de vírus não puderam ser baixadas agora; o ClamAV tentará "
                                       "novamente automaticamente em segundo plano.");
            appendDetails(QStringLiteral("freshclam não concluiu a atualização das definições."));
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
        setPhase(Phase::Preparing, QStringLiteral("Verificando o Flathub…"));
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

            setPhase(Phase::Preparing, QStringLiteral("Configurando o Flathub…"));
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
    setPhase(Phase::Downloading, QStringLiteral("Baixando %1…").arg(step.label));

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
        ? QStringLiteral("Baixando e instalando %1 (componente %2 de %3)…")
              .arg(label).arg(m_flatpakOperation).arg(m_flatpakOperations)
        : QStringLiteral("Baixando e instalando %1…").arg(label);
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

    const QString technical = QStringLiteral("Código de saída: %1").arg(exitCode)
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
    appendDetails(QStringLiteral("Etapa: %1 de %2 · motivo: %3").arg(step()).arg(stepCount()).arg(kind));
    appendDetails(technical);
    setPhase(Phase::Error, QString());
    endJob();
}

void InstallService::finishCancelled()
{
    m_resultMessage = m_stepIndex > 0
        ? QStringLiteral("A autorização foi cancelada. Os componentes instalados nas etapas anteriores foram mantidos.")
        : QStringLiteral("A autorização foi cancelada. Nenhuma alteração foi feita no sistema.");
    setPhase(Phase::Cancelled, QString());
    endJob();
}

void InstallService::endJob()
{
    const QString completed = m_profileId;
    const bool success = m_phase == Phase::Success;
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
        emit launchFailed(QStringLiteral("Não foi possível abrir %1.").arg(profile->name));
        return false;
    }
    return true;
}
