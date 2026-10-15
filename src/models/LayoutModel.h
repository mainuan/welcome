#pragma once

#include <QAbstractListModel>
#include <QList>

// The six desktop layouts shown in Aparência > Layout do desktop. Availability
// and the layout in use come from LayoutService.
class LayoutModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        LayoutIdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole
    };

    explicit LayoutModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Entry {
        QString id;
        QString name;
        QString description;
    };
    QList<Entry> m_entries;
};
