#pragma once
#include <voelund/document/IdentifiedDocumentSnapshot.hpp>
#include <TDocStd_Document.hxx>

namespace voelund::document::detail {
// Native adapter for all current feature kinds; not the product dispatch or a mutable backend.
class NativePrimitiveCodec final {
  public:
    static void write(const Handle(TDocStd_Document)& document,
                      const IdentifiedDocumentSnapshot& snapshot);
    [[nodiscard]] static IdentifiedDocumentSnapshot read(
        const Handle(TDocStd_Document)& document);
};
} // namespace voelund::document::detail
