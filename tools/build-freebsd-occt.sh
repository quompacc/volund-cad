#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

set -eu

if [ "$(uname -s)" != "FreeBSD" ]; then
    echo "This OCCT build is intended for FreeBSD." >&2
    exit 1
fi

exec sh "$(dirname -- "$0")/build-lean-occt.sh" "$@"
