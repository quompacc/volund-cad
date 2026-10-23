// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace voelund::document {

// M01 (user decision 2026-10-10): what the user sets for a body as a whole. The name is
// document content (renaming is one undo step); `hidden` is stored with the document and
// marks it modified, but undo and redo keep the current visibility. N06 (schema 38, user
// decision 2026-10-23): the colour is document content too (one undo step).
struct BodyAttributes final {
    std::string name{}; // empty: the shell shows "Körper N"
    bool hidden{false};
    std::optional<std::uint32_t> color{}; // 0xRRGGBB; none: the theme's body colour
    [[nodiscard]] bool operator==(const BodyAttributes&) const = default;
};

struct BodyAttributeEntry final {
    std::uint64_t body; // BodyId value
    BodyAttributes attributes;
    [[nodiscard]] bool operator==(const BodyAttributeEntry&) const = default;
};

// Sorted by body; only bodies of the document, and no entry that holds only defaults. A
// vector (not a map), so snapshots keep their nothrow moves on every standard library.
using BodyAttributeTable = std::vector<BodyAttributeEntry>;

inline constexpr std::size_t maximumBodyNameBytes = 256;
inline constexpr std::uint32_t maximumBodyColor = 0xFFFFFF;

[[nodiscard]] inline BodyAttributes bodyAttributes(const BodyAttributeTable& table,
                                                   std::uint64_t body) {
    const auto found = std::ranges::lower_bound(table, body, {}, &BodyAttributeEntry::body);
    return found != table.end() && found->body == body ? found->attributes : BodyAttributes{};
}

// Sets one body's attributes; defaults remove the entry, the order stays sorted.
inline void setBodyAttributes(BodyAttributeTable& table, std::uint64_t body,
                              BodyAttributes attributes) {
    const auto found = std::ranges::lower_bound(table, body, {}, &BodyAttributeEntry::body);
    const bool present = found != table.end() && found->body == body;
    if (attributes == BodyAttributes{}) {
        if (present)
            table.erase(found);
        return;
    }
    if (present)
        found->attributes = std::move(attributes);
    else
        table.insert(found, BodyAttributeEntry{body, std::move(attributes)});
}

} // namespace voelund::document
