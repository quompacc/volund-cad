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
// M19 (schema 19): offset on the label, base plane code (1, 0 for a face), face body (2) and
// origin (3), rotation axis (4), angle (5).
void writeNativeDatumPlane(const TDF_Label& label, const DatumPlaneParameters& parameters);
DatumPlaneParameters readNativeDatumPlane(const TDF_Label& label, std::size_t remainingRecords,
                                          int schemaVersion);
std::size_t nativeDatumPlaneRecords(const DatumPlaneParameters& parameters);
// M19: base axis code on the label (0 for an edge), edge body (1), faces (2, 3), edge id (4).
void writeNativeDatumAxis(const TDF_Label& label, const DatumAxisParameters& parameters);
DatumAxisParameters readNativeDatumAxis(const TDF_Label& label, std::size_t remainingRecords,
                                        int schemaVersion);
std::size_t nativeDatumAxisRecords(const DatumAxisParameters& parameters);
}
