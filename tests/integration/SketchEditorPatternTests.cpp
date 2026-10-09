#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// S12b: associative patterns in the editor. Copies follow every change of their source (and of
// a mirror axis), are read-only, change with their pattern (count, spacing), stay apart from
// relations, become independent when the pattern is dissolved; refusals name the reason;
// accept, reopen and the evaluation keep the pattern; a legacy rectangle is copied once.
namespace {
using namespace voelund;
using application::SketchEditRefusal;
using application::SketchEditorSession;
using document::SketchArc;
using document::SketchElementId;
using document::SketchLine;
using document::SketchPatternKind;
using document::SketchPoint;
using Position = document::SketchPointPosition;
constexpr double pi = std::numbers::pi;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }
bool near(SketchPoint a, SketchPoint b) { return near(a.x, b.x) && near(a.y, b.y); }
SketchElementId id(std::uint64_t value) { return SketchElementId{value}; }

struct Fixture final {
    application::CadSession session;
    std::unique_ptr<SketchEditorSession> editor;
    explicit Fixture(const std::vector<document::SketchGeometry>& geometry) {
        session.newDocument();
        editor = std::make_unique<SketchEditorSession>(session, document::DatumPlane::xy);
        document::SketchElements elements;
        for (const auto& item : geometry)
            elements.values.push_back({SketchElementId{elements.nextId++}, item});
        require(editor->loadDraft(std::move(elements), {}), "Draft loads");
    }
    template <typename Geometry> [[nodiscard]] Geometry get(std::uint64_t element) const {
        for (const auto& item : editor->elements())
            if (item.id == id(element))
                return std::get<Geometry>(item.geometry);
        throw std::runtime_error("No element " + std::to_string(element));
    }
    void select(std::uint64_t first, std::optional<std::uint64_t> second = std::nullopt) {
        require(editor->selectNearestReference(id(first), {0, 0}, false), "Select");
        if (second)
            require(editor->selectNearestReference(id(*second), {0, 0}, true), "Add");
    }
    std::uint64_t create(SketchPatternKind kind, int count, double spacing, double angle) {
        const auto pattern = editor->createPattern(kind, count, spacing, angle);
        require(pattern.has_value(), "A pattern, not a copy");
        editor->commitHistory();
        return *pattern;
    }
    void refused(const std::function<void()>& action, const std::string& reason) {
        const auto before = editor->draftParameters();
        try {
            action();
        } catch (const std::invalid_argument& error) {
            require(std::string(error.what()).find(reason) != std::string::npos,
                    "Expected \"" + reason + "\", got \"" + error.what() + "\"");
            require(editor->draftParameters() == before, "No change for " + reason);
            return;
        }
        throw std::runtime_error("Not refused: " + reason);
    }
};

void linear() {
    Fixture f({SketchLine{{0, 0}, {4, 0}}});
    f.select(1);
    const auto pattern = f.create(SketchPatternKind::linear, 3, 10, 0);
    require(f.editor->elements().size() == 3 && f.editor->patterns().size() == 1,
            "Two copies and the pattern");
    require(near(f.get<SketchLine>(3).start, {20, 0}), "Third position at x = 20");

    // The source changes, the copies follow; a copy itself is read-only.
    f.select(1);
    require(f.editor->setSelectedLineLength(6), "Source length 6");
    require(near(f.get<SketchLine>(2).end, {16, 0}) && near(f.get<SketchLine>(3).end, {26, 0}),
            "Copies follow the length");
    require(f.editor->translateSelected(0, 2), "Move the source");
    const auto source = f.get<SketchLine>(1);
    require(source.start.y > 1 &&
                near(f.get<SketchLine>(3).start, {source.start.x + 20, source.start.y}) &&
                near(f.get<SketchLine>(3).end, {source.end.x + 20, source.end.y}),
            "Copies follow the move");
    f.select(2);
    require(f.editor->patternLock(false) && !f.editor->translateSelected(1, 0) &&
                !f.editor->removeSelected() && !f.editor->toggleSelectedConstruction() &&
                !f.editor->setSelectedLineLength(9),
            "A copy is not moved, deleted, switched or dimensioned");
    f.refused([&] { (void)f.editor->editCurve(application::SketchCurveEdit::split); },
              "Kopie folgt ihrem Original");
    f.select(1);
    require(f.editor->patternLock(true) && !f.editor->removeSelected(),
            "The source of a pattern is not deleted");

    // Count and spacing change the pattern, each as one step.
    f.editor->commitHistory();
    const auto before = f.editor->draftParameters();
    require(f.editor->editPattern(pattern, 5, 7, 0), "Five positions, 7 apart");
    f.editor->commitHistory();
    require(f.editor->elements().size() == 5 &&
                near(f.get<SketchLine>(5).start,
                     {f.get<SketchLine>(1).start.x + 28, f.get<SketchLine>(1).start.y}),
            "Positions added at the end");
    require(f.editor->editPattern(pattern, 2, 7, 0), "Back to two positions");
    f.editor->commitHistory();
    require(f.editor->elements().size() == 2, "Positions removed from the end");
    require(f.editor->undo() && f.editor->undo() && f.editor->draftParameters() == before,
            "Each change is one undo step");

    // Refusals.
    f.select(2);
    f.refused([&] { (void)f.editor->createPattern(SketchPatternKind::linear, 3, 5, 0); },
              "Eine Kopie kann kein Original");
    f.select(1);
    f.refused([&] { (void)f.editor->createPattern(SketchPatternKind::linear, 1, 5, 0); },
              "zwischen 2 und 1000");
    f.refused([&] { (void)f.editor->createPattern(SketchPatternKind::linear, 1001, 5, 0); },
              "zwischen 2 und 1000");
    f.refused([&] { (void)f.editor->createPattern(SketchPatternKind::linear, 3, 0, 0); },
              "Abstand darf nicht 0");

    // Dissolved, the copies are independent.
    require(f.editor->dissolvePattern(pattern) && f.editor->patterns().empty(), "Dissolved");
    f.select(2);
    require(f.editor->translateSelected(0, 1), "A former copy moves on its own");
    const auto former = f.get<SketchLine>(2);
    f.select(1);
    require(f.editor->setSelectedLineLength(3), "Source length 3");
    require(f.get<SketchLine>(2) == former, "A former copy no longer follows");
}

void circularAndMirror() {
    Fixture f({SketchLine{{2, 0}, {6, 0}}, SketchArc{{10, 0}, 2, 0, pi / 2},
               SketchLine{{20, -10}, {20, 10}}});
    f.select(1);
    f.create(SketchPatternKind::circular, 4, 0, 360);
    require(near(f.get<SketchLine>(4).end, {0, 6}) && near(f.get<SketchLine>(6).end, {0, -6}),
            "Quarter turns about the origin");
    f.select(2, 3);
    f.create(SketchPatternKind::mirror, 2, 0, 0);
    const auto mirrored = f.get<SketchArc>(7);
    require(near(mirrored.center, {30, 0}), "Arc mirrored at x = 20");
    require(f.editor->selectElement(id(3)) && f.editor->translateSelected(5, 0),
            "Move the axis to x = 25");
    require(near(f.get<SketchArc>(7).center, {40, 0}), "The mirror follows its axis");
}

void persisted() {
    Fixture f({SketchLine{{0, 0}, {4, 0}}});
    f.select(1);
    f.create(SketchPatternKind::linear, 2, 10, 0);
    require(f.editor->tryAccept().status() == application::CommandStatus::committed, "Accept");
    const auto sketch = f.session.features().back().id;
    auto stored = f.session.sketchParameters(sketch);
    require(stored.patterns.values.size() == 1, "The pattern is stored");
    // A stored source change with a stale copy: the evaluation derives the copy again.
    std::get<SketchLine>(stored.elements.values[0].geometry).end = {8, 0};
    require(f.session.editSketchParameters(sketch, stored).status() ==
                application::CommandStatus::committed,
            "Edit the stored source");
    require(f.session.selectFeature(sketch), "Select the sketch");
    bool followed = false;
    for (const auto& element : f.session.selectedSketchElements())
        if (element.id == id(2))
            for (const auto& point : element.points)
                followed = followed || near(point.local, {18, 0});
    require(followed, "The evaluated copy follows the stored source");
    SketchEditorSession reopened(f.session, sketch, f.session.sketchParameters(sketch));
    require(reopened.patterns().size() == 1, "Reopened with its pattern");

    Fixture legacy({document::SketchRectangle(0, 0, 4, 2)});
    legacy.select(1);
    require(!legacy.editor->createPattern(SketchPatternKind::linear, 2, 10, 0) &&
                legacy.editor->patterns().empty() && legacy.editor->elements().size() == 2,
            "A legacy rectangle is copied once, without a pattern");
}

} // namespace

int main() {
    try {
        linear();
        circularAndMirror();
        persisted();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
