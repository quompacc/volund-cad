// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <BRepClass3d_SolidClassifier.hxx>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// N12a (schema 50, packet acceptance): a box 20 x 20 x 10 split at the construction plane
// x = 8 keeping both sides is two bodies of 2400 (front, x > 8, the box's own body) and 1600
// ("Teilen 1 Teil 2"), together the box. Keeping the front only drops the part; the back only
// keeps 1600. The XY base plane under the box does not cut it and is refused. Undo and
// Save/Open keep the split and its part.
namespace {
using namespace voelund;
using application::CommandStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
double volume(const application::CadSession& session) {
    double total = 0;
    for (const auto& body : session.validBodies())
        total += test::volumeOf(body.shape);
    return total;
}
double volumeOf(const application::CadSession& session, document::BodyId body) {
    for (const auto& valid : session.validBodies())
        if (valid.bodyId == body)
            return test::volumeOf(valid.shape);
    return 0;
}

struct Box final {
    application::CadSession session;
    document::FeatureId box{1};
    document::FeatureId plane{1};
};
void make(Box& made) {
    made.session.newDocument();
    made.box = *made.session.createBoxFeature({20, 20, 10}).feature();
    made.plane = *made.session
                      .createDatumPlane(document::DatumPlane::yz, std::nullopt, std::nullopt, 8,
                                        document::DatumRotationAxis::none, 0)
                      .feature();
}
application::CommandResult split(Box& made, document::SplitKeep keep,
                                 std::optional<document::DatumPlane> base = std::nullopt) {
    application::CreateSplitAction action{made.box,     base,         std::nullopt,
                                          std::nullopt, std::nullopt, keep};
    if (!base)
        action.constructionPlane = made.plane;
    return made.session.createSplit(action);
}

void keepOneSide(application::CadSession& session, document::FeatureId splitId);

void both() {
    Box made;
    make(made);
    auto& session = made.session;
    require(split(made, document::SplitKeep::both).status() == CommandStatus::committed &&
                session.validBodies().size() == 2 && near(volume(session), 4000) &&
                near(volumeOf(session, document::BodyId{made.box.value()}), 2400) &&
                session.features().back().name == "Teilen 1 Teil 2",
            "Both sides: 2400 on the box's body, 1600 as Teil 2");
    const auto splitId = session.features()[2].id;
    const auto path = test::temporaryFilePath("voelund-n12a-split.volund");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    require(loaded.validBodies().size() == 2 && near(volume(loaded), 4000) &&
                loaded.identifiedSnapshot().snapshot().splitParameters(splitId) ==
                    session.identifiedSnapshot().snapshot().splitParameters(splitId),
            "Save/Open keeps the split and its part");
    keepOneSide(session, splitId);
}

// The front only drops the part, the back only keeps 1600; undo twice restores both.
void keepOneSide(application::CadSession& session, document::FeatureId splitId) {
    auto front = session.identifiedSnapshot().snapshot().splitParameters(splitId);
    front.keep = document::SplitKeep::front;
    require(session.editSplitParameters(application::EditSplitParametersAction{splitId, front})
                        .status() == CommandStatus::committed &&
                session.validBodies().size() == 1 && near(volume(session), 2400) &&
                session.features().size() == 3,
            "The front only: the part goes");
    auto back = front;
    back.keep = document::SplitKeep::back;
    require(session.editSplitParameters(application::EditSplitParametersAction{splitId, back})
                        .status() == CommandStatus::committed &&
                near(volume(session), 1600),
            "The back only: 1600");
    require(session.undo().status() == CommandStatus::committed &&
                session.undo().status() == CommandStatus::committed &&
                session.validBodies().size() == 2 && near(volume(session), 4000),
            "Undo twice brings both sides back");
}

void misses() {
    Box made;
    make(made);
    const auto refused = split(made, document::SplitKeep::both, document::DatumPlane::xy);
    require(refused.status() != CommandStatus::committed && made.session.features().size() == 2,
            "The XY plane under the box does not cut it: " + refused.diagnostic());
}
} // namespace

int main() {
    try {
        both();
        misses();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS N12a split: both sides, front, back, refused plane, undo, open\n";
    return 0;
}
