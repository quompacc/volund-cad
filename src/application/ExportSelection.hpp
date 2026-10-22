// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/application/StepExchangeResult.hpp>
#include <voelund/document/BodyId.hpp>
#include <voelund/kernel/StepExchange.hpp>

#include <optional>
#include <span>
#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::application::detail {

// M17/N03: the solids an export writes: those of `bodies`, none given every visible current
// body, each named as the browser names it (a body of several solids numbers them). Or why
// there are none to write.
struct ExportSolids final {
    std::vector<kernel::StepSolid> solids;
    std::optional<StepExportResult> refusal;
};
[[nodiscard]] ExportSolids exportSolids(const CadSession& session,
                                        std::span<const document::BodyId> bodies);

} // namespace voelund::application::detail
