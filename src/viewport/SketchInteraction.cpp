#include "SketchInteraction.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_TypeOfColor.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <algorithm>
#include <utility>

namespace voelund::viewport {
namespace {

gp_Pnt worldPoint(const PlanarFaceSelection& support, double x, double y)
{
    const gp_Dir normal(support.normalX, support.normalY, support.normalZ);
    const gp_Dir xDirection(
        support.xDirectionX, support.xDirectionY, support.xDirectionZ);
    const gp_Dir yDirection = normal.Crossed(xDirection);
    return gp_Pnt(support.originX, support.originY, support.originZ)
        .Translated(gp_Vec(xDirection) * x + gp_Vec(yDirection) * y);
}

} // namespace

std::optional<PlanarFaceSelection> planarFaceSelection(
    std::uint64_t bodyId,
    const TopoDS_Shape& detectedShape)
{
    if (detectedShape.IsNull() || detectedShape.ShapeType() != TopAbs_FACE) {
        return std::nullopt;
    }
    const TopoDS_Face face = TopoDS::Face(detectedShape);
    const BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane) {
        return std::nullopt;
    }
    const gp_Pln plane = surface.Plane();
    gp_Dir normal = plane.Axis().Direction();
    if (face.Orientation() == TopAbs_REVERSED) {
        normal.Reverse();
    }
    const gp_Pnt origin = plane.Location();
    const gp_Dir xDirection = plane.XAxis().Direction();
    return PlanarFaceSelection{
        bodyId,
        origin.X(), origin.Y(), origin.Z(),
        normal.X(), normal.Y(), normal.Z(),
        xDirection.X(), xDirection.Y(), xDirection.Z()};
}

void SketchInteraction::begin(
    const PlanarFaceSelection& support,
    const occ::handle<AIS_InteractiveContext>& context,
    const occ::handle<V3d_View>& view)
{
    cancel();
    support_ = support;
    context_ = context;
    view_ = view;
    active_ = true;
}

void SketchInteraction::cancel()
{
    if (!context_.IsNull() && !preview_.IsNull()) {
        context_->Remove(preview_, false);
    }
    preview_.Nullify();
    context_.Nullify();
    view_.Nullify();
    firstPoint_.reset();
    secondPoint_.reset();
    active_ = false;
}

void SketchInteraction::setSnapping(bool enabled, double linearStep)
{
    snapping_ = enabled;
    linearStep_ = std::max(0.001, linearStep);
}

void SketchInteraction::setDistancePreviewHandler(std::function<void(double)> handler)
{
    distancePreviewHandler_ = std::move(handler);
}

void SketchInteraction::updateDistance(double distance)
{
    if (!secondPoint_.has_value() || distance <= 0.0) {
        return;
    }
    distance_ = distance;
    showExtrusionPreview();
}

bool SketchInteraction::isActive() const noexcept
{
    return active_;
}

std::optional<std::pair<double, double>> SketchInteraction::project(int x, int y) const
{
    if (!active_ || view_.IsNull()) {
        return std::nullopt;
    }
    double px = 0.0;
    double py = 0.0;
    double pz = 0.0;
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
    view_->ConvertWithProj(x, y, px, py, pz, dx, dy, dz);
    const gp_Pnt rayPoint(px, py, pz);
    const gp_Vec rayDirection(dx, dy, dz);
    const gp_Pnt origin(support_.originX, support_.originY, support_.originZ);
    const gp_Vec normal(support_.normalX, support_.normalY, support_.normalZ);
    const double denominator = normal.Dot(rayDirection);
    if (std::abs(denominator) < 1.0e-12) {
        return std::nullopt;
    }
    const double parameter = normal.Dot(gp_Vec(rayPoint, origin)) / denominator;
    const gp_Pnt point = rayPoint.Translated(rayDirection * parameter);
    const gp_Vec offset(origin, point);
    const gp_Vec xDirection(
        support_.xDirectionX, support_.xDirectionY, support_.xDirectionZ);
    const gp_Vec yDirection = normal.Crossed(xDirection);
    return std::pair<double, double>{offset.Dot(xDirection), offset.Dot(yDirection)};
}

void SketchInteraction::move(int x, int y)
{
    if (!firstPoint_.has_value()) {
        return;
    }
    if (secondPoint_.has_value()) {
        constexpr double millimetresPerPixel = 0.25;
        const double rawDistance = std::max(
            0.001, std::abs(static_cast<double>(y - extrusionAnchorY_))
                * millimetresPerPixel);
        distance_ = snapping_
            ? std::max(linearStep_, std::round(rawDistance / linearStep_) * linearStep_)
            : rawDistance;
        showExtrusionPreview();
        if (distancePreviewHandler_) {
            distancePreviewHandler_(distance_);
        }
        return;
    }
    const auto point = project(x, y);
    if (point.has_value()) {
        showRectanglePreview(point->first, point->second);
    }
}

std::optional<SketchRectangleResult> SketchInteraction::click(int x, int y)
{
    if (secondPoint_.has_value()) {
        const SketchRectangleResult result{
            support_, firstPoint_->first, firstPoint_->second,
            secondPoint_->first, secondPoint_->second, distance_};
        active_ = false;
        return result;
    }
    const auto point = project(x, y);
    if (!point.has_value()) {
        return std::nullopt;
    }
    if (!firstPoint_.has_value()) {
        firstPoint_ = point;
        return std::nullopt;
    }
    constexpr double minimumSize = 1.0e-6;
    if (std::abs(point->first - firstPoint_->first) < minimumSize
        || std::abs(point->second - firstPoint_->second) < minimumSize) {
        return std::nullopt;
    }
    secondPoint_ = point;
    extrusionAnchorY_ = y;
    distance_ = snapping_ ? linearStep_ : 1.0;
    showExtrusionPreview();
    if (distancePreviewHandler_) {
        distancePreviewHandler_(distance_);
    }
    return std::nullopt;
}

void SketchInteraction::showRectanglePreview(double x, double y)
{
    constexpr double minimumSize = 1.0e-6;
    const auto [firstX, firstY] = *firstPoint_;
    if (std::abs(x - firstX) < minimumSize
        || std::abs(y - firstY) < minimumSize) {
        if (!preview_.IsNull()) {
            context_->Remove(preview_, false);
            preview_.Nullify();
        }
        return;
    }
    if (!preview_.IsNull()) {
        context_->Remove(preview_, false);
    }
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(worldPoint(support_, firstX, firstY));
    polygon.Add(worldPoint(support_, x, firstY));
    polygon.Add(worldPoint(support_, x, y));
    polygon.Add(worldPoint(support_, firstX, y));
    polygon.Close();
    const TopoDS_Shape previewShape = polygon.Wire();
    preview_ = new AIS_Shape(previewShape);
    preview_->SetColor(Quantity_Color(0.1, 1.0, 0.35, Quantity_TOC_RGB));
    preview_->SetWidth(3.0);
    context_->Display(preview_, false);
}

void SketchInteraction::showExtrusionPreview()
{
    if (!firstPoint_.has_value() || !secondPoint_.has_value()) {
        return;
    }
    if (!preview_.IsNull()) {
        context_->Remove(preview_, false);
    }
    const auto [firstX, firstY] = *firstPoint_;
    const auto [secondX, secondY] = *secondPoint_;
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(worldPoint(support_, firstX, firstY));
    polygon.Add(worldPoint(support_, secondX, firstY));
    polygon.Add(worldPoint(support_, secondX, secondY));
    polygon.Add(worldPoint(support_, firstX, secondY));
    polygon.Close();
    const TopoDS_Shape face = BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
    const gp_Vec direction(
        support_.normalX * distance_,
        support_.normalY * distance_,
        support_.normalZ * distance_);
    preview_ = new AIS_Shape(BRepPrimAPI_MakePrism(face, direction).Shape());
    preview_->SetColor(Quantity_Color(0.1, 0.75, 1.0, Quantity_TOC_RGB));
    preview_->SetTransparency(0.35);
    context_->Display(preview_, false);
}

} // namespace voelund::viewport
