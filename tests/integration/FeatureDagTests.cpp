// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/features/FeatureEvaluationDetail.hpp"
#include "../BoundSketchFixture.hpp"
#include "../TestPaths.hpp"
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/features/FeatureDependencies.hpp>

namespace {
using namespace voelund;
using document::FeatureId;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Action> void rejects(Action action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    require(rejected, "Invalid dependency accepted");
}
void topology() {
    using features::FeatureDependencies;
    // Diamond with explicit two-input merge and an independent root, deliberately shuffled.
    const FeatureDependencies graph({{FeatureId{4}, {FeatureId{2}, FeatureId{3}}},
                                     {FeatureId{3}, {FeatureId{1}}},
                                     {FeatureId{5}, {}},
                                     {FeatureId{2}, {FeatureId{1}}},
                                     {FeatureId{1}, {}}});
    std::map<std::uint64_t, int> visits;
    for (const auto index : graph.order()) {
        const auto& node = graph.nodes()[index];
        for (const auto input : node.inputs)
            require(visits[input.value()] == 1, "Not topologically ordered");
        ++visits[node.feature.value()];
    }
    require(visits.size() == 5 && visits[1] == 1 && visits[4] == 1,
            "Diamond scheduled a node twice");
    const std::vector<FeatureId> changed{FeatureId{2}};
    require(graph.affected(changed) == std::vector<bool>{true, false, false, true, false},
            "Dirty propagation reached unrelated branch");
    rejects([] {
        (void)FeatureDependencies({{FeatureId{1}, {FeatureId{2}}}, {FeatureId{2}, {FeatureId{1}}}});
    });
    rejects([] { (void)FeatureDependencies({{FeatureId{1}, {FeatureId{1}}}}); });
    rejects([] { (void)FeatureDependencies({{FeatureId{1}, {FeatureId{99}}}}); });
    rejects([] { (void)FeatureDependencies({{FeatureId{1}, {}}, {FeatureId{1}, {}}}); });
    rejects([] {
        (void)FeatureDependencies(
            {{FeatureId{1}, {}}, {FeatureId{2}, {FeatureId{1}, FeatureId{1}}}});
    });
    auto retained = graph;
    rejects([&] {
        retained =
            FeatureDependencies({{FeatureId{1}, {FeatureId{2}}}, {FeatureId{2}, {FeatureId{1}}}});
    });
    require(std::ranges::equal(retained.nodes(), graph.nodes()),
            "Rejected cycle replaced valid graph");
}
document::ModelDocument fixture() {
    auto model = document::ModelDocument::empty();
    const auto a = model.addBoxFeature("A", {40, 30, 20});
    const auto sa = model.addSketchFeature(
        "Sketch A", a,
        {{0, 0, 20, 0, 0, 1, 1, 0, 0},
         {{{document::SketchElementId{1}, document::SketchRectangle{1, 1, 5, 5}}}, 2}});
    const auto ea = model.addExtrusionFeature("Extrude A", sa, {3, {document::SketchProfileId{1}}});
    const auto b = model.addBoxFeature("B", {10, 10, 10});
    const auto sb = model.addSketchFeature(
        "Sketch B", b,
        {{0, 0, 10, 0, 0, 1, 1, 0, 0},
         {{{document::SketchElementId{1}, document::SketchRectangle{1, 1, 5, 5}}}, 2}});
    (void)model.addExtrusionFeature("Extrude B", sb, {3, {document::SketchProfileId{1}}});
    (void)model.addCylinderFeature(
        "Dependent A", ea, {2, 5, {0, 0, -3, {}, {}, {}}, document::CylinderResultMode::join});
    test::bindTop(model, sa);
    test::bindTop(model, sb);
    return model;
}
std::map<std::uint64_t, int> evaluations;
void count(FeatureId id) { ++evaluations[id.value()]; }
struct CountScope {
    CountScope() {
        evaluations.clear();
        features::detail::beforeFeatureEvaluation = count;
    }
    ~CountScope() { features::detail::beforeFeatureEvaluation = nullptr; }
};
void incremental() {
    auto model = fixture();
    features::FeatureGraph graph;
    const auto first = graph.evaluateDetailed(document::DocumentSnapshot::capture(model));
    require(first->valid(), "Initial DAG geometry invalid");
    CountScope scope;
    auto repeated = graph.evaluateDetailed(document::DocumentSnapshot::capture(model), first);
    require(evaluations.empty(), "Unchanged nodes reevaluated");
    require(model.setBoxDimensions(FeatureId{4}, {12, 10, 10}), "Box edit failed");
    const auto edited =
        graph.evaluateDetailed(document::DocumentSnapshot::capture(model), repeated);
    require(edited->valid() && evaluations == std::map<std::uint64_t, int>{{4, 1}, {5, 1}, {6, 1}},
            "Independent branch reevaluated or shared support evaluated twice");
    evaluations.clear();
    require(model.setExtrusionParameters(FeatureId{3}, {3, {document::SketchProfileId{999}}}),
            "Error edit failed");
    const auto failed = graph.evaluateDetailed(document::DocumentSnapshot::capture(model), edited);
    require(!failed->valid() && evaluations == std::map<std::uint64_t, int>{{3, 1}} &&
                failed->report.features[2].status == features::EvaluationStatus::failed &&
                failed->report.features[6].status ==
                    features::EvaluationStatus::blockedByDependency &&
                failed->report.features[5].status == features::EvaluationStatus::valid &&
                failed->result.bodies.size() == 1,
            "Failure did not isolate actual dependent nodes/body");
    // L06: an appended feature is the only one evaluated; the earlier ones keep their results
    // and statuses, the failure included.
    evaluations.clear();
    const auto appended = model.addBoxFeature("Appended", {4, 4, 4});
    const auto extended =
        graph.evaluateDetailed(document::DocumentSnapshot::capture(model), failed);
    require(evaluations == std::map<std::uint64_t, int>{{appended.value(), 1}} &&
                extended->report.features[2].status == features::EvaluationStatus::failed &&
                extended->report.features.back().status == features::EvaluationStatus::valid,
            "Appending a feature reevaluated earlier ones");
}
void validB(const application::CadSession& session, double volume) {
    const auto bodies = session.validBodies();
    const auto b = std::ranges::find(bodies, document::BodyId{4}, &features::EvaluatedBody::bodyId);
    require(b != bodies.end() && b->sourceFeature == FeatureId{6} &&
                BRepCheck_Analyzer(b->shape).IsValid(),
            "Independent current body missing");
    GProp_GProps properties;
    BRepGProp::VolumeProperties(b->shape, properties);
    require(std::abs(properties.Mass() - volume) < 1e-7, "Independent body has wrong volume");
    require(session.featureShape(FeatureId{4}).has_value(),
            "Independent branch geometry gated globally");
}
void product(const std::filesystem::path& path) {
    {
        std::ofstream output(path, std::ios::binary);
        document::DocumentSerializer::write(fixture(), output);
    }
    application::CadSession session;
    session.loadDocument(path);
    require(session.selectFeature(FeatureId{3}), "Select A extrusion");
    require(
        session.editSelectedExtrusionParameters({3, {document::SketchProfileId{999}}},
                                                application::ErrorDisposition::keepEditableErrors)
                .status() == application::CommandStatus::committedWithErrors,
        "Explicit failure not committed");
    require(session.featureStates()[2].status == application::FeatureRuntimeStatus::error &&
                session.featureStates()[6].status == application::FeatureRuntimeStatus::blocked &&
                session.featureStates()[5].status == application::FeatureRuntimeStatus::current,
            "Application still blocks by feature order");
    validB(session, 1048);
    require(session.selectFeature(FeatureId{4}), "Select independent box");
    require(session.editSelectedBoxDimensions({12, 10, 10}).status() ==
                application::CommandStatus::committedWithErrors,
            "Unrelated old failure rejected valid edit");
    validB(session, 1248);
    require(session.undo().status() == application::CommandStatus::committedWithErrors,
            "Undo B edit failed");
    validB(session, 1048);
    require(session.redo().status() == application::CommandStatus::committedWithErrors,
            "Redo B edit failed");
    validB(session, 1248);
    session.saveDocument(path);
    const auto saved = session.identifiedSnapshot();
    session.loadDocument(path);
    require(session.identifiedSnapshot() == saved && !session.isModified(),
            "DAG errors did not persist");
    validB(session, 1248);
    require(
        session.selectFeature(FeatureId{3}) &&
            session.editSelectedExtrusionParameters({3, {document::SketchProfileId{1}}}).status() ==
                application::CommandStatus::committed &&
            session.sceneStatus().isCurrent(),
        "Repair did not unblock descendants");
    const auto before = session.identifiedSnapshot();
    const auto stamp = session.sceneStatus().document;
    const auto selected = session.selectedFeature();
    {
        std::ofstream output(path, std::ios::binary);
        output << "VOLUND 16\nNEXT_ID 4\nFEATURES 3\nBOX 1 \"A\" 10 10 10 0 0 0 0 0 0\n"
                  "CYLINDER 2 \"Cycle A\" 3 2 5 0 0 0 0 0 0 JOIN\n"
                  "CYLINDER 3 \"Cycle B\" 2 2 5 0 0 0 0 0 0 JOIN\n";
    }
    rejects([&] { session.loadDocument(path); });
    require(session.identifiedSnapshot() == before && session.sceneStatus().document == stamp &&
                session.selectedFeature() == selected && session.isModified() && session.canUndo(),
            "Cyclic input replaced live document/history");
}
} // namespace
int main() {
    const auto path = test::temporaryFilePath(
        "dag-" + document::documentIdToHex(document::generateDocumentId()) + ".volund");
    try {
        topology();
        incremental();
        product(path);
        std::filesystem::remove(path);
        std::cout << "PASS stable DAG, diamond, cycles, incremental evaluation, independent "
                     "errors/edit/repair/undo/save/open\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        return 1;
    }
}
