# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

# Machine-readable scope for the project's own quality checks.
#
# Every exception needs a literal repository-relative path, a reason and the
# affected checks.  Keep frozen third-party code separate rather than hiding it
# behind a broad pattern.
set(VOELUND_QUALITY_OWNED_DIRECTORIES
    apps
    include
    src
    tests
    tools
    third_party/planegcs/compat
)

set(VOELUND_QUALITY_OWNED_FILES
    CMakeLists.txt
    .clang-format
    .clang-tidy
)

set(VOELUND_QUALITY_EXCLUDED_DIRECTORIES
    build
    .git
    artifacts
    spikes/phase-0-freecad
    third_party/planegcs/upstream
)

# Entries use: repository-relative-path | reason | checks.
set(VOELUND_QUALITY_EXCEPTIONS)
