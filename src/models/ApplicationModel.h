#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QSet>
#include <QString>

class InstallService;
class PackageService;

struct ApplicationEntry
{
    QString id;
    QString name;
    QString description;
    QString packageId;
    QString iconSource;
    QString webUrl;
};

class ApplicationModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        ApplicationIdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        PackageIdRole,
        IconSourceRole,
        InstalledRole,
        BusyRole,
        AvailableRole,
        InstallStateRole,
        WebUrlRole,
        InstallProfileRole
    };

    explicit ApplicationModel(PackageService *service, QList<ApplicationEntry> entries,
                              QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Registers the Flatpak entries as installer profiles ("app:<id>") and
    // reflects running installations in the busy/installState roles.
    void setInstallService(InstallService *installer);

    Q_INVOKABLE bool remove(int row);

private slots:
    void updateAllRows();
    void operationStarted(const QString &applicationId);
    void operationFinished(const QString &applicationId, bool success, const QString &message);

private:
    int rowForPackage(const QString &packageId) const;
    void notifyRow(int row);

    PackageService *m_service = nullptr;
    InstallService *m_installer = nullptr;
    QList<ApplicationEntry> m_entries;
    QSet<QString> m_busyPackages;
    QHash<QString, QString> m_errors;
};
