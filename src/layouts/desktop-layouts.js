// Mainuan Welcome desktop layouts, run by plasmashell through
// org.kde.PlasmaShell.evaluateScript (Plasma desktop scripting API).
//
// The Welcome prepends `installedWidgets` (plasmoid ids found on disk, which
// also covers packages Plasma does not list in knownWidgetTypes) and appends
// one call, applyMainuanLayout("<id>"), with an id from its own allowlist.
// Every panel is tagged with MainuanLayout=<id> so the Welcome can tell which
// layout is in use. Widgets that are not installed are skipped, so a missing
// third-party widget never breaks a layout. Third-party widgets start with
// their default settings.

var LAYOUT_KEY = "MainuanLayout";

function hasWidget(type) {
    return knownWidgetTypes.indexOf(type) >= 0
        || (typeof installedWidgets !== "undefined" && installedWidgets.indexOf(type) >= 0);
}

// config: { "General": { key: value } } — group paths separated by "/".
function addWidget(panel, type, config) {
    if (!hasWidget(type)) {
        return null;
    }
    var widget = panel.addWidget(type);
    if (config) {
        for (var group in config) {
            widget.currentConfigGroup = group.split("/");
            var values = config[group];
            for (var key in values) {
                widget.writeConfig(key, values[key]);
            }
        }
        widget.currentConfigGroup = [];
    }
    return widget;
}

function addLauncher(panel, config) {
    return addWidget(panel, "org.kde.plasma.kickoff", config) || addWidget(panel, "org.kde.plasma.kicker", config);
}

// Task managers on secondary screens only show that screen's windows.
function taskConfig(screen, extra) {
    var general = { "showOnlyCurrentScreen": screen > 0 ? "true" : "false" };
    for (var key in extra) {
        general[key] = extra[key];
    }
    return { "General": general };
}

function newPanel(screen, id, props) {
    var panel = new Panel("org.kde.panel");
    panel.screen = screen;
    panel.location = props.location;
    panel.height = props.height;
    panel.hiding = props.hiding || "none";
    panel.floating = props.floating === true;
    panel.alignment = props.alignment || "center";
    panel.lengthMode = props.lengthMode || "fill";
    if (props.lengthMode === "custom") {
        panel.minimumLength = props.length;
        panel.maximumLength = props.length;
    }
    if (props.offset !== undefined) {
        panel.offset = props.offset;
    }
    panel.writeConfig(LAYOUT_KEY, id);
    return panel;
}

// Mainuan's pinned applications (a string list in the task manager's config).
var MAINUAN_LAUNCHERS = ["applications:org.kde.kdeconnect.app.desktop", "applications:org.kde.discover.desktop",
                         "applications:onlyoffice-desktopeditors.desktop", "preferred://filemanager",
                         "applications:firefox.desktop"];

var layouts = {
    // Mainuan's default: floating "islands" along the bottom edge.
    "plasma-default": function (screen) {
        var launcher = newPanel(screen, "plasma-default", { location: "bottom", height: 40, floating: true, lengthMode: "fit", alignment: "left" });
        addLauncher(launcher);
        addWidget(launcher, "org.kde.plasma.marginsseparator");

        if (hasWidget("com.mike.desktop") || hasWidget("Chaac.Complete.Weather")) {
            var info = newPanel(screen, "plasma-default", { location: "bottom", height: 40, floating: true, lengthMode: "custom", length: 210, alignment: "left", offset: 68 });
            addWidget(info, "Chaac.Complete.Weather");
            addWidget(info, "com.mike.desktop");
        }

        var tasks = newPanel(screen, "plasma-default", { location: "bottom", height: 40, floating: true, lengthMode: "fit", alignment: "center" });
        addWidget(tasks, "org.kde.plasma.icontasks", taskConfig(screen, { "launchers": MAINUAN_LAUNCHERS }));

        var tray = newPanel(screen, "plasma-default", { location: "bottom", height: 40, floating: true, lengthMode: "fit", alignment: "right", offset: 151 });
        addWidget(tray, "org.kde.plasma.systemtray");
        addWidget(tray, "KdeControlStation");
        addWidget(tray, "org.kde.plasma.kdeaichat");
        addWidget(tray, "ChatAI-Plasmoid");

        var clock = newPanel(screen, "plasma-default", { location: "bottom", height: 40, floating: true, lengthMode: "fit", alignment: "right" });
        addWidget(clock, "org.kde.plasma.digitalclock");
        addWidget(clock, "org.kde.plasma.showdesktop");
    },

    // Full-width bar at the top with labelled tasks.
    "panel-top": function (screen) {
        var panel = newPanel(screen, "panel-top", { location: "top", height: 36 });
        addLauncher(panel);
        addWidget(panel, "org.kde.plasma.pager");
        addWidget(panel, "org.kde.plasma.taskmanager", taskConfig(screen, {}));
        addWidget(panel, "org.kde.plasma.systemtray");
        addWidget(panel, "org.kde.plasma.digitalclock");
        addWidget(panel, "org.kde.plasma.showdesktop");
    },

    // A single centred dock that floats above the bottom edge.
    "floating": function (screen) {
        var panel = newPanel(screen, "floating", { location: "bottom", height: 52, floating: true, lengthMode: "fit", alignment: "center" });
        addLauncher(panel);
        addWidget(panel, "org.kde.plasma.marginsseparator");
        addWidget(panel, "org.kde.plasma.icontasks", taskConfig(screen, { "launchers": MAINUAN_LAUNCHERS }));
        addWidget(panel, "org.kde.plasma.marginsseparator");
        addWidget(panel, "org.kde.plasma.systemtray");
        addWidget(panel, "org.kde.plasma.digitalclock");
    },

    // A thin bottom bar that moves out of the way of windows.
    "minimal": function (screen) {
        var panel = newPanel(screen, "minimal", { location: "bottom", height: 32, hiding: "dodgewindows" });
        addLauncher(panel);
        addWidget(panel, "org.kde.plasma.icontasks", taskConfig(screen, {}));
        addWidget(panel, "org.kde.plasma.panelspacer");
        addWidget(panel, "org.kde.plasma.systemtray");
        addWidget(panel, "org.kde.plasma.digitalclock");
    },

    // Unity-style: global menu bar at the top and a launcher dock on the left.
    "unity": function (screen) {
        var top = newPanel(screen, "unity", { location: "top", height: 28 });
        addWidget(top, "org.kde.plasma.appmenu");
        addWidget(top, "org.kde.plasma.panelspacer");
        addWidget(top, "org.kde.plasma.systemtray");
        addWidget(top, "org.kde.plasma.digitalclock");

        var dock = newPanel(screen, "unity", { location: "left", height: 56 });
        addLauncher(dock);
        addWidget(dock, "org.kde.plasma.icontasks", taskConfig(screen, { "launchers": MAINUAN_LAUNCHERS }));
        addWidget(dock, "org.kde.plasma.panelspacer");
        addWidget(dock, "org.kde.plasma.trash");
    },

    // Compact top bar centred on virtual desktops, for tiled windows.
    "tiling": function (screen) {
        var panel = newPanel(screen, "tiling", { location: "top", height: 30 });
        addLauncher(panel);
        addWidget(panel, "org.kde.plasma.pager", { "General": { "displayedText": "Number" } });
        addWidget(panel, "org.kde.plasma.panelspacer");
        addWidget(panel, "org.kde.plasma.icontasks", taskConfig(screen, {}));
        addWidget(panel, "org.kde.plasma.panelspacer");
        addWidget(panel, "org.kde.plasma.systemtray");
        addWidget(panel, "org.kde.plasma.digitalclock");
    }
};

function applyMainuanLayout(id) {
    var build = layouts[id];
    if (!build) {
        throw new Error("unknown layout " + id);
    }
    panels().forEach(function (panel) { panel.remove(); });
    for (var screen = 0; screen < screenCount; ++screen) {
        build(screen);
    }
    print("applied " + id + " panels=" + panels().length);
}

// Prints "<layout id or empty>|<panel count>|<launchers>" for the Welcome.
function describeMainuanLayout() {
    var ids = [];
    var launchers = 0;
    panels().forEach(function (panel) {
        ids.push(panel.readConfig(LAYOUT_KEY, ""));
        panel.widgets().forEach(function (widget) {
            if (["org.kde.plasma.kickoff", "org.kde.plasma.kicker", "org.kde.plasma.kickerdash"].indexOf(widget.type) >= 0) {
                ++launchers;
            }
        });
    });
    var same = ids.length > 0 && ids.every(function (value) { return value !== "" && value === ids[0]; });
    print((same ? ids[0] : "") + "|" + panels().length + "|" + launchers);
}
