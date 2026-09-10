if(VOELUND_VIEWPORT_BACKEND STREQUAL "native" AND WIN32)
    set(voelund_viewport_sources CadViewport.cpp NativeOcctWindowWin32.cpp)
elseif(VOELUND_VIEWPORT_BACKEND STREQUAL "qt-opengl")
    set(voelund_viewport_sources CadViewportQtOpenGl.cpp QtOcctGlContext.cpp)
else()
    message(
        FATAL_ERROR
        "The native viewport is verified only on Win32. Select qt-opengl for the "
        "Qt-owned OpenGL context path used by the Wayland design."
    )
endif()

list(APPEND voelund_viewport_sources
    SketchInteraction.cpp
    TransformManipulator.cpp
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
        TKOpenGl
        TKService
)

if(VOELUND_VIEWPORT_BACKEND STREQUAL "qt-opengl")
    target_compile_definitions(voelund_viewport PUBLIC VOELUND_VIEWPORT_QT_OPENGL=1)
    target_link_libraries(voelund_viewport PUBLIC Qt6::OpenGLWidgets OpenGL::GL)
endif()

add_library(Voelund::Viewport ALIAS voelund_viewport)
