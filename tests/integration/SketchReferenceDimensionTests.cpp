#include "../CommandResultAssertions.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// S18 (user decision 2026-10-14): reference dimensions. A distance switched to reference shows
// what the line measures while two driving distances move the end; switching it back would
// overdetermine and is refused; the same distance added again becomes a reference by itself;
// the label is in parentheses, the value cannot be typed; undo, save and open (schema 24).
namespace {
using namespace voelund;
using application::CommandStatus;
using document::SketchConstraintKind;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-7; }

const document::SketchConstraint& constraint(const application::SketchEditorSession& editor,
                                             document::SketchConstraintId id) {
    for (const auto& candidate : editor.constraints())
        if (candidate.id == id)
            return candidate;
    throw std::runtime_error("Constraint missing");
}

document::SketchConstraintId dimension(application::SketchEditorSession& editor,
                                       SketchConstraintKind kind) {
    const document::SketchElementId line{1};
    require(editor.selectNearestReference(line, {0, 0}, false) &&
                editor.selectNearestReference(line, {100, 100}, true) &&
                editor.addSelectedRelation(kind),
            "Dimension between the line ends");
    editor.commitHistory(); // as the sketch panel does after each command
    return editor.constraints().back().id;
}

void references() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(application::SketchDrawingTool::line);
    require(editor.setPreview({0, 0}, {10, 0}) && editor.confirmPreview().has_value(), "Line");
    editor.commitHistory();

    const auto length = dimension(editor, SketchConstraintKind::pointDistance);
    require(!constraint(editor, length).reference && near(constraint(editor, length).value, 10),
            "A new distance drives");
    editor.toggleReference(length);
    require(editor.isReferenceDimension(length) && editor.dimensionProblem(length).has_value() &&
                !editor.setConstraintValue(length, 12),
            "A reference cannot be typed");
    std::string label;
    for (const auto& annotation : editor.evaluatedConstraints())
        if (annotation.id == length)
            label = annotation.label;
    require(label.starts_with("(") && label.ends_with(")") && !label.empty(),
            "A reference is shown in parentheses");

    const auto x = dimension(editor, SketchConstraintKind::pointDistanceX);
    const auto y = dimension(editor, SketchConstraintKind::pointDistanceY);
    require(editor.setConstraintValue(x, 15) && editor.setConstraintValue(y, 8),
            "Two driving distances move the end");
    editor.commitHistory();
    require(near(constraint(editor, length).value, 17), "The reference follows: 17");

    bool refused = false;
    try {
        editor.toggleReference(length);
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused && editor.isReferenceDimension(length),
            "Driving it would overdetermine; it stays a reference");

    const auto again = dimension(editor, SketchConstraintKind::pointDistance);
    require(constraint(editor, again).reference && near(constraint(editor, again).value, 17) &&
                editor.lastSolveResult().status == features::SketchSolveStatus::success,
            "The same distance again becomes a reference by itself");
    require(editor.undo() && !editor.constraints().empty() && editor.constraints().back().id == y,
            "Undo removes it");

    require(editor.tryAccept().status() == CommandStatus::committed, "Accepted");
    const auto sketch = session.features().back().id;
    const auto stored = session.sketchParameters(sketch).constraints;

    const auto directory = std::filesystem::temp_directory_path() / "voelund-s18-reference";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "reference.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.sketchParameters(sketch).constraints == stored &&
                reopened.sketchParameters(sketch).constraints.values.front().reference,
            "Open restores the reference");
    std::filesystem::remove_all(directory);

    // Without the vertical distance it can drive again; the line keeps 17.
    application::SketchEditorSession edit(reopened, sketch, reopened.sketchParameters(sketch));
    require(edit.removeConstraint(y), "Remove the vertical distance");
    edit.toggleReference(length);
    require(!edit.isReferenceDimension(length) && near(constraint(edit, length).value, 17) &&
                edit.lastSolveResult().status == features::SketchSolveStatus::success,
            "Driving again");
}

} // namespace

int main() {
    try {
        references();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
