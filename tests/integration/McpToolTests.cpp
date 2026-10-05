#include "CadSessionTestSupport.hpp"

#include <voelund/mcp/ActionJson.hpp>
#include <voelund/mcp/ToolHost.hpp>

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
    explicit Client(application::CadSession& session) : host(session, [this] { ++changes; }) {}
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
// 80 x 40 plate around the origin with a bore of radius A (approximate 9, solved to A).
QJsonObject plateSketch(int parameter) {
    const QJsonObject rectangle{{"id", 1},
                                {"type", "rectangle"},
                                {"first", QJsonArray{-40, -20}},
                                {"second", QJsonArray{40, 20}}};
    const QJsonObject radius{{"id", 1},
                             {"kind", "radius"},
                             {"first", QJsonObject{{"element", 2}, {"point", "center"}}},
                             {"parameterId", parameter}};
    return {{"plane", "xy"},
            {"elements", QJsonArray{rectangle, circle(2, 0, 9)}},
            {"constraints", QJsonArray{radius}}};
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
    require(names.size() == 16 && names.contains("get_context") &&
                names.contains("create_sketch_extrusion") && names.contains("set_placement"),
            "Version 1 has get_context and 15 action tools with unique names");
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
        rejections();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
