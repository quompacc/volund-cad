#include "CadSessionTestSupport.hpp"

#include <voelund/mcp/ActionJson.hpp>
#include <voelund/mcp/ToolHost.hpp>

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// AI04a: tool catalog, JSON codec and ToolHost without a transport. A plate with a bore
// bound to a parameter, built only through tool calls; argument errors, action rejections,
// previews and retries never mutate twice.
namespace {
using namespace voelund;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
QJsonObject mm(double value) { return {{"value", value}, {"unit", "mm"}}; }
std::string text(const QJsonObject& object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString();
}

class Client final {
  public:
    explicit Client(application::CadSession& session) : host(session, [this](const auto&) { ++changes; }) {}
    QJsonObject context() { return raw("get_context", {}); }
    // The request envelope on the current stamp.
    QJsonObject call(const QString& tool, QJsonObject arguments, QString requestId = {}) {
        const auto current = context();
        if (requestId.isEmpty())
            requestId = tool + QString::number(++sequence_);
        arguments.insert("requestId", requestId);
        arguments.insert("documentId", current.value("documentId"));
        arguments.insert("stamp", current.value("stamp"));
        last = arguments;
        return raw(tool, arguments);
    }
    QJsonObject raw(const QString& tool, const QJsonObject& arguments) {
        const auto result = host.callTool(tool, arguments);
        const auto structured = result.value("structuredContent").toObject();
        const auto content = result.value("content").toArray();
        require(content.size() == 1 &&
                    QJsonDocument::fromJson(
                        content.at(0).toObject().value("text").toString().toUtf8())
                            .object() == structured,
                "Text content mirrors the structured content");
        isError = result.value("isError").toBool();
        return structured;
    }
    QJsonObject committed(const QString& tool, QJsonObject arguments) {
        auto result = call(tool, std::move(arguments));
        require(!isError && result.value("status") == QLatin1String("committed"),
                tool.toStdString() + ": " + text(result));
        return result;
    }
    mcp::ToolHost host;
    int changes{0};
    bool isError{false};
    QJsonObject last;

  private:
    int sequence_{0};
};

QJsonObject circle(int id, double x, double radius) {
    return {{"id", id}, {"type", "circle"}, {"center", QJsonArray{x, 0}}, {"radius", radius}};
}
// 80 x 40 plate around the origin with a bore of radius A (approximate 9, solved to A) whose
// center (given as (1.5, -1)) is fixed at the origin by coordinate constraints.
QJsonObject plateSketch(int parameter) {
    const QJsonObject rectangle{{"id", 1},
                                {"type", "rectangle"},
                                {"first", QJsonArray{-40, -20}},
                                {"second", QJsonArray{40, 20}}};
    const QJsonObject radius{{"id", 1},
                             {"kind", "radius"},
                             {"first", QJsonObject{{"element", 2}, {"point", "center"}}},
                             {"parameterId", parameter}};
    const auto coordinate = [](int id, const char* kind) {
        return QJsonObject{{"id", id},
                           {"kind", kind},
                           {"first", QJsonObject{{"element", 2}, {"point", "center"}}},
                           {"value", mm(0)}};
    };
    auto bore = circle(2, 1.5, 9);
    bore.insert("center", QJsonArray{1.5, -1});
    return {{"plane", "xy"},
            {"elements", QJsonArray{rectangle, bore}},
            {"constraints",
             QJsonArray{radius, coordinate(2, "coordinateX"), coordinate(3, "coordinateY")}}};
}
double plateVolume(double radius, double thickness) {
    return (80.0 * 40.0 - std::numbers::pi * radius * radius) * thickness;
}

void catalog() {
    QSet<QString> names;
    for (const auto& tool : mcp::toolCatalog()) {
        require(tool.inputSchema.value("type") == QLatin1String("object") &&
                    !tool.description.isEmpty(),
                "Every tool has an object schema and a description");
        names.insert(tool.name);
    }
    require(names.size() == 17 && names.contains("get_context") &&
                names.contains("create_sketch_extrusion") && names.contains("set_placement") &&
                names.contains("create_sketch_on_face"),
            "Version 1 has get_context and 16 action tools with unique names");
}

void plateWithBore() {
    application::CadSession session;
    session.newDocument();
    Client client(session);
    const auto context = client.context();
    require(context.value("documentId").toString().size() == 32 &&
                context.value("available").toObject().value("document").toArray().contains(
                    QStringLiteral("create_sketch_extrusion")),
            "Context names the document and the tools on it");

    auto result = client.committed(
        "create_parameter", {{"name", "A"}, {"unit", "length"}, {"value", mm(8)}});
    const int a = result.value("createdParameters").toArray().at(0).toInt();
    result = client.committed("create_sketch_extrusion",
                              {{"sketch", plateSketch(a)}, {"distance", mm(10)}});
    require(result.value("createdFeatures").toArray().size() == 2 &&
                near(test::volumeOf(session.currentShape()), plateVolume(8, 10)),
            "The bore is a hole and has the parameter's radius: " + text(result));
    require(client.changes == 2, "Two published changes");

    result = client.committed("set_parameter_value", {{"parameterId", a}, {"value", mm(10)}});
    require(near(test::volumeOf(session.currentShape()), plateVolume(10, 10)),
            "The bore follows A");
    const auto sketch = client.context().value("features").toArray().at(0).toObject();
    const auto stored = session.sketchParameters(document::FeatureId{
        static_cast<std::uint64_t>(sketch.value("id").toInteger())});
    const auto& center = std::get<document::SketchCircle>(stored.elements.values[1].geometry).center;
    const auto constraints =
        sketch.value("parameters").toObject().value("sketch").toObject().value("constraints").toArray();
    require(std::abs(center.x) < 1e-9 && std::abs(center.y) < 1e-9 && constraints.size() == 3 &&
                constraints.at(1).toObject().value("value").toObject().value("value") == 0,
            "Coordinates fix the bore center and appear in the context with their value");
    require(sketch.value("kind") == QLatin1String("sketch") &&
                sketch.value("regions").toArray().size() == 2 &&
                sketch.value("parameters").toObject().value("sketch").toObject().value("plane") ==
                    QLatin1String("xy"),
            "Context lists the sketch with its regions in tool form");

    // Retry of the same request: the stored result, no second change.
    const auto before = session.sceneStatus().document;
    const auto retried = client.raw("set_parameter_value", client.last);
    require(retried.value("repeated").toBool() && session.sceneStatus().document == before &&
                client.changes == 3,
            "A retried request does not change the document again");
    // Preview: evaluated on a copy only.
    auto preview = client.call("set_parameter_value", {{"parameterId", a},
                                                        {"value", mm(5)},
                                                        {"preview", true}});
    require(preview.value("preview").toBool() &&
                preview.value("status") == QLatin1String("committed") &&
                session.sceneStatus().document == before && client.changes == 3 &&
                near(test::volumeOf(session.currentShape()), plateVolume(10, 10)),
            "A preview leaves the document alone");
}

double coordinate(const QJsonValue& vector, int axis) {
    return vector.toArray().at(axis).toDouble();
}
double topOf(const TopoDS_Shape& shape) {
    Bnd_Box bounds;
    BRepBndLib::AddOptimal(shape, bounds, false, false);
    double xMin = 0, yMin = 0, zMin = 0, xMax = 0, yMax = 0, zMax = 0;
    bounds.Get(xMin, yMin, zMin, xMax, yMax, zMax);
    return zMax;
}
// The face of the only body whose normal points along +z.
QJsonObject topFace(const QJsonObject& context) {
    const auto bodies = context.value("bodies").toArray();
    require(bodies.size() == 1, "One body: " + text(context));
    for (const auto& face : bodies.at(0).toObject().value("faces").toArray())
        if (near(coordinate(face.toObject().value("normal"), 2), 1.0))
            return face.toObject();
    throw std::runtime_error("No top face: " + text(context));
}
// A world point (x, y) on the face, in the face's sketch coordinates.
QJsonArray onFace(const QJsonObject& face, double x, double y) {
    const auto origin = face.value("origin");
    const double dx = x - coordinate(origin, 0);
    const double dy = y - coordinate(origin, 1);
    const auto axis = [&](const char* name) {
        const auto direction = face.value(name);
        return dx * coordinate(direction, 0) + dy * coordinate(direction, 1);
    };
    return {axis("xDirection"), axis("yDirection")};
}

// T01 (the gap of AI03c): a rib 60 x 4 x 8 on top of a plate with a bore, sketched on the
// plate's top face through get_context's face reference, is one body with the plate and
// follows the plate's thickness.
void ribOnPlate() {
    application::CadSession session;
    session.newDocument();
    Client client(session);
    const QJsonObject plate{{"plane", "xy"},
                            {"elements", QJsonArray{QJsonObject{{"id", 1},
                                                                {"type", "rectangle"},
                                                                {"first", QJsonArray{-40, -20}},
                                                                {"second", QJsonArray{40, 20}}},
                                                    circle(2, 0, 8)}}};
    auto result =
        client.committed("create_sketch_extrusion", {{"sketch", plate}, {"distance", mm(10)}});
    const int plateExtrusion = result.value("createdFeatures").toArray().at(1).toInt();
    auto context = client.context();
    const auto body = context.value("bodies").toArray().at(0).toObject();
    auto face = topFace(context);
    require(body.value("supportId").toInt() == plateExtrusion &&
                face.value("face").toObject() ==
                    QJsonObject{{"feature", plateExtrusion}, {"boundary", QJsonArray{1, 2}}} &&
                near(coordinate(face.value("origin"), 2), 10.0) &&
                context.value("available")
                    .toObject()
                    .value("features")
                    .toObject()
                    .value(QString::number(plateExtrusion))
                    .toArray()
                    .contains(QStringLiteral("create_sketch_on_face")),
            "The plate offers its top face by its end-cap boundary: " + text(body));
    const auto min = face.value("min").toArray();
    const auto max = face.value("max").toArray();
    require(near(max.at(0).toDouble() - min.at(0).toDouble(), 80) &&
                near(max.at(1).toDouble() - min.at(1).toDouble(), 40),
            "The face extent is 80 x 40 in its own frame: " + text(face));

    const QJsonObject rib{{"elements", QJsonArray{QJsonObject{{"id", 1},
                                                              {"type", "rectangle"},
                                                              {"first", onFace(face, -30, 12)},
                                                              {"second", onFace(face, 30, 16)}}}}};
    result = client.committed("create_sketch_on_face", {{"supportId", plateExtrusion},
                                                        {"face", face.value("face")},
                                                        {"sketch", rib}});
    const int ribSketch = result.value("feature").toInt();
    const auto ribRegions = client.context().value("features").toArray().at(2).toObject().value(
        "regions").toArray();
    require(ribRegions.size() == 1, "The rib sketch has one region");
    result = client.committed(
        "create_profile_extrusion",
        {{"sketchId", ribSketch},
         {"regions", QJsonArray{ribRegions.at(0).toObject().value("id")}},
         {"distance", mm(8)}});
    const double plateVolume = (80.0 * 40.0 - std::numbers::pi * 64.0) * 10.0;
    require(session.validBodies().size() == 1 &&
                near(test::volumeOf(session.currentShape()), plateVolume + 60.0 * 4.0 * 8.0) &&
                near(topOf(session.currentShape()), 18.0),
            "The rib sits on the plate and is one body with it");
    context = client.context();
    const auto sketch = context.value("features").toArray().at(2).toObject();
    require(sketch.value("face").toObject() == face.value("face").toObject() &&
                near(coordinate(sketch.value("frame").toObject().value("origin"), 2), 10.0) &&
                sketch.value("parameters").toObject().value("sketch").toObject().value("plane") ==
                    QLatin1String("face"),
            "The context names the sketch's face and frame: " + text(sketch));

    client.committed("set_extrusion_distance",
                     {{"extrusionId", plateExtrusion}, {"distance", mm(12)}});
    require(session.validBodies().size() == 1 && near(topOf(session.currentShape()), 20.0),
            "The rib follows the thicker plate");

    // A role on an extrusion face, and a support that is no body's latest result.
    const auto before = session.sceneStatus().document;
    const auto changes = client.changes;
    const auto support = client.context().value("bodies").toArray().at(0).toObject().value("supportId");
    result = client.call("create_sketch_on_face",
                         {{"supportId", support},
                          {"face", QJsonObject{{"feature", plateExtrusion}, {"role", "+z"}}},
                          {"sketch", rib}});
    require(client.isError && result.value("field") == QLatin1String("face"),
            "An unknown face is an argument error: " + text(result));
    result = client.call("create_sketch_on_face", {{"supportId", plateExtrusion},
                                                   {"face", face.value("face")},
                                                   {"sketch", rib}});
    require(client.isError && result.value("field") == QLatin1String("supportId"),
            "An earlier result offers no faces: " + text(result));
    result = client.call("create_sketch_on_face", {{"supportId", 77},
                                                   {"face", face.value("face")},
                                                   {"sketch", rib}});
    require(result.value("rejection") == QLatin1String("unknownTarget"),
            "An unknown support is rejected: " + text(result));
    require(session.sceneStatus().document == before && client.changes == changes,
            "Rejected face sketches change nothing");
}

// A box offers its planar sides by role.
void boxFaces() {
    application::CadSession session;
    session.newDocument();
    Client client(session);
    client.committed("create_box", {{"width", mm(30)}, {"depth", mm(20)}, {"height", mm(10)}});
    const auto faces =
        client.context().value("bodies").toArray().at(0).toObject().value("faces").toArray();
    QSet<QString> roles;
    for (const auto& face : faces)
        roles.insert(face.toObject().value("face").toObject().value("role").toString());
    require(faces.size() == 6 && roles.size() == 6 && roles.contains("+z"),
            "Six box sides with their roles: " + text(QJsonObject{{"faces", faces}}));
}

void rejections() {
    application::CadSession session;
    session.newDocument();
    Client client(session);
    client.committed("create_parameter", {{"name", "A"}, {"unit", "length"}, {"value", mm(8)}});
    const auto stamp = session.sceneStatus().document;
    const auto snapshot = session.identifiedSnapshot();
    const auto unchanged = [&](const std::string& message) {
        require(session.sceneStatus().document == stamp && session.identifiedSnapshot() == snapshot &&
                    client.changes == 1,
                message + ": no change");
    };
    const auto argumentError = [&](const QJsonObject& result, const QString& field,
                                   const std::string& message) {
        require(client.isError && result.value("error") == QLatin1String("invalidArguments") &&
                    result.value("field").toString() == field,
                message + ": " + text(result));
        unchanged(message);
    };

    // Action layer rejections, the same as a direct call.
    auto result = client.call("set_parameter_value",
                              {{"parameterId", 1}, {"value", QJsonObject{{"value", 9}, {"unit", "deg"}}}});
    require(client.isError && result.value("rejection") == QLatin1String("invalidUnit"),
            "Wrong unit: " + text(result));
    unchanged("Wrong unit");
    result = client.call("set_box_dimensions",
                         {{"boxId", 77}, {"width", mm(1)}, {"depth", mm(1)}, {"height", mm(1)}});
    require(result.value("rejection") == QLatin1String("unknownTarget"), "Unknown box");
    unchanged("Unknown box");
    auto stale = client.last;
    stale.insert("stamp", QJsonObject{{"generation", static_cast<double>(stamp.generation)},
                                      {"revision", static_cast<double>(stamp.revision - 1)}});
    stale.insert("requestId", "stale");
    result = client.raw("set_box_dimensions", stale);
    require(result.value("rejection") == QLatin1String("staleStamp"), "Old stamp");
    unchanged("Old stamp");

    // Malformed arguments never reach the document.
    auto missing = client.last;
    missing.remove("requestId");
    argumentError(client.raw("set_box_dimensions", missing), "requestId", "Missing requestId");
    argumentError(client.call("create_box", {{"width", mm(1)}, {"depth", "x"}, {"height", mm(1)}}),
                  "depth", "Wrong type");
    argumentError(client.call("create_profile_extrusion",
                              {{"sketchId", 1}, {"regions", QJsonArray{"nonsense"}}, {"distance", mm(1)}}),
                  "regions[0]", "Bad region id");
    QJsonObject triangle{{"plane", "xy"},
                         {"elements", QJsonArray{QJsonObject{{"id", 1}, {"type", "spline"}}}}};
    argumentError(client.call("create_sketch", {{"sketch", triangle}}), "sketch.elements[0].type",
                  "Unknown element type");
    // Radius 5 and diameter 20 on the same circle cannot both hold.
    const QJsonObject reference{{"element", 1}, {"point", "center"}};
    QJsonObject conflicting{
        {"plane", "xy"},
        {"elements", QJsonArray{circle(1, 0, 5)}},
        {"constraints",
         QJsonArray{QJsonObject{{"id", 1}, {"kind", "radius"}, {"first", reference}, {"value", mm(5)}},
                    QJsonObject{{"id", 2}, {"kind", "diameter"}, {"first", reference}, {"value", mm(20)}}}}};
    argumentError(client.call("create_sketch", {{"sketch", conflicting}}), "sketch.constraints",
                  "Unsolvable sketch");
    conflicting.insert("plane", "xz");
    argumentError(client.call("edit_sketch", {{"sketchId", 1}, {"sketch", conflicting}}),
                  "sketch.plane", "edit_sketch keeps the plane");
    bool unknown = false;
    try {
        static_cast<void>(client.host.callTool("delete_everything", {}));
    } catch (const std::invalid_argument&) {
        unknown = true;
    }
    require(unknown && !client.host.hasTool("delete_everything"), "Unknown tools are not called");
    unchanged("Unknown tool");
}
} // namespace

int main() {
    try {
        catalog();
        plateWithBore();
        ribOnPlate();
        boxFaces();
        rejections();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
