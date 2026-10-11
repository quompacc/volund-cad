#pragma once
#include <stdexcept>
#include <string>

namespace voelund::features {
// Expected failure of structurally valid, editable feature inputs.
// Infrastructure and programming errors must not derive from this type.
class GeometryEvaluationError final : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
    // M03: a stable code (EvaluationIssue::code) for failures the shell explains itself.
    GeometryEvaluationError(const std::string& message, std::string code)
        : std::runtime_error(message), code_(std::move(code)) {}
    [[nodiscard]] const std::string& code() const noexcept { return code_; }

  private:
    std::string code_;
};
} // namespace voelund::features
