#include "models/ApplicationModel.h"
#include "services/FreeOfficeProfile.h"
#include "services/InstallService.h"
#include "services/PackageService.h"

#include <QtTest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryDir>
#include <QTimer>
#include <cstring>

namespace {
QString fixture(const char *name) { return QStringLiteral(MAINUAN_TEST_FIXTURES "/") + QString::fromLatin1(name); }

void writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(data), data.size());
}

class SimulatedReply final : public QNetworkReply
{
public:
    SimulatedReply(const QNetworkRequest &request, QByteArray data, const QString &fault, QObject *parent)
        : QNetworkReply(parent), m_data(std::move(data))
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::GetOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, fault == "http" ? 404 : fault == "redirect" ? 302 : 200);
        if (fault == "redirect") {
            setAttribute(QNetworkRequest::RedirectionTargetAttribute, QUrl("https://untrusted.invalid/package.deb"));
        }
        setHeader(QNetworkRequest::ContentLengthHeader, m_data.size());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (fault == "network" || fault == "tls") {
            QTimer::singleShot(0, this, [this, fault]() {
                setError(fault == "tls" ? QNetworkReply::SslHandshakeFailedError : QNetworkReply::HostNotFoundError,
                    QStringLiteral("Simulated DNS/TLS failure"));
                setFinished(true);
                emit finished();
            });
        } else {
            QTimer::singleShot(0, this, [this]() { pump(); });
        }
    }
    void abort() override
    {
        if (isFinished()) return;
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("Aborted"));
        setFinished(true);
        emit finished();
    }
    qint64 bytesAvailable() const override { return m_available - m_read + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *data, qint64 maximum) override
    {
        const qint64 count = std::min(maximum, m_available - m_read);
        if (count <= 0) return isFinished() ? -1 : 0;
        std::memcpy(data, m_data.constData() + m_read, static_cast<size_t>(count));
        m_read += count;
        return count;
    }
private:
    void pump()
    {
        if (isFinished()) return;
        m_available = std::min<qint64>(m_data.size(), m_available + 128);
        emit downloadProgress(m_available, m_data.size());
        emit readyRead();
        if (isFinished()) return;
        if (m_available == m_data.size()) {
            setFinished(true);
            emit finished();
        } else {
            QTimer::singleShot(1, this, [this]() { pump(); });
        }
    }
    QByteArray m_data;
    qint64 m_available = 0;
    qint64 m_read = 0;
};

class SimulatedNetwork final : public QNetworkAccessManager
{
public:
    QByteArray payload;
    QString fault;
    QList<QNetworkRequest> requests;
protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *) override
    {
        Q_ASSERT(operation == GetOperation);
        requests.append(request);
        return new SimulatedReply(request, payload, fault, this);
    }
};

QByteArray makeDeb(const QString &directory, const QByteArray &package = "softmaker-freeoffice-2024",
                   const QByteArray &arch = "amd64", const QByteArray &version = "3714")
{
    QDir().mkpath(directory + "/tree/DEBIAN");
    writeFile(directory + "/tree/DEBIAN/control", "Package: " + package + "\nVersion: " + version
        + "\nArchitecture: " + arch + "\nMaintainer: Test <test@example.invalid>\nDescription: isolated fixture\n");
    QProcess builder;
    builder.start(QStringLiteral("/usr/bin/dpkg-deb"), {"--build", "--root-owner-group", directory + "/tree", directory + "/fixture.deb"});
    if (!builder.waitForFinished(10000) || builder.exitCode() != 0) return {};
    QFile file(directory + "/fixture.deb");
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

InstallService::Backend backend(SimulatedNetwork *network)
{
    InstallService::Backend result{fixture("pkexec"), fixture("helper"), fixture("dpkg-query"), QStringLiteral("/usr/bin/true")};
    result.network = network;
    result.freeOffice.size = network->payload.size();
    result.freeOffice.sha256 = QCryptographicHash::hash(network->payload, QCryptographicHash::Sha256).toHex();
    return result;
}

bool detected(InstallService &installer)
{
    return QTest::qWaitFor([&]() {
        return installer.profiles().value("freeoffice").toMap().value("state") != "checking";
    });
}
}

class AppInstallationTest final : public QObject
{
    Q_OBJECT
private slots:
    void init()
    {
        m_state = std::make_unique<QTemporaryDir>();
        writeFile(m_state->filePath("deb"), {});
        writeFile(m_state->filePath("flatpak"), {});
        qputenv("MOCK_STATE_DIR", m_state->path().toUtf8());
    }
    void cleanup()
    {
        for (const char *key : {"MOCK_STATE_DIR", "MOCK_SCENARIO", "MOCK_DELAY", "MOCK_FLATHUB_SCOPE"}) qunsetenv(key);
        m_state.reset();
    }
    void pinnedOfficialProfile()
    {
        const auto profile = FreeOfficeProfile::official();
        QVERIFY(profile.isValid());
        QCOMPARE(profile.package, QStringLiteral("softmaker-freeoffice-2024"));
        QCOMPARE(profile.version, QStringLiteral("3714"));
        QCOMPARE(profile.architecture, QStringLiteral("amd64"));
        QCOMPARE(profile.size, 167839604);
        QVERIFY(profile.matchesMetadata("softmaker-freeoffice-2024\n3714\namd64\n"));
        for (const QByteArray &metadata : {QByteArray("commercial-office\n3714\namd64\n"),
            QByteArray("softmaker-freeoffice-2024\n3714\narm64\n"), QByteArray("softmaker-freeoffice-2024\n3715\namd64\n")}) {
            QVERIFY(!profile.matchesMetadata(metadata));
        }
        for (const QString &url : {"http://www.softmaker.net/down/softmaker-freeoffice-2024_1238-01_amd64.deb",
            "https://untrusted.invalid/file.deb", "https://www.softmaker.net/other.deb",
            "file:///tmp/file.deb", "https://www.softmaker.net/down/softmaker-freeoffice-2024_1238-01_amd64.deb?x=1"}) {
            QVERIFY(!profile.acceptsUrl(QUrl(url)));
        }
    }
    void profileSources()
    {
        PackageService packages(nullptr, fixture("flatpak"));
        InstallService installer(&packages, backendWithoutNetwork());
        QCOMPARE(installer.profileInfo("freeoffice").value("source").toString(), QStringLiteral("SoftMaker"));
        QCOMPARE(installer.profileInfo("firewall").value("source").toString(), QStringLiteral("Repositórios configurados"));
        QCOMPARE(installer.profileInfo("codecs").value("source").toString(), QStringLiteral("Repositórios configurados"));
        QCOMPARE(installer.profileInfo("antivirus").value("source").toString(), QStringLiteral("Repositórios configurados / Flathub"));
        for (const QString &id : {"com.vivaldi.Vivaldi", "app.zen_browser.zen", "com.brave.Browser", "com.google.Chrome",
             "io.gitlab.librewolf-community", "org.libreoffice.LibreOffice", "com.wps.Office", "com.microsoft.Edge",
             "com.opera.opera-gx", "org.torproject.torbrowser-launcher", "io.github.linx_systems.ClamUI"}) {
            QVERIFY(installer.registerApplication(id, id, {}, {}));
            QCOMPARE(installer.profileInfo("app:" + id).value("source").toString(), QStringLiteral("Flathub"));
        }
        QVERIFY(installer.profileInfo("untrusted").isEmpty());
        QVERIFY(detected(installer));
    }
    void browsersUseInternalFlatpak_data()
    {
        QTest::addColumn<QString>("id");
        QTest::newRow("vivaldi") << QStringLiteral("com.vivaldi.Vivaldi");
        QTest::newRow("zen") << QStringLiteral("app.zen_browser.zen");
    }
    void browsersUseInternalFlatpak()
    {
        QFETCH(QString, id);
        qputenv("MOCK_DELAY", "0.005");
        qputenv("MOCK_FLATHUB_SCOPE", "user");
        PackageService packages(nullptr, fixture("flatpak"));
        InstallService installer(&packages, backendWithoutNetwork());
        ApplicationModel model(&packages, {{id, id, {}, id, "qrc:/assets/vivaldi.png", {}}});
        model.setInstallService(&installer);
        QVERIFY(PackageService::isAllowedApplication(id));
        QVERIFY(!installer.registerApplication(id, id, {}, {}));
        QVERIFY(detected(installer));
        QSignalSpy done(&installer, &InstallService::finished);
        const QString profile = "app:" + id;
        QVERIFY(installer.start(profile));
        QVERIFY(!installer.start(profile));
        QVERIFY(model.data(model.index(0), ApplicationModel::BusyRole).toBool());
        QCOMPARE(model.data(model.index(0), ApplicationModel::InstallStateRole).toString(), QStringLiteral("Instalando…"));
        QVERIFY(done.wait(10000));
        QCOMPARE(installer.phase(), QStringLiteral("success"));
        QTRY_VERIFY(model.data(model.index(0), ApplicationModel::InstalledRole).toBool());
        QCOMPARE(packages.installation(id), QStringLiteral("user"));
        QVERIFY(installer.launch(profile));
        QSignalSpy removed(&packages, &PackageService::operationFinished);
        QVERIFY(model.remove(0));
        QVERIFY(removed.wait(5000));
        QTRY_VERIFY(!packages.isInstalled(id));
    }
    void freeOfficeFlow_data()
    {
        QTest::addColumn<QString>("fault");
        QTest::addColumn<QString>("result");
        for (const QString &name : {"success", "network", "tls", "http", "redirect", "corrupt", "short", "oversize",
             "package", "architecture", "version", "not-deb", "cancel", "auth", "helper-noop", "apt-error"}) {
            QTest::newRow(qPrintable(name)) << name << (name == "success" ? QStringLiteral("success")
                : name == "cancel" ? QStringLiteral("cancelled") : QStringLiteral("error"));
        }
    }
    void freeOfficeFlow()
    {
        QFETCH(QString, fault);
        QFETCH(QString, result);
        QTemporaryDir directory;
        SimulatedNetwork network;
        network.payload = makeDeb(directory.path(), fault == "package" ? "commercial-office" : "softmaker-freeoffice-2024",
            fault == "architecture" ? "arm64" : "amd64", fault == "version" ? "3715" : "3714");
        QVERIFY(!network.payload.isEmpty());
        if (fault == "not-deb") network.payload = "not a debian archive";
        auto mock = backend(&network);
        if (fault == "corrupt") network.payload[network.payload.size() / 2] ^= 1;
        if (fault == "short") network.payload.chop(1);
        if (fault == "oversize") network.payload.append('x');
        network.fault = fault;
        if (fault == "cancel" || fault == "auth" || fault == "helper-noop" || fault == "apt-error") qputenv("MOCK_SCENARIO", fault.toUtf8());
        PackageService packages(nullptr, QStringLiteral("/nonexistent/flatpak"));
        InstallService installer(&packages, mock);
        ApplicationModel model(&packages, {{"freeoffice", "FreeOffice", {}, "softmaker-freeoffice-2024", "qrc:/assets/freeOffice.png", {}, ApplicationAction::InternalDebian}});
        model.setInstallService(&installer);
        QVERIFY(detected(installer));
        QVERIFY(model.data(model.index(0), ApplicationModel::AvailableRole).toBool());
        bool responsive = false;
        QStringList phases;
        QList<int> downloadProgress;
        connect(&installer, &InstallService::stateChanged, this, [&]() {
            phases << installer.phase();
            if (installer.phase() == "downloading") downloadProgress << installer.percent();
        });
        QSignalSpy done(&installer, &InstallService::finished);
        QVERIFY(installer.start("freeoffice"));
        QVERIFY(!installer.start("freeoffice"));
        QTimer::singleShot(0, [&]() { responsive = true; });
        QVERIFY(done.wait(10000));
        QVERIFY(responsive);
        QCOMPARE(installer.phase(), result);
        QCOMPARE(network.requests.size(), 1);
        QCOMPARE(network.requests.first().attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(), int(QNetworkRequest::ManualRedirectPolicy));
        QVERIFY(!model.remove(0));
        if (fault == "success") {
            QVERIFY(phases.indexOf("downloading") < phases.indexOf("authenticating"));
            QVERIFY(std::any_of(downloadProgress.begin(), downloadProgress.end(), [](int v) { return v > 0 && v < 100; }));
            QVERIFY(model.data(model.index(0), ApplicationModel::InstalledRole).toBool());
            QVERIFY(installer.profileInfo("freeoffice").value("sourceNotice").toString().contains("scripts"));
            QSignalSpy again(&installer, &InstallService::finished);
            QVERIFY(installer.start("freeoffice"));
            QVERIFY(again.wait(5000));
            QCOMPARE(network.requests.size(), 1); // Installed detection avoids another download/authentication.
        } else if (fault != "cancel" && fault != "auth" && fault != "helper-noop" && fault != "apt-error") {
            QVERIFY(!phases.contains("authenticating"));
        }
        QVERIFY(!PackageService::isAllowedApplication("softmaker-freeoffice-2024"));
    }
    void browserFailures_data()
    {
        QTest::addColumn<QByteArray>("scenario");
        QTest::newRow("network") << QByteArray("flatpak-network");
        QTest::newRow("denied") << QByteArray("flatpak-denied");
        QTest::newRow("false-success") << QByteArray("flatpak-noop");
    }
    void browserFailures()
    {
        QFETCH(QByteArray, scenario);
        qputenv("MOCK_SCENARIO", scenario);
        PackageService packages(nullptr, fixture("flatpak"));
        InstallService installer(&packages, backendWithoutNetwork());
        ApplicationModel model(&packages, {{"zen", "Zen", {}, "app.zen_browser.zen", {}, {}}});
        model.setInstallService(&installer);
        QVERIFY(detected(installer));
        QSignalSpy done(&installer, &InstallService::finished);
        QVERIFY(installer.start("app:app.zen_browser.zen"));
        QVERIFY(done.wait(10000));
        QVERIFY(installer.phase() == "error" || installer.phase() == "cancelled");
        QVERIFY(!model.data(model.index(0), ApplicationModel::InstalledRole).toBool());
        QCOMPARE(model.data(model.index(0), ApplicationModel::InstallStateRole).toString(), QStringLiteral("Erro"));
        qunsetenv("MOCK_SCENARIO");
        QVERIFY(installer.start("app:app.zen_browser.zen"));
        QVERIFY(done.wait(10000));
        QCOMPARE(installer.phase(), QStringLiteral("success"));
    }
    void privilegedValidatorRejectsTampering()
    {
        QTemporaryDir directory;
        const QByteArray data = makeDeb(directory.path());
        QVERIFY(!data.isEmpty());
        const auto validate = [&](const QString &path, const QByteArray &digest, const QString &package, const QString &arch) {
            QProcess process;
            process.start("/bin/bash", {"-c", "source \"$1\"; validate_freeoffice \"$2\" \"$3\" \"$4\" \"$5\" 3714 \"$6\"", "validator-test",
                QStringLiteral(MAINUAN_TEST_HELPER), path, QString::number(data.size()), QString::fromLatin1(digest), package, arch});
            if (!process.waitForFinished(10000)) return -1;
            return process.exitCode();
        };
        const auto digest = QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
        const QString path = directory.filePath("fixture.deb");
        QCOMPARE(validate(path, digest, "softmaker-freeoffice-2024", "amd64"), 0);
        QVERIFY(validate(path, digest, "arbitrary-package", "amd64") != 0);
        QVERIFY(validate(path, digest, "softmaker-freeoffice-2024", "arm64") != 0);
        QVERIFY(validate(path, QByteArray(64, '0'), "softmaker-freeoffice-2024", "amd64") != 0);
        QVERIFY(QFile::link(path, directory.filePath("symlink.deb")));
        QVERIFY(validate(directory.filePath("symlink.deb"), digest, "softmaker-freeoffice-2024", "amd64") != 0);
        writeFile(path, data.left(data.size() - 1));
        QVERIFY(validate(path, digest, "softmaker-freeoffice-2024", "amd64") != 0);
        for (const QStringList &arguments : {QStringList{"install", "freeoffice", "/tmp/arbitrary.deb"},
             QStringList{"install", "https://untrusted.invalid/file.deb"}, QStringList{"install", "arbitrary-package"}}) {
            QProcess process;
            process.start(QStringLiteral(MAINUAN_TEST_HELPER), arguments);
            QVERIFY(process.waitForFinished());
            QCOMPARE(process.exitCode(), 2);
        }
    }
    void refusesUnauthorizedSourceBeforeAuthentication()
    {
        SimulatedNetwork network;
        network.payload = "unused";
        auto mock = backend(&network);
        mock.freeOffice.url = QUrl(QStringLiteral("https://untrusted.invalid/file.deb"));
        PackageService packages(nullptr, QStringLiteral("/nonexistent/flatpak"));
        InstallService installer(&packages, mock);
        QVERIFY(detected(installer));
        QSignalSpy done(&installer, &InstallService::finished);
        QVERIFY(installer.start("freeoffice"));
        QVERIFY(done.wait(5000));
        QCOMPARE(installer.phase(), QStringLiteral("error"));
        QVERIFY(network.requests.isEmpty());
        QVERIFY(!installer.start("install:/tmp/arbitrary.deb"));
        QVERIFY(!installer.start("https://untrusted.invalid/file.deb"));
    }
private:
    static InstallService::Backend backendWithoutNetwork()
    {
        return {fixture("pkexec"), fixture("helper"), fixture("dpkg-query"), QStringLiteral("/usr/bin/true")};
    }
    std::unique_ptr<QTemporaryDir> m_state;
};

QTEST_GUILESS_MAIN(AppInstallationTest)
#include "tst_appinstallation.moc"
