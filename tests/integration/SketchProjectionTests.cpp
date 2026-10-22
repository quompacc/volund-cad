// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../CommandResultAssertions.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

// S17 (user decision 2026-10-14): a body edge projected into a sketch. It becomes a fixed
// construction line that follows the box when the box changes; a drawn line joined to it
// follows too; a second projection of the same edge, trimming and an edge across the plane are
// refused; undo/redo and delete take the projection with the element; save/open keep it
// (schema 23). An edge whose source sketch line is gone is a localized error on the sketch.
namespace {
using namespace voelund;
using application::CommandStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

// The top edge of the box along x, and a vertical edge (across an XY sketch).
TopoDS_Shape boxEdge(const application::CadSession& session, bool vertical) {
    for (TopExp_Explorer edges(session.validBodies().front().shape, TopAbs_EDGE); edges.More();
         edges.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(edges.Current()));
        if (curve.GetType() != GeomAbs_Line)
            continue;
        const auto direction = curve.Line().Direction();
        const auto middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (vertical ? std::abs(direction.Z()) > 0.99
                     : std::abs(direction.X()) > 0.99 && middle.Z() > 5 && middle.Y() < 1)
            return edges.Current();
    }
    throw std::runtime_error("Box edge missing");
}

double lengthOf(const document::SketchGeometry& geometry) {
    const auto& line = std::get<document::SketchLine>(geometry);
    return std::hypot(line.end.x - line.start.x, line.end.y - line.start.y);
}

const document::SketchElement& element(std::span<const document::SketchElement> elements,
                                       std::uint64_t id) {
    for (const auto& candidate : elements)
        if (candidate.id.value() == id)
            return candidate;
    throw std::runtime_error("Element missing");
}

bool refused(const std::function<void()>& edit) {
    try {
        edit();
    } catch (const application::SketchEditRefusal&) {
        return true;
    }
    return false;
}

void projection() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 10}).feature();

    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    const auto projected = editor.projectEdge(box, boxEdge(session, false)).created;
    require(projected.has_value() && editor.projections().size() == 1 &&
                editor.isProjected(*projected),
            "The top edge is projected");
    const auto& line = element(editor.elements(), projected->value());
    require(line.construction && near(lengthOf(line.geometry), 40),
            "A construction line of the edge length");
    require(refused([&] { static_cast<void>(editor.projectEdge(box, boxEdge(session, false))); }),
            "The same edge twice is refused");
    require(refused([&] { static_cast<void>(editor.projectEdge(box, boxEdge(session, true))); }),
            "An edge across the sketch plane is refused");
    const auto start = std::get<document::SketchLine>(line.geometry).start;
    require(editor.selectNearestReference(*projected, start, false) && refused([&] {
                static_cast<void>(editor.editCurve(application::SketchCurveEdit::split));
            }),
            "A projected line is not split");

    require(editor.undo() && editor.projections().empty() && editor.elements().empty() &&
                editor.redo() && editor.projections().size() == 1,
            "Undo and redo take the projection with the element");

    // A drawn line joined to the start of the projection; the projection stays fixed.
    editor.setTool(application::SketchDrawingTool::line);
    require(editor.setPreview({start.x - 20, start.y - 20}, {start.x + 1, start.y + 1}) &&
                editor.confirmPreview().has_value(),
            "A drawn line");
    const auto drawn = editor.elements().back().id;
    require(editor.selectNearestReference(drawn, {start.x + 1, start.y + 1}, false) &&
                editor.selectNearestReference(*projected, start, true) &&
                editor.addSelectedRelation(document::SketchConstraintKind::coincident),
            "The drawn line ends on the projection");
    const auto& joined =
        std::get<document::SketchLine>(element(editor.elements(), drawn.value()).geometry);
    require(near(joined.end.x, start.x) && near(joined.end.y, start.y),
            "The drawn line moved, not the projection");
    require(editor.tryAccept().status() == CommandStatus::committed, "The sketch is accepted");
    const auto sketch = session.features().back().id;
    require(session.sketchParameters(sketch).projections.size() == 1, "The projection is stored");

    // A wider box: the projection and the joined line follow.
    auto wider = session.identifiedSnapshot().snapshot().boxParameters(box);
    wider.dimensions.width = 50;
    require(session.editBoxParameters(box, wider).status() == CommandStatus::committed,
            "Widen the box");
    const auto evaluated = session.evaluatedSketch(sketch);
    require(
        evaluated.has_value() &&
            near(lengthOf(element(evaluated->elements.values, projected->value()).geometry), 50),
        "The projection follows the box");
    const auto& follow = std::get<document::SketchLine>(
        element(evaluated->elements.values, projected->value()).geometry);
    const auto& end =
        std::get<document::SketchLine>(element(evaluated->elements.values, drawn.value()).geometry);
    require(near(end.end.x, follow.start.x) && near(end.end.y, follow.start.y),
            "The joined line follows the projection");

    // Editing shows the current edge; deleting the element removes the projection.
    {
        application::SketchEditorSession again(session, sketch, session.sketchParameters(sketch));
        require(again.projections().size() == 1 &&
                    near(lengthOf(element(again.elements(), projected->value()).geometry), 50),
                "The editor opens with the current edge");
        require(again.selectElement(*projected) && again.removeSelected() &&
                    again.projections().empty(),
                "Deleting the element removes the projection");
    }

    const auto directory = std::filesystem::temp_directory_path() / "voelund-s17-projection";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "projection.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.sketchParameters(sketch).projections ==
                    session.sketchParameters(sketch).projections &&
                reopened.evaluatedSketch(sketch).has_value(),
            "Open restores the projection");
    std::filesystem::remove_all(directory);
}

// The source of a projected edge goes away: its extruded rectangle becomes a circle.
void missingSource() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession base(session, document::DatumPlane::xy);
    base.setTool(application::SketchDrawingTool::rectangle);
    require(base.setPreview({0, 0}, {40, 30}) && base.confirmPreview().has_value() &&
                base.tryAccept().status() == CommandStatus::committed,
            "A rectangle sketch");
    const auto profile = session.features().back().id;
    const auto regions =
        features::identifySketchRegions(session.sketchParameters(profile).elements.values).ids;
    require(session.createSketchProfileExtrusion(profile, regions, 10).status() ==
                CommandStatus::committed,
            "Extruded");
    const auto body = session.features().back().id;

    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    require(editor.projectEdge(body, boxEdge(session, false)).created.has_value() &&
                editor.tryAccept().status() == CommandStatus::committed,
            "The top edge of the extrusion is projected");
    const auto sketch = session.features().back().id;

    auto circle = session.sketchParameters(profile);
    circle.elements.values = {
        {document::SketchElementId{circle.elements.nextId}, document::SketchCircle{{20, 15}, 10}}};
    ++circle.elements.nextId;
    circle.constraints.values.clear();
    const auto changed = session.editSketchParameters(
        profile, circle, application::ErrorDisposition::keepEditableErrors);
    bool blocked = false;
    for (const auto& issue : changed.evaluation().features)
        blocked = blocked || (issue.feature == sketch && issue.code == "blocked-by-dependency");
    require(blocked, "The sketch shows that its source failed");
}

// A projection naming two box faces without a common edge (top and bottom): a missing edge,
// reported on the sketch and its element.
void missingEdge() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 10}).feature();
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    const auto projected = editor.projectEdge(box, boxEdge(session, false)).created;
    require(projected.has_value() && editor.tryAccept().status() == CommandStatus::committed,
            "Projected");
    const auto sketch = session.features().back().id;
    std::optional<document::SubshapeOrigin> top, bottom;
    for (const auto& face : session.supportFaces(box)) {
        if (face.plane.normalZ > 0.5)
            top = face.origin;
        else if (face.plane.normalZ < -0.5)
            bottom = face.origin;
    }
    require(top && bottom, "Top and bottom faces");
    auto parameters = session.sketchParameters(sketch);
    parameters.projections.front().edge = {*top, *bottom, 1};
    const auto changed = session.editSketchParameters(
        sketch, parameters, application::ErrorDisposition::keepEditableErrors);
    bool localized = false;
    for (const auto& issue : changed.evaluation().features)
        localized = localized || (issue.feature == sketch && issue.code == "projection-missing" &&
                                  issue.elements == std::vector{projected->value()});
    require(localized, "The sketch reports the missing edge at its element");
}

} // namespace

int main() {
    try {
        projection();
        missingSource();
        missingEdge();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
