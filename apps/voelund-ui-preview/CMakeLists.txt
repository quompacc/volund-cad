add_executable(
    voelund-ui-preview WIN32
    main.cpp
    PreviewCanvas.cpp
    PreviewCanvas.hpp
    PreviewIcons.cpp
    PreviewIcons.hpp
    SketchBackend.cpp
    SketchBackend.hpp
    PreviewStyle.cpp
    PreviewStyle.hpp
    PreviewWindow.cpp
    PreviewWindow.hpp
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
            PROPERTIES TIMEOUT 20 LABELS "freebsd;wayland;desktop;ui"
        )
    elseif(WIN32)
        add_test(NAME desktop.ui_preview_startup COMMAND voelund-ui-preview --smoke-test)
        set_tests_properties(desktop.ui_preview_startup PROPERTIES TIMEOUT 15 LABELS "desktop;ui")
    endif()
endif()
