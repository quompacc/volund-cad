#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// AI02c, A-ACTION-01: preview on an isolated candidate, a two-step action as one commit and
// one undo step, failure in step 2 leaves nothing, and request IDs never mutate twice.
namespace {
using namespace voelund;
using application::ActionDispatcher;
using application::ActionRejection;
using application::ActionRequest;
using application::CommandStatus;
using application::Quantity;
using application::QuantityUnit;
using application::RejectionReason;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
Quantity mm(double value) { return {value, QuantityUnit::millimetre}; }

ActionRequest request(const application::CadSession& session, std::string id,
                      application::Action action) {
    return {std::move(id), session.documentId(), session.sceneStatus().document,
            std::move(action)};
}
document::SketchParameters datumSketch(std::vector<document::SketchElement> elements) {
    const auto next = elements.size() + 1;
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xy),
                                      {std::move(elements), next}};
    sketch.datum = document::DatumPlane::xy;
    return sketch;
}
document::SketchParameters circle(double radius) {
    return datumSketch(
        {{document::SketchElementId{1}, document::SketchCircle{{0, 0}, radius}}});
}
application::CreateSketchExtrusionAction plate(double radius, double distance,
                                               std::vector<document::SketchRegionId> regions = {}) {
    return {circle(radius), std::move(regions), mm(distance)};
}

// Model content without the ID watermark, which undo deliberately does not lower.
bool sameContent(const document::IdentifiedDocumentSnapshot& left,
                 const document::IdentifiedDocumentSnapshot& right) {
    const auto& a = left.snapshot();
    const auto& b = right.snapshot();
    return left.id() == right.id() && std::ranges::equal(a.features(), b.features()) &&
           std::ranges::equal(a.parameters(), b.parameters()) &&
           a.parameterTable() == b.parameterTable();
}

// Captures everything a rejected or previewed request must leave alone.
struct Unchanged final {
    explicit Unchanged(const application::CadSession& session)
        : session_(session), snapshot_(session.identifiedSnapshot()),
          stamp_(session.sceneStatus().document), canUndo_(session.canUndo()),
          canRedo_(session.canRedo()) {}
    void check(const std::string& message) const {
        require(session_.identifiedSnapshot() == snapshot_ &&
                    session_.sceneStatus().document == stamp_ && session_.canUndo() == canUndo_ &&
                    session_.canRedo() == canRedo_,
                message + ": document, stamp and history unchanged");
    }
    const application::CadSession& session_;
    document::IdentifiedDocumentSnapshot snapshot_;
    application::DocumentStamp stamp_;
    bool canUndo_;
    bool canRedo_;
};

void oneCommitOneUndo() {
    application::CadSession session;
    session.newDocument();
    ActionDispatcher dispatcher(session);
    const auto before = session.identifiedSnapshot();
    const auto result = dispatcher.execute(request(session, "plate", plate(10, 5)));
    require(result.command.status() == CommandStatus::committed &&
                result.createdFeatures.size() == 2 &&
                near(test::volumeOf(session.currentShape()), std::numbers::pi * 100 * 5),
            "Sketch and extrusion in one request: " + result.command.diagnostic());
    const auto after = session.identifiedSnapshot();
    require(session.undo().status() == CommandStatus::committed &&
                sameContent(session.identifiedSnapshot(), before) && !session.canUndo(),
            "Exactly one undo step removes sketch and extrusion");
    require(session.redo().status() == CommandStatus::committed &&
                session.identifiedSnapshot() == after &&
                near(test::volumeOf(session.currentShape()), std::numbers::pi * 100 * 5),
            "Redo restores the identical state");
}

void failureInStepTwo() {
    application::CadSession session;
    session.newDocument();
    ActionDispatcher dispatcher(session);
    require(dispatcher.execute(request(session, "box", application::CreateBoxAction{
                                                          mm(10), mm(10), mm(10)}))
                    .command.status() == CommandStatus::committed,
            "Starting body");
    const Unchanged state(session);
    // Each sketch alone would be valid; the extrusion step fails inside the transaction.
    auto result = dispatcher.execute(request(session, "negative", plate(10, -5)));
    require(result.command.status() == CommandStatus::rejected &&
                result.command.reason() == RejectionReason::invalidInput &&
                result.rejection == ActionRejection::none,
            "A negative distance fails in step 2");
    state.check("Negative distance");
    const auto line = datumSketch(
        {{document::SketchElementId{1}, document::SketchLine{{0, 0}, {10, 0}}}});
    result = dispatcher.execute(
        request(session, "open", application::CreateSketchExtrusionAction{line, {}, mm(5)}));
    require(result.command.status() == CommandStatus::rejected &&
                result.command.reason() == RejectionReason::invalidInput,
            "An open sketch has no region for step 2");
    state.check("Open sketch");
    const auto foreign = document::SketchRegionId::fromBoundaries(
        {{document::SketchElementId{9}, 0, std::nullopt}}, {});
    result = dispatcher.execute(request(session, "foreign region", plate(10, 5, {foreign})));
    require(result.rejection == ActionRejection::unresolvedReference,
            "A region outside the new sketch is rejected before the transaction");
    state.check("Foreign region");
}

void previewIsIsolated() {
    application::CadSession session;
    session.newDocument();
    ActionDispatcher dispatcher(session);
    const auto proposal = request(session, "proposal", plate(10, 5));
    const Unchanged state(session);
    const auto preview = dispatcher.preview(proposal);
    require(preview.result.command.status() == CommandStatus::committed &&
                preview.result.createdFeatures.size() == 2 && preview.bodies.size() == 1 &&
                near(test::volumeOf(preview.bodies.front().shape), std::numbers::pi * 100 * 5) &&
                preview.result.stamp == session.sceneStatus().document,
            "The preview evaluates the candidate");
    state.check("Preview");
    // The user changes the document while the proposal is shown.
    require(session.createBoxFeature().status() == CommandStatus::committed, "User adds a box");
    const auto features = session.features().size();
    const auto stale = dispatcher.execute(proposal);
    require(stale.rejection == ActionRejection::staleStamp && session.features().size() == features,
            "The user's change invalidates the previewed commit");
    const auto fresh = dispatcher.execute(request(session, "proposal again", plate(10, 5)));
    require(fresh.command.status() == CommandStatus::committed &&
                session.features().size() == features + 2,
            "A request on the current stamp commits");
}

void requestIds() {
    application::CadSession session;
    session.newDocument();
    ActionDispatcher dispatcher(session);
    const auto create = request(session, "A",
                                application::CreateParameterAction{
                                    "A", document::ParameterUnit::length, mm(10), {}});
    const auto first = dispatcher.execute(create);
    require(first.command.status() == CommandStatus::committed && !first.repeated,
            "First execution");
    const Unchanged state(session);
    const auto second = dispatcher.execute(create);
    require(second.repeated && second.createdParameters == first.createdParameters &&
                second.stamp == first.stamp && session.parameterTable().values.size() == 1,
            "A repeated request returns the stored result");
    state.check("Repetition");

    auto changed = create;
    changed.action =
        application::CreateParameterAction{"A", document::ParameterUnit::length, mm(20), {}};
    require(dispatcher.execute(changed).rejection == ActionRejection::requestConflict,
            "Other content under the same ID");
    changed = create;
    changed.expected = session.sceneStatus().document;
    require(dispatcher.execute(changed).rejection == ActionRejection::requestConflict,
            "Another stamp is other content too");
    require(dispatcher
                    .execute(request(session, "",
                                     application::RenameParameterAction{
                                         first.createdParameters.front(), "B"}))
                    .rejection == ActionRejection::missingRequestId,
            "A request needs an ID");
    state.check("Conflicts");

    // The table is bounded: after `capacity` newer requests the first one is forgotten and
    // runs again, now against a stale stamp.
    for (std::size_t index = 0; index < ActionDispatcher::capacity; ++index)
        static_cast<void>(dispatcher.execute(
            request(session, "fill " + std::to_string(index),
                    application::RenameParameterAction{document::ParameterId{99}, "B"})));
    const auto forgotten = dispatcher.execute(create);
    require(!forgotten.repeated && forgotten.rejection == ActionRejection::staleStamp,
            "Evicted IDs are checked anew");
    state.check("Bounded table");

    // A new document generation starts an empty table.
    const auto reused = request(session, "reused",
                                application::CreateParameterAction{
                                    "C", document::ParameterUnit::length, mm(1), {}});
    require(dispatcher.execute(reused).command.status() == CommandStatus::committed, "Reused 1");
    session.newDocument();
    const auto again = request(session, "reused",
                               application::CreateParameterAction{
                                   "D", document::ParameterUnit::length, mm(2), {}});
    require(dispatcher.execute(again).command.status() == CommandStatus::committed,
            "The ID is free again in the next generation");
}
} // namespace

int main() {
    try {
        oneCommitOneUndo();
        failureInStepTwo();
        previewIsIsolated();
        requestIds();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
