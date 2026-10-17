#include "BoundedDocumentInput.hpp"
#include "DocumentFormat.hpp"
#include "DocumentSerializationDetail.hpp"
#include "SketchConstraintSerialization.hpp"
#include <voelund/document/LegacyDocumentImporter.hpp>
#include <voelund/document/SketchPlaneValidation.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <voelund/document/ModelDocument.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace voelund::document {
namespace {

using namespace detail;

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
        invalidDocument(kernel::modelRangeDiagnostic);
    }
    validateTransform(box.placement);
    ensureLineComplete(line);
    return {{id, FeatureKind::box, std::move(name), std::nullopt}, box};
}

ParsedFeature readCylinderFeature(FeatureId id, std::string name, std::istringstream& line,
                                  const std::vector<FeatureDescriptor>& previousFeatures) {
    std::uint64_t rawInputId = 0;
    CylinderParameters cylinder{};
    if (!(line >> unsignedValue(rawInputId) >> cylinder.radius >> cylinder.height)) {
        invalidDocument("malformed cylinder parameters");
    }
    cylinder.placement = readTransform(line);
    cylinder.resultMode = readResultMode(line);
    const FeatureId inputId{rawInputId};
    if (!containsFeature(previousFeatures, inputId)) {
        invalidDocument("cylinder input must reference an earlier feature");
    }
    if (!positiveFinite(cylinder.radius) || !positiveFinite(cylinder.height)) {
        invalidDocument(kernel::modelRangeDiagnostic);
    }
    validateTransform(cylinder.placement);
    ensureLineComplete(line);
    return {{id, FeatureKind::cylinder, std::move(name), inputId}, cylinder};
}

SketchElement readSketchElement(std::istream& input, std::uint64_t version) {
    std::istringstream elementLine(requiredLine(input, "sketch element"));
    elementLine.imbue(std::locale::classic());
    std::string marker;
    std::string kind;
    std::uint64_t rawId = 0;
    if (!(elementLine >> marker >> kind >> unsignedValue(rawId)) || marker != "ELEMENT") {
        invalidDocument("malformed sketch element identity");
    }
    SketchGeometry geometry;
    if (kind == "LINE") {
        SketchLine value{};
        if (!(elementLine >> value.start.x >> value.start.y >> value.end.x >> value.end.y))
            invalidDocument("malformed sketch line");
        if (version >= 3)
            value.constraint = readLineConstraint(elementLine);
        geometry = value;
    } else if (kind == "RECTANGLE") {
        SketchRectangle value{};
        if (!(elementLine >> value.first.x >> value.first.y >> value.second.x >> value.second.y)) {
            invalidDocument("malformed sketch rectangle");
        }
        geometry = value;
    } else if (kind == "CIRCLE") {
        SketchCircle value{};
        if (!(elementLine >> value.center.x >> value.center.y >> value.radius)) {
            invalidDocument("malformed sketch circle");
        }
        geometry = value;
    } else if (kind == "ARC" && version >= 15) {
        SketchArc value{};
        if (!(elementLine >> value.center.x >> value.center.y >> value.radius >> value.startAngle >>
              value.endAngle))
            invalidDocument("malformed sketch arc");
        geometry = value;
    } else if (kind == "ELLIPSE" && version >= 16) {
        SketchEllipse value{};
        if (!(elementLine >> value.center.x >> value.center.y >> value.majorRadius >>
              value.minorRadius >> value.rotation))
            invalidDocument("malformed sketch ellipse");
        geometry = value;
    } else {
        invalidDocument("unknown sketch element kind");
    }
    bool construction = false;
    if (version >= 10) {
        int rawConstruction = 0;
        if (!(elementLine >> rawConstruction) || (rawConstruction != 0 && rawConstruction != 1))
            invalidDocument("invalid sketch construction flag");
        construction = rawConstruction != 0;
    }
    ensureLineComplete(elementLine);
    return SketchElement{SketchElementId{rawId}, geometry, construction};
}

SketchConstraint readConstraint(std::istream& input, std::uint64_t version) {
    std::istringstream constraintLine(requiredLine(input, "sketch constraint"));
    constraintLine.imbue(std::locale::classic());
    std::string marker;
    std::uint64_t rawId = 0;
    std::uint64_t firstElement = 0;
    std::uint64_t secondElement = 0;
    if (!(constraintLine >> marker >> unsignedValue(rawId)) || marker != "CONSTRAINT")
        invalidDocument("malformed sketch constraint identity");
    const SketchConstraintKind kind = readSketchConstraintKind(constraintLine);
    validateConstraintVersion(kind, version);
    if (!(constraintLine >> unsignedValue(firstElement)))
        invalidDocument("malformed first sketch constraint reference");
    const SketchPointPosition firstPoint = readPointPosition(constraintLine);
    if (!(constraintLine >> unsignedValue(secondElement)))
        invalidDocument("malformed second sketch constraint reference");
    const SketchPointPosition secondPoint = readPointPosition(constraintLine);
    double value = 0.0;
    if (!(constraintLine >> value))
        invalidDocument("malformed sketch constraint value");
    std::optional<SketchGeometryReference> third;
    if (version >= 8) {
        std::uint64_t thirdElement = 0;
        if (!(constraintLine >> unsignedValue(thirdElement)))
            invalidDocument("malformed third sketch constraint reference");
        const SketchPointPosition thirdPoint = readPointPosition(constraintLine);
        if (thirdElement != 0)
            third = SketchGeometryReference{SketchElementId{thirdElement}, thirdPoint};
    }
    std::vector<double> auxiliaryValues;
    if (version >= 14) {
        std::uint64_t valueCount = 0;
        if (!(constraintLine >> unsignedValue(valueCount)) || valueCount > 4)
            invalidDocument("invalid auxiliary sketch constraint value count");
        auxiliaryValues.reserve(static_cast<std::size_t>(valueCount));
        for (std::uint64_t valueIndex = 0; valueIndex < valueCount; ++valueIndex) {
            double auxiliaryValue = 0.0;
            if (!(constraintLine >> auxiliaryValue))
                invalidDocument("malformed auxiliary sketch constraint value");
            auxiliaryValues.push_back(auxiliaryValue);
        }
    }
    ensureLineComplete(constraintLine);
    std::optional<SketchGeometryReference> second;
    if (secondElement != 0)
        second = SketchGeometryReference{SketchElementId{secondElement}, secondPoint};
    SketchConstraint result{SketchConstraintId{rawId},
                            kind,
                            SketchGeometryReference{SketchElementId{firstElement}, firstPoint},
                            second,
                            kind == SketchConstraintKind::angle ? 0.0 : value,
                            third,
                            std::move(auxiliaryValues)};
    if (kind == SketchConstraintKind::angle)
        result.angleValue = kernel::toRadians(kernel::Degrees(value));
    return result;
}

ParsedFeature readSketchFeature(FeatureId id, std::string name, std::istringstream& line,
                                const SketchReadContext& context) {
    std::uint64_t rawSupportId = 0;
    SketchParameters sketch{};
    SketchPlane& plane = sketch.plane;
    if (!(line >> unsignedValue(rawSupportId) >> plane.originX >> plane.originY >> plane.originZ >>
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
        if (!(line >> unsignedValue(elementCount) >> unsignedValue(sketch.elements.nextId)) ||
            elementCount == 0 || elementCount > maximumSketchElementCount) {
            invalidDocument("invalid sketch element count");
        }
        std::uint64_t constraintCount = 0;
        if (context.version >= 4 && (!(line >> unsignedValue(constraintCount) >>
                                       unsignedValue(sketch.constraints.nextId)) ||
                                     constraintCount > maximumSketchConstraintCount))
            invalidDocument("invalid sketch constraint count");

        for (std::uint64_t index = 0; index < elementCount; ++index) {
            sketch.elements.values.push_back(readSketchElement(context.input, context.version));
        }
        if (context.version >= 4) {

            for (std::uint64_t index = 0; index < constraintCount; ++index) {
                sketch.constraints.values.push_back(readConstraint(context.input, context.version));
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
    if (sketch.elements.nextId == std::numeric_limits<std::uint64_t>::max() ||
        sketch.constraints.nextId == std::numeric_limits<std::uint64_t>::max())
        invalidDocument("sketch next ID is exhausted");
    try {
        // Preserve historical parameters; eligibility is checked before evaluation.
        validateSketchPlane(sketch.plane);
        validateSketchElements(sketch.elements);
        validateSketchConstraints(sketch.elements, sketch.constraints);
    } catch (const std::invalid_argument& error) {
        invalidDocument(error.what());
    }
    return {{id, FeatureKind::sketch, std::move(name), supportId}, sketch};
}

ParsedFeature readExtrusionFeature(FeatureId id, std::string name, std::istringstream& line,
                                   const std::vector<FeatureDescriptor>& previousFeatures,
                                   std::uint64_t version) {
    std::uint64_t rawSketchId = 0;
    ExtrusionParameters extrusion{};
    if (!(line >> unsignedValue(rawSketchId) >> extrusion.distance)) {
        invalidDocument("malformed extrusion parameters");
    }
    if (!positiveFinite(extrusion.distance))
        invalidDocument(kernel::modelRangeDiagnostic);
    const FeatureId sketchId{rawSketchId};
    if (!containsFeatureKind(previousFeatures, sketchId, FeatureKind::sketch)) {
        invalidDocument("extrusion input must reference an earlier sketch");
    }
    if (version >= 5) {
        std::set<std::uint64_t> profileIds;
        std::uint64_t profileCount = 0;
        if (!(line >> unsignedValue(profileCount)) || profileCount > maximumExtrusionProfileCount)
            invalidDocument("invalid extrusion profile count");

        for (std::uint64_t index = 0; index < profileCount; ++index) {
            std::uint64_t rawProfileId = 0;
            if (!(line >> unsignedValue(rawProfileId)) || rawProfileId == 0)
                invalidDocument("malformed extrusion profile ID");
            const SketchProfileId profileId{rawProfileId};
            if (!profileIds.insert(rawProfileId).second)
                invalidDocument("duplicate extrusion profile ID");
            extrusion.profiles.push_back(profileId);
        }
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
    if (!(line >> kind >> unsignedValue(rawId) >> std::quoted(name)) || rawId == 0 ||
        name.empty() || name.size() > maximumNameBytes) {
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
        return readExtrusionFeature(id, std::move(name), line, previousFeatures, version);
    }
    invalidDocument("unknown feature kind");
}

} // namespace

ImportedDocument LegacyDocumentImporter::read(std::istream& source) {
    detail::BoundedDocumentInput input(source);
    input.imbue(std::locale::classic());
    return readPayload(input, detail::readSourceFormat(input));
}

ImportedDocument LegacyDocumentImporter::readPayload(std::istream& input,
                                                     DocumentSourceFormat format) {
    const auto version = format.version;
    const std::uint64_t nextFeatureId = headerValue(input, "NEXT_ID");
    const std::uint64_t featureCount = headerValue(input, "FEATURES");
    if (featureCount > maximumFeatureCount) {
        invalidDocument("feature count is outside the supported range");
    }

    std::vector<FeatureDescriptor> descriptors;
    std::vector<FeatureParameters> parameters;
    std::map<std::uint64_t, std::size_t> depths;
    std::uint64_t maximumId = 0;
    for (std::uint64_t index = 0; index < featureCount; ++index) {
        ParsedFeature feature =
            readFeature(requiredLine(input, "feature"), descriptors, input, version);
        const auto parent = feature.descriptor.inputFeature;
        const std::size_t depth = parent ? depths.at(parent->value()) + 1 : 1;
        if (depth > maximumFeatureDepth)
            invalidDocument("feature dependency depth limit exceeded");
        depths.emplace(feature.descriptor.id.value(), depth);
        maximumId = std::max(maximumId, feature.descriptor.id.value());
        descriptors.push_back(std::move(feature.descriptor));
        parameters.push_back(std::move(feature.parameters));
    }
    if (nextFeatureId <= maximumId || nextFeatureId == std::numeric_limits<std::uint64_t>::max()) {
        invalidDocument("NEXT_ID must be greater than every feature ID");
    }
    std::string trailing;
    while (std::getline(input, trailing)) {
        if (trailing.find_first_not_of(" \t\r") != std::string::npos) {
            invalidDocument("unexpected data after feature list");
        }
    }
    MigrationReport report{format, {}, {}};
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const auto& descriptor = descriptors[index];
        if (descriptor.kind == FeatureKind::sketch) {
            const auto& plane = std::get<SketchParameters>(parameters[index]).plane;
            try {
                validateCanonicalSketchPlane(plane);
            } catch (const std::invalid_argument& error) {
                report.planeIssues.push_back({descriptor.id, plane, error.what()});
            }
            report.referenceIssues.push_back({descriptor.id,
                                              *descriptor.inputFeature,
                                              LegacyReferenceKind::supportPlaneOnly,
                                              {},
                                              "Legacy sketch stores a support feature and plane, "
                                              "not a persistent face identity; review in R06"});
        } else if (descriptor.kind == FeatureKind::extrusion) {
            const auto& profiles = std::get<ExtrusionParameters>(parameters[index]).profiles;
            report.referenceIssues.push_back(
                {descriptor.id, *descriptor.inputFeature,
                 profiles.empty() ? LegacyReferenceKind::implicitAllProfiles
                                  : LegacyReferenceKind::legacyProfileIds,
                 profiles,
                 profiles.empty()
                     ? "Legacy extrusion implicitly selects all closed profiles; migrate "
                       "explicitly in R06/R09"
                     : "Legacy profile IDs require region identity migration in R06/R09"});
        }
    }
    return {DocumentSnapshot(std::move(descriptors), std::move(parameters), nextFeatureId),
            std::move(report)};
}

} // namespace voelund::document
