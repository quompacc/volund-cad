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
    // M22: walls with the picked planar face opened; enabled like the hole.
    std::function<void()> shell{};
    // M21: combines the selected body with clicked tool bodies; enabled like the fillet.
    std::function<void()> combine{};
    // M09: mirrors the selected body (about the picked planar face, if any).
    std::function<void()> mirror{};
    std::function<bool()> canMirror{};
    // M10: linear and circular patterns of the selected body.
    std::function<void()> patternLinear{};
    std::function<void()> patternCircular{};
    std::function<bool()> canPattern{};
    // M20: turns the selected sketch regions about an axis; enabled like the extrusion.
    std::function<void()> revolve{};
    // M23: sweeps the selected sketch regions along an earlier sketch; enabled like the extrusion.
    std::function<void()> sweep{};
    // M24: lofts the one selected region to another sketch; enabled like the extrusion.
    std::function<void()> loft{};
    // M26: a helix of the selected region about an axis; enabled like the extrusion.
    std::function<void()> helix{};
    // M25: a draft about the picked planar face; enabled like the hole.
    std::function<void()> draft{};
    // M19: construction planes and axes.
    std::function<void()> datumPlane{};
    std::function<void()> datumAxis{};
    std::function<bool()> canDatum{};
    // M27: sphere (0), cone (1), torus (2) and wedge (3), enabled like the cylinder.
    std::function<void(int)> primitive{};
    std::function<bool()> canPrimitive{};
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
