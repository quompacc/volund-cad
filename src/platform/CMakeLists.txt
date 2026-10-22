# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

add_library(voelund_platform STATIC AtomicFileStore.cpp AtomicFileStoreNative.cpp RandomBytes.cpp)
target_include_directories(voelund_platform PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(voelund_platform PUBLIC voelund_project_options)
if(WIN32)
    target_link_libraries(voelund_platform PRIVATE bcrypt)
endif()
add_library(Voelund::Platform ALIAS voelund_platform)
