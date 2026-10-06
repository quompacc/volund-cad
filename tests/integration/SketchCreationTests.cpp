#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>

// U08: one creation path. For every drawing tool the editor's create() and the plain
// preview/confirm steps (plus the same explicit references) give the same editor state; the
// result names the created elements by role, and the roles point at the right geometry, so
// no caller needs IDs from insertion order.
namespace {
using namespace voelund;
using application::SketchCreationInput;
using application::SketchCreationRole;
using application::SketchDrawingTool;
using application::SketchEditorSession;
using application::SketchSnap;
using application::SketchSnapKind;
using document::SketchPoint;
using document::SketchPointPosition;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool same(const SketchEditorSession& a, const SketchEditorSession& b) {
    return std::ranges::equal(a.elements(), b.elements()) &&
           std::ranges::equal(a.constraints(), b.constraints());
}
bool near(SketchPoint a, SketchPoint b) { return std::hypot(a.x - b.x, a.y - b.y) < 1e-9; }
const document::SketchGeometry& geometryOf(const SketchEditorSession& editor,
                                           document::SketchElementId id) {
    return std::ranges::find(editor.elements(), id, &document::SketchElement::id)->geometry;
}

struct Case final {
    const char* name;
    SketchDrawingTool tool;
    SketchCreationInput input;
};

// Without snaps, create() is exactly preview + confirm.
void plainStepsMatch(application::CadSession& session) {
    const Case cases[] = {
        {"line", SketchDrawingTool::line, {{0, 0}, {10, 3}}},
        {"rectangle", SketchDrawingTool::rectangle, {{1, 2}, {9, 7}}},
        {"circle", SketchDrawingTool::circle, {{2, 2}, {6, 2}}},
        {"arc", SketchDrawingTool::arc, {{0, 0}, {5, 0}, SketchPoint{0, 5}}},
        {"three-point arc", SketchDrawingTool::threePointArc, {{5, 0}, {0, 5}, SketchPoint{-5, 0}}},
        {"ellipse", SketchDrawingTool::ellipse, {{0, 0}, {8, 0}, SketchPoint{0, 3}}},
        {"slot", SketchDrawingTool::slot, {{0, 0}, {10, 0}, SketchPoint{0, 2}}},
        {"polygon", SketchDrawingTool::polygon, {{0, 0}, {6, 0}}},
    };
    for (const auto& item : cases) {
        SketchEditorSession viaCreate(session, document::DatumPlane::xy);
        SketchEditorSession viaSteps(session, document::DatumPlane::xy);
        viaCreate.setTool(item.tool);
        viaSteps.setTool(item.tool);
        // A free line gets its automatic relation in both paths.
        const auto created = viaCreate.create(item.input);
        require(viaSteps.setPreview(item.input.first, item.input.second, item.input.third),
                std::string(item.name) + ": preview");
        const auto confirmed = viaSteps.confirmCreation();
        if (item.tool == SketchDrawingTool::line)
            static_cast<void>(viaSteps.addAutomaticLineRelation(confirmed->primary()));
        require(created && confirmed && *created == *confirmed && same(viaCreate, viaSteps),
                std::string(item.name) + ": create() differs from the plain steps");
    }
}

// Roles point at geometry, not at positions in the element list.
void rolesNameGeometry(application::CadSession& session) {
    SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(SketchDrawingTool::rectangle);
    const auto rectangle = editor.create({{1, 2}, {9, 7}});
    const auto fromSecond = rectangle->find(SketchCreationRole::rectangleFromSecondCorner);
    require(rectangle->elements.size() == 4 && fromSecond &&
                near(std::get<document::SketchLine>(geometryOf(editor, *fromSecond)).start, {9, 7}) &&
                near(std::get<document::SketchLine>(geometryOf(editor, rectangle->primary())).start,
                     {1, 2}),
            "Rectangle roles name the corner edges");
    editor.setTool(SketchDrawingTool::slot);
    const auto slot = editor.create({{20, 0}, {30, 0}, SketchPoint{20, 2}});
    const auto firstCap = slot->find(SketchCreationRole::slotFirstCap);
    const auto secondCap = slot->find(SketchCreationRole::slotSecondCap);
    require(firstCap && secondCap &&
                near(std::get<document::SketchArc>(geometryOf(editor, *firstCap)).center, {20, 0}) &&
                near(std::get<document::SketchArc>(geometryOf(editor, *secondCap)).center, {30, 0}),
            "Slot roles name the caps around each center");
}

// Snaps become the same references as the explicit editor calls.
void snapsMatchExplicitReferences(application::CadSession& session) {
    SketchEditorSession viaCreate(session, document::DatumPlane::xy);
    SketchEditorSession viaSteps(session, document::DatumPlane::xy);
    for (auto* editor : {&viaCreate, &viaSteps}) {
        editor->setTool(SketchDrawingTool::rectangle);
        static_cast<void>(editor->create({{0, 0}, {10, 6}}));
        editor->setTool(SketchDrawingTool::circle);
    }
    const auto corner = viaCreate.elements()[2].id; // Only to aim the snap; not a creation ID.
    SketchCreationInput input{{10, 6}, {13, 6}};
    input.firstSnap = SketchSnap{SketchSnapKind::start, corner};
    const auto circle = viaCreate.create(input);
    require(viaSteps.setPreview({10, 6}, {13, 6}), "circle preview");
    const auto stepCircle = viaSteps.confirmPreview();
    require(viaSteps.addCoincidentReferences({*stepCircle, SketchPointPosition::center},
                                             {corner, SketchPointPosition::start}),
            "explicit coincidence");
    require(circle && same(viaCreate, viaSteps),
            "A snapped circle center is the same coincidence as the explicit call");

    // Centered rectangle: rectangle plus two construction lines, centered on the snap.
    SketchEditorSession centered(session, document::DatumPlane::xy);
    centered.setTool(SketchDrawingTool::rectangle);
    SketchCreationInput around{{0, 0}, {4, 3}};
    around.centered = true;
    around.firstSnap = SketchSnap{SketchSnapKind::origin};
    const auto result = centered.create(around);
    const auto half = result->find(SketchCreationRole::centerHalfDiagonal);
    require(result->elements.size() == 6 && half &&
                result->find(SketchCreationRole::centerDiagonal) &&
                near(std::get<document::SketchLine>(geometryOf(centered, *half)).start, {0, 0}) &&
                centered.tool() == SketchDrawingTool::rectangle,
            "A centered rectangle names its construction lines");
}
} // namespace

int main() {
    try {
        application::CadSession session;
        session.newDocument();
        plainStepsMatch(session);
        rolesNameGeometry(session);
        snapsMatchExplicitReferences(session);
        std::cout << "PASS U08 one sketch creation path with roles\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
