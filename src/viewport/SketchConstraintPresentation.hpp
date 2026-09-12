#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <AIS_TextLabel.hxx>

#include <optional>
#include <span>
#include <vector>

namespace voelund::viewport {

class SketchConstraintPresentation final {
  public:
    void setConstraints(const occ::handle<AIS_InteractiveContext>& context,
                        std::span<const SceneSketchConstraint> constraints);
    void clear(const occ::handle<AIS_InteractiveContext>& context);
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::optional<std::uint64_t>
    detectedConstraint(const occ::handle<AIS_InteractiveObject>& detected) const noexcept;

  private:
    struct ConstraintPresentation final {
        std::uint64_t id;
        occ::handle<AIS_TextLabel> label;
        bool editable;
    };

    std::vector<ConstraintPresentation> constraints_;
};

} // namespace voelund::viewport
