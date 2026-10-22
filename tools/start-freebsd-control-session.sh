#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

set -eu

if [ "$(uname -s)" != "FreeBSD" ]; then
    echo "This control session launcher is intended for FreeBSD." >&2
    exit 1
fi

if [ -z "${XDG_RUNTIME_DIR:-}" ]; then
    runtime_dir="/tmp/xdg-runtime-$(id -u)"
    if [ -e "$runtime_dir" ]; then
        if [ ! -d "$runtime_dir" ] || [ "$(stat -f %u "$runtime_dir")" != "$(id -u)" ]; then
            echo "Unsafe runtime path: $runtime_dir" >&2
            exit 1
        fi
    else
        mkdir -m 700 "$runtime_dir"
    fi
    chmod 700 "$runtime_dir"
    export XDG_RUNTIME_DIR="$runtime_dir"
fi

export XDG_CURRENT_DESKTOP=sway
export XDG_SESSION_DESKTOP=sway
export XDG_SESSION_TYPE=wayland

exec dbus-run-session sway
