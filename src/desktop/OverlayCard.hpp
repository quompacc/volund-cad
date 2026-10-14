#pragma once

#include <QFrame>

#include <functional>

namespace voelund::desktop {

// U14 (user decision 2026-10-14): the viewport is the whole window; the shell floats over it
// as rounded cards. The places are fixed, so the cards never cover each other:
//   top left: project card, below it the model tree and right of that the tool rail;
//   top right: header card, below it the view cube and the tool panel card;
//   bottom centre: the timeline, the context bar above it.
namespace overlay {
inline constexpr int margin = 16;
inline constexpr int gap = 10;
inline constexpr int barHeight = 48;  // project and header card
inline constexpr int treeWidth = 248; // model tree card
inline constexpr int columnTop = margin + barHeight + gap;
inline constexpr int railLeft = margin + treeWidth + gap;
inline constexpr int cubeTop = margin + barHeight + 2;
inline constexpr int cubeHeight = 178;
inline constexpr int rightColumnTop = cubeTop + cubeHeight + 2;
inline constexpr int rightColumnWidth = 344; // tool panel card
inline constexpr int timelineHeight = 60;
// Kept free at the bottom of the left column: timeline, context bar and their gaps.
inline constexpr int bottomBand = margin + timelineHeight + gap + 56 + gap;
} // namespace overlay

// A card over the viewport. `placement` gives its geometry in the host; it is applied when
// the host is resized and when the card's own layout changes.
class OverlayCard : public QFrame {
  public:
    using Placement = std::function<QRect(const QWidget& host)>;

    OverlayCard(QWidget& host, const QString& objectName, Placement placement = {});

    void setPlacement(Placement placement);
    void place();

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool event(QEvent* event) override;

  private:
    QWidget& host_;
    Placement placement_;
    bool placing_{false};
};

} // namespace voelund::desktop
