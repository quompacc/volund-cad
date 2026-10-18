#include "OperationHistoryDetail.hpp"
#include "ShapeValidity.hpp"
#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepTools_History.hxx>
#include <NCollection_IndexedMap.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>

#include <optional>
#include <vector>

// T05: a join that leaves no seam where the tool continues a face or an edge of the input
// (pulling the top of a box gives sides of one face each). Coplanar faces and collinear edges
// are unified after the Boolean, and the Boolean's history is carried through the unification.
// Only seams between the input and the tool go: when two subshapes of the input would merge
// with each other, the plain join is returned, so names in existing models never change.
namespace voelund::kernel {
namespace {

using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

int solids(const TopoDS_Shape& shape) {
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next())
        ++count;
    return count;
}

// The images of `shapes` after the unification; false when one left the result untraced.
bool carry(const std::vector<TopoDS_Shape>& shapes, BRepTools_History& history,
           const ShapeMap& outputs, std::vector<TopoDS_Shape>& images) {
    bool complete = true;
    for (const auto& shape : shapes) {
        if (history.IsRemoved(shape))
            continue;
        const auto& modified = history.Modified(shape);
        if (modified.IsEmpty()) {
            if (outputs.Contains(shape))
                detail::appendUnique(images, shape);
            else
                complete = false;
            continue;
        }
        for (const auto& image : modified)
            if (outputs.Contains(image))
                detail::appendUnique(images, image);
            else
                complete = false;
    }
    return complete;
}

std::optional<ShapeOperationResult> unified(const ShapeOperationResult& joined) {
    ShapeUpgrade_UnifySameDomain unify(joined.shape, true, true, false);
    unify.Build();
    const TopoDS_Shape merged = unify.Shape();
    if (merged.IsNull() || merged.IsSame(joined.shape) || solids(merged) != 1 ||
        !detail::isValidShape(merged))
        return std::nullopt;
    const auto history = unify.History();
    if (history.IsNull())
        return std::nullopt;
    ShapeMap outputs;
    TopExp::MapShapes(merged, outputs);
    ShapeOperationResult result{merged, joined.origins, {}};
    // Which input subshape each result shape continues: two input subshapes may not share one.
    std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>> inputImages;
    for (const auto& entry : joined.history) {
        SubshapeHistory next{entry.operand, entry.source, {}, {}};
        std::vector<TopoDS_Shape> current = entry.modified;
        if (entry.unchanged)
            current.push_back(entry.source);
        std::vector<TopoDS_Shape> images;
        bool complete = carry(current, *history, outputs, images);
        complete = carry(entry.generated, *history, outputs, next.generated) && complete;
        // The source itself still in the result counts as unchanged, its images as modified.
        for (const auto& image : images) {
            if (image.IsSame(entry.source))
                next.unchanged = true;
            else
                next.modified.push_back(image);
        }
        next.deleted = images.empty();
        next.coverage = entry.coverage == HistoryCoverage::complete && complete
                            ? HistoryCoverage::complete
                            : HistoryCoverage::unavailable;
        if (entry.operand == HistoryOperand::first)
            for (const auto& image : images) {
                for (const auto& [other, otherImage] : inputImages)
                    if (otherImage.IsSame(image) && !other.IsSame(entry.source))
                        return std::nullopt; // two input subshapes would merge
                inputImages.emplace_back(entry.source, image);
            }
        result.history.push_back(std::move(next));
    }
    return result;
}

} // namespace

ShapeOperationResult ShapeOperations::fuseMergingWithHistory(const TopoDS_Shape& left,
                                                             const TopoDS_Shape& right) {
    auto joined = fuseWithHistory(left, right);
    try {
        if (auto merged = unified(joined))
            return std::move(*merged);
    } catch (const Standard_Failure&) {
        // The plain join stays valid; unification is an improvement only.
    }
    return joined;
}

} // namespace voelund::kernel
