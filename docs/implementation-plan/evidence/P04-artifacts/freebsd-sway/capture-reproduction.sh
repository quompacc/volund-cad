#!/bin/sh
set -eu
cd /home/eduard/src/voelund-p04-20260913
export XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland
export SWAYSOCK=/var/run/xdg/eduard/sway-ipc.1001.2262.sock
unset DISPLAY
build/freebsd-clang-lean-occt/apps/voelund-ui-preview/voelund-ui-preview >p04-artifacts/compositor-app.log 2>&1 &
app_pid=$!
trap 'kill "$app_pid" 2>/dev/null || true' EXIT HUP INT TERM
sleep 2
swaymsg "[pid=$app_pid] floating enable" >p04-artifacts/resize-command.json
swaymsg "[pid=$app_pid] resize set width 1200 px height 800 px" >>p04-artifacts/resize-command.json
sleep 1
grim p04-artifacts/sway-resized.png
swaymsg "[pid=$app_pid] fullscreen enable" >p04-artifacts/fullscreen-command.json
sleep 1
grim p04-artifacts/sway-fullscreen.png
swaymsg -t get_outputs >p04-artifacts/outputs.json
swaymsg -t get_version >p04-artifacts/compositor.json
kill "$app_pid"
wait "$app_pid" || true
trap - EXIT HUP INT TERM
ctest --test-dir build/freebsd-clang-lean-occt --output-on-failure --output-junit /home/eduard/src/voelund-p04-20260913/p04-artifacts/ctest-final.xml >p04-artifacts/ctest-final.log 2>&1