#pragma once

#include <QString>

namespace UserAvatar {
// Empty on success. Copies original image bytes, never follows destination
// symlinks, and keeps the first previous avatar in .face.mainuan-welcome-backup.
QString update(const QString &source, const QString &home);
}
