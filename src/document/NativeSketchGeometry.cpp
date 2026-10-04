#include "NativeSketchGeometry.hpp"
#include "DocumentSerializationDetail.hpp"
#include "NativeSketchConstraints.hpp"
#include "NativeSketchSupport.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_RealArray.hxx>
#include <algorithm>
#include <charconv>
#include <voelund/document/IdCapacity.hpp>
#include <voelund/document/SketchPlaneValidation.hpp>

namespace voelund::document::detail {
namespace {
void require(bool condition) {
    if (!condition)
        throw std::invalid_argument("Invalid or unsupported native sketch geometry");
}
TDF_Label child(const TDF_Label& label, int tag) {
    const auto result = label.FindChild(tag, false);
    require(!result.IsNull());
    return result;
}
int integer(const TDF_Label& label) {
    Handle(TDataStd_Integer) value;
    require(label.FindAttribute(TDataStd_Integer::GetID(), value));
    return value->Get();
}
std::string text(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.FindAttribute(TDataStd_AsciiString::GetID(), value) &&
            value->Get().Length() <= 32);
    return {value->Get().ToCString(), static_cast<std::size_t>(value->Get().Length())};
}
std::uint64_t number(const TDF_Label& label, bool counter) {
    const auto encoded = text(label);
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(encoded.data(), encoded.data() + encoded.size(), value);
    require(parsed.ec == std::errc{} && parsed.ptr == encoded.data() + encoded.size() &&
            encoded == std::to_string(value) && hasIdCapacity(value, counter ? 0 : 1));
    return value;
}
void writeNumber(const TDF_Label& label, std::uint64_t value) {
    TDataStd_AsciiString::Set(label, std::to_string(value).c_str());
}
void writeReals(const TDF_Label& label, const std::vector<double>& values) {
    auto array = TDataStd_RealArray::Set(label, 1, static_cast<int>(values.size()));
    for (int index = 1; index <= static_cast<int>(values.size()); ++index)
        array->SetValue(index, values[static_cast<std::size_t>(index - 1)]);
}
Handle(TDataStd_RealArray) reals(const TDF_Label& label, int size) {
    Handle(TDataStd_RealArray) array;
    require(label.FindAttribute(TDataStd_RealArray::GetID(), array));
    require(array->Lower() == 1 && array->Upper() == size);
    return array;
}
int lineMode(SketchLineConstraint mode) {
    switch (mode) {
    case SketchLineConstraint::none:
        return 0;
    case SketchLineConstraint::horizontal:
        return 1;
    case SketchLineConstraint::vertical:
        return 2;
    }
    throw std::invalid_argument("Unknown native line direction mode");
}
SketchLineConstraint lineMode(int code) {
    switch (code) {
    case 0:
        return SketchLineConstraint::none;
    case 1:
        return SketchLineConstraint::horizontal;
    case 2:
        return SketchLineConstraint::vertical;
    default:
        throw std::invalid_argument("Unknown native line direction mode");
    }
}
void writeElement(const TDF_Label& label, const SketchElement& element) {
    writeNumber(label.FindChild(1), element.id.value());
    TDataStd_Integer::Set(label.FindChild(3), element.construction ? 1 : 0);
    const char* kind;
    std::vector<double> values;
    if (const auto* line = std::get_if<SketchLine>(&element.geometry)) {
        kind = "line";
        values = {line->start.x, line->start.y, line->end.x, line->end.y};
        TDataStd_Integer::Set(label.FindChild(5), lineMode(line->constraint));
    } else if (const auto* rectangle = std::get_if<SketchRectangle>(&element.geometry)) {
        kind = "rectangle";
        values = {rectangle->first.x, rectangle->first.y, rectangle->second.x, rectangle->second.y};
    } else if (const auto* circle = std::get_if<SketchCircle>(&element.geometry)) {
        kind = "circle";
        values = {circle->center.x, circle->center.y, circle->radius};
    } else if (const auto* arc = std::get_if<SketchArc>(&element.geometry)) {
        kind = "arc";
        values = {arc->center.x, arc->center.y, arc->radius, arc->startAngle, arc->endAngle};
    } else {
        const auto& ellipse = std::get<SketchEllipse>(element.geometry);
        kind = "ellipse";
        values = {ellipse.center.x, ellipse.center.y, ellipse.majorRadius, ellipse.minorRadius,
                  ellipse.rotation};
    }
    TDataStd_AsciiString::Set(label.FindChild(2), kind);
    writeReals(label.FindChild(4), values);
}
SketchElement readElement(const TDF_Label& label) {
    const auto kind = text(child(label, 2));
    const bool line = kind == "line";
    require(label.NbChildren() == (line ? 5 : 4));
    const int construction = integer(child(label, 3));
    require(construction == 0 || construction == 1);
    const auto geometry = [&]() -> SketchGeometry {
        if (line || kind == "rectangle") {
            const auto data = reals(child(label, 4), 4);
            if (line)
                return SketchLine{{data->Value(1), data->Value(2)},
                                  {data->Value(3), data->Value(4)},
                                  lineMode(integer(child(label, 5)))};
            return SketchRectangle{data->Value(1), data->Value(2), data->Value(3), data->Value(4)};
        }
        if (kind == "circle") {
            const auto data = reals(child(label, 4), 3);
            return SketchCircle{{data->Value(1), data->Value(2)}, data->Value(3)};
        }
        require(kind == "arc" || kind == "ellipse");
        const auto data = reals(child(label, 4), 5);
        if (kind == "arc")
            return SketchArc{
                {data->Value(1), data->Value(2)}, data->Value(3), data->Value(4), data->Value(5)};
        return SketchEllipse{
            {data->Value(1), data->Value(2)}, data->Value(3), data->Value(4), data->Value(5)};
    }();
    return {SketchElementId{number(child(label, 1), false)}, geometry, construction != 0};
}
} // namespace
void validateNativeSketchGeometry(const SketchParameters& sketch) {
    if (sketch.repairReason) {
        const auto reason = *sketch.repairReason;
        require(reason == SketchRepairReason::legacySupport ||
                reason == SketchRepairReason::legacyEdges);
        require((reason == SketchRepairReason::legacySupport) ==
                !sketch.supportBinding.has_value());
        if (reason == SketchRepairReason::legacyEdges)
            require(std::ranges::any_of(sketch.constraints.values, [](const auto& c) {
                return (c.kind == SketchConstraintKind::coordinateX ||
                        c.kind == SketchConstraintKind::coordinateY) &&
                       c.auxiliaryValues.size() == 2;
            }));
    }
    if (sketch.datum) {
        const auto value = static_cast<int>(*sketch.datum);
        require(value >= 1 && value <= 3 && !sketch.supportBinding && !sketch.repairReason &&
                sketch.plane == datumSketchPlane(*sketch.datum));
    }
    require(sketch.elements.values.size() <= maximumSketchElementCount);
    // Preserve legacy axis magnitudes; canonicalization remains an explicit edit.
    validateSketchPlane(sketch.plane);
    validateSketchElements(sketch.elements);
    validateNativeSketchConstraints(sketch);
    for (const auto& constraint : sketch.constraints.values)
        if (constraint.projectedEdge && !sketch.supportBinding)
            throw std::invalid_argument(
                "Projected edge requires an explicitly bound sketch support");
    if (sketch.supportBinding) {
        validateSubshapeOrigin(sketch.supportBinding->reference.origin);
        validateCanonicalSketchPlane(sketch.supportBinding->localPlane);
    }
    for (const auto& element : sketch.elements.values)
        if (const auto* line = std::get_if<SketchLine>(&element.geometry))
            (void)lineMode(line->constraint);
}
void writeNativeSketchGeometry(const TDF_Label& label, const SketchParameters& sketch) {
    const auto& plane = sketch.plane;
    writeReals(label, {plane.originX, plane.originY, plane.originZ, plane.normalX, plane.normalY,
                       plane.normalZ, plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ});
    writeNumber(label.FindChild(1), sketch.elements.nextId);
    writeNumber(label.FindChild(2), sketch.constraints.nextId);
    const auto elements = label.FindChild(10);
    TDataStd_Integer::Set(elements, static_cast<int>(sketch.elements.values.size()));
    for (std::size_t index = 0; index < sketch.elements.values.size(); ++index)
        writeElement(elements.FindChild(static_cast<int>(index + 1)),
                     sketch.elements.values[index]);
    writeNativeSketchConstraints(label.FindChild(3), elements, sketch);
    if (sketch.supportBinding)
        writeNativeSketchSupport(label.FindChild(11), *sketch.supportBinding);
    if (sketch.repairReason)
        TDataStd_Integer::Set(label.FindChild(12), static_cast<int>(*sketch.repairReason));
    if (sketch.datum)
        TDataStd_Integer::Set(label.FindChild(13), static_cast<int>(*sketch.datum));
}
SketchParameters readNativeSketchGeometry(const TDF_Label& label, std::size_t remainingElements,
                                          int schemaVersion) {
    const auto binding = label.FindChild(11, false);
    const auto repair = label.FindChild(12, false);
    const auto datum = label.FindChild(13, false);
    require((binding.IsNull() || schemaVersion >= 2) && (repair.IsNull() || schemaVersion >= 4) &&
            (datum.IsNull() || schemaVersion >= 6));
    require(label.NbChildren() == 4 + (binding.IsNull() ? 0 : 1) + (repair.IsNull() ? 0 : 1) +
                                      (datum.IsNull() ? 0 : 1));
    const auto plane = reals(label, 9);
    const auto elements = child(label, 10);
    const int count = integer(elements);
    require(count > 0 && count <= static_cast<int>(maximumSketchElementCount) &&
            static_cast<std::size_t>(count) <= remainingElements && elements.NbChildren() == count);
    SketchParameters sketch{{plane->Value(1), plane->Value(2), plane->Value(3), plane->Value(4),
                             plane->Value(5), plane->Value(6), plane->Value(7), plane->Value(8),
                             plane->Value(9)},
                            {},
                            {}};
    sketch.elements.nextId = number(child(label, 1), true);
    sketch.constraints.nextId = number(child(label, 2), true);
    for (int index = 1; index <= count; ++index)
        sketch.elements.values.push_back(readElement(child(elements, index)));
    sketch.constraints = readNativeSketchConstraints(
        child(label, 3), elements, sketch.elements, sketch.constraints.nextId,
        remainingElements - static_cast<std::size_t>(count), schemaVersion);
    if (!binding.IsNull())
        sketch.supportBinding = readNativeSketchSupport(
            child(label, 11),
            remainingElements - sketch.elements.values.size() - sketch.constraints.values.size());
    if (!repair.IsNull()) {
        require(repair.NbChildren() == 0 && repair.NbAttributes() == 1);
        sketch.repairReason = static_cast<SketchRepairReason>(integer(repair));
    }
    if (!datum.IsNull()) {
        require(datum.NbChildren() == 0 && datum.NbAttributes() == 1);
        const int value = integer(datum);
        require(value >= 1 && value <= 3);
        sketch.datum = static_cast<DatumPlane>(value);
    }
    validateNativeSketchGeometry(sketch);
    return sketch;
}
} // namespace voelund::document::detail
