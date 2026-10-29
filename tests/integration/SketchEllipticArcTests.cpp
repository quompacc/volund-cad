// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// N14c (schema 54): elliptic arcs. Trimming an ellipse leaves an elliptic arc, its ends on the
// cutter; the half ellipse with its chord extrudes to half the ellipse's area; an elliptic arc
// splits into two joined ones and extends to the next edge along its ellipse; an ellipse cuts a
// line; extending or splitting a whole ellipse is refused; save and open keep the arc.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::SketchCurveEdit;
using document::SketchEllipse;
using document::SketchEllipticArc;
using document::SketchGeometry;
using document::SketchLine;
using document::SketchPoint;
using Kind = document::SketchConstraintKind;
using Position = document::SketchPointPosition;
constexpr double pi = std::numbers::pi;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b, double tolerance = 1e-6) { return std::abs(a - b) < tolerance; }
bool near(SketchPoint a, SketchPoint b) { return near(a.x, b.x) && near(a.y, b.y); }
document::SketchElementId id(std::uint64_t value) { return document::SketchElementId{value}; }
SketchPoint startOf(const SketchEllipticArc& arc) {
    return document::ellipsePoint(arc.ellipse(), arc.startParameter);
}
SketchPoint endOf(const SketchEllipticArc& arc) {
    return document::ellipsePoint(arc.ellipse(), arc.endParameter);
}

struct Fixture final {
    application::CadSession session;
    std::unique_ptr<application::SketchEditorSession> editor;
    explicit Fixture(const std::vector<SketchGeometry>& geometry) {
        session.newDocument();
        editor =
            std::make_unique<application::SketchEditorSession>(session, document::DatumPlane::xy);
        document::SketchElements elements;
        for (const auto& item : geometry)
            elements.values.push_back({document::SketchElementId{elements.nextId++}, item});
        require(editor->loadDraft(std::move(elements), {}), "Draft loads");
    }
    application::SketchCurveEditResult edit(SketchCurveEdit kind, std::uint64_t element,
                                            SketchPoint pick) {
        require(editor->selectNearestReference(id(element), pick, false), "Select");
        auto result = editor->editCurve(kind);
        require(editor->lastSolveResult().status == features::SketchSolveStatus::success,
                "Solved after the edit");
        editor->commitHistory();
        return result;
    }
    void refused(SketchCurveEdit kind, std::uint64_t element, SketchPoint pick,
                 const std::string& reason) {
        const auto before = editor->draftParameters();
        require(editor->selectNearestReference(id(element), pick, false), "Select");
        try {
            static_cast<void>(editor->editCurve(kind));
        } catch (const std::invalid_argument& error) {
            require(std::string(error.what()).find(reason) != std::string::npos,
                    "Expected \"" + reason + "\", got \"" + error.what() + "\"");
            require(editor->draftParameters() == before, "No change for " + reason);
            return;
        }
        throw std::runtime_error("Not refused: " + reason);
    }
    template <typename Geometry> [[nodiscard]] Geometry get(std::uint64_t element) const {
        for (const auto& item : editor->elements())
            if (item.id == id(element))
                return std::get<Geometry>(item.geometry);
        throw std::runtime_error("No element " + std::to_string(element));
    }
    [[nodiscard]] bool related(Kind kind, std::uint64_t first, std::uint64_t second) const {
        for (const auto& constraint : editor->constraints())
            if (constraint.kind == kind && constraint.second &&
                ((constraint.first.element == id(first) &&
                  constraint.second->element == id(second)) ||
                 (constraint.first.element == id(second) &&
                  constraint.second->element == id(first))))
                return true;
        return false;
    }
};

// Ellipse about (20; 0) with radii 4 and 2; its chord on y = 0 from (16; 0) to (24; 0).
void trimToHalf() {
    Fixture f({SketchEllipse{{20, 0}, 4, 2, 0}, SketchLine{{16, 0}, {24, 0}}});
    f.edit(SketchCurveEdit::trim, 1, {20, 2});
    const auto half = f.get<SketchEllipticArc>(1);
    require(near(startOf(half), {16, 0}) && near(endOf(half), {24, 0}) &&
                near(document::ellipsePoint(half.ellipse(),
                                            (half.startParameter + half.endParameter) / 2),
                     {20, -2}),
            "The lower half remains, from (16; 0) to (24; 0)");
    require(f.related(Kind::coincident, 1, 2), "Its ends are joined to the chord's ends");

    require(f.editor->tryAccept().status() == CommandStatus::committed, "Accepted");
    const auto sketch = f.session.features().back().id;
    const auto regions =
        features::identifySketchRegions(f.session.sketchParameters(sketch).elements.values).ids;
    require(regions.size() == 1, "One region");
    require(f.session.createSketchProfileExtrusion(sketch, regions, 10).status() ==
                CommandStatus::committed,
            "Extruded");
    double volume = 0;
    for (const auto& body : f.session.validBodies())
        volume += test::volumeOf(body.shape);
    require(near(volume, pi * 4 * 2 / 2 * 10, 1e-6 * volume),
            "Half the ellipse's area times 10: " + std::to_string(volume));

    const auto directory = std::filesystem::temp_directory_path() / "voelund-n14c-elliptic";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto file = directory / "elliptic.volund";
    f.session.saveDocument(file);
    application::CadSession reopened;
    reopened.loadDocument(file);
    require(reopened.sketchParameters(sketch) == f.session.sketchParameters(sketch),
            "Open restores the elliptic arc");
    std::filesystem::remove_all(directory);
}

void splitAndExtend() {
    Fixture f({SketchEllipticArc{{20, 0}, 4, 2, 0, 0, pi / 2}, SketchLine{{17, -5}, {17, 5}}});
    f.edit(SketchCurveEdit::extend, 1, {20.1, 2});
    const double t = std::acos(-0.75);
    require(near(endOf(f.get<SketchEllipticArc>(1)), {17, 2 * std::sin(t)}),
            "The end runs along the ellipse to x = 17");
    require(f.related(Kind::pointOnObject, 1, 2), "The new end lies on the line");

    f.edit(SketchCurveEdit::split, 1, document::ellipsePoint({{20, 0}, 4, 2, 0}, pi / 4));
    const auto first = f.get<SketchEllipticArc>(1), second = f.get<SketchEllipticArc>(3);
    require(near(startOf(first), {24, 0}) &&
                near(endOf(first), document::ellipsePoint(first.ellipse(), pi / 4)) &&
                near(startOf(second), endOf(first)) && near(endOf(second), {17, 2 * std::sin(t)}),
            "Two pieces meet at the split point");
    require(f.related(Kind::coincident, 1, 3), "Joined at the split point");
}

void ellipseCutsLine() {
    Fixture f({SketchEllipse{{20, 0}, 4, 2, 0}, SketchLine{{-10, 0}, {30, 0}}});
    const auto result = f.edit(SketchCurveEdit::trim, 2, {20, 0});
    require(result.created.has_value(), "The middle is cut out");
    require(near(f.get<SketchLine>(2).end, {16, 0}) && near(f.get<SketchLine>(3).start, {24, 0}),
            "The line ends at the ellipse and starts again behind it");

    f.refused(SketchCurveEdit::extend, 1, {20, 2}, "Ellipsen sind geschlossen");
    f.refused(SketchCurveEdit::split, 1, {20, 2}, "Ellipsen haben keinen Endpunkt");
    Fixture lone({SketchEllipse{{0, 0}, 4, 2, 0}, SketchLine{{-10, 0}, {-4, 0}}});
    lone.refused(SketchCurveEdit::trim, 1, {0, 2}, "Eine Ellipse braucht mindestens zwei");
}

} // namespace

int main() {
    try {
        trimToHalf();
        splitAndExtend();
        ellipseCutsLine();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
