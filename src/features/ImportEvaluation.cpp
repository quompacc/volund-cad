// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>

#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>

// M17: an imported solid is a body as stored with its import. Its faces are named by their
// number in the stored solid, so later features find them again; its edges have no names of
// their own (fillets and chamfers name them by their faces).
namespace voelund::features::detail {
namespace {

void addSolid(const document::FeatureDescriptor& descriptor, const TopoDS_Shape& solid,
              std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
              std::vector<EvaluatedBody>& referenceBodies) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
    TopExp::MapShapes(solid, TopAbs_FACE, faces);
    std::vector<SupportProvenance> origins;
    for (int index = 1; index <= faces.Extent(); ++index) {
        SupportOrigin origin{descriptor.id, std::nullopt};
        origin.importedFace = static_cast<std::uint32_t>(index);
        SupportProvenance item{std::move(origin), {faces(index)}};
        item.frameFace = faces(index);
        origins.push_back(std::move(item));
    }
    const document::BodyId bodyId{descriptor.id.value()};
    outputs.push_back(EvaluatedFeature{descriptor.id, solid, solid, bodyId, origins, {}, origins});
    bodies.push_back(EvaluatedBody{bodyId, descriptor.id, solid});
    referenceBodies.push_back(EvaluatedBody{bodyId, descriptor.id, solid});
}

} // namespace

void evaluateImport(const document::DocumentSnapshot& document,
                    const document::FeatureDescriptor& descriptor,
                    std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                    std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.importParameters(descriptor.id);
    addSolid(descriptor, parameters.solids.front().solid, outputs, bodies, referenceBodies);
}

void evaluateImportPart(const document::DocumentSnapshot& document,
                        const document::FeatureDescriptor& descriptor,
                        std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                        std::vector<EvaluatedBody>& referenceBodies) {
    const auto& part = document.importPartParameters(descriptor.id);
    const auto& parameters = document.importParameters(part.import);
    if (part.index >= parameters.solids.size())
        throw GeometryEvaluationError("The import has no such solid", "import-part-missing");
    addSolid(descriptor, parameters.solids[part.index].solid, outputs, bodies, referenceBodies);
}

} // namespace voelund::features::detail
