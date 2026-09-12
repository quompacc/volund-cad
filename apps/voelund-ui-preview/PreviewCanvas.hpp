#pragma once

#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWidget>

#include <optional>
#include <functional>

namespace voelund::preview {

class PreviewCanvas final : public QWidget {
    Q_OBJECT

public:
    enum class Mode { Solid, Sketch };
    enum class SketchTool { Select, Line, Rectangle, Circle, Arc, Constraint, Dimension, Trim, Offset };

    explicit PreviewCanvas(QWidget* parent = nullptr);
    void setMode(Mode mode);
    void setSketchTool(SketchTool tool);
    void resetSketch();
    void setDegreesOfFreedom(int value);
    void setSketchCreateHandler(
        std::function<bool(SketchTool, const QPointF&, const QPointF&)> handler);
    [[nodiscard]] Mode mode() const noexcept { return mode_; }

signals:
    void selectionChanged(const QString& label);
    void sketchStatusChanged(const QString& status);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void paintBackground(QPainter& painter) const;
    void paintSolid(QPainter& painter) const;
    void paintSketch(QPainter& painter) const;
    void paintViewCube(QPainter& painter) const;
    void paintNavigation(QPainter& painter) const;
    [[nodiscard]] QRectF sketchPlane() const;
    [[nodiscard]] QPointF toSketch(const QPointF& screenPoint) const;
    [[nodiscard]] QPointF toScreen(const QPointF& sketchPoint) const;
    [[nodiscard]] QPointF snapPoint(const QPointF& sketchPoint, QString* relation = nullptr) const;
    [[nodiscard]] int nearestEntity(const QPointF& sketchPoint) const;
    void handleSketchClick(const QPointF& screenPoint);

    struct SketchEntity {
        enum class Kind { Line, Rectangle, Circle, Arc } kind{Kind::Line};
        QPointF first;
        QPointF second;
        bool dimensioned{false};
        bool constrained{false};
    };

    Mode mode_{Mode::Solid};
    bool sideSelected_{false};
    SketchTool sketchTool_{SketchTool::Select};
    QVector<SketchEntity> entities_;
    std::optional<QPointF> pendingPoint_;
    QPointF hoverPoint_;
    int selectedEntity_{-1};
    int degreesOfFreedom_{0};
    QString transientRelation_;
    std::function<bool(SketchTool, const QPointF&, const QPointF&)> sketchCreateHandler_;
};

} // namespace voelund::preview
