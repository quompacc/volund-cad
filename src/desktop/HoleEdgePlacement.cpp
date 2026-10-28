// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "HoleToolController.hpp"

#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>

#include <voelund/kernel/EdgeQueries.hpp>

#include <TopoDS.hxx>

#include <QObject>

#include <algorithm>

// N09c (agent decision 2026-10-28, as Fusion's hole placement by reference edges): "An zwei
// Kanten" places the hole at distances from two straight edges of its body. The first two
// clicked edges keep the hole where it is (their present distances); the fields change the
// distances; a third edge starts over. The edges are named like fillet edges, so the hole
// follows them when the body changes.
namespace voelund::desktop {

void HoleToolController::placeByEdges(bool byEdges) {
    byEdges_ = byEdges;
    offsets_.clear();
    if (byEdges_)
        panel_.setGuidance(QObject::tr("Klicke zwei gerade Kanten des Körpers an; die Abstände "
                                       "zu ihnen legen die Bohrung fest."));
    showVariantFields();
    preview();
}

std::vector<document::HoleEdgeOffset> HoleToolController::edgeOffsets() const {
    return byEdges_ && offsets_.size() == 2 ? offsets_ : std::vector<document::HoleEdgeOffset>{};
}

// The distance of the hole's present centre from the line of `edge`.
std::optional<double> HoleToolController::distanceFrom(const TopoDS_Shape& edge) const {
    const auto curve = kernel::describeEdge(TopoDS::Edge(edge));
    if (curve.kind != kernel::EdgeCurve::Kind::line)
        return std::nullopt; // straight edges only
    const auto faces = session_.supportFaces(support_);
    const auto face = std::ranges::find_if(
        faces, [&](const auto& candidate) { return face_ && candidate.origin == *face_; });
    if (face == faces.end())
        return std::nullopt;
    const auto& p = face->plane;
    const gp_Vec x(p.xDirectionX, p.xDirectionY, p.xDirectionZ);
    const gp_Vec y = gp_Vec(p.normalX, p.normalY, p.normalZ).Crossed(x);
    const gp_Pnt centre = gp_Pnt(p.originX, p.originY, p.originZ).Translated(x * x_ + y * y_);
    return curve.line.Distance(centre);
}

// The edge's name on the body, also for an edge of the previewed body.
std::optional<document::FilletEdge> HoleToolController::edgeName(const TopoDS_Shape& edge) const {
    auto named = session_.filletEdge(support_, edge);
    if (!named && previewSession_ && previewFeature_)
        named = previewSession_->filletEdge(*previewFeature_, edge);
    return named;
}

void HoleToolController::pickedEdge(std::uint64_t body, const TopoDS_Shape& edge) {
    if (!active_ || !byEdges_ || sketch_ || body == 0 || edge.ShapeType() != TopAbs_EDGE)
        return;
    const auto named = edgeName(edge);
    const auto distance = distanceFrom(edge);
    if (!named || !distance) {
        const auto message = QObject::tr("Wähle eine gerade Kante dieses Körpers.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    if (offsets_.size() == 2)
        offsets_.clear();
    offsets_.push_back({*named, *distance});
    if (offsets_.size() == 2) {
        panel_.setLengthValue("offset1", offsets_[0].distance);
        panel_.setLengthValue("offset2", offsets_[1].distance);
    }
    showVariantFields();
    preview();
}

} // namespace voelund::desktop
