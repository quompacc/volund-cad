#include "SketchBoxSelection.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS_Vertex.hxx>
#include <QLineF>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <algorithm>

namespace voelund::viewport {
namespace {

// Screen points along every edge of the element's shape.
std::vector<std::vector<QPointF>> screenCurves(const TopoDS_Shape& shape,
                                               const occ::handle<V3d_View>& view) {
    constexpr int samples = 48;
    std::vector<std::vector<QPointF>> curves;
    // S15: a sketch point is a curve of one point.
    if (shape.ShapeType() == TopAbs_VERTEX) {
        const gp_Pnt world = BRep_Tool::Pnt(TopoDS::Vertex(shape));
        int x = 0, y = 0;
        view->Convert(world.X(), world.Y(), world.Z(), x, y);
        curves.push_back({QPointF(x, y)});
        return curves;
    }
    for (TopExp_Explorer edges(shape, TopAbs_EDGE); edges.More(); edges.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(edges.Current()));
        std::vector<QPointF> points;
        for (int index = 0; index <= samples; ++index) {
            const double parameter = curve.FirstParameter() +
                                     (curve.LastParameter() - curve.FirstParameter()) * index /
                                         samples;
            const gp_Pnt world = curve.Value(parameter);
            int x = 0, y = 0;
            view->Convert(world.X(), world.Y(), world.Z(), x, y);
            points.emplace_back(x, y);
        }
        curves.push_back(std::move(points));
    }
    return curves;
}

bool crosses(const QLineF& segment, const QRectF& box) {
    const QPointF corners[] = {box.topLeft(), box.topRight(), box.bottomRight(),
                               box.bottomLeft()};
    for (int side = 0; side < 4; ++side) {
        QPointF hit;
        if (segment.intersects(QLineF(corners[side], corners[(side + 1) % 4]), &hit) ==
            QLineF::BoundedIntersection)
            return true;
    }
    return false;
}

} // namespace

std::vector<std::uint64_t> boxSelection(std::span<const SceneSketchElement> elements,
                                        const occ::handle<V3d_View>& view,
                                        const QRect& rectangle, bool crossing) {
    std::vector<std::uint64_t> result;
    if (view.IsNull() || rectangle.isEmpty())
        return result;
    const QRectF box(rectangle);
    for (const SceneSketchElement& element : elements) {
        if (element.shape.IsNull())
            continue;
        const auto curves = screenCurves(element.shape, view);
        if (curves.empty())
            continue;
        bool inside = true;
        bool touched = false;
        for (const auto& points : curves)
            for (std::size_t index = 0; index < points.size(); ++index) {
                const bool contained = box.contains(points[index]);
                inside = inside && contained;
                touched = touched || contained ||
                          (index > 0 && crosses({points[index - 1], points[index]}, box));
            }
        if (crossing ? touched : inside)
            result.push_back(element.id);
    }
    return result;
}

} // namespace voelund::viewport
