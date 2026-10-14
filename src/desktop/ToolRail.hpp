#pragma once

#include "SketchToolActions.hpp"
#include "Theme.hpp"

#include <QFrame>

#include <utility>
#include <vector>

class QScrollArea;
class QToolButton;

namespace voelund::viewport {
class CadViewport;
}

namespace voelund::desktop {

class CommandActions;

// U04: the tool rail (ADR 0012). It shows every solid command; commands that do not fit the
// selection are disabled, never hidden. The rail can be switched off (view.tool_rail).
// U14 (user decisions 2026-10-14): a bar at the top centre, between the project card and the
// header card. Outside a sketch it shows the groups (Erstellen, Skizze, Bearbeiten, Bewegen,
// Mehr) as pills; a group opens its commands in a flyout below it, a command closes it again.
// In a sketch it shows the sketch tools in a row, one button per family (U13, S14).
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
    // The free stretch of the top row between the project card and the header card.
    [[nodiscard]] std::pair<int, int> freeRow();

    viewport::CadViewport& viewport_;
    QFrame* bar_{nullptr};
    QScrollArea* scroll_{nullptr};
    QWidget* solidPage_{nullptr};
    QWidget* sketchPage_{nullptr};
    QFrame* flyout_{nullptr};
    std::vector<QToolButton*> groupButtons_;
    std::vector<QWidget*> flyoutPages_;
    std::vector<QWidget*> watchedCards_;
    int openGroup_{-1};
    bool sketchShown_{false};
    bool placing_{false};
};

// Icons of all commands that carry one, in the colors of the current theme.
void applyCommandIcons(QWidget& window, ThemeMode mode);

} // namespace voelund::desktop
