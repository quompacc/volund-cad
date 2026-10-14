add_library(
    voelund_features STATIC
    BoxFeature.cpp CylinderFeature.cpp FeatureDependencies.cpp FeatureGraph.cpp SupportProvenance.cpp EdgeProvenance.cpp SketchConstraintAnnotations.cpp
    SketchConstraintSolver.cpp SketchEvaluation.cpp SketchProfiles.cpp SketchContourGraph.cpp SketchIntersections.cpp SketchRegions.cpp SketchRegionGeometry.cpp SketchRegionIdentity.cpp SketchRegionFaces.cpp SketchRegionIntervalFaces.cpp SketchIntersectionCurves.cpp SketchContourGeometry.cpp SketchSupportResolver.cpp SketchSupportBinding.cpp ExtrusionEvaluation.cpp FaceExtrusionEvaluation.cpp FilletEvaluation.cpp HoleEvaluation.cpp CombineEvaluation.cpp DatumEvaluation.cpp ShellEvaluation.cpp ContourProvenance.cpp SketchReferenceReview.cpp)

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
        Voelund::PlaneGCS
        TKTopAlgo
)

add_library(Voelund::Features ALIAS voelund_features)
