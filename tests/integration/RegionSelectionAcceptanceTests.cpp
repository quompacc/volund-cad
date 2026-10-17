#include "../BoundSketchFixture.hpp"
#include "CadSessionTestSupport.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// A-REG-02 / R09c through the application: a selected ring keeps its identity across
// pure dimension changes, reordering and irrelevant construction; split and deleted
// boundaries require repair and never select another region.
namespace {
using namespace voelund;
using application::CommandStatus;
using features::SketchRegionReferenceStatus;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
document::FeatureId committed(const application::CommandResult& result) {
    require(result.status() == CommandStatus::committed && result.feature().has_value(),
            ("Command was not committed: " + result.diagnostic()).c_str());
    return *result.feature();
}
// Deliberately broken edits are kept editable and reported with errors.
void keptWithErrors(const application::CommandResult& result) {
    require(result.status() == CommandStatus::committedWithErrors,
            ("Edit was not kept with errors: " + result.diagnostic()).c_str());
}
void edited(const application::CommandResult& result) {
    require(result.status() == CommandStatus::committed,
            ("Edit was not committed: " + result.diagnostic()).c_str());
}
const char* const ring = "o:1.0,1.1,1.2,1.3;h:2.0+";

struct Fixture {
    application::CadSession session;
    document::FeatureId sketch{0};
    double box{0};
};

document::SketchParameters parameters(double radius) {
    const document::SketchPlane plane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    return {plane,
            document::SketchElements{
                {{document::SketchElementId{1}, document::SketchRectangle{2.0, 3.0, 8.0, 9.0}},
                 {document::SketchElementId{2}, document::SketchCircle{{5.0, 6.0}, radius}}},
                3}};
}

void setUp(Fixture& f) {
    f.session.newDocument();
    const auto box = committed(f.session.createBoxFeature());
    f.box = test::volumeOf(f.session.currentShape());
    f.sketch = committed(f.session.createSketchFeature(box, parameters(1.0)));
    test::bindTop(f.session, f.sketch);
}

std::string selection(const application::CadSession& session) {
    std::string text;
    for (const auto& region : session.selectedSketchProfiles())
        text += region.text() + "|";
    return text;
}

SketchRegionReferenceStatus status(const application::CadSession& session,
                                   document::FeatureId extrusion) {
    const auto state = session.inspectExtrusionSelection(extrusion);
    require(state.legacyProfiles.empty() && state.regions.size() == 1 &&
                state.regions.front().region.text() == ring,
            "Stored region selection changed");
    return state.regions.front().status;
}

void ringSelection() {
    Fixture f;
    setUp(f);
    require(f.session.selectFeature(f.sketch), "Select sketch");
    require(f.session.selectSketchProfileByElement(document::SketchElementId{1}, false) &&
                selection(f.session) == std::string(ring) + "|",
            "Rectangle pick selects the ring");
    require(f.session.selectSketchProfileByElement(document::SketchElementId{2}, false) &&
                selection(f.session) == "o:2.0+|",
            "Circle pick selects the inner disc");
    require(f.session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "Reselect ring");
    const std::vector<document::SketchRegionId> selected(f.session.selectedSketchProfiles().begin(),
                                                         f.session.selectedSketchProfiles().end());
    const auto extrusion =
        committed(f.session.createSketchProfileExtrusion(f.sketch, selected, 10.0));
    const auto volume = [&] { return test::volumeOf(f.session.currentShape()); };
    require(std::abs(volume() - (f.box + (36.0 - std::numbers::pi) * 10.0)) < 1e-6,
            "Ring extrusion keeps its hole");

    edited(f.session.editSketchParameters(f.sketch, [&] {
        auto p = f.session.identifiedSnapshot().snapshot().sketchParameters(f.sketch);
        std::get<document::SketchCircle>(p.elements.values[1].geometry).radius = 1.5;
        return p;
    }()));
    require(status(f.session, extrusion) == SketchRegionReferenceStatus::resolved &&
                std::abs(volume() - (f.box + (36.0 - 2.25 * std::numbers::pi) * 10.0)) < 1e-6,
            "Pure radius change keeps the ring");

    edited(f.session.editSketchParameters(f.sketch, [&] {
        auto p = f.session.identifiedSnapshot().snapshot().sketchParameters(f.sketch);
        std::ranges::reverse(p.elements.values);
        p.elements.values.push_back(
            {document::SketchElementId{3}, document::SketchLine{{2.0, 3.0}, {8.0, 9.0}}, true});
        p.elements.nextId = 4;
        return p;
    }()));
    require(status(f.session, extrusion) == SketchRegionReferenceStatus::resolved &&
                std::abs(volume() - (f.box + (36.0 - 2.25 * std::numbers::pi) * 10.0)) < 1e-6,
            "Reordering and irrelevant construction keep the ring");

    const auto path = std::filesystem::temp_directory_path() / "voelund-region-selection.volund";
    f.session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    std::filesystem::remove(path);
    require(status(reopened, extrusion) == SketchRegionReferenceStatus::resolved &&
                std::abs(test::volumeOf(reopened.currentShape()) - volume()) < 1e-6,
            "Region selection lost on save/open");
    std::cout << "ring pick, radius change, reordering, construction and save/open passed\n";

    // A line across the rectangle splits the ring's cell: repair, never a guess.
    keptWithErrors(f.session.editSketchParameters(
        f.sketch,
        [&] {
            auto p = f.session.identifiedSnapshot().snapshot().sketchParameters(f.sketch);
            p.elements.values.push_back(
                {document::SketchElementId{4}, document::SketchLine{{2.0, 4.0}, {8.0, 4.0}}});
            p.elements.nextId = 5;
            return p;
        }(),
        application::ErrorDisposition::keepEditableErrors));
    require(status(f.session, extrusion) == SketchRegionReferenceStatus::topologyChanged &&
                f.session.inspectExtrusionSelection(extrusion).needsRepair(),
            "Split must require repair");
    edited(f.session.undo());
    require(status(f.session, extrusion) == SketchRegionReferenceStatus::resolved,
            "Undo restores the resolved ring");

    // Replace the rectangle (smallest boundary ID) by four lines of the same square.
    keptWithErrors(f.session.editSketchParameters(
        f.sketch,
        [&] {
            auto p = f.session.identifiedSnapshot().snapshot().sketchParameters(f.sketch);
            std::erase_if(p.elements.values,
                          [](const auto& e) { return e.id == document::SketchElementId{1}; });
            const double x[]{2.0, 8.0, 8.0, 2.0}, y[]{3.0, 3.0, 9.0, 9.0};
            for (std::uint64_t i = 0; i < 4; ++i)
                p.elements.values.push_back(
                    {document::SketchElementId{5 + i},
                     document::SketchLine{{x[i], y[i]}, {x[(i + 1) % 4], y[(i + 1) % 4]}}});
            p.elements.nextId = 9;
            return p;
        }(),
        application::ErrorDisposition::keepEditableErrors));
    require(status(f.session, extrusion) == SketchRegionReferenceStatus::elementsRemoved,
            "Deleted smallest boundary ID must not select another region");
    std::cout << "split and deleted boundary require repair instead of redirecting passed\n";
}
} // namespace

int main() {
    try {
        ringSelection();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
