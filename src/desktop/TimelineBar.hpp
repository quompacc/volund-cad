#pragma once

#include "Theme.hpp"

#include <voelund/document/ModelDocument.hpp>

#include <QFrame>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

class QHBoxLayout;
class QLabel;
class QToolButton;

namespace voelund::desktop {

// U06: the timeline at the bottom of the window (ADR 0012). It shows every feature in
// document order; a click opens the feature (sketch editor for sketches, properties for the
// others). Rollback, reordering and error marks follow in M15.
class TimelineBar final : public QFrame {
  public:
    explicit TimelineBar(QWidget* parent = nullptr);

    void setOpenHandler(std::function<void(std::uint64_t)> handler);
    void showFeatures(std::span<const document::FeatureDescriptor> features,
                      std::optional<document::FeatureId> selected);
    void setTheme(ThemeMode mode);

  private:
    QHBoxLayout* steps_{nullptr};
    QLabel* empty_{nullptr};
    QLabel* end_{nullptr};
    std::vector<QToolButton*> buttons_;
    std::vector<document::FeatureKind> kinds_;
    std::function<void(std::uint64_t)> open_;
    ThemeMode mode_{ThemeMode::light};
};

} // namespace voelund::desktop
