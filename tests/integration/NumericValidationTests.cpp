#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <array>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/SketchPlaneValidation.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

namespace {
using namespace voelund;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::string serialized(const document::ModelDocument& doc) {
    std::ostringstream output;
    document::DocumentSerializer::write(doc, output);
    return output.str();
}
std::string legacy(const document::SketchPlane& plane) {
    std::ostringstream output;
    output << std::setprecision(17)
           << "VOLUND 1\nNEXT_ID 3\nFEATURES 2\nBOX 1 \"Support\" 20 20 30 0 0 0 0 0 0\n"
           << "SKETCH 2 \"Sketch\" 1 " << plane.originX << ' ' << plane.originY << ' '
           << plane.originZ << ' ' << plane.normalX << ' ' << plane.normalY << ' ' << plane.normalZ
           << ' ' << plane.xDirectionX << ' ' << plane.xDirectionY << ' ' << plane.xDirectionZ
           << " 1 2 6 8\n";
    return output.str();
}
document::SketchParameters sketch(const document::SketchPlane& plane) {
    return {
        plane, {{{document::SketchElementId{1}, document::SketchRectangle{1, 2, 6, 8}}}, 2}, {}};
}
void rejectedPlane(const document::SketchPlane& plane) {
    document::ModelDocument doc;
    const auto before = document::DocumentSnapshot::capture(doc);
    const auto revision = doc.revision();
    bool rejected = false;
    try {
        static_cast<void>(doc.addSketchFeature("Invalid", document::FeatureId{1}, sketch(plane)));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && document::DocumentSnapshot::capture(doc) == before && doc.revision() == revision && !doc.canUndo(),
            "Invalid plane mutated document or history");
    std::istringstream input(legacy(plane));
    rejected = false;
    try {
        static_cast<void>(document::DocumentSerializer::read(input));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "Legacy parser accepted a plane rejected by document");
}
void planes() {
    const document::SketchPlane valid{0, 0, 30, 0, 0, 1, 1, 0, 0};
    auto invalid = valid;
    invalid.normalX = 1e308;
    invalid.normalZ = 0;
    invalid.xDirectionX = 1e308;
    rejectedPlane(invalid); // old direct validator compared NaN > tolerance
    invalid = valid;
    invalid.normalX = 1.3e308;
    invalid.normalY = 1.3e308;
    invalid.normalZ = 0;
    invalid.xDirectionX = 0;
    invalid.xDirectionZ = 1;
    rejectedPlane(invalid); // individually finite components, infinite norm
    for (auto field : {&document::SketchPlane::originX, &document::SketchPlane::originY,
                       &document::SketchPlane::originZ, &document::SketchPlane::normalX,
                       &document::SketchPlane::normalY, &document::SketchPlane::normalZ,
                       &document::SketchPlane::xDirectionX, &document::SketchPlane::xDirectionY,
                       &document::SketchPlane::xDirectionZ}) {
        for (double bad :
             {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity()}) {
            invalid = valid;
            invalid.*field = bad;
            rejectedPlane(invalid);
        }
    }
    for (double small : {0.0, 1e-10, 1e-9}) {
        invalid = valid;
        invalid.normalZ = small;
        rejectedPlane(invalid);
    }
    invalid = valid;
    invalid.xDirectionZ = 2e-8;
    rejectedPlane(invalid);
    const double diagonal = 1.0 / std::sqrt(2.0);
    for (const auto& plane : std::array{valid, document::SketchPlane{0, 0, 30, diagonal, diagonal,
                                                                     0, -diagonal, diagonal, 0}}) {
        document::ModelDocument doc;
        const auto id = doc.addSketchFeature("Valid", document::FeatureId{1}, sketch(plane));
        const auto bytes = serialized(doc);
        const auto snapshot = document::DocumentSnapshot::capture(doc);
        std::istringstream input(bytes), old(legacy(plane));
        require(document::DocumentSerializer::read(input).sketchParameters(id).plane == plane &&
                    document::DocumentSerializer::read(old)
                            .sketchParameters(document::FeatureId{2})
                            .plane == plane,
                "Validation silently rewrote persisted plane coordinates");
        require(!doc.setSketchParameters(id, sketch(plane)), "Unchanged plane created history");
        invalid = plane;
        invalid.xDirectionX = invalid.normalX;
        invalid.xDirectionY = invalid.normalY;
        invalid.xDirectionZ = invalid.normalZ;
        bool rejected = false;
        try {
            static_cast<void>(doc.setSketchParameters(id, sketch(invalid)));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected && document::DocumentSnapshot::capture(doc) == snapshot, "Rejected plane edit changed document");
    }
}
void cylinderDirections() {
    for (double magnitude :
         {1e-300, 1.0, 1e200, 1e308, std::numeric_limits<double>::denorm_min()}) {
        const auto direction = kernel::UnitDirection::fromComponents(magnitude, magnitude, 0);
        require(std::abs(direction.dot(direction) - 1.0) < 1e-14, "Direction is not normalized");
        const auto shape =
            kernel::ShapeFactory::makeCylinder({2, 5, 0, 0, 0, magnitude, magnitude, 0});
        GProp_GProps properties;
        BRepGProp::VolumeProperties(shape, properties);
        const auto center = properties.CentreOfMass();
        require(BRepCheck_Analyzer(shape).IsValid() &&
                    std::abs(properties.Mass() - 20 * std::numbers::pi) < 1e-9 &&
                    std::abs(center.X() - 2.5 / std::sqrt(2.0)) < 1e-9 &&
                    std::abs(center.Y() - center.X()) < 1e-9,
                "Axis magnitude changed cylinder size or orientation");
    }
    bool rejected = false;
    try {
        static_cast<void>(kernel::ShapeFactory::makeCylinder({2, 5, 0, 0, 0, 1.3e308, 1.3e308, 0}));
    } catch (const kernel::KernelError& error) {
        rejected = error.operation() == kernel::KernelOperation::makeCylinder;
    }
    require(rejected, "Unrepresentable axis magnitude was not a typed kernel error");
}
void scales() {
    for (double length : {0.01, 100.0, 100000.0}) {
        document::ModelDocument doc;
        const document::FeatureId id{1};
        const document::BoxParameters parameters{
            {length, length * 2, length * 3},
            {length * 4, 0, 0, {}, {}, kernel::toRadians(kernel::Degrees(90))}};
        require(doc.setBoxParameters(id, parameters), "Scale fixture did not change");
        std::istringstream input(serialized(doc));
        const auto loaded = document::DocumentSerializer::read(input);
        require(loaded.boxParameters(id) == parameters, "Roundtrip changed scale or rotation");
        const auto shape = kernel::ShapeFactory::makeBox(loaded.boxDimensions(id));
        const auto rotated =
            kernel::ShapeFactory::applyTransform(shape, 0, 0, 0, parameters.placement);
        GProp_GProps properties;
        BRepGProp::VolumeProperties(rotated, properties);
        const double expected = 6 * length * length * length;
        require(BRepCheck_Analyzer(rotated).IsValid() &&
                    std::abs(properties.Mass() - expected) <= expected * 1e-10,
                "Rotation changed box volume across supported test scales");
    }
}
} // namespace
int main() {
    try {
        planes();
        cylinderDirections();
        scales();
        std::cout << "PASS direction overflow/underflow, atomic plane rejection, legacy parity, "
                     "scale roundtrip\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
