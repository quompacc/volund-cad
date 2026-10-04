#pragma once

#include "Theme.hpp"

#include <voelund/viewport/CadViewport.hpp>

#include <QWidget>

#include <functional>

namespace voelund::desktop {

// U03: standard views as an overlay in the viewport's top-right corner (ADR 0012). Faces
// pick top, front and right; the buttons below pick isometric and fit all.
class ViewCube final : public QWidget {
  public:
    explicit ViewCube(viewport::CadViewport& viewport);

    void setTokens(const ThemeTokens& tokens);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    enum class Region { none, top, front, right, isometric, fit };
    [[nodiscard]] Region regionAt(const QPointF& point) const;
    void placeInCorner();

    viewport::CadViewport& viewport_;
    ThemeTokens tokens_;
    Region hovered_{Region::none};
};

} // namespace voelund::desktop
