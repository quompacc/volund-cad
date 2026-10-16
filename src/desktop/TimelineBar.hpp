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
class TimelineBar final : public OverlayCard {
  public:
    explicit TimelineBar(QWidget& host);

    void setOpenHandler(std::function<void(std::uint64_t)> handler);
    // M14: a right click on a step asks for its menu at that screen point.
    void setMenuHandler(std::function<void(std::uint64_t, QPoint)> handler);
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
    ThemeMode mode_{ThemeMode::light};
};

} // namespace voelund::desktop
