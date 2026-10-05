#pragma once

#include "Theme.hpp"

#include <voelund/viewport/CadViewport.hpp>

#include <QPointF>
#include <QWidget>

#include <array>
#include <optional>

class QTimer;

namespace voelund::desktop {

// U03, reworked after the user's report of 2026-10-05: a 3D cube in the viewport's top-right
// corner that turns with the camera (ADR 0012). Each face has nine cells: the middle one
// looks at the face, the border cells at the edge, the corner cells at the corner, so all 26
// directions are a click away. Dragging the cube orbits the view, the arrows roll it by 90°,
// the house returns to the isometric view and "Alles" fits the model.
class ViewCube final : public QWidget {
  public:
    using Direction = std::array<int, 3>; // Each component -1, 0 or 1; not all 0.

    explicit ViewCube(viewport::CadViewport& viewport);

    void setTokens(const ThemeTokens& tokens);
    // Center of a visible cell looking along `direction` (for tests); nullopt if hidden.
    [[nodiscard]] std::optional<QPointF> cellCenter(const Direction& direction) const;
    // Places of the buttons, for tests.
    [[nodiscard]] QPointF rollLeftCenter() const;
    [[nodiscard]] QPointF rollRightCenter() const;
    [[nodiscard]] QPointF homeCenter() const;
    [[nodiscard]] QPointF fitCenter() const;

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    enum class Button { none, rollLeft, rollRight, home, fit };
    struct Hit final {
        Button button{Button::none};
        std::optional<Direction> direction;
    };
    [[nodiscard]] Hit hitAt(const QPointF& point) const;
    void lookAlong(const Direction& direction);
    void placeInCorner();
    void followCamera();

    viewport::CadViewport& viewport_;
    ThemeTokens tokens_;
    Hit hovered_;
    std::optional<QPointF> pressed_;
    bool dragging_{false};
    QPointF lastDrag_;
    viewport::CameraFrame frame_{};
    QTimer* follow_{nullptr};
};

} // namespace voelund::desktop
