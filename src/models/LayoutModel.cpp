#include "LayoutModel.h"

#include "services/LayoutService.h"

#include <KLocalizedString>
LayoutModel::LayoutModel(QObject *parent)
    : QAbstractListModel(parent)
{
    const QHash<QString, QString> descriptions = {
        {QStringLiteral("plasma-default"), i18nc("@info desktop layout description", "The Mainuan look: floating islands at the bottom with menu, tasks, system tray and clock.")},
        {QStringLiteral("three-panels"), i18nc("@info desktop layout description", "Three floating segments at the bottom: menu and information, tasks, and system controls.")},
        {QStringLiteral("single-bar"), i18nc("@info desktop layout description", "A floating bottom bar with side margins, widgets and pinned applications.")},
        {QStringLiteral("panel-top"), i18nc("@info desktop layout description", "Floating top panel with TahoeLauncher, search, tasks and controls.")},
        {QStringLiteral("minimal"), i18nc("@info desktop layout description", "Full-width bottom bar, not floating, with Kickoff and pinned applications.")},
        {QStringLiteral("unity"), i18nc("@info desktop layout description", "Global menu at the top and a vertical panel on the left with Orbit Start Menu.")},
        {QStringLiteral("tiling"), i18nc("@info desktop layout description", "Compact bar at the top and KWin tiles to arrange windows side by side.")}};
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
