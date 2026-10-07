#include "CadSessionTestSupport.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// T04a: push/pull. A planar body face is the profile of one extrusion: pulling joins,
// pushing cuts. The end face of a push/pull can be pulled again, the distance stays
// editable, the extrusion follows its face when the body changes, and undo and save/open
// keep it. A zero distance and a face the body does not offer are rejected without change.
namespace {
using namespace voelund;
using application::CommandStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
document::FeatureId committed(const application::CommandResult& result) {
    for (const auto& issue : result.evaluation().features)
        if (!issue.message.empty())
            std::cerr << "feature " << issue.feature.value() << ": " << issue.message
                      << std::endl;
    require(result.status() == CommandStatus::committed && result.feature().has_value(),
            "Command was not committed: " + result.diagnostic());
    return *result.feature();
}
double volume(const application::CadSession& session) {
    return test::volumeOf(session.currentShape());
}
// A face `support`'s current result offers, by its origin: a box side or a push/pull cap.
features::SupportOrigin face(const application::CadSession& session, document::FeatureId support,
                             const features::SupportOrigin& wanted) {
    const auto faces = session.supportFaces(support);
    require(std::ranges::find(faces, wanted, &features::SupportFaceFrame::origin) != faces.end(),
            "The body offers the face (feature " + std::to_string(support.value()) + ")");
    return wanted;
}
features::SupportOrigin side(document::FeatureId box, kernel::PrimitiveRole role) {
    return {box, role};
}
features::SupportOrigin cap(document::FeatureId pushPull) { return {pushPull, std::nullopt, {}, true}; }

void pushAndPull() {
    application::CadSession session;
    session.newDocument();
    const auto box = committed(session.createBoxFeature({10, 10, 10}));
    require(near(volume(session), 1000), "box");

    const auto pulled = committed(session.pushPullFace(box, face(session, box, side(box, kernel::PrimitiveRole::boxPositiveZ)), 5));
    require(near(volume(session), 1500) && session.validBodies().size() == 1,
            "Pulling the top face joins 10 x 10 x 5");
    const auto end = face(session, pulled, cap(pulled)); // its own stable origin
    const auto again = committed(session.pushPullFace(pulled, end, 3));
    require(near(volume(session), 1800), "The end face can be pulled again");
    const auto pushed = committed(session.pushPullFace(again, face(session, again, side(box, kernel::PrimitiveRole::boxNegativeX)), -2));
    // Coplanar faces of a join stay separate faces: the box side is still 10 x 10.
    require(near(volume(session), 1800 - 2 * 10 * 10) && session.validBodies().size() == 1,
            "Pushing the box's side face into the body cuts 2 x 10 x 10");

    // The distance stays editable; later push/pulls follow their faces.
    auto first = session.extrusionParameters(pulled);
    first.distance = 7;
    require(session.editExtrusionParameters(pulled, first, application::ErrorDisposition::reject)
                    .status() == CommandStatus::committed,
            "edit distance");
    require(near(volume(session), 10 * 10 * 20 - 2 * 10 * 10), "A changed distance moves on");
    // The extrusions follow the box when it grows.
    require(session.editBoxDimensions(box, {10, 10, 20}, application::ErrorDisposition::reject)
                    .status() == CommandStatus::committed,
            "edit box");
    require(near(volume(session), 10 * 10 * 30 - 2 * 10 * 20),
            "The push/pulls follow the grown box");

    // Undo returns to the earlier state, redo comes back.
    require(session.undo().status() == CommandStatus::committed &&
                near(volume(session), 10 * 10 * 20 - 2 * 10 * 10),
            "undo");
    require(session.redo().status() == CommandStatus::committed &&
                near(volume(session), 10 * 10 * 30 - 2 * 10 * 20),
            "redo");

    // Save/open keeps the face profiles and the cap reference.
    const auto path = std::filesystem::temp_directory_path() / "voelund-t04-push-pull.volund";
    const auto snapshot = session.identifiedSnapshot().snapshot();
    session.saveDocument(path);
    session.newDocument();
    session.loadDocument(path);
    std::filesystem::remove(path);
    require(std::ranges::equal(session.identifiedSnapshot().snapshot().parameters(),
                               snapshot.parameters()) &&
                near(volume(session), 10 * 10 * 30 - 2 * 10 * 20) &&
                session.extrusionParameters(again).faceProfile->origin.faceExtrusionCap &&
                !session.features().back().inputFeature,
            "Save and open keep push/pulls");
    static_cast<void>(pushed);
}

// Every box side reports a frame facing out of the box, so a client choosing a face by its
// normal (get_context) and a sketch on the bottom face both point away from the material.
void outwardFrames() {
    application::CadSession session;
    session.newDocument();
    const auto box = committed(session.createBoxFeature({10, 10, 10}));
    using kernel::PrimitiveRole;
    const struct {
        PrimitiveRole role;
        double x, y, z;
    } sides[] = {{PrimitiveRole::boxNegativeX, -1, 0, 0}, {PrimitiveRole::boxPositiveX, 1, 0, 0},
                 {PrimitiveRole::boxNegativeY, 0, -1, 0}, {PrimitiveRole::boxPositiveY, 0, 1, 0},
                 {PrimitiveRole::boxNegativeZ, 0, 0, -1}, {PrimitiveRole::boxPositiveZ, 0, 0, 1}};
    const auto faces = session.supportFaces(box);
    for (const auto& item : sides) {
        const auto found =
            std::ranges::find(faces, side(box, item.role), &features::SupportFaceFrame::origin);
        require(found != faces.end() && near(found->plane.normalX, item.x) &&
                    near(found->plane.normalY, item.y) && near(found->plane.normalZ, item.z),
                "Box side " + std::to_string(static_cast<int>(item.role)) + " faces outward");
    }
}

void rejections() {
    application::CadSession session;
    session.newDocument();
    const auto box = committed(session.createBoxFeature({10, 10, 10}));
    const auto before = session.identifiedSnapshot();
    const auto top = face(session, box, side(box, kernel::PrimitiveRole::boxPositiveZ));
    require(session.pushPullFace(box, top, 0).status() == CommandStatus::rejected &&
                session.identifiedSnapshot() == before,
            "A zero distance is rejected");
    features::SupportOrigin unknown{document::FeatureId{999}, kernel::PrimitiveRole::boxPositiveZ};
    require(session.pushPullFace(box, unknown, 5).status() == CommandStatus::rejected &&
                session.identifiedSnapshot() == before,
            "A face the body does not offer is rejected");
    // Pushing through the whole body leaves no solid: the evaluation fails, nothing is kept.
    require(session.pushPullFace(box, top, -10).status() != CommandStatus::committed &&
                session.identifiedSnapshot() == before,
            "Pushing the whole body away is not committed");
}
} // namespace

int main() {
    try {
        pushAndPull();
        outwardFrames();
        rejections();
        std::cout << "PASS T04a push/pull of body faces\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
