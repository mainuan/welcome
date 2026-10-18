#include "models/LayoutModel.h"
#include "services/LayoutService.h"

#include <QFile>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

namespace {
const QStringList ids = {"plasma-default", "three-panels", "single-bar", "tiling", "panel-top", "unity", "minimal"};
const QStringList names = {"Clássico — 5 barras", "3 barras", "Barra única", "Tiling", "Painel superior", "Lomiri", "Barra W7"};
const QStringList widgets = {"org.kde.plasma.kickoff", "org.kde.plasma.kicker", "Orbit.Start.Menu", "TahoeLauncher",
    "org.kde.plasma.icontasks", "org.kde.plasma.systemtray", "org.kde.plasma.digitalclock", "org.kde.plasma.panelspacer",
    "org.kde.plasma.marginsseparator", "org.kde.plasma.appmenu", "org.kde.plasma.pager", "org.kde.plasma.trash",
    "org.kde.plasma.powershowdesktop", "org.kde.plasma.showdesktop", "Chaac.Complete.Weather", "com.mike.desktop",
    "org.kde.milou", "ChatAI-Plasmoid", "KdeControlStation"};
QString json(const QStringList &values) { return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(values)).toJson(QJsonDocument::Compact)); }
QByteArray read(const QString &path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
void write(const QString &path, const QByteArray &bytes) {
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); QCOMPARE(file.write(bytes), bytes.size());
}

class PlasmaMock {
public:
    QJSEngine engine;
    PlasmaMock() {
        const QByteArray script = read(QStringLiteral(MAINUAN_TEST_FIXTURES "/plasma-layout-mock.js"));
        if (script.isEmpty() || engine.evaluate(QString::fromUtf8(script)).isError()) qFatal("Cannot load the Plasma mock");
        reset();
    }
    QJSValue run(const QString &script) {
        const auto result = engine.evaluate(script);
        if (result.isError()) qFatal("Plasma mock error: %s at line %d", qPrintable(result.toString()), result.property("lineNumber").toInt());
        return result;
    }
    void reset(const QString &screens = "[{width:1920,height:1080}]") {
        run("mockReset(" + screens + "); knownWidgetTypes=" + json(widgets) + ";");
    }
    QString output() { return run("mockOutput").toString().trimmed(); }
    LayoutService::Description description() {
        run("mockOutput='';describeMainuanLayout();");
        return LayoutService::parseDescription(output());
    }
    void apply(const QString &id, const QStringList &installed = {}) { run(LayoutService::buildScript(id, installed)); }
};

// Ensure the C++ plugin discovery cannot see the real user's plasmoids.
class IsolatedData {
public:
    QTemporaryDir root;
    IsolatedData() {
        for (const char *name : {"XDG_DATA_HOME", "XDG_DATA_DIRS"}) {
            previous.insert(name, {qEnvironmentVariableIsSet(name), qgetenv(name)});
            qputenv(name, root.path().toUtf8());
        }
    }
    ~IsolatedData() {
        for (auto it = previous.cbegin(); it != previous.cend(); ++it) {
            if (it.value().first) qputenv(it.key(), it.value().second); else qunsetenv(it.key());
        }
    }
private:
    QMap<QByteArray, QPair<bool, QByteArray>> previous;
};
}

class LayoutTest final : public QObject {
    Q_OBJECT
private slots:
    void modelOrderAndNames();
    void createsAndIdentifiesAllLayouts();
    void preservesReferenceWidgetsAndSettings();
    void adaptsToScreens();
    void correctsClassicSegmentsAndLengthModes();
    void classicSpacing_data();
    void classicSpacing();
    void classicTrayBounds_data();
    void classicTrayBounds();
    void normalizesSupportedIconSettings();
    void preservesLegacyFloatingWithoutApplyingIt();
    void checksRequiredAndOptionalWidgets();
    void rejectsIncompleteLayouts();
    void sequencesBackupApplicationAndRollback();
};

void LayoutTest::modelOrderAndNames() {
    QCOMPARE(LayoutService::layoutIds(), ids);
    LayoutModel model;
    QCOMPARE(model.rowCount(), 7);
    QCOMPARE(model.roleNames().value(LayoutModel::LayoutIdRole), QByteArray("layoutId"));
    for (int row = 0; row < ids.size(); ++row) {
        QCOMPARE(LayoutService::displayName(ids[row]), names[row]);
        QCOMPARE(model.data(model.index(row), LayoutModel::LayoutIdRole).toString(), ids[row]);
        QCOMPARE(model.data(model.index(row), LayoutModel::NameRole).toString(), names[row]);
        QVERIFY(!model.data(model.index(row), LayoutModel::DescriptionRole).toString().isEmpty());
    }
}

void LayoutTest::createsAndIdentifiesAllLayouts() {
    PlasmaMock plasma;
    const QList<int> counts = {5, 3, 1, 1, 1, 2, 1};
    for (int screens : {1, 2}) {
        for (int i = 0; i < ids.size(); ++i) {
            plasma.reset(screens == 1 ? "[{width:1920,height:1080}]" : "[{width:1366,height:768},{width:1280,height:720}]");
            plasma.apply(ids[i]); // The KWin service is deliberately never invoked.
            QCOMPARE(plasma.run("mockRemoved").toInt(), 1);
            QCOMPARE(plasma.run("panels().length").toInt(), counts[i] * screens);
            QVERIFY2(LayoutService::matchesLayout(plasma.description(), ids[i]), qPrintable(ids[i] + ": " + plasma.output()));
            QVERIFY(plasma.run("panels().every(function(p){return p.widgets().every(function(w){return w.currentConfigGroup.length===0;});})").toBool());
        }
    }
}

void LayoutTest::preservesReferenceWidgetsAndSettings() {
    PlasmaMock plasma;
    for (const QString &id : {QString("three-panels"), QString("single-bar"), QString("panel-top"), QString("unity"), QString("minimal")}) {
        plasma.reset(); plasma.apply(id);
        // Every sanitized reference widget appears in order; config values survive the API conversion.
        QVERIFY(plasma.run("panels().every(function(p,i){return p.widgets().map(function(w){return w.type;}).join('|') === REFERENCE_LAYOUTS['" + id + "'][i].widgets.map(function(w){return w.type;}).join('|');})").toBool());
        QVERIFY(plasma.run("panels().every(function(p){return p.widgets().filter(function(w){return w.type==='org.kde.plasma.icontasks';}).every(function(w){return w.config['General/launchers'].indexOf('preferred://filemanager')>=0 && w.config['General/launchers'].indexOf('applications:firefox.desktop')>=0;});})").toBool());
    }
    plasma.reset(); plasma.apply("three-panels");
    QCOMPARE(plasma.run("panels()[0].items.map(function(w){return w.type;}).join('|')").toString(), QString("org.kde.plasma.kickoff|Chaac.Complete.Weather|org.kde.milou|com.mike.desktop"));
    QCOMPARE(plasma.run("panels()[1].items[0].config['General/launchers'][1]").toString(), QString("applications:org.jitsi.jitsi-meet.desktop"));
    QCOMPARE(plasma.run("panels()[2].items[2].config['Appearance/scale']").toInt(), 95);
    plasma.reset(); plasma.apply("single-bar");
    QVERIFY(plasma.run("panels()[0].floating && panels()[0].lengthMode==='custom'").toBool());
    plasma.reset(); plasma.apply("panel-top");
    QVERIFY(plasma.run("panels()[0].items.some(function(w){return w.type==='TahoeLauncher';})").toBool());
    plasma.reset(); plasma.apply("unity");
    QVERIFY(plasma.run("panels()[1].items.some(function(w){return w.type==='Orbit.Start.Menu' && w.config['General/appIconSize']===undefined;})").toBool());
    plasma.reset(); plasma.apply("minimal");
    QVERIFY(plasma.run("panels()[0].location==='bottom' && !panels()[0].floating && panels()[0].lengthMode==='fill' && panels()[0].hiding==='none'").toBool());
    const QString script = LayoutService::buildScript("unity", {});
    for (const QString &privateValue : {QString("ScreenMapping"), QString("/home/"), QString("activityId"), QString("AppletOrder"), QString("PreloadWeight"), QString("file:///var/lib/flatpak/")})
        QVERIFY2(!script.contains(privateValue), qPrintable(privateValue));
}

void LayoutTest::adaptsToScreens() {
    PlasmaMock plasma;
    for (const QSize &size : {QSize(1280,720), QSize(1366,768), QSize(1920,1080)}) {
        for (const QString &id : ids) {
            plasma.reset(QString("[{width:%1,height:%2},{width:1280,height:720}]").arg(size.width()).arg(size.height()));
            plasma.apply(id);
            QVERIFY2(LayoutService::matchesLayout(plasma.description(), id), qPrintable(id));
            QVERIFY(plasma.run("panels().every(function(p){return p.height===40 && (p.lengthMode==='custom' || p.writes.length===0);})").toBool());
            if (id == "three-panels" || id == "plasma-default") {
                QVERIFY2(plasma.run("positionMainuanLayout('" + id + "').ok").toBool(), qPrintable(id));
                // A second set of intrinsic icon widths still fits; lengths are measured, never imposed.
                plasma.run("mockIconFactor=1.2;");
                QVERIFY(plasma.run("positionMainuanLayout('" + id + "').ok").toBool());
                if (id == "plasma-default") {
                    QVERIFY(plasma.run("[0,1].every(function(s){var p=panels().filter(function(p){return p.screen===s;});var w=screenGeometry(s).width,l=(w-p[2].length)/2,i=p[1],r=p[3];return i.offset>p[0].length && i.offset+i.maximumLength<l && r.offset>p[4].length && w-r.offset-r.maximumLength>(w+p[2].length)/2;})").toBool());
                }
            }
            if (id == "single-bar" || id == "panel-top") {
                QVERIFY(plasma.run("panels().every(function(p){return p.lengthMode==='custom' && p.maximumLength<screenGeometry(p.screen).width && p.maximumLength===p.minimumLength;})").toBool());
            }
            if (id == "unity") QVERIFY(plasma.run("panels().every(function(p){return p.lengthMode==='fill' && p.height===40 && !p.floating && p.writes.length===0;})").toBool());
        }
    }
    plasma.reset(); plasma.apply("three-panels");
    plasma.run("mockIconFactor=10;");
    QVERIFY(!plasma.run("positionMainuanLayout('three-panels').ok").toBool());
}

void LayoutTest::correctsClassicSegmentsAndLengthModes() {
    PlasmaMock plasma;
    plasma.apply("plasma-default");
    QCOMPARE(plasma.run("panels().length").toInt(), 5);
    QCOMPARE(plasma.run("panels()[0].items.map(function(w){return w.type;}).join('|')").toString(), QString("org.kde.plasma.marginsseparator|org.kde.plasma.kickoff"));
    QCOMPARE(plasma.run("panels()[1].items.map(function(w){return w.type;}).join('|')").toString(), QString("Chaac.Complete.Weather|com.mike.desktop"));
    QCOMPARE(plasma.run("panels()[3].items.map(function(w){return w.type;}).join('|')").toString(), QString("ChatAI-Plasmoid|org.kde.plasma.systemtray|KdeControlStation"));
    QCOMPARE(plasma.run("panels()[4].items.map(function(w){return w.type;}).join('|')").toString(), QString("org.kde.plasma.digitalclock|org.kde.plasma.powershowdesktop"));
    QVERIFY(plasma.run("panels()[3].lengthMode==='custom' && panels()[3].minimumLength<panels()[3].maximumLength").toBool());
    QVERIFY(!LayoutService::buildScript("plasma-default", {}).contains("org.kde.plasma.kdeaichat"));
    for (const QString &missing : {QString("Chaac.Complete.Weather"), QString("com.mike.desktop"), QString("ChatAI-Plasmoid"), QString("KdeControlStation")}) {
        plasma.reset(); plasma.run("knownWidgetTypes=knownWidgetTypes.filter(function(t){return t!=='"+missing+"';});");
        plasma.apply("plasma-default");
        QCOMPARE(plasma.run("mockRemoved").toInt(), 0);
        QVERIFY(plasma.output().contains(missing));
    }
    plasma.reset(); plasma.apply("plasma-default"); plasma.run("panels()[1].remove();");
    QVERIFY(!LayoutService::matchesLayout(plasma.description(), "plasma-default"));
    plasma.reset(); plasma.apply("plasma-default"); plasma.run("panels()[3].items.pop();");
    QVERIFY(!LayoutService::matchesLayout(plasma.description(), "plasma-default"));
    plasma.reset(); plasma.apply("three-panels");
    QVERIFY(plasma.run("panels().every(function(p){return p.lengthMode==='fit' && p.floating && p.writes.length===0 && p.items.every(function(w){return w.type!=='org.kde.plasma.panelspacer' || w.config['General/expanding']===false;});})").toBool());
}

void LayoutTest::classicSpacing_data() {
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");
    QTest::newRow("1280x720") << 1280 << 720;
    QTest::newRow("1366x768-manual-reference") << 1366 << 768;
    QTest::newRow("1920x1080") << 1920 << 1080;
}

void LayoutTest::classicSpacing() {
    QFETCH(int, width);
    QFETCH(int, height);
    PlasmaMock plasma;
    plasma.reset(QString("[{width:%1,height:%2},{width:1366,height:768}]").arg(width).arg(height));
    plasma.apply("plasma-default");
    // Simulated measured footprints compatible with the manual capture.
    // No source snapshot specifies the physical widths of the fit panels.
    plasma.run("mockObservedLengths={launcher:48,clock:95,tasks:240};panels().forEach(function(p){var role=p.readConfig('MainuanPanelRole','');if(mockObservedLengths[role]!==undefined)Object.defineProperty(p,'length',{get:function(){return mockObservedLengths[role];}});});");
    const QString originalWidgets = plasma.run("JSON.stringify(panels().map(function(p){return p.items.map(function(w){return {type:w.type,config:w.config};});}))").toString();
    for (int sample = 0; sample < 3; ++sample) {
        QVERIFY(plasma.run("positionMainuanLayout('plasma-default').ok").toBool());
        for (int screen : {0, 1}) {
            const int base = screen * 5;
            QCOMPARE(plasma.run(QString("panels()[%1].offset").arg(base + 1)).toInt(), 65);
            QCOMPARE(plasma.run(QString("panels()[%1].offset").arg(base + 3)).toInt(), 111);
            QCOMPARE(plasma.run(QString("panels()[%1].minimumLength").arg(base + 1)).toInt(), 217);
            QCOMPARE(plasma.run(QString("panels()[%1].maximumLength").arg(base + 1)).toInt(), 221);
            QCOMPARE(plasma.run(QString("panels()[%1].minimumLength").arg(base + 3)).toInt(), 203);
            QCOMPARE(plasma.run(QString("panels()[%1].maximumLength").arg(base + 3)).toInt(), 273);
        }
        QCOMPARE(plasma.run("panels().slice(0,5).map(function(p){return p.lengthMode;}).join('|')").toString(), QString("fit|custom|fit|custom|fit"));
        QCOMPARE(plasma.run("panels().slice(0,5).map(function(p){return p.alignment;}).join('|')").toString(), QString("left|left|center|right|right"));
        QVERIFY(plasma.run("panels().every(function(p){return p.height===40 && p.floating && (p.lengthMode==='custom' || p.writes.length===0);})").toBool());
        // Use the maximum custom extent and a 16-pixel allowance at the
        // neighboring fit panels to model floating theme margins conservatively.
        QVERIFY(plasma.run("[0,1].every(function(s){var p=panels().filter(function(p){return p.screen===s;});var w=screenGeometry(s).width;return p[0].length+16<=p[1].offset && p[1].offset+p[1].maximumLength+16<=(w-p[2].length)/2 && (w+p[2].length)/2+16<=w-p[3].offset-p[3].maximumLength && p[4].length+16<=p[3].offset;})").toBool());
    }
    // A wider clock or launcher moves only the adjacent custom segment, by
    // the minimum necessary amount rather than distributing the empty space.
    plasma.run("mockObservedLengths.launcher=90;mockObservedLengths.clock=170;");
    QVERIFY(plasma.run("positionMainuanLayout('plasma-default').ok").toBool());
    QCOMPARE(plasma.run("panels()[1].offset").toInt(), 106);
    QCOMPARE(plasma.run("panels()[3].offset").toInt(), 186);
    QCOMPARE(plasma.run("panels()[1].maximumLength").toInt(), 221);
    QCOMPARE(plasma.run("panels()[3].maximumLength").toInt(), 273);
    QCOMPARE(plasma.run("JSON.stringify(panels().map(function(p){return p.items.map(function(w){return {type:w.type,config:w.config};});}))").toString(), originalWidgets);
    QVERIFY(LayoutService::matchesLayout(plasma.description(), "plasma-default"));
    plasma.run("mockObservedLengths.tasks=1100;");
    QVERIFY(!plasma.run("positionMainuanLayout('plasma-default').ok").toBool());
}

void LayoutTest::classicTrayBounds_data() {
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("maximum");
    QTest::newRow("1280-manual-range") << 1280 << 273;
    QTest::newRow("1323-manual-range") << 1323 << 273;
    QTest::newRow("1366-reference-range") << 1366 << 273;
    QTest::newRow("1920-keeps-compact-range") << 1920 << 273;
    QTest::newRow("2560-does-not-grow-proportionally") << 2560 << 273;
}

void LayoutTest::classicTrayBounds() {
    QFETCH(int, width);
    QFETCH(int, maximum);
    PlasmaMock plasma;
    plasma.reset(QString("[{width:%1,height:1080},{width:1280,height:720}]").arg(width));
    plasma.apply("plasma-default");
    QCOMPARE(plasma.run("panels()[3].minimumLength").toInt(), 203);
    QCOMPARE(plasma.run("panels()[3].maximumLength").toInt(), maximum);
    QCOMPARE(plasma.run("panels()[8].minimumLength").toInt(), 203);
    QCOMPARE(plasma.run("panels()[8].maximumLength").toInt(), 273);
    const QString widgetsBefore = plasma.run("JSON.stringify(panels()[3].items.map(function(w){return {type:w.type,config:w.config};}))").toString();
    for (double iconFactor : {1.0, 1.2}) {
        plasma.run(QString("mockIconFactor=%1;").arg(iconFactor));
        for (int sample = 0; sample < 3; ++sample) {
            QVERIFY(plasma.run("positionMainuanLayout('plasma-default').ok").toBool());
            QCOMPARE(plasma.run("panels()[3].minimumLength").toInt(), 203);
            QCOMPARE(plasma.run("panels()[3].maximumLength").toInt(), maximum);
            QVERIFY(plasma.run("panels()[3].lengthMode==='custom' && panels()[3].height===40 && panels()[3].floating && panels()[3].alignment==='right'").toBool());
            QVERIFY(plasma.run("[0,1].every(function(s){var p=panels().filter(function(p){return p.screen===s;});var w=screenGeometry(s).width,t=p[3];var left=w-t.offset-t.maximumLength;return left>=(w+p[2].length)/2+16 && t.offset>=p[4].length+16;})").toBool());
        }
    }
    QCOMPARE(plasma.run("JSON.stringify(panels()[3].items.map(function(w){return {type:w.type,config:w.config};}))").toString(), widgetsBefore);
    QVERIFY(LayoutService::matchesLayout(plasma.description(), "plasma-default"));
    // Real Plasma exposes allocated widget geometry, while panel.length may be
    // an unbounded preferred size. Refuse observable overflow instead of clipping.
    plasma.run("panels()[3].items[0].geometry={width:32};panels()[3].items[1].geometry={width:160};panels()[3].items[2].geometry={width:24};");
    QVERIFY(plasma.run("positionMainuanLayout('plasma-default').ok").toBool());
    plasma.run(QString("panels()[3].items[1].geometry.width=%1;").arg(maximum + 1));
    QVERIFY(!plasma.run("positionMainuanLayout('plasma-default').ok").toBool());
}

void LayoutTest::normalizesSupportedIconSettings() {
    PlasmaMock plasma;
    for (const QString &id : ids) {
        plasma.reset(); plasma.apply(id);
        QVERIFY(plasma.run("panels().every(function(p){return p.items.every(function(w){if(w.type==='org.kde.plasma.systemtray')return w.config['General/scaleIconsToFit']===true && w.config['General/iconSpacing']===1;if(w.type==='org.kde.plasma.icontasks')return w.config['General/maxStripes']===1 && w.config['General/iconSpacing']===1 && (p.lengthMode!=='fit' || w.config['General/fill']===false);return w.config['General/iconSize']===undefined;});})").toBool());
    }
}

void LayoutTest::preservesLegacyFloatingWithoutApplyingIt() {
    IsolatedData isolated;
    PlasmaMock plasma;
    plasma.apply("single-bar");
    plasma.run("panels()[0].writeConfig('MainuanLayout','floating');panels()[0].height=52;");
    QVERIFY(!LayoutService::isAllowedLayout("floating"));
    QVERIFY(LayoutService::buildScript("floating", {}).isEmpty());
    QVERIFY(LayoutService::matchesLayout(plasma.description(), "floating"));
    const int removed = plasma.run("mockRemoved").toInt();
    LayoutService::Backend backend;
    backend.evaluate = [&](const QString &script, auto done) { plasma.run("mockOutput='';"); plasma.run(script); done(true, plasma.output()); };
    QTemporaryDir config, backups;
    LayoutService service(nullptr, config.path(), backups.path(), backend);
    service.refresh();
    QCOMPARE(service.activeLayout(), QString("floating"));
    service.apply("floating");
    QVERIFY(service.messageIsError()); QVERIFY(!service.hasBackup());
    QCOMPARE(plasma.run("mockRemoved").toInt(), removed);
    QCOMPARE(plasma.run("panels()[0].height").toInt(), 52); // Startup preserves the user's existing panel.
}

void LayoutTest::checksRequiredAndOptionalWidgets() {
    PlasmaMock plasma;
    for (const QString &id : ids) {
        plasma.reset();
        plasma.run("knownWidgetTypes=[];");
        plasma.apply(id);
        QCOMPARE(plasma.run("mockRemoved").toInt(), 0);
        QVERIFY(plasma.output().startsWith("MAINUAN_PREFLIGHT_ERROR:"));
    }
    for (const QString &id : {QString("unity"), QString("panel-top")}) {
        plasma.reset(); plasma.run("knownWidgetTypes=['org.kde.plasma.kickoff'];"); plasma.apply(id);
        QCOMPARE(plasma.run("mockRemoved").toInt(), 0);
        QVERIFY(plasma.output().contains(id == "unity" ? "Orbit.Start.Menu" : "TahoeLauncher"));
    }
    plasma.reset(); plasma.run("knownWidgetTypes=['org.kde.plasma.kicker'];"); plasma.apply("three-panels");
    QCOMPARE(plasma.run("panels().length").toInt(), 3);
    QVERIFY(LayoutService::matchesLayout(plasma.description(), "three-panels"));
    plasma.reset(); plasma.run("knownWidgetTypes=[];"); plasma.apply("unity", {"Orbit.Start.Menu"});
    QVERIFY(LayoutService::matchesLayout(plasma.description(), "unity")); // plugins discovered on disk
    plasma.reset(); plasma.run("screenCount=0;"); plasma.apply("minimal");
    QCOMPARE(plasma.run("mockRemoved").toInt(), 0);
    plasma.reset(); plasma.run("mockScreens[0].width=NaN;"); plasma.apply("minimal");
    QCOMPARE(plasma.run("mockRemoved").toInt(), 0);
}

void LayoutTest::rejectsIncompleteLayouts() {
    PlasmaMock plasma;
    plasma.apply("unity");
    plasma.run("panels()[1].items.forEach(function(w){if(w.type==='Orbit.Start.Menu')w.type='org.kde.plasma.kickoff';});");
    const auto wrongLauncher = plasma.description();
    QCOMPARE(wrongLauncher.launchers, 1);
    QCOMPARE(wrongLauncher.matchingLaunchers, 0);
    QVERIFY(!LayoutService::matchesLayout(wrongLauncher, "unity"));
    plasma.reset(); plasma.apply("three-panels"); plasma.run("panels()[2].remove();");
    QVERIFY(!LayoutService::matchesLayout(plasma.description(), "three-panels"));
    plasma.reset("[{width:1280,height:720},{width:1920,height:1080}]"); plasma.apply("unity");
    plasma.run("panels().forEach(function(p){p.screen=0;});");
    QVERIFY(!LayoutService::matchesLayout(plasma.description(), "unity"));
    plasma.reset(); plasma.apply("minimal"); plasma.run("panels()[0].floating=true;");
    QVERIFY(!LayoutService::matchesLayout(plasma.description(), "minimal"));
    QVERIFY(!LayoutService::matchesLayout(LayoutService::parseDescription("unity|2|1"), "unity"));
    QVERIFY(!LayoutService::parseDescription("{\"layout\":\"unity\",\"panels\":2}").valid());
    QVERIFY(!LayoutService::isAllowedLayout("constructor"));
    QVERIFY(LayoutService::buildPreflightScript("evil", {}).isEmpty());
}

void LayoutTest::sequencesBackupApplicationAndRollback() {
    IsolatedData isolated;
    PlasmaMock plasma;
    QTemporaryDir config, backups;
    const QString applets = config.filePath("plasma-org.kde.plasma.desktop-appletsrc");
    const QString shell = config.filePath("plasmashellrc");
    write(applets, "original panels\n"); write(shell, "original shell\n");
    QStringList controls;
    bool breakNextApplication = false, failNextApplication = false, backupBeforeApply = false, oversized = false;
    LayoutService::Backend backend;
    backend.evaluate = [&](const QString &script, auto done) {
        QTimer::singleShot(0, &plasma.engine, [&, script, done]() {
            const bool apply = script.trimmed().endsWith(");") && script.contains("\napplyMainuanLayout(\"");
            if (apply) {
                backupBeforeApply = !QDir(backups.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty();
                write(applets, "changed panels\n"); write(shell, "changed shell\n");
            }
            plasma.run("mockOutput='';");
            plasma.run(script);
            if (apply && oversized) { oversized = false; plasma.run("mockIconFactor=10;"); }
            if (apply && breakNextApplication) { breakNextApplication = false; plasma.run("panels()[0].items=[];"); }
            if (apply && failNextApplication) { failNextApplication = false; done(false, "simulated D-Bus failure"); }
            else done(true, plasma.output());
        });
    };
    backend.controlPlasma = [&](const QString &action, auto done) {
        controls << action;
        QTimer::singleShot(0, &plasma.engine, [&, action, done]() {
            if (action == "start") plasma.reset();
            done(true);
        });
    };
    LayoutService service(nullptr, config.path(), backups.path(), backend);
    service.refresh(); QTRY_VERIFY(service.available());
    plasma.run("knownWidgetTypes=['org.kde.plasma.kickoff'];");
    service.apply("unity"); QVERIFY(service.busy()); QTRY_VERIFY(!service.busy());
    QVERIFY(service.messageIsError()); QVERIFY(service.message().contains("Orbit.Start.Menu"));
    QVERIFY(!service.hasBackup()); QCOMPARE(plasma.run("mockRemoved").toInt(), 0); QVERIFY(controls.isEmpty());
    QCOMPARE(read(applets), QByteArray("original panels\n"));
    plasma.reset(); service.apply("single-bar"); QTRY_VERIFY(!service.busy());
    QVERIFY2(!service.messageIsError(), qPrintable(service.message()));
    QVERIFY(backupBeforeApply); QVERIFY(service.hasBackup()); QCOMPARE(service.activeLayout(), QString("single-bar"));
    QVERIFY(controls.isEmpty());
    // Reopening detects the active layout through actual scripted panel state.
    LayoutService reopened(nullptr, config.path(), backups.path(), backend);
    reopened.refresh(); QTRY_COMPARE(reopened.activeLayout(), QString("single-bar"));
    service.restoreBackup(); QTRY_VERIFY(!service.busy());
    QVERIFY(!service.messageIsError()); QCOMPARE(controls, QStringList({"stop", "start"}));
    QCOMPARE(read(applets), QByteArray("original panels\n")); QCOMPARE(read(shell), QByteArray("original shell\n"));
    controls.clear(); breakNextApplication = true;
    service.apply("panel-top"); QTRY_VERIFY(!service.busy());
    QVERIFY(service.messageIsError()); QVERIFY(service.message().contains("restaurado"));
    QCOMPARE(controls, QStringList({"stop", "start"}));
    QCOMPARE(read(applets), QByteArray("original panels\n"));
    controls.clear(); failNextApplication = true;
    service.apply("minimal"); QTRY_VERIFY(!service.busy());
    QVERIFY(service.messageIsError()); QCOMPARE(controls, QStringList({"stop", "start"}));
    QCOMPARE(read(shell), QByteArray("original shell\n"));
    controls.clear();
    service.apply("plasma-default");
    QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 5000);
    QVERIFY2(!service.messageIsError(), qPrintable(service.message()));
    QCOMPARE(service.activeLayout(), QString("plasma-default"));
    QVERIFY(controls.isEmpty());
    service.apply("three-panels");
    QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 5000);
    QVERIFY2(!service.messageIsError(), qPrintable(service.message()));
    QCOMPARE(service.activeLayout(), QString("three-panels"));
    QVERIFY(controls.isEmpty());
    const QByteArray beforeOverlap = read(applets);
    oversized = true;
    bool eventLoopResponsive = false;
    QTimer::singleShot(10, &service, [&]() { eventLoopResponsive = true; });
    service.apply("three-panels");
    QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 5000);
    QVERIFY(eventLoopResponsive);
    QVERIFY(service.messageIsError());
    QVERIFY(service.message().contains("sobreposição"));
    QCOMPARE(controls, QStringList({"stop", "start"}));
    QCOMPARE(read(applets), beforeOverlap);

}
QTEST_GUILESS_MAIN(LayoutTest)
#include "tst_layouts.moc"
