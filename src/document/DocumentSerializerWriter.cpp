#include "DocumentSerializationDetail.hpp"
#include "SketchConstraintSerialization.hpp"

#include <voelund/document/DocumentSerializer.hpp>

#include <iomanip>
#include <limits>
#include <locale>
#include <ostream>
#include <stdexcept>

namespace voelund::document {
namespace {

void writeSketch(const FeatureDescriptor& feature, const SketchParameters& sketch,
                 std::ostream& output) {
    const auto& plane = sketch.plane;
    output << ' ' << feature.inputFeature->value() << ' ' << plane.originX << ' ' << plane.originY
           << ' ' << plane.originZ << ' ' << plane.normalX << ' ' << plane.normalY << ' '
           << plane.normalZ << ' ' << plane.xDirectionX << ' ' << plane.xDirectionY << ' '
           << plane.xDirectionZ << ' ' << sketch.elements.values.size() << ' '
           << sketch.elements.nextId << ' ' << sketch.constraints.values.size() << ' '
           << sketch.constraints.nextId << '\n';
    for (const SketchElement& element : sketch.elements.values) {
        output << "ELEMENT ";
        if (const auto* line = std::get_if<SketchLine>(&element.geometry))
            output << "LINE " << element.id.value() << ' ' << line->start.x << ' ' << line->start.y
                   << ' ' << line->end.x << ' ' << line->end.y << ' '
                   << detail::lineConstraintName(line->constraint);
        else if (const auto* rectangle = std::get_if<SketchRectangle>(&element.geometry))
            output << "RECTANGLE " << element.id.value() << ' ' << rectangle->first.x << ' '
                   << rectangle->first.y << ' ' << rectangle->second.x << ' '
                   << rectangle->second.y;
        else if (const auto* circle = std::get_if<SketchCircle>(&element.geometry))
            output << "CIRCLE " << element.id.value() << ' ' << circle->center.x << ' '
                   << circle->center.y << ' ' << circle->radius;
        else if (const auto* arc = std::get_if<SketchArc>(&element.geometry))
            output << "ARC " << element.id.value() << ' ' << arc->center.x << ' ' << arc->center.y
                   << ' ' << arc->radius << ' ' << arc->startAngle << ' ' << arc->endAngle;
        else {
            const auto& ellipse = std::get<SketchEllipse>(element.geometry);
            output << "ELLIPSE " << element.id.value() << ' ' << ellipse.center.x << ' '
                   << ellipse.center.y << ' ' << ellipse.majorRadius << ' ' << ellipse.minorRadius
                   << ' ' << ellipse.rotation;
        }
        output << ' ' << (element.construction ? 1 : 0) << '\n';
    }
    for (const SketchConstraint& constraint : sketch.constraints.values) {
        output << "CONSTRAINT " << constraint.id.value() << ' '
               << sketchConstraintKindName(constraint.kind) << ' '
               << constraint.first.element.value() << ' '
               << detail::pointPositionName(constraint.first.point) << ' ';
        if (constraint.second)
            output << constraint.second->element.value() << ' '
                   << detail::pointPositionName(constraint.second->point);
        else
            output << "0 START";
        output << ' '
               << (constraint.kind == SketchConstraintKind::angle
                       ? kernel::toDegrees(constraint.angleValue).value()
                       : constraint.value)
               << ' ';
        if (constraint.third)
            output << constraint.third->element.value() << ' '
                   << detail::pointPositionName(constraint.third->point);
        else
            output << "0 START";
        output << ' ' << constraint.auxiliaryValues.size();
        for (double value : constraint.auxiliaryValues)
            output << ' ' << value;
        output << '\n';
    }
}

} // namespace

void DocumentSerializer::write(const ModelDocument& document, std::ostream& output) {
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    output << "VOLUND 16\nNEXT_ID " << document.nextFeatureId_ << "\nFEATURES "
           << document.features_.size() << '\n';
    for (std::size_t index = 0; index < document.features_.size(); ++index) {
        const FeatureDescriptor& feature = document.features_[index];
        const char* kindName = feature.kind == FeatureKind::box        ? "BOX "
                               : feature.kind == FeatureKind::cylinder ? "CYLINDER "
                               : feature.kind == FeatureKind::sketch   ? "SKETCH "
                                                                       : "EXTRUSION ";
        output << kindName << feature.id.value() << ' ' << std::quoted(feature.name);
        if (feature.kind == FeatureKind::box) {
            const auto& box = std::get<BoxParameters>(document.parameters_[index]);
            output << ' ' << box.dimensions.width << ' ' << box.dimensions.depth << ' '
                   << box.dimensions.height;
            detail::writeTransform(output, box.placement);
        } else if (feature.kind == FeatureKind::cylinder) {
            const auto& cylinder = std::get<CylinderParameters>(document.parameters_[index]);
            output << ' ' << feature.inputFeature->value() << ' ' << cylinder.radius << ' '
                   << cylinder.height;
            detail::writeTransform(output, cylinder.placement);
            output << ' ' << detail::resultModeName(cylinder.resultMode);
        } else if (feature.kind == FeatureKind::extrusion) {
            const auto& extrusion = std::get<ExtrusionParameters>(document.parameters_[index]);
            output << ' ' << feature.inputFeature->value() << ' ' << extrusion.distance << ' '
                   << extrusion.profiles.size();
            for (SketchProfileId profile : extrusion.profiles)
                output << ' ' << profile.value();
        } else {
            const auto& sketch = std::get<SketchParameters>(document.parameters_[index]);
            writeSketch(feature, sketch, output);
            continue;
        }
        output << '\n';
    }
    if (!output)
        throw std::runtime_error("Failed to write Völund document");
}

} // namespace voelund::document
