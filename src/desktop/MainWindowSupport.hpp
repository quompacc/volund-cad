// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/application/CadSession.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QObject>
#include <QString>
#include <QVariant>

#include <cstdint>
#include <optional>
#include <set>
#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

// M01: a body's name in the shell: the user's name, else "Körper N" by its position.
[[nodiscard]] QString bodyLabel(const application::ModelOutline& outline, document::BodyId body);

// Bodies listed in hiddenBodies (model tree eyes, U06) are left out of the scene.
[[nodiscard]] std::vector<viewport::SceneBody>
makeSceneBodies(const application::CadSession& session,
                const std::set<std::uint64_t>& hiddenBodies = {});
// M19: the session's current construction planes and axes.
[[nodiscard]] std::vector<viewport::SceneDatum>
makeSceneDatums(const application::CadSession& session, const std::set<std::uint64_t>& hidden = {});
[[nodiscard]] int resultModeIndex(document::CylinderResultMode mode);
[[nodiscard]] document::CylinderResultMode resultModeFromIndex(int index);
// Every user-visible action carries a stable command ID (U01 command table in
// docs/ui-concept.md); registry commands use their registered ID.
inline constexpr const char* commandIdProperty = "commandId";
inline void setCommandId(QObject& action, const char* id) {
    action.setProperty(commandIdProperty, QString::fromLatin1(id));
}
[[nodiscard]] bool selectTransformFeatureForBody(application::CadSession& session,
                                                 document::FeatureId bodyId);

} // namespace voelund::desktop
