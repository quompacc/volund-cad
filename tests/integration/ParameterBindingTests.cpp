#include "CadSessionTestSupport.hpp"

#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

// PAR02 (docs/parameter-table.md): binding in the sketch editor and in a new extrusion. A line's
// length bound to A = 20 takes 20; an angle parameter is refused for it; a typed 15 unbinds it
// (rule 7); bound again it follows A. Parameters created while the editor is open do not keep
// it from accepting, and the stored sketch keeps the binding. A new extrusion bound to C = 8
// takes 8 and follows C to 12; undo returns to 8.
namespace {
using namespace voelund;
using application::CommandStatus;
using document::ParameterUnit;
using document::SketchConstraintKind;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }

document::ParameterId parameterNamed(const application::CadSession& session,
                                     const std::string& name) {
    for (const auto& entry : session.parameterTable().values)
        if (entry.name == name)
            return entry.id;
    throw std::runtime_error("Parameter " + name);
}
double lineLength(const application::SketchEditorSession& editor) {
    const auto& line = std::get<document::SketchLine>(editor.elements().front().geometry);
    return std::hypot(line.end.x - line.start.x, line.end.y - line.start.y);
}
double volume(const application::CadSession& session) {
    double total = 0;
    for (const auto& body : session.validBodies())
        total += test::volumeOf(body.shape);
    return total;
}

void editorBinding() {
    application::CadSession session;
    session.newDocument();
    require(session.createParameter("A", ParameterUnit::length, 20).status() ==
                CommandStatus::committed,
            "A = 20");
    const auto a = parameterNamed(session, "A");
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(application::SketchDrawingTool::line);
    require(editor.setPreview({0, 0}, {10, 0}) && editor.confirmPreview().has_value(), "Line");
    editor.commitHistory();
    const document::SketchElementId line{1};
    require(editor.selectElement(line) && editor.setSelectedLineLength(10), "Length dimension");
    const auto length = editor.selectedDimension(SketchConstraintKind::distance);
    require(length.has_value(), "The line has its length dimension");

    require(editor.bindDimension(*length, a) && near(lineLength(editor), 20) &&
                editor.dimensionParameter(*length) == a,
            "Bound to A, the line is 20 long");
    // An angle parameter, created while the editor is open, does not fit a length.
    require(session.createParameter("W", ParameterUnit::angle, 30).status() ==
                CommandStatus::committed,
            "W = 30 degrees");
    editor.acceptParameterTableChange();
    bool refused = false;
    try {
        static_cast<void>(editor.bindDimension(*length, parameterNamed(session, "W")));
    } catch (const std::invalid_argument& error) {
        refused = std::string(error.what()).find("Winkel") != std::string::npos;
    }
    require(refused && editor.dimensionParameter(*length) == a, "An angle is refused for a length");
    require(editor.editDimension(*length, 15) && near(lineLength(editor), 15) &&
                !editor.dimensionParameter(*length),
            "A typed number unbinds (rule 7)");
    require(editor.bindDimension(*length, a) && near(lineLength(editor), 20), "Bound again");
    require(editor.setSelectedLineLength(18) && !editor.dimensionParameter(*length),
            "The length field unbinds as well");
    require(editor.bindDimension(*length, a), "Bound once more");

    const auto accepted = editor.tryAccept();
    require(accepted.status() == CommandStatus::committed && accepted.feature().has_value(),
            "The editor accepts although parameters were created meanwhile: " +
                accepted.diagnostic());
    const auto stored = session.sketchParameters(*accepted.feature());
    bool kept = false;
    for (const auto& constraint : stored.constraints.values)
        kept = kept || (constraint.id == *length && constraint.parameter == a);
    require(kept, "The stored sketch keeps the binding");
    require(session.setParameterValue(a, 25).status() != CommandStatus::rejected,
            "A = 25");
    const auto storedAfter = session.sketchParameters(*accepted.feature());
    const auto moved = std::get<document::SketchLine>(storedAfter.elements.values.front().geometry);
    require(near(std::hypot(moved.end.x - moved.start.x, moved.end.y - moved.start.y), 25),
            "Changing A changes the stored line: " +
                std::to_string(std::hypot(moved.end.x - moved.start.x, moved.end.y - moved.start.y)));
}

void extrusionBinding() {
    application::CadSession session;
    session.newDocument();
    require(session.createParameter("C", ParameterUnit::length, 8).status() ==
                CommandStatus::committed,
            "C = 8");
    const auto c = parameterNamed(session, "C");
    document::SketchParameters square{document::datumSketchPlane(document::DatumPlane::xy), {}};
    square.datum = document::DatumPlane::xy;
    square.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchRectangle{0, 0, 40, 20}});
    square.elements.nextId = 2;
    const auto sketch = session.createDatumSketchFeature(square).feature();
    require(sketch.has_value(), "Sketch");
    application::ExtrusionMode mode;
    mode.distanceParameter = c;
    const auto regions = features::identifySketchRegions(square.elements.values).ids;
    const auto extrusion = session.createSketchProfileExtrusion(*sketch, regions, 3, mode);
    require(extrusion.status() == CommandStatus::committed && near(volume(session), 40 * 20 * 8) &&
                session.extrusionParameters(*extrusion.feature()).distanceParameter == c,
            "A new extrusion bound to C takes 8, not the typed 3");
    require(session.setParameterValue(c, 12).status() == CommandStatus::committed &&
                near(volume(session), 40 * 20 * 12),
            "C = 12 changes the volume");
    require(session.undo().status() == CommandStatus::committed &&
                near(volume(session), 40 * 20 * 8),
            "Undo returns to 8");
}

} // namespace

int main() {
    try {
        editorBinding();
        extrusionBinding();
        std::cout << "PASS PAR02 binding: sketch dimension bound, refused for the wrong unit, "
                     "unbound by a number, accepted after parameters created meanwhile; new "
                     "extrusion bound to a parameter, value change and undo\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
