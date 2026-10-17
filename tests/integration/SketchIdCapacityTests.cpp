#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <sstream>
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/IdCapacity.hpp>

namespace {
using namespace voelund;
using Editor = application::SketchEditorSession;
using Kind = document::SketchConstraintKind;
constexpr auto limit = std::numeric_limits<std::uint64_t>::max();
constexpr document::SketchGeometryReference start{document::SketchElementId{1},
                                                  document::SketchPointPosition::start};
constexpr document::SketchGeometryReference other{document::SketchElementId{2},
                                                  document::SketchPointPosition::start};
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
document::SketchParameters fixture(std::uint64_t elements, std::uint64_t constraints) {
    return {{0, 0, 20, 0, 0, 1, 1, 0, 0},
            {{{document::SketchElementId{1}, document::SketchLine{{0, 0}, {10, 0}}},
              {document::SketchElementId{2}, document::SketchLine{{0, 0}, {0, 10}}},
              {document::SketchElementId{3}, document::SketchCircle{{5, 5}, 2}}},
             elements},
            {{}, constraints}};
}
Editor editor(application::CadSession& session, std::uint64_t elements, std::uint64_t constraints) {
    return Editor(session, document::FeatureId{3}, document::FeatureId{1},
                  fixture(elements, constraints));
}
template <class Action> void rejected(Editor& ed, Action action) {
    const auto before = ed.draftParameters();
    const auto selection = ed.selectedElement();
    const std::vector<document::SketchGeometryReference> refs(ed.selectedReferences().begin(),
                                                              ed.selectedReferences().end());
    const auto solve = ed.lastSolveResult();
    const auto undo = ed.canUndo(), redo = ed.canRedo();
    require(!action(), "ID exhaustion was accepted");
    ed.commitHistory();
    require(ed.draftParameters() == before && ed.selectedElement() == selection &&
                std::ranges::equal(ed.selectedReferences(), refs) && ed.canUndo() == undo &&
                ed.canRedo() == redo && ed.lastSolveResult().status == solve.status &&
                ed.lastSolveResult().degreesOfFreedom == solve.degreesOfFreedom &&
                ed.lastSolveResult().conflictingConstraints == solve.conflictingConstraints,
            "ID exhaustion changed draft, selection, solve status or history");
}
void selectPair(Editor& ed) {
    require(ed.selectNearestReference(document::SketchElementId{1}, {5, 0}, false) &&
                ed.selectNearestReference(document::SketchElementId{2}, {0, 5}, true),
            "Cannot select fixture pair");
}
void drawing() {
    using Tool = application::SketchDrawingTool;
    struct Case {
        Tool tool;
        std::uint64_t elements;
        std::uint64_t constraints;
    };
    for (const auto test : std::array{Case{Tool::line, 1, 0}, Case{Tool::rectangle, 4, 8},
                                      Case{Tool::slot, 5, 11}, Case{Tool::polygon, 7, 17}}) {
        application::CadSession session;
        auto shortElements = editor(session, limit - test.elements, 10);
        shortElements.setTool(test.tool);
        rejected(shortElements, [&] { return shortElements.setPreview({1, 1}, {6, 4}, {{6, 6}}); });
        rejected(shortElements, [&] { return shortElements.confirmPreview().has_value(); });
        if (test.constraints) {
            auto shortConstraints = editor(session, 10, limit - test.constraints);
            shortConstraints.setTool(test.tool);
            rejected(shortConstraints,
                     [&] { return shortConstraints.setPreview({1, 1}, {6, 4}, {{6, 6}}); });
        }
        auto exact = editor(session, limit - 1 - test.elements, limit - 1 - test.constraints);
        exact.setTool(test.tool);
        require(exact.setPreview({1, 1}, {6, 4}, {{6, 6}}) && exact.confirmPreview().has_value(),
                "Exact remaining ID capacity was rejected");
        const auto result = exact.draftParameters();
        require(result.elements.nextId == limit - 1 && result.constraints.nextId == limit - 1,
                "Drawing did not consume its declared ID budget");
        document::validateSketchParameters(result);
        exact.commitHistory();
        require(exact.undo() && exact.redo() && exact.draftParameters() == result,
                "High IDs changed across editor history");
    }
    application::CadSession session;
    auto ed = editor(session, 10, limit - 9);
    ed.setTool(Tool::rectangle);
    require(ed.setPreview({1, 1}, {4, 4}) && ed.addFixedReference(start, {0, 0}),
            "Cannot prepare a preview whose remaining budget changes");
    ed.commitHistory();
    rejected(ed, [&] { return ed.confirmPreview().has_value(); });
}
void operations() {
    application::CadSession session;
    for (bool constraintsExhausted : {false, true}) {
        auto ed = editor(session, constraintsExhausted ? 10 : limit - 1,
                         constraintsExhausted ? limit - 1 : 10);
        require(ed.selectNearestReference(document::SketchElementId{1}, {5, 0}, false), "Select");
        rejected(ed, [&] { return ed.splitSelected(); });
        selectPair(ed);
        rejected(ed, [&] { return ed.filletSelected(1); });
        if (!constraintsExhausted) {
            rejected(ed, [&] { return ed.mirrorSelected(); });
            rejected(ed, [&] { return ed.linearPatternSelected(4, 1); });
            rejected(ed, [&] { return ed.circularPatternSelected(4, 45); });
            rejected(ed, [&] { return ed.offsetSelected(1); });
        }
    }
    auto shortPattern = editor(session, limit - 4, 10); // two sources, two copies need four
    selectPair(shortPattern);
    rejected(shortPattern, [&] { return shortPattern.linearPatternSelected(3, 1); });
    rejected(shortPattern, [&] { return shortPattern.circularPatternSelected(3, 45); });
    require(!document::hasRepeatedIdCapacity(1, limit, limit), "ID product overflow accepted");
    auto exactPattern = editor(session, limit - 5, 10);
    selectPair(exactPattern);
    require(exactPattern.linearPatternSelected(3, 1) &&
                exactPattern.draftParameters().elements.nextId == limit - 1,
            "Exact pattern capacity rejected");
}
void relations() {
    application::CadSession session;
    auto ed = editor(session, 10, limit - 1);
    selectPair(ed);
    rejected(ed, [&] { return ed.addSelectedRelation(Kind::perpendicular); });
    rejected(ed, [&] { return ed.addCoincidentReferences(start, other); });
    rejected(ed, [&] { return ed.addMidpointReference(start, document::SketchElementId{2}); });
    rejected(ed, [&] { return ed.addPointOnObjectReference(start, document::SketchElementId{2}); });
    rejected(ed,
             [&] { return ed.addAutomaticLineRelation(document::SketchElementId{1}).has_value(); });
    rejected(ed, [&] {
        return ed.addTangencyReference(document::SketchElementId{3}, document::SketchElementId{1});
    });
    require(ed.selectElement(document::SketchElementId{1}), "Select line");
    rejected(ed, [&] { return ed.setSelectedLineLength(12); });
    rejected(ed, [&] {
        return ed.setSelectedLineConstraint(document::SketchLineConstraint::horizontal);
    });
    require(ed.selectElement(document::SketchElementId{3}), "Select circle");
    rejected(ed, [&] { return ed.setSelectedCircleRadius(3); });
    for (auto remaining : {0ULL, 1ULL}) {
        auto pair = editor(session, 10, limit - 1 - remaining);
        require(pair.selectNearestReference(document::SketchElementId{1}, {0, 0}, false),
                "Select point");
        rejected(pair, [&] { return pair.lockSelectedReference(); });
        rejected(pair, [&] {
            return pair.addAxisReference(start, application::SketchAxisReference::origin);
        });
        rejected(pair, [&] { return pair.addFixedReference(start, {0, 0}); });
        rejected(pair, [&] { return pair.addSupportReference(start, {0, 0}, 1, 0.5); });
    }
    auto exact = editor(session, 10, limit - 3);
    require(exact.addAxisReference(start, application::SketchAxisReference::origin) &&
                exact.addAxisReference(start, application::SketchAxisReference::origin),
            "Exact or duplicate axis reference rejected");
    auto dimension = editor(session, 10, limit - 2);
    require(dimension.selectElement(document::SketchElementId{1}) &&
                dimension.setSelectedLineLength(11) && dimension.setSelectedLineLength(12),
            "Updating a dimension unnecessarily required a new ID");
}
std::string bytes(const document::ModelDocument& doc) {
    std::ostringstream out;
    document::DocumentSerializer::write(doc, out);
    return out.str();
}
void documentAndImport() {
    document::ModelDocument doc;
    const auto id =
        doc.addSketchFeature("IDs", document::FeatureId{1}, fixture(limit - 2, limit - 1));
    static_cast<void>(doc.addSketchElement(id, document::SketchLine{{1, 1}, {2, 1}}));
    const auto original = bytes(doc);
    const auto revision = doc.revision();
    bool rejectedId = false;
    try {
        static_cast<void>(doc.addSketchElement(id, document::SketchCircle{{0, 0}, 1}));
    } catch (const std::overflow_error&) {
        rejectedId = true;
    }
    require(rejectedId && bytes(doc) == original && doc.revision() == revision,
            "Document ID exhaustion mutated state");
    std::istringstream input(original);
    require(bytes(document::DocumentSerializer::read(input)) == original,
            "High ID roundtrip failed");
    for (bool element : {false, true}) {
        auto invalid = doc.sketchParameters(id);
        (element ? invalid.elements.nextId : invalid.constraints.nextId) = limit;
        rejectedId = false;
        try {
            static_cast<void>(doc.setSketchParameters(id, invalid));
        } catch (const std::invalid_argument&) {
            rejectedId = true;
        }
        require(rejectedId && bytes(doc) == original, "Document accepted unserializable next ID");
    }
    std::istringstream featureInput("VOLUND 1\nNEXT_ID 18446744073709551614\nFEATURES 0\n");
    auto exhausted = document::DocumentSerializer::read(featureInput);
    const auto before = bytes(exhausted);
    rejectedId = false;
    try {
        static_cast<void>(exhausted.addBoxFeature("Overflow", {1, 1, 1}));
    } catch (const std::overflow_error&) {
        rejectedId = true;
    }
    require(rejectedId && bytes(exhausted) == before && !exhausted.canUndo(),
            "Feature allocation made its next counter unreadable");
}
} // namespace
int main() {
    try {
        drawing();
        operations();
        relations();
        documentAndImport();
        std::cout
            << "PASS ID capacity: drawing, operations, relations, atomic rejection and roundtrip\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
