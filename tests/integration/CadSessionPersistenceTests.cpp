// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../BoundSketchFixture.hpp"
#include "../CommandResultAssertions.hpp"
#include "CadSessionTestCases.hpp"
#include "CadSessionTestSupport.hpp"

namespace voelund::test {

bool sessionFileRoundtripRestoresTheDocument() {
    const std::filesystem::path path =
        voelund::test::temporaryFilePath("voelund-session-roundtrip.volund");
    voelund::application::CadSession session;
    if (session.isModified()) {
        return false;
    }
    const voelund::document::FeatureId boxId =
        voelund::test::requireCommittedFeature(session.createBoxFeature());
    voelund::kernel::RigidTransform placement = session.selectedFeaturePlacement();
    placement.translationX = 37.0;
    static_cast<void>((session.editSelectedFeaturePlacement(placement).status() ==
                       voelund::application::CommandStatus::committed));
    if (!session.isModified()) {
        return false;
    }
    session.saveDocument(path);
    if (session.isModified()) {
        return false;
    }
    voelund::kernel::RigidTransform changedPlacement = placement;
    changedPlacement.translationX = 38.0;
    static_cast<void>((session.editSelectedFeaturePlacement(changedPlacement).status() ==
                       voelund::application::CommandStatus::committed));
    if (!session.isModified() ||
        !(session.undo().status() == voelund::application::CommandStatus::committed) ||
        session.isModified()) {
        return false;
    }
    static_cast<void>(voelund::test::requireCommittedFeature(session.createCylinderFeature()));
    if (!session.isModified()) {
        return false;
    }
    session.loadDocument(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    return !error && !session.isModified() && session.features().size() == 3 &&
           session.selectedFeature() == boxId && session.selectedFeaturePlacement() == placement &&
           !session.canUndo() && !session.canRedo();
}

bool multipleSketchElementsExtrudeAllClosedProfiles() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto boxId = voelund::test::requireCommittedFeature(session.createBoxFeature());
    const double boxVolume = volumeOf(session.currentShape());
    const voelund::document::SketchPlane plane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    const std::vector<voelund::document::SketchElement> elements{
        {voelund::document::SketchElementId{1},
         voelund::document::SketchRectangle{2.0, 3.0, 8.0, 9.0}},
        {voelund::document::SketchElementId{2}, voelund::document::SketchCircle{{14.0, 8.0}, 2.0}},
        {voelund::document::SketchElementId{3},
         voelund::document::SketchLine{{10.0, 15.0}, {14.0, 15.0}}},
        {voelund::document::SketchElementId{4},
         voelund::document::SketchLine{{14.0, 15.0}, {10.0, 19.0}}},
        {voelund::document::SketchElementId{5},
         voelund::document::SketchLine{{10.0, 19.0}, {10.0, 15.0}}},
        {voelund::document::SketchElementId{6},
         voelund::document::SketchLine{{17.0, 17.0}, {19.0, 17.0}}}};
    const voelund::document::SketchParameters sketch{
        plane, voelund::document::SketchElements{elements, 7}};
    const auto extrusionId =
        voelund::test::requireCommittedFeature(session.createSketchExtrusion(boxId, sketch, 10.0));
    const double expected = boxVolume + (36.0 + std::numbers::pi * 4.0 + 8.0) * 10.0;
    if (session.featureStates().back().status !=
            voelund::application::FeatureRuntimeStatus::current ||
        std::abs(volumeOf(session.currentShape()) - expected) >= 1.0e-4) {
        return false;
    }
    const auto sketchId = session.features()[1].id;
    bindTop(session, sketchId);
    if (!session.selectFeature(sketchId)) {
        return false;
    }
    const auto evaluatedElements = session.selectedSketchElements();
    if (evaluatedElements.size() != elements.size() ||
        std::ranges::any_of(evaluatedElements,
                            [](const auto& element) { return element.shape.IsNull(); }) ||
        !session.selectSketchElement(voelund::document::SketchElementId{4}) ||
        session.selectedSketchElement() != voelund::document::SketchElementId{4} ||
        session.selectSketchElement(voelund::document::SketchElementId{99}) ||
        !session.selectFeature(extrusionId) || session.selectedSketchElement().has_value()) {
        return false;
    }
    const std::filesystem::path path =
        voelund::test::temporaryFilePath("voelund-multi-sketch-roundtrip.volund");
    session.saveDocument(path);
    voelund::application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    return !error && loaded.features().size() == 3 &&
           std::abs(volumeOf(loaded.currentShape()) - expected) < 1.0e-4;
}

bool selectedSketchProfilesExtrudeAndRoundtrip() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    const auto boxId = voelund::test::requireCommittedFeature(session.createBoxFeature());
    const double boxVolume = volumeOf(session.currentShape());
    const document::SketchPlane plane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    const std::vector<document::SketchElement> elements{
        {document::SketchElementId{1}, document::SketchRectangle{2.0, 3.0, 8.0, 9.0}},
        {document::SketchElementId{2}, document::SketchCircle{{14.0, 8.0}, 2.0}},
        {document::SketchElementId{3}, document::SketchLine{{10.0, 15.0}, {14.0, 15.0}}},
        {document::SketchElementId{4}, document::SketchLine{{14.0, 15.0}, {10.0, 19.0}}},
        {document::SketchElementId{5}, document::SketchLine{{10.0, 19.0}, {10.0, 15.0}}},
        {document::SketchElementId{6}, document::SketchLine{{17.0, 17.0}, {19.0, 17.0}}}};
    const auto sketchId = voelund::test::requireCommittedFeature(session.createSketchFeature(
        boxId, document::SketchParameters{plane, document::SketchElements{elements, 7}}));
    bindTop(session, sketchId);
    if (!session.selectSketchProfileByElement(document::SketchElementId{4}, false) ||
        !session.selectSketchProfileByElement(document::SketchElementId{2}, true) ||
        session.selectSketchProfileByElement(document::SketchElementId{6}, true) ||
        session.selectedSketchProfiles().size() != 2 ||
        session.selectedSketchProfileElements().size() != 4 ||
        !session.canExtrudeSelectedSketchProfiles())
        return false;

    const std::vector<document::SketchRegionId> selected(session.selectedSketchProfiles().begin(),
                                                         session.selectedSketchProfiles().end());
    const auto extrusionId = voelund::test::requireCommittedFeature(
        session.createSketchProfileExtrusion(sketchId, selected, 10.0));
    const double expected = boxVolume + (8.0 + std::numbers::pi * 4.0) * 10.0;
    if (session.selectedFeature() != extrusionId ||
        session.selectedExtrusionParameters().regions != selected ||
        std::abs(volumeOf(session.currentShape()) - expected) >= 1.0e-4 ||
        !(session.undo().status() == voelund::application::CommandStatus::committed) ||
        std::abs(volumeOf(session.currentShape()) - boxVolume) >= 1.0e-4 ||
        !(session.redo().status() == voelund::application::CommandStatus::committed) ||
        std::abs(volumeOf(session.currentShape()) - expected) >= 1.0e-4)
        return false;

    const std::filesystem::path path =
        voelund::test::temporaryFilePath("voelund-selected-profiles-roundtrip.volund");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    return !error && loaded.selectedExtrusionParameters().regions == selected &&
           std::abs(volumeOf(loaded.currentShape()) - expected) < 1.0e-4;
}

} // namespace voelund::test
