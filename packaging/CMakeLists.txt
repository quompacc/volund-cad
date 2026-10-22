# L04: what the package carries besides the program: THIRD-PARTY-NOTICES.md, the licence
# texts of every shipped or embedded component (licenses/, provenance in licenses/README.md),
# the PlaneGCS source (user decision 2026-10-18: PlaneGCS as a shared library, its source in
# the package) and, written last from the staged files, MANIFEST.txt (Components.cmake).

# Versions of this build for the manifest.
function(voelund_vcpkg_port_version port out)
    set(version "")
    set(spdx "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/${port}/vcpkg.spdx.json")
    if(DEFINED VCPKG_INSTALLED_DIR AND EXISTS "${spdx}")
        file(READ "${spdx}" text)
        if(text MATCHES "\"versionInfo\": *\"([^\"]+)\"")
            set(version "${CMAKE_MATCH_1}")
        endif()
    endif()
    set(${out} "${version}" PARENT_SCOPE)
endfunction()
set(occt_version "${OpenCASCADE_VERSION}")
if(occt_version STREQUAL "")
    set(occt_version
        "${OpenCASCADE_MAJOR_VERSION}.${OpenCASCADE_MINOR_VERSION}.${OpenCASCADE_MAINTENANCE_VERSION}")
endif()
set(versions "")
foreach(entry
        "VOELUND_VERSION=${PROJECT_VERSION}"
        "VOELUND_OCCT_VERSION=${occt_version}"
        "VOELUND_QT_VERSION=${Qt6_VERSION}"
        "VOELUND_EIGEN_VERSION=${Eigen3_VERSION}"
        "VOELUND_BOOST_VERSION=${Boost_VERSION}"
        "VOELUND_MSVC_VERSION=${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    string(REPLACE "=" ";" pair "${entry}")
    list(GET pair 0 name)
    list(GET pair 1 value)
    string(APPEND versions "set(${name} \"${value}\")\n")
endforeach()
foreach(port freetype zlib libpng bzip2 brotli)
    voelund_vcpkg_port_version(${port} version)
    string(TOUPPER "${port}" upper)
    string(APPEND versions "set(VOELUND_${upper}_VERSION \"${version}\")\n")
endforeach()
set(VOELUND_COMPONENT_VERSIONS "${PROJECT_BINARY_DIR}/packaging/component-versions.cmake"
    CACHE INTERNAL "L04: configured component versions")
file(CONFIGURE OUTPUT "${VOELUND_COMPONENT_VERSIONS}" CONTENT "${versions}")
set(VOELUND_COMPONENTS "${CMAKE_CURRENT_SOURCE_DIR}/Components.cmake"
    CACHE INTERNAL "L04: package components")

set(licenses "${CMAKE_CURRENT_SOURCE_DIR}/licenses")
install(FILES THIRD-PARTY-NOTICES.md DESTINATION .)
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION licenses/voelund) # GPL-3.0-or-later
install(DIRECTORY "${licenses}/opencascade" "${licenses}/eigen" "${licenses}/boost"
    "${licenses}/occt-samples-qt"
    DESTINATION licenses)
install(FILES "${PROJECT_SOURCE_DIR}/resources/fonts/AlbertSans-OFL.txt"
    "${PROJECT_SOURCE_DIR}/resources/fonts/JetBrainsMono-OFL.txt"
    DESTINATION licenses/fonts)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/planegcs/COPYING.LIB"
    DESTINATION licenses/planegcs)
if(WIN32)
    # Shipped only on Windows; on Linux Qt and FreeType come from the system.
    install(DIRECTORY "${licenses}/qt" "${licenses}/freetype" "${licenses}/zlib"
        "${licenses}/libpng" "${licenses}/bzip2" "${licenses}/brotli"
        DESTINATION licenses)
endif()
install(DIRECTORY "${PROJECT_SOURCE_DIR}/third_party/planegcs/" DESTINATION source/planegcs)
install(FILES planegcs-source-README.md DESTINATION source/planegcs RENAME README-Voelund.md)

# Last: the manifest of what was staged (CMP0082 keeps this after the other directories).
install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\"
    \"-DVOELUND_PREFIX=\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\"
    \"-DVOELUND_COMPONENTS=${VOELUND_COMPONENTS}\"
    \"-DVOELUND_COMPONENT_VERSIONS=${VOELUND_COMPONENT_VERSIONS}\"
    -P \"${PROJECT_SOURCE_DIR}/tools/cmake/WritePackageManifest.cmake\"
    COMMAND_ERROR_IS_FATAL ANY)")
