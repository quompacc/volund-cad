#include "NativeSketchGeometry.hpp"
#include "DocumentSerializationDetail.hpp"
#include <voelund/document/IdCapacity.hpp>
#include <voelund/document/SketchPlaneValidation.hpp>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_RealArray.hxx>
#include <charconv>

namespace voelund::document::detail {
namespace {
void require(bool condition) {
    if (!condition) throw std::invalid_argument("Invalid or unsupported native sketch geometry");
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
    require(label.FindAttribute(TDataStd_AsciiString::GetID(), value) && value->Get().Length() <= 32);
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
    case SketchLineConstraint::none: return 0;
    case SketchLineConstraint::horizontal: return 1;
    case SketchLineConstraint::vertical: return 2;
    }
    throw std::invalid_argument("Unknown native line direction mode");
}
SketchLineConstraint lineMode(int code) {
    switch (code) {
    case 0: return SketchLineConstraint::none;
    case 1: return SketchLineConstraint::horizontal;
    case 2: return SketchLineConstraint::vertical;
    default: throw std::invalid_argument("Unknown native line direction mode");
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
            if (line) return SketchLine{{data->Value(1), data->Value(2)},
                {data->Value(3), data->Value(4)}, lineMode(integer(child(label, 5)))};
            return SketchRectangle{data->Value(1), data->Value(2), data->Value(3), data->Value(4)};
        }
        if (kind == "circle") {
            const auto data = reals(child(label, 4), 3);
            return SketchCircle{{data->Value(1), data->Value(2)}, data->Value(3)};
        }
        require(kind == "arc" || kind == "ellipse");
        const auto data = reals(child(label, 4), 5);
        if (kind == "arc") return SketchArc{{data->Value(1), data->Value(2)}, data->Value(3),
                                             data->Value(4), data->Value(5)};
        return SketchEllipse{{data->Value(1), data->Value(2)}, data->Value(3),
                              data->Value(4), data->Value(5)};
    }();
    return {SketchElementId{number(child(label, 1), false)}, geometry, construction != 0};
}
} // namespace
void validateNativeSketchGeometry(const SketchParameters& sketch) {
    require(sketch.constraints.values.empty() && sketch.elements.values.size() <= maximumSketchElementCount);
    // Preserve legacy axis magnitudes; canonicalization remains an explicit edit.
    validateSketchPlane(sketch.plane);
    validateSketchElements(sketch.elements);
    validateSketchConstraints(sketch.elements, sketch.constraints);
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
    TDataStd_Integer::Set(label.FindChild(3), 0); // Non-empty constraints not supported yet.
    const auto elements = label.FindChild(10);
    TDataStd_Integer::Set(elements, static_cast<int>(sketch.elements.values.size()));
    for (std::size_t index = 0; index < sketch.elements.values.size(); ++index)
        writeElement(elements.FindChild(static_cast<int>(index + 1)), sketch.elements.values[index]);
}
SketchParameters readNativeSketchGeometry(const TDF_Label& label, std::size_t remainingElements) {
    require(label.NbChildren() == 4 && integer(child(label, 3)) == 0);
    const auto plane = reals(label, 9);
    const auto elements = child(label, 10);
    const int count = integer(elements);
    require(count > 0 && count <= static_cast<int>(maximumSketchElementCount) &&
            static_cast<std::size_t>(count) <= remainingElements && elements.NbChildren() == count);
    SketchParameters sketch{{plane->Value(1), plane->Value(2), plane->Value(3), plane->Value(4),
        plane->Value(5), plane->Value(6), plane->Value(7), plane->Value(8), plane->Value(9)}, {}, {}};
    sketch.elements.nextId = number(child(label, 1), true);
    sketch.constraints.nextId = number(child(label, 2), true);
    for (int index = 1; index <= count; ++index)
        sketch.elements.values.push_back(readElement(child(elements, index)));
    validateNativeSketchGeometry(sketch);
    return sketch;
}
} // namespace voelund::document::detail
