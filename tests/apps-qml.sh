#!/bin/bash
# Actual cards/modal with simulated services and no connection to the live desktop.
set -eu
runner=$1
source_dir=$2
isolated=$(mktemp -d)
trap 'rm -rf "$isolated"' EXIT
mkdir -p "$isolated/components" "$isolated/runtime" "$isolated/config" "$isolated/data"
chmod 700 "$isolated/runtime"
cp "$source_dir"/qml/components/*.qml "$isolated/components/"
cp "$source_dir/qml/pages/AppsPage.qml" "$isolated/components/"
cp "$source_dir"/rootfs/usr/share/welcome/{freeOffice,vivaldi,zenBrowser}.png "$isolated/components/"
cp "$source_dir/tests/qml/tst_Applications.qml" "$isolated/"
set +e
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
    XDG_RUNTIME_DIR="$isolated/runtime" XDG_CONFIG_HOME="$isolated/config" XDG_DATA_HOME="$isolated/data" \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    "$runner" -input "$isolated" -o -,txt > "$isolated/result.log" 2>&1
status=$?
set -e
cat "$isolated/result.log"
if grep -Eq 'module "(QtTest|org.kde.kirigami)" is not installed' "$isolated/result.log"; then exit 77; fi
exit "$status"
