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
    // S14 (user decision 2026-10-10): the rail shows one button per family; its arrow opens
    // the other variants (two rectangles, line and polyline, …).
    struct Section final {
        QString title;
        std::vector<std::vector<QAction*>> families;
    };

    SketchToolActions(QWidget& window, ToolPanelHost& host);

    // Builds the actions on the first sketch session and mirrors enabled/checked states.
    void sync();
    [[nodiscard]] bool sketchActive() const;
    [[nodiscard]] const std::vector<Group>& groups() const noexcept { return groups_; }
    // The rail's arrangement of the same actions. Commands without a place in the rail
    // (sketch undo/redo: Ctrl+Z/Ctrl+Y and the command search) are left out.
    [[nodiscard]] std::vector<Section> sections() const;

  private:
    void build();

    QWidget& window_;
    ToolPanelHost& host_;
    QActionGroup* tools_{nullptr};
    std::vector<Group> groups_;
};

} // namespace voelund::desktop
