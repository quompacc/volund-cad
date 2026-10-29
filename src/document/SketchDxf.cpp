// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/document/SketchDxf.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <numbers>
#include <variant>

namespace voelund::document {
namespace {

class DxfWriter final {
  public:
    void group(int code, const std::string& value) {
        out_ += std::to_string(code);
        out_ += '\n';
        out_ += value;
        out_ += '\n';
    }
    void group(int code, double value) {
        char buffer[32];
        const auto written = std::to_chars(buffer, buffer + sizeof buffer, value);
        group(code, std::string(buffer, written.ptr));
    }
    void group(int code, int value) { group(code, std::to_string(value)); }
    void point(int code, double x, double y) {
        group(code, x);
        group(code + 10, y);
        group(code + 20, 0.0);
    }
    // The entity's type, layer and subclass markers.
    void entity(const char* type, const char* subclass, const std::string& layer) {
        group(0, type);
        group(100, "AcDbEntity");
        group(8, layer);
        group(100, subclass);
    }
    [[nodiscard]] const std::string& text() const noexcept { return out_; }

  private:
    std::string out_;
};

double degrees(double radians) {
    double value = std::fmod(radians * 180.0 / std::numbers::pi, 360.0);
    return value < 0 ? value + 360.0 : value;
}

void line(DxfWriter& dxf, const std::string& layer, SketchPoint start, SketchPoint end) {
    dxf.entity("LINE", "AcDbLine", layer);
    dxf.point(10, start.x, start.y);
    dxf.point(11, end.x, end.y);
}

// Knots of the sketch's clamped uniform B-spline: 0 and n - degree with degree + 1 each.
void spline(DxfWriter& dxf, const std::string& layer, const SketchBSpline& curve) {
    const int count = static_cast<int>(curve.poles.size());
    const int degree = 3;
    const auto knots = bsplineKnots(curve); // N14c2: also a trimmed spline's own
    dxf.entity("SPLINE", "AcDbSpline", layer);
    dxf.group(210, 0.0);
    dxf.group(220, 0.0);
    dxf.group(230, 1.0);
    dxf.group(70, 8); // planar
    dxf.group(71, degree);
    dxf.group(72, static_cast<int>(knots.size()));
    dxf.group(73, count);
    dxf.group(74, 0);
    for (const double knot : knots)
        dxf.group(40, knot);
    for (const auto& pole : curve.poles)
        dxf.point(10, pole.x, pole.y);
}

struct EntityWriter final {
    DxfWriter& dxf;
    const std::string& layer;

    void operator()(const SketchLine& value) const { line(dxf, layer, value.start, value.end); }
    void operator()(const SketchRectangle& value) const {
        const SketchPoint corners[] = {value.first,
                                       {value.second.x, value.first.y},
                                       value.second,
                                       {value.first.x, value.second.y}};
        for (std::size_t index = 0; index < 4; ++index)
            line(dxf, layer, corners[index], corners[(index + 1) % 4]);
    }
    void operator()(const SketchCircle& value) const {
        dxf.entity("CIRCLE", "AcDbCircle", layer);
        dxf.point(10, value.center.x, value.center.y);
        dxf.group(40, value.radius);
    }
    void operator()(const SketchArc& value) const {
        dxf.entity("ARC", "AcDbCircle", layer);
        dxf.point(10, value.center.x, value.center.y);
        dxf.group(40, value.radius);
        dxf.group(100, "AcDbArc");
        dxf.group(50, degrees(value.startAngle));
        dxf.group(51, degrees(value.endAngle));
    }
    void operator()(const SketchEllipse& value) const { ellipse(value, 0.0, 2 * std::numbers::pi); }
    void operator()(const SketchEllipticArc& value) const { // N14c: same parameters as DXF
        ellipse(value.ellipse(), value.startParameter, value.endParameter);
    }
    void ellipse(const SketchEllipse& value, double start, double end) const {
        dxf.entity("ELLIPSE", "AcDbEllipse", layer);
        dxf.point(10, value.center.x, value.center.y);
        dxf.point(11, value.majorRadius * std::cos(value.rotation),
                  value.majorRadius * std::sin(value.rotation));
        dxf.group(40, value.minorRadius / value.majorRadius);
        dxf.group(41, start);
        dxf.group(42, end);
    }
    void operator()(const SketchPointGeometry& value) const {
        dxf.entity("POINT", "AcDbPoint", layer);
        dxf.point(10, value.position.x, value.position.y);
    }
    void operator()(const SketchBSpline& value) const { spline(dxf, layer, value); }
};

} // namespace

std::string sketchDxf(const SketchElements& elements) {
    DxfWriter dxf;
    dxf.group(0, "SECTION");
    dxf.group(2, "HEADER");
    dxf.group(9, "$ACADVER");
    dxf.group(1, "AC1015");
    dxf.group(9, "$INSUNITS");
    dxf.group(70, 4); // millimetres
    dxf.group(0, "ENDSEC");
    dxf.group(0, "SECTION");
    dxf.group(2, "ENTITIES");
    const std::string geometry = "0";
    const std::string construction = "Konstruktion";
    for (const auto& element : elements.values)
        std::visit(EntityWriter{dxf, element.construction ? construction : geometry},
                   element.geometry);
    dxf.group(0, "ENDSEC");
    dxf.group(0, "EOF");
    return dxf.text();
}

} // namespace voelund::document
