#pragma once
#include <voelund/mcp/ActionJson.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace voelund::mcp::detail {

// Typed reads of one JSON object; every failure names its field path.
class JsonReader final {
  public:
    JsonReader(QJsonObject object, QString path)
        : object_(std::move(object)), path_(std::move(path)) {}

    [[nodiscard]] QString field(const QString& key) const {
        return path_.isEmpty() ? key : path_ + QLatin1Char('.') + key;
    }
    [[nodiscard]] bool has(const QString& key) const {
        return object_.contains(key) && !object_.value(key).isNull();
    }
    [[nodiscard]] QJsonValue value(const QString& key) const {
        if (!has(key))
            throw ArgumentError(field(key), "Required field is missing");
        return object_.value(key);
    }
    [[nodiscard]] QString string(const QString& key) const {
        const auto found = value(key);
        if (!found.isString())
            throw ArgumentError(field(key), "Expected a string");
        return found.toString();
    }
    [[nodiscard]] double number(const QString& key) const {
        const auto found = value(key);
        if (!found.isDouble())
            throw ArgumentError(field(key), "Expected a number");
        return found.toDouble();
    }
    [[nodiscard]] std::uint64_t id(const QString& key) const {
        const double found = number(key);
        // Exact integers only; JSON numbers are doubles.
        if (!(found >= 0.0 && found <= 9007199254740992.0) || std::floor(found) != found)
            throw ArgumentError(field(key), "Expected a non-negative integer");
        return static_cast<std::uint64_t>(found);
    }
    [[nodiscard]] std::optional<std::uint64_t> optionalId(const QString& key) const {
        return has(key) ? std::optional{id(key)} : std::nullopt;
    }
    [[nodiscard]] bool boolean(const QString& key, bool fallback) const {
        if (!has(key))
            return fallback;
        const auto found = object_.value(key);
        if (!found.isBool())
            throw ArgumentError(field(key), "Expected true or false");
        return found.toBool();
    }
    [[nodiscard]] JsonReader object(const QString& key) const {
        const auto found = value(key);
        if (!found.isObject())
            throw ArgumentError(field(key), "Expected an object");
        return {found.toObject(), field(key)};
    }
    [[nodiscard]] QJsonArray array(const QString& key) const {
        const auto found = value(key);
        if (!found.isArray())
            throw ArgumentError(field(key), "Expected an array");
        return found.toArray();
    }
    [[nodiscard]] JsonReader element(const QString& key, qsizetype index) const {
        const auto found = array(key).at(index);
        const auto path = field(key) + QStringLiteral("[%1]").arg(index);
        if (!found.isObject())
            throw ArgumentError(path, "Expected an object");
        return {found.toObject(), path};
    }
    // [x, y] in millimetres.
    [[nodiscard]] std::pair<double, double> point(const QString& key) const {
        const auto found = array(key);
        if (found.size() != 2 || !found.at(0).isDouble() || !found.at(1).isDouble())
            throw ArgumentError(field(key), "Expected [x, y]");
        return {found.at(0).toDouble(), found.at(1).toDouble()};
    }
    // One of `names`, returned as its index.
    template <std::size_t N>
    [[nodiscard]] std::size_t choice(const QString& key, const char* const (&names)[N]) const {
        const auto text = string(key);
        for (std::size_t index = 0; index < N; ++index)
            if (text == QLatin1String(names[index]))
                return index;
        throw ArgumentError(field(key), "Unsupported value: " + text.toStdString());
    }

  private:
    QJsonObject object_;
    QString path_;
};

} // namespace voelund::mcp::detail
