#include "ApplicationModel.h"

#include "services/InstallService.h"
#include "services/PackageService.h"

#include <QVariant>

ApplicationModel::ApplicationModel(PackageService *service, QList<ApplicationEntry> entries,
                                   QObject *parent)
    : QAbstractListModel(parent)
    , m_service(service)
    , m_entries(std::move(entries))
{
    Q_ASSERT(m_service != nullptr);
    connect(m_service, &PackageService::installedApplicationsChanged, this, &ApplicationModel::updateAllRows);
    connect(m_service, &PackageService::operationChanged, this, &ApplicationModel::updateAllRows);
    connect(m_service, &PackageService::operationStarted, this, &ApplicationModel::operationStarted);
    connect(m_service, &PackageService::operationFinished, this, &ApplicationModel::operationFinished);
}

void ApplicationModel::setInstallService(InstallService *installer)
{
    m_installer = installer;
    if (m_installer == nullptr) {
        return;
    }
    for (const ApplicationEntry &entry : std::as_const(m_entries)) {
        if (!entry.packageId.isEmpty() && entry.webUrl.isEmpty()) {
            m_installer->registerApplication(entry.packageId, entry.name, entry.description, entry.iconSource);
        }
    }
    connect(m_installer, &InstallService::busyChanged, this, &ApplicationModel::updateAllRows);
    connect(m_installer, &InstallService::finished, this, [this](const QString &profileId, bool success) {
        const QString packageId = profileId.section(QLatin1Char(':'), 1);
        if (success) {
            m_errors.remove(packageId);
        }
        updateAllRows();
    });
}

int ApplicationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant ApplicationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const ApplicationEntry &entry = m_entries.at(index.row());
    const bool installed = m_service->isInstalled(entry.packageId);
    const QString profile = entry.packageId.isEmpty() || !entry.webUrl.isEmpty()
        ? QString() : QStringLiteral("app:") + entry.packageId;
    const bool installing = m_installer != nullptr && m_installer->busy() && !profile.isEmpty()
        && m_installer->profileId() == profile;
    const bool busy = installing || m_busyPackages.contains(entry.packageId);
    const bool online = !entry.webUrl.isEmpty();
    const bool available = online || (PackageService::isAllowedApplication(entry.packageId)
                                      && m_service->flatpakAvailable());

    switch (role) {
    case ApplicationIdRole: return entry.id;
    case NameRole: return entry.name;
    case DescriptionRole: return entry.description;
    case PackageIdRole: return entry.packageId;
    case IconSourceRole: return entry.iconSource;
    case InstalledRole: return installed;
    case BusyRole: return busy;
    case AvailableRole: return available;
    case InstallStateRole:
        if (!available) {
            return QStringLiteral("Indisponível");
        }
        if (online) {
            return QStringLiteral("Online");
        }
        if (busy) {
            return installing ? QStringLiteral("Instalando…") : QStringLiteral("Removendo…");
        }
        if (m_errors.contains(entry.packageId)) {
            return QStringLiteral("Erro");
        }
        return installed ? QStringLiteral("Instalado") : QStringLiteral("Disponível");
    case WebUrlRole: return entry.webUrl;
    case InstallProfileRole: return profile;
    default: return {};
    }
}

QHash<int, QByteArray> ApplicationModel::roleNames() const
{
    return {
        {ApplicationIdRole, "applicationId"}, {NameRole, "name"},
        {DescriptionRole, "description"},   {PackageIdRole, "packageId"},
        {IconSourceRole, "iconSource"},      {InstalledRole, "installed"},
        {BusyRole, "busy"},                  {AvailableRole, "available"},
        {InstallStateRole, "installState"}, {WebUrlRole, "webUrl"},
        {InstallProfileRole, "installProfile"}
    };
}

bool ApplicationModel::remove(int row)
{
    if (row < 0 || row >= m_entries.size()) {
        return false;
    }
    return m_service->remove(m_entries.at(row).packageId);
}

void ApplicationModel::updateAllRows()
{
    if (m_entries.isEmpty()) {
        return;
    }
    emit dataChanged(index(0, 0), index(static_cast<int>(m_entries.size()) - 1, 0));
}

void ApplicationModel::operationStarted(const QString &applicationId)
{
    const int row = rowForPackage(applicationId);
    if (row >= 0) {
        m_busyPackages.insert(applicationId);
        notifyRow(row);
    }
}

void ApplicationModel::operationFinished(const QString &applicationId, bool success, const QString &message)
{
    const int row = rowForPackage(applicationId);
    m_busyPackages.remove(applicationId);
    if (!success) {
        m_errors.insert(applicationId, message);
    } else {
        m_errors.remove(applicationId);
    }
    if (row >= 0) {
        notifyRow(row);
    }
}

int ApplicationModel::rowForPackage(const QString &packageId) const
{
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).packageId == packageId) {
            return row;
        }
    }
    return -1;
}

void ApplicationModel::notifyRow(int row)
{
    emit dataChanged(index(row, 0), index(row, 0));
}
