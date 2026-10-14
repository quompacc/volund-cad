#pragma once
#include <voelund/document/ModelDocument.hpp>
#include <TDF_Label.hxx>
namespace voelund::document::detail {
// M21 (schema 18): operation code on the label, target body (1), tool bodies (2), keep (3).
void writeNativeCombine(const TDF_Label& label, const CombineParameters& parameters);
CombineParameters readNativeCombine(const TDF_Label& label, std::size_t remainingRecords,
                                    int schemaVersion);
// Records a combination consumes from the document budget: its tool bodies.
std::size_t nativeCombineRecords(const CombineParameters& parameters);
}
