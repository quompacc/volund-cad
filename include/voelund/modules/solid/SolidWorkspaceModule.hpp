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
    // T04c: push/pull of the picked planar face; absent services publish no command.
    std::function<void()> pushPull{};
    std::function<bool()> canPushPull{};
    // M06: rounds edges of the selected body; absent services publish no command.
    std::function<void()> fillet{};
    std::function<bool()> canFillet{};
    // M07: a symmetric chamfer of edges of the selected body, enabled like the fillet.
    std::function<void()> chamfer{};
    // M08: a hole at the clicked point of the picked planar face.
    std::function<void()> hole{};
    std::function<bool()> canHole{};
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
