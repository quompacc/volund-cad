#pragma once

#include <TopoDS_Shape.hxx>
#include <vector>

namespace voelund::kernel {

enum class PrimitiveRole {
    boxNegativeX,
    boxPositiveX,
    boxNegativeY,
    boxPositiveY,
    boxNegativeZ,
    boxPositiveZ,
    cylinderStart,
    cylinderEnd,
    cylinderSide,
    sphereSurface, // M27 (schema 25)
    torusSurface
};
struct PrimitiveOrigin final {
    PrimitiveRole role;
    TopoDS_Shape shape;
};
enum class HistoryOperand { first, second };
enum class HistoryCoverage { complete, unavailable };
struct SubshapeHistory final {
    HistoryOperand operand;
    TopoDS_Shape source;
    std::vector<TopoDS_Shape> modified;
    std::vector<TopoDS_Shape> generated;
    bool deleted{false};
    bool unchanged{false};
    HistoryCoverage coverage{HistoryCoverage::unavailable};
};
// Value-owned OCCT handles retain source/result topology after the builder dies.
// Treat published shapes as immutable. History covers input faces and edges only.
// No traversal indices, persistent IDs, document revisions or geometric matching.
struct ShapeOperationResult final {
    TopoDS_Shape shape;
    std::vector<PrimitiveOrigin> origins;
    std::vector<SubshapeHistory> history;
};

} // namespace voelund::kernel
