#pragma once
#include <TDF_Label.hxx>
#include <voelund/document/ModelDocument.hpp>
namespace voelund::document::detail {
void validateNativeExtrusion(const ExtrusionParameters& parameters);
void writeNativeExtrusion(const TDF_Label& label, const ExtrusionParameters& parameters);
ExtrusionParameters readNativeExtrusion(const TDF_Label& label, std::size_t remainingRecords,
                                        int schemaVersion);
// Records an extrusion consumes from the document budget: profiles, regions and chunks.
std::size_t nativeExtrusionRecords(const ExtrusionParameters& parameters);
} // namespace voelund::document::detail
