#include "CadSessionTestSupport.hpp"

#include <TopExp_Explorer.hxx>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// T05: pulling the top of a box leaves no seams; pushing a side works on the whole side.
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
            std::cerr << "feature " << issue.feature.value() << ": " << issue.message << std::endl;
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
features::SupportOrigin cap(document::FeatureId pushPull) {
    return {pushPull, std::nullopt, {}, true};
}

void pushAndPull() {
    application::CadSession session;
    session.newDocument();
    const auto box = committed(session.createBoxFeature({10, 10, 10}));
    require(near(volume(session), 1000), "box");

    const auto pulled = committed(session.pushPullFace(
        box, face(session, box, side(box, kernel::PrimitiveRole::boxPositiveZ)), 5));
    require(near(volume(session), 1500) && session.validBodies().size() == 1,
            "Pulling the top face joins 10 x 10 x 5");
    const auto end = face(session, pulled, cap(pulled)); // its own stable origin
    const auto again = committed(session.pushPullFace(pulled, end, 3));
    require(near(volume(session), 1800), "The end face can be pulled again");
    require(session.features().back().name == "Ziehen 2", "A pull is named after what it does");
    // T05: no seams; every side is one face, named by the box; the second pull's sides are
    // named too (from the cap edges of the first), as second names of those faces.
    int faces = 0;
    for (TopExp_Explorer explorer(session.currentShape(), TopAbs_FACE); explorer.More();
         explorer.Next())
        ++faces;
    require(faces == 6, "Six faces without seams, got " + std::to_string(faces));
    const auto frames = session.supportFaces(again);
    const auto boxSide = std::ranges::find(frames, side(box, kernel::PrimitiveRole::boxNegativeX),
                                           &features::SupportFaceFrame::origin);
    require(boxSide != frames.end() && !boxSide->secondary &&
                near(std::max(boxSide->maxX - boxSide->minX, boxSide->maxY - boxSide->minY), 18),
            "The box side spans all 18 mm");
    require(std::ranges::count_if(frames,
                                  [&](const features::SupportFaceFrame& frame) {
                                      return frame.origin.feature == again &&
                                             frame.origin.extrusionFace ==
                                                 document::ExtrusionFace::side &&
                                             frame.secondary;
                                  }) == 4,
            "The second pull's four sides are named");
    const auto pushed = committed(session.pushPullFace(
        again, face(session, again, side(box, kernel::PrimitiveRole::boxNegativeX)), -2));
    // T05: the box side is the whole side now, 10 x 18.
    require(near(volume(session), 1800 - 2 * 10 * 18) && session.validBodies().size() == 1,
            "Pushing the box's side face into the body cuts 2 x 10 x 18");

    // The distance stays editable; later push/pulls follow their faces.
    auto first = session.extrusionParameters(pulled);
    first.distance = 7;
    require(session.editExtrusionParameters(pulled, first, application::ErrorDisposition::reject)
                    .status() == CommandStatus::committed,
            "edit distance");
    require(near(volume(session), 10 * 10 * 20 - 2 * 10 * 20), "A changed distance moves on");
    // The extrusions follow the box when it grows.
    require(session.editBoxDimensions(box, {10, 10, 20}, application::ErrorDisposition::reject)
                    .status() == CommandStatus::committed,
            "edit box");
    require(near(volume(session), 10 * 10 * 30 - 2 * 10 * 30),
            "The push/pulls follow the grown box");

    // Undo returns to the earlier state, redo comes back.
    require(session.undo().status() == CommandStatus::committed &&
                near(volume(session), 10 * 10 * 20 - 2 * 10 * 20),
            "undo");
    require(session.redo().status() == CommandStatus::committed &&
                near(volume(session), 10 * 10 * 30 - 2 * 10 * 30),
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
                near(volume(session), 10 * 10 * 30 - 2 * 10 * 30) &&
                session.extrusionParameters(again).faceProfile->origin.faceExtrusion &&
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

// T04b: the faces of `support`'s result facing (nx, ny, nz) whose origin is of `kind`.
std::vector<features::SupportOrigin> facing(const application::CadSession& session,
                                            document::FeatureId support, double nx, double ny,
                                            double nz, document::ExtrusionFace kind) {
    std::vector<features::SupportOrigin> result;
    for (const auto& item : session.supportFaces(support))
        if (!item.origin.role && item.origin.extrusionFace == kind &&
            near(item.plane.normalX, nx) && near(item.plane.normalY, ny) &&
            near(item.plane.normalZ, nz))
            result.push_back(item.origin);
    return result;
}

// T04b: start and side faces of extrusions are stable and can be pushed or pulled: the side
// and the bottom of a sketched plate, and the side of a pull named by the box edge it was
// swept from. Save/open keeps the side origins.
void startAndSideFaces() {
    application::CadSession session;
    session.newDocument();
    document::SketchParameters plate{
        document::datumSketchPlane(document::DatumPlane::xy),
        {{{document::SketchElementId{1}, document::SketchRectangle{-40, -20, 40, 20}}}, 2}};
    plate.datum = document::DatumPlane::xy;
    const auto extrusion = committed(session.createDatumSketchExtrusion(plate, {}, 10));
    require(near(volume(session), 32000), "plate");
    using document::ExtrusionFace;
    const auto right = facing(session, extrusion, 1, 0, 0, ExtrusionFace::side);
    require(right.size() == 1 && right.front().sweptEdge, "The plate offers its +x side");
    const auto wider = committed(session.pushPullFace(extrusion, right.front(), 5));
    require(near(volume(session), 32000 + 5 * 40 * 10), "Pulling the plate's side widens it");
    const auto bottom = facing(session, wider, 0, 0, -1, ExtrusionFace::start);
    require(bottom.size() == 1, "The plate offers its bottom (start) face");
    static_cast<void>(committed(session.pushPullFace(wider, bottom.front(), -2)));
    // T05: the pull merged its seams, so the bottom is one face of 85 x 40.
    require(near(volume(session), 34000 - 85 * 40 * 2),
            "Pushing the bottom in cuts 2 mm from the whole bottom");

    // The sides of a pull are named by the box edges they were swept from.
    const auto box = committed(session.createBoxFeature({10, 10, 10}));
    const double boxes = volume(session);
    const auto pulled = committed(session.pushPullFace(
        box, face(session, box, side(box, kernel::PrimitiveRole::boxPositiveZ)), 5));
    const auto pullSide = facing(session, pulled, 1, 0, 0, ExtrusionFace::side);
    require(pullSide.size() == 1 && pullSide.front().faceExtrusion &&
                std::holds_alternative<document::PrimitiveEdgeOrigin>(
                    pullSide.front().sweptEdge->value),
            "The side of a pull is named by its box edge");
    static_cast<void>(committed(session.pushPullFace(pulled, pullSide.front(), 2)));
    // T05: the pull's side is (a second name of) the whole box side, 15 high.
    require(near(volume(session), boxes + 10 * 10 * 5 + 2 * 10 * 15),
            "Pulling the side of a pull widens the whole side");

    const auto path = std::filesystem::temp_directory_path() / "voelund-t04b-sides.volund";
    const auto snapshot = session.identifiedSnapshot().snapshot();
    const double before = volume(session);
    session.saveDocument(path);
    session.newDocument();
    session.loadDocument(path);
    std::filesystem::remove(path);
    require(std::ranges::equal(session.identifiedSnapshot().snapshot().parameters(),
                               snapshot.parameters()) &&
                near(volume(session), before),
            "Save and open keep start and side origins");
}
} // namespace

int main() {
    try {
        pushAndPull();
        outwardFrames();
        rejections();
        startAndSideFaces();
        std::cout << "PASS T04a/T04b push/pull of body faces, start and side faces\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
