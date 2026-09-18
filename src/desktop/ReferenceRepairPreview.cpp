#include "ReferenceRepairDialog.hpp"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QPen>
#include <gp_Vec.hxx>
#include <limits>

namespace voelund::desktop {
void ReferenceRepairDialog::preview(const application::SketchSupportProjection& projection,
                                    const application::ProjectedSketchPoint* point) {
    if (!preview_->scene())
        preview_->setScene(new QGraphicsScene(preview_));
    auto* scene = preview_->scene();
    scene->clear();
    preview_->setBackgroundBrush(QColor("#202b35"));
    const auto& p = projection.plane;
    const gp_Pnt origin(p.originX, p.originY, p.originZ);
    const gp_Vec x(p.xDirectionX, p.xDirectionY, p.xDirectionZ);
    const auto y = gp_Vec(p.normalX, p.normalY, p.normalZ).Crossed(x);
    const auto local = [&](const gp_Pnt& world) {
        const gp_Vec offset(origin, world);
        return QPointF(offset.Dot(x), -offset.Dot(y));
    };
    QPen outline(QColor("#a7d5f2"));
    outline.setCosmetic(true);
    outline.setWidth(2);
    QPen selected(QColor("#ffb347"));
    selected.setCosmetic(true);
    selected.setWidth(4);
    double minX = std::numeric_limits<double>::infinity(), minY = minX;
    double maxX = -minX, maxY = -minX;
    for (const auto& segment : projection.segments) {
        for (const auto& world : {segment.first, segment.second}) {
            const auto at = local(world);
            minX = std::min(minX, at.x());
            maxX = std::max(maxX, at.x());
            minY = std::min(minY, -at.y());
            maxY = std::max(maxY, -at.y());
        }
        scene->addLine(QLineF(local(segment.first), local(segment.second)),
                       point && segment.origin == point->reference.origin ? selected : outline);
    }
    previewInfo_->setText(projection.segments.empty()
                              ? tr("No outline available.")
                              : tr("Outline in face coordinates: X %1 to %2 mm; Y %3 to %4 mm")
                                    .arg(minX)
                                    .arg(maxX)
                                    .arg(minY)
                                    .arg(maxY));
    auto bounds = scene->itemsBoundingRect();
    const double scale = std::max({bounds.width(), bounds.height(), 1.0});
    if (point) {
        const auto at = local(point->world);
        const auto radius = scale * .02;
        scene->addEllipse(at.x() - radius, at.y() - radius, 2 * radius, 2 * radius, selected,
                          QColor("#ffb347"));
    }
    bounds =
        scene->itemsBoundingRect().adjusted(-scale * .08, -scale * .08, scale * .08, scale * .08);
    scene->setSceneRect(bounds);
    preview_->fitInView(bounds, Qt::KeepAspectRatio);
}
} // namespace voelund::desktop
