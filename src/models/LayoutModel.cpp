#include "LayoutModel.h"

#include "services/LayoutService.h"

LayoutModel::LayoutModel(QObject *parent)
    : QAbstractListModel(parent)
{
    const QHash<QString, QString> descriptions = {
        {QStringLiteral("plasma-default"), QStringLiteral("O visual do Mainuan: ilhas flutuantes na base com menu, tarefas, bandeja e relógio.")},
        {QStringLiteral("panel-top"), QStringLiteral("Barra completa no topo com menu, áreas de trabalho, janelas abertas e relógio.")},
        {QStringLiteral("floating"), QStringLiteral("Um dock central que flutua acima da borda da tela, com tudo à mão.")},
        {QStringLiteral("minimal"), QStringLiteral("Uma barra fina na base que sai do caminho das janelas.")},
        {QStringLiteral("unity"), QStringLiteral("Barra de menus global no topo e um lançador vertical à esquerda.")},
        {QStringLiteral("tiling"), QStringLiteral("Barra compacta no topo e blocos do KWin para organizar as janelas lado a lado.")}};
    for (const QString &id : LayoutService::layoutIds()) {
        m_entries.append({id, LayoutService::displayName(id), descriptions.value(id)});
    }
}

int LayoutModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant LayoutModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case LayoutIdRole: return entry.id;
    case NameRole: return entry.name;
    case DescriptionRole: return entry.description;
    default: return {};
    }
}

QHash<int, QByteArray> LayoutModel::roleNames() const
{
    return {{LayoutIdRole, "layoutId"}, {NameRole, "name"}, {DescriptionRole, "description"}};
}
