#pragma once

#include <voelund/workspaces/WorkspaceModule.hpp>

#include <functional>
#include <vector>

namespace voelund::modules::solid {

struct SolidCommandServices final {
    std::function<void()> addBox;
    std::function<void()> addCylinder;
    std::function<void()> sketchEditor;
    std::function<void()> sketchRectangle;
    std::function<void()> editSketch;
    std::function<void(bool)> transform;
    std::function<bool()> canAddCylinder;
    std::function<bool()> canEditSketch;
    std::function<bool()> canSketch;
    std::function<bool()> canEditExistingSketch;
    std::function<bool()> canTransform;
};

class SolidWorkspaceModule final : public workspaces::WorkspaceModule {
public:
    explicit SolidWorkspaceModule(SolidCommandServices services);
    [[nodiscard]] std::string id() const override;
    [[nodiscard]] std::string title() const override;
    [[nodiscard]] std::span<const workspaces::CommandDescriptor> commands() const override;

private:
    std::vector<workspaces::CommandDescriptor> commands_;
};

} // namespace voelund::modules::solid
