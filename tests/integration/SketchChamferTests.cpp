#include "CadSessionTestSupport.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// S19 (user decision 2026-10-14): a sketch chamfer. The lower right corner of a 20 x 10
// rectangle chamfered at 3: a new line of length 3 * sqrt(2), still one closed region that
// extrudes with the corner cut off; the corner's relations stay at a virtual corner and both
// chamfer ends keep 3 from it; a distance too large is refused with the limit and changes
// nothing; undo; save and open.
namespace {
using namespace voelund;
using application::CommandStatus;
using document::SketchConstraintKind;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

void chamfer() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(application::SketchDrawingTool::rectangle);
    require(editor.setPreview({0, 0}, {20, 10}) && editor.confirmPreview().has_value(),
            "Rectangle");
    editor.commitHistory();
    std::optional<document::SketchElementId> bottom, right;
    for (const auto& element : editor.elements()) {
        const auto& line = std::get<document::SketchLine>(element.geometry);
        if (near(line.start.y, 0) && near(line.end.y, 0))
            bottom = element.id;
        if (near(line.start.x, 20) && near(line.end.x, 20))
            right = element.id;
    }
    require(bottom && right, "Bottom and right side");
    const auto select = [&] {
        require(editor.selectNearestReference(*bottom, {10, 0}, false) &&
                    editor.selectNearestReference(*right, {20, 5}, true),
                "Two sides selected");
    };

    select();
    const auto before = editor.draftParameters();
    try {
        static_cast<void>(editor.chamferCorner(15));
        throw std::runtime_error("A distance of 15 is refused");
    } catch (const application::SketchEditRefusal& refusal) {
        const std::string reason = refusal.what();
        require(reason.starts_with("Fasen: ") && reason.find("zu groß") != std::string::npos &&
                    reason.find("höchstens 9.99") != std::string::npos &&
                    editor.draftParameters() == before,
                "Too large names the limit (just under 10) and changes nothing, got " + reason);
    }

    select();
    const auto created = editor.chamferCorner(3).created;
    editor.commitHistory();
    require(created.has_value() &&
                editor.lastSolveResult().status == features::SketchSolveStatus::success,
            "Chamfered and solved");
    const document::SketchLine* cut = nullptr;
    bool construction = true;
    for (const auto& element : editor.elements())
        if (element.id == *created) {
            cut = std::get_if<document::SketchLine>(&element.geometry);
            construction = element.construction;
        }
    require(cut != nullptr && !construction &&
                near(std::hypot(cut->end.x - cut->start.x, cut->end.y - cut->start.y),
                     3 * std::sqrt(2.0)),
            "A chamfer line of length 3 * sqrt(2)");
    int distances = 0;
    for (const auto& constraint : editor.constraints())
        distances += constraint.kind == SketchConstraintKind::pointDistance &&
                     near(constraint.value, 3);
    require(distances == 2, "Both chamfer ends keep 3 from the virtual corner");
    require(features::identifySketchRegions(editor.elements()).ids.size() == 1,
            "Still one closed region");

    require(editor.undo() && editor.elements().size() == 4 && editor.redo(),
            "Undo and redo the chamfer");
    require(editor.tryAccept().status() == CommandStatus::committed, "Accepted");
    const auto sketch = session.features().back().id;
    const auto regions =
        features::identifySketchRegions(session.sketchParameters(sketch).elements.values).ids;
    require(session.createSketchProfileExtrusion(sketch, regions, 5).status() ==
                    CommandStatus::committed &&
                near(test::volumeOf(session.validBodies().front().shape), (200 - 4.5) * 5),
            "The extrusion has the corner cut off");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-s19-chamfer";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "chamfer.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.sketchParameters(sketch) == session.sketchParameters(sketch) &&
                near(test::volumeOf(reopened.validBodies().front().shape), (200 - 4.5) * 5),
            "Open restores the chamfered sketch");
    std::filesystem::remove_all(directory);
}

} // namespace

int main() {
    try {
        chamfer();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
