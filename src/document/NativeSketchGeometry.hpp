#pragma once
#include <TDF_Label.hxx>
#include <voelund/document/ModelDocument.hpp>

namespace voelund::document::detail {
void validateNativeSketchGeometry(const SketchParameters& sketch);
void writeNativeSketchGeometry(const TDF_Label& label, const SketchParameters& sketch);
// S17: records the projected edges take from the document budget.
[[nodiscard]] std::size_t nativeSketchProjectionRecords(const SketchParameters& sketch) noexcept;
[[nodiscard]] SketchParameters readNativeSketchGeometry(const TDF_Label& label,
                                                        std::size_t remainingElements,
                                                        int schemaVersion = 4);
} // namespace voelund::document::detail
