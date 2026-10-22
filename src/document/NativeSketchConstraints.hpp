// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <TDF_Label.hxx>
#include <voelund/document/ModelDocument.hpp>

namespace voelund::document::detail {
void validateNativeSketchConstraints(const SketchParameters& sketch);
void writeNativeSketchConstraints(const TDF_Label& label, const TDF_Label& elements,
                                  const SketchParameters& sketch);
SketchConstraints readNativeSketchConstraints(const TDF_Label& label, const TDF_Label& elements,
                                              const SketchElements& geometry, std::uint64_t nextId,
                                              std::size_t remainingRecords, int schemaVersion = 3);
} // namespace voelund::document::detail
