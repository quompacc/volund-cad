#pragma once
#include <voelund/document/ModelDocument.hpp>
#include <TDF_Label.hxx>
namespace voelund::document::detail {
void validateNativeExtrusion(const ExtrusionParameters& parameters);
void writeNativeExtrusion(const TDF_Label& label, const ExtrusionParameters& parameters);
ExtrusionParameters readNativeExtrusion(const TDF_Label& label, std::size_t remainingRecords);
}
