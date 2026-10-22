# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

# AI04: MCP access to the action layer (docs/implementation-plan/AI04-plan.md).
add_library(voelund_mcp STATIC
    ActionDecoding.cpp
    FaceJson.cpp
    McpHttpServer.cpp
    McpProtocol.cpp
    ResultEncoding.cpp
    SketchJson.cpp
    ToolCatalog.cpp
    ToolHost.cpp
)

target_include_directories(
    voelund_mcp
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
    voelund_mcp
    PUBLIC
        voelund_project_options
        Voelund::Application
        Qt6::Core
        Qt6::Network
)

add_library(Voelund::Mcp ALIAS voelund_mcp)
