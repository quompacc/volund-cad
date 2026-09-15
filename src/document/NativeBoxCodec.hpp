#pragma once
#include <voelund/document/IdentifiedDocumentSnapshot.hpp>
#include <TDocStd_Document.hxx>

namespace voelund::document::detail {
// Isolated box-family adapter; not the product dispatch or a mutable shadow backend.
class NativeBoxCodec final {
  public:
    static void write(const Handle(TDocStd_Document)& document,
                      const IdentifiedDocumentSnapshot& snapshot);
    [[nodiscard]] static IdentifiedDocumentSnapshot read(
        const Handle(TDocStd_Document)& document);
};
} // namespace voelund::document::detail
