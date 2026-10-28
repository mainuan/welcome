#!/bin/bash
# Actual cards/modal with simulated services and no connection to the live desktop.
set -eu
runner=$1
source_dir=$2
isolated=$(mktemp -d)
trap 'rm -rf "$isolated"' EXIT
mkdir -p "$isolated/components" "$isolated/runtime" "$isolated/config" "$isolated/data" \
    "$isolated/home" "$isolated/cache" "$isolated/config-dirs"
chmod 700 "$isolated/runtime"
cp "$source_dir"/qml/components/*.qml "$isolated/components/"
cp "$source_dir"/qml/pages/*.qml "$isolated/components/"
cp "$source_dir/qml/Main.qml" "$isolated/components/WelcomeWindow.qml"
cp -R "$source_dir/qml/icons" "$isolated/components/icons"
# The runner has no compiled qrc. Resolve the exact production SVGs from the
# copied test tree, leaving every binding and layout expression unchanged.
python3 - "$isolated/components" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
for path in root.glob('*.qml'):
    path.write_text(path.read_text().replace('qrc:/icons/', (root / 'icons').as_uri() + '/'))
PY
cp "$source_dir"/rootfs/usr/share/welcome/{freeOffice,vivaldi,zenBrowser}.png "$isolated/components/"
cp "$source_dir/tests/qml/tst_Applications.qml" "$isolated/"
cp "$source_dir/tests/qml/tst_Responsive.qml" "$isolated/"
status=0
set +e
for scale in 1 1.25 1.5 2; do
    printf 'Qt scale factor: %s\n' "$scale" >> "$isolated/result.log"
    env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
        HOME="$isolated/home" XDG_CACHE_HOME="$isolated/cache" XDG_CONFIG_DIRS="$isolated/config-dirs" \
        XDG_DATA_DIRS=/usr/local/share:/usr/share \
        XDG_RUNTIME_DIR="$isolated/runtime" XDG_CONFIG_HOME="$isolated/config" XDG_DATA_HOME="$isolated/data" \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_SCALE_FACTOR="$scale" \
        "$runner" -input "$isolated" -o -,txt >> "$isolated/result.log" 2>&1
    status=$?
    if [ "$status" -ne 0 ]; then break; fi
done
set -e
cat "$isolated/result.log"
if grep -Eq 'module "(QtTest|org.kde.kirigami)" is not installed' "$isolated/result.log"; then exit 77; fi
exit "$status"
