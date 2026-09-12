#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <QWidget>

#include <functional>

namespace voelund::preview {

class ViewCube final : public QWidget {
public:
    explicit ViewCube(QWidget* parent = nullptr);

    void setCameraHandler(std::function<void(viewport::CameraView)> handler);
    void setFitHandler(std::function<void()> handler);
    void setProjectionHandler(std::function<void(bool)> handler);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    enum class Region { none, top, front, right, isometric, fit, projection };
    [[nodiscard]] Region regionAt(const QPointF& point) const;

    std::function<void(viewport::CameraView)> cameraHandler_;
    std::function<void()> fitHandler_;
    std::function<void(bool)> projectionHandler_;
    Region hovered_{Region::none};
    bool perspective_{false};
};

} // namespace voelund::preview
