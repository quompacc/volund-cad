#pragma once

#include <QString>

#include <vector>

class QAction;
class QActionGroup;
class QWidget;

namespace voelund::desktop {

class ToolPanelHost;

// U13: the sketch editor's drawing tools and actions as window-owned QActions with command
// IDs (sketch.tool.<tool>, sketch.<action>), so the tool rail, the command search and
// shortcuts use the same way in. The controller keeps the rules; these only forward to the
// active tool session and mirror its availability.
class SketchToolActions final {
  public:
    struct Group final {
        QString title;
        std::vector<QAction*> actions;
    };

    SketchToolActions(QWidget& window, ToolPanelHost& host);

    // Builds the actions on the first sketch session and mirrors enabled/checked states.
    void sync();
    [[nodiscard]] bool sketchActive() const;
    [[nodiscard]] const std::vector<Group>& groups() const noexcept { return groups_; }

  private:
    void build();

    QWidget& window_;
    ToolPanelHost& host_;
    QActionGroup* tools_{nullptr};
    std::vector<Group> groups_;
};

} // namespace voelund::desktop
