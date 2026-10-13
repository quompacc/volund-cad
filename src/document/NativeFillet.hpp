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
// M07 (schema 15): a chamfer in the same encoding, its distance on the label.
void writeNativeChamfer(const TDF_Label& label, const ChamferParameters& parameters);
ChamferParameters readNativeChamfer(const TDF_Label& label, std::size_t remainingRecords,
                                    int schemaVersion);
std::size_t nativeChamferRecords(const ChamferParameters& parameters);
// M08 (schema 16): diameter on the label, body (1), face origin (2), x/y (3), depth (4,
// absent for through all).
void writeNativeHole(const TDF_Label& label, const HoleParameters& parameters);
HoleParameters readNativeHole(const TDF_Label& label, std::size_t remainingRecords,
                              int schemaVersion);
std::size_t nativeHoleRecords(const HoleParameters& parameters);
}
