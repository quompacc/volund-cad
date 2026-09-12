add_library(
    voelund_features STATIC
    BoxFeature.cpp CylinderFeature.cpp FeatureGraph.cpp SketchProfiles.cpp)

target_include_directories(
    voelund_features
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
    voelund_features
    PUBLIC
        voelund_project_options
        Voelund::Document
        Voelund::Kernel
        TKTopAlgo
)

add_library(Voelund::Features ALIAS voelund_features)
