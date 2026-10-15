#include "PackageService.h"

#include <QFileInfo>
#include <QStandardPaths>

PackageService::PackageService(QObject *parent, const QString &flatpakProgram)
    : QObject(parent)
    , m_flatpakOverride(flatpakProgram)
{
    refresh();
}

QString PackageService::flatpakExecutable() const
{
    if (!m_flatpakOverride.isEmpty()) {
        const QFileInfo info(m_flatpakOverride);
        return info.isFile() && info.isExecutable() ? m_flatpakOverride : QString();
    }
    return QStandardPaths::findExecutable(QStringLiteral("flatpak"));
}

bool PackageService::flatpakAvailable() const
{
    return m_flatpakAvailable;
}

QString PackageService::lastMessage() const
{
    return m_lastMessage;
}

QString PackageService::currentApplication() const
{
    return m_currentApplication;
}

bool PackageService::busy() const
{
    return m_process != nullptr;
}

bool PackageService::isAllowedApplication(const QString &applicationId)
{
    static const QStringList allowlist = {
        QStringLiteral("org.libreoffice.LibreOffice"),
        QStringLiteral("com.wps.Office"),
        QStringLiteral("com.google.Chrome"),
        QStringLiteral("com.microsoft.Edge"),
        QStringLiteral("com.brave.Browser"),
        QStringLiteral("io.gitlab.librewolf-community"),
        QStringLiteral("com.opera.opera-gx"),
        QStringLiteral("org.torproject.torbrowser-launcher"),
        QStringLiteral("io.github.linx_systems.ClamUI")
    };
    return allowlist.contains(applicationId);
}

bool PackageService::isInstalled(const QString &applicationId) const
{
    return m_installations.contains(applicationId);
}

QString PackageService::installation(const QString &applicationId) const
{
    return m_installations.value(applicationId);
}

void PackageService::setMessage(const QString &message)
{
    if (message != m_lastMessage) {
        m_lastMessage = message;
        emit lastMessageChanged();
    }
}

void PackageService::refresh()
{
    if (m_process != nullptr) {
        return;
    }

    const QString flatpak = flatpakExecutable();
    const bool available = !flatpak.isEmpty();
    if (available != m_flatpakAvailable) {
        m_flatpakAvailable = available;
        emit flatpakAvailableChanged();
    }

    if (!available) {
        m_installations.clear();
        emit installedApplicationsChanged();
        setMessage(QStringLiteral("Flatpak não está instalado."));
        return;
    }

    QProcess *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            QHash<QString, QString> found;
            const QString output = QString::fromLocal8Bit(process->readAllStandardOutput());
            const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                const QStringList fields = line.split(QLatin1Char('\t'));
                if (fields.size() >= 2 && isAllowedApplication(fields.at(0).trimmed())) {
                    found.insert(fields.at(0).trimmed(), fields.at(1).trimmed());
                }
            }
            m_installations = found;
            emit installedApplicationsChanged();
        }
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setMessage(QStringLiteral("Não foi possível consultar o Flatpak."));
        }
        process->deleteLater();
    });
    process->start(flatpak, {QStringLiteral("list"), QStringLiteral("--app"),
                             QStringLiteral("--columns=application,installation")});
}

bool PackageService::remove(const QString &applicationId)
{
    if (!isAllowedApplication(applicationId)) {
        setMessage(QStringLiteral("Aplicação fora da lista permitida."));
        return false;
    }
    if (m_process != nullptr) {
        setMessage(QStringLiteral("Outra operação do Flatpak está em andamento."));
        return false;
    }

    const QString flatpak = flatpakExecutable();
    if (flatpak.isEmpty()) {
        setMessage(QStringLiteral("Flatpak não está instalado."));
        return false;
    }

    QStringList arguments = {QStringLiteral("uninstall"), QStringLiteral("--assumeyes"), QStringLiteral("--delete-data")};
    if (installation(applicationId) == QStringLiteral("user")) {
        arguments << QStringLiteral("--user");
    }
    arguments << applicationId;

    m_currentApplication = applicationId;
    m_process = new QProcess(this);
    m_process->setStandardInputFile(QProcess::nullDevice());
    emit operationChanged();
    emit operationStarted(applicationId);
    setMessage(QStringLiteral("Removendo %1…").arg(applicationId));

    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
        const QString stderrText = QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
        const QString stdoutText = QString::fromLocal8Bit(m_process->readAllStandardOutput()).trimmed();
        const bool success = exitCode == 0;
        const QString message = success ? QStringLiteral("Remoção concluída.")
                                        : (stderrText.isEmpty() ? stdoutText : stderrText);
        setMessage(success ? message : QStringLiteral("Falha: ") + message);
        const QString completedApplicationId = m_currentApplication;
        m_process->deleteLater();
        m_process = nullptr;
        m_currentApplication.clear();
        emit operationChanged();
        emit operationFinished(completedApplicationId, success, message);
        refresh();
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_process != nullptr) {
            setMessage(QStringLiteral("Não foi possível iniciar o Flatpak."));
            const QString completedApplicationId = m_currentApplication;
            m_process->deleteLater();
            m_process = nullptr;
            m_currentApplication.clear();
            emit operationChanged();
            emit operationFinished(completedApplicationId, false, m_lastMessage);
        }
    });
    m_process->start(flatpak, arguments);
    return true;
}
