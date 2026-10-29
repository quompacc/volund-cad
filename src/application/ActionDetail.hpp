// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <voelund/application/Actions.hpp>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSnapshot.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace voelund::application::detail {
// The single capability rule shared by the context and the validation of requests.
// Feature targets also depend on the current results (cylinder input, placement).
[[nodiscard]] std::vector<ActionKind> availableActions(const CadSession& session,
                                                       const document::DocumentSnapshot& snapshot,
                                                       const ActionTarget& target);
// First finding of every check category; the AI01 order decides between categories.
struct Findings final {
    std::optional<std::string> unknownTarget;
    std::optional<std::string> unsupportedTarget;
    std::optional<std::string> invalidUnit;
    std::optional<std::string> nonFinite;
    std::optional<std::string> unresolvedReference;

    static void note(std::optional<std::string>& category, std::string message) {
        if (!category)
            category = std::move(message);
    }
    [[nodiscard]] std::optional<std::pair<ActionRejection, std::string>> first() const {
        const std::pair<ActionRejection, const std::optional<std::string>*> ordered[] = {
            {ActionRejection::unknownTarget, &unknownTarget},
            {ActionRejection::unsupportedTarget, &unsupportedTarget},
            {ActionRejection::invalidUnit, &invalidUnit},
            {ActionRejection::nonFinite, &nonFinite},
            {ActionRejection::unresolvedReference, &unresolvedReference}};
        for (const auto& [rejection, finding] : ordered)
            if (*finding)
                return std::pair{rejection, **finding};
        return std::nullopt;
    }
};
// The actions the session takes as they are.
inline CommandResult runWhole(CadSession& session, const CreateMirrorAction& a) {
    return session.createMirror(a);
}
inline CommandResult runWhole(CadSession& session, const CreateSplitAction& a) {
    return session.createSplit(a);
}
inline CommandResult runWhole(CadSession& session, const EditSplitParametersAction& a) {
    return session.editSplitParameters(a);
}
inline CommandResult runWhole(CadSession& session, const CreateFaceEditAction& a) {
    return session.createFaceEdit(a);
}
inline CommandResult runWhole(CadSession& session, const EditFaceEditParametersAction& a) {
    return session.editFaceEdit(a);
}
template <class A>
concept WholeAction = requires(CadSession& session, const A& action) { runWhole(session, action); };
} // namespace voelund::application::detail
