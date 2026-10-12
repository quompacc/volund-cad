#pragma once
#include <voelund/document/ModelDocument.hpp>
#include <TDF_Label.hxx>
namespace voelund::document::detail {
// M05 (schema 14): radius on the label, body (1), edges (2) as pairs of face origins.
void writeNativeFillet(const TDF_Label& label, const FilletParameters& parameters);
FilletParameters readNativeFillet(const TDF_Label& label, std::size_t remainingRecords,
                                  int schemaVersion);
// Records a fillet consumes from the document budget: its edges and their face boundaries.
std::size_t nativeFilletRecords(const FilletParameters& parameters);
}
