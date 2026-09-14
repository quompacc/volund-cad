#pragma once
#include <stdexcept>

namespace voelund::features {
// Expected failure of structurally valid, editable feature inputs.
// Infrastructure and programming errors must not derive from this type.
class GeometryEvaluationError final : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
} // namespace voelund::features
