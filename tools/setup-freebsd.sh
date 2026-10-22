#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

for command_name in clang++ cmake ninja pkgconf; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "Required command is missing: $command_name" >&2
        exit 1
    fi
done

for package_name in qt6-base opencascade; do
    if ! pkg info -e "$package_name" >/dev/null 2>&1; then
        echo "Required package is missing: $package_name" >&2
        exit 1
    fi
done

cd "$repository_root"
cmake --preset freebsd-clang
