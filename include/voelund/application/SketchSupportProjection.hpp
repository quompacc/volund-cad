#pragma once
#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/EdgeProvenance.hpp>

namespace voelund::application {
struct ProjectedSketchPoint final {
    document::ProjectedEdgeReference reference;
    document::SketchPoint local;
    gp_Pnt world;
};
struct SketchSupportProjection final {
    document::SketchPlane plane;
    std::vector<features::EdgeDisplaySegment> segments;
    // Fixed analytic samples, independent of display tessellation density.
    std::vector<ProjectedSketchPoint> points;
};
} // namespace voelund::application
