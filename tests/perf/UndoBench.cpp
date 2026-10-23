// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

// UNDO-MESSUNG (2026-10-22): memory and time of the undo history, which keeps a complete OCAF
// document per step (D12c). No delta undo is implemented here; this only measures.
//
//   voelund_undo_bench <model> <steps> [<STEP file for the large model>]
//   models: small (one box), medium (25 bodies of box and joined cylinder, 50 features),
//           large (medium plus an imported STEP part, embedded as BRep)
//
// One run per process, so the memory belongs to it. Each step is one committed edit of the
// first box (one undo entry). Result lines read
//   model=<model> steps=<n> metric=<name> value=<number> unit=<unit>
// A run stops early when the resident memory passes 8 GiB and says so.

#include <voelund/application/CadSession.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// psapi.h needs the Windows types declared first.
#include <psapi.h>
#endif

namespace {
using namespace voelund;
using application::CommandStatus;
using Clock = std::chrono::steady_clock;

std::string prefix;

void report(const std::string& metric, double value, const std::string& unit) {
    std::cout << prefix << " metric=" << metric << " value=" << value << " unit=" << unit
              << std::endl;
}

double milliseconds(const std::function<void()>& action) {
    const auto start = Clock::now();
    action();
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void reportSeries(const std::string& metric, std::vector<double> samples) {
    if (samples.empty())
        return;
    std::sort(samples.begin(), samples.end());
    const auto at = [&samples](double fraction) {
        return samples[static_cast<std::size_t>(
            std::ceil(fraction * static_cast<double>(samples.size())) - 1)];
    };
    report(metric + ".median", at(0.5), "ms");
    report(metric + ".p95", at(0.95), "ms");
    report(metric + ".max", samples.back(), "ms");
}

// Resident memory of this process in MiB.
double resident() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
    return static_cast<double>(counters.WorkingSetSize) / 1048576.0;
#else
    std::ifstream status("/proc/self/status");
    for (std::string line; std::getline(status, line);)
        if (line.rfind("VmRSS:", 0) == 0)
            return std::stod(line.substr(6)) / 1024.0;
    return 0;
#endif
}

document::FeatureId committed(const application::CommandResult& result, const char* what) {
    if (result.status() != CommandStatus::committed || !result.feature())
        throw std::runtime_error(std::string(what) + " was not committed");
    return *result.feature();
}

// The first box of the model, the one every step edits.
document::FeatureId buildModel(application::CadSession& session, const std::string& model,
                               const std::filesystem::path& stepFile) {
    const auto first = committed(session.createBoxFeature({40, 30, 20}), "Box");
    if (model == "small")
        return first;
    for (int body = 1; body < 25; ++body) {
        const auto box = committed(session.createBoxFeature({40, 30, 20}), "Box");
        document::BoxParameters placed{
            {40, 30, 20},
            {body * 60.0, 0, 0, kernel::Radians{0}, kernel::Radians{0}, kernel::Radians{0}}};
        if (session.editBoxParameters(box, placed).status() != CommandStatus::committed)
            throw std::runtime_error("Box placement");
        if (session.createCylinderFeature(box, document::CylinderResultMode::join).status() !=
            CommandStatus::committed)
            throw std::runtime_error("Joined cylinder");
    }
    if (model == "large") {
        const auto imported = session.importStep(stepFile);
        if (imported.status != application::StepImportStatus::imported)
            throw std::runtime_error("STEP import: " + imported.message);
    }
    return first;
}

int run(const std::string& model, int steps, const std::filesystem::path& stepFile) {
    application::CadSession session;
    session.newDocument();
    const double empty = resident();
    const auto box = buildModel(session, model, stepFile);
    // The steps measure the history, not the building: start from a clear history.
    const auto file = std::filesystem::temp_directory_path() / ("undo-bench-" + model + ".volund");
    session.saveDocument(file);
    session.loadDocument(file);
    report("features", static_cast<double>(session.features().size()), "count");
    report("file_size", static_cast<double>(std::filesystem::file_size(file)) / 1024.0, "KiB");
    std::filesystem::remove(file);
    const double before = resident();
    report("memory_model", before - empty, "MiB");

    std::vector<double> edits;
    int done = 0;
    for (; done < steps; ++done) {
        document::BoxParameters parameters{
            {40.0 + (done % 2 == 0 ? 1 : 0), 30, 20},
            {0, 0, 0, kernel::Radians{0}, kernel::Radians{0}, kernel::Radians{0}}};
        edits.push_back(milliseconds([&] {
            if (session.editBoxParameters(box, parameters).status() != CommandStatus::committed)
                throw std::runtime_error("Edit step");
        }));
        if (resident() > 8192) {
            report("stopped_at_step", done + 1, "count");
            ++done;
            break;
        }
    }
    const double after = resident();
    report("steps_done", done, "count");
    report("memory_history", after - before, "MiB");
    report("memory_per_step", (after - before) / std::max(1, done), "MiB");
    reportSeries("edit", edits);

    std::vector<double> undos;
    while (session.canUndo())
        undos.push_back(milliseconds([&] {
            if (session.undo().status() == CommandStatus::rejected)
                throw std::runtime_error("Undo");
        }));
    report("undo_steps", static_cast<double>(undos.size()), "count");
    reportSeries("undo", undos);
    std::vector<double> redos;
    while (session.canRedo())
        redos.push_back(milliseconds([&] {
            if (session.redo().status() == CommandStatus::rejected)
                throw std::runtime_error("Redo");
        }));
    reportSeries("redo", redos);
    report("memory_after_undo_redo", resident() - before, "MiB");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::map<std::string, bool> models{
            {"small", false}, {"medium", false}, {"large", true}};
        if (argc < 3 || !models.contains(argv[1]) || (models.at(argv[1]) && argc < 4)) {
            std::cerr << "usage: voelund_undo_bench small|medium|large <steps> [<STEP file>]\n";
            return 2;
        }
        const std::string model = argv[1];
        const int steps = std::stoi(argv[2]);
        prefix = "model=" + model + " steps=" + std::to_string(steps);
        return run(model, steps, argc > 3 ? std::filesystem::path(argv[3]) : "");
    } catch (const std::exception& error) {
        std::cerr << prefix << " failed: " << error.what() << '\n';
        return 1;
    }
}
