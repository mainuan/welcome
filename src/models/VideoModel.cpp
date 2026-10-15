#include "VideoModel.h"

#include <QFileInfo>
#include <QUrl>

VideoModel::VideoModel(QObject *parent)
    : QAbstractListModel(parent)
{
    const QString videoDirectory = QStringLiteral("/var/lib/curso-linux/videos");
    const QList<QStringList> videos = {
        {QStringLiteral("Conhecendo o sistema"), QStringLiteral("aula_1.mp4"), QStringLiteral("icone-videoaulas_g204.svg")},
        {QStringLiteral("Conectando à internet"), QStringLiteral("aula_2.mp4"), QStringLiteral("icone-videoaulas_g207.svg")},
        {QStringLiteral("Personalizando o sistema"), QStringLiteral("aula_3.mp4"), QStringLiteral("icone-videoaulas_g208.svg")},
        {QStringLiteral("Instalando programas"), QStringLiteral("aula_4.mp4"), QStringLiteral("icone-videoaulas_g205.svg")},
        {QStringLiteral("Aulas online"), QStringLiteral("aula_5.mp4"), QStringLiteral("icone-videoaulas_g209.svg")}
    };

    for (const QStringList &video : videos) {
        const QString path = videoDirectory + QLatin1Char('/') + video.at(1);
        m_entries.append({video.at(0), video.at(1), QUrl::fromLocalFile(path).toString(),
                          QStringLiteral("qrc:/assets/") + video.at(2), QFileInfo::exists(path)});
    }
}

int VideoModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant VideoModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case TitleRole: return entry.title;
    case FileNameRole: return entry.fileName;
    case FileSourceRole: return entry.fileSource;
    case IconSourceRole: return entry.iconSource;
    case AvailableRole: return entry.available;
    default: return {};
    }
}

QHash<int, QByteArray> VideoModel::roleNames() const
{
    return {{TitleRole, "title"}, {FileNameRole, "fileName"}, {FileSourceRole, "fileSource"},
            {IconSourceRole, "iconSource"}, {AvailableRole, "available"}};
}
