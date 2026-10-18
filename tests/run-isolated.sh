#!/bin/bash
# One private home/runtime/bus per native suite, including under dh_auto_test.
set -euo pipefail
bus_runner=$1
bus_config=$2
shift 2
root=$(mktemp -d)
trap 'rm -rf -- "$root"' EXIT
mkdir -p "$root"/{home,config,config-dirs,data,data-dirs,cache,runtime}
chmod 700 "$root/runtime"

# Removing the address alone does not disconnect Qt from a host session: it can
# discover the socket through XDG_RUNTIME_DIR. A connected private bus is also
# needed so QDBus async errors arrive normally when Plasma is absent.
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
    HOME="$root/home" XDG_CONFIG_HOME="$root/config" XDG_CONFIG_DIRS="$root/config-dirs" \
    XDG_DATA_HOME="$root/data" XDG_DATA_DIRS="$root/data-dirs" XDG_CACHE_HOME="$root/cache" \
    XDG_RUNTIME_DIR="$root/runtime" QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    "$bus_runner" --config-file "$bus_config" -- "$@"
