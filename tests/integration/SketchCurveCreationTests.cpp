#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

// S05: curves in real creation (SketchEditorSession::create). A circle tangent to a line, an
// arc concentric, equal and tangent, an ellipse centre snapped to a circle or ellipse centre;
// relations the solver adapter does not support (tangent or equal to an ellipse) are not
// claimed; degenerate picks are refused with their reason and change nothing.
namespace {
using namespace voelund;
using application::SketchAutoRelation;
using application::SketchConstraintOrigin;
using application::SketchCreationInput;
using application::SketchCreationResult;
using application::SketchDrawingTool;
using application::SketchEditorSession;
using application::SketchSnap;
using application::SketchSnapKind;
using document::SketchElementId;
using document::SketchPoint;
using Kind = document::SketchConstraintKind;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

struct Editor final {
    application::CadSession session;
    std::unique_ptr<SketchEditorSession> editor;
    Editor() {
        session.newDocument();
        editor = std::make_unique<SketchEditorSession>(session, document::DatumPlane::xy);
    }
    SketchCreationResult create(SketchDrawingTool tool, SketchCreationInput input) {
        editor->setTool(tool);
        auto result = editor->create(input);
        require(result.has_value(), "Created");
        return *result;
    }
    const document::SketchGeometry& geometry(SketchElementId id) const {
        const auto found = std::ranges::find(editor->elements(), id, &document::SketchElement::id);
        require(found != editor->elements().end(), "Element");
        return found->geometry;
    }
};

// The kinds of the relation constraints the step reports.
std::vector<Kind> relations(const Editor& e, const SketchCreationResult& result) {
    std::vector<Kind> kinds;
    for (const auto& created : result.constraints)
        if (created.origin == SketchConstraintOrigin::relation)
            kinds.push_back(std::ranges::find(e.editor->constraints(), created.id,
                                              &document::SketchConstraint::id)
                                ->kind);
    return kinds;
}
bool solved(const Editor& e) {
    return e.editor->lastSolveResult().status == features::SketchSolveStatus::success &&
           e.editor->lastSolveResult().conflictingConstraints.empty();
}

void circleTangentToLine() {
    Editor e;
    const auto line = e.create(SketchDrawingTool::line, {{-20, 10}, {20, 10}}).primary();
    SketchCreationInput input{{0, 0}, {0, 9.5}};
    input.relation = SketchAutoRelation::tangent;
    input.secondSnap = SketchSnap{SketchSnapKind::onObject, line};
    const auto circle = e.create(SketchDrawingTool::circle, input);
    require(relations(e, circle) == std::vector{Kind::tangent} && solved(e),
            "The circle is tangent to the line");
    const auto& shape = std::get<document::SketchCircle>(e.geometry(circle.primary()));
    const auto& edge = std::get<document::SketchLine>(e.geometry(line));
    require(near(std::abs(edge.start.y - shape.center.y), shape.radius),
            "The line touches the circle");
    std::cout << "PASS circle tangent to a line\n";
}

void arcRelations() {
    Editor e;
    const auto circle = e.create(SketchDrawingTool::circle, {{0, 0}, {5, 0}}).primary();
    SketchCreationInput concentric{{0.2, 0.1}, {8, 0}, SketchPoint{0, 8}};
    concentric.relation = SketchAutoRelation::concentric;
    concentric.relationTarget = circle;
    const auto around = e.create(SketchDrawingTool::arc, concentric);
    require(relations(e, around) == std::vector{Kind::concentric} && solved(e),
            "The arc is concentric with the circle");
    const auto& ring = std::get<document::SketchArc>(e.geometry(around.primary()));
    const auto& disc = std::get<document::SketchCircle>(e.geometry(circle));
    require(near(ring.center.x, disc.center.x) && near(ring.center.y, disc.center.y),
            "The arc shares the circle's centre");

    SketchCreationInput equal{{20, 0}, {26, 0}, SketchPoint{20, 6}};
    equal.relation = SketchAutoRelation::equal;
    equal.relationTarget = circle;
    const auto same = e.create(SketchDrawingTool::arc, equal);
    require(relations(e, same) == std::vector{Kind::equal} && solved(e) &&
                near(std::get<document::SketchArc>(e.geometry(same.primary())).radius,
                     std::get<document::SketchCircle>(e.geometry(circle)).radius),
            "The arc and the circle have equal radii");

    SketchCreationInput tangent{{0, 20}, {4, 20}, SketchPoint{0, 24}};
    tangent.relation = SketchAutoRelation::tangent;
    tangent.relationTarget = circle;
    const auto touching = e.create(SketchDrawingTool::arc, tangent);
    require(relations(e, touching) == std::vector{Kind::tangent} && solved(e),
            "The arc is tangent to the circle");

    // An arc ending on a line: the end binds to the line, tangency is not claimed there.
    const auto line = e.create(SketchDrawingTool::line, {{-30, 60}, {30, 60}}).primary();
    SketchCreationInput onLine{{0, 50}, {10, 50}, SketchPoint{0, 60}};
    onLine.relation = SketchAutoRelation::tangent;
    onLine.thirdSnap = SketchSnap{SketchSnapKind::onObject, line};
    const auto ending = e.create(SketchDrawingTool::arc, onLine);
    require(relations(e, ending).empty() && solved(e),
            "No tangency is claimed at an arc end on a line");
    std::cout << "PASS arc concentric, equal and tangent\n";
}

void ellipseCentre() {
    Editor e;
    const auto circle = e.create(SketchDrawingTool::circle, {{30, 30}, {33, 30}}).primary();
    SketchCreationInput onCircle{{30, 30}, {40, 30}, SketchPoint{30, 34}};
    onCircle.firstSnap = SketchSnap{SketchSnapKind::center, circle};
    const auto first = e.create(SketchDrawingTool::ellipse, onCircle);
    require(std::ranges::any_of(first.constraints,
                                [](const auto& c) { return c.origin == SketchConstraintOrigin::snap; }) &&
                solved(e),
            "The ellipse centre binds to the circle centre");
    SketchCreationInput onEllipse{{30, 30}, {36, 33}, SketchPoint{31, 32}};
    onEllipse.firstSnap = SketchSnap{SketchSnapKind::center, first.primary()};
    const auto second = e.create(SketchDrawingTool::ellipse, onEllipse);
    require(std::ranges::any_of(second.constraints,
                                [](const auto& c) { return c.origin == SketchConstraintOrigin::snap; }) &&
                solved(e),
            "An ellipse centre binds to another ellipse centre");
    const auto& moved = std::get<document::SketchEllipse>(e.geometry(second.primary()));
    require(near(moved.center.x, 30) && near(moved.center.y, 30), "Both centres coincide");
    std::cout << "PASS ellipse centre snaps\n";
}

// Tangent and equal to an ellipse are not supported by the solver adapter: nothing is claimed.
void unsupportedNotClaimed() {
    Editor e;
    const auto ellipse =
        e.create(SketchDrawingTool::ellipse, {{0, 0}, {10, 0}, SketchPoint{0, 4}}).primary();
    for (const auto relation : {SketchAutoRelation::tangent, SketchAutoRelation::equal}) {
        SketchCreationInput input{{30, 0}, {35, 0}};
        input.relation = relation;
        input.relationTarget = ellipse;
        const auto circle = e.create(SketchDrawingTool::circle, input);
        require(relations(e, circle).empty() && solved(e),
                "No relation to an ellipse is claimed");
    }
    SketchCreationInput onEllipse{{30, 20}, {35, 20}};
    onEllipse.relation = SketchAutoRelation::tangent;
    onEllipse.secondSnap = SketchSnap{SketchSnapKind::onObject, ellipse};
    // A circle's radius pick binds nothing; tangency to the ellipse is not claimed either.
    const auto touching = e.create(SketchDrawingTool::circle, onEllipse);
    require(relations(e, touching).empty() && solved(e),
            "No tangency to an ellipse through the radius pick");
    std::cout << "PASS no claimed tangency or equality with an ellipse\n";
}

void degenerate() {
    Editor e;
    e.create(SketchDrawingTool::line, {{-5, -5}, {5, -5}});
    struct Case {
        SketchDrawingTool tool;
        SketchCreationInput input;
        const char* reason;
    };
    SketchCreationInput centered{{3, 3}, {3, 9}};
    centered.centered = true;
    const std::vector<Case> cases{
        {SketchDrawingTool::arc, {{0, 0}, {5, 0}, SketchPoint{5, 0}}, "Bogen: Anfangs- und Endpunkt"},
        {SketchDrawingTool::arc, {{0, 0}, {0, 0}, SketchPoint{5, 0}}, "Bogen: Der Anfangspunkt"},
        {SketchDrawingTool::threePointArc, {{0, 0}, {5, 0}, SketchPoint{10, 0}},
         "Dreipunktbogen: Die drei Punkte"},
        {SketchDrawingTool::threePointArc, {{0, 0}, {0, 0}, SketchPoint{10, 0}},
         "Dreipunktbogen: Zwei Punkte"},
        {SketchDrawingTool::circle, {{1, 1}, {1, 1}}, "Kreis: Der Radius"},
        {SketchDrawingTool::line, {{2, 2}, {2, 2}}, "Linie: Anfangs- und Endpunkt"},
        {SketchDrawingTool::rectangle, centered, "Rechteck aus dem Mittelpunkt"},
    };
    for (const auto& item : cases) {
        const auto elements = std::vector(e.editor->elements().begin(), e.editor->elements().end());
        const auto constraints =
            std::vector(e.editor->constraints().begin(), e.editor->constraints().end());
        std::string message;
        try {
            e.editor->setTool(item.tool);
            static_cast<void>(e.editor->create(item.input));
        } catch (const std::invalid_argument& error) {
            message = error.what();
        }
        require(message.find(item.reason) != std::string::npos,
                std::string("Expected \"") + item.reason + "\", got \"" + message + "\"");
        require(std::ranges::equal(elements, e.editor->elements()) &&
                    std::ranges::equal(constraints, e.editor->constraints()),
                std::string("No change for ") + item.reason);
    }
    std::cout << "PASS degenerate picks are refused with their reason\n";
}
} // namespace

int main() {
    try {
        circleTangentToLine();
        arcRelations();
        ellipseCentre();
        unsupportedNotClaimed();
        degenerate();
        std::cout << "PASS S05 curve creation\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
