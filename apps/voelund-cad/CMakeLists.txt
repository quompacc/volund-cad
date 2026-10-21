add_executable(voelund-cad WIN32 main.cpp CommandLine.cpp)
if(WIN32)
    # N16: the programme symbol in the executable (Explorer, task bar, default window icon).
    target_sources(voelund-cad PRIVATE voelund-cad.rc)
endif()

target_link_libraries(
    voelund-cad
    PRIVATE
        voelund_project_options
        Voelund::Application
        Voelund::Desktop
        voelund_build_info_headers
)
voelund_deploy_planegcs(voelund-cad)

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
# libraries (project-local build) and PlaneGCS (L04) go beside it in the library directory;
# Qt and the rest come from the system. On Windows it is exactly the set beside the build's
# executable: Qt, OpenCASCADE, FreeType and their dependencies, PlaneGCS, and the platform
# plugin.
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
elseif(WIN32)
    # L02: exactly the DLLs the executable's dependency graph resolves (Qt, OpenCASCADE,
    # FreeType and theirs) from the build output, which the post-build steps fill; system DLLs
    # stay out. The platform plugin is loaded at run time and installed by name, the MSVC
    # runtime by CMake's InstallRequiredSystemLibraries.
    install(TARGETS voelund-cad
        RUNTIME_DEPENDENCY_SET voelund_windows_runtime
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(RUNTIME_DEPENDENCY_SET voelund_windows_runtime
        DIRECTORIES "$<TARGET_FILE_DIR:voelund-cad>"
        PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
        POST_EXCLUDE_REGEXES "^[A-Za-z]:[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\]"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
        DESTINATION "${CMAKE_INSTALL_BINDIR}/platforms")
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
    include(InstallRequiredSystemLibraries)
else()
    install(TARGETS voelund-cad RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
# L04: PlaneGCS is a target of this project, which dependency sets leave out; the import
# library is not shipped.
install(TARGETS voelund_planegcs
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" EXCLUDE_FROM_ALL)

if(UNIX AND NOT APPLE)
    install(
        FILES "${PROJECT_SOURCE_DIR}/packaging/unix/io.github.quompacc.Voelund.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/packaging/unix/io.github.quompacc.Voelund.xml"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages"
    )
    # N16: the programme symbol under the desktop entry's name.
    install(
        FILES "${PROJECT_SOURCE_DIR}/packaging/icons/voelund-cad.svg"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
        RENAME io.github.quompacc.Voelund.svg
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/packaging/icons/voelund-cad-256.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
        RENAME io.github.quompacc.Voelund.png
    )
    # L02: per-user registration of the unpacked tarball (desktop entry and MIME type).
    install(
        PROGRAMS "${PROJECT_SOURCE_DIR}/packaging/unix/install-user.sh"
            "${PROJECT_SOURCE_DIR}/packaging/unix/uninstall-user.sh"
        DESTINATION .
    )
endif()
if(WIN32)
    # L02: per-user .volund association of the portable folder (HKCU, no administrator).
    install(
        FILES "${PROJECT_SOURCE_DIR}/packaging/windows/register-file-type.ps1"
            "${PROJECT_SOURCE_DIR}/packaging/windows/unregister-file-type.ps1"
        DESTINATION .
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

        # L02: the tarball registered per user opens a document through its association.
        if(VOELUND_NATIVE_WAYLAND)
            add_test(NAME desktop.file_association
                COMMAND "${CMAKE_COMMAND}"
                    "-DVOELUND_BUILD_DIR=${PROJECT_BINARY_DIR}"
                    "-DVOELUND_TEST_ROOT=${PROJECT_BINARY_DIR}/file-association-test"
                    "-DVOELUND_FIXTURE=${file_argument_fixture}"
                    -P "${PROJECT_SOURCE_DIR}/tests/cmake/FileAssociationTests.cmake")
            set_tests_properties(desktop.file_association PROPERTIES
                TIMEOUT 180 RUN_SERIAL TRUE LABELS "desktop;platform;package;acceptance")
        endif()
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
