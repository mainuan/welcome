#include "UserAvatar.h"

#include <KLocalizedString>

#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <QLoggingCategory>
#include <QUuid>

#include <cerrno>
#include <climits>
#include <functional>
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcAvatar, "mainuan.welcome.avatar")

namespace {

class Descriptor
{
public:
    explicit Descriptor(int value) : fd(value) {}
    ~Descriptor() { if (fd >= 0) ::close(fd); }
    Descriptor(const Descriptor &) = delete;
    Descriptor &operator=(const Descriptor &) = delete;
    const int fd;
};

constexpr qint64 maxImageBytes = 8 * 1024 * 1024;
constexpr const char *faceName = ".face";
constexpr const char *backupName = ".face.mainuan-welcome-backup";

bool regularOwnedFile(const struct stat &info)
{
    return S_ISREG(info.st_mode) && info.st_uid == ::geteuid();
}

// The directory descriptor anchors all writes even if a path is renamed.
// A random exclusive temporary file is fully written before it becomes visible.
QByteArray stage(int directory, const QByteArray &bytes, mode_t mode)
{
    const QByteArray name = ".mainuan-avatar-" + QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
    const Descriptor file(::openat(directory, name.constData(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    if (file.fd < 0) {
        return {};
    }
    qint64 offset = 0;
    while (offset < bytes.size()) {
        const ssize_t written = ::write(file.fd, bytes.constData() + offset, static_cast<size_t>(bytes.size() - offset));
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            ::unlinkat(directory, name.constData(), 0);
            return {};
        }
        offset += written;
    }
    if (::fchmod(file.fd, mode) != 0 || ::fsync(file.fd) != 0) {
        ::unlinkat(directory, name.constData(), 0);
        return {};
    }
    return name;
}

QByteArray encoded(const QString &path)
{
    return QFile::encodeName(path);
}

enum class ImageKind { Any, Official };

// Reads an opened regular file (at most maxImageBytes) and checks that it
// decodes as an image; official avatars must be SVG or PNG.
QString readImage(int fd, ImageKind kind, QByteArray *out)
{
    QFile file;
    if (!file.open(fd, QIODevice::ReadOnly)) {
        return i18nc("avatar error in a list of failed adjustments", "could not read the source avatar");
    }
    const QByteArray bytes = file.read(maxImageBytes + 1);
    if (bytes.size() > maxImageBytes || file.error() != QFileDevice::NoError) {
        return i18nc("avatar error in a list of failed adjustments", "could not read the source avatar");
    }
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setDecideFormatFromContent(true);
    const QByteArray format = reader.format();
    const QSize size = reader.size();
    if ((kind == ImageKind::Official && format != "svg" && format != "png")
        || !size.isValid() || size.width() > 8192 || size.height() > 8192 || reader.read().isNull()) {
        return i18nc("avatar error in a list of failed adjustments", "the .face file does not contain a valid image (SVG requires the Qt SVG plugin)");
    }
    *out = bytes;
    return {};
}

QString realPath(const QString &path)
{
    char resolved[PATH_MAX];
    return ::realpath(encoded(path).constData(), resolved) == nullptr ? QString() : QFile::decodeName(resolved);
}

struct Loaded
{
    QByteArray bytes;
    QString error;        // empty when `bytes` holds a safe official image
    bool missing = false; // nothing at all at the path
};

// Loads `path` only if it really is an official avatar under the policy (see
// UserAvatar::SourcePolicy). Links are resolved to decide where the file is,
// then the final path is opened without following links.
Loaded loadOfficial(const QString &path, const UserAvatar::SourcePolicy &policy)
{
    Loaded result;
    const QString real = realPath(path);
    if (real.isEmpty()) {
        result.missing = errno == ENOENT || errno == ENOTDIR;
        result.error = QStringLiteral("cannot resolve the path");
        return result;
    }
    QString root;
    for (const QString &candidate : policy.dataRoots) {
        const QString canonical = realPath(candidate);
        if (canonical.isEmpty()) {
            continue;
        }
        const QString prefix = canonical.endsWith(QLatin1Char('/')) ? canonical : canonical + QLatin1Char('/');
        if (real.startsWith(prefix)) {
            root = canonical;
            break;
        }
    }
    if (root.isEmpty()) {
        result.error = QStringLiteral("resolves outside the trusted data directories: ") + real;
        return result;
    }
    // Nobody but the owner (or root) may rename anything between the trusted
    // root and the file.
    const auto trustedDirectory = [&policy](const struct stat &info) {
        return S_ISDIR(info.st_mode) && (info.st_uid == policy.owner || info.st_uid == 0)
            && (info.st_mode & (S_IWGRP | S_IWOTH)) == 0;
    };
    const QStringList parts = real.mid(root.size()).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString directory = root;
    for (qsizetype i = 0; i < parts.size(); ++i) {
        struct stat info {};
        if (::lstat(encoded(directory).constData(), &info) != 0 || !trustedDirectory(info)) {
            result.error = QStringLiteral("unsafe directory: ") + directory;
            return result;
        }
        directory += QLatin1Char('/') + parts.at(i);
    }
    const Descriptor input(::open(encoded(real).constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
    struct stat info {};
    if (input.fd < 0 || ::fstat(input.fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_uid != policy.owner
        || (info.st_mode & (S_IWGRP | S_IWOTH)) != 0 || info.st_size <= 0 || info.st_size > maxImageBytes) {
        result.error = QStringLiteral("not a regular, protected file of the expected owner: ") + real;
        return result;
    }
    result.error = readImage(input.fd, ImageKind::Official, &result.bytes);
    if (!result.error.isEmpty()) {
        result.bytes.clear();
    }
    return result;
}

// Opens the home directory that anchors every write.
QString openHome(const QString &home, int *fd)
{
    *fd = ::open(encoded(home).constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    struct stat homeInfo {};
    if (*fd < 0 || ::fstat(*fd, &homeInfo) != 0 || homeInfo.st_uid != ::geteuid()
        || (homeInfo.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
        return i18nc("avatar error in a list of failed adjustments", "home folder unavailable or with unsafe permissions");
    }
    return {};
}

// Publishes `bytes` as ~/.face. `previous` is the entry that was checked (a
// regular file or a link to an official avatar), or null when there was none;
// if the entry changed meanwhile nothing is replaced.
QString writeFace(int directory, const QByteArray &bytes, const struct stat *previous)
{
    const bool regular = previous != nullptr && S_ISREG(previous->st_mode);
    const mode_t permissions = regular ? (previous->st_mode & 0644) | 0600 : 0644;
    const QByteArray temporary = stage(directory, bytes, permissions);
    if (temporary.isEmpty()) {
        return i18nc("avatar error in a list of failed adjustments", "could not write the new avatar; avatar preserved");
    }
    struct stat current {};
    const bool stillExists = ::fstatat(directory, faceName, &current, AT_SYMLINK_NOFOLLOW) == 0;
    const bool unchanged = previous != nullptr
        ? stillExists && current.st_dev == previous->st_dev && current.st_ino == previous->st_ino
            && (regular ? regularOwnedFile(current) : S_ISLNK(current.st_mode))
        : !stillExists && errno == ENOENT;
    if (!unchanged || ::renameat(directory, temporary.constData(), directory, faceName) != 0) {
        ::unlinkat(directory, temporary.constData(), 0);
        return i18nc("avatar error in a list of failed adjustments", "could not safely replace ~/.face; avatar preserved");
    }
    if (::fsync(directory) != 0) {
        return i18nc("avatar error in a list of failed adjustments", "avatar updated, but writing to disk could not be confirmed");
    }
    // Existing links (including .face.icon -> .face) and independent files stay
    // untouched. symlinkat cannot overwrite an entry created concurrently.
    struct stat icon {};
    if (::fstatat(directory, ".face.icon", &icon, AT_SYMLINK_NOFOLLOW) != 0) {
        if (errno != ENOENT) {
            return i18nc("avatar error in a list of failed adjustments", "avatar updated, but ~/.face.icon could not be checked");
        }
        if (::symlinkat(".face", directory, ".face.icon") != 0 && errno != EEXIST) {
            return i18nc("avatar error in a list of failed adjustments", "avatar updated, but ~/.face.icon could not be created");
        }
        if (::fsync(directory) != 0) {
            return i18nc("avatar error in a list of failed adjustments", "avatar updated, but ~/.face.icon could not be confirmed on disk");
        }
    }
    return {};
}

} // namespace

UserAvatar::SourcePolicy UserAvatar::SourcePolicy::system()
{
    return {{QStringLiteral("/usr/share")}, 0};
}

QStringList UserAvatar::officialSources(const QString &iconTheme)
{
    QStringList sources;
    if (!iconTheme.isEmpty() && !iconTheme.contains(QLatin1Char('/')) && !iconTheme.startsWith(QLatin1Char('.'))) {
        sources << QStringLiteral("icons/") + iconTheme + QStringLiteral("/status/scalable/.face")
                << QStringLiteral("mainuan-user-avatar/") + iconTheme + QStringLiteral(".svg");
    }
    sources << QStringLiteral("mainuan-user-avatar/face.svg") << QStringLiteral("plasma/avatars/logoMainuan.png");
    return sources;
}

UserAvatar::Result UserAvatar::applyOfficial(const QStringList &sources, const QStringList &knownSources,
                                             const QString &home, const SourcePolicy &policy)
{
    if (::geteuid() == 0) {
        return {Outcome::Failed, i18nc("avatar error in a list of failed adjustments", "avatar update not allowed as administrator")};
    }
    // The first safe official image; rejected candidates are only logged.
    QByteArray chosen;
    QList<QByteArray> known;
    const auto load = [&policy](const QString &relative, const std::function<void(const QByteArray &)> &accept) {
        for (const QString &root : policy.dataRoots) {
            const QString path = root + QLatin1Char('/') + relative;
            const Loaded loaded = loadOfficial(path, policy);
            if (loaded.error.isEmpty()) {
                accept(loaded.bytes);
                return true;
            }
            if (!loaded.missing) {
                qCWarning(lcAvatar).noquote() << "official avatar rejected:" << path << "-" << loaded.error;
            }
        }
        return false;
    };
    for (const QString &source : sources) {
        if (load(source, [&chosen](const QByteArray &bytes) { chosen = bytes; })) {
            break;
        }
    }
    if (chosen.isEmpty()) {
        qCInfo(lcAvatar) << "no official avatar installed; ~/.face left unchanged";
        return {Outcome::SourceMissing, {}};
    }
    known << chosen;
    for (const QString &source : knownSources + sources) {
        load(source, [&known](const QByteArray &bytes) { if (!known.contains(bytes)) known << bytes; });
    }

    int fd = -1;
    const QString homeError = openHome(home, &fd);
    const Descriptor directory(fd);
    if (!homeError.isEmpty()) {
        return {Outcome::Failed, homeError};
    }
    struct stat previous {};
    const bool exists = ::fstatat(directory.fd, faceName, &previous, AT_SYMLINK_NOFOLLOW) == 0;
    if (!exists && errno != ENOENT) {
        return {Outcome::Failed, i18nc("avatar error in a list of failed adjustments", "~/.face is not a safe regular file; avatar preserved")};
    }
    if (exists && S_ISLNK(previous.st_mode)) {
        // A link (as copied from /etc/skel) is ours only if it leads to an
        // official avatar; its target is inspected, never written.
        QByteArray target(PATH_MAX, '\0');
        const ssize_t length = ::readlinkat(directory.fd, faceName, target.data(), static_cast<size_t>(target.size()));
        if (length <= 0 || length >= target.size()) {
            return {Outcome::KeptPersonal, {}};
        }
        target.truncate(length);
        QString path = QFile::decodeName(target);
        if (!path.startsWith(QLatin1Char('/'))) {
            path = home + QLatin1Char('/') + path;
        }
        const Loaded linked = loadOfficial(path, policy);
        if (!linked.error.isEmpty() || !known.contains(linked.bytes)) {
            return {Outcome::KeptPersonal, {}};
        }
        if (linked.bytes == chosen) {
            return {Outcome::AlreadyCurrent, {}};
        }
    } else if (exists) {
        if (!regularOwnedFile(previous) || previous.st_size > maxImageBytes) {
            return {Outcome::KeptPersonal, {}};
        }
        const Descriptor old(::openat(directory.fd, faceName, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
        struct stat opened {};
        if (old.fd < 0 || ::fstat(old.fd, &opened) != 0 || !regularOwnedFile(opened)
            || opened.st_dev != previous.st_dev || opened.st_ino != previous.st_ino) {
            return {Outcome::KeptPersonal, {}};
        }
        QFile oldFile;
        if (!oldFile.open(old.fd, QIODevice::ReadOnly)) {
            return {Outcome::KeptPersonal, {}};
        }
        const QByteArray current = oldFile.read(maxImageBytes + 1);
        if (current == chosen) {
            return {Outcome::AlreadyCurrent, {}};
        }
        if (!known.contains(current)) {
            return {Outcome::KeptPersonal, {}};
        }
    }
    const QString error = writeFace(directory.fd, chosen, exists ? &previous : nullptr);
    return error.isEmpty() ? Result{Outcome::Updated, {}} : Result{Outcome::Failed, error};
}

QString UserAvatar::update(const QString &source, const QString &home)
{
    // This feature is a session operation, never an administrator operation.
    if (::geteuid() == 0) {
        return i18nc("avatar error in a list of failed adjustments", "avatar update not allowed as administrator");
    }
    const Descriptor input(::open(encoded(source).constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
    struct stat sourceInfo {};
    if (input.fd < 0 || ::fstat(input.fd, &sourceInfo) != 0 || !S_ISREG(sourceInfo.st_mode)
        || sourceInfo.st_size <= 0 || sourceInfo.st_size > maxImageBytes) {
        return i18nc("avatar error in a list of failed adjustments", "source .face file missing, unreadable or unsafe");
    }
    QByteArray bytes;
    const QString imageError = readImage(input.fd, ImageKind::Any, &bytes);
    if (!imageError.isEmpty()) {
        return imageError;
    }

    int fd = -1;
    const QString homeError = openHome(home, &fd);
    const Descriptor directory(fd);
    if (!homeError.isEmpty()) {
        return homeError;
    }

    struct stat previous {};
    const bool exists = ::fstatat(directory.fd, faceName, &previous, AT_SYMLINK_NOFOLLOW) == 0;
    if ((!exists && errno != ENOENT) || (exists && !regularOwnedFile(previous))) {
        return i18nc("avatar error in a list of failed adjustments", "~/.face is not a safe regular file; avatar preserved");
    }
    if (exists) {
        struct stat backup {};
        const bool hasBackup = ::fstatat(directory.fd, backupName, &backup, AT_SYMLINK_NOFOLLOW) == 0;
        if ((!hasBackup && errno != ENOENT) || (hasBackup && !regularOwnedFile(backup))) {
            return i18nc("avatar error in a list of failed adjustments", "unsafe avatar backup; avatar preserved");
        }
        if (!hasBackup) {
            const Descriptor old(::openat(directory.fd, faceName, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
            struct stat opened {};
            if (old.fd < 0 || ::fstat(old.fd, &opened) != 0 || !regularOwnedFile(opened)
                || opened.st_dev != previous.st_dev || opened.st_ino != previous.st_ino
                || opened.st_size > maxImageBytes) {
                return i18nc("avatar error in a list of failed adjustments", "could not safely open the previous avatar");
            }
            QFile oldFile;
            if (!oldFile.open(old.fd, QIODevice::ReadOnly)) {
                return i18nc("avatar error in a list of failed adjustments", "could not read the previous avatar");
            }
            const QByteArray original = oldFile.read(maxImageBytes + 1);
            if (original.size() > maxImageBytes || oldFile.error() != QFileDevice::NoError) {
                return i18nc("avatar error in a list of failed adjustments", "could not preserve the previous avatar");
            }
            const QByteArray temporary = stage(directory.fd, original, 0600);
            if (temporary.isEmpty()) {
                return i18nc("avatar error in a list of failed adjustments", "could not write the backup; avatar preserved");
            }
            // linkat publishes the completed backup without overwriting any entry.
            const bool saved = ::linkat(directory.fd, temporary.constData(), directory.fd, backupName, 0) == 0;
            ::unlinkat(directory.fd, temporary.constData(), 0);
            if (!saved || ::fsync(directory.fd) != 0) {
                return i18nc("avatar error in a list of failed adjustments", "could not publish the backup; avatar preserved");
            }
        }
    }
    return writeFace(directory.fd, bytes, exists ? &previous : nullptr);
}
