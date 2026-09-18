#pragma once
#include <TDF_Label.hxx>
#include <voelund/document/ModelDocument.hpp>
namespace voelund::document::detail {
void writeNativeSketchSupport(const TDF_Label&, const SketchSupportBinding&);
SketchSupportBinding readNativeSketchSupport(const TDF_Label&, std::size_t remainingElements);
} // namespace voelund::document::detail
