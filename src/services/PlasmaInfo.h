#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace PlasmaInfo {

// Version of the running Plasma shell ("6.6.6"), read asynchronously from the
// applicationVersion property plasmashell publishes on D-Bus. Starting
// `plasmashell --version` instead launches a second shell, which crashes when
// it inherits an offscreen or headless environment (tests, package builds).
// `done` receives an empty string when Plasma is not running; it is not called
// once `context` is destroyed.
void queryVersion(QObject *context, std::function<void(const QString &version)> done);

}
