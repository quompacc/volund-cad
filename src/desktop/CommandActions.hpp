#pragma once

#include <voelund/workspaces/WorkspaceModule.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

class QAction;
class QWidget;

namespace voelund::desktop {

// U04: one QAction per registered command, owned by the window so shortcuts work. The tool
// rail, the context bar and the command search all show these same actions; none of them
// creates its own (ADR 0012).
class CommandActions final {
  public:
    CommandActions(QWidget& window, const workspaces::WorkspaceModule& module);

    [[nodiscard]] QAction* action(std::string_view commandId) const;
    [[nodiscard]] const std::vector<QAction*>& ordered() const noexcept { return ordered_; }
    [[nodiscard]] workspaces::CommandGroup group(const QAction& action) const;
    void refreshEnabled();

  private:
    const workspaces::WorkspaceModule& module_;
    std::map<std::string, QAction*, std::less<>> actions_;
    std::vector<QAction*> ordered_;
};

} // namespace voelund::desktop
