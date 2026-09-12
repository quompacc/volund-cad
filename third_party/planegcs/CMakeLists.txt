set(planegcs_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/upstream/src/Mod/Sketcher/App/planegcs")
find_package(Threads REQUIRED)

add_library(
    voelund_planegcs STATIC
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

if(MSVC)
    target_compile_options(voelund_planegcs PRIVATE /wd4244 /wd4267 /wd4456 /wd4457 /wd4701)
endif()

add_library(Voelund::PlaneGCS ALIAS voelund_planegcs)
