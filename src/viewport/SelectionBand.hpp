#pragma once

#include <QWidget>

namespace voelund::viewport {

// S13: the sketch selection box. Dragged left to right it is a window (solid frame: elements
// fully inside), right to left a crossing box (dashed frame: elements it touches).
class SelectionBand final : public QWidget {
  public:
    explicit SelectionBand(QWidget* parent);
    void setCrossing(bool crossing);

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    bool crossing_{false};
};

} // namespace voelund::viewport
