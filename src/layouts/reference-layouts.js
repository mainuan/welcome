// Sanitized widget order and settings from the Mainuan layout references.
// No containment/applet/activity IDs, screen mappings, personal paths or migration state.
// Active, tagged classic panels from the manual spacing reference.
// Logical pixel offsets are relative to the corresponding screen edge.
var CLASSIC_PANEL_GEOMETRY = {
    "launcher": {
        "lengthMode": "fit",
        "alignment": "left"
    },
    "info": {
        "lengthMode": "custom",
        "alignment": "left",
        "minimumLength": 217,
        "maximumLength": 221,
        "offset": 65
    },
    "tasks": {
        "lengthMode": "fit",
        "alignment": "center"
    },
    "tray": {
        "lengthMode": "custom",
        "alignment": "right",
        "minimumLength": 203,
        "maximumLength": 273,
        "offset": 111
    },
    "clock": {
        "lengthMode": "fit",
        "alignment": "right"
    }
};

var REFERENCE_LAYOUTS = {
    "plasma-default": [
        {
            "role": "launcher",
            "widgets": [
                {
                    "type": "org.kde.plasma.marginsseparator",
                    "required": true
                },
                {
                    "type": "org.kde.plasma.kickoff",
                    "required": true,
                    "config": {
                        "General": {
                            "applicationsDisplay": 0,
                            "icon": "kde",
                            "showAppsByName": true,
                            "systemFavorites": [
                                "suspend",
                                "hibernate",
                                "reboot",
                                "shutdown"
                            ]
                        },
                        "Shortcuts": {
                            "global": "Alt+F1"
                        }
                    }
                }
            ]
        },
        {
            "role": "info",
            "widgets": [
                {
                    "type": "Chaac.Complete.Weather",
                    "required": true
                },
                {
                    "type": "com.mike.desktop",
                    "required": true
                }
            ]
        },
        {
            "role": "tasks",
            "widgets": [
                {
                    "type": "org.kde.plasma.icontasks",
                    "required": true,
                    "config": {
                        "General": {
                            "launchers": [
                                "applications:org.kde.kdeconnect.app.desktop",
                                "applications:org.kde.discover.desktop",
                                "applications:onlyoffice-desktopeditors.desktop",
                                "preferred://filemanager",
                                "applications:firefox.desktop"
                            ]
                        }
                    }
                }
            ]
        },
        {
            "role": "tray",
            "widgets": [
                {
                    "type": "ChatAI-Plasmoid",
                    "required": true,
                    "config": {
                        "General": {
                            "customSitesJson": "[{\"name\":\"Maritaca AI\",\"url\":\"https://chat.maritaca.ai/auth\"}]",
                            "showBingCopilot": true,
                            "url": "https://chat.maritaca.ai/auth"
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.systemtray",
                    "required": true,
                    "config": {
                        "General": {
                            "extraItems": [
                                "org.kde.kupapplet",
                                "org.kde.kdeconnect",
                                "org.kde.plasma.vault",
                                "org.kde.plasma.kclock_1x2",
                                "org.kde.plasma.keyboardindicator",
                                "org.kde.plasma.weather",
                                "org.kde.plasma.mediacontroller",
                                "org.kde.plasma.printmanager",
                                "org.kde.plasma.devicenotifier",
                                "org.kde.plasma.clipboard",
                                "org.kde.plasma.keyboardlayout",
                                "org.kde.plasma.notifications",
                                "org.kde.plasma.networkmanagement",
                                "org.kde.plasma.bluetooth",
                                "org.kde.plasma.battery",
                                "org.kde.plasma.manage-inputmethod",
                                "org.kde.plasma.volume",
                                "org.kde.kscreen",
                                "org.kde.plasma.cameraindicator"
                            ]
                        }
                    }
                },
                {
                    "type": "KdeControlStation",
                    "required": true
                }
            ]
        },
        {
            "role": "clock",
            "widgets": [
                {
                    "type": "org.kde.plasma.digitalclock",
                    "required": true,
                    "config": {
                        "Appearance": {
                            "showDate": true,
                            "showSeconds": false
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.powershowdesktop",
                    "required": true,
                    "config": {
                        "General": {
                            "edgeColor": "#669e9e9e",
                            "hoveredColor": "#cbcbcb"
                        }
                    }
                }
            ]
        }
    ],
    "three-panels": [
        {
            "role": "left",
            "widgets": [
                {
                    "type": "org.kde.plasma.kickoff",
                    "config": {
                        "General": {
                            "activationIndicator": false,
                            "appNameFormat": 0,
                            "applicationsDisplay": 0,
                            "customButtonImage": "start-here-hexabee",
                            "customGreeting": "Breathe. Focus. Start.",
                            "displayPosition": 2,
                            "floating": true,
                            "glowColor": 1,
                            "icon": "kde",
                            "launcherPosition": 1,
                            "offsetY": 10,
                            "systemFavorites": [
                                "suspend",
                                "hibernate",
                                "reboot",
                                "shutdown"
                            ],
                            "useCustomButtonImage": true,
                            "useSystemFontSettings": true
                        }
                    }
                },
                {
                    "type": "Chaac.Complete.Weather",
                    "config": {
                        "General": {
                            "sizeFontConfig": 10,
                            "textweather": false
                        }
                    }
                },
                {
                    "type": "org.kde.milou"
                },
                {
                    "type": "com.mike.desktop",
                    "config": {
                        "Pills": {
                            "pillsActiveColor": "#000000",
                            "pillsInactiveColor": "#000000",
                            "pillsInactiveOpacity": 30
                        }
                    }
                }
            ]
        },
        {
            "role": "tasks",
            "widgets": [
                {
                    "type": "org.kde.plasma.icontasks",
                    "config": {
                        "General": {
                            "launchers": [
                                "applications:org.kde.kdeconnect.app.desktop",
                                "applications:org.jitsi.jitsi-meet.desktop",
                                "applications:org.kde.discover.desktop",
                                "applications:onlyoffice-desktopeditors.desktop",
                                "preferred://filemanager",
                                "applications:firefox.desktop"
                            ]
                        }
                    }
                }
            ]
        },
        {
            "role": "right",
            "widgets": [
                {
                    "type": "ChatAI-Plasmoid"
                },
                {
                    "type": "org.kde.plasma.systemtray",
                    "config": {
                        "General": {
                            "extraItems": [
                                "org.kde.plasma.clipboard",
                                "org.kde.plasma.bluetooth",
                                "org.kde.plasma.keyboardlayout",
                                "org.kde.plasma.vault",
                                "org.kde.plasma.cameraindicator",
                                "org.kde.plasma.manage-inputmethod",
                                "org.kde.plasma.mediacontroller",
                                "org.kde.plasma.notifications",
                                "org.kde.plasma.networkmanagement",
                                "org.kde.plasma.weather",
                                "org.kde.plasma.devicenotifier",
                                "org.kde.plasma.printmanager",
                                "org.kde.plasma.keyboardindicator",
                                "org.kde.plasma.battery",
                                "org.kde.plasma.volume",
                                "org.kde.kscreen",
                                "org.kde.kupapplet",
                                "org.kde.kdeconnect",
                                "org.kde.plasma.kclock_1x2"
                            ],
                            "hiddenItems": [
                                "org.kde.kscreen"
                            ]
                        }
                    }
                },
                {
                    "type": "KdeControlStation",
                    "config": {
                        "Appearance": {
                            "darkGlobalTheme": "Blue-Dark-1",
                            "lightGlobalTheme": "Blue-Light-1",
                            "preferChangeGlobalTheme": true,
                            "scale": 95,
                            "showBorders": false,
                            "showMediaPlayer": false,
                            "showPercentage": true,
                            "transparency": true,
                            "transparencyLevel": 50
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.digitalclock",
                    "config": {
                        "Appearance": {
                            "autoFontAndSize": false,
                            "boldText": true,
                            "dateDisplayFormat": "BelowTime",
                            "fontFamily": "Adwaita Mono",
                            "fontSize": 12,
                            "fontStyleName": "Bold",
                            "fontWeight": 700,
                            "use24hFormat": 2
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.powershowdesktop"
                },
                {
                    "type": "org.kde.plasma.panelspacer",
                    "config": {
                        "General": {
                            "expanding": false,
                            "length": 5
                        }
                    }
                }
            ]
        }
    ],
    "single-bar": [
        {
            "role": "bar",
            "widgets": [
                {
                    "type": "org.kde.plasma.panelspacer",
                    "config": {
                        "General": {
                            "expanding": false,
                            "length": 5
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.kickoff",
                    "config": {
                        "General": {
                            "activationIndicator": false,
                            "appNameFormat": 0,
                            "applicationsDisplay": 0,
                            "customButtonImage": "start-here-hexabee",
                            "customGreeting": "Breathe. Focus. Start.",
                            "displayPosition": 2,
                            "floating": true,
                            "glowColor": 1,
                            "icon": "kde",
                            "launcherPosition": 1,
                            "offsetY": 10,
                            "systemFavorites": [
                                "suspend",
                                "hibernate",
                                "reboot",
                                "shutdown"
                            ],
                            "useCustomButtonImage": true,
                            "useSystemFontSettings": true
                        }
                    }
                },
                {
                    "type": "Chaac.Complete.Weather",
                    "config": {
                        "General": {
                            "sizeFontConfig": 10,
                            "textweather": false
                        }
                    }
                },
                {
                    "type": "org.kde.milou"
                },
                {
                    "type": "com.mike.desktop",
                    "config": {
                        "Pills": {
                            "pillsActiveColor": "#000000",
                            "pillsInactiveColor": "#000000",
                            "pillsInactiveOpacity": 30
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.panelspacer"
                },
                {
                    "type": "org.kde.plasma.icontasks",
                    "config": {
                        "General": {
                            "launchers": [
                                "applications:org.kde.kdeconnect.app.desktop",
                                "applications:org.jitsi.jitsi-meet.desktop",
                                "applications:org.kde.discover.desktop",
                                "applications:onlyoffice-desktopeditors.desktop",
                                "preferred://filemanager",
                                "applications:firefox.desktop"
                            ]
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.panelspacer"
                },
                {
                    "type": "ChatAI-Plasmoid",
                    "config": {
                        "General": {
                            "customSitesJson": "[{\"name\":\"Maritalk\",\"url\":\"https://chat.maritaca.ai/\"}]"
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.systemtray",
                    "config": {
                        "General": {
                            "extraItems": [
                                "org.kde.plasma.clipboard",
                                "org.kde.plasma.bluetooth",
                                "org.kde.plasma.keyboardlayout",
                                "org.kde.plasma.vault",
                                "org.kde.plasma.cameraindicator",
                                "org.kde.plasma.manage-inputmethod",
                                "org.kde.plasma.mediacontroller",
                                "org.kde.plasma.notifications",
                                "org.kde.plasma.networkmanagement",
                                "org.kde.plasma.weather",
                                "org.kde.plasma.devicenotifier",
                                "org.kde.plasma.printmanager",
                                "org.kde.plasma.keyboardindicator",
                                "org.kde.plasma.battery",
                                "org.kde.plasma.volume",
                                "org.kde.kscreen",
                                "org.kde.kupapplet",
                                "org.kde.kdeconnect",
                                "org.kde.plasma.kclock_1x2"
                            ],
                            "hiddenItems": [
                                "org.kde.kscreen"
                            ]
                        }
                    }
                },
                {
                    "type": "KdeControlStation",
                    "config": {
                        "Appearance": {
                            "darkGlobalTheme": "Blue-Dark-1",
                            "lightGlobalTheme": "Blue-Light-1",
                            "preferChangeGlobalTheme": true,
                            "scale": 95,
                            "showBorders": false,
                            "showMediaPlayer": false,
                            "showPercentage": true,
                            "transparency": true,
                            "transparencyLevel": 50
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.digitalclock",
                    "config": {
                        "Appearance": {
                            "autoFontAndSize": false,
                            "boldText": true,
                            "dateDisplayFormat": "BelowTime",
                            "fontFamily": "Adwaita Mono",
                            "fontSize": 12,
                            "fontStyleName": "Bold",
                            "fontWeight": 700,
                            "use24hFormat": 2
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.powershowdesktop"
                },
                {
                    "type": "org.kde.plasma.panelspacer",
                    "config": {
                        "General": {
                            "expanding": false,
                            "length": 5
                        }
                    }
                }
            ]
        }
    ],
    "unity": [
        {
            "role": "top",
            "widgets": [
                {
                    "type": "org.kde.plasma.appmenu"
                },
                {
                    "type": "org.kde.plasma.panelspacer"
                },
                {
                    "type": "org.kde.plasma.digitalclock"
                },
                {
                    "type": "org.kde.plasma.panelspacer"
                },
                {
                    "type": "ChatAI-Plasmoid"
                },
                {
                    "type": "org.kde.plasma.systemtray",
                    "config": {
                        "General": {
                            "extraItems": [
                                "org.kde.kdeconnect",
                                "org.kde.kupapplet",
                                "org.kde.plasma.kclock_1x2",
                                "org.kde.plasma.vault",
                                "org.kde.kscreen",
                                "org.kde.plasma.battery",
                                "org.kde.plasma.bluetooth",
                                "org.kde.plasma.brightness",
                                "org.kde.plasma.cameraindicator",
                                "org.kde.plasma.clipboard",
                                "org.kde.plasma.devicenotifier",
                                "org.kde.plasma.keyboardindicator",
                                "org.kde.plasma.keyboardlayout",
                                "org.kde.plasma.manage-inputmethod",
                                "org.kde.plasma.mediacontroller",
                                "org.kde.plasma.networkmanagement",
                                "org.kde.plasma.notifications",
                                "org.kde.plasma.printmanager",
                                "org.kde.plasma.volume",
                                "org.kde.plasma.weather"
                            ]
                        }
                    }
                },
                {
                    "type": "KdeControlStation"
                },
                {
                    "type": "org.kde.plasma.powershowdesktop"
                }
            ]
        },
        {
            "role": "dock",
            "widgets": [
                {
                    "type": "org.kde.plasma.panelspacer",
                    "config": {
                        "General": {
                            "expanding": false,
                            "length": 15
                        }
                    }
                },
                {
                    "type": "Orbit.Start.Menu",
                    "config": {
                        "General": {
                            "backgroundOpacity": 60,
                            "displayPosition": "Default",
                            "launcherPosition": 0,
                            "maxNumberColumns": 4
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.icontasks",
                    "config": {
                        "General": {
                            "launchers": [
                                "applications:org.kde.kdeconnect.app.desktop",
                                "applications:org.kde.discover.desktop",
                                "applications:onlyoffice-desktopeditors.desktop",
                                "preferred://filemanager",
                                "applications:firefox.desktop"
                            ],
                            "showOnlyCurrentScreen": false
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.panelspacer"
                },
                {
                    "type": "org.kde.plasma.trash"
                },
                {
                    "type": "org.kde.plasma.panelspacer",
                    "config": {
                        "General": {
                            "expanding": false,
                            "length": 10
                        }
                    }
                }
            ]
        }
    ],
    "panel-top": [
        {
            "role": "bar",
            "widgets": [
                {
                    "type": "org.kde.plasma.panelspacer",
                    "config": {
                        "General": {
                            "expanding": false,
                            "length": 5
                        }
                    }
                },
                {
                    "type": "TahoeLauncher",
                    "config": {
                        "General": {
                            "activationIndicator": false,
                            "appNameFormat": 0,
                            "applicationsDisplay": 0,
                            "appsIconSize": 2,
                            "customButtonImage": "start-here-hexabee",
                            "customGreeting": "Breathe. Focus. Start.",
                            "displayPosition": 2,
                            "glowColor": 1,
                            "launcherPosition": 0,
                            "offsetX": 5,
                            "offsetY": 5,
                            "systemFavorites": [
                                "suspend",
                                "hibernate",
                                "reboot",
                                "shutdown"
                            ],
                            "useSystemFontSettings": true
                        }
                    }
                },
                {
                    "type": "Chaac.Complete.Weather",
                    "config": {
                        "General": {
                            "sizeFontConfig": 10,
                            "textweather": false
                        }
                    }
                },
                {
                    "type": "org.kde.milou"
                },
                {
                    "type": "com.mike.desktop",
                    "config": {
                        "Pills": {
                            "pillsActiveColor": "#000000",
                            "pillsInactiveColor": "#000000",
                            "pillsInactiveOpacity": 30
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.panelspacer",
                    "config": {
                        "General": {
                            "expanding": false,
                            "length": 10
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.icontasks",
                    "config": {
                        "General": {
                            "launchers": [
                                "applications:org.kde.kdeconnect.app.desktop",
                                "applications:org.jitsi.jitsi-meet.desktop",
                                "applications:org.kde.discover.desktop",
                                "applications:onlyoffice-desktopeditors.desktop",
                                "preferred://filemanager",
                                "applications:firefox.desktop"
                            ]
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.panelspacer"
                },
                {
                    "type": "ChatAI-Plasmoid",
                    "config": {
                        "General": {
                            "customSitesJson": "[{\"name\":\"Maritalk\",\"url\":\"https://chat.maritaca.ai/\"}]"
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.systemtray",
                    "config": {
                        "General": {
                            "extraItems": [
                                "org.kde.plasma.clipboard",
                                "org.kde.plasma.bluetooth",
                                "org.kde.plasma.keyboardlayout",
                                "org.kde.plasma.vault",
                                "org.kde.plasma.cameraindicator",
                                "org.kde.plasma.manage-inputmethod",
                                "org.kde.plasma.mediacontroller",
                                "org.kde.plasma.notifications",
                                "org.kde.plasma.networkmanagement",
                                "org.kde.plasma.weather",
                                "org.kde.plasma.devicenotifier",
                                "org.kde.plasma.printmanager",
                                "org.kde.plasma.keyboardindicator",
                                "org.kde.plasma.battery",
                                "org.kde.plasma.volume",
                                "org.kde.kscreen",
                                "org.kde.kupapplet",
                                "org.kde.kdeconnect",
                                "org.kde.plasma.kclock_1x2"
                            ],
                            "hiddenItems": [
                                "org.kde.kscreen"
                            ]
                        }
                    }
                },
                {
                    "type": "KdeControlStation",
                    "config": {
                        "Appearance": {
                            "darkGlobalTheme": "Blue-Dark-1",
                            "lightGlobalTheme": "Blue-Light-1",
                            "preferChangeGlobalTheme": true,
                            "scale": 95,
                            "showBorders": false,
                            "showMediaPlayer": false,
                            "showPercentage": true,
                            "transparency": true,
                            "transparencyLevel": 50
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.digitalclock",
                    "config": {
                        "Appearance": {
                            "autoFontAndSize": false,
                            "boldText": true,
                            "dateDisplayFormat": "BelowTime",
                            "fontFamily": "Adwaita Mono",
                            "fontSize": 12,
                            "fontStyleName": "Bold",
                            "fontWeight": 700,
                            "use24hFormat": 2
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.powershowdesktop"
                },
                {
                    "type": "org.kde.plasma.panelspacer",
                    "config": {
                        "General": {
                            "expanding": false,
                            "length": 5
                        }
                    }
                }
            ]
        }
    ],
    "minimal": [
        {
            "role": "bar",
            "widgets": [
                {
                    "type": "org.kde.plasma.panelspacer",
                    "config": {
                        "General": {
                            "expanding": false,
                            "length": 5
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.kickoff",
                    "config": {
                        "General": {
                            "activationIndicator": false,
                            "appNameFormat": 0,
                            "applicationsDisplay": 0,
                            "customButtonImage": "start-here-hexabee",
                            "customGreeting": "Breathe. Focus. Start.",
                            "displayPosition": 2,
                            "floating": true,
                            "glowColor": 1,
                            "icon": "kde",
                            "launcherPosition": 1,
                            "offsetY": 10,
                            "systemFavorites": [
                                "suspend",
                                "hibernate",
                                "reboot",
                                "shutdown"
                            ],
                            "useCustomButtonImage": true,
                            "useSystemFontSettings": true
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.panelspacer",
                    "config": {
                        "General": {
                            "expanding": false,
                            "length": 10
                        }
                    }
                },
                {
                    "type": "org.kde.milou"
                },
                {
                    "type": "com.mike.desktop",
                    "config": {
                        "Pills": {
                            "pillsActiveColor": "#000000",
                            "pillsInactiveColor": "#000000",
                            "pillsInactiveOpacity": 30
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.icontasks",
                    "config": {
                        "General": {
                            "launchers": [
                                "applications:org.kde.kdeconnect.app.desktop",
                                "applications:org.jitsi.jitsi-meet.desktop",
                                "applications:onlyoffice-desktopeditors.desktop",
                                "preferred://filemanager",
                                "applications:firefox.desktop"
                            ]
                        }
                    }
                },
                {
                    "type": "ChatAI-Plasmoid",
                    "config": {
                        "General": {
                            "customSitesJson": "[{\"name\":\"Maritalk\",\"url\":\"https://chat.maritaca.ai/\"}]"
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.systemtray",
                    "config": {
                        "General": {
                            "extraItems": [
                                "org.kde.plasma.clipboard",
                                "org.kde.plasma.bluetooth",
                                "org.kde.plasma.keyboardlayout",
                                "org.kde.plasma.vault",
                                "org.kde.plasma.cameraindicator",
                                "org.kde.plasma.manage-inputmethod",
                                "org.kde.plasma.mediacontroller",
                                "org.kde.plasma.notifications",
                                "org.kde.plasma.networkmanagement",
                                "org.kde.plasma.weather",
                                "org.kde.plasma.devicenotifier",
                                "org.kde.plasma.printmanager",
                                "org.kde.plasma.keyboardindicator",
                                "org.kde.plasma.battery",
                                "org.kde.plasma.volume",
                                "org.kde.kscreen",
                                "org.kde.kupapplet",
                                "org.kde.kdeconnect",
                                "org.kde.plasma.kclock_1x2"
                            ],
                            "hiddenItems": [
                                "org.kde.kscreen"
                            ]
                        }
                    }
                },
                {
                    "type": "KdeControlStation",
                    "config": {
                        "Appearance": {
                            "darkGlobalTheme": "Blue-Dark-1",
                            "lightGlobalTheme": "Blue-Light-1",
                            "preferChangeGlobalTheme": true,
                            "scale": 95,
                            "showBorders": false,
                            "showMediaPlayer": false,
                            "showPercentage": true,
                            "transparency": true,
                            "transparencyLevel": 50
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.digitalclock",
                    "config": {
                        "Appearance": {
                            "autoFontAndSize": false,
                            "boldText": true,
                            "dateDisplayFormat": "BelowTime",
                            "fontFamily": "Adwaita Mono",
                            "fontSize": 12,
                            "fontStyleName": "Bold",
                            "fontWeight": 700,
                            "use24hFormat": 2
                        }
                    }
                },
                {
                    "type": "org.kde.plasma.powershowdesktop"
                },
                {
                    "type": "org.kde.plasma.panelspacer",
                    "config": {
                        "General": {
                            "expanding": false,
                            "length": 5
                        }
                    }
                }
            ]
        }
    ]
};
