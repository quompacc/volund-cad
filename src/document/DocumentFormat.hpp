// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include "DocumentSerializationDetail.hpp"
#include <voelund/document/LegacyDocumentImporter.hpp>

namespace voelund::document::detail {
inline DocumentSourceFormat readSourceFormat(std::istream& input) {
    const auto header = requiredLine(input, "file header");
    for (std::uint64_t version = 1; version <= 16; ++version)
        if (header == "VOLUND " + std::to_string(version))
            return {DocumentEncoding::legacyText, version};
    if (header.starts_with("VOLUND "))
        invalidDocument("unsupported legacy schema version");
    invalidDocument("unknown document encoding; native product reader is not enabled");
}
} // namespace voelund::document::detail
