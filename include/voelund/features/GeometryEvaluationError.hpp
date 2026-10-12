#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace voelund::features {
// Expected failure of structurally valid, editable feature inputs.
// Infrastructure and programming errors must not derive from this type.
class GeometryEvaluationError final : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
    // M03: a stable code (EvaluationIssue::code) for failures the shell explains itself.
    GeometryEvaluationError(const std::string& message, std::string code)
        : std::runtime_error(message), code_(std::move(code)) {}
    // M04: the sketch elements of the contour at fault, for the shell to mark.
    GeometryEvaluationError(const std::string& message, std::string code,
                            std::vector<std::uint64_t> elements)
        : std::runtime_error(message), code_(std::move(code)), elements_(std::move(elements)) {}
    [[nodiscard]] const std::string& code() const noexcept { return code_; }
    [[nodiscard]] const std::vector<std::uint64_t>& elements() const noexcept {
        return elements_;
    }

  private:
    std::string code_;
    std::vector<std::uint64_t> elements_;
};
} // namespace voelund::features
