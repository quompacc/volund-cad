#pragma once

#include "SketchToolActions.hpp"
#include "Theme.hpp"

#include <QFrame>

#include <vector>

class QScrollArea;
class QToolButton;

namespace voelund::viewport {
class CadViewport;
}

namespace voelund::desktop {

class CommandActions;

// U04: the vertical tool rail at the viewport's left edge (ADR 0012). It shows every solid
// command; commands that do not fit the selection are disabled, never hidden. The rail can
// be switched off (view.tool_rail).
// U14 (user decision 2026-10-14): a slim column of groups (Erstellen, Skizze, Bearbeiten,
// Bewegen, Mehr) right of the model tree; a group opens its commands in a flyout beside the
// column, a command closes it again.
class ToolRail final : public QFrame {
  public:
    ToolRail(viewport::CadViewport& viewport, const CommandActions& commands);

    // U13: the rail shows the tools of the active function.
    void showSolid();
    void showSketch(const std::vector<SketchToolActions::Section>& sections);
    [[nodiscard]] bool showsSketch() const noexcept { return sketchShown_; }
    void closeFlyout();

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool event(QEvent* event) override;

  private:
    void place();
    void openFlyout(int group);
    void buildSketchPage(const std::vector<SketchToolActions::Section>& sections);

    viewport::CadViewport& viewport_;
    QFrame* column_{nullptr};
    QWidget* solidPage_{nullptr};
    QFrame* flyout_{nullptr};
    std::vector<QToolButton*> groupButtons_;
    std::vector<QWidget*> flyoutPages_;
    int openGroup_{-1};
    QScrollArea* sketchPage_{nullptr};
    bool sketchShown_{false};
};

// Icons of all commands that carry one, in the colors of the current theme.
void applyCommandIcons(QWidget& window, ThemeMode mode);

} // namespace voelund::desktop
