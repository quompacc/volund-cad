#include "../CommandResultAssertions.hpp"
#include "../TestPaths.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// S14 (A-SKETCH-01): one row per active sketch tool and edit operation. Each row checks, in
// the editor the product uses: preview (drawing tools), the actual change, a later edit,
// undo/redo as exactly one editor step, the solve state (DOF, one annotation per relation),
// and the round trip accept -> save -> open -> reopen in the editor with identical geometry,
// relations and patterns. Snap, labels in the view and refusals per curve type have their
// own tests (sketch_snap_matrix, sketch_selection, sketch_curve_edit, ...); the S14 evidence
// lists them per row.
namespace {
using namespace voelund;
using application::SketchCreationInput;
using application::SketchCurveEdit;
using application::SketchDrawingTool;
using application::SketchEditorSession;
using document::SketchElementId;
using document::SketchGeometry;
using document::SketchLine;
using document::SketchPatternKind;
using document::SketchPoint;
using Kind = document::SketchConstraintKind;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

bool same(const document::SketchParameters& a, const document::SketchParameters& b) {
    return a.elements == b.elements && a.constraints == b.constraints && a.patterns == b.patterns;
}

// Accept, save, open, reopen: the stored sketch and a new editor on it equal the accepted one.
void roundTrip(application::CadSession& session, SketchEditorSession& editor,
               const std::string& row) {
    require(editor.canAccept(), row + ": acceptable");
    const auto accepted = editor.draftParameters();
    static_cast<void>(test::requireCommittedFeature(editor.tryAccept()));
    const auto path = test::temporaryFilePath("voelund-sketch-matrix.volund");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    const auto sketch = loaded.features().back().id;
    require(loaded.selectFeature(sketch), row + ": stored sketch selectable");
    const auto stored = loaded.selectedSketchParameters();
    require(same(stored, accepted), row + ": save/open keeps geometry, relations, patterns");
    SketchEditorSession reopened(loaded, sketch, stored);
    require(same(reopened.draftParameters(), accepted) && reopened.canAccept(),
            row + ": the reopened editor shows the same sketch");
}

// The solve state the panel and the labels show.
void solved(const SketchEditorSession& editor, const std::string& row) {
    const auto& solve = editor.lastSolveResult();
    require(solve.status == features::SketchSolveStatus::success && solve.degreesOfFreedom >= 0,
            row + ": solved with a degree-of-freedom count");
    require(editor.evaluatedConstraints().size() == editor.constraints().size(),
            row + ": one annotation per relation");
}

struct DrawingRow {
    const char* name;
    SketchDrawingTool tool;
    SketchCreationInput input;
};

void drawingRow(const DrawingRow& row) {
    application::CadSession session;
    session.newDocument();
    SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(row.tool);
    require(editor.setPreview(row.input.first, row.input.second, row.input.third) &&
                !editor.evaluatedElements().empty(),
            std::string(row.name) + ": live preview");
    editor.clearPreview();
    require(editor.elements().empty(), std::string(row.name) + ": the preview is no element");

    const auto created = editor.create(row.input);
    require(created.has_value() && !editor.elements().empty(), std::string(row.name) + ": created");
    editor.commitHistory(); // as the shell does after each step
    solved(editor, row.name);
    const auto afterCreate = editor.draftParameters();

    // Edit: move the whole shape, as a selection box and a drag would.
    std::vector<SketchElementId> all;
    for (const auto& element : editor.elements())
        all.push_back(element.id);
    require(editor.selectElements(all, false) && editor.translateSelected(2.0, 1.0),
            std::string(row.name) + ": moved");
    editor.commitHistory();
    solved(editor, row.name);
    const auto afterEdit = editor.draftParameters();
    require(!same(afterEdit, afterCreate), std::string(row.name) + ": the move changed it");

    require(editor.undo() && same(editor.draftParameters(), afterCreate),
            std::string(row.name) + ": one undo reverts the move");
    require(editor.undo() && editor.elements().empty(),
            std::string(row.name) + ": the next undo removes the shape as one step");
    require(editor.redo() && editor.redo() && same(editor.draftParameters(), afterEdit),
            std::string(row.name) + ": redo restores both steps");
    roundTrip(session, editor, row.name);
}

// An edit operation on a prepared draft: it changes the sketch, is one undo step, redo
// restores it, and the result survives save/open.
struct OperationRow {
    const char* name;
    std::vector<SketchGeometry> geometry;
    std::function<void(SketchEditorSession&)> operation;
    // Steps before the checked one (a pattern to edit, a lock to release), each its own
    // undo step.
    std::function<void(SketchEditorSession&)> prepare{};
};

void operationRow(const OperationRow& row) {
    application::CadSession session;
    session.newDocument();
    SketchEditorSession editor(session, document::DatumPlane::xy);
    document::SketchElements elements;
    for (const auto& item : row.geometry)
        elements.values.push_back({SketchElementId{elements.nextId++}, item});
    require(editor.loadDraft(std::move(elements), {}), std::string(row.name) + ": draft");
    if (row.prepare) {
        row.prepare(editor);
        editor.commitHistory();
    }
    const auto before = editor.draftParameters();
    row.operation(editor);
    editor.commitHistory();
    solved(editor, row.name);
    const auto after = editor.draftParameters();
    require(!same(after, before), std::string(row.name) + ": changed the sketch");
    require(editor.undo() && same(editor.draftParameters(), before),
            std::string(row.name) + ": one undo restores the sketch");
    require(editor.redo() && same(editor.draftParameters(), after),
            std::string(row.name) + ": redo repeats it");
    roundTrip(session, editor, row.name);
}

SketchElementId id(std::uint64_t value) { return SketchElementId{value}; }
void pick(SketchEditorSession& editor, std::uint64_t element, SketchPoint at, bool add = false) {
    require(editor.selectNearestReference(id(element), at, add), "Select element");
}
std::uint64_t pattern(SketchEditorSession& editor, SketchPatternKind kind, int count,
                      double spacing, double angle) {
    pick(editor, 1, {0, 0});
    const auto created = editor.createPattern(kind, count, spacing, angle);
    require(created.has_value(), "Pattern created");
    editor.commitHistory();
    return *created;
}

int run() {
    // Centers away from the origin, which would bind them (and forbid the move).
    SketchCreationInput centered{{3, 2}, {8, 5}};
    centered.centered = true;
    const std::vector<DrawingRow> drawing{
        {"line", SketchDrawingTool::line, {{0, 0}, {10, 3}}},
        {"polyline", SketchDrawingTool::polyline, {{0, 0}, {10, 0}}},
        {"rectangle", SketchDrawingTool::rectangle, {{0, 0}, {10, 6}}},
        {"center rectangle", SketchDrawingTool::rectangle, centered},
        {"circle", SketchDrawingTool::circle, {{0, 0}, {5, 0}}},
        {"center arc", SketchDrawingTool::arc, {{0, 0}, {5, 0}, SketchPoint{0, 5}}},
        {"three-point arc", SketchDrawingTool::threePointArc, {{5, 0}, {-5, 0}, SketchPoint{0, 5}}},
        {"ellipse", SketchDrawingTool::ellipse, {{0, 0}, {8, 0}, SketchPoint{0, 3}}},
        {"slot", SketchDrawingTool::slot, {{0, 0}, {12, 0}, SketchPoint{0, 2}}},
        {"polygon", SketchDrawingTool::polygon, {{3, 2}, {9, 2}}}};
    for (const auto& row : drawing)
        drawingRow(row);

    const SketchLine base{{0, 0}, {10, 0}};
    const SketchLine cross{{6, -5}, {6, 5}};
    const std::vector<OperationRow> operations{
        {"trim",
         {base, cross},
         [](auto& e) {
             pick(e, 1, {9, 0});
             static_cast<void>(e.editCurve(SketchCurveEdit::trim));
         }},
        {"extend",
         {SketchLine{{0, 0}, {4, 0}}, cross},
         [](auto& e) {
             pick(e, 1, {4, 0});
             static_cast<void>(e.editCurve(SketchCurveEdit::extend));
         }},
        {"split",
         {base},
         [](auto& e) {
             pick(e, 1, {3, 0});
             static_cast<void>(e.editCurve(SketchCurveEdit::split));
         }},
        {"fillet",
         {base, SketchLine{{0, 0}, {0, 10}}},
         [](auto& e) {
             pick(e, 1, {5, 0});
             pick(e, 2, {0, 5}, true);
             static_cast<void>(e.filletCorner(2.0));
         }},
        {"offset",
         {base},
         [](auto& e) {
             pick(e, 1, {5, 0});
             require(e.offsetSelected(2.0), "Offset");
         }},
        {"mirror",
         {base, SketchLine{{-5, 5}, {15, 5}}},
         [](auto& e) {
             pick(e, 1, {5, 0});
             pick(e, 2, {5, 5}, true);
             require(e.mirrorSelected(), "Mirror");
         }},
        {"linear pattern",
         {base},
         [](auto& e) { pattern(e, SketchPatternKind::linear, 3, 15.0, 0.0); }},
        {"circular pattern",
         {base},
         [](auto& e) { pattern(e, SketchPatternKind::circular, 4, 0.0, 360.0); }},
        {"edit pattern",
         {base},
         [](auto& e) { require(e.editPattern(e.patterns().front().id, 4, 12.0, 0.0), "Edit"); },
         [](auto& e) { pattern(e, SketchPatternKind::linear, 2, 15.0, 0.0); }},
        {"dissolve pattern",
         {base},
         [](auto& e) { require(e.dissolvePattern(e.patterns().front().id), "Dissolve"); },
         [](auto& e) { pattern(e, SketchPatternKind::linear, 3, 15.0, 0.0); }},
        {"construction",
         {base},
         [](auto& e) {
             pick(e, 1, {5, 0});
             require(e.toggleSelectedConstruction(), "Construction");
         }},
        {"lock point",
         {base},
         [](auto& e) {
             pick(e, 1, {0, 0});
             require(e.lockSelectedReference(), "Lock");
         }},
        {"unlock point",
         {base},
         [](auto& e) {
             pick(e, 1, {0, 0});
             require(e.unlockSelectedReference(), "Unlock");
         },
         [](auto& e) {
             pick(e, 1, {0, 0});
             require(e.lockSelectedReference(), "Lock first");
         }},
        {"relation",
         {SketchLine{{0, 0}, {10, 1}}},
         [](auto& e) {
             require(e.selectElement(id(1)), "Select the line");
             require(e.setSelectedLineConstraint(document::SketchLineConstraint::horizontal),
                     "Horizontal");
         }},
        {"parallel",
         {base, SketchLine{{0, 5}, {10, 7}}},
         [](auto& e) {
             require(e.selectElement(id(1)), "First line");
             require(e.selectNearestReference(id(2), {5, 6}, true), "Second line");
             require(e.addSelectedRelation(Kind::parallel), "Parallel");
         }},
        {"remove relation",
         {SketchLine{{0, 0}, {10, 1}}},
         [](auto& e) { require(e.removeConstraint(e.constraints().back().id), "Remove"); },
         [](auto& e) {
             require(e.selectElement(id(1)), "Select the line");
             require(e.setSelectedLineConstraint(document::SketchLineConstraint::horizontal),
                     "Horizontal first");
         }},
        {"delete",
         {base, cross},
         [](auto& e) {
             pick(e, 2, {6, 0});
             require(e.removeSelected(), "Delete");
         }},
        {"move",
         {base},
         [](auto& e) {
             pick(e, 1, {5, 0});
             require(e.translateSelected(3.0, 4.0), "Move");
         }},
        // A-TXN-03 in the editor: 100 drag updates are one undo step.
        {"drag",
         {base},
         [](auto& e) {
             pick(e, 1, {5, 0});
             e.beginDrag();
             for (int step = 1; step <= 100; ++step)
                 require(e.dragSelected(0.03 * step, 0.02 * step), "Drag update");
             e.endDrag();
         }},
    };
    for (const auto& row : operations) {
        try {
            operationRow(row);
        } catch (const std::exception& error) {
            throw std::runtime_error(std::string(row.name) + ": " + error.what());
        }
    }
    std::cout << "PASS S14 sketch tool matrix: " << drawing.size() << " drawing tools, "
              << operations.size() << " operations\n";
    return 0;
}
} // namespace

int main() {
    try {
        return run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
