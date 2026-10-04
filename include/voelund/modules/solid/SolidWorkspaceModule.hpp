#pragma once

#include <voelund/workspaces/WorkspaceModule.hpp>

#include <functional>
#include <vector>

namespace voelund::modules::solid {

struct SolidCommandServices final {
    std::function<void()> addBox;
    std::function<void()> addCylinder;
    std::function<void()> sketchEditor;
    std::function<void()> editSketch;
    std::function<void()> extrudeProfiles;
    std::function<void(bool)> transform;
    std::function<bool()> canAddCylinder;
    std::function<bool()> canEditSketch;
    std::function<bool()> canEditExistingSketch;
    std::function<bool()> canExtrudeProfiles;
    std::function<bool()> canTransform;
    // Optional; primitives are always available when empty (U05 blocks them during a tool).
    std::function<bool()> canAddBox{};
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
