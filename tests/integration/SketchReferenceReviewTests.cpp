// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
using namespace voelund;
using namespace document;
using features::SketchReferenceReviewStatus;
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
SketchParameters sketch() {
    return {{0, 0, 10, 0, 0, 1, 1, 0, 0},
            {{{SketchElementId{1}, SketchLine{{2, 2}, {6, 6}, SketchLineConstraint::none}}}, 2},
            {}};
}
std::size_t matches(const features::SketchReferenceReview& review) {
    return static_cast<std::size_t>(std::ranges::count_if(
        review.candidates, [](const auto& c) { return c.matchesOriginalPlane; }));
}
void classification() {
    auto model = ModelDocument::empty();
    const auto box = model.addBoxFeature("Box", {10, 10, 10});
    const auto evaluated =
        features::FeatureGraph{}.evaluateDetailed(DocumentSnapshot::capture(model));
    auto support = evaluated->result.features.front();
    auto original = sketch();
    original.constraints.values.push_back({SketchConstraintId{1},
                                           SketchConstraintKind::coordinateX,
                                           {SketchElementId{1}, SketchPointPosition::start},
                                           std::nullopt,
                                           2,
                                           std::nullopt,
                                           {3, .25}});
    original.constraints.nextId = 2;
    const auto reviewed = features::reviewSketchReferences(model.documentId(), original, &support);
    require(reviewed.status == SketchReferenceReviewStatus::legacyNeedsExplicitChoice &&
                matches(reviewed) == 1 && reviewed.candidates.size() == 6,
            "Unique plane was not just a proposal");
    require(reviewed.original == original &&
                reviewed.legacyEdgeConstraints == std::vector{SketchConstraintId{1}},
            "Legacy values or segment IDs changed");
    std::ranges::reverse(support.supportOrigins);
    const auto reordered = features::reviewSketchReferences(model.documentId(), original, &support);
    for (std::size_t i = 0; i < reviewed.candidates.size(); ++i)
        require(reviewed.candidates[i].binding == reordered.candidates[i].binding,
                "Origin iteration order changed candidate order");
    const auto candidate = *std::ranges::find_if(
        reviewed.candidates, [](const auto& c) { return c.matchesOriginalPlane; });
    auto bound = sketch();
    bound.supportBinding = candidate.binding;
    require(features::reviewSketchReferences(model.documentId(), bound, &support).status ==
                SketchReferenceReviewStatus::current,
            "Explicit current reference misclassified");
    auto broken = bound;
    broken.supportBinding->reference.origin.feature = FeatureId{999};
    require(features::reviewSketchReferences(model.documentId(), broken, &support).status ==
                SketchReferenceReviewStatus::brokenBinding,
            "Broken reference silently replaced");
    broken = bound;
    broken.supportBinding->reference.document = ModelDocument::empty().documentId();
    require(features::reviewSketchReferences(model.documentId(), broken, &support).status ==
                SketchReferenceReviewStatus::brokenBinding,
            "Foreign namespace accepted");
    auto invalid = original;
    invalid.plane.normalZ = 0;
    const auto invalidReview =
        features::reviewSketchReferences(model.documentId(), invalid, &support);
    require(invalidReview.status == SketchReferenceReviewStatus::invalidLegacyPlane &&
                invalidReview.original == invalid && matches(invalidReview) == 0,
            "Invalid plane normalized implicitly");
    require(features::reviewSketchReferences(model.documentId(), original, nullptr).status ==
                SketchReferenceReviewStatus::supportUnavailable,
            "Missing output guessed");
    // A complete name mapping to two faces cannot supply a persistent repair candidate.
    support.supportOrigins.front().faces.push_back(support.supportOrigins.back().faces.front());
    support.supportOrigins[1].coverage = kernel::HistoryCoverage::unavailable;
    support.supportOrigins[2].faces.clear();
    auto blocked = features::reviewSketchReferences(model.documentId(), original, &support);
    require(blocked.unresolvedOrigins >= 3 && blocked.candidates.size() < 6,
            "Unproven history offered as repair");
    (void)box;
    std::cout << "legacy single-match proposal, stable order, original values, namespaces and "
                 "history rejection passed\n";
}
void coplanar() {
    auto model = ModelDocument::empty();
    (void)model.addBoxFeature("A", {10, 10, 10});
    (void)model.addBoxFeature("B", {20, 10, 10});
    const auto e = features::FeatureGraph{}.evaluateDetailed(DocumentSnapshot::capture(model));
    auto support = e->result.features.front();
    const auto& other = e->result.features.back();
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    builder.Add(compound, support.shape);
    builder.Add(compound, other.shape);
    support.shape = compound;
    support.supportOrigins.insert(support.supportOrigins.end(), other.supportOrigins.begin(),
                                  other.supportOrigins.end());
    const auto review = features::reviewSketchReferences(model.documentId(), sketch(), &support);
    require(matches(review) == 2 &&
                review.status == SketchReferenceReviewStatus::legacyNeedsExplicitChoice,
            "Coplanar alternatives reduced to arbitrary winner");
    require(!review.original.supportBinding, "Inspection created binding");
    std::cout << "two coplanar named faces retained without automatic choice passed\n";
}
void applicationBoundary(const std::filesystem::path& directory) {
    auto model = ModelDocument::empty();
    const auto box = model.addBoxFeature("Box", {10, 10, 10});
    const auto id = model.addSketchFeature("Legacy", box, sketch());
    const auto path = directory / "reference-review-source.volund";
    {
        std::ofstream file(path, std::ios::binary);
        DocumentSerializer::write(model, file);
    }
    const auto bytes = [&] {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };
    const auto originalBytes = bytes();
    application::CadSession session;
    session.loadDocument(path);
    const auto before = session.identifiedSnapshot();
    const auto stamp = session.sceneStatus();
    const auto selected = session.selectedFeature();
    const auto dirty = session.isModified();
    const auto undo = session.canUndo(), redo = session.canRedo();
    const auto review = session.inspectSketchReferences(id);
    require(review.document == session.documentId() && review.stamp == stamp.document &&
                review.feature == id && review.support == box,
            "Inspection scope missing");
    require(review.review.status == SketchReferenceReviewStatus::legacyNeedsExplicitChoice &&
                matches(review.review) == 1,
            "Native unbound source not identified");
    require(session.identifiedSnapshot() == before &&
                session.sceneStatus().document == stamp.document &&
                session.selectedFeature() == selected && session.isModified() == dirty &&
                session.canUndo() == undo && session.canRedo() == redo && bytes() == originalBytes,
            "Read-only inspection mutated document/file/history");
    session.duplicateDocument();
    const auto copy = session.inspectSketchReferences(id);
    require(copy.document != review.document && copy.stamp != review.stamp,
            "Inspection did not carry document replacement identity");
    for (const auto& c : copy.review.candidates)
        require(c.binding.reference.document == copy.document, "Stale candidate namespace");
    std::cout << "native inspection preserves file, dirty state, selection and undo; replacement "
                 "stamp passed\n";
}
void legacyText(const std::filesystem::path& fixtures, const std::filesystem::path& directory) {
    const auto path = fixtures / "v16.volund";
    const auto bytes = [&] {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), {});
    };
    const auto originalBytes = bytes();
    application::CadSession session;
    session.loadDocument(path);
    const auto original = session.sketchParameters(FeatureId{3});
    const auto report = session.lastImportReport();
    const auto stamp = session.sceneStatus().document;
    const auto inspected = session.inspectSketchReferences(FeatureId{3});
    require(inspected.review.status == SketchReferenceReviewStatus::legacyNeedsExplicitChoice &&
                matches(inspected.review) == 1 && inspected.review.original == original,
            "Text import silently assigned geometric face match");
    require(session.lastImportReport() == report && session.sceneStatus().document == stamp &&
                bytes() == originalBytes,
            "Inspection altered import report/source");
    const auto converted = directory / "reference-review-converted.volund";
    session.saveDocument(converted);
    session.loadDocument(converted);
    const auto reopened = session.inspectSketchReferences(FeatureId{3});
    require(reopened.review.status == SketchReferenceReviewStatus::legacyNeedsExplicitChoice &&
                reopened.review.original == original && bytes() == originalBytes,
            "Native Save/Open discarded original unresolved reference");
    std::cout
        << "legacy text proposal and native Save/Open preserve original unresolved data passed\n";
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Expected artifact and fixture directories");
        classification();
        coplanar();
        applicationBoundary(argv[1]);
        legacyText(argv[2], argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
