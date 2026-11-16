#!/bin/bash
# Renders a real component with the KI18n runner under one language.
# Usage: i18n-qml.sh <runner> <source dir> <locale dir> <pt_BR|en>
set -eu
runner=$1
source_dir=$2
locale_dir=$3
language=$4
isolated=$(mktemp -d)
trap 'rm -rf "$isolated"' EXIT
mkdir -p "$isolated/components" "$isolated/runtime" "$isolated/config" "$isolated/data"
chmod 700 "$isolated/runtime"
cp "$source_dir"/qml/components/*.qml "$isolated/components/"
cp "$source_dir/tests/qml/tst_I18n.qml" "$isolated/"
set +e
# glibc ignores LANGUAGE under the C locale: run in a Brazilian session locale
# (Build-Depends: locales-all); LANGUAGE=en must still select English.
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
    LC_ALL=pt_BR.UTF-8 LANGUAGE="$language" MAINUAN_EXPECT_LANGUAGE="$language" MAINUAN_LOCALE_DIR="$locale_dir" \
    XDG_RUNTIME_DIR="$isolated/runtime" XDG_CONFIG_HOME="$isolated/config" \
    XDG_DATA_HOME="$isolated/data" QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    "$runner" -input "$isolated" -o -,txt > "$isolated/result.log" 2>&1
status=$?
set -e
cat "$isolated/result.log"
if grep -Eq 'module "(QtTest|org.kde.kirigami)" is not installed' "$isolated/result.log"; then
    exit 77
fi
exit "$status"
