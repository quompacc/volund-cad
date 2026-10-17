#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <iostream>
#include <sstream>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

namespace {
using namespace voelund;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void check(const TopoDS_Shape& shape, double size, double height, double expectedVolume) {
    require(BRepCheck_Analyzer(shape).IsValid(), "Repeated join is invalid");
    int solids = 0;
    for (TopExp_Explorer it(shape, TopAbs_SOLID); it.More(); it.Next())
        ++solids;
    require(solids == 1, "Repeated join is disconnected");
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    require(std::abs(properties.Mass() - expectedVolume) <=
                expectedVolume * kernel::modelComparisonTolerance.relative,
            "Join changed analytical volume");
    Bnd_Box bounds;
    BRepBndLib::AddOptimal(shape, bounds, false, false);
    double x0, y0, z0, x1, y1, z1;
    bounds.Get(x0, y0, z0, x1, y1, z1);
    const double tolerance = size * kernel::modelComparisonTolerance.relative;
    require(std::abs(x0) <= tolerance && std::abs(y0) <= tolerance && std::abs(z0) <= tolerance &&
                std::abs(x1 - size) <= tolerance && std::abs(y1 - size) <= tolerance &&
                std::abs(z1 - height) <= tolerance,
            "Join changed requested bounds");
}
void repeatedJoins() {
    for (double size : {0.01, 10.0, 100000.0}) {
        auto doc = document::ModelDocument::empty();
        auto support = doc.addBoxFeature("Support", {size, size, size});
        const document::BodyId bodyId{support.value()};
        const double distance = size / 10;
        double height = size;
        double volume = size * size * size;
        for (int step = 0; step < 4; ++step) {
            const document::SketchParameters sketch{
                {0, 0, height, 0, 0, 1, 1, 0, 0},
                {{{document::SketchElementId{1},
                   document::SketchRectangle{size / 4, size / 4, size * 3 / 4, size * 3 / 4}}},
                 2}};
            const auto sketchId = doc.addSketchFeature("Profile", support, sketch);
            support = doc.addExtrusionFeature("Join", sketchId, {distance, {}});
            height += distance;
            volume += size * size * distance / 4;
            const auto result = features::FeatureGraph{}.evaluate(doc);
            require(result.bodies.size() == 1 && result.bodies.front().bodyId == bodyId &&
                        result.bodies.front().sourceFeature == support &&
                        result.features.back().bodyId == bodyId &&
                        !result.features[result.features.size() - 2].bodyId,
                    "Extrusion changed body identity or assigned a solid body to its sketch");
            check(result.resultShape, size, height, volume);
            require(doc.extrusionParameters(support).distance == distance,
                    "Join changed stored extrusion length");
            std::ostringstream bytes;
            document::DocumentSerializer::write(doc, bytes);
            std::istringstream input(bytes.str());
            const auto loaded = document::DocumentSerializer::read(input);
            const auto loadedResult = features::FeatureGraph{}.evaluate(loaded);
            require(loadedResult.bodies.front().bodyId == bodyId,
                    "Extrusion body identity changed after save/open");
            check(loadedResult.resultShape, size, height, volume);
            require(doc.undo() && doc.redo(), "Join history failed");
            check(features::FeatureGraph{}.evaluate(doc).resultShape, size, height, volume);
        }
    }
}
} // namespace
int main() {
    try {
        repeatedJoins();
        std::cout << "PASS exact extrusion bounds, repeated joins, volume, roundtrip and history\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
