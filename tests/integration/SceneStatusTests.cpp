// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/features/FeatureEvaluationDetail.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <voelund/application/CadSession.hpp>

namespace {
using namespace voelund;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void requireNotCurrent(application::CadSession& session) {
    bool rejected = false;
    try {
        static_cast<void>(session.currentShape());
    } catch (const std::logic_error&) {
        rejected = true;
    }
    bool bodiesRejected = false;
    try {
        static_cast<void>(session.currentBodies());
    } catch (const std::logic_error&) {
        bodiesRejected = true;
    }
    require(rejected && bodiesRejected && !session.sceneStatus().isCurrent() &&
                !session.featureShape(document::ModelDocument::initialBoxId()) &&
                !session.canTransformFeature(document::ModelDocument::initialBoxId()) &&
                !session.canAddCylinder(),
            "stale display leaked into current geometry API");
}
struct Fixture {
    std::filesystem::path path;
    ~Fixture() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};
void run(const std::filesystem::path& root) {
    Fixture file{root / ("scene-status-" + std::to_string(std::random_device{}()) + ".volund")};
    application::CadSession session;
    const auto initial = session.sceneStatus();
    require(initial.isCurrent(), "initial scene has no current revision");
    const auto initialShape = session.currentShape();
    require(session.selectFeature(document::ModelDocument::initialCylinderId()), "select");
    auto parameters = session.selectedCylinderParameters();
    parameters.placement.translationZ = 10;
    require(session.editSelectedCylinderParameters(parameters).status() ==
                    application::CommandStatus::rejected &&
                session.sceneStatus().document == initial.document,
            "rejection advanced revision");
    require(session.editSelectedCylinderParameters(
                       parameters, application::ErrorDisposition::keepEditableErrors)
                    .status() == application::CommandStatus::committedWithErrors,
            "explicit error commit");
    const auto broken = session.sceneStatus();
    require(broken.document.revision == initial.document.revision + 1 &&
                broken.geometry == initial.geometry &&
                session.displayShape().IsSame(initialShape) && !session.displayBodies().empty(),
            "last valid scene lost revision provenance");
    requireNotCurrent(session);
    auto separate = parameters;
    separate.resultMode = document::CylinderResultMode::separate;
    require(session.editSelectedCylinderParameters(separate).reason() ==
                    application::RejectionReason::incompleteEvaluation &&
                session.sceneStatus().document == broken.document,
            "stale input was used to derive a cylinder placement");
    session.saveDocument(file.path);
    require(!session.isModified() && session.sceneStatus().geometry == broken.geometry,
            "save silently repaired stale scene");
    application::CadSession loaded;
    loaded.loadDocument(file.path);
    require(!loaded.isModified() && !loaded.sceneStatus().geometry &&
                loaded.displayShape().IsNull(),
            "load reused another document's geometry");
    requireNotCurrent(loaded);
    parameters.placement.translationZ = 0;
    require(loaded.editSelectedCylinderParameters(parameters).status() ==
                    application::CommandStatus::committed &&
                loaded.sceneStatus().isCurrent(),
            "repair did not advance geometry revision");
    const auto repaired = loaded.sceneStatus();
    require((loaded.undo().status() == voelund::application::CommandStatus::committedWithErrors) &&
                !loaded.sceneStatus().isCurrent() &&
                loaded.sceneStatus().geometry == repaired.geometry,
            "undo error projection");
    requireNotCurrent(loaded);
    require((loaded.redo().status() == voelund::application::CommandStatus::committed) &&
                loaded.sceneStatus().isCurrent(),
            "redo repaired projection");
    loaded.saveDocument(file.path);
    const auto beforeLoad = session.sceneStatus();
    const auto beforeFile = session.currentFile();
    const auto previousHook = features::detail::beforeFeatureEvaluation;
    features::detail::beforeFeatureEvaluation = [](document::FeatureId) { throw std::bad_alloc{}; };
    bool failed = false;
    try {
        session.loadDocument(file.path);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    features::detail::beforeFeatureEvaluation = previousHook;
    require(failed && session.sceneStatus().document == beforeLoad.document &&
                session.sceneStatus().geometry == beforeLoad.geometry &&
                session.currentFile() == beforeFile && session.displayShape().IsSame(initialShape),
            "failed open changed projection/generation");
    session.loadDocument(file.path);
    require(session.sceneStatus().isCurrent() &&
                session.sceneStatus().document.generation == beforeLoad.document.generation + 1,
            "open did not replace document generation");
    const auto beforeNew = session.sceneStatus();
    session.newDocument();
    require(session.sceneStatus().isCurrent() && session.currentShape().IsNull() &&
                session.sceneStatus().document.generation == beforeNew.document.generation + 1 &&
                session.currentFile().empty() && !session.isModified(),
            "new document projection");
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 2, "fixture directory required");
        run(argv[1]);
        std::cout << "Scene revision, display isolation, history and persistence tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        features::detail::beforeFeatureEvaluation = nullptr;
        std::cerr << error.what() << '\n';
        return 1;
    }
}
