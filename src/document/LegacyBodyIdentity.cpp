// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/document/LegacyBodyIdentity.hpp>

#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace voelund::document {
namespace {

// A cylinder or M27 primitive that makes a body of its own.
bool separatePrimitive(const FeatureDescriptor& feature, const FeatureParameters& parameters) {
    if (feature.kind == FeatureKind::cylinder)
        return std::get<CylinderParameters>(parameters).resultMode == CylinderResultMode::separate;
    return feature.kind == FeatureKind::primitive &&
           std::get<PrimitiveParameters>(parameters).resultMode == CylinderResultMode::separate;
}

void validateKind(const FeatureDescriptor& feature, const FeatureParameters& parameters) {
    bool matches = false;
    switch (feature.kind) {
    case FeatureKind::box:
        matches = std::holds_alternative<BoxParameters>(parameters);
        break;
    case FeatureKind::cylinder:
        matches = std::holds_alternative<CylinderParameters>(parameters);
        if (matches)
            switch (std::get<CylinderParameters>(parameters).resultMode) {
            case CylinderResultMode::join:
            case CylinderResultMode::cut:
            case CylinderResultMode::intersect:
            case CylinderResultMode::separate:
                break;
            default:
                matches = false;
            }
        break;
    case FeatureKind::sketch:
        matches = std::holds_alternative<SketchParameters>(parameters);
        break;
    case FeatureKind::extrusion:
        matches = std::holds_alternative<ExtrusionParameters>(parameters);
        break;
    case FeatureKind::fillet:
        matches = std::holds_alternative<FilletParameters>(parameters);
        break;
    case FeatureKind::chamfer:
        matches = std::holds_alternative<ChamferParameters>(parameters);
        break;
    case FeatureKind::hole:
        matches = std::holds_alternative<HoleParameters>(parameters);
        break;
    case FeatureKind::combine:
        matches = std::holds_alternative<CombineParameters>(parameters);
        break;
    case FeatureKind::datumPlane:
        matches = std::holds_alternative<DatumPlaneParameters>(parameters);
        break;
    case FeatureKind::datumAxis:
        matches = std::holds_alternative<DatumAxisParameters>(parameters);
        break;
    case FeatureKind::shell:
        matches = std::holds_alternative<ShellParameters>(parameters);
        break;
    case FeatureKind::primitive: // M27
        matches = std::holds_alternative<PrimitiveParameters>(parameters);
        break;
    case FeatureKind::draft: // M25
        matches = std::holds_alternative<DraftParameters>(parameters);
        break;
    case FeatureKind::mirror: // M09
        matches = std::holds_alternative<MirrorParameters>(parameters);
        break;
    case FeatureKind::pattern: // M10
        matches = std::holds_alternative<PatternParameters>(parameters);
        break;
    case FeatureKind::patternCopy:
        matches = std::holds_alternative<PatternCopyParameters>(parameters);
        break;
    case FeatureKind::move: // M11
        matches = std::holds_alternative<MoveParameters>(parameters);
        break;
    case FeatureKind::stepImport: // M17
        matches = std::holds_alternative<ImportParameters>(parameters);
        break;
    case FeatureKind::importPart:
        matches = std::holds_alternative<ImportPartParameters>(parameters);
        break;
    }
    if (!matches)
        throw std::invalid_argument("Legacy body identity: feature parameter kind mismatch");
    // Boxes, base-plane (datum) sketches, face extrusions (T04) and fillets (M05) stand
    // alone; every other feature has an input.
    const bool standalone =
        feature.kind == FeatureKind::box || feature.kind == FeatureKind::fillet ||
        feature.kind == FeatureKind::chamfer || feature.kind == FeatureKind::hole ||
        feature.kind == FeatureKind::combine || feature.kind == FeatureKind::datumPlane ||
        feature.kind == FeatureKind::datumAxis || feature.kind == FeatureKind::shell ||
        feature.kind == FeatureKind::draft || feature.kind == FeatureKind::mirror ||
        feature.kind == FeatureKind::pattern || feature.kind == FeatureKind::patternCopy ||
        feature.kind == FeatureKind::move || feature.kind == FeatureKind::stepImport ||
        feature.kind == FeatureKind::importPart ||
        (feature.kind == FeatureKind::sketch &&
         (std::get<SketchParameters>(parameters).datum ||
          std::get<SketchParameters>(parameters).constructionPlane)) ||
        (feature.kind == FeatureKind::extrusion &&
         std::get<ExtrusionParameters>(parameters).faceProfile);
    // M01: a separate cylinder may also stand alone, as a new body at the origin.
    // M27: so may a separate primitive.
    const bool optionalInput = separatePrimitive(feature, parameters);
    if (!optionalInput && standalone == feature.inputFeature.has_value())
        throw std::invalid_argument("Legacy body identity: invalid input presence");
}

} // namespace

std::vector<std::optional<BodyId>>
featureBodyContexts(std::span<const FeatureDescriptor> features,
                    std::span<const FeatureParameters> parameters) {
    if (features.size() != parameters.size())
        throw std::invalid_argument("Legacy body identity: parameter count mismatch");
    std::map<std::uint64_t, std::size_t> indices;
    for (std::size_t index = 0; index < features.size(); ++index) {
        const auto& feature = features[index];
        if (!hasIdCapacity(feature.id.value(), 1) ||
            !indices.emplace(feature.id.value(), index).second)
            throw std::invalid_argument("Legacy body identity: invalid or duplicate feature ID");
        validateKind(feature, parameters[index]);
    }
    std::vector<std::optional<std::size_t>> inputs(features.size());
    for (std::size_t index = 0; index < features.size(); ++index) {
        if (!features[index].inputFeature)
            continue;
        const auto input = indices.find(features[index].inputFeature->value());
        if (input == indices.end())
            throw std::invalid_argument("Legacy body identity: missing input feature");
        inputs[index] = input->second;
        const bool sketchInput = features[input->second].kind == FeatureKind::sketch;
        if ((features[index].kind == FeatureKind::extrusion) != sketchInput)
            throw std::invalid_argument("Legacy body identity: incompatible input kind");
    }
    const auto extrusion = [&](std::size_t index) -> const ExtrusionParameters* {
        return features[index].kind == FeatureKind::extrusion
                   ? &std::get<ExtrusionParameters>(parameters[index])
                   : nullptr;
    };
    // A base-plane sketch opens a body; T02: so does a new-body prism of a face sketch.
    const auto createsBody = [&](std::size_t index) {
        const auto& feature = features[index];
        if (const auto* prism = extrusion(index))
            return !prism->faceProfile && inputs[*inputs[index]].has_value() &&
                   effectiveOperation(*prism, false) == ExtrusionOperation::newBody;
        // M09: so does a mirror image of its own; M10: and every copy of a pattern.
        if (const auto* mirror = std::get_if<MirrorParameters>(&parameters[index]))
            return mirror->mode == MirrorMode::newBody;
        if (feature.kind == FeatureKind::patternCopy)
            return true;
        // M17: every imported solid is a body.
        if (feature.kind == FeatureKind::stepImport || feature.kind == FeatureKind::importPart)
            return true;
        return feature.kind == FeatureKind::box ||
               (feature.kind == FeatureKind::sketch && !inputs[index]) ||
               separatePrimitive(feature, parameters[index]);
    };
    // T02: a join or cut of a base-plane sketch names a body that some feature creates;
    // T03: so does the face an extrusion ends on.
    const auto isBody = [&](BodyId body) {
        const auto creator = indices.find(body.value());
        return creator != indices.end() && createsBody(creator->second);
    };
    for (std::size_t index = 0; index < features.size(); ++index) {
        const auto* prism = extrusion(index);
        if (prism == nullptr)
            continue;
        if (prism->faceProfile && !isBody(prism->faceProfile->body))
            throw std::invalid_argument("Extrusion profile face is not on a body of this document");
        if (prism->target &&
            (prism->faceProfile || inputs[*inputs[index]] || !isBody(*prism->target)))
            throw std::invalid_argument("Extrusion target is not a body of this document");
        if (prism->upToFace && !isBody(prism->upToFace->body))
            throw std::invalid_argument("Extrusion end face is not on a body of this document");
    }
    // M05/M07: a fillet or chamfer treats edges of a body some feature creates.
    const auto treatedBody = [&](std::size_t index) -> std::optional<BodyId> {
        if (features[index].kind == FeatureKind::fillet)
            return std::get<FilletParameters>(parameters[index]).body;
        if (features[index].kind == FeatureKind::chamfer)
            return std::get<ChamferParameters>(parameters[index]).body;
        // M08: a hole cuts the body of its face.
        if (features[index].kind == FeatureKind::hole)
            return std::get<HoleParameters>(parameters[index]).face.body;
        // M21: a combination's result belongs to its target body.
        if (features[index].kind == FeatureKind::combine)
            return std::get<CombineParameters>(parameters[index]).target;
        // M22: a shell hollows its body.
        if (features[index].kind == FeatureKind::shell)
            return std::get<ShellParameters>(parameters[index]).body;
        // M25: a draft tilts faces of its body.
        if (features[index].kind == FeatureKind::draft)
            return std::get<DraftParameters>(parameters[index]).body;
        // M11: a move moves its body.
        if (const auto* move = std::get_if<MoveParameters>(&parameters[index]))
            return move->body;
        // M10: a pattern passes its body on, or joins the copies into it.
        if (const auto* pattern = std::get_if<PatternParameters>(&parameters[index]))
            return pattern->body;
        // M09: a mirror joined to its body changes that body.
        if (const auto* mirror = std::get_if<MirrorParameters>(&parameters[index]);
            mirror && mirror->mode == MirrorMode::join)
            return mirror->body;
        return std::nullopt;
    };
    for (std::size_t index = 0; index < features.size(); ++index)
        if (const auto body = treatedBody(index); body && !isBody(*body))
            throw std::invalid_argument("Fillet or chamfer body is not a body of this document");
    // M19: construction features refer to bodies of the document; a sketch names an earlier
    // construction plane.
    for (std::size_t index = 0; index < features.size(); ++index) {
        const auto* plane = std::get_if<DatumPlaneParameters>(&parameters[index]);
        const auto* axis = std::get_if<DatumAxisParameters>(&parameters[index]);
        if ((plane && plane->face && !isBody(plane->face->body)) ||
            (axis && axis->body && !isBody(*axis->body)))
            throw std::invalid_argument("Construction reference is not a body of this document");
        // M20: a revolve names an earlier construction axis.
        const auto* prism = std::get_if<ExtrusionParameters>(&parameters[index]);
        if (prism && prism->revolve && prism->revolve->axis.datumAxis) {
            const auto found = indices.find(prism->revolve->axis.datumAxis->value());
            if (found == indices.end() || found->second >= index ||
                features[found->second].kind != FeatureKind::datumAxis)
                throw std::invalid_argument("Revolve axis is not an earlier construction axis");
        }
        // M23: a sweep follows a path in an earlier sketch.
        if (prism && prism->sweep) {
            const auto found = indices.find(prism->sweep->sketch.value());
            if (found == indices.end() || found->second >= index ||
                features[found->second].kind != FeatureKind::sketch)
                throw std::invalid_argument("Sweep path is not an earlier sketch");
        }
        // M24: loft sections are earlier sketches.
        if (prism && prism->loft)
            for (const auto& section : prism->loft->sections) {
                const auto found = indices.find(section.sketch.value());
                if (found == indices.end() || found->second >= index ||
                    features[found->second].kind != FeatureKind::sketch)
                    throw std::invalid_argument("Loft section is not an earlier sketch");
            }
        const auto* sketch = std::get_if<SketchParameters>(&parameters[index]);
        // S17: projected edges lie on bodies of the document.
        if (sketch)
            for (const auto& projection : sketch->projections)
                if (!isBody(projection.body))
                    throw std::invalid_argument("Projected edge is not on a body of this document");
        if (sketch && sketch->constructionPlane) {
            const auto found = indices.find(sketch->constructionPlane->value());
            if (found == indices.end() || found->second >= index ||
                features[found->second].kind != FeatureKind::datumPlane)
                throw std::invalid_argument("Sketch construction plane is not an earlier plane");
        }
    }
    // M09: a mirror reads a body of the document, about a face of one or an earlier
    // construction plane.
    for (std::size_t index = 0; index < features.size(); ++index) {
        const auto* mirror = std::get_if<MirrorParameters>(&parameters[index]);
        if (mirror == nullptr)
            continue;
        if (!isBody(mirror->body) || (mirror->plane.face && !isBody(mirror->plane.face->body)))
            throw std::invalid_argument("Mirror body is not a body of this document");
        if (mirror->plane.constructionPlane) {
            const auto found = indices.find(mirror->plane.constructionPlane->value());
            if (found == indices.end() || found->second >= index ||
                features[found->second].kind != FeatureKind::datumPlane)
                throw std::invalid_argument("Mirror plane is not an earlier construction plane");
        }
    }
    // M10: a pattern copies a body of the document along an edge of one or an earlier
    // construction axis; each copy names an earlier new-body pattern and one index of it.
    std::vector<std::pair<std::uint64_t, std::uint32_t>> copies;
    for (std::size_t index = 0; index < features.size(); ++index) {
        if (const auto* pattern = std::get_if<PatternParameters>(&parameters[index])) {
            if (!isBody(pattern->body) || (pattern->axis.body && !isBody(*pattern->axis.body)))
                throw std::invalid_argument("Pattern body is not a body of this document");
            if (pattern->datumAxis) {
                const auto found = indices.find(pattern->datumAxis->value());
                if (found == indices.end() || found->second >= index ||
                    features[found->second].kind != FeatureKind::datumAxis)
                    throw std::invalid_argument("Pattern axis is not an earlier construction axis");
            }
        }
        const auto* copy = std::get_if<PatternCopyParameters>(&parameters[index]);
        if (copy == nullptr)
            continue;
        const auto found = indices.find(copy->pattern.value());
        const auto* pattern = found == indices.end() || found->second >= index
                                  ? nullptr
                                  : std::get_if<PatternParameters>(&parameters[found->second]);
        if (pattern == nullptr || pattern->mode != PatternMode::newBodies || copy->index == 0 ||
            copy->index >= pattern->count ||
            std::ranges::find(copies, std::pair{copy->pattern.value(), copy->index}) !=
                copies.end())
            throw std::invalid_argument("Pattern copy does not belong to an earlier pattern");
        copies.emplace_back(copy->pattern.value(), copy->index);
    }
    // M17: each part names an earlier import and one further solid of it.
    std::vector<std::pair<std::uint64_t, std::uint32_t>> parts;
    for (std::size_t index = 0; index < features.size(); ++index) {
        const auto* part = std::get_if<ImportPartParameters>(&parameters[index]);
        if (part == nullptr)
            continue;
        const auto found = indices.find(part->import.value());
        const auto* import = found == indices.end() || found->second >= index
                                 ? nullptr
                                 : std::get_if<ImportParameters>(&parameters[found->second]);
        if (import == nullptr || part->index == 0 || part->index >= import->solids.size() ||
            std::ranges::find(parts, std::pair{part->import.value(), part->index}) != parts.end())
            throw std::invalid_argument("Import part does not belong to an earlier import");
        parts.emplace_back(part->import.value(), part->index);
    }
    for (std::size_t index = 0; index < features.size(); ++index)
        if (features[index].kind == FeatureKind::combine)
            for (const BodyId tool : std::get<CombineParameters>(parameters[index]).tools)
                if (!isBody(tool))
                    throw std::invalid_argument("Combine tool is not a body of this document");
    // Iterative traversal also handles long legacy chains without recursive stack growth.
    std::vector<unsigned char> state(features.size(), 0);
    std::vector<std::optional<BodyId>> context(features.size());
    std::vector<std::size_t> path;
    for (std::size_t start = 0; start < features.size(); ++start) {
        auto current = start;
        while (state[current] != 2) {
            if (state[current] == 1)
                throw std::invalid_argument("Legacy body identity: dependency cycle");
            state[current] = 1;
            path.push_back(current);
            if (!inputs[current])
                break;
            current = *inputs[current];
        }
        while (!path.empty()) {
            const auto index = path.back();
            path.pop_back();
            const auto& feature = features[index];
            // A datum sketch opens a body; its extrusion produces the body's first solid.
            if (createsBody(index))
                context[index] = BodyId{feature.id.value()};
            // M19: a construction feature belongs to no body; it is stored in a group of its
            // own, under its own id (the shell lists it apart from the bodies).
            else if (feature.kind == FeatureKind::datumPlane ||
                     feature.kind == FeatureKind::datumAxis)
                context[index] = BodyId{feature.id.value()};
            else if (const auto* prism = extrusion(index); prism && prism->faceProfile)
                context[index] = prism->faceProfile->body;
            else if (prism && prism->target)
                context[index] = prism->target;
            else if (const auto body = treatedBody(index))
                context[index] = body;
            else
                context[index] = context[*inputs[index]];
            state[index] = 2;
        }
    }
    // A base-plane sketch whose prisms all work on one named body belongs to that body.
    for (std::size_t index = 0; index < features.size(); ++index) {
        if (features[index].kind != FeatureKind::sketch || inputs[index])
            continue;
        std::optional<BodyId> target;
        bool shared = true;
        for (std::size_t user = 0; user < features.size(); ++user)
            if (inputs[user] == index) {
                const auto named = extrusion(user)->target;
                shared = shared && named && (!target || target == named);
                target = named;
            }
        if (shared && target)
            context[index] = target;
    }
    return context;
}

std::vector<FeatureBodyIdentity>
legacyBodyIdentities(std::span<const FeatureDescriptor> features,
                     std::span<const FeatureParameters> parameters) {
    const auto context = featureBodyContexts(features, parameters);
    std::vector<FeatureBodyIdentity> result;
    result.reserve(features.size());
    for (std::size_t index = 0; index < features.size(); ++index)
        result.push_back({features[index].id, features[index].kind == FeatureKind::sketch
                                                  ? std::nullopt
                                                  : context[index]});
    return result;
}

} // namespace voelund::document
