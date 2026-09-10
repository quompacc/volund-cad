#pragma once

#include <voelund/workspaces/WorkspaceModule.hpp>

#include <memory>
#include <string_view>
#include <unordered_map>

class QAction;
class QMainWindow;

namespace voelund::desktop {

class WorkspaceToolbar final {
public:
    WorkspaceToolbar(QMainWindow& window, const workspaces::WorkspaceModule& module);
    [[nodiscard]] QAction* action(std::string_view commandId) const;
    void refreshEnabled();

private:
    const workspaces::WorkspaceModule& module_;
    std::unordered_map<std::string, QAction*> actions_;
};

} // namespace voelund::desktop
