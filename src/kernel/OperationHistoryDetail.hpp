#pragma once

#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepTools_History.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <algorithm>
#include <voelund/kernel/OperationHistory.hpp>

namespace voelund::kernel::detail {

inline std::vector<TopoDS_Shape> uniqueSubshapes(const TopoDS_Shape& shape) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> map;
    TopExp::MapShapes(shape, TopAbs_FACE, map);
    TopExp::MapShapes(shape, TopAbs_EDGE, map);
    std::vector<TopoDS_Shape> result;
    result.reserve(static_cast<std::size_t>(map.Extent()));
    for (int i = 1; i <= map.Extent(); ++i)
        result.push_back(map(i));
    return result;
}
inline void appendUnique(std::vector<TopoDS_Shape>& shapes, const TopoDS_Shape& shape) {
    if (std::none_of(shapes.begin(), shapes.end(), [&](const auto& s) { return s.IsSame(shape); }))
        shapes.push_back(shape);
}
inline void configureBoolean(BRepAlgoAPI_BooleanOperation& operation, const TopoDS_Shape& first,
                             const TopoDS_Shape& second) {
    NCollection_List<TopoDS_Shape> arguments, tools;
    arguments.Append(first);
    tools.Append(second);
    operation.SetArguments(arguments);
    operation.SetTools(tools);
    operation.SetNonDestructive(true);
    operation.SetToFillHistory(true);
    operation.Build();
}
// User report 2026-10-13: several steps merged into one history (BRepTools_History).
struct MergedHistory final {
    BRepTools_History& history;
    const NCollection_List<TopoDS_Shape>& Modified(const TopoDS_Shape& shape) {
        return history.Modified(shape);
    }
    const NCollection_List<TopoDS_Shape>& Generated(const TopoDS_Shape& shape) {
        return history.Generated(shape);
    }
    bool IsDeleted(const TopoDS_Shape& shape) { return history.IsRemoved(shape); }
};
// M05: any OCCT builder with Modified/Generated/IsDeleted (Booleans, fillets), or a merged
// history.
template <class Operation>
inline void appendHistory(ShapeOperationResult& result, Operation& operation, bool hasHistory,
                          const TopoDS_Shape& input, HistoryOperand operand) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> outputs;
    TopExp::MapShapes(result.shape, outputs);
    for (const auto& source : uniqueSubshapes(input)) {
        SubshapeHistory entry{operand, source, {}, {}};
        bool complete = hasHistory;
        for (const auto& shape : operation.Modified(source)) {
            if (outputs.Contains(shape) && shape.ShapeType() == source.ShapeType())
                appendUnique(entry.modified, shape);
            else
                complete = false;
        }
        for (const auto& shape : operation.Generated(source)) {
            if (outputs.Contains(shape))
                appendUnique(entry.generated, shape);
            else
                complete = false;
        }
        entry.deleted = operation.IsDeleted(source);
        entry.unchanged = outputs.Contains(source);
        const bool continued = entry.unchanged || !entry.modified.empty();
        // Generated geometry does not establish continuation of the source identity.
        if (entry.deleted == continued)
            complete = false;
        entry.coverage = complete ? HistoryCoverage::complete : HistoryCoverage::unavailable;
        result.history.push_back(std::move(entry));
    }
}
inline ShapeOperationResult booleanResult(BRepAlgoAPI_BooleanOperation& operation,
                                          const TopoDS_Shape& first, const TopoDS_Shape& second) {
    ShapeOperationResult result{operation.Shape(), {}, {}};
    appendHistory(result, operation, operation.HasHistory(), first, HistoryOperand::first);
    appendHistory(result, operation, operation.HasHistory(), second, HistoryOperand::second);
    return result;
}
inline ShapeOperationResult transformInput(const TopoDS_Shape& source) {
    ShapeOperationResult result{source, {}, {}};
    for (const auto& shape : uniqueSubshapes(source))
        result.history.push_back(
            {HistoryOperand::first, shape, {shape}, {}, false, false, HistoryCoverage::complete});
    return result;
}
inline void transformStep(ShapeOperationResult& result, const gp_Trsf& transform) {
    BRepBuilderAPI_Transform operation(result.shape, transform, true);
    result.shape = operation.Shape();
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> outputs;
    TopExp::MapShapes(result.shape, outputs);
    for (auto& entry : result.history) {
        if (entry.coverage == HistoryCoverage::unavailable)
            continue;
        const auto mapped = operation.ModifiedShape(entry.modified.front());
        if (mapped.IsNull() || !outputs.Contains(mapped) ||
            mapped.ShapeType() != entry.source.ShapeType()) {
            entry.modified.clear();
            entry.coverage = HistoryCoverage::unavailable;
        } else {
            entry.modified = {mapped};
        }
    }
}
} // namespace voelund::kernel::detail
