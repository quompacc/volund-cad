// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

// User report 2026-10-25 (performance): what each step of a saved model costs when it is added.
//
//   voelund_model_probe <file.volund>
//
// Loads the model, then moves the rollback marker one step at a time from the start to the end,
// so each move evaluates just that step on top of the steps before it. Prints the time per step
// and the time to load and to reevaluate everything.

#include <voelund/application/CadSession.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>

namespace {
using Clock = std::chrono::steady_clock;

double milliseconds(const std::function<void()>& action) {
    const auto start = Clock::now();
    action();
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: voelund_model_probe <file.volund>\n";
        return 2;
    }
    using namespace voelund;
    application::CadSession session;
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "load " << milliseconds([&] { session.loadDocument(argv[1]); }) << " ms\n";
    const auto features = session.features();
    double total = 0;
    static_cast<void>(session.setRollback(features.front().id));
    for (const auto& feature : features) {
        const double step = milliseconds([&] {
            static_cast<void>(session.setRollback(feature.id));
            static_cast<void>(session.validBodies());
        });
        total += step;
        std::cout << std::setw(8) << step << " ms  " << feature.name << '\n';
    }
    std::cout << "sum of steps " << total << " ms\n";
    std::cout << "back to the end "
              << milliseconds([&] { static_cast<void>(session.setRollback(std::nullopt)); })
              << " ms\n";
    // An edit at the end: only the last step should need evaluating again.
    const auto last = features.back().id;
    std::cout << "suppress the last step "
              << milliseconds([&] { static_cast<void>(session.setFeatureSuppressed(last, true)); })
              << " ms\n";
    std::cout << "bring it back "
              << milliseconds([&] { static_cast<void>(session.setFeatureSuppressed(last, false)); })
              << " ms\n";
    return 0;
}
