set(planegcs_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/upstream/src/Mod/Sketcher/App/planegcs")
find_package(Threads REQUIRED)

add_library(
    voelund_planegcs SHARED
    "${planegcs_source_dir}/Constraints.cpp"
    "${planegcs_source_dir}/GCS.cpp"
    "${planegcs_source_dir}/Geo.cpp"
    "${planegcs_source_dir}/qp_eq.cpp"
    "${planegcs_source_dir}/SubSystem.cpp"
)

target_compile_features(voelund_planegcs PUBLIC cxx_std_20)
target_include_directories(
    voelund_planegcs
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/upstream/src"
)
target_include_directories(
    voelund_planegcs
    SYSTEM PUBLIC
        "${planegcs_source_dir}"
        "${CMAKE_CURRENT_SOURCE_DIR}/compat"
)
target_link_libraries(
    voelund_planegcs
    PUBLIC
        Eigen3::Eigen
        Boost::graph
        Threads::Threads
)

# L04 (user decision 2026-10-18): a shared library, replaceable under LGPL-2.1. Building it
# defines Sketcher_EXPORTS, so SketcherGlobal.h exports the solver classes (compat/FCGlobal.h).
target_compile_definitions(voelund_planegcs PRIVATE Sketcher_EXPORTS)

if(MSVC)
    # C4251/C4275: exported solver classes hold standard-library and Eigen members.
    target_compile_options(voelund_planegcs PRIVATE /wd4244 /wd4267 /wd4456 /wd4457 /wd4701
        /wd4251 /wd4275)
endif()

if(WIN32)
    # Windows finds a DLL beside the executable: the test executables share the tests
    # directory; other executables copy the library after linking (voelund_deploy_planegcs).
    set_target_properties(voelund_planegcs PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tests")
endif()
function(voelund_deploy_planegcs target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:voelund_planegcs>"
                "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endif()
endfunction()

add_library(Voelund::PlaneGCS ALIAS voelund_planegcs)
