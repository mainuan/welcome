// Isolated implementation of the Plasma scripting surface used by the layouts.
var knownWidgetTypes = [];
var installedWidgets = [];
var screenCount = 1;
var mockScreens = [{width: 1920, height: 1080}];
var mockPanels = [], mockRemoved = 0, mockOutput = "";
var mockIconFactor = 1;
function screenGeometry(screen) { return mockScreens[screen]; }
function print(value) { mockOutput += String(value) + "\n"; }
function panels() { return mockPanels.slice(); }
function MockConfig() { this.config = {}; this.currentConfigGroup = []; }
MockConfig.prototype.writeConfig = function (key, value) {
    this.config[this.currentConfigGroup.concat([key]).join("/")] = value;
};
MockConfig.prototype.readConfig = function (key, fallback) {
    var path = this.currentConfigGroup.concat([key]).join("/");
    return Object.prototype.hasOwnProperty.call(this.config, path) ? this.config[path] : fallback;
};
function MockWidget(type) { MockConfig.call(this); this.type = type; }
MockWidget.prototype = Object.create(MockConfig.prototype);
function Panel(type) {
    MockConfig.call(this);
    this.type = type; this.screen = 0; this.location = "bottom";
    this.floating = false; this.items = []; this.writes = [];
    this._minimumLength = 0; this._maximumLength = screenGeometry(this.screen).width; this._offset = 0;
    mockPanels.push(this);
}
Panel.prototype = Object.create(MockConfig.prototype);
['minimumLength', 'maximumLength', 'offset'].forEach(function (key) {
    Object.defineProperty(Panel.prototype, key, {
        get: function () { return this['_' + key]; },
        set: function (value) { this.writes.push(key); this['_' + key] = value; }
    });
});
Object.defineProperty(Panel.prototype, 'length', {
    get: function () {
        return this.items.reduce(function (total, widget) {
            var type = widget.type, value = 40 * mockIconFactor;
            if (type === 'org.kde.plasma.icontasks') value *= (widget.config['General/launchers'] || [1]).length;
            if (type === 'org.kde.plasma.digitalclock') value = 110;
            if (type === 'org.kde.plasma.systemtray') value = 140;
            if (type === 'Chaac.Complete.Weather' || type === 'com.mike.desktop') value = 90;
            if (type === 'org.kde.plasma.marginsseparator') value = 4;
            if (type === 'org.kde.plasma.panelspacer') value = widget.config['General/expanding'] === false ? widget.config['General/length'] : 0;
            return total + value;
        }, 0);
    },
    set: function () { throw new Error('Do not impose the content length reported by Plasma'); }
});
Panel.prototype.addWidget = function (type) { var widget = new MockWidget(type); this.items.push(widget); return widget; };
Panel.prototype.widgets = function () { return this.items.slice(); };
Panel.prototype.remove = function () { ++mockRemoved; mockPanels.splice(mockPanels.indexOf(this), 1); };
function mockReset(screens) {
    mockScreens = screens; screenCount = screens.length; mockPanels = []; mockRemoved = 0; mockOutput = ""; mockIconFactor = 1;
    var original = new Panel("org.kde.panel");
    original.addWidget("org.kde.plasma.kickoff");
}
