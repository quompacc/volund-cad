#include "CadSessionTestSupport.hpp"

#include <voelund/application/CadSession.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// M25 (user decision 2026-10-14): a draft ("Formschräge"). The four sides of a 40 x 30 x 20 box
// tilted 5 degrees about the bottom face give the analytic volume of the tapered box; 10 degrees
// follows; the top face (parallel to the neutral plane) cannot be tilted and is reported by its
// number; undo, save and open.
namespace {
using namespace voelund;
using application::CommandStatus;
constexpr double pi = std::numbers::pi;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-4 * std::max(1.0, std::abs(b)); }
double total(const application::CadSession& session) {
    double sum = 0;
    for (const auto& body : session.validBodies())
        sum += test::volumeOf(body.shape);
    return sum;
}
// Sides of a 40 x 30 box, each moved in by tan(angle) per height, integrated over 0..20.
double tapered(double degrees) {
    const double t = std::tan(degrees * pi / 180);
    return 1200 * 20 - 70 * t * 400 + 4.0 / 3.0 * t * t * 8000;
}

void draft() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 20}).feature();
    std::vector<document::SubshapeOrigin> sides;
    std::optional<document::SubshapeOrigin> bottom, top;
    for (const auto& face : session.supportFaces(box)) {
        if (face.plane.normalZ < -0.5)
            bottom = face.origin;
        else if (face.plane.normalZ > 0.5)
            top = face.origin;
        else
            sides.push_back(face.origin);
    }
    require(sides.size() == 4 && bottom && top, "Four sides, bottom and top");
    const auto created = session.createDraft(box, sides, *bottom, 5 * pi / 180);
    if (created.status() != CommandStatus::committed)
        for (const auto& issue : created.evaluation().features)
            std::cerr << issue.code << ' ' << issue.message << std::endl;
    require(created.status() == CommandStatus::committed &&
                session.features().back().name == "Formschräge 1",
            "Formschräge 1");
    const auto feature = session.features().back().id;
    require(near(total(session), tapered(5)),
            "The tapered box: " + std::to_string(total(session)) + " for " +
                std::to_string(tapered(5)));

    require(session.supportFaces(feature).size() == 6, "All six faces stay named planar faces");
    auto parameters = session.identifiedSnapshot().snapshot().draftParameters(feature);
    parameters.angle = kernel::Radians{10 * pi / 180};
    require(session.editDraftParameters(feature, parameters).status() == CommandStatus::committed &&
                near(total(session), tapered(10)),
            "10 degrees follow");
    require(session.undo().status() == CommandStatus::committed && near(total(session), tapered(5)),
            "Undo returns to 5 degrees");

    auto withTop = parameters;
    withTop.angle = kernel::Radians{5 * pi / 180};
    withTop.faces = {*top};
    const auto refused = session.editDraftParameters(feature, withTop);
    bool localized = false;
    for (const auto& issue : refused.evaluation().features)
        localized = localized ||
                    (issue.code == "draft-face-refused" && issue.elements == std::vector<std::uint64_t>{1});
    require(refused.status() == CommandStatus::rejected && localized,
            "The top face cannot be tilted and is named");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m25-draft";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto file = directory / "draft.volund";
    session.saveDocument(file);
    application::CadSession reopened;
    reopened.loadDocument(file);
    require(reopened.identifiedSnapshot().snapshot().draftParameters(feature) ==
                    session.identifiedSnapshot().snapshot().draftParameters(feature) &&
                near(total(reopened), tapered(5)),
            "Open restores the draft");
    std::filesystem::remove_all(directory);
}

} // namespace

int main() {
    try {
        draft();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
