// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <TDocStd_Document.hxx>
#include <voelund/document/IdentifiedDocumentSnapshot.hpp>

namespace voelund::document::detail {
// Native adapter for all current feature kinds; not the product dispatch or a mutable backend.
class NativePrimitiveCodec final {
  public:
    static void write(const Handle(TDocStd_Document) & document,
                      const IdentifiedDocumentSnapshot& snapshot);
    [[nodiscard]] static IdentifiedDocumentSnapshot read(const Handle(TDocStd_Document) & document);
};
} // namespace voelund::document::detail
