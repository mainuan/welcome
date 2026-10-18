// Mainuan Welcome desktop layouts, run by plasmashell through
// org.kde.PlasmaShell.evaluateScript (Plasma desktop scripting API).
//
// The Welcome prepends `installedWidgets` (plasmoid ids found on disk, which
// also covers packages Plasma does not list in knownWidgetTypes) and appends
// one call, applyMainuanLayout("<id>"), with an id from its own allowlist.
// Every panel is tagged with MainuanLayout=<id> so the Welcome can tell which
// layout is in use. Widgets that are not installed are skipped, so a missing
// optional widget never breaks a layout. Required launchers are checked before
// removing any panels. Sanitized reference settings live in reference-layouts.js.

var LAYOUT_KEY = "MainuanLayout";
var PANEL_THICKNESS = 40;

function hasWidget(type) {
    return knownWidgetTypes.indexOf(type) >= 0
        || (typeof installedWidgets !== "undefined" && installedWidgets.indexOf(type) >= 0);
}

// config: { "General": { key: value } } — group paths separated by "/".
function addWidget(panel, type, config) {
    if (!hasWidget(type)) {
        return null;
    }
    // Only documented per-widget options: there is no universal iconSize key.
    config = JSON.parse(JSON.stringify(config || {}));
    if (type === "org.kde.plasma.systemtray") {
        config.General = config.General || {};
        config.General.scaleIconsToFit = true;
        config.General.iconSpacing = 1;
    } else if (type === "org.kde.plasma.icontasks") {
        config.General = config.General || {};
        config.General.maxStripes = 1;
        config.General.iconSpacing = 1;
        if (panel.lengthMode === "fit") config.General.fill = false;
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
    panel.height = PANEL_THICKNESS;
    panel.hiding = props.hiding || "none";
    panel.floating = props.floating === true;
    panel.alignment = props.alignment || "center";
    panel.lengthMode = props.lengthMode || "fill";
    if (props.lengthMode === "custom") {
        panel.maximumLength = props.maximumLength !== undefined ? props.maximumLength : props.length;
        panel.minimumLength = props.minimumLength !== undefined ? props.minimumLength : props.length;
    }
    if (props.lengthMode === "custom" && props.offset !== undefined) {
        panel.offset = props.offset;
    }
    panel.writeConfig(LAYOUT_KEY, id);
    if (props.role) panel.writeConfig("MainuanPanelRole", props.role);
    return panel;
}

// Mainuan's pinned applications (a string list in the task manager's config).
var MAINUAN_LAUNCHERS = ["applications:org.kde.kdeconnect.app.desktop", "applications:org.kde.discover.desktop",
                         "applications:onlyoffice-desktopeditors.desktop", "preferred://filemanager",
                         "applications:firefox.desktop"];

var layouts = {
    "plasma-default": function (screen) { buildReferenceLayout("plasma-default", screen); },

    "three-panels": function (screen) { buildReferenceLayout("three-panels", screen); },
    "single-bar": function (screen) { buildReferenceLayout("single-bar", screen); },
    "panel-top": function (screen) { buildReferenceLayout("panel-top", screen); },

    "minimal": function (screen) { buildReferenceLayout("minimal", screen); },
    "unity": function (screen) { buildReferenceLayout("unity", screen); },

    // Compact top bar centred on virtual desktops, for tiled windows.
    "tiling": function (screen) {
        var panel = newPanel(screen, "tiling", { location: "top" });
        addLauncher(panel);
        addWidget(panel, "org.kde.plasma.pager", { "General": { "displayedText": "Number" } });
        addWidget(panel, "org.kde.plasma.panelspacer");
        addWidget(panel, "org.kde.plasma.icontasks", taskConfig(screen, {}));
        addWidget(panel, "org.kde.plasma.panelspacer");
        addWidget(panel, "org.kde.plasma.systemtray");
        addWidget(panel, "org.kde.plasma.digitalclock");
    }
};

// Geometry is expressed in logical pixels. Fit panels receive no custom limits
// or offsets: Plasma derives their lengths from their widgets.
function referenceGeometry(id, screen, role) {
    var width = Math.floor(screenGeometry(screen).width);
    var margin = Math.max(10, Math.round(width * 0.018));
    var props = { role: role, location: "bottom", floating: true,
                  lengthMode: "custom", alignment: "center", length: width - 2 * margin };
    if (id === "plasma-default") {
        // The active manual reference already uses 40-pixel panels. Keep its
        // lengths and edge-relative offsets independent of monitor resolution.
        var classic = CLASSIC_PANEL_GEOMETRY[role];
        for (var key in classic) props[key] = classic[key];
    } else if (id === "three-panels") {
        props.lengthMode = "fit";
        props.alignment = role === "left" ? "left" : role === "right" ? "right" : "center";
    } else if (id === "panel-top") {
        props.location = "top";
    } else if (id === "minimal") {
        props.floating = false;
        props.lengthMode = "fill";
    } else if (id === "unity") {
        props.floating = false;
        props.location = role === "top" ? "top" : "left";
        props.lengthMode = "fill";
    }
    return props;
}

function launcherTypes(id) {
    if (id === "unity") return ["Orbit.Start.Menu"];
    if (id === "panel-top") return ["TahoeLauncher"];
    return ["org.kde.plasma.kickoff", "org.kde.plasma.kicker"];
}

function buildReferenceLayout(id, screen) {
    REFERENCE_LAYOUTS[id].forEach(function (spec) {
        var panel = newPanel(screen, id, referenceGeometry(id, screen, spec.role));
        panel.opacity = "adaptive";
        spec.widgets.forEach(function (entry) {
            var config = entry.config || {};
            if (entry.type === "org.kde.plasma.icontasks") {
                config = taskConfig(screen, config.General || {});
                config.General.showOnlyCurrentScreen = screen > 0;
            }
            if (entry.type === "org.kde.plasma.kickoff") addLauncher(panel, config);
            else addWidget(panel, entry.type, config);
        });
    });
}

function preflightMainuanLayout(id) {
    if (!Object.prototype.hasOwnProperty.call(layouts, id)) {
        return { ok: false, message: "Layout inválido." };
    }
    if (!launcherTypes(id).some(hasWidget)) {
        return { ok: false, message: "Lançador obrigatório ausente: " + launcherTypes(id).join(" ou ") + ". Os painéis anteriores foram preservados." };
    }
    var required = REFERENCE_LAYOUTS[id] || [];
    for (var i = 0; i < required.length; ++i) {
        for (var j = 0; j < required[i].widgets.length; ++j) {
            var entry = required[i].widgets[j];
            if (entry.required && entry.type !== "org.kde.plasma.kickoff" && !hasWidget(entry.type)) {
                return { ok: false, message: "Widget obrigatório ausente: " + entry.type + ". Os painéis anteriores foram preservados." };
            }
        }
    }
    if (screenCount < 1) return { ok: false, message: "Nenhuma tela disponível no Plasma." };
    for (var screen = 0; screen < screenCount; ++screen) {
        var geometry = screenGeometry(screen);
        if (!geometry || !isFinite(geometry.width) || !isFinite(geometry.height) || geometry.width <= 0 || geometry.height <= 0) {
            return { ok: false, message: "Não foi possível consultar a geometria da tela." };
        }
    }
    return { ok: true };
}

function applyMainuanLayout(id) {
    var check = preflightMainuanLayout(id); // Repeat just before any destructive step.
    if (!check.ok) {
        print("MAINUAN_PREFLIGHT_ERROR:" + check.message);
        return;
    }
    panels().forEach(function (panel) { panel.remove(); });
    for (var screen = 0; screen < screenCount; ++screen) layouts[id](screen);
    print("applied " + id + " panels=" + panels().length);
}

function panelsPerScreen(id) {
    if (id === "three-panels") return 3;
    if (id === "unity") return 2;
    if (id === "plasma-default") return 5;
    return 1;
}

// Called only after an explicit application, once QML has had time to measure
// its widgets. Never run this positioning step from a startup/refresh query.
function positionMainuanLayout(id) {
    var measurements = [], ok = true;
    for (var screen = 0; screen < screenCount; ++screen) {
        var width = screenGeometry(screen).width;
        var onScreen = panels().filter(function (panel) { return panel.screen === screen; });
        var byRole = {};
        onScreen.forEach(function (panel) {
            var role = panel.readConfig("MainuanPanelRole", "");
            var length = Math.max(PANEL_THICKNESS, panel.length);
            if (panel.lengthMode === "custom") length = Math.max(panel.minimumLength, Math.min(panel.maximumLength, length));
            byRole[role] = { panel: panel, length: length };
            measurements.push(Math.round(length));
        });
        var gap = Math.max(32, Math.round(width * 0.01));
        if (id === "plasma-default") {
            if (!byRole.launcher || !byRole.info || !byRole.tasks || !byRole.tray || !byRole.clock) return { ok: false };
            var taskLeft = (width - byRole.tasks.length) / 2;
            // Cluster the information panel with the launcher and controls
            // with the clock, instead of bisecting the empty space to tasks.
            // This clearance is logical and applies only to the classic layout.
            var classicGap = 16;
            var infoOffset = Math.max(CLASSIC_PANEL_GEOMETRY.info.offset,
                Math.ceil(byRole.launcher.length + classicGap));
            var trayOffset = Math.max(CLASSIC_PANEL_GEOMETRY.tray.offset,
                Math.ceil(byRole.clock.length + classicGap));
            ["info", "tray"].forEach(function (role) {
                var panel = byRole[role].panel;
                var offset = role === "info" ? infoOffset : trayOffset;
                var limit = Math.floor(taskLeft - offset - classicGap);
                ok = ok && limit >= panel.minimumLength;
                if (limit >= panel.minimumLength) {
                    panel.offset = offset;
                    panel.maximumLength = Math.min(CLASSIC_PANEL_GEOMETRY[role].maximumLength, limit);
                }
            });
            if (ok) {
                // A plasmoid can report an unbounded preferred panel length.
                // Check the actual fourth-panel widgets before accepting a
                // bound that would clip them; the existing rollback handles it.
                var trayWidgetWidth = 0;
                byRole.tray.panel.widgets().forEach(function (widget) {
                    var geometry = widget.geometry;
                    if (geometry && isFinite(geometry.width)) trayWidgetWidth += geometry.width;
                });
                ok = ok && trayWidgetWidth <= byRole.tray.panel.maximumLength;
            }
        } else if (id === "three-panels") {
            if (!byRole.left || !byRole.tasks || !byRole.right) return { ok: false };
            var centerLeft = (width - byRole.tasks.length) / 2;
            ok = ok && centerLeft - byRole.left.length >= gap && centerLeft - byRole.right.length >= gap;
        }
    }
    return { ok: ok, signature: measurements.join(","),
             message: ok ? "" : "Os widgets não cabem na tela sem sobrepor os segmentos." };
}

// Validation reads real panel locations and real widget types, not just tags.
function describeMainuanLayout() {
    var all = panels();
    var id = all.length ? all[0].readConfig(LAYOUT_KEY, "") : "";
    var same = all.every(function (panel) { return id !== "" && panel.readConfig(LAYOUT_KEY, "") === id; });
    if (!same || (!Object.prototype.hasOwnProperty.call(layouts, id) && id !== "floating")) id = "";
    var launchers = 0, matching = 0, structure = id !== "";
    var launcherIds = ["org.kde.plasma.kickoff", "org.kde.plasma.kicker", "org.kde.plasma.kickerdash", "Orbit.Start.Menu", "TahoeLauncher"];
    all.forEach(function (panel) {
        panel.widgets().forEach(function (widget) { if (launcherIds.indexOf(widget.type) >= 0) ++launchers; });
    });
    if (id) {
        for (var screen = 0; screen < screenCount; ++screen) {
            var onScreen = all.filter(function (panel) { return panel.screen === screen; });
            var count = 0;
            structure = structure && onScreen.length === panelsPerScreen(id);
            onScreen.forEach(function (panel) {
                var expectedLocation = id === "unity" ? panel.readConfig("MainuanPanelRole", "") === "dock" ? "left" : "top"
                    : id === "panel-top" || id === "tiling" ? "top" : "bottom";
                structure = structure && panel.location === expectedLocation;
                if (id === "minimal" || id === "unity") structure = structure && panel.floating === false;
                if (id === "plasma-default" || id === "three-panels" || id === "single-bar" || id === "panel-top" || id === "floating") structure = structure && panel.floating === true;
                if (id !== "floating") structure = structure && panel.height === PANEL_THICKNESS;
                if (REFERENCE_LAYOUTS[id]) {
                    var role = panel.readConfig("MainuanPanelRole", "");
                    var expected = referenceGeometry(id, screen, role);
                    structure = structure && panel.lengthMode === expected.lengthMode;
                    var spec = REFERENCE_LAYOUTS[id].filter(function (item) { return item.role === role; })[0];
                    if (spec) structure = structure && spec.widgets.filter(function (entry) { return entry.required; }).every(function (entry) {
                        return panel.widgets().some(function (widget) {
                            return entry.type === "org.kde.plasma.kickoff" ? launcherTypes(id).indexOf(widget.type) >= 0 : widget.type === entry.type;
                        });
                    });
                }
                panel.widgets().forEach(function (widget) {
                    if (launcherTypes(id).indexOf(widget.type) >= 0
                        && (id !== "unity" || panel.location === "left")) ++count;
                });
            });
            matching += count;
            structure = structure && count === 1;
            if (REFERENCE_LAYOUTS[id]) {
                var roles = onScreen.map(function (panel) { return panel.readConfig("MainuanPanelRole", ""); });
                structure = structure && REFERENCE_LAYOUTS[id].every(function (spec) {
                    return roles.filter(function (role) { return role === spec.role; }).length === 1;
                });
            }
        }
        structure = structure && all.length === screenCount * panelsPerScreen(id) && launchers === screenCount;
    }
    print(JSON.stringify({ layout: id, panels: all.length, launchers: launchers, screens: screenCount,
                           matchingLaunchers: matching, structureOk: structure }));
}
