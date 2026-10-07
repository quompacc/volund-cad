#include <voelund/mcp/ActionJson.hpp>

#include <QJsonArray>
#include <QStringList>

namespace voelund::mcp {
namespace {

QJsonObject typed(const char* type, const QString& description = {}) {
    QJsonObject schema{{"type", type}};
    if (!description.isEmpty())
        schema.insert("description", description);
    return schema;
}
QJsonObject object(const QJsonObject& properties, const QStringList& required,
                   const QString& description = {}) {
    QJsonObject schema{{"type", "object"},
                       {"properties", properties},
                       {"required", QJsonArray::fromStringList(required)},
                       {"additionalProperties", false}};
    if (!description.isEmpty())
        schema.insert("description", description);
    return schema;
}
QJsonObject choice(const QStringList& values, const QString& description = {}) {
    auto schema = typed("string", description);
    schema.insert("enum", QJsonArray::fromStringList(values));
    return schema;
}
QJsonObject quantity(const QStringList& units, const QString& description) {
    return object({{"value", typed("number")}, {"unit", choice(units)}}, {"value", "unit"},
                  description);
}
QJsonObject length(const QString& description) { return quantity({"mm"}, description + " (mm)"); }
QJsonObject featureId(const QString& description) {
    return typed("integer", description + " (feature id from get_context)");
}
QJsonObject point(const QString& description) {
    QJsonObject schema = typed("array", description + " [x, y] in mm");
    schema.insert("items", typed("number"));
    schema.insert("minItems", 2);
    schema.insert("maxItems", 2);
    return schema;
}

QJsonObject sketchSchema(bool withPlane) {
    const QJsonObject element = object(
        {{"id", typed("integer", "Unique element id within the sketch, starting at 1")},
         {"type", choice({"line", "rectangle", "circle", "arc"})},
         {"construction", typed("boolean", "Construction geometry is not part of regions")},
         {"start", point("line start")},
         {"end", point("line end")},
         {"first", point("rectangle corner")},
         {"second", point("opposite rectangle corner")},
         {"center", point("circle or arc center")},
         {"radius", typed("number", "circle or arc radius in mm")},
         {"startAngle", typed("number", "arc start angle in rad, counter-clockwise")},
         {"endAngle", typed("number", "arc end angle in rad")}},
        {"id", "type"});
    const QJsonObject reference =
        object({{"element", typed("integer")},
                {"point", choice({"start", "end", "center"},
                                 "Point of the element; lines use start/end, circles center")}},
               {"element"});
    const QJsonObject constraint = object(
        {{"id", typed("integer", "Unique constraint id within the sketch, starting at 1")},
         {"kind", choice({"horizontal", "vertical", "coincident", "parallel", "perpendicular",
                          "equal", "concentric", "distance", "radius", "diameter",
                          "pointDistance", "pointDistanceX", "pointDistanceY", "coordinateX",
                          "coordinateY", "angle"})},
         {"first", reference},
         {"second", reference},
         {"value", quantity({"mm", "rad"},
                            "Value of a dimension or coordinate: mm for lengths and "
                            "coordinates, rad for angles. Optional when parameterId is set")},
         {"parameterId", typed("integer", "Bind this dimension to a document parameter "
                                          "(not coordinates)")}},
        {"id", "kind", "first"});
    QJsonObject properties{
        {"elements", QJsonObject{{"type", "array"}, {"items", element}}},
        {"constraints", QJsonObject{{"type", "array"}, {"items", constraint}}}};
    QStringList required{"elements"};
    if (withPlane) {
        properties.insert("plane", choice({"xy", "xz", "yz"}, "Base plane of the sketch"));
        required.prepend("plane");
    }
    return object(properties, required,
                  "Sketch in plane coordinates (mm). With constraints, the geometry is solved "
                  "before it is stored; approximate coordinates are fine. Closed contours form "
                  "regions; a contour inside another one makes a hole.");
}

// T01/T04: a face reference as get_context gives it.
QJsonObject faceSchema() {
    return object(
        {{"feature", typed("integer", "Feature that produced the face")},
         {"role", choice({"-x", "+x", "-y", "+y", "-z", "+z", "start", "end"},
                         "Box side in the box's own axes, or cylinder end")},
         {"boundary", QJsonObject{{"type", "array"},
                                  {"items", typed("integer")},
                                  {"description", "Sketch elements bounding an extrusion's end "
                                                  "face"}}},
         {"cap", choice({"end"}, "End face of a push/pull")}},
        {"feature"}, "A face reference copied from bodies[].faces[].face");
}

// T02: what a new extrusion does; all optional.
void insertExtrusionMode(QJsonObject& properties) {
    properties.insert(
        "operation",
        choice({"newBody", "join", "cut"},
               "newBody, join or cut. Default: a base-plane sketch makes a new body, a face "
               "sketch joins its body. A join must touch the body, a cut must remove material"));
    properties.insert("direction",
                      choice({"normal", "reversed"},
                             "Along the sketch normal (default) or against it, e.g. a cut "
                             "from a face into its body"));
    properties.insert("targetBodyId",
                      typed("integer", "Join or cut of a base-plane sketch: the body id from "
                                       "get_context. A face sketch always works on its body"));
    properties.insert("offset", length("Start offset along the extrusion direction, may be "
                                       "negative; default 0"));
    properties.insert(
        "upToFace",
        object({{"supportId", featureId("supportId of the face's body in get_context")},
                {"face", typed("object", "A face reference from bodies[].faces[].face")}},
               {"supportId", "face"},
               "End on this planar face, parallel to the sketch, instead of after distance; the "
               "extrusion follows the face when it moves"));
}

// Every mutating tool: the request envelope plus its own fields.
ToolDefinition action(const char* name, const QString& description, QJsonObject properties,
                      QStringList required) {
    properties.insert("requestId",
                      typed("string", "Unique per intended change. Retrying with the same id "
                                      "and content returns the first result without a second "
                                      "change; other content under the same id is rejected"));
    properties.insert("documentId", typed("string", "documentId from get_context"));
    properties.insert(
        "stamp",
        object({{"generation", typed("integer")}, {"revision", typed("integer")}},
               {"generation", "revision"},
               "stamp from get_context; a request on an older stamp is rejected (staleStamp) "
               "and needs a fresh get_context"));
    properties.insert("preview", typed("boolean", "true: evaluate on a copy and report the "
                                                  "result without changing the document"));
    required.append({"requestId", "documentId", "stamp"});
    return {name,
            description + " Every executed change is one undo step in Völund.",
            object(properties, required)};
}

std::vector<ToolDefinition> makeCatalog() {
    std::vector<ToolDefinition> tools;
    tools.push_back(
        {"get_context",
         "Current Völund document: documentId and stamp for requests, units, features with "
         "their parameters (sketches in the form create_sketch accepts, regions with ids and "
         "areas), bodies with the planar faces a sketch can be placed on, document parameters "
         "and the tools available per target. Call it first and again after a staleStamp "
         "rejection.",
         object({}, {})});
    tools.push_back(action(
        "create_parameter", "Create a named document parameter (length in mm, angle in deg).",
        {{"name", typed("string", "Letter first, then letters, digits or _; unique")},
         {"unit", choice({"length", "angle"})},
         {"value", quantity({"mm", "deg"}, "mm for length, deg for angle")},
         {"comment", typed("string")}},
        {"name", "unit", "value"}));
    tools.push_back(action("set_parameter_value",
                           "Change a parameter; every bound dimension and distance follows.",
                           {{"parameterId", typed("integer")},
                            {"value", quantity({"mm", "deg"}, "unit of the parameter")}},
                           {"parameterId", "value"}));
    tools.push_back(action("rename_parameter", "Rename a parameter; bindings stay.",
                           {{"parameterId", typed("integer")}, {"name", typed("string")}},
                           {"parameterId", "name"}));
    tools.push_back(action(
        "bind_sketch_dimension",
        "Bind a sketch dimension to a parameter, or unbind it (parameterId omitted).",
        {{"sketchId", featureId("sketch")},
         {"constraintId", typed("integer", "Dimension constraint id within the sketch")},
         {"parameterId", typed("integer")}},
        {"sketchId", "constraintId"}));
    tools.push_back(action("bind_extrusion_distance",
                           "Bind an extrusion distance (or its start offset) to a length "
                           "parameter, or unbind it.",
                           {{"extrusionId", featureId("extrusion")},
                            {"parameterId", typed("integer")},
                            {"length", choice({"distance", "offset"}, "Default distance")}},
                           {"extrusionId"}));
    tools.push_back(action("create_sketch", "Create a sketch on a base plane; opens a new body.",
                           {{"sketch", sketchSchema(true)}}, {"sketch"}));
    tools.push_back(action(
        "create_sketch_on_face",
        "Create a sketch on a planar face of a body (e.g. a rib on top of a plate). An "
        "extrusion of it joins that body. Sketch coordinates are in the face's frame from "
        "get_context: world point = origin + x * xDirection + y * yDirection; min/max give "
        "the face extent in these coordinates.",
        {{"supportId", featureId("supportId of the body in get_context")},
         {"face", faceSchema()},
         {"sketch", sketchSchema(false)}},
        {"supportId", "face", "sketch"}));
    tools.push_back(action(
        "push_pull_face",
        "Push or pull a planar face of a body, as one extrusion of that face: a positive "
        "distance pulls it out (joins), a negative one pushes it into the body (cuts). The "
        "extrusion follows the face when the body changes, and its end face can be pushed "
        "or pulled again.",
        {{"supportId", featureId("supportId of the body in get_context")},
         {"face", faceSchema()},
         {"distance", length("Along the face's outward normal; negative pushes into the body")}},
        {"supportId", "face", "distance"}));
    tools.push_back(action("edit_sketch",
                           "Replace elements and constraints of a sketch; its plane stays.",
                           {{"sketchId", featureId("sketch")}, {"sketch", sketchSchema(false)}},
                           {"sketchId", "sketch"}));
    QJsonObject profileExtrusion{
        {"sketchId", featureId("sketch")},
        {"regions",
         QJsonObject{{"type", "array"}, {"items", typed("string", "region id from get_context")}}},
        {"distance", length("extrusion distance")}};
    insertExtrusionMode(profileExtrusion);
    tools.push_back(action("create_profile_extrusion",
                           "Extrude regions of an existing sketch: a new body, joined to a body "
                           "or cut from it.",
                           profileExtrusion, {"sketchId", "regions", "distance"}));
    QJsonObject sketchExtrusion{{"sketch", sketchSchema(true)},
                                {"regions", QJsonObject{{"type", "array"}, {"items", typed("string")}}},
                                {"distance", length("extrusion distance")}};
    insertExtrusionMode(sketchExtrusion);
    tools.push_back(action(
        "create_sketch_extrusion",
        "Create a base-plane sketch and extrude it in one step (one undo step). Without "
        "regions, nested contours alternate as in a drawing: the outer contour is filled and "
        "a contour inside it makes a hole (e.g. a plate with bores).",
        sketchExtrusion, {"sketch", "distance"}));
    tools.push_back(action("set_extrusion_distance",
                           "Change an unbound extrusion distance (bound ones follow their "
                           "parameter).",
                           {{"extrusionId", featureId("extrusion")},
                            {"distance", length("distance")}},
                           {"extrusionId", "distance"}));
    tools.push_back(action("create_box", "Create a box as a new body.",
                           {{"width", length("width")},
                            {"depth", length("depth")},
                            {"height", length("height")}},
                           {"width", "depth", "height"}));
    tools.push_back(action("set_box_dimensions", "Change the dimensions of a box.",
                           {{"boxId", featureId("box")},
                            {"width", length("width")},
                            {"depth", length("depth")},
                            {"height", length("height")}},
                           {"boxId", "width", "depth", "height"}));
    tools.push_back(action("create_cylinder",
                           "Add a cylinder on the result of a feature: joined, cut, "
                           "intersected or as a separate body.",
                           {{"inputId", featureId("input")},
                            {"mode", choice({"join", "cut", "intersect", "separate"})}},
                           {"inputId", "mode"}));
    tools.push_back(action("set_cylinder_size", "Change radius and height of a cylinder.",
                           {{"cylinderId", featureId("cylinder")},
                            {"radius", length("radius")},
                            {"height", length("height")}},
                           {"cylinderId", "radius", "height"}));
    tools.push_back(action(
        "set_placement",
        "Set the absolute placement of the body that places a feature's result.",
        {{"featureId", featureId("feature")},
         {"x", length("x")},
         {"y", length("y")},
         {"z", length("z")},
         {"rotationX", quantity({"rad"}, "rotation about x (rad)")},
         {"rotationY", quantity({"rad"}, "rotation about y (rad)")},
         {"rotationZ", quantity({"rad"}, "rotation about z (rad)")}},
        {"featureId", "x", "y", "z", "rotationX", "rotationY", "rotationZ"}));
    return tools;
}
} // namespace

const std::vector<ToolDefinition>& toolCatalog() {
    static const std::vector<ToolDefinition> catalog = makeCatalog();
    return catalog;
}

} // namespace voelund::mcp
