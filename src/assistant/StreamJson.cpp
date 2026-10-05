#include <voelund/assistant/Assistant.hpp>

#include <QJsonArray>
#include <QJsonDocument>

namespace voelund::assistant {
namespace {
using Kind = AssistantEvent::Kind;

const QString prefix = QStringLiteral("mcp__volund__");

QString toolName(const QString& name) {
    return name.startsWith(prefix) ? name.mid(prefix.size()) : name;
}

// Tool results arrive as text or as a list of text blocks; Völund's are JSON objects.
QString resultText(const QJsonValue& content) {
    if (content.isString())
        return content.toString();
    QString text;
    for (const auto& block : content.toArray())
        if (block.toObject().value("type") == QLatin1String("text"))
            text += block.toObject().value("text").toString();
    return text;
}
} // namespace

std::vector<AssistantEvent> parseStreamJsonLine(const QByteArray& line) {
    const auto document = QJsonDocument::fromJson(line.trimmed());
    if (!document.isObject())
        return {};
    const auto object = document.object();
    const auto type = object.value("type").toString();
    const auto session = object.value("session_id").toString();
    std::vector<AssistantEvent> events;

    if (type == QLatin1String("system") && object.value("subtype") == QLatin1String("init")) {
        events.push_back({Kind::started, {}, {}, {}, false, {}, session});
        for (const auto& server : object.value("mcp_servers").toArray()) {
            const auto entry = server.toObject();
            if (entry.value("name") == QLatin1String("volund") &&
                entry.value("status") != QLatin1String("connected"))
                events.push_back({Kind::failed,
                                  QStringLiteral("Völund ist für Claude Code nicht erreichbar (%1)")
                                      .arg(entry.value("status").toString()),
                                  {}, {}, false, {}, session});
        }
    } else if (type == QLatin1String("assistant")) {
        for (const auto& item : object.value("message").toObject().value("content").toArray()) {
            const auto block = item.toObject();
            const auto kind = block.value("type").toString();
            if (kind == QLatin1String("text") && !block.value("text").toString().isEmpty())
                events.push_back({Kind::text, block.value("text").toString()});
            else if (kind == QLatin1String("tool_use"))
                events.push_back({Kind::toolStarted, {}, toolName(block.value("name").toString()),
                                  block.value("id").toString()});
        }
    } else if (type == QLatin1String("user")) {
        for (const auto& item : object.value("message").toObject().value("content").toArray()) {
            const auto block = item.toObject();
            if (block.value("type") != QLatin1String("tool_result"))
                continue;
            const auto text = resultText(block.value("content"));
            events.push_back({Kind::toolFinished, text, {}, block.value("tool_use_id").toString(),
                              block.value("is_error").toBool(),
                              QJsonDocument::fromJson(text.toUtf8()).object()});
        }
    } else if (type == QLatin1String("result")) {
        const bool success = object.value("subtype") == QLatin1String("success") &&
                             !object.value("is_error").toBool();
        auto text = object.value("result").toString();
        if (text.isEmpty() && !success)
            text = object.value("subtype").toString();
        events.push_back({success ? Kind::finished : Kind::failed, text, {}, {}, false, {},
                          session});
    }
    return events;
}

} // namespace voelund::assistant
