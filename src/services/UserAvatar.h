#pragma once

#include <QString>
#include <QStringList>

#include <sys/types.h>

namespace UserAvatar {

// Where the official Mainuan avatars may come from. A source is accepted only
// when its real path (symlinks resolved) is inside one of `dataRoots`, every
// directory from that root down to it and the file itself belong to `owner`
// (or root) and are not writable by group or others, and it is a regular SVG
// or PNG image. Production uses system(): /usr/share, owned by root.
struct SourcePolicy
{
    QStringList dataRoots;
    uid_t owner = 0;
    static SourcePolicy system();
};

// Official avatar sources for an icon theme, relative to a data root, in
// order of preference: the avatar matching the Kora color (in the icon theme,
// or as mainuan-user-avatar/<icon theme>.svg), the generic Mainuan avatar
// (mainuan-user-avatar/face.svg) and the logo of mainuan-desktop-defaults.
QStringList officialSources(const QString &iconTheme);

enum class Outcome {
    Updated,        // ~/.face now holds the official avatar
    AlreadyCurrent, // ~/.face already held it; nothing written
    KeptPersonal,   // ~/.face is the user's own picture (or not ours to touch)
    SourceMissing,  // no safe official avatar is installed; nothing written
    Failed,         // could not write; `error` says why, ~/.face is preserved
};

struct Result
{
    Outcome outcome = Outcome::Failed;
    QString error;
};

// Sets ~/.face to the first safe official source, only when ~/.face is
// missing or already one of the official avatars (byte-identical to a safe
// file in `knownSources`, or a link to one, as /etc/skel/.face is). A picture
// of the user's own is never replaced. Never follows ~/.face itself.
Result applyOfficial(const QStringList &sources, const QStringList &knownSources, const QString &home,
                     const SourcePolicy &policy = SourcePolicy::system());

// Explicit replacement of ~/.face with `source` (a regular file, never a
// link). Empty on success. Copies original image bytes, never follows
// destination symlinks, and keeps the first previous avatar in
// .face.mainuan-welcome-backup.
QString update(const QString &source, const QString &home);

}
