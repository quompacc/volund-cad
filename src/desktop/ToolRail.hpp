#pragma once

#include "SketchToolActions.hpp"
#include "Theme.hpp"

#include <QFrame>

class QScrollArea;

namespace voelund::viewport {
class CadViewport;
}

namespace voelund::desktop {

class CommandActions;

// U04: the vertical tool rail at the viewport's left edge (ADR 0012). It shows every solid
// command; commands that do not fit the selection are disabled, never hidden. The rail can
// be switched off (view.tool_rail).
class ToolRail final : public QFrame {
  public:
    ToolRail(viewport::CadViewport& viewport, const CommandActions& commands);

    // U13: the rail shows the tools of the active function.
    void showSolid();
    void showSketch(const std::vector<SketchToolActions::Group>& groups);
    [[nodiscard]] bool showsSketch() const noexcept { return sketchShown_; }

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool event(QEvent* event) override;

  private:
    void place();
    void buildSketchPage(const std::vector<SketchToolActions::Group>& groups);

    viewport::CadViewport& viewport_;
    QWidget* solidPage_{nullptr};
    QScrollArea* sketchPage_{nullptr};
    bool sketchShown_{false};
};

// Icons of all commands that carry one, in the colors of the current theme.
void applyCommandIcons(QWidget& window, ThemeMode mode);

} // namespace voelund::desktop
