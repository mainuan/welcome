#pragma once

#include <QAbstractListModel>
#include <QList>

class VideoModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        TitleRole = Qt::UserRole + 1,
        FileNameRole,
        FileSourceRole,
        IconSourceRole,
        AvailableRole
    };

    explicit VideoModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Entry {
        QString title;
        QString fileName;
        QString fileSource;
        QString iconSource;
        bool available = false;
    };
    QList<Entry> m_entries;
};
