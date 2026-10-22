// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// N02 (user decision 2026-10-21): a copied box pasted is a fixed copy "Kopie 1" of its own
// body; growing the original leaves the copy at 1000. Undo removes the paste; save/open keeps
// it; pasted into a new document it is there too. Two bodies pasted at once are "Kopie 1.1" and
// "Kopie 1.2". A body without a valid solid cannot be copied.
namespace {
using namespace voelund;
using application::CommandStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }

double bodyVolume(const application::CadSession& session, document::BodyId body) {
    for (const auto& item : session.validBodies())
        if (item.bodyId == body)
            return test::volumeOf(item.shape);
    return -1;
}
std::string bodyName(const application::CadSession& session, document::BodyId body) {
    return document::bodyAttributes(session.identifiedSnapshot().snapshot().bodyAttributes(),
                                    body.value())
        .name;
}

document::BodyId pastedBody(application::CadSession& session,
                            std::vector<document::ImportedSolid> solids) {
    const auto result = session.pasteBodies(std::move(solids));
    require(result.status() == CommandStatus::committed && result.feature().has_value(),
            "Pasted: " + result.diagnostic());
    return document::BodyId{result.feature()->value()};
}

void fixedCopy(const std::filesystem::path& directory) {
    application::CadSession session;
    session.newDocument();
    const auto box = session.createBoxFeature({10, 10, 10}).feature();
    require(box.has_value(), "Box");
    const document::BodyId original{box->value()};
    const auto copied = session.copyBodies(std::span{&original, 1});
    const auto copy = pastedBody(session, copied);
    require(session.validBodies().size() == 2 && near(bodyVolume(session, copy), 1000) &&
                bodyName(session, copy) == "Kopie 1",
            "Kopie 1 of 1000 beside the box");
    require(session.editBoxDimensions(*box, {20, 10, 10}).status() == CommandStatus::committed &&
                near(bodyVolume(session, original), 2000) && near(bodyVolume(session, copy), 1000),
            "The copy keeps its shape when the original grows");
    const auto path = directory / "Kopie Ä.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(near(bodyVolume(reopened, copy), 1000) && reopened.validBodies().size() == 2,
            "Save/open keeps the copy");
    require(session.undo().status() == CommandStatus::committed &&
                session.undo().status() == CommandStatus::committed &&
                session.validBodies().size() == 1,
            "Undo removes the paste");

    application::CadSession other;
    other.newDocument();
    const auto foreign = pastedBody(other, copied);
    require(other.validBodies().size() == 1 && near(bodyVolume(other, foreign), 1000),
            "Pasted into another document");
}

void twoAtOnce() {
    application::CadSession session;
    session.newDocument();
    const auto first = session.createBoxFeature({10, 10, 10}).feature();
    const auto second = session.createBoxFeature({5, 5, 5}).feature();
    require(first && second, "Two boxes");
    const std::vector<document::BodyId> bodies{document::BodyId{first->value()},
                                               document::BodyId{second->value()}};
    const auto pasted = session.pasteBodies(session.copyBodies(bodies));
    require(pasted.status() == CommandStatus::committed && session.validBodies().size() == 4,
            "Two bodies pasted in one step");
    const document::BodyId a{pasted.feature()->value()};
    require(bodyName(session, a) == "Kopie 1.1", "First named Kopie 1.1: " + bodyName(session, a));
    bool missing = false;
    try {
        static_cast<void>(session.copyBodies(std::vector{document::BodyId{999}}));
    } catch (const std::invalid_argument&) {
        missing = true;
    }
    require(missing, "A body without a valid solid is refused");
}

} // namespace

int main() {
    try {
        const auto directory = std::filesystem::temp_directory_path() / "voelund-clipboard-tests";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        fixedCopy(directory);
        twoAtOnce();
        std::filesystem::remove_all(directory);
        std::cout << "PASS N02 copy/paste: fixed copy, independent of the original, undo, "
                     "save/open, other document, two at once, invalid body refused\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
