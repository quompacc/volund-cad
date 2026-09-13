add_executable(
    voelund-ui-preview WIN32
    main.cpp
    PreviewCanvas.cpp
    PreviewCanvas.hpp
    PreviewIcons.cpp
    PreviewIcons.hpp
    SketchBackend.cpp
    SketchBackend.hpp
    ViewCube.cpp
    ViewCube.hpp
    PreviewStyle.cpp
    PreviewStyle.hpp
    PreviewWindow.cpp
    PreviewWindow.hpp
    PreviewWindowRibbon.cpp
    PreviewWindowSketch.cpp
)

set_target_properties(voelund-ui-preview PROPERTIES AUTOMOC ON)

target_link_libraries(
    voelund-ui-preview
    PRIVATE
        voelund_project_options
        Voelund::Application
        Voelund::Viewport
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

if(WIN32)
    add_custom_command(
        TARGET voelund-ui-preview
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::Core>"
            "$<TARGET_FILE:Qt6::Gui>"
            "$<TARGET_FILE:Qt6::Widgets>"
            "$<TARGET_FILE_DIR:voelund-ui-preview>"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:voelund-ui-preview>/platforms"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:voelund-ui-preview>/platforms"
        COMMENT "Deploying the Qt runtime for the UI preview"
        VERBATIM
    )

    if(VOELUND_VIEWPORT_BACKEND STREQUAL "qt-opengl")
        add_custom_command(
            TARGET voelund-ui-preview
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::OpenGL>"
                "$<TARGET_FILE:Qt6::OpenGLWidgets>"
                "$<TARGET_FILE_DIR:voelund-ui-preview>"
            COMMENT "Deploying the Qt OpenGL runtime for the UI preview"
            VERBATIM
        )
    endif()
endif()

if(BUILD_TESTING)
    if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        add_test(
            NAME desktop.ui_preview_startup.native_wayland
            COMMAND
                /usr/bin/env -u DISPLAY QT_QPA_PLATFORM=wayland
                $<TARGET_FILE:voelund-ui-preview> --smoke-test
        )
        set_tests_properties(
            desktop.ui_preview_startup.native_wayland
            PROPERTIES TIMEOUT 20 LABELS "freebsd;wayland;desktop;ui;platform"
        )
    elseif(WIN32)
        add_test(NAME desktop.ui_preview_startup COMMAND voelund-ui-preview --smoke-test)
        set_tests_properties(desktop.ui_preview_startup PROPERTIES TIMEOUT 15 LABELS "windows;desktop;ui;platform")
    endif()
endif()

target_sources(voelund-ui-preview PRIVATE "${PROJECT_SOURCE_DIR}/apps/SmokeSupport.cpp")
if(WIN32)
    target_link_libraries(voelund-ui-preview PRIVATE opengl32)
endif()
if(BUILD_TESTING AND VOELUND_BUILD_DESKTOP AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD"))
    set(smoke_require_wayland OFF)
    if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        set(smoke_require_wayland ON)
    endif()
    add_test(NAME desktop.ui_preview_startup.uninitialized
        COMMAND "${CMAKE_COMMAND}"
            "-DVOELUND_SMOKE_EXECUTABLE=$<TARGET_FILE:voelund-ui-preview>"
            "-DVOELUND_REQUIRE_WAYLAND=${smoke_require_wayland}"
            -P "${PROJECT_SOURCE_DIR}/tools/cmake/RunExpectedSmoke.cmake")
    set_tests_properties(desktop.ui_preview_startup.uninitialized
        PROPERTIES TIMEOUT 15 LABELS "desktop;platform;negative;regression")
endif()
