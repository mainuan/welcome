#include "UserAvatar.h"

#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <QUuid>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

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

} // namespace

QString UserAvatar::update(const QString &source, const QString &home)
{
    // This feature is a session operation, never an administrator operation.
    if (::geteuid() == 0) {
        return QStringLiteral("atualização do avatar não permitida como administrador");
    }
    const Descriptor input(::open(QFile::encodeName(source).constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
    struct stat sourceInfo {};
    if (input.fd < 0 || ::fstat(input.fd, &sourceInfo) != 0 || !S_ISREG(sourceInfo.st_mode)
        || sourceInfo.st_size <= 0 || sourceInfo.st_size > maxImageBytes) {
        return QStringLiteral("arquivo .face de origem ausente, ilegível ou inseguro");
    }
    QFile sourceFile;
    if (!sourceFile.open(input.fd, QIODevice::ReadOnly)) {
        return QStringLiteral("não foi possível ler o avatar de origem");
    }
    const QByteArray bytes = sourceFile.read(maxImageBytes + 1);
    if (bytes.size() > maxImageBytes || sourceFile.error() != QFileDevice::NoError) {
        return QStringLiteral("não foi possível ler o avatar de origem");
    }
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setDecideFormatFromContent(true);
    const QSize size = reader.size();
    if (!size.isValid() || size.width() > 8192 || size.height() > 8192 || reader.read().isNull()) {
        return QStringLiteral("o arquivo .face não contém uma imagem válida (SVG requer o plugin Qt SVG)");
    }

    const Descriptor directory(::open(QFile::encodeName(home).constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    struct stat homeInfo {};
    if (directory.fd < 0 || ::fstat(directory.fd, &homeInfo) != 0 || homeInfo.st_uid != ::geteuid()
        || (homeInfo.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
        return QStringLiteral("diretório pessoal indisponível ou com permissões inseguras");
    }

    struct stat previous {};
    const bool exists = ::fstatat(directory.fd, faceName, &previous, AT_SYMLINK_NOFOLLOW) == 0;
    if ((!exists && errno != ENOENT) || (exists && !regularOwnedFile(previous))) {
        return QStringLiteral("~/.face não é um arquivo regular seguro; avatar preservado");
    }
    if (exists) {
        struct stat backup {};
        const bool hasBackup = ::fstatat(directory.fd, backupName, &backup, AT_SYMLINK_NOFOLLOW) == 0;
        if ((!hasBackup && errno != ENOENT) || (hasBackup && !regularOwnedFile(backup))) {
            return QStringLiteral("backup do avatar inseguro; avatar preservado");
        }
        if (!hasBackup) {
            const Descriptor old(::openat(directory.fd, faceName, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
            struct stat opened {};
            if (old.fd < 0 || ::fstat(old.fd, &opened) != 0 || !regularOwnedFile(opened)
                || opened.st_dev != previous.st_dev || opened.st_ino != previous.st_ino
                || opened.st_size > maxImageBytes) {
                return QStringLiteral("não foi possível abrir o avatar anterior com segurança");
            }
            QFile oldFile;
            if (!oldFile.open(old.fd, QIODevice::ReadOnly)) {
                return QStringLiteral("não foi possível ler o avatar anterior");
            }
            const QByteArray original = oldFile.read(maxImageBytes + 1);
            if (original.size() > maxImageBytes || oldFile.error() != QFileDevice::NoError) {
                return QStringLiteral("não foi possível preservar o avatar anterior");
            }
            const QByteArray temporary = stage(directory.fd, original, 0600);
            if (temporary.isEmpty()) {
                return QStringLiteral("não foi possível gravar o backup; avatar preservado");
            }
            // linkat publishes the completed backup without overwriting any entry.
            const bool saved = ::linkat(directory.fd, temporary.constData(), directory.fd, backupName, 0) == 0;
            ::unlinkat(directory.fd, temporary.constData(), 0);
            if (!saved || ::fsync(directory.fd) != 0) {
                return QStringLiteral("não foi possível publicar o backup; avatar preservado");
            }
        }
    }

    const mode_t permissions = exists ? (previous.st_mode & 0644) | 0600 : 0644;
    const QByteArray temporary = stage(directory.fd, bytes, permissions);
    if (temporary.isEmpty()) {
        return QStringLiteral("não foi possível gravar o novo avatar; avatar preservado");
    }
    struct stat current {};
    const bool stillExists = ::fstatat(directory.fd, faceName, &current, AT_SYMLINK_NOFOLLOW) == 0;
    const bool unchanged = exists
        ? stillExists && regularOwnedFile(current) && current.st_dev == previous.st_dev && current.st_ino == previous.st_ino
        : !stillExists && errno == ENOENT;
    if (!unchanged || ::renameat(directory.fd, temporary.constData(), directory.fd, faceName) != 0) {
        ::unlinkat(directory.fd, temporary.constData(), 0);
        return QStringLiteral("não foi possível substituir ~/.face com segurança; avatar preservado");
    }
    if (::fsync(directory.fd) != 0) {
        return QStringLiteral("avatar atualizado, mas não foi possível confirmar a gravação em disco");
    }
    // Existing links (including .face.icon -> .face) and independent files stay
    // untouched. symlinkat cannot overwrite an entry created concurrently.
    struct stat icon {};
    if (::fstatat(directory.fd, ".face.icon", &icon, AT_SYMLINK_NOFOLLOW) != 0) {
        if (errno != ENOENT) {
            return QStringLiteral("avatar atualizado, mas não foi possível consultar ~/.face.icon");
        }
        if (::symlinkat(".face", directory.fd, ".face.icon") != 0 && errno != EEXIST) {
            return QStringLiteral("avatar atualizado, mas não foi possível criar ~/.face.icon");
        }
        if (::fsync(directory.fd) != 0) {
            return QStringLiteral("avatar atualizado, mas não foi possível confirmar ~/.face.icon em disco");
        }
    }
    return {};
}
