# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

if(VOELUND_VIEWPORT_BACKEND STREQUAL "native" AND WIN32)
    set(voelund_viewport_sources
        CadViewport.cpp
        CadViewportInput.cpp
        CadViewportNativeCallbacks.cpp
        NativeOcctWindowWin32.cpp
    )
elseif(VOELUND_VIEWPORT_BACKEND STREQUAL "qt-opengl")
    set(voelund_viewport_sources
        CadViewport.cpp
        CadViewportInput.cpp
        CadViewportQtOpenGl.cpp
        QtOcctGlContext.cpp
    )
else()
    message(
        FATAL_ERROR
        "The native viewport is verified only on Win32. Select qt-opengl for the "
        "Qt-owned OpenGL context path used by the Wayland design."
    )
endif()

list(APPEND voelund_viewport_sources
    BasePlanePresentation.cpp
    DatumPresentation.cpp
    CadViewportHandlers.cpp
    SketchEditorInteraction.cpp
    PlanarFaceSelection.cpp
    SketchSnapOverlay.cpp
    SketchViewOrientation.cpp
    SketchConstraintPresentation.cpp
    SketchBoxSelection.cpp
    SelectionBand.cpp
    SketchPresentation.cpp
    TransformManipulator.cpp
    PushPullHandle.cpp
    ViewportHighlightStyle.cpp
)

add_library(voelund_viewport STATIC ${voelund_viewport_sources})

target_include_directories(
    voelund_viewport
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
    voelund_viewport
    PUBLIC
        voelund_project_options
        Voelund::Kernel
        Qt6::Widgets
        TKV3d
        TKService
)

if(VOELUND_NATIVE_WAYLAND AND VOELUND_VIEWPORT_BACKEND STREQUAL "qt-opengl")
    target_compile_definitions(voelund_viewport PUBLIC HAVE_GLES2=1 VOELUND_OCCT_GLES=1)
    target_link_libraries(voelund_viewport PUBLIC TKOpenGles OpenGL::EGL)
else()
    target_link_libraries(voelund_viewport PUBLIC TKOpenGl)
endif()

if(VOELUND_VIEWPORT_BACKEND STREQUAL "qt-opengl")
    target_compile_definitions(voelund_viewport PUBLIC VOELUND_VIEWPORT_QT_OPENGL=1)
    target_link_libraries(voelund_viewport PUBLIC Qt6::OpenGLWidgets)
    if(WIN32)
        target_link_libraries(voelund_viewport PUBLIC OpenGL::GL)
    endif()
endif()

add_library(Voelund::Viewport ALIAS voelund_viewport)
