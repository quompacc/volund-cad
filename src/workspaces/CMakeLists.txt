# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

add_library(voelund_workspaces STATIC CommandRegistry.cpp)

target_include_directories(voelund_workspaces PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(voelund_workspaces PUBLIC voelund_project_options)

add_library(Voelund::Workspaces ALIAS voelund_workspaces)
