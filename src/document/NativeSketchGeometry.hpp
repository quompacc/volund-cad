#pragma once
#include <voelund/document/ModelDocument.hpp>
#include <TDF_Label.hxx>

namespace voelund::document::detail {
void validateNativeSketchGeometry(const SketchParameters& sketch);
void writeNativeSketchGeometry(const TDF_Label& label, const SketchParameters& sketch);
[[nodiscard]] SketchParameters readNativeSketchGeometry(const TDF_Label& label,
                                                        std::size_t remainingElements);
} // namespace voelund::document::detail
