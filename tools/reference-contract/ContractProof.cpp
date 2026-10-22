// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SubshapeContract.hpp"

#include <iostream>
#include <stdexcept>

using namespace voelund::reference_contract;
using namespace voelund::document;

namespace {
const DocumentId document{1, 2};
const SubshapeRef top{document, BodyId{1}, FeatureId{1}, Topology::face,
                      BoxFaceKey{BoxSide::positiveZ}};
const EvaluationScope scope{document, BodyId{1}, FeatureId{9}, 42};
const Candidate faceA{"face-A", Topology::face};
const Candidate faceB{"face-B", Topology::face};
int checks = 0;

void check(const char* id, const SubshapeRef& ref, const EvaluationScope& target,
           const LineageProjection& lineage, Status status, Reason reason, std::size_t count) {
    const auto result = classify(ref, target, lineage);
    if (result.status != status || result.reason != reason || result.candidates.size() != count ||
        result.scope != target)
        throw std::runtime_error(id);
    auto reversed = lineage;
    std::reverse(reversed.candidates.begin(), reversed.candidates.end());
    const auto repeat = classify(ref, target, reversed);
    if (repeat.status != result.status || repeat.reason != result.reason ||
        repeat.candidates != result.candidates)
        throw std::runtime_error("Traversal order affected resolution");
    ++checks;
    std::cout << id << " PASS\n";
}
void check(const char* id, Coverage coverage, std::vector<Candidate> candidates, Status status,
           Reason reason, std::size_t count) {
    check(id, top, scope, {top, scope, coverage, std::move(candidates)}, status, reason, count);
}
template <class F> void reject(const char* id, F action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        ++checks;
        std::cout << id << " PASS\n";
        return;
    }
    throw std::runtime_error(id);
}

void topologyFixtures() {
    check("box-top", Coverage::complete, {faceA}, Status::resolved, Reason::unique, 1);
    // Coplanarity is irrelevant: only A has the requested provenance.
    check("coplanar-known-origin", Coverage::complete, {faceA}, Status::resolved, Reason::unique,
          1);
    check("coplanar-legacy-tie", Coverage::unavailable, {faceB, faceA}, Status::ambiguous,
          Reason::historyUnavailable, 2);
    check("split", Coverage::complete, {faceA, faceB}, Status::ambiguous, Reason::split, 2);
    check("merge", Coverage::complete, {faceA}, Status::resolved, Reason::unique, 1);
    check("split-then-merge", Coverage::complete, {faceA, faceA}, Status::resolved, Reason::unique,
          1);
    check("deleted", Coverage::deleted, {}, Status::missing, Reason::deleted, 0);
    check("rotated-90", Coverage::complete, {{"rotated-A", Topology::face}}, Status::resolved,
          Reason::unique, 1);
    check("boolean-modified", Coverage::complete, {{"cut-A", Topology::face}}, Status::resolved,
          Reason::unique, 1);
    check("boolean-generated-is-not-continuation", Coverage::deleted, {}, Status::missing,
          Reason::deleted, 0);
    check("boolean-history-gap", Coverage::unavailable, {}, Status::missing,
          Reason::historyUnavailable, 0);
    check("single-geometric-suggestion", Coverage::unavailable, {faceA}, Status::missing,
          Reason::historyUnavailable, 1);
}
void identityFixtures() {
    auto ref = top;
    ref.document = DocumentId{3, 4};
    check("foreign-document", ref, scope, {ref, scope, Coverage::complete, {faceA}},
          Status::missing, Reason::wrongScope, 0);
    ref = top;
    ref.body = BodyId{2};
    check("foreign-body", ref, scope, {ref, scope, Coverage::complete, {faceA}}, Status::missing,
          Reason::wrongScope, 0);
    auto old = scope;
    --old.revision;
    check("stale-revision", top, scope, {top, old, Coverage::complete, {faceA}}, Status::missing,
          Reason::wrongScope, 0);
    old = scope;
    old.outputFeature = FeatureId{8};
    check("wrong-output-producer", top, scope, {top, old, Coverage::complete, {faceA}},
          Status::missing, Reason::wrongScope, 0);
    ref = top;
    ref.origin = BoxFaceKey{BoxSide::negativeZ};
    check("different-origin-role", top, scope, {ref, scope, Coverage::complete, {faceA}},
          Status::missing, Reason::unknownOrigin, 0);
    ref = top;
    ref.originFeature = FeatureId{2};
    check("different-origin-feature", top, scope, {ref, scope, Coverage::complete, {faceA}},
          Status::missing, Reason::unknownOrigin, 0);
    const SubshapeRef cap{document, BodyId{1}, FeatureId{3}, Topology::face,
                          ExtrusionCapKey{"region:outer-loop-A", Cap::end}};
    check("extrusion-end-cap", cap, scope, {cap, scope, Coverage::complete, {faceA}},
          Status::resolved, Reason::unique, 1);
    const ContourEdgeKey boundary{FeatureId{2}, 17, ElementPart::rectangleBottom};
    const SubshapeRef edge{document, BodyId{1}, FeatureId{2}, Topology::edge, boundary};
    check("contour-edge-tessellation-independent", edge, scope,
          {edge, scope, Coverage::complete, {{"curve-17-bottom", Topology::edge}}},
          Status::resolved, Reason::unique, 1);
    const SubshapeRef side{document, BodyId{1}, FeatureId{3}, Topology::face,
                           ExtrusionSideKey{"region:outer-loop-A", boundary}};
    check("extrusion-side", side, scope, {side, scope, Coverage::complete, {faceA}},
          Status::resolved, Reason::unique, 1);
}
void invalidFixtures() {
    reject("invalid-topology", [] {
        (void)classify(top, scope, {top, scope, Coverage::complete, {{"edge", Topology::edge}}});
    });
    reject("contradictory-deletion",
           [] { (void)classify(top, scope, {top, scope, Coverage::deleted, {faceA}}); });
    reject("unexplained-empty-history",
           [] { (void)classify(top, scope, {top, scope, Coverage::complete, {}}); });
    reject("zero-origin-id", [] {
        auto ref = top;
        ref.originFeature = FeatureId{0};
        (void)classify(ref, scope, {ref, scope, Coverage::complete, {faceA}});
    });
    reject("empty-region-key", [] {
        auto ref = top;
        ref.origin = ExtrusionCapKey{"", Cap::end};
        (void)classify(ref, scope, {ref, scope, Coverage::complete, {faceA}});
    });
}
} // namespace

int main() {
    try {
        topologyFixtures();
        identityFixtures();
        invalidFixtures();
        std::cout << checks << " contract fixtures passed (symbolic, no OCCT history proof)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
