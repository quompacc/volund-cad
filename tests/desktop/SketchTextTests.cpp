// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/desktop/SketchText.hpp"
#include "../../src/desktop/Theme.hpp"
#include "IsolatedSettings.hpp"

#include <QApplication>

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/kernel/MassProperties.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

// N15c (packet acceptance): the text "Völund", capital letters 10 mm high in the bundled font,
// becomes sketch curves on its baseline; the "d", drawn by the variable font as one contour
// crossing itself, comes out as a clean outline. Engraved 1 mm deep into the bottom of a plate
// it leaves one valid body that lost the letters' volume (the counters of "ö", "d" stay).
namespace {
using namespace voelund;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

double volume(const application::CadSession& session) {
    double sum = 0.0;
    for (const auto& body : session.validBodies())
        sum += kernel::massProperties(body.shape).volume;
    return sum;
}

void run() {
    const auto curves =
        desktop::sketchTextCurves(QStringLiteral("Völund"), desktop::uiFontFamily(), 10, {5, 10});
    require(curves.size() > 20, "The outline has curves: " + std::to_string(curves.size()));
    double low = 1e9, high = -1e9;
    for (const auto& curve : curves)
        for (const auto end :
             {document::SketchPointPosition::start, document::SketchPointPosition::end})
            if (const auto point = document::sketchReferencePoint(curve, end)) {
                low = std::min(low, point->y);
                high = std::max(high, point->y);
            }
    require(low > 9.5 && low < 10.5 && high > 19.5 && high < 22,
            "On the baseline y = 10, capitals up to y = 20 (ascenders a little higher): " +
                std::to_string(low) + " .. " + std::to_string(high));

    application::CadSession session;
    session.newDocument();
    require(session.createBoxFeature({80, 30, 10}).status() ==
                application::CommandStatus::committed,
            "A plate 80 x 30 x 10");
    const double before = volume(session);
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    require(editor.addTextOutline(curves) == curves.size(), "The text in the sketch");
    application::ExtrusionMode engrave;
    engrave.operation = document::ExtrusionOperation::cut;
    engrave.target = session.modelOutline().bodies.front().id;
    const auto result =
        session.createDatumSketchExtrusion(editor.draftParameters(), {}, 1.0, engrave);
    require(result.status() == application::CommandStatus::committed,
            "Engraved 1 mm deep: " + result.diagnostic());
    const double removed = before - volume(session);
    require(session.validBodies().size() == 1 && removed > 20 && removed < 400,
            "One valid body without the letters: " + std::to_string(removed) + " mm3 removed");
    std::cout << "PASS N15c text engraved, " << curves.size() << " curves, " << removed << " mm3\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
