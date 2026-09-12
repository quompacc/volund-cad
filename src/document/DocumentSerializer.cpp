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
constexpr std::size_t maximumSketchElementCount = 1'000'000;
constexpr std::size_t maximumSketchConstraintCount = 2'000'000;

[[noreturn]] void invalidDocument(const std::string& reason) {
    throw std::runtime_error("Invalid Völund document: " + reason);
}

std::string requiredLine(std::istream& input, const char* description) {
    std::string line;
    if (!std::getline(input, line)) {
        invalidDocument(std::string("missing ") + description);
    }
    return line;
}

std::uint64_t headerValue(std::istream& input, const char* expectedName) {
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

kernel::RigidTransform readTransform(std::istringstream& line) {
    kernel::RigidTransform transform{};
    if (!(line >> transform.translationX >> transform.translationY >> transform.translationZ >>
          transform.rotationX >> transform.rotationY >> transform.rotationZ)) {
        invalidDocument("malformed feature placement");
    }
    return transform;
}

void validateTransform(const kernel::RigidTransform& transform) {
    const double values[] = {transform.translationX, transform.translationY, transform.translationZ,
                             transform.rotationX,    transform.rotationY,    transform.rotationZ};
    for (double value : values) {
        if (!std::isfinite(value)) {
            invalidDocument("placement contains a non-finite value");
        }
    }
}

bool positiveFinite(double value) { return std::isfinite(value) && value > 0.0; }

CylinderResultMode readResultMode(std::istringstream& line) {
    std::string mode;
    if (!(line >> mode)) {
        invalidDocument("missing cylinder result mode");
    }
    if (mode == "JOIN")
        return CylinderResultMode::join;
    if (mode == "CUT")
        return CylinderResultMode::cut;
    if (mode == "INTERSECT")
        return CylinderResultMode::intersect;
    if (mode == "SEPARATE")
        return CylinderResultMode::separate;
    invalidDocument("unknown cylinder result mode");
}

const char* resultModeName(CylinderResultMode mode) {
    switch (mode) {
    case CylinderResultMode::join:
        return "JOIN";
    case CylinderResultMode::cut:
        return "CUT";
    case CylinderResultMode::intersect:
        return "INTERSECT";
    case CylinderResultMode::separate:
        return "SEPARATE";
    }
    return "JOIN";
}

SketchLineConstraint readLineConstraint(std::istringstream& line) {
    std::string constraint;
    if (!(line >> constraint))
        invalidDocument("missing sketch line constraint");
    if (constraint == "FREE")
        return SketchLineConstraint::none;
    if (constraint == "HORIZONTAL")
        return SketchLineConstraint::horizontal;
    if (constraint == "VERTICAL")
        return SketchLineConstraint::vertical;
    invalidDocument("unknown sketch line constraint");
}

const char* lineConstraintName(SketchLineConstraint constraint) {
    if (constraint == SketchLineConstraint::horizontal)
        return "HORIZONTAL";
    if (constraint == SketchLineConstraint::vertical)
        return "VERTICAL";
    return "FREE";
}

SketchPointPosition readPointPosition(std::istringstream& line) {
    std::string value;
    if (!(line >> value))
        invalidDocument("missing sketch point position");
    if (value == "START")
        return SketchPointPosition::start;
    if (value == "END")
        return SketchPointPosition::end;
    if (value == "CENTER")
        return SketchPointPosition::center;
    invalidDocument("unknown sketch point position");
}

const char* pointPositionName(SketchPointPosition position) {
    if (position == SketchPointPosition::end)
        return "END";
    if (position == SketchPointPosition::center)
        return "CENTER";
    return "START";
}

SketchConstraintKind readConstraintKind(std::istringstream& line) {
    std::string value;
    if (!(line >> value))
        invalidDocument("missing sketch constraint kind");
    if (value == "HORIZONTAL")
        return SketchConstraintKind::horizontal;
    if (value == "VERTICAL")
        return SketchConstraintKind::vertical;
    if (value == "DISTANCE")
        return SketchConstraintKind::distance;
    if (value == "RADIUS")
        return SketchConstraintKind::radius;
    if (value == "COINCIDENT")
        return SketchConstraintKind::coincident;
    if (value == "PARALLEL")
        return SketchConstraintKind::parallel;
    if (value == "PERPENDICULAR")
        return SketchConstraintKind::perpendicular;
    invalidDocument("unknown sketch constraint kind");
}

const char* constraintKindName(SketchConstraintKind kind) {
    switch (kind) {
    case SketchConstraintKind::horizontal:
        return "HORIZONTAL";
    case SketchConstraintKind::vertical:
        return "VERTICAL";
    case SketchConstraintKind::distance:
        return "DISTANCE";
    case SketchConstraintKind::radius:
        return "RADIUS";
    case SketchConstraintKind::coincident:
        return "COINCIDENT";
    case SketchConstraintKind::parallel:
        return "PARALLEL";
    case SketchConstraintKind::perpendicular:
        return "PERPENDICULAR";
    }
    return "HORIZONTAL";
}

void writeTransform(std::ostream& output, const kernel::RigidTransform& transform) {
    output << ' ' << transform.translationX << ' ' << transform.translationY << ' '
           << transform.translationZ << ' ' << transform.rotationX << ' ' << transform.rotationY
           << ' ' << transform.rotationZ;
}

bool containsFeature(const std::vector<FeatureDescriptor>& features, FeatureId featureId) {
    for (const FeatureDescriptor& feature : features) {
        if (feature.id == featureId) {
            return true;
        }
    }
    return false;
}

bool containsFeatureKind(const std::vector<FeatureDescriptor>& features, FeatureId featureId,
                         FeatureKind kind) {
    const auto feature = std::ranges::find(features, featureId, &FeatureDescriptor::id);
    return feature != features.end() && feature->kind == kind;
}

void ensureLineComplete(std::istringstream& line) {
    std::string trailing;
    if (line >> trailing) {
        invalidDocument("unexpected data after feature");
    }
}

struct ParsedFeature final {
    FeatureDescriptor descriptor;
    FeatureParameters parameters;
};

struct SketchReadContext final {
    const std::vector<FeatureDescriptor>& previousFeatures;
    std::istream& input;
    std::uint64_t version;
};

ParsedFeature readBoxFeature(FeatureId id, std::string name, std::istringstream& line) {
    BoxParameters box{};
    if (!(line >> box.dimensions.width >> box.dimensions.depth >> box.dimensions.height)) {
        invalidDocument("malformed box dimensions");
    }
    box.placement = readTransform(line);
    if (!positiveFinite(box.dimensions.width) || !positiveFinite(box.dimensions.depth) ||
        !positiveFinite(box.dimensions.height)) {
        invalidDocument("box dimensions must be positive and finite");
    }
    validateTransform(box.placement);
    ensureLineComplete(line);
    return {{id, FeatureKind::box, std::move(name), std::nullopt}, box};
}

ParsedFeature readCylinderFeature(FeatureId id, std::string name, std::istringstream& line,
                                  const std::vector<FeatureDescriptor>& previousFeatures) {
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

ParsedFeature readSketchFeature(FeatureId id, std::string name, std::istringstream& line,
                                const SketchReadContext& context) {
    std::uint64_t rawSupportId = 0;
    SketchParameters sketch{};
    SketchPlane& plane = sketch.plane;
    if (!(line >> rawSupportId >> plane.originX >> plane.originY >> plane.originZ >>
          plane.normalX >> plane.normalY >> plane.normalZ >> plane.xDirectionX >>
          plane.xDirectionY >> plane.xDirectionZ)) {
        invalidDocument("malformed sketch parameters");
    }
    const FeatureId supportId{rawSupportId};
    if (!containsFeature(context.previousFeatures, supportId)) {
        invalidDocument("sketch support must reference an earlier feature");
    }
    if (context.version == 1) {
        SketchRectangle rectangle{};
        if (!(line >> rectangle.first.x >> rectangle.first.y >> rectangle.second.x >>
              rectangle.second.y)) {
            invalidDocument("malformed legacy sketch rectangle");
        }
        sketch.elements.values.push_back(SketchElement{SketchElementId{1}, rectangle});
        sketch.elements.nextId = 2;
    } else {
        std::uint64_t elementCount = 0;
        if (!(line >> elementCount >> sketch.elements.nextId) || elementCount == 0 ||
            elementCount > maximumSketchElementCount) {
            invalidDocument("invalid sketch element count");
        }
        std::uint64_t constraintCount = 0;
        if (context.version >= 4 && (!(line >> constraintCount >> sketch.constraints.nextId) ||
                                     constraintCount > maximumSketchConstraintCount))
            invalidDocument("invalid sketch constraint count");
        sketch.elements.values.reserve(static_cast<std::size_t>(elementCount));
        for (std::uint64_t index = 0; index < elementCount; ++index) {
            std::istringstream elementLine(requiredLine(context.input, "sketch element"));
            elementLine.imbue(std::locale::classic());
            std::string marker;
            std::string kind;
            std::uint64_t rawId = 0;
            if (!(elementLine >> marker >> kind >> rawId) || marker != "ELEMENT") {
                invalidDocument("malformed sketch element identity");
            }
            SketchGeometry geometry;
            if (kind == "LINE") {
                SketchLine value{};
                if (!(elementLine >> value.start.x >> value.start.y >> value.end.x >> value.end.y))
                    invalidDocument("malformed sketch line");
                if (context.version >= 3)
                    value.constraint = readLineConstraint(elementLine);
                geometry = value;
            } else if (kind == "RECTANGLE") {
                SketchRectangle value{};
                if (!(elementLine >> value.first.x >> value.first.y >> value.second.x >>
                      value.second.y)) {
                    invalidDocument("malformed sketch rectangle");
                }
                geometry = value;
            } else if (kind == "CIRCLE") {
                SketchCircle value{};
                if (!(elementLine >> value.center.x >> value.center.y >> value.radius)) {
                    invalidDocument("malformed sketch circle");
                }
                geometry = value;
            } else {
                invalidDocument("unknown sketch element kind");
            }
            ensureLineComplete(elementLine);
            sketch.elements.values.push_back(SketchElement{SketchElementId{rawId}, geometry});
        }
        if (context.version >= 4) {
            sketch.constraints.values.reserve(static_cast<std::size_t>(constraintCount));
            for (std::uint64_t index = 0; index < constraintCount; ++index) {
                std::istringstream constraintLine(requiredLine(context.input, "sketch constraint"));
                constraintLine.imbue(std::locale::classic());
                std::string marker;
                std::uint64_t rawId = 0;
                std::uint64_t firstElement = 0;
                std::uint64_t secondElement = 0;
                if (!(constraintLine >> marker >> rawId) || marker != "CONSTRAINT")
                    invalidDocument("malformed sketch constraint identity");
                const SketchConstraintKind kind = readConstraintKind(constraintLine);
                if (!(constraintLine >> firstElement))
                    invalidDocument("malformed first sketch constraint reference");
                const SketchPointPosition firstPoint = readPointPosition(constraintLine);
                if (!(constraintLine >> secondElement))
                    invalidDocument("malformed second sketch constraint reference");
                const SketchPointPosition secondPoint = readPointPosition(constraintLine);
                double value = 0.0;
                if (!(constraintLine >> value))
                    invalidDocument("malformed sketch constraint value");
                ensureLineComplete(constraintLine);
                std::optional<SketchGeometryReference> second;
                if (secondElement != 0)
                    second = SketchGeometryReference{SketchElementId{secondElement}, secondPoint};
                sketch.constraints.values.push_back(
                    {SketchConstraintId{rawId}, kind,
                     SketchGeometryReference{SketchElementId{firstElement}, firstPoint}, second,
                     value});
            }
        } else if (context.version >= 3) {
            for (const SketchElement& element : sketch.elements.values) {
                const auto* legacyLine = std::get_if<SketchLine>(&element.geometry);
                if (legacyLine == nullptr || legacyLine->constraint == SketchLineConstraint::none)
                    continue;
                const SketchConstraintKind kind =
                    legacyLine->constraint == SketchLineConstraint::horizontal
                        ? SketchConstraintKind::horizontal
                        : SketchConstraintKind::vertical;
                sketch.constraints.values.push_back(
                    {SketchConstraintId{sketch.constraints.nextId++}, kind,
                     SketchGeometryReference{element.id, SketchPointPosition::start}, std::nullopt,
                     0.0});
            }
        }
    }
    ensureLineComplete(line);
    try {
        validateSketchParameters(sketch);
    } catch (const std::invalid_argument& error) {
        invalidDocument(error.what());
    }
    return {{id, FeatureKind::sketch, std::move(name), supportId}, sketch};
}

ParsedFeature readExtrusionFeature(FeatureId id, std::string name, std::istringstream& line,
                                   const std::vector<FeatureDescriptor>& previousFeatures) {
    std::uint64_t rawSketchId = 0;
    ExtrusionParameters extrusion{};
    if (!(line >> rawSketchId >> extrusion.distance) || !positiveFinite(extrusion.distance)) {
        invalidDocument("malformed extrusion parameters");
    }
    const FeatureId sketchId{rawSketchId};
    if (!containsFeatureKind(previousFeatures, sketchId, FeatureKind::sketch)) {
        invalidDocument("extrusion input must reference an earlier sketch");
    }
    ensureLineComplete(line);
    return {{id, FeatureKind::extrusion, std::move(name), sketchId}, extrusion};
}

ParsedFeature readFeature(const std::string& source,
                          const std::vector<FeatureDescriptor>& previousFeatures,
                          std::istream& input, std::uint64_t version) {
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
        return readSketchFeature(id, std::move(name), line,
                                 SketchReadContext{previousFeatures, input, version});
    }
    if (kind == "EXTRUSION") {
        return readExtrusionFeature(id, std::move(name), line, previousFeatures);
    }
    invalidDocument("unknown feature kind");
}

} // namespace

void DocumentSerializer::write(const ModelDocument& document, std::ostream& output) {
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    output << "VOLUND 4\nNEXT_ID " << document.nextFeatureId_ << "\nFEATURES "
           << document.features_.size() << '\n';
    for (std::size_t index = 0; index < document.features_.size(); ++index) {
        const FeatureDescriptor& feature = document.features_[index];
        const char* kindName = "EXTRUSION ";
        if (feature.kind == FeatureKind::box)
            kindName = "BOX ";
        if (feature.kind == FeatureKind::cylinder)
            kindName = "CYLINDER ";
        if (feature.kind == FeatureKind::sketch)
            kindName = "SKETCH ";
        output << kindName << feature.id.value() << ' ' << std::quoted(feature.name);
        if (feature.kind == FeatureKind::box) {
            const BoxParameters& box = std::get<BoxParameters>(document.parameters_[index]);
            output << ' ' << box.dimensions.width << ' ' << box.dimensions.depth << ' '
                   << box.dimensions.height;
            writeTransform(output, box.placement);
        } else if (feature.kind == FeatureKind::cylinder) {
            const CylinderParameters& cylinder =
                std::get<CylinderParameters>(document.parameters_[index]);
            output << ' ' << feature.inputFeature->value() << ' ' << cylinder.radius << ' '
                   << cylinder.height;
            writeTransform(output, cylinder.placement);
            output << ' ' << resultModeName(cylinder.resultMode);
        } else if (feature.kind == FeatureKind::sketch) {
            const SketchParameters& sketch =
                std::get<SketchParameters>(document.parameters_[index]);
            const SketchPlane& plane = sketch.plane;
            output << ' ' << feature.inputFeature->value() << ' ' << plane.originX << ' '
                   << plane.originY << ' ' << plane.originZ << ' ' << plane.normalX << ' '
                   << plane.normalY << ' ' << plane.normalZ << ' ' << plane.xDirectionX << ' '
                   << plane.xDirectionY << ' ' << plane.xDirectionZ << ' '
                   << sketch.elements.values.size() << ' ' << sketch.elements.nextId << ' '
                   << sketch.constraints.values.size() << ' ' << sketch.constraints.nextId << '\n';
            for (const SketchElement& element : sketch.elements.values) {
                output << "ELEMENT ";
                if (const auto* line = std::get_if<SketchLine>(&element.geometry)) {
                    output << "LINE " << element.id.value() << ' ' << line->start.x << ' '
                           << line->start.y << ' ' << line->end.x << ' ' << line->end.y << ' '
                           << lineConstraintName(line->constraint);
                } else if (const auto* rectangle =
                               std::get_if<SketchRectangle>(&element.geometry)) {
                    output << "RECTANGLE " << element.id.value() << ' ' << rectangle->first.x << ' '
                           << rectangle->first.y << ' ' << rectangle->second.x << ' '
                           << rectangle->second.y;
                } else {
                    const SketchCircle& circle = std::get<SketchCircle>(element.geometry);
                    output << "CIRCLE " << element.id.value() << ' ' << circle.center.x << ' '
                           << circle.center.y << ' ' << circle.radius;
                }
                output << '\n';
            }
            for (const SketchConstraint& constraint : sketch.constraints.values) {
                output << "CONSTRAINT " << constraint.id.value() << ' '
                       << constraintKindName(constraint.kind) << ' '
                       << constraint.first.element.value() << ' '
                       << pointPositionName(constraint.first.point) << ' ';
                if (constraint.second.has_value()) {
                    output << constraint.second->element.value() << ' '
                           << pointPositionName(constraint.second->point);
                } else {
                    output << "0 START";
                }
                output << ' ' << constraint.value << '\n';
            }
            continue;
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

ModelDocument DocumentSerializer::read(std::istream& input) {
    input.imbue(std::locale::classic());
    const std::string header = requiredLine(input, "file header");
    const std::uint64_t version = header == "VOLUND 1"   ? 1
                                  : header == "VOLUND 2" ? 2
                                  : header == "VOLUND 3" ? 3
                                  : header == "VOLUND 4" ? 4
                                                         : 0;
    if (version == 0) {
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
        ParsedFeature feature =
            readFeature(requiredLine(input, "feature"), document.features_, input, version);
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
