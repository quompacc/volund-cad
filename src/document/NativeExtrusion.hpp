// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <TDF_Label.hxx>
#include <voelund/document/ModelDocument.hpp>

#include <limits>
namespace voelund::document::detail {
// `schemaVersion` of the file read; written documents are of the current one.
void validateNativeExtrusion(const ExtrusionParameters& parameters,
                             int schemaVersion = std::numeric_limits<int>::max());
void writeNativeExtrusion(const TDF_Label& label, const ExtrusionParameters& parameters);
ExtrusionParameters readNativeExtrusion(const TDF_Label& label, std::size_t remainingRecords,
                                        int schemaVersion);
// Records an extrusion consumes from the document budget: profiles, regions and chunks.
std::size_t nativeExtrusionRecords(const ExtrusionParameters& parameters);
} // namespace voelund::document::detail
