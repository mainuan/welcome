#include "LayoutModel.h"

#include "services/LayoutService.h"

LayoutModel::LayoutModel(QObject *parent)
    : QAbstractListModel(parent)
{
    const QHash<QString, QString> descriptions = {
        {QStringLiteral("plasma-default"), QStringLiteral("O visual do Mainuan: ilhas flutuantes na base com menu, tarefas, bandeja e relógio.")},
        {QStringLiteral("three-panels"), QStringLiteral("Três segmentos flutuantes na base: menu e informações, tarefas e controles do sistema.")},
        {QStringLiteral("single-bar"), QStringLiteral("Uma barra inferior flutuante com margens laterais, widgets e aplicativos fixados.")},
        {QStringLiteral("panel-top"), QStringLiteral("Painel superior flutuante com TahoeLauncher, pesquisa, tarefas e controles.")},
        {QStringLiteral("minimal"), QStringLiteral("Barra inferior de largura total, sem flutuação, com Kickoff e aplicativos fixados.")},
        {QStringLiteral("unity"), QStringLiteral("Menu global no topo e painel vertical à esquerda com Orbit Start Menu.")},
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
