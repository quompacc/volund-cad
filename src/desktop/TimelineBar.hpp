#pragma once

#include "OverlayCard.hpp"
#include "ShellIcons.hpp"
#include "Theme.hpp"

#include <voelund/document/ModelDocument.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

class QHBoxLayout;
class QLabel;
class QScrollArea;
class QMouseEvent;
class QToolButton;

namespace voelund::application {
struct FeatureRuntimeState;
}

namespace voelund::desktop {

// U06: the timeline at the bottom of the window (ADR 0012). It shows every feature in
// document order; a click opens the feature (sketch editor for sketches, properties for the
// others). Rollback, reordering and error marks follow in M15.
// U14 (user decision 2026-10-14): a card at the bottom centre of the viewport with one symbol
// chip per step; number and name are in the tooltip. Long histories scroll sideways.
// M15: the rollback marker (and the end marker) can be dragged into any gap between steps; Left
// and Right select the neighbouring step, Home and End the first and last, Enter opens it.
// M16: a step dragged into another gap moves there; Delete deletes it.
class TimelineBar final : public OverlayCard {
  public:
    explicit TimelineBar(QWidget& host);

    void setOpenHandler(std::function<void(std::uint64_t)> handler);
    // M14: a right click on a step asks for its menu at that screen point.
    void setMenuHandler(std::function<void(std::uint64_t, QPoint)> handler);
    // M14/M15: the marker after a step (its feature), or at the end (nullopt): a click on the
    // end marker while rolled back, or a drag of the marker.
    void setRollbackHandler(std::function<void(std::optional<std::uint64_t>)> handler);
    // M15: a step chosen with the keyboard is selected, not opened.
    void setSelectHandler(std::function<void(std::uint64_t)> handler);
    // M16: a step dragged into another gap asks to move after that step (none: to the front);
    // Delete on a step asks to delete it.
    void
    setReorderHandler(std::function<void(std::uint64_t, std::optional<std::uint64_t>)> handler);
    void setDeleteHandler(std::function<void(std::uint64_t)> handler);
    // N02b: Strg-click adds or removes a step, Umschalt-click adds the steps up to it; the
    // grouped steps are marked.
    void setGroupHandler(std::function<void(std::uint64_t, bool range)> handler);
    void markGroup(std::span<const document::FeatureId> features);

  private:
    void stepPressed(QToolButton& step, const QMouseEvent& event, std::uint64_t id);

  public:
    // M20/U14: `icons` overrides the kind's icon, e.g. revolve, sweep or the primitive's shape.
    using IconOverride = std::pair<document::FeatureId, ShellIcon>;
    void showFeatures(std::span<const document::FeatureDescriptor> features,
                      std::optional<document::FeatureId> selected,
                      std::span<const IconOverride> icons = {});
    void setTheme(ThemeMode mode);
    // U10: red (error) or amber (blocked) markers with the message as tooltip.
    void showStates(std::span<const application::FeatureRuntimeState> states);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    [[nodiscard]] QRect placement(const QWidget& host) const;

    QScrollArea* scroll_{nullptr};
    QWidget* content_{nullptr};
    QHBoxLayout* steps_{nullptr};
    QLabel* empty_{nullptr};
    std::vector<QToolButton*> buttons_;
    std::vector<ShellIcon> icons_;
    std::function<void(std::uint64_t)> open_;
    std::function<void(std::uint64_t, QPoint)> menu_;
    std::function<void(std::optional<std::uint64_t>)> rollback_;
    std::function<void(std::uint64_t)> select_;
    std::function<void(std::uint64_t, std::optional<std::uint64_t>)> reorder_;
    std::function<void(std::uint64_t)> delete_;
    std::function<void(std::uint64_t, bool)> group_;
    Qt::KeyboardModifiers pressModifiers_; // N02b: of the last press on a step
    // M16: a step being dragged, from where it was pressed (content coordinates).
    std::optional<std::uint64_t> pressedStep_;
    QPoint pressedAt_;
    bool draggingStep_{false};
    QToolButton* end_{nullptr};
    // M15: the gap the marker was dropped in (steps before it), from the track.
    void markerDropped(std::size_t stepsBefore);
    ThemeMode mode_{ThemeMode::light};
};

} // namespace voelund::desktop
