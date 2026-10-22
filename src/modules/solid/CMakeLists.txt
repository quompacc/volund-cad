# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

add_library(voelund_module_solid STATIC SolidWorkspaceModule.cpp)

target_include_directories(voelund_module_solid PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(
    voelund_module_solid
    PUBLIC
        voelund_project_options
        Voelund::Workspaces
)

add_library(Voelund::ModuleSolid ALIAS voelund_module_solid)
