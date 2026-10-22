// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "NativeBodyAttributes.hpp"

#include <voelund/document/IdCapacity.hpp>
#include <voelund/document/LegacyBodyIdentity.hpp>

#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>

#include <charconv>
#include <set>
#include <stdexcept>
#include <string>

namespace voelund::document {

void validateBodyAttributes(const BodyAttributeTable& table,
                            std::span<const FeatureDescriptor> features,
                            std::span<const FeatureParameters> parameters) {
    if (table.empty())
        return;
    std::set<std::uint64_t> bodies;
    for (const auto& context : featureBodyContexts(features, parameters))
        if (context)
            bodies.insert(context->value());
    for (std::size_t index = 0; index < table.size(); ++index) {
        const auto& [body, attributes] = table[index];
        if (index > 0 && table[index - 1].body >= body)
            throw std::invalid_argument("Body attributes are not sorted or repeat a body");
        if (!bodies.contains(body))
            throw std::invalid_argument("Body attributes name a body the document does not have");
        if (attributes == BodyAttributes{})
            throw std::invalid_argument("Body attributes hold only defaults");
        if (attributes.name.size() > maximumBodyNameBytes ||
            attributes.name.find('\0') != std::string::npos)
            throw std::invalid_argument("Invalid body name");
    }
}

namespace detail {
namespace {
void require(bool condition) {
    if (!condition)
        throw std::invalid_argument("Invalid native body attributes");
}
TDF_Label child(const TDF_Label& parent, int tag) {
    const auto label = parent.FindChild(tag, false);
    require(!label.IsNull());
    return label;
}
std::string text(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.NbChildren() == 0 && label.FindAttribute(TDataStd_AsciiString::GetID(), value));
    const auto& string = value->Get();
    require(string.Length() <= static_cast<int>(maximumBodyNameBytes));
    return {string.ToCString(), static_cast<std::size_t>(string.Length())};
}
int integer(const TDF_Label& label) {
    Handle(TDataStd_Integer) value;
    require(label.FindAttribute(TDataStd_Integer::GetID(), value));
    return value->Get();
}
} // namespace

void writeNativeBodyAttributes(const TDF_Label& label, const BodyAttributeTable& table) {
    TDataStd_Integer::Set(label, static_cast<int>(table.size()));
    int index = 0;
    for (const auto& [body, attributes] : table) {
        const auto entry = label.FindChild(++index);
        TDataStd_AsciiString::Set(entry.FindChild(1), std::to_string(body).c_str());
        TDataStd_AsciiString::Set(entry.FindChild(2), attributes.name.c_str());
        TDataStd_Integer::Set(entry.FindChild(3), attributes.hidden ? 1 : 0);
    }
}

BodyAttributeTable readNativeBodyAttributes(const TDF_Label& label) {
    const int count = integer(label);
    require(count > 0 && label.NbChildren() == count && label.NbAttributes() == 1);
    BodyAttributeTable table;
    for (int index = 1; index <= count; ++index) {
        const auto entry = child(label, index);
        require(entry.NbChildren() == 3);
        const auto encoded = text(child(entry, 1));
        std::uint64_t body = 0;
        const auto parsed = std::from_chars(encoded.data(), encoded.data() + encoded.size(), body);
        require(parsed.ec == std::errc{} && parsed.ptr == encoded.data() + encoded.size() &&
                encoded == std::to_string(body) && body != 0 && hasIdCapacity(body, 0));
        const int hidden = integer(child(entry, 3));
        require(hidden == 0 || hidden == 1);
        require(table.empty() || table.back().body < body);
        table.push_back({body, BodyAttributes{text(child(entry, 2)), hidden == 1}});
    }
    return table;
}

} // namespace detail
} // namespace voelund::document
