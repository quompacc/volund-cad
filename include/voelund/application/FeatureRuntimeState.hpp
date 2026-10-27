// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <string>

namespace voelund::application {
// M14: suppressed is no error; the feature is skipped on purpose.
enum class FeatureRuntimeStatus { current, error, blocked, suppressed, rolledBack };

struct FeatureRuntimeState final {
    document::FeatureId id;
    FeatureRuntimeStatus status;
    std::string message;
    std::string code{}; // U15: features::EvaluationIssue::code, for the shell's own wording
};
} // namespace voelund::application
