add_executable(voelund-cad WIN32 main.cpp)

target_link_libraries(
    voelund-cad
    PRIVATE
        voelund_project_options
        Voelund::Application
        Voelund::Desktop
)

if(WIN32)
    add_custom_command(
        TARGET voelund-cad
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::Core>"
            "$<TARGET_FILE:Qt6::Gui>"
            "$<TARGET_FILE:Qt6::Widgets>"
            "$<TARGET_FILE_DIR:voelund-cad>"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:voelund-cad>/platforms"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:voelund-cad>/platforms"
        COMMENT "Deploying the minimal Qt runtime"
        VERBATIM
    )

    if(VOELUND_VIEWPORT_BACKEND STREQUAL "qt-opengl")
        add_custom_command(
            TARGET voelund-cad
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::OpenGL>"
                "$<TARGET_FILE:Qt6::OpenGLWidgets>"
                "$<TARGET_FILE_DIR:voelund-cad>"
            COMMENT "Deploying the Qt OpenGL runtime"
            VERBATIM
        )
    endif()
endif()

if(BUILD_TESTING AND WIN32)
    add_test(NAME desktop.viewport_startup COMMAND voelund-cad --smoke-test)
    set_tests_properties(desktop.viewport_startup PROPERTIES TIMEOUT 15)
endif()
