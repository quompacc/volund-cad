// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

// User report 2026-10-25 (performance): where the time goes in the window when a step is added
// to a saved model. By hand, under a Wayland session:
//
//   VOELUND_PROBE_MODEL=/path/model.volund voelund_desktop_model_probe
//
// Times opening, a scene refresh, a step taken away and brought back (model, refresh and
// drawing apart) and the pointer moving over the model.

#include "../desktop/IsolatedSettings.hpp"
#include "../desktop/ViewportLifecycleSupport.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

#include <QApplication>
#include <QElapsedTimer>

#include <cstdlib>
#include <functional>
#include <iostream>

namespace {
using namespace voelund;

double milliseconds(const std::function<void()>& action) {
    QElapsedTimer timer;
    timer.start();
    action();
    return static_cast<double>(timer.nsecsElapsed()) / 1e6;
}
void settle() {
    for (int i = 0; i < 5; ++i)
        QApplication::processEvents();
}

void run(const char* model) {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1500, 900);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    test::render(*viewport);
    std::cout << "open " << milliseconds([&] {
        window.openFile(QString::fromLocal8Bit(model));
        settle();
        test::render(*viewport);
    }) << " ms\n";
    std::cout << "scene refresh " << milliseconds([&] {
        window.refreshAfterExternalChange();
        settle();
    }) << " ms\n";
    std::cout << "draw " << milliseconds([&] { test::render(*viewport); }) << " ms\n";
    const auto last = session.features().back().id;
    for (const bool suppressed : {true, false}) {
        std::cout << (suppressed ? "take the last step away" : "bring it back") << ": model "
                  << milliseconds(
                         [&] { static_cast<void>(session.setFeatureSuppressed(last, suppressed)); })
                  << " ms, refresh " << milliseconds([&] {
                         window.refreshAfterExternalChange();
                         settle();
                     })
                  << " ms, draw " << milliseconds([&] { test::render(*viewport); }) << " ms\n";
    }
    std::cout << "pointer over the model, 40 moves " << milliseconds([&] {
        for (int step = 0; step < 40; ++step) {
            const QPoint at(viewport->width() / 4 + step * viewport->width() / 80,
                            viewport->height() / 2);
            test::mouse(*viewport, QEvent::MouseMove, at, Qt::NoButton, Qt::NoButton);
            QApplication::processEvents();
        }
        test::render(*viewport);
    }) << " ms\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    const char* model = std::getenv("VOELUND_PROBE_MODEL");
    if (model == nullptr) {
        std::cout << "SKIP: VOELUND_PROBE_MODEL names no model\n";
        return 0;
    }
    try {
        run(model);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
