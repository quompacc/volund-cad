// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <cstddef>
#include <string_view>

namespace voelund::document::detail {
// Preflight without input-sized allocations for the BinOcaf 12 envelope produced by our
// supported little-endian OCCT builds. Returns the start of the native payload.
// Does not validate the section table, attributes, label tree or trailing data.
[[nodiscard]] std::size_t validateNativeContainerHeader(std::string_view bytes);
} // namespace voelund::document::detail
