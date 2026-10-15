#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

// Collects the information shown in Sobre and in "Relatório da máquina".
// Files under /proc, /sys and /etc are read directly and the Plasma version
// comes from D-Bus; lspci, lsblk, glxinfo and dpkg-query run asynchronously
// with a timeout and are optional. Nothing sensitive is collected: no addresses, serial numbers,
// user names or network names, and MAC addresses are masked.
class SystemReportService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList sections READ sections NOTIFY changed)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)

public:
    struct Item
    {
        QString label;
        QString value;
        bool shared = true; // false: shown in the Welcome, left out of the copied/saved text
    };
    struct Section
    {
        QString id;
        QString title;
        QString icon; // qml/icons file name
        QList<Item> items;
    };
    struct Cpu
    {
        QString model;
        int cores = 0;
        int threads = 0;
    };
    struct Memory
    {
        qint64 totalKiB = 0;
        qint64 availableKiB = 0;
        qint64 swapTotalKiB = 0;
        qint64 swapFreeKiB = 0;
    };

    // `root` prefixes /proc, /sys, /etc, /var and /boot (tests only).
    explicit SystemReportService(QObject *parent = nullptr, const QString &root = {});

    static QHash<QString, QString> parseOsRelease(const QString &text);
    static Cpu parseCpuInfo(const QString &text);
    static Memory parseMemInfo(const QString &text);
    static int countInstalledPackages(const QString &dpkgStatus);
    static QString maskMac(const QString &mac);
    // System mount points as they are; anything else (removable media, user
    // mounts) returns an empty string because the path names the user or volume.
    static QString publicMountPoint(const QString &mountPoint);
    // Interface names derived from the MAC (enx…, wlx…) with the device part masked.
    static QString publicInterfaceName(const QString &interface);
    static QString humanBytes(qint64 bytes);
    static QString humanDuration(qint64 seconds);
    // "00:02.0" → "Vendor Device" from `lspci -mm`.
    static QHash<QString, QString> parseLspci(const QString &text);

    QVariantList sections() const;
    QVariantMap summary() const;
    bool busy() const;
    bool ready() const;

    QString reportText() const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool copyReport();
    Q_INVOKABLE bool saveReport(const QUrl &file);

signals:
    void changed();
    void busyChanged();

private:
    QString path(const QString &absolute) const;
    QString readFile(const QString &absolute) const;
    void runTool(const QString &key, const QString &program, const QStringList &arguments);
    void toolFinished();
    void build();

    QString m_root;
    QList<Section> m_sections;
    QVariantMap m_summary;
    QHash<QString, QString> m_toolOutput;
    QDateTime m_generatedAt;
    int m_pendingTools = 0;
    bool m_busy = false;
};
