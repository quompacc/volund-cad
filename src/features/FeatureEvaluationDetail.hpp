#pragma once
#include <voelund/document/FeatureId.hpp>

namespace voelund::features::detail {
// Private deterministic fault seam. No product configuration enables it.
// Thread-local so one test's evaluation cannot affect another thread.
inline thread_local void (*beforeFeatureEvaluation)(document::FeatureId) = nullptr;
} // namespace voelund::features::detail
