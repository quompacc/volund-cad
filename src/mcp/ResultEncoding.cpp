// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "McpDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>

#include <QJsonArray>

#include <algorithm>
#include <iterator>
#include <numbers>
#include <variant>

namespace voelund::mcp {
namespace {
using application::ActionKind;

// Parallel to ActionKind; shell-only actions have no tool.
constexpr const char* toolNames[] = {"create_parameter",
                                     "set_parameter_value",
                                     "rename_parameter",
                                     "bind_sketch_dimension",
                                     "bind_extrusion_distance",
                                     "create_sketch",
                                     "edit_sketch",
                                     "create_profile_extrusion",
                                     "set_extrusion_distance",
                                     "create_box",
                                     "set_box_dimensions",
                                     "create_cylinder",
                                     "set_cylinder_size",
                                     "set_placement",
                                     "create_sketch_extrusion",
                                     "create_sketch_on_face",
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     "push_pull_face",
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr, // M27: no tools yet
                                     nullptr,
                                     nullptr, // M25: no tools yet
                                     nullptr,
                                     nullptr, // M09: no tools yet
                                     nullptr,
                                     nullptr, // M10: no tools yet
                                     nullptr,
                                     nullptr, // M11: no tools yet
                                     nullptr, // M14: no tool yet
                                     nullptr, // N12a: no tools yet
                                     nullptr,
                                     nullptr, // N12b: no tool yet
                                     nullptr, // N13a: no tools yet
                                     nullptr,
                                     nullptr, // N13b: no tools yet
                                     nullptr};
static_assert(std::size(toolNames) == std::variant_size_v<application::Action>,
              "One tool name (or none) per action kind");
constexpr const char* statusNames[] = {"rejected", "committed", "committedWithErrors"};
constexpr const char* rejectionNames[] = {
    "none",           "foreignDocument",     "staleStamp",
    "unknownTarget",  "unsupportedTarget",   "invalidUnit",
    "nonFinite",      "unresolvedReference", "missingRequestId",
    "requestConflict"};
constexpr const char* reasonNames[] = {"none",           "cancelled",        "noChange",
                                       "invalidInput",   "evaluationFailed", "incompleteEvaluation",
                                       "alreadyFinished"};
constexpr const char* evaluationNames[] = {
    "valid", "dirty", "failed", "blockedByDependency", "suppressed", "rolledBack"};
constexpr const char* featureKinds[] = {
    "box",        "cylinder", "sketch",     "extrusion",   "fillet",    "chamfer",
    "hole",       "combine",  "datumPlane", "datumAxis",   "shell",     "primitive",
    "draft",      "mirror",   "pattern",    "patternCopy", "move",      "stepImport",
    "importPart", "split",    "splitPart",  "faceEdit",    "datumPoint"};
static_assert(std::size(featureKinds) == std::variant_size_v<document::FeatureParameters>,
              "One name per feature kind");
constexpr const char* primitiveShapes[] = {"sphere", "cone", "torus", "wedge"};
constexpr const char* basePlaneNames[] = {"", "xy", "xz", "yz"};
constexpr const char* baseAxisNames[] = {"", "x", "y", "z"};
constexpr const char* rotationAxisNames[] = {"none", "x", "y"};
// Indexed by CombineOperation (join = 1).
constexpr const char* combineOperations[] = {"", "join", "cut", "intersect"};
constexpr const char* runtimeNames[] = {"current", "error", "blocked"};
constexpr const char* cylinderModes[] = {"join", "cut", "intersect", "separate"};
// Indexed by ExtrusionOperation (newBody = 1).
constexpr const char* operationNames[] = {"", "newBody", "join", "cut", "intersect"};
// Indexed by MirrorMode (newBody = 1).
constexpr const char* mirrorModes[] = {"", "newBody", "join"};
// Indexed by PatternKind and PatternMode (linear and newBodies = 1).
constexpr const char* patternKinds[] = {"", "linear", "circular", "path"};
constexpr const char* patternModes[] = {"", "newBodies", "join"};

template <class Enum, std::size_t N> const char* name(const char* const (&names)[N], Enum value) {
    return names[static_cast<std::size_t>(value)];
}
QJsonValue id(std::uint64_t value) { return static_cast<double>(value); }
template <class Ids> QJsonArray ids(const Ids& values) {
    QJsonArray result;
    for (const auto& value : values)
        result.append(id(value.value()));
    return result;
}
QJsonObject stamp(const application::DocumentStamp& value) {
    return {{"generation", id(value.generation)}, {"revision", id(value.revision)}};
}
QJsonObject placement(const kernel::RigidTransform& value) {
    return {{"x", detail::quantity(value.translationX, "mm")},
            {"y", detail::quantity(value.translationY, "mm")},
            {"z", detail::quantity(value.translationZ, "mm")},
            {"rotationX", detail::quantity(value.rotationX.value(), "rad")},
            {"rotationY", detail::quantity(value.rotationY.value(), "rad")},
            {"rotationZ", detail::quantity(value.rotationZ.value(), "rad")}};
}

constexpr const char* splitKeeps[] = {"", "both", "front", "back"};
QJsonObject splitJson(const document::SplitParameters& split) { // N12a
    return {{"bodyId", id(split.body.value())}, {"keep", name(splitKeeps, split.keep)}};
}
QJsonObject splitJson(const document::SplitPartParameters& part) {
    return {{"splitId", id(part.split.value())}};
}
QJsonObject datumPointJson(const document::DatumPointParameters& point) { // N13b
    return {{"spot", static_cast<int>(point.spot)}, {"onEdge", point.edge.has_value()}};
}
QJsonObject faceEditJson(const document::FaceEditParameters& edit) { // N13a
    return {{"bodyId", id(edit.body.value())},
            {"mode", edit.mode == document::FaceEditMode::offset ? "offset" : "remove"},
            {"faces", static_cast<int>(edit.faces.size())},
            {"distance", detail::quantity(edit.distance, "mm")}};
}

// M10: what a pattern runs along; N11c: or its path sketch.
void insertPatternAxis(QJsonObject& json, const document::PatternParameters& pattern) {
    if (pattern.datumAxis)
        json.insert("constructionAxisId", id(pattern.datumAxis->value()));
    else if (pattern.axis.baseAxis)
        json.insert("baseAxis", name(baseAxisNames, *pattern.axis.baseAxis));
    else if (pattern.axis.body)
        json.insert("edgeBodyId", id(pattern.axis.body->value()));
    else if (pattern.path)
        json.insert("pathSketchId", id(pattern.path->sketch.value()));
}

QJsonObject featureParameters(const document::FeatureParameters& parameters) {
    return std::visit(
        detail::Overloaded{
            [](const document::BoxParameters& box) -> QJsonObject {
                return {{"width", detail::quantity(box.dimensions.width, "mm")},
                        {"depth", detail::quantity(box.dimensions.depth, "mm")},
                        {"height", detail::quantity(box.dimensions.height, "mm")},
                        {"placement", placement(box.placement)}};
            },
            [](const document::CylinderParameters& cylinder) -> QJsonObject {
                return {{"radius", detail::quantity(cylinder.radius, "mm")},
                        {"height", detail::quantity(cylinder.height, "mm")},
                        {"mode", name(cylinderModes, cylinder.resultMode)},
                        {"placement", placement(cylinder.placement)}};
            },
            [](const document::DraftParameters& draft) -> QJsonObject { // M25
                return {{"angle", detail::quantity(draft.angle.value(), "rad")},
                        {"bodyId", id(draft.body.value())},
                        {"faceCount", static_cast<int>(draft.faces.size())}};
            },
            [](const document::MirrorParameters& mirror) -> QJsonObject { // M09
                QJsonObject json{{"bodyId", id(mirror.body.value())},
                                 {"mode", name(mirrorModes, mirror.mode)}};
                if (mirror.plane.basePlane)
                    json.insert("basePlane", name(basePlaneNames, *mirror.plane.basePlane));
                else if (mirror.plane.constructionPlane)
                    json.insert("constructionPlaneId", id(mirror.plane.constructionPlane->value()));
                else
                    json.insert("faceBodyId", id(mirror.plane.face->body.value()));
                return json;
            },
            [](const document::PatternParameters& pattern) -> QJsonObject { // M10
                QJsonObject json{{"bodyId", id(pattern.body.value())},
                                 {"kind", name(patternKinds, pattern.kind)},
                                 {"count", static_cast<int>(pattern.count)},
                                 {"spacing", detail::quantity(pattern.spacing, "mm")},
                                 {"angle", detail::quantity(pattern.angle.value(), "rad")},
                                 {"reversed", pattern.reversed},
                                 {"mode", name(patternModes, pattern.mode)}};
                insertPatternAxis(json, pattern);
                return json;
            },
            [](const document::MoveParameters& move) -> QJsonObject { // M11
                QJsonArray pivot;
                for (const double value : move.pivot)
                    pivot.append(value);
                return {{"bodyId", id(move.body.value())},
                        {"placement", placement(move.transform)},
                        {"pivot", pivot}};
            },
            [](const document::PatternCopyParameters& copy) -> QJsonObject {
                return {{"patternId", id(copy.pattern.value())},
                        {"index", static_cast<int>(copy.index)}};
            },
            [](const document::ImportParameters& import) -> QJsonObject { // M17
                QJsonArray names;
                for (const auto& solid : import.solids)
                    names.append(QString::fromStdString(solid.name));
                return {{"fileName", QString::fromStdString(import.fileName)},
                        {"sourceUnit", QString::fromStdString(import.sourceUnit)},
                        {"solidNames", names}};
            },
            [](const document::ImportPartParameters& part) -> QJsonObject {
                return {{"importId", id(part.import.value())},
                        {"index", static_cast<int>(part.index)}};
            },
            [](const document::SplitParameters& split) { return splitJson(split); }, // N12a
            [](const document::SplitPartParameters& part) { return splitJson(part); },
            [](const document::FaceEditParameters& edit) { return faceEditJson(edit); },
            [](const document::DatumPointParameters& point) { return datumPointJson(point); },
            [](const document::PrimitiveParameters& primitive) -> QJsonObject { // M27
                QJsonArray sizes;
                for (const double size : primitive.sizes)
                    sizes.append(size);
                return {{"shape", name(primitiveShapes, primitive.shape)},
                        {"sizes", sizes},
                        {"mode", name(cylinderModes, primitive.resultMode)},
                        {"placement", placement(primitive.placement)}};
            },
            [](const document::SketchParameters& sketch) -> QJsonObject {
                return {{"sketch", encodeSketch(sketch)}};
            },
            [](const document::ExtrusionParameters& extrusion) -> QJsonObject {
                QJsonArray regions;
                for (const auto& region : extrusion.regions)
                    regions.append(QString::fromStdString(region.text()));
                QJsonObject json{{"distance", detail::quantity(extrusion.distance, "mm")},
                                 {"regions", regions}};
                if (extrusion.distanceParameter)
                    json.insert("distanceParameterId", id(extrusion.distanceParameter->value()));
                if (!extrusion.profiles.empty())
                    json.insert("needsRepair", true); // Unmigrated legacy profiles.
                if (extrusion.revolve) {              // M20: a revolve; its distance is unused
                    const auto& axis = extrusion.revolve->axis;
                    json.insert("revolveAngle",
                                detail::quantity(extrusion.revolve->angle.value(), "rad"));
                    if (axis.sketchLine)
                        json.insert("revolveAxisElementId", id(axis.sketchLine->value()));
                    else if (axis.baseAxis)
                        json.insert("revolveBaseAxis", name(baseAxisNames, *axis.baseAxis));
                    else
                        json.insert("revolveAxisFeatureId", id(axis.datumAxis->value()));
                }
                return json;
            },
            [](const document::FilletParameters& fillet) -> QJsonObject {
                // M05: the faces of each edge are shown with M06's tool; here their count.
                return {{"radius", detail::quantity(fillet.radius, "mm")},
                        {"bodyId", id(fillet.body.value())},
                        {"edgeCount", static_cast<int>(fillet.edges.size())}};
            },
            [](const document::ChamferParameters& chamfer) -> QJsonObject {
                return {{"distance", detail::quantity(chamfer.distance, "mm")},
                        {"bodyId", id(chamfer.body.value())},
                        {"edgeCount", static_cast<int>(chamfer.edges.size())}};
            },
            [](const document::HoleParameters& hole) -> QJsonObject {
                QJsonObject json{{"diameter", detail::quantity(hole.diameter, "mm")},
                                 {"bodyId", id(hole.face.body.value())},
                                 {"x", detail::quantity(hole.x, "mm")},
                                 {"y", detail::quantity(hole.y, "mm")},
                                 {"throughAll", !hole.depth.has_value()}};
                if (hole.depth)
                    json.insert("depth", detail::quantity(*hole.depth, "mm"));
                return json;
            },
            [](const document::CombineParameters& combine) -> QJsonObject {
                QJsonArray tools;
                for (const auto tool : combine.tools)
                    tools.append(id(tool.value()));
                return {{"targetBodyId", id(combine.target.value())},
                        {"toolBodyIds", tools},
                        {"operation", name(combineOperations, combine.operation)},
                        {"keepTools", combine.keepTools}};
            },
            [](const document::DatumPlaneParameters& plane) -> QJsonObject {
                QJsonObject json{{"offset", detail::quantity(plane.offset, "mm")},
                                 {"rotationAxis", name(rotationAxisNames, plane.rotationAxis)},
                                 {"angle", detail::quantity(plane.angle.value(), "rad")}};
                if (plane.basePlane)
                    json.insert("basePlane", name(basePlaneNames, *plane.basePlane));
                else
                    json.insert("bodyId", id(plane.face->body.value()));
                return json;
            },
            [](const document::ShellParameters& shell) -> QJsonObject {
                return {{"thickness", detail::quantity(shell.thickness, "mm")},
                        {"bodyId", id(shell.body.value())},
                        {"openFaceCount", static_cast<int>(shell.openFaces.size())},
                        {"outward", shell.outward}};
            },
            [](const document::DatumAxisParameters& axis) -> QJsonObject {
                if (axis.baseAxis)
                    return {{"baseAxis", name(baseAxisNames, *axis.baseAxis)}};
                return {{"bodyId", id(axis.body->value())}};
            }},
        parameters);
}

// A sketch on a body face: the face it names and its current frame, in which its
// coordinates are given.
void insertSketchFace(QJsonObject& json, const application::CadSession& session,
                      document::FeatureId sketch) {
    auto parameters = session.sketchParameters(sketch);
    if (!parameters.supportBinding)
        return;
    json.insert("face", detail::encodeFaceOrigin(parameters.supportBinding->reference.origin));
    try {
        parameters = session.sketchParametersForEditing(sketch);
    } catch (const std::exception&) {
        return; // The support is not current; the feature state explains why.
    }
    json.insert("frame", detail::encodeSketchFrame(parameters.plane));
}

// T02: the operation in effect, also for extrusions that predate the choice.
// W16: the taper of an extrusion, only when it leans.
void insertTaper(QJsonObject& parameters, const document::ExtrusionParameters& extrusion) {
    if (extrusion.taperAngle != 0.0)
        parameters.insert("taperAngle",
                          detail::quantity(extrusion.taperAngle * std::numbers::pi / 180, "rad"));
}
void insertExtrusionMode(QJsonObject& json, const document::DocumentSnapshot& snapshot,
                         const document::FeatureDescriptor& feature) {
    const auto& extrusion = snapshot.extrusionParameters(feature.id);
    const auto features = snapshot.features();
    // T04: a face extrusion has no sketch; it joins or cuts its face's body.
    const auto sketch = feature.inputFeature ? std::ranges::find(features, *feature.inputFeature,
                                                                 &document::FeatureDescriptor::id)
                                             : features.end();
    const bool baseSketch = sketch != features.end() && !sketch->inputFeature;
    auto parameters = json.value("parameters").toObject();
    parameters.insert("operation",
                      name(operationNames, document::effectiveOperation(extrusion, baseSketch)));
    parameters.insert("direction", extrusion.symmetric  ? "symmetric"
                                   : extrusion.reversed ? "reversed"
                                                        : "normal");
    if (extrusion.target)
        parameters.insert("targetBodyId", id(extrusion.target->value()));
    if (extrusion.mergeSeams)
        parameters.insert("mergeSeams", true); // T05
    parameters.insert("offset", detail::quantity(extrusion.offset, "mm"));
    insertTaper(parameters, extrusion); // W16
    if (extrusion.offsetParameter)
        parameters.insert("offsetParameterId", id(extrusion.offsetParameter->value()));
    if (extrusion.upToFace)
        parameters.insert(
            "upToFace",
            QJsonObject{{"bodyId", id(extrusion.upToFace->body.value())},
                        {"face", detail::encodeFaceOrigin(extrusion.upToFace->origin)}});
    if (extrusion.faceProfile)
        parameters.insert(
            "profileFace",
            QJsonObject{{"bodyId", id(extrusion.faceProfile->body.value())},
                        {"face", detail::encodeFaceOrigin(extrusion.faceProfile->origin)}});
    json.insert("parameters", parameters);
}

QJsonArray toolsFor(const std::vector<ActionKind>& actions) {
    QJsonArray tools;
    for (const auto kind : actions)
        if (const char* tool = name(toolNames, kind))
            tools.append(tool);
    return tools;
}
} // namespace

QJsonObject encodeResult(const application::ActionResult& result, bool preview) {
    const auto& command = result.command;
    QJsonArray issues;
    for (const auto& issue : command.evaluation().features)
        if (issue.status != features::EvaluationStatus::valid)
            issues.append(QJsonObject{{"feature", id(issue.feature.value())},
                                      {"status", name(evaluationNames, issue.status)},
                                      {"code", QString::fromStdString(issue.code)},
                                      {"message", QString::fromStdString(issue.message)}});
    QJsonObject json{{"toolVersion", toolVersion},
                     {"preview", preview},
                     {"status", name(statusNames, command.status())},
                     {"rejection", name(rejectionNames, result.rejection)},
                     {"reason", name(reasonNames, command.reason())},
                     {"diagnostic", QString::fromStdString(command.diagnostic())},
                     {"stamp", stamp(result.stamp)},
                     {"createdFeatures", ids(result.createdFeatures)},
                     {"changedFeatures", ids(result.changedFeatures)},
                     {"createdParameters", ids(result.createdParameters)},
                     {"changedParameters", ids(result.changedParameters)},
                     {"repeated", result.repeated},
                     {"issues", issues}};
    if (command.feature())
        json.insert("feature", id(command.feature()->value()));
    return json;
}

QJsonObject encodeContext(const application::CadSession& session) {
    const auto context = application::describeActionContext(session);
    const auto identified = session.identifiedSnapshot();
    const auto& snapshot = identified.snapshot();
    QJsonObject available{{"document", QJsonArray{}}};
    QJsonObject featureTools;
    QJsonObject parameterTools;
    for (const auto& entry : context.available) {
        const auto tools = toolsFor(entry.actions);
        if (const auto* feature = std::get_if<document::FeatureId>(&entry.target))
            featureTools.insert(QString::number(feature->value()), tools);
        else if (const auto* parameter = std::get_if<document::ParameterId>(&entry.target))
            parameterTools.insert(QString::number(parameter->value()), tools);
        else
            available.insert("document", tools);
    }
    available.insert("features", featureTools);
    available.insert("parameters", parameterTools);

    QJsonArray featureList;
    for (const auto& feature : context.features) {
        QJsonObject json{{"id", id(feature.id.value())},
                         {"kind", name(featureKinds, feature.kind)},
                         {"name", QString::fromStdString(feature.name)},
                         {"parameters", featureParameters(snapshot.featureParameters(feature.id))}};
        if (feature.inputFeature)
            json.insert("inputId", id(feature.inputFeature->value()));
        const auto state = std::ranges::find(context.featureStates, feature.id,
                                             &application::FeatureRuntimeState::id);
        if (state != context.featureStates.end()) {
            json.insert("state", name(runtimeNames, state->status));
            if (!state->message.empty())
                json.insert("message", QString::fromStdString(state->message));
        }
        if (feature.kind == document::FeatureKind::sketch)
            insertSketchFace(json, session, feature.id);
        if (feature.kind == document::FeatureKind::extrusion)
            insertExtrusionMode(json, snapshot, feature);
        const auto regions = std::ranges::find(context.sketchRegions, feature.id,
                                               &application::SketchRegionsSummary::sketch);
        if (regions != context.sketchRegions.end()) {
            QJsonArray list;
            for (const auto& region : regions->regions)
                list.append(QJsonObject{{"id", QString::fromStdString(region.id.text())},
                                        {"area", detail::quantity(region.area, "mm2")},
                                        {"ambiguous", region.ambiguous}});
            json.insert("regions", list);
            json.insert("regionsValid", regions->valid);
        }
        featureList.append(json);
    }
    QJsonArray bodies;
    for (const auto& body : context.outline.bodies) {
        QJsonObject json{{"id", id(body.id.value())}, {"features", ids(body.features)}};
        const auto faces =
            std::ranges::find(context.bodyFaces, body.id, &application::BodyFaces::body);
        if (faces != context.bodyFaces.end()) {
            QJsonArray list;
            for (const auto& face : faces->faces)
                list.append(detail::encodeFace(face));
            json.insert("supportId", id(faces->support.value()));
            json.insert("faces", list);
        }
        bodies.append(json);
    }
    QJsonArray parameters;
    for (const auto& parameter : context.parameters.values)
        parameters.append(QJsonObject{
            {"id", id(parameter.id.value())},
            {"name", QString::fromStdString(parameter.name)},
            {"unit", parameter.unit == document::ParameterUnit::length ? "length" : "angle"},
            {"value",
             detail::quantity(parameter.value,
                              parameter.unit == document::ParameterUnit::length ? "mm" : "deg")},
            {"comment", QString::fromStdString(parameter.comment)}});
    return {{"toolVersion", toolVersion},
            {"documentId", QString::fromStdString(document::documentIdToHex(context.document))},
            {"stamp", stamp(context.stamp)},
            {"units", QJsonObject{{"length", "mm"},
                                  {"sketchCoordinates", "mm"},
                                  {"sketchAngle", "rad"},
                                  {"angleParameter", "deg"}}},
            {"features", featureList},
            {"bodies", bodies},
            {"looseFeatures", ids(context.outline.loose)},
            {"parameters", parameters},
            {"available", available}};
}

} // namespace voelund::mcp
