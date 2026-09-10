#include <voelund/document/DocumentSerializer.hpp>

#include <voelund/document/ModelDocument.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace voelund::document {
namespace {

constexpr std::size_t maximumFeatureCount = 100'000;

[[noreturn]] void invalidDocument(const std::string& reason)
{
    throw std::runtime_error("Invalid Völund document: " + reason);
}

std::string requiredLine(std::istream& input, const char* description)
{
    std::string line;
    if (!std::getline(input, line)) {
        invalidDocument(std::string("missing ") + description);
    }
    return line;
}

std::uint64_t headerValue(std::istream& input, const char* expectedName)
{
    std::istringstream line(requiredLine(input, expectedName));
    line.imbue(std::locale::classic());
    std::string name;
    std::uint64_t value = 0;
    std::string trailing;
    if (!(line >> name >> value) || name != expectedName || (line >> trailing)) {
        invalidDocument(std::string("malformed ") + expectedName + " header");
    }
    return value;
}

kernel::RigidTransform readTransform(std::istringstream& line)
{
    kernel::RigidTransform transform{};
    if (!(line >> transform.translationX >> transform.translationY
          >> transform.translationZ >> transform.rotationX
          >> transform.rotationY >> transform.rotationZ)) {
        invalidDocument("malformed feature placement");
    }
    return transform;
}

void validateTransform(const kernel::RigidTransform& transform)
{
    const double values[] = {
        transform.translationX, transform.translationY, transform.translationZ,
        transform.rotationX, transform.rotationY, transform.rotationZ};
    for (double value : values) {
        if (!std::isfinite(value)) {
            invalidDocument("placement contains a non-finite value");
        }
    }
}

bool positiveFinite(double value)
{
    return std::isfinite(value) && value > 0.0;
}

CylinderResultMode readResultMode(std::istringstream& line)
{
    std::string mode;
    if (!(line >> mode)) {
        invalidDocument("missing cylinder result mode");
    }
    if (mode == "JOIN") return CylinderResultMode::join;
    if (mode == "CUT") return CylinderResultMode::cut;
    if (mode == "INTERSECT") return CylinderResultMode::intersect;
    if (mode == "SEPARATE") return CylinderResultMode::separate;
    invalidDocument("unknown cylinder result mode");
}

const char* resultModeName(CylinderResultMode mode)
{
    switch (mode) {
    case CylinderResultMode::join: return "JOIN";
    case CylinderResultMode::cut: return "CUT";
    case CylinderResultMode::intersect: return "INTERSECT";
    case CylinderResultMode::separate: return "SEPARATE";
    }
    return "JOIN";
}

void writeTransform(std::ostream& output, const kernel::RigidTransform& transform)
{
    output << ' ' << transform.translationX << ' ' << transform.translationY
           << ' ' << transform.translationZ << ' ' << transform.rotationX
           << ' ' << transform.rotationY << ' ' << transform.rotationZ;
}

bool containsFeature(
    const std::vector<FeatureDescriptor>& features,
    FeatureId featureId)
{
    for (const FeatureDescriptor& feature : features) {
        if (feature.id == featureId) {
            return true;
        }
    }
    return false;
}

void ensureLineComplete(std::istringstream& line)
{
    std::string trailing;
    if (line >> trailing) {
        invalidDocument("unexpected data after feature");
    }
}

struct ParsedFeature final {
    FeatureDescriptor descriptor;
    FeatureParameters parameters;
};

ParsedFeature readBoxFeature(
    FeatureId id,
    std::string name,
    std::istringstream& line)
{
    BoxParameters box{};
    if (!(line >> box.dimensions.width >> box.dimensions.depth >> box.dimensions.height)) {
        invalidDocument("malformed box dimensions");
    }
    box.placement = readTransform(line);
    if (!positiveFinite(box.dimensions.width)
        || !positiveFinite(box.dimensions.depth)
        || !positiveFinite(box.dimensions.height)) {
        invalidDocument("box dimensions must be positive and finite");
    }
    validateTransform(box.placement);
    ensureLineComplete(line);
    return {{id, FeatureKind::box, std::move(name), std::nullopt}, box};
}

ParsedFeature readCylinderFeature(
    FeatureId id,
    std::string name,
    std::istringstream& line,
    const std::vector<FeatureDescriptor>& previousFeatures)
{
    std::uint64_t rawInputId = 0;
    CylinderParameters cylinder{};
    if (!(line >> rawInputId >> cylinder.radius >> cylinder.height)) {
        invalidDocument("malformed cylinder parameters");
    }
    cylinder.placement = readTransform(line);
    cylinder.resultMode = readResultMode(line);
    const FeatureId inputId{rawInputId};
    if (!containsFeature(previousFeatures, inputId)) {
        invalidDocument("cylinder input must reference an earlier feature");
    }
    if (!positiveFinite(cylinder.radius) || !positiveFinite(cylinder.height)) {
        invalidDocument("cylinder dimensions must be positive and finite");
    }
    validateTransform(cylinder.placement);
    ensureLineComplete(line);
    return {{id, FeatureKind::cylinder, std::move(name), inputId}, cylinder};
}

ParsedFeature readSketchFeature(
    FeatureId id,
    std::string name,
    std::istringstream& line,
    const std::vector<FeatureDescriptor>& previousFeatures)
{
    std::uint64_t rawSupportId = 0;
    SketchParameters sketch{};
    SketchPlane& plane = sketch.plane;
    SketchRectangle& rectangle = sketch.rectangle;
    if (!(line >> rawSupportId
          >> plane.originX >> plane.originY >> plane.originZ
          >> plane.normalX >> plane.normalY >> plane.normalZ
          >> plane.xDirectionX >> plane.xDirectionY >> plane.xDirectionZ
          >> rectangle.firstX >> rectangle.firstY
          >> rectangle.secondX >> rectangle.secondY)) {
        invalidDocument("malformed sketch parameters");
    }
    const FeatureId supportId{rawSupportId};
    if (!containsFeature(previousFeatures, supportId)) {
        invalidDocument("sketch support must reference an earlier feature");
    }
    ensureLineComplete(line);
    return {{id, FeatureKind::sketch, std::move(name), supportId}, sketch};
}

ParsedFeature readExtrusionFeature(
    FeatureId id,
    std::string name,
    std::istringstream& line,
    const std::vector<FeatureDescriptor>& previousFeatures)
{
    std::uint64_t rawSketchId = 0;
    ExtrusionParameters extrusion{};
    if (!(line >> rawSketchId >> extrusion.distance)
        || !positiveFinite(extrusion.distance)) {
        invalidDocument("malformed extrusion parameters");
    }
    const FeatureId sketchId{rawSketchId};
    if (!containsFeature(previousFeatures, sketchId)) {
        invalidDocument("extrusion input must reference an earlier sketch");
    }
    ensureLineComplete(line);
    return {{id, FeatureKind::extrusion, std::move(name), sketchId}, extrusion};
}

ParsedFeature readFeature(
    const std::string& source,
    const std::vector<FeatureDescriptor>& previousFeatures)
{
    std::istringstream line(source);
    line.imbue(std::locale::classic());
    std::string kind;
    std::uint64_t rawId = 0;
    std::string name;
    if (!(line >> kind >> rawId >> std::quoted(name)) || rawId == 0 || name.empty()) {
        invalidDocument("malformed feature identity");
    }
    const FeatureId id{rawId};
    if (containsFeature(previousFeatures, id)) {
        invalidDocument("duplicate feature ID");
    }
    if (kind == "BOX") {
        return readBoxFeature(id, std::move(name), line);
    }
    if (kind == "CYLINDER") {
        return readCylinderFeature(id, std::move(name), line, previousFeatures);
    }
    if (kind == "SKETCH") {
        return readSketchFeature(id, std::move(name), line, previousFeatures);
    }
    if (kind == "EXTRUSION") {
        return readExtrusionFeature(id, std::move(name), line, previousFeatures);
    }
    invalidDocument("unknown feature kind");
}

} // namespace

void DocumentSerializer::write(const ModelDocument& document, std::ostream& output)
{
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    output << "VOLUND 1\nNEXT_ID " << document.nextFeatureId_ << "\nFEATURES "
           << document.features_.size() << '\n';
    for (std::size_t index = 0; index < document.features_.size(); ++index) {
        const FeatureDescriptor& feature = document.features_[index];
        const char* kindName = "EXTRUSION ";
        if (feature.kind == FeatureKind::box) kindName = "BOX ";
        if (feature.kind == FeatureKind::cylinder) kindName = "CYLINDER ";
        if (feature.kind == FeatureKind::sketch) kindName = "SKETCH ";
        output << kindName << feature.id.value() << ' ' << std::quoted(feature.name);
        if (feature.kind == FeatureKind::box) {
            const BoxParameters& box = std::get<BoxParameters>(document.parameters_[index]);
            output << ' ' << box.dimensions.width << ' ' << box.dimensions.depth
                   << ' ' << box.dimensions.height;
            writeTransform(output, box.placement);
        } else if (feature.kind == FeatureKind::cylinder) {
            const CylinderParameters& cylinder =
                std::get<CylinderParameters>(document.parameters_[index]);
            output << ' ' << feature.inputFeature->value() << ' ' << cylinder.radius
                   << ' ' << cylinder.height;
            writeTransform(output, cylinder.placement);
            output << ' ' << resultModeName(cylinder.resultMode);
        } else if (feature.kind == FeatureKind::sketch) {
            const SketchParameters& sketch =
                std::get<SketchParameters>(document.parameters_[index]);
            const SketchPlane& plane = sketch.plane;
            const SketchRectangle& rectangle = sketch.rectangle;
            output << ' ' << feature.inputFeature->value()
                   << ' ' << plane.originX << ' ' << plane.originY << ' ' << plane.originZ
                   << ' ' << plane.normalX << ' ' << plane.normalY << ' ' << plane.normalZ
                   << ' ' << plane.xDirectionX << ' ' << plane.xDirectionY
                   << ' ' << plane.xDirectionZ << ' ' << rectangle.firstX
                   << ' ' << rectangle.firstY << ' ' << rectangle.secondX
                   << ' ' << rectangle.secondY;
        } else {
            const ExtrusionParameters& extrusion =
                std::get<ExtrusionParameters>(document.parameters_[index]);
            output << ' ' << feature.inputFeature->value() << ' ' << extrusion.distance;
        }
        output << '\n';
    }
    if (!output) {
        throw std::runtime_error("Failed to write Völund document");
    }
}

ModelDocument DocumentSerializer::read(std::istream& input)
{
    input.imbue(std::locale::classic());
    if (requiredLine(input, "file header") != "VOLUND 1") {
        invalidDocument("unsupported file header or version");
    }
    const std::uint64_t nextFeatureId = headerValue(input, "NEXT_ID");
    const std::uint64_t featureCount = headerValue(input, "FEATURES");
    if (featureCount > maximumFeatureCount) {
        invalidDocument("feature count is outside the supported range");
    }

    ModelDocument document = ModelDocument::empty();
    std::uint64_t maximumId = 0;
    for (std::uint64_t index = 0; index < featureCount; ++index) {
        ParsedFeature feature = readFeature(
            requiredLine(input, "feature"), document.features_);
        maximumId = std::max(maximumId, feature.descriptor.id.value());
        document.features_.push_back(std::move(feature.descriptor));
        document.parameters_.push_back(std::move(feature.parameters));
    }
    if (nextFeatureId <= maximumId) {
        invalidDocument("NEXT_ID must be greater than every feature ID");
    }
    std::string trailing;
    while (std::getline(input, trailing)) {
        if (trailing.find_first_not_of(" \t\r") != std::string::npos) {
            invalidDocument("unexpected data after feature list");
        }
    }
    document.nextFeatureId_ = nextFeatureId;
    document.undoHistory_.clear();
    document.redoHistory_.clear();
    return document;
}

} // namespace voelund::document
