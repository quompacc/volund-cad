#include "McpDetail.hpp"

#include <QJsonArray>

#include <algorithm>
#include <stdexcept>

namespace voelund::mcp {
namespace {
using application::Action;
using application::CadSession;
using detail::JsonReader;
using detail::readQuantity;
using document::FeatureId;
using document::ParameterId;

std::optional<ParameterId> optionalParameter(const JsonReader& reader) {
    const auto id = reader.optionalId("parameterId");
    return id ? std::optional{ParameterId{*id}} : std::nullopt;
}
FeatureId feature(const JsonReader& reader, const QString& key) { return FeatureId{reader.id(key)}; }

std::vector<document::SketchRegionId> regions(const JsonReader& reader) {
    std::vector<document::SketchRegionId> result;
    if (!reader.has("regions"))
        return result;
    const auto values = reader.array("regions");
    for (qsizetype index = 0; index < values.size(); ++index) {
        const auto field = reader.field("regions") + QStringLiteral("[%1]").arg(index);
        if (!values.at(index).isString())
            throw ArgumentError(field, "Expected a region ID from get_context");
        auto region = document::SketchRegionId::parse(values.at(index).toString().toStdString());
        if (!region)
            throw ArgumentError(field, "Not a region ID");
        result.push_back(std::move(*region));
    }
    return result;
}

document::SketchParameters newSketch(const JsonReader& reader, const CadSession& session) {
    static_cast<void>(reader.object("sketch")); // Type check with the field path.
    return decodeSketch(reader.value("sketch").toObject(), reader.field("sketch"),
                        session.parameterTable());
}

// The edited sketch keeps plane, base plane and support binding; only content changes.
document::SketchParameters editedSketch(const JsonReader& reader, const CadSession& session,
                                        FeatureId id) {
    const auto features = session.features();
    const auto found = std::ranges::find(features, id, &document::FeatureDescriptor::id);
    document::SketchParameters sketch;
    if (found != features.end() && found->kind == document::FeatureKind::sketch)
        sketch = session.sketchParameters(id);
    else // The action layer rejects the target; the content still has to be well-formed.
        sketch = {document::datumSketchPlane(document::DatumPlane::xy), {}};
    const auto content = reader.object("sketch");
    if (content.has("plane"))
        throw ArgumentError(content.field("plane"), "edit_sketch keeps the sketch's plane");
    detail::decodeSketchContent(content, sketch, session.parameterTable());
    return sketch;
}

constexpr const char* operationNames[] = {"newBody", "join", "cut", "intersect"};
constexpr const char* directionNames[] = {"normal", "reversed", "symmetric"};
constexpr const char* extrusionLengths[] = {"distance", "offset"};
// T02/T03: optional operation, direction, target body, start offset and end face.
application::ExtrusionMode extrusionMode(const JsonReader& reader, const CadSession& session) {
    application::ExtrusionMode mode;
    if (reader.has("operation"))
        mode.operation =
            static_cast<document::ExtrusionOperation>(reader.choice("operation", operationNames) + 1);
    const std::size_t direction =
        reader.has("direction") ? reader.choice("direction", directionNames) : 0;
    mode.reversed = direction == 1;
    mode.symmetric = direction == 2; // M02: the distance is the total length
    if (const auto target = reader.optionalId("targetBodyId"))
        mode.target = document::BodyId{*target};
    if (reader.has("offset"))
        mode.offset = detail::readQuantity(reader, "offset", "mm");
    if (reader.has("upToFace")) {
        const auto end = reader.object("upToFace");
        const auto support = FeatureId{end.id("supportId")};
        const auto face = detail::decodeFaceOrigin(end.object("face"));
        try {
            mode.upToFace = session.faceReference(support, face);
        } catch (const std::invalid_argument&) {
            throw ArgumentError(end.field("face"), "Not a current planar face of this support; "
                                                   "see bodies[].faces in get_context");
        }
    }
    return mode;
}

using Decoder = Action (*)(const JsonReader&, const CadSession&);
struct ToolDecoder final {
    const char* name;
    Decoder decode;
};

constexpr const char* parameterUnits[] = {"length", "angle"};
constexpr const char* cylinderModes[] = {"join", "cut", "intersect", "separate"};

const ToolDecoder decoders[] = {
    {"create_parameter",
     [](const JsonReader& r, const CadSession&) -> Action {
         return application::CreateParameterAction{
             r.string("name").toStdString(),
             static_cast<document::ParameterUnit>(r.choice("unit", parameterUnits) + 1),
             readQuantity(r, "value"),
             r.has("comment") ? r.string("comment").toStdString() : std::string{}};
     }},
    {"set_parameter_value",
     [](const JsonReader& r, const CadSession&) -> Action {
         return application::SetParameterValueAction{ParameterId{r.id("parameterId")},
                                                     readQuantity(r, "value")};
     }},
    {"rename_parameter",
     [](const JsonReader& r, const CadSession&) -> Action {
         return application::RenameParameterAction{ParameterId{r.id("parameterId")},
                                                   r.string("name").toStdString()};
     }},
    {"bind_sketch_dimension",
     [](const JsonReader& r, const CadSession&) -> Action {
         return application::BindSketchDimensionAction{
             feature(r, "sketchId"), document::SketchConstraintId{r.id("constraintId")},
             optionalParameter(r)};
     }},
    {"bind_extrusion_distance",
     [](const JsonReader& r, const CadSession&) -> Action {
         return application::BindExtrusionDistanceAction{
             feature(r, "extrusionId"), optionalParameter(r),
             r.has("length") && r.choice("length", extrusionLengths) == 1};
     }},
    {"create_sketch",
     [](const JsonReader& r, const CadSession& s) -> Action {
         return application::CreateDatumSketchAction{newSketch(r, s)};
     }},
    {"push_pull_face",
     [](const JsonReader& r, const CadSession&) -> Action {
         return application::PushPullFaceAction{feature(r, "supportId"),
                                                detail::decodeFaceOrigin(r.object("face")),
                                                readQuantity(r, "distance")};
     }},
    {"create_sketch_on_face",
     [](const JsonReader& r, const CadSession& s) -> Action {
         const auto support = feature(r, "supportId");
         const auto face = detail::decodeFaceOrigin(r.object("face"));
         document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xy),
                                           {}};
         // Faces come from a body's latest result only: an extrusion on an earlier one could
         // not join the body. An unknown support is the action layer's rejection.
         const auto bodies = s.validBodies();
         if (s.featureShape(support) &&
             std::ranges::find(bodies, support, &features::EvaluatedBody::sourceFeature) ==
                 bodies.end())
             throw ArgumentError(r.field("supportId"),
                                 "Use the supportId of a body from get_context");
         if (!s.supportFaces(support).empty()) {
             try {
                 sketch = s.sketchOnFace(support, face);
             } catch (const std::invalid_argument&) {
                 throw ArgumentError(r.field("face"), "Not a current planar face of this "
                                                      "support; see bodies[].faces in "
                                                      "get_context");
             }
         }
         const auto content = r.object("sketch");
         if (content.has("plane"))
             throw ArgumentError(content.field("plane"), "The face gives the sketch's plane");
         detail::decodeSketchContent(content, sketch, s.parameterTable());
         return application::CreateSketchAction{support, std::move(sketch)};
     }},
    {"edit_sketch",
     [](const JsonReader& r, const CadSession& s) -> Action {
         const auto id = feature(r, "sketchId");
         return application::EditSketchAction{id, editedSketch(r, s, id)};
     }},
    {"create_profile_extrusion",
     [](const JsonReader& r, const CadSession& s) -> Action {
         return application::CreateProfileExtrusionAction{feature(r, "sketchId"), regions(r),
                                                           readQuantity(r, "distance"),
                                                           extrusionMode(r, s)};
     }},
    {"create_sketch_extrusion",
     [](const JsonReader& r, const CadSession& s) -> Action {
         return application::CreateSketchExtrusionAction{newSketch(r, s), regions(r),
                                                          readQuantity(r, "distance"),
                                                          extrusionMode(r, s)};
     }},
    {"set_extrusion_distance",
     [](const JsonReader& r, const CadSession&) -> Action {
         return application::SetExtrusionDistanceAction{feature(r, "extrusionId"),
                                                        readQuantity(r, "distance")};
     }},
    {"create_box",
     [](const JsonReader& r, const CadSession&) -> Action {
         return application::CreateBoxAction{readQuantity(r, "width"), readQuantity(r, "depth"),
                                             readQuantity(r, "height")};
     }},
    {"set_box_dimensions",
     [](const JsonReader& r, const CadSession&) -> Action {
         return application::SetBoxDimensionsAction{feature(r, "boxId"), readQuantity(r, "width"),
                                                    readQuantity(r, "depth"),
                                                    readQuantity(r, "height")};
     }},
    {"create_cylinder",
     [](const JsonReader& r, const CadSession&) -> Action {
         return application::CreateCylinderAction{
             r.has("inputId") ? std::optional{feature(r, "inputId")} : std::nullopt,
             static_cast<document::CylinderResultMode>(r.choice("mode", cylinderModes))};
     }},
    {"set_cylinder_size",
     [](const JsonReader& r, const CadSession&) -> Action {
         return application::SetCylinderSizeAction{
             feature(r, "cylinderId"), readQuantity(r, "radius"), readQuantity(r, "height")};
     }},
    {"set_placement",
     [](const JsonReader& r, const CadSession&) -> Action {
         return application::SetPlacementAction{
             feature(r, "featureId"),     readQuantity(r, "x"),
             readQuantity(r, "y"),        readQuantity(r, "z"),
             readQuantity(r, "rotationX"), readQuantity(r, "rotationY"),
             readQuantity(r, "rotationZ")};
     }},
};
} // namespace

namespace detail {
bool isActionTool(const QString& name) {
    return std::ranges::any_of(decoders, [&](const ToolDecoder& decoder) {
        return name == QLatin1String(decoder.name);
    });
}
} // namespace detail

DecodedCall decodeActionCall(const QString& tool, const QJsonObject& arguments,
                             const application::CadSession& session) {
    const auto decoder = std::ranges::find_if(decoders, [&](const ToolDecoder& candidate) {
        return tool == QLatin1String(candidate.name);
    });
    if (decoder == std::end(decoders))
        throw std::invalid_argument("Unknown action tool: " + tool.toStdString());
    const JsonReader reader(arguments, {});
    const auto stamp = reader.object("stamp");
    document::DocumentId document{1, 0};
    try {
        document = document::documentIdFromHex(reader.string("documentId").toStdString());
    } catch (const std::logic_error&) {
        throw ArgumentError(reader.field("documentId"), "Expected the documentId of get_context");
    }
    return {{reader.string("requestId").toStdString(), document,
             application::DocumentStamp{stamp.id("generation"), stamp.id("revision")},
             decoder->decode(reader, session), std::nullopt},
            reader.boolean("preview", false)};
}

} // namespace voelund::mcp
