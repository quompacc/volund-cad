#pragma once
#include <voelund/document/ModelDocument.hpp>
#include <TDF_Label.hxx>

namespace voelund::document::detail {
void validateNativeSketchConstraints(const SketchParameters& sketch);
void writeNativeSketchConstraints(const TDF_Label& label, const TDF_Label& elements,
                                  const SketchParameters& sketch);
SketchConstraints readNativeSketchConstraints(const TDF_Label& label, const TDF_Label& elements,
    const SketchElements& geometry, std::uint64_t nextId, std::size_t remainingRecords);
} // namespace voelund::document::detail
