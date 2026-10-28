#pragma once

#include <QRect>
#include <QSize>
#include <algorithm>

namespace WelcomeWindow {

// Available geometry already excludes Plasma panels. Leave room for the window
// frame, without limiting subsequent manual resizing or maximization.
inline QRect initialGeometry(const QRect &available)
{
    const QSize size(std::min(870, std::max(1, available.width() - 24)),
                     std::min(600, std::max(1, available.height() - 40)));
    return {QPoint(available.x() + (available.width() - size.width()) / 2,
                   available.y() + (available.height() - size.height()) / 2), size};
}

inline QSize minimumSize(const QSize &initial)
{
    return {std::min(760, std::max(1, initial.width() - 16)),
            std::min(520, std::max(1, initial.height() - 16))};
}

} // namespace WelcomeWindow
