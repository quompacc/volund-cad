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
            "$<TARGET_FILE:Qt6::Network>"
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

# U12: the installed product carries the runtime the tests use. On Linux the OpenCASCADE
# libraries (project-local build) go beside it in the library directory; Qt and the rest
# come from the system. On Windows it is exactly the set beside the build's executable: Qt,
# OpenCASCADE, FreeType and their dependencies, and the platform plugin.
if(UNIX AND NOT APPLE)
    set_target_properties(voelund-cad PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    install(TARGETS voelund-cad
        RUNTIME_DEPENDENCY_SET voelund_occt_runtime
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(RUNTIME_DEPENDENCY_SET voelund_occt_runtime
        POST_INCLUDE_REGEXES "libTK[A-Za-z0-9]+\\.so"
        POST_EXCLUDE_REGEXES ".*"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
else()
    install(TARGETS voelund-cad RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
if(WIN32)
    install(
        DIRECTORY "$<TARGET_FILE_DIR:voelund-cad>/"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        FILES_MATCHING
        PATTERN "*.dll"
        PATTERN "CMakeFiles" EXCLUDE
    )
endif()

if(UNIX AND NOT APPLE)
    install(
        FILES "${PROJECT_SOURCE_DIR}/packaging/unix/io.github.quompacc.Voelund.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/packaging/unix/io.github.quompacc.Voelund.xml"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages"
    )
endif()

if(BUILD_TESTING AND VOELUND_BUILD_DESKTOP)
    if(VOELUND_NATIVE_WAYLAND)
        add_test(
            NAME desktop.viewport_startup.native_wayland
            COMMAND
                /usr/bin/env -u DISPLAY QT_QPA_PLATFORM=wayland
                $<TARGET_FILE:voelund-cad> --smoke-test
        )
        set_tests_properties(
            desktop.viewport_startup.native_wayland
            PROPERTIES TIMEOUT 20 LABELS "${VOELUND_PLATFORM_LABEL};wayland;desktop;platform"
        )
    elseif(WIN32)
        add_test(NAME desktop.viewport_startup COMMAND voelund-cad --smoke-test)
        set_tests_properties(desktop.viewport_startup PROPERTIES TIMEOUT 15 LABELS "windows;desktop;platform")
    endif()
    # U11 (A-UI-03): the product opens a document given as its argument (file association).
    set(file_argument_fixture "${PROJECT_SOURCE_DIR}/tests/fixtures/documents/v09.volund")
    if(VOELUND_NATIVE_WAYLAND)
        add_test(NAME desktop.file_argument
            COMMAND /usr/bin/env -u DISPLAY QT_QPA_PLATFORM=wayland
                $<TARGET_FILE:voelund-cad> --smoke-test-open "${file_argument_fixture}")
    elseif(WIN32)
        add_test(NAME desktop.file_argument
            COMMAND voelund-cad --smoke-test-open "${file_argument_fixture}")
    endif()
    if(VOELUND_NATIVE_WAYLAND OR WIN32)
        set_tests_properties(desktop.file_argument PROPERTIES
            TIMEOUT 30 LABELS "desktop;platform;acceptance")
        # U12: the installed product, not the build tree's executable.
        add_test(NAME desktop.installed_product
            COMMAND "${CMAKE_COMMAND}"
                "-DVOELUND_BUILD_DIR=${PROJECT_BINARY_DIR}"
                "-DVOELUND_INSTALL_DIR=${PROJECT_BINARY_DIR}/installed-product-test"
                "-DVOELUND_CONFIG=$<CONFIG>"
                "-DVOELUND_FIXTURE=${file_argument_fixture}"
                "-DVOELUND_REQUIRE_WAYLAND=${VOELUND_NATIVE_WAYLAND}"
                -P "${PROJECT_SOURCE_DIR}/tools/cmake/RunInstalledProduct.cmake")
        set_tests_properties(desktop.installed_product PROPERTIES
            TIMEOUT 180 RUN_SERIAL TRUE LABELS "desktop;platform;acceptance")
    endif()
endif()

target_sources(voelund-cad PRIVATE "${PROJECT_SOURCE_DIR}/apps/SmokeSupport.cpp")
if(WIN32)
    target_link_libraries(voelund-cad PRIVATE opengl32)
endif()
if(BUILD_TESTING AND VOELUND_BUILD_DESKTOP AND (WIN32 OR VOELUND_NATIVE_WAYLAND))
    set(smoke_require_wayland ${VOELUND_NATIVE_WAYLAND})
    add_test(NAME desktop.viewport_startup.uninitialized
        COMMAND "${CMAKE_COMMAND}"
            "-DVOELUND_SMOKE_EXECUTABLE=$<TARGET_FILE:voelund-cad>"
            "-DVOELUND_REQUIRE_WAYLAND=${smoke_require_wayland}"
            -P "${PROJECT_SOURCE_DIR}/tools/cmake/RunExpectedSmoke.cmake")
    set_tests_properties(desktop.viewport_startup.uninitialized
        PROPERTIES TIMEOUT 15 LABELS "desktop;platform;negative;regression")
endif()
