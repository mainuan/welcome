#include "SystemReportService.h"

#include "PlasmaInfo.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QLoggingCategory>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTimer>

#include <KLocalizedString>
#include <kconfig_version.h>

Q_LOGGING_CATEGORY(lcReport, "mainuan.welcome.report")

namespace {

constexpr int kToolTimeoutMs = 6000;
// A function, not a static: the text must be translated after the application
// domain and the user's languages are known.
QString unavailable()
{
    return i18nc("@info system report value", "Not available");
}

// Dates follow the user's regional format (pt_BR: dd/MM/yyyy HH:mm).
QString dateTime(const QDateTime &value)
{
    return QLocale().toString(value, QLocale::ShortFormat);
}

QString date(const QDate &value)
{
    return QLocale().toString(value, QLocale::ShortFormat);
}

QString firstLine(const QString &text)
{
    return text.section(QLatin1Char('\n'), 0, 0).trimmed();
}

QString readLink(const QString &path)
{
    const QString target = QFileInfo(path).symLinkTarget();
    return target.isEmpty() ? QString() : QFileInfo(target).fileName();
}

} // namespace

SystemReportService::SystemReportService(QObject *parent, const QString &root)
    : QObject(parent)
    , m_root(root)
{
}

QString SystemReportService::path(const QString &absolute) const
{
    return m_root + absolute;
}

QString SystemReportService::readFile(const QString &absolute) const
{
    QFile file(path(absolute));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

QHash<QString, QString> SystemReportService::parseOsRelease(const QString &text)
{
    QHash<QString, QString> values;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0 || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        QString value = line.mid(equals + 1).trimmed();
        if (value.size() >= 2 && (value.startsWith(QLatin1Char('"')) || value.startsWith(QLatin1Char('\'')))) {
            value = value.mid(1, value.size() - 2);
        }
        values.insert(line.left(equals).trimmed(), value);
    }
    return values;
}

SystemReportService::Cpu SystemReportService::parseCpuInfo(const QString &text)
{
    Cpu cpu;
    QSet<QString> cores;
    QString physical;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QString key = line.section(QLatin1Char(':'), 0, 0).trimmed();
        const QString value = line.section(QLatin1Char(':'), 1).trimmed();
        if (key == QStringLiteral("processor")) {
            ++cpu.threads;
        } else if (key == QStringLiteral("model name") && cpu.model.isEmpty()) {
            cpu.model = value.simplified();
        } else if (key == QStringLiteral("physical id")) {
            physical = value;
        } else if (key == QStringLiteral("core id")) {
            cores.insert(physical + QLatin1Char('/') + value);
        }
    }
    cpu.cores = cores.isEmpty() ? cpu.threads : static_cast<int>(cores.size());
    return cpu;
}

SystemReportService::Memory SystemReportService::parseMemInfo(const QString &text)
{
    Memory memory;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QString key = line.section(QLatin1Char(':'), 0, 0).trimmed();
        const qint64 value = line.section(QLatin1Char(':'), 1).trimmed().section(QLatin1Char(' '), 0, 0).toLongLong();
        if (key == QStringLiteral("MemTotal")) {
            memory.totalKiB = value;
        } else if (key == QStringLiteral("MemAvailable")) {
            memory.availableKiB = value;
        } else if (key == QStringLiteral("SwapTotal")) {
            memory.swapTotalKiB = value;
        } else if (key == QStringLiteral("SwapFree")) {
            memory.swapFreeKiB = value;
        }
    }
    return memory;
}

int SystemReportService::countInstalledPackages(const QString &dpkgStatus)
{
    // Removed packages keep "deinstall ok config-files" stanzas: only count installed ones.
    return static_cast<int>(dpkgStatus.count(QStringLiteral("\nStatus: install ok installed"))
                            + (dpkgStatus.startsWith(QStringLiteral("Status: install ok installed")) ? 1 : 0));
}

QString SystemReportService::maskMac(const QString &mac)
{
    const QStringList parts = mac.trimmed().split(QLatin1Char(':'));
    if (parts.size() != 6) {
        return {};
    }
    // The vendor prefix helps support; the device part identifies the machine.
    return QStringList{parts.at(0), parts.at(1), parts.at(2), QStringLiteral("xx"), QStringLiteral("xx"), QStringLiteral("xx")}
        .join(QLatin1Char(':'));
}

QString SystemReportService::publicMountPoint(const QString &mountPoint)
{
    // Removable and user mounts (/media/<user>/<label>, /run/media/…, /home/<user>/…)
    // name the user or the volume, so only system locations are shown.
    static const QRegularExpression system(QStringLiteral("^/(boot(/efi)?|home|var|usr|opt|srv|tmp)?$"));
    return system.match(mountPoint).hasMatch() ? mountPoint : QString();
}

QString SystemReportService::publicInterfaceName(const QString &interface)
{
    // systemd names USB and some Wi-Fi adapters after their MAC (enx001122334455).
    static const QRegularExpression byMac(QStringLiteral("^(enx|wlx)([0-9a-f]{6})[0-9a-f]{6}$"));
    const QRegularExpressionMatch match = byMac.match(interface);
    return match.hasMatch() ? match.captured(1) + match.captured(2) + QStringLiteral("xxxxxx") : interface;
}

QString SystemReportService::humanBytes(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

QString SystemReportService::humanDuration(qint64 seconds)
{
    const qint64 days = seconds / 86400;
    const qint64 hours = (seconds % 86400) / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    QStringList parts;
    if (days > 0) {
        parts << i18np("%1 day", "%1 days", days);
    }
    if (hours > 0) {
        parts << i18np("%1 hour", "%1 hours", hours);
    }
    if (minutes > 0 && days == 0) {
        parts << i18np("%1 minute", "%1 minutes", minutes);
    }
    if (parts.isEmpty()) {
        return i18n("less than a minute");
    }
    if (parts.size() == 1) {
        return parts.constFirst();
    }
    const QString last = parts.takeLast();
    // i18n: joins the leading parts of a duration ("2 days, 3 hours") with the last one
    return i18nc("@info duration", "%1 and %2", parts.join(i18nc("@info duration list separator", ", ")), last);
}

QHash<QString, QString> SystemReportService::parseLspci(const QString &text)
{
    // `lspci -mm`: 00:02.0 "VGA compatible controller" "Vendor" "Device" -r01 …
    static const QRegularExpression line(QStringLiteral(R"re(^(\S+)\s+"[^"]*"\s+"([^"]*)"\s+"([^"]*)")re"));
    QHash<QString, QString> devices;
    for (const QString &entry : text.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch match = line.match(entry);
        if (match.hasMatch()) {
            devices.insert(match.captured(1), (match.captured(2) + QLatin1Char(' ') + match.captured(3)).simplified());
        }
    }
    return devices;
}

QVariantList SystemReportService::sections() const
{
    QVariantList list;
    for (const Section &section : m_sections) {
        QVariantList items;
        for (const Item &item : section.items) {
            items.append(QVariantMap{{QStringLiteral("label"), item.label}, {QStringLiteral("value"), item.value}});
        }
        list.append(QVariantMap{{QStringLiteral("id"), section.id},
                                {QStringLiteral("title"), section.title},
                                {QStringLiteral("icon"), section.icon},
                                {QStringLiteral("items"), items}});
    }
    return list;
}

QVariantMap SystemReportService::summary() const
{
    return m_summary;
}

bool SystemReportService::busy() const
{
    return m_busy;
}

bool SystemReportService::ready() const
{
    return m_generatedAt.isValid();
}

void SystemReportService::refresh()
{
    if (m_busy) {
        return;
    }
    m_busy = true;
    emit busyChanged();
    m_toolOutput.clear();
    m_pendingTools = 1; // released after every tool has been started
    if (m_root.isEmpty()) {
        ++m_pendingTools;
        PlasmaInfo::queryVersion(this, [this](const QString &version) {
            if (!version.isEmpty()) {
                m_toolOutput.insert(QStringLiteral("plasma"), version);
            }
            toolFinished();
        });
    }
    runTool(QStringLiteral("lspci"), QStringLiteral("lspci"), {QStringLiteral("-mm")});
    runTool(QStringLiteral("lsblk"), QStringLiteral("lsblk"),
            {QStringLiteral("-J"), QStringLiteral("-b"), QStringLiteral("-o"), QStringLiteral("NAME,TYPE,SIZE,MODEL,ROTA,TRAN")});
    runTool(QStringLiteral("glxinfo"), QStringLiteral("glxinfo"), {QStringLiteral("-B")});
    runTool(QStringLiteral("frameworks"), QStringLiteral("dpkg-query"),
            {QStringLiteral("-W"), QStringLiteral("-f=${Version}"), QStringLiteral("libkf6coreaddons6")});
    toolFinished();
}

void SystemReportService::runTool(const QString &key, const QString &program, const QStringList &arguments)
{
    const QString executable = QStandardPaths::findExecutable(program);
    if (executable.isEmpty() || !m_root.isEmpty()) {
        return; // optional tool; tests use files only
    }
    ++m_pendingTools;
    auto *process = new QProcess(this);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process->setProcessEnvironment(environment);
    process->setStandardInputFile(QProcess::nullDevice());
    auto *timeout = new QTimer(process);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, process, [process, key]() {
        qCWarning(lcReport) << key << "timed out";
        process->kill();
    });
    connect(process, &QProcess::finished, this, [this, process, key](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit && exitCode == 0) {
            m_toolOutput.insert(key, QString::fromUtf8(process->readAllStandardOutput()));
        }
        process->deleteLater();
        toolFinished();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            toolFinished();
        }
    });
    timeout->start(kToolTimeoutMs);
    process->start(executable, arguments);
}

void SystemReportService::toolFinished()
{
    if (--m_pendingTools > 0) {
        return;
    }
    build();
    m_busy = false;
    emit busyChanged();
}

void SystemReportService::build()
{
    m_generatedAt = QDateTime::currentDateTime();
    const QHash<QString, QString> os = parseOsRelease(readFile(QStringLiteral("/etc/os-release")));
    const QString osName = os.value(QStringLiteral("PRETTY_NAME"), os.value(QStringLiteral("NAME"), QSysInfo::prettyProductName()));
    const QString plasma = m_toolOutput.value(QStringLiteral("plasma"));
    const QString frameworks = m_toolOutput.value(QStringLiteral("frameworks")).section(QLatin1Char('-'), 0, 0).trimmed();
    const Cpu cpu = parseCpuInfo(readFile(QStringLiteral("/proc/cpuinfo")));
    const Memory memory = parseMemInfo(readFile(QStringLiteral("/proc/meminfo")));
    const qint64 uptime = static_cast<qint64>(readFile(QStringLiteral("/proc/uptime")).section(QLatin1Char(' '), 0, 0).toDouble());
    const QString architecture = QSysInfo::WordSize == 64
        ? i18nc("@info CPU architecture", "%1 (64-bit)", QSysInfo::currentCpuArchitecture())
        : i18nc("@info CPU architecture", "%1 (32-bit)", QSysInfo::currentCpuArchitecture());

    m_sections.clear();

    // System. The installer writes /var/log/installer when it finishes, so its
    // date is the installation date; without it, the creation time of the root
    // file system is only an estimate and is labelled as such.
    Section system{QStringLiteral("system"), i18nc("@title system report section", "System"), QStringLiteral("info"), {}};
    system.items << Item{i18nc("@label system report item", "Distribution"), osName};
    if (os.contains(QStringLiteral("VERSION"))) {
        system.items << Item{i18nc("@label system report item", "Version"), os.value(QStringLiteral("VERSION"))};
    }
    if (os.value(QStringLiteral("ID")) == QStringLiteral("ubuntu") || os.value(QStringLiteral("ID_LIKE")).contains(QStringLiteral("ubuntu"))) {
        system.items << Item{i18nc("@label system report item", "Base"), QStringLiteral("Ubuntu %1").arg(os.value(QStringLiteral("UBUNTU_CODENAME"), os.value(QStringLiteral("VERSION_CODENAME"))))};
    } else if (os.contains(QStringLiteral("ID_LIKE"))) {
        system.items << Item{i18nc("@label system report item", "Base"), os.value(QStringLiteral("ID_LIKE"))};
    }
    const QString media = firstLine(readFile(QStringLiteral("/var/log/installer/media-info")));
    if (!media.isEmpty()) {
        system.items << Item{i18nc("@label system report item", "Installation media"), media};
    }
    bool installEstimated = false;
    QDateTime installed = QFileInfo(path(QStringLiteral("/var/log/installer/media-info"))).lastModified();
    if (!installed.isValid()) {
        installed = QFileInfo(path(QStringLiteral("/var/log/installer"))).lastModified();
    }
    if (!installed.isValid()) {
        installed = QFileInfo(path(QStringLiteral("/"))).birthTime();
        installEstimated = true;
    }
    const QString installLabel = installEstimated ? i18nc("@label system report item", "Estimated installation date") : i18nc("@label system report item", "Installation date");
    system.items << Item{installLabel, installed.isValid() ? date(installed.date()) : unavailable()};
    system.items << Item{i18nc("@label system report item", "Architecture"), architecture};
    // Mainuan's default host name contains the user name: shown, not exported.
    system.items << Item{i18nc("@label system report item", "Computer name"), QSysInfo::machineHostName(), false};
    system.items << Item{QStringLiteral("Kernel"), QSysInfo::kernelVersion()};
    system.items << Item{i18nc("@label system report item", "Language"), QLocale::system().name()};
    system.items << Item{i18nc("@label system report item", "Date and time"), dateTime(m_generatedAt)};
    m_sections << system;

    Section desktop{QStringLiteral("desktop"), i18nc("@title system report section", "Desktop"), QStringLiteral("layout-panel-top"), {}};
    desktop.items << Item{QStringLiteral("KDE Plasma"), plasma.isEmpty() ? unavailable() : plasma};
    desktop.items << Item{QStringLiteral("KDE Frameworks"), frameworks.isEmpty() ? QStringLiteral(KCONFIG_VERSION_STRING) : frameworks};
    desktop.items << Item{QStringLiteral("Qt"), QString::fromLatin1(qVersion())};
    desktop.items << Item{i18nc("@label system report item", "Session"), qEnvironmentVariable("XDG_SESSION_TYPE", unavailable())};
    desktop.items << Item{i18nc("@label system report item", "Environment"), qEnvironmentVariable("XDG_CURRENT_DESKTOP", unavailable())};
    m_sections << desktop;

    Section hardware{QStringLiteral("hardware"), i18nc("@title system report section", "Hardware"), QStringLiteral("cpu"), {}};
    const QString vendor = readFile(QStringLiteral("/sys/class/dmi/id/sys_vendor")).trimmed();
    const QString product = readFile(QStringLiteral("/sys/class/dmi/id/product_name")).trimmed();
    if (!vendor.isEmpty() || !product.isEmpty()) {
        hardware.items << Item{i18nc("@label system report item", "Computer"), (vendor + QLatin1Char(' ') + product).trimmed()};
    }
    hardware.items << Item{i18nc("@label system report item", "Processor"), cpu.model.isEmpty() ? unavailable() : cpu.model};
    hardware.items << Item{i18nc("@label system report item", "Cores / threads"), QStringLiteral("%1 / %2").arg(cpu.cores).arg(cpu.threads)};
    const bool uefi = QFileInfo::exists(path(QStringLiteral("/sys/firmware/efi")));
    hardware.items << Item{QStringLiteral("Firmware"), uefi ? QStringLiteral("UEFI") : i18nc("@info firmware type", "BIOS (legacy)")};
    QString secureBoot = i18nc("@info Secure Boot state", "Not applicable (BIOS)");
    if (uefi) {
        QFile variable(path(QStringLiteral("/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c")));
        if (variable.open(QIODevice::ReadOnly)) {
            const QByteArray data = variable.readAll(); // 4 attribute bytes, then the value
            secureBoot = data.size() >= 5 && data.at(4) == 1 ? i18nc("@info Secure Boot state", "Enabled") : i18nc("@info Secure Boot state", "Disabled");
        } else {
            secureBoot = i18nc("@info Secure Boot state", "Could not be determined");
        }
    }
    hardware.items << Item{QStringLiteral("Secure Boot"), secureBoot};
    m_sections << hardware;

    // Graphics: DRM cards from /sys, names from lspci when available.
    Section graphics{QStringLiteral("graphics"), i18nc("@title system report section", "Graphics"), QStringLiteral("monitor"), {}};
    const QHash<QString, QString> pci = parseLspci(m_toolOutput.value(QStringLiteral("lspci")));
    QStringList gpuNames;
    const QStringList cards = QDir(path(QStringLiteral("/sys/class/drm"))).entryList({QStringLiteral("card*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    static const QRegularExpression cardName(QStringLiteral("^card\\d+$"));
    for (const QString &card : cards) {
        if (!cardName.match(card).hasMatch()) {
            continue;
        }
        const QString device = path(QStringLiteral("/sys/class/drm/")) + card + QStringLiteral("/device");
        const QString slot = QFileInfo(QFileInfo(device).symLinkTarget()).fileName().section(QLatin1Char(':'), 1);
        QString driver = readLink(device + QStringLiteral("/driver"));
        if (driver == QStringLiteral("virtio-pci")) {
            // The PCI link names the bus driver; the DRM driver sits on the virtio device.
            for (const QString &child : QDir(device).entryList({QStringLiteral("virtio*")}, QDir::Dirs)) {
                const QString inner = readLink(device + QLatin1Char('/') + child + QStringLiteral("/driver"));
                if (!inner.isEmpty()) {
                    driver = inner;
                }
            }
        }
        const QString name = pci.value(slot, QStringLiteral("PCI %1").arg(slot.isEmpty() ? card : slot));
        gpuNames << name;
        graphics.items << Item{QStringLiteral("GPU %1").arg(gpuNames.size()),
                               name + (driver.isEmpty() ? QString() : i18nc("@info appended to a device name", " · driver %1", driver))};
    }
    if (gpuNames.isEmpty()) {
        graphics.items << Item{QStringLiteral("GPU"), unavailable()};
    }
    static const QRegularExpression renderer(QStringLiteral("OpenGL renderer string:\\s*(.+)"));
    const QRegularExpressionMatch rendererMatch = renderer.match(m_toolOutput.value(QStringLiteral("glxinfo")));
    if (rendererMatch.hasMatch()) {
        graphics.items << Item{i18nc("@label system report item", "OpenGL renderer"), rendererMatch.captured(1).trimmed()};
    }
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance()) != nullptr) {
        const QList<QScreen *> screens = QGuiApplication::screens();
        for (qsizetype i = 0; i < screens.size(); ++i) {
            const QScreen *screen = screens.at(i);
            const QSize pixels = screen->size() * screen->devicePixelRatio();
            graphics.items << Item{i18nc("@label system report item", "Monitor %1", i + 1),
                                   // i18n: screen name · width×height · refresh rate · scale percentage
                                   i18nc("@info monitor details", "%1 · %2×%3 · %4 Hz · scale %5%", screen->name(),
                                         QString::number(pixels.width()), QString::number(pixels.height()),
                                         QString::number(qRound(screen->refreshRate())),
                                         QString::number(qRound(screen->devicePixelRatio() * 100)))};
        }
    }
    m_sections << graphics;

    Section memorySection{QStringLiteral("memory"), i18nc("@title system report section", "Memory"), QStringLiteral("memory-stick"), {}};
    const qint64 usedKiB = memory.totalKiB - memory.availableKiB;
    memorySection.items << Item{i18nc("@label system report item", "Total"), humanBytes(memory.totalKiB * 1024)};
    memorySection.items << Item{i18nc("@label system report item", "In use"), humanBytes(usedKiB * 1024)};
    memorySection.items << Item{i18nc("@label system report item", "Available"), humanBytes(memory.availableKiB * 1024)};
    memorySection.items << Item{QStringLiteral("Swap"), memory.swapTotalKiB > 0
        ? i18nc("@info swap usage", "%1 of %2 in use", humanBytes((memory.swapTotalKiB - memory.swapFreeKiB) * 1024), humanBytes(memory.swapTotalKiB * 1024))
        : i18n("No swap")};
    m_sections << memorySection;

    // Storage: physical disks from lsblk (no serial numbers), mounted volumes from Qt.
    Section storage{QStringLiteral("storage"), i18nc("@title system report section", "Storage"), QStringLiteral("hard-drive"), {}};
    const QJsonArray devices = QJsonDocument::fromJson(m_toolOutput.value(QStringLiteral("lsblk")).toUtf8())
                                   .object().value(QStringLiteral("blockdevices")).toArray();
    for (const QJsonValue &value : devices) {
        const QJsonObject disk = value.toObject();
        if (disk.value(QStringLiteral("type")).toString() != QStringLiteral("disk")) {
            continue;
        }
        const QString transport = disk.value(QStringLiteral("tran")).toString();
        const QString kind = transport == QStringLiteral("nvme") ? QStringLiteral("NVMe")
            : (disk.value(QStringLiteral("rota")).toBool() ? QStringLiteral("HDD") : QStringLiteral("SSD"));
        const QString model = disk.value(QStringLiteral("model")).toString().trimmed();
        storage.items << Item{i18nc("@label system report item", "Disk %1", disk.value(QStringLiteral("name")).toString()),
                              QStringLiteral("%1 · %2%3%4").arg(humanBytes(static_cast<qint64>(disk.value(QStringLiteral("size")).toDouble())), kind,
                                                                transport.isEmpty() ? QString() : QStringLiteral(" · ") + transport,
                                                                model.isEmpty() ? QString() : QStringLiteral(" · ") + model)};
    }
    QStorageInfo rootVolume;
    int otherVolumes = 0;
    for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
        const QString device = QString::fromUtf8(volume.device());
        if (!volume.isValid() || !volume.isReady() || !device.startsWith(QStringLiteral("/dev/"))
            || device.startsWith(QStringLiteral("/dev/loop")) || volume.bytesTotal() <= 0) {
            continue;
        }
        if (volume.isRoot()) {
            rootVolume = volume;
        }
        const qint64 used = volume.bytesTotal() - volume.bytesFree();
        QString mountPoint = publicMountPoint(volume.rootPath());
        if (mountPoint.isEmpty()) {
            mountPoint = i18nc("@label system report item", "Other volume %1", QString::number(++otherVolumes));
        }
        storage.items << Item{mountPoint + QStringLiteral(" (") + QString::fromUtf8(volume.fileSystemType()) + QLatin1Char(')'),
                              i18nc("@info volume usage", "%1 used of %2 · %3 free", humanBytes(used), humanBytes(volume.bytesTotal()), humanBytes(volume.bytesFree()))};
    }
    if (storage.items.isEmpty()) {
        storage.items << Item{i18nc("@label system report item", "Disks"), unavailable()};
    }
    m_sections << storage;

    // Packages: count without starting package managers.
    Section packages{QStringLiteral("packages"), i18nc("@title system report section", "Packages"), QStringLiteral("package"), {}};
    const QString dpkgStatus = readFile(QStringLiteral("/var/lib/dpkg/status"));
    packages.items << Item{QStringLiteral("APT / dpkg"), dpkgStatus.isEmpty() ? unavailable()
                                                                              : i18np("%1 package", "%1 packages", countInstalledPackages(dpkgStatus))};
    int flatpaks = 0;
    const QStringList flatpakRoots = {path(QStringLiteral("/var/lib/flatpak/app")),
                                      m_root.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/flatpak/app") : QString()};
    for (const QString &rootDir : flatpakRoots) {
        if (rootDir.isEmpty()) {
            continue;
        }
        for (const QString &app : QDir(rootDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            flatpaks += QFileInfo::exists(rootDir + QLatin1Char('/') + app + QStringLiteral("/current")) ? 1 : 0;
        }
    }
    packages.items << Item{QStringLiteral("Flatpak"), i18np("%1 application", "%1 applications", flatpaks)};
    const QStringList snaps = QDir(path(QStringLiteral("/snap"))).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    packages.items << Item{QStringLiteral("Snap"), QFileInfo::exists(path(QStringLiteral("/snap")))
                                                       ? i18np("%1 package", "%1 packages", snaps.size() - static_cast<int>(snaps.contains(QStringLiteral("bin"))))
                                                       : i18n("Snap not installed")};
    QStringList kernels;
    const QString running = QSysInfo::kernelVersion();
    for (const QString &image : QDir(path(QStringLiteral("/boot"))).entryList({QStringLiteral("vmlinuz-*")}, QDir::Files, QDir::Name)) {
        const QString version = image.mid(8);
        kernels << (version == running ? i18nc("@info kernel version", "%1 (in use)", version) : version);
    }
    packages.items << Item{i18nc("@label system report item", "Installed kernels"), kernels.isEmpty() ? unavailable() : kernels.join(QStringLiteral(", "))};
    m_sections << packages;

    Section boot{QStringLiteral("boot"), i18nc("@title system report section", "Startup"), QStringLiteral("power"), {}};
    boot.items << Item{i18nc("@label system report item", "Uptime"), uptime > 0 ? humanDuration(uptime) : unavailable()};
    boot.items << Item{i18nc("@label system report item", "Last boot"), uptime > 0 ? dateTime(m_generatedAt.addSecs(-uptime)) : unavailable()};
    m_sections << boot;

    // Network: adapters only — no addresses, no network names.
    Section network{QStringLiteral("network"), i18nc("@title system report section", "Network"), QStringLiteral("network"), {}};
    const QString netRoot = path(QStringLiteral("/sys/class/net"));
    for (const QString &interface : QDir(netRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System, QDir::Name)) {
        const QString base = netRoot + QLatin1Char('/') + interface;
        if (interface == QStringLiteral("lo") || !QFileInfo::exists(base + QStringLiteral("/device"))) {
            continue; // loopback and virtual interfaces
        }
        const bool wireless = QFileInfo::exists(base + QStringLiteral("/wireless")) || QFileInfo::exists(base + QStringLiteral("/phy80211"));
        const QString state = readFile(QStringLiteral("/sys/class/net/") + interface + QStringLiteral("/operstate")).trimmed();
        const QString speed = readFile(QStringLiteral("/sys/class/net/") + interface + QStringLiteral("/speed")).trimmed();
        QStringList parts = {wireless ? QStringLiteral("Wi-Fi") : QStringLiteral("Ethernet")};
        const QString driver = readLink(base + QStringLiteral("/device/driver"));
        if (!driver.isEmpty()) {
            parts << i18nc("@info network adapter", "driver %1", driver);
        }
        parts << (state == QStringLiteral("up") ? i18nc("@info network adapter", "connected") : i18nc("@info network adapter", "disconnected"));
        if (state == QStringLiteral("up") && speed.toInt() > 0) {
            parts << QStringLiteral("%1 Mb/s").arg(speed);
        }
        const QString mac = maskMac(readFile(QStringLiteral("/sys/class/net/") + interface + QStringLiteral("/address")));
        if (!mac.isEmpty()) {
            parts << QStringLiteral("MAC %1").arg(mac);
        }
        network.items << Item{publicInterfaceName(interface), parts.join(QStringLiteral(" · "))};
    }
    if (network.items.isEmpty()) {
        network.items << Item{i18nc("@label system report item", "Adapters"), unavailable()};
    }
    m_sections << network;

    m_summary = {
        {QStringLiteral("os"), osName},
        {QStringLiteral("version"), os.value(QStringLiteral("VERSION"))},
        {QStringLiteral("plasma"), plasma.isEmpty() ? QString() : QStringLiteral("KDE Plasma %1").arg(plasma)},
        {QStringLiteral("kernel"), QStringLiteral("Kernel %1").arg(QSysInfo::kernelVersion())},
        {QStringLiteral("architecture"), QSysInfo::WordSize == 64 ? i18nc("@info CPU word size", "64-bit") : i18nc("@info CPU word size", "32-bit")},
        {QStringLiteral("cpu"), cpu.model},
        {QStringLiteral("gpu"), gpuNames.value(0)},
        {QStringLiteral("memory"), memory.totalKiB > 0 ? i18nc("@info memory usage summary", "%1 of %2", humanBytes(usedKiB * 1024), humanBytes(memory.totalKiB * 1024)) : QString()},
        {QStringLiteral("storage"), rootVolume.isValid() ? i18nc("@info storage summary", "%1 free of %2", humanBytes(rootVolume.bytesFree()), humanBytes(rootVolume.bytesTotal())) : QString()},
        {QStringLiteral("uptime"), uptime > 0 ? humanDuration(uptime) : QString()},
        {QStringLiteral("installDate"), installed.isValid() ? date(installed.date()) : QString()},
        {QStringLiteral("installEstimated"), installEstimated}};
    qCInfo(lcReport) << "system report collected";
    emit changed();
}

QString SystemReportService::reportText() const
{
    QStringList lines = {QStringLiteral("Mainuan System Report"), QStringLiteral("====================="),
                         i18nc("@info system report header", "Generated on: %1", dateTime(m_generatedAt)), QString()};
    for (const Section &section : m_sections) {
        lines << QStringLiteral("[%1]").arg(section.title);
        for (const Item &item : section.items) {
            if (item.shared) {
                lines << i18nc("@info system report line: label, value", "%1: %2", item.label, item.value);
            }
        }
        lines << QString();
    }
    lines << i18nc("@info system report footer", "No network addresses, serial numbers or computer name.") << QString();
    return lines.join(QLatin1Char('\n'));
}

bool SystemReportService::copyReport()
{
    if (!ready() || qobject_cast<QGuiApplication *>(QCoreApplication::instance()) == nullptr
        || QGuiApplication::clipboard() == nullptr) {
        return false;
    }
    QGuiApplication::clipboard()->setText(reportText());
    return true;
}

bool SystemReportService::saveReport(const QUrl &file)
{
    if (!ready() || !file.isLocalFile()) {
        return false;
    }
    QSaveFile output(file.toLocalFile());
    const QByteArray text = reportText().toUtf8();
    if (!output.open(QIODevice::WriteOnly) || output.write(text) != text.size() || !output.commit()) {
        qCWarning(lcReport) << "could not save report:" << output.errorString();
        return false;
    }
    return true;
}
