// L06 (A-PERF-01): load stages with representative content, measured on the application layer.
//
//   voelund_perf_bench <stage> [<work directory>]
//   stages: sketch-100 sketch-1000 features-10 features-100 features-500 bodies-1 bodies-10
//           bodies-100
//
// One stage per process, so peak memory belongs to it. Every result line reads
//   stage=<stage> metric=<name> value=<number> unit=<unit>
// with median and p95 for repeated measurements. These are load stages, not supported limits.

#include <voelund/application/CadSession.hpp>
#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <algorithm>
#include <array>
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

std::string stageName;

void report(const std::string& metric, double value, const std::string& unit) {
    std::cout << "stage=" << stageName << " metric=" << metric << " value=" << value
              << " unit=" << unit << std::endl;
}

double milliseconds(const std::function<void()>& action) {
    const auto start = Clock::now();
    action();
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void reportSeries(const std::string& metric, std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    const auto at = [&samples](double fraction) {
        return samples[static_cast<std::size_t>(
            std::ceil(fraction * static_cast<double>(samples.size())) - 1)];
    };
    report(metric + ".median", at(0.5), "ms");
    report(metric + ".p95", at(0.95), "ms");
}

// Resident and peak memory of this process in MiB.
std::pair<double, double> memory() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
    return {static_cast<double>(counters.WorkingSetSize) / 1048576.0,
            static_cast<double>(counters.PeakWorkingSetSize) / 1048576.0};
#else
    std::ifstream status("/proc/self/status");
    double resident = 0;
    double peak = 0;
    for (std::string line; std::getline(status, line);) {
        if (line.rfind("VmRSS:", 0) == 0)
            resident = std::stod(line.substr(6)) / 1024.0;
        if (line.rfind("VmHWM:", 0) == 0)
            peak = std::stod(line.substr(6)) / 1024.0;
    }
    return {resident, peak};
#endif
}

document::FeatureId committed(const application::CommandResult& result, const char* what) {
    if (result.status() != CommandStatus::committed || !result.feature())
        throw std::runtime_error(std::string(what) + " was not committed");
    return *result.feature();
}

void addConstraint(document::SketchParameters& sketch, document::SketchConstraintKind kind,
                   document::SketchGeometryReference first,
                   std::optional<document::SketchGeometryReference> second = {}, double value = 0) {
    sketch.constraints.values.emplace_back(
        document::SketchConstraintId{sketch.constraints.nextId++}, kind, first, second, value);
}

// Squares of four lines in a grid, each fully determined: coincident corners, horizontal and
// vertical sides, two lengths, the first corner placed. The lines start slightly off so the
// solver has to move them.
document::SketchParameters squareGrid(std::size_t elements, double side) {
    using Kind = document::SketchConstraintKind;
    using Position = document::SketchPointPosition;
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xy), {}};
    sketch.datum = document::DatumPlane::xy;
    const std::size_t squares = elements / 4;
    const auto columns =
        static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(squares))));
    for (std::size_t index = 0; index < squares; ++index) {
        const double x = static_cast<double>(index % columns) * side * 2;
        const double y = static_cast<double>(index / columns) * side * 2;
        const double off = 0.05 * side;
        const std::array<document::SketchPoint, 4> corners{
            document::SketchPoint{x, y}, document::SketchPoint{x + side + off, y},
            document::SketchPoint{x + side, y + side + off},
            document::SketchPoint{x - off, y + side}};
        const std::uint64_t first = sketch.elements.nextId;
        for (std::size_t edge = 0; edge < 4; ++edge)
            sketch.elements.values.emplace_back(
                document::SketchElementId{sketch.elements.nextId++},
                document::SketchLine{corners[edge], corners[(edge + 1) % 4]});
        const auto line = [first](std::size_t edge, Position position) {
            return document::SketchGeometryReference{document::SketchElementId{first + edge},
                                                     position};
        };
        for (std::size_t edge = 0; edge < 4; ++edge)
            addConstraint(sketch, Kind::coincident, line(edge, Position::end),
                          line((edge + 1) % 4, Position::start));
        addConstraint(sketch, Kind::horizontal, line(0, Position::start));
        addConstraint(sketch, Kind::horizontal, line(2, Position::start));
        addConstraint(sketch, Kind::vertical, line(1, Position::start));
        addConstraint(sketch, Kind::vertical, line(3, Position::start));
        addConstraint(sketch, Kind::distance, line(0, Position::start), {}, side);
        addConstraint(sketch, Kind::distance, line(1, Position::start), {}, side);
        addConstraint(sketch, Kind::coordinateX, line(0, Position::start), {}, x);
        addConstraint(sketch, Kind::coordinateY, line(0, Position::start), {}, y);
    }
    return sketch;
}

document::FeatureId sketchOf(const application::CadSession& session, document::FeatureId feature) {
    const auto features = session.features();
    const auto found = std::ranges::find(features, feature, &document::FeatureDescriptor::id);
    if (found == features.end() || !found->inputFeature)
        throw std::runtime_error("Extrusion without sketch");
    return *found->inputFeature;
}

void sketchStage(application::CadSession& session, std::size_t elements) {
    auto sketch = squareGrid(elements, 4);
    std::vector<double> solves;
    for (int repeat = 0; repeat < 5; ++repeat) {
        auto copy = sketch;
        solves.push_back(milliseconds([&copy] {
            if (features::solveSketchConstraints(copy).status !=
                features::SketchSolveStatus::success)
                throw std::runtime_error("The grid does not solve");
        }));
    }
    reportSeries("solve", solves);
    (void)features::solveSketchConstraints(sketch);
    const auto regions = features::identifySketchRegions(sketch.elements.values).ids;
    report("regions", static_cast<double>(regions.size()), "count");
    document::FeatureId extrusion{0};
    report("create_and_extrude", milliseconds([&] {
               extrusion = committed(session.createDatumSketchExtrusion(sketch, regions, 5),
                                     "Sketch extrusion");
           }),
           "ms");
    const auto sketchFeature = sketchOf(session, extrusion);
    std::vector<double> edits;
    for (int repeat = 0; repeat < 5; ++repeat) {
        auto parameters = session.sketchParameters(sketchFeature);
        auto& length = parameters.constraints.values[4];
        length.value = 4.0 + 0.1 * (repeat % 2 == 0 ? 1 : -1);
        edits.push_back(milliseconds([&] {
            if (session.editSketchParameters(sketchFeature, parameters).status() !=
                CommandStatus::committed)
                throw std::runtime_error("Dimension edit");
        }));
    }
    reportSeries("edit_dimension_recompute", edits);
}

// A base plate and bosses joined onto it, one sketch and one extrusion each.
void featureStage(application::CadSession& session, std::size_t count) {
    document::SketchParameters plate{document::datumSketchPlane(document::DatumPlane::xy), {}};
    plate.datum = document::DatumPlane::xy;
    plate.elements.values.emplace_back(document::SketchElementId{1},
                                       document::SketchRectangle{0, 0, 400, 400});
    plate.elements.nextId = 2;
    const auto base =
        committed(session.createDatumSketchExtrusion(
                      plate, features::identifySketchRegions(plate.elements.values).ids, 10),
                  "Plate");
    const auto body = *session.resultBody(base);
    std::vector<double> creates;
    const std::size_t bosses = count > 2 ? (count - 2) / 2 : 0;
    for (std::size_t index = 0; index < bosses; ++index) {
        document::SketchParameters boss{document::datumSketchPlane(document::DatumPlane::xy), {}};
        boss.datum = document::DatumPlane::xy;
        const double x = 10 + static_cast<double>(index % 20) * 19;
        const double y = 10 + static_cast<double>(index / 20 % 20) * 19;
        boss.elements.values.emplace_back(document::SketchElementId{1},
                                          document::SketchRectangle{x, y, x + 8, y + 8});
        boss.elements.nextId = 2;
        application::ExtrusionMode mode;
        mode.operation = document::ExtrusionOperation::join;
        mode.target = body;
        const auto regions = features::identifySketchRegions(boss.elements.values).ids;
        creates.push_back(milliseconds([&] {
            (void)committed(session.createDatumSketchExtrusion(
                                boss, regions, 12 + static_cast<double>(index % 5), mode),
                            "Boss");
        }));
    }
    report("features", static_cast<double>(session.features().size()), "count");
    if (!creates.empty())
        reportSeries("add_boss", creates);
    const auto edit = [&session](document::FeatureId sketch, double shift) {
        auto parameters = session.sketchParameters(sketch);
        auto& geometry = parameters.elements.values.front().geometry;
        if (auto* rectangle = std::get_if<document::SketchRectangle>(&geometry))
            rectangle->second.x += shift;
        else if (auto* line = std::get_if<document::SketchLine>(&geometry))
            line->start.y += shift;
        return milliseconds([&] {
            if (session.editSketchParameters(sketch, parameters).status() !=
                CommandStatus::committed)
                throw std::runtime_error("Sketch edit");
        });
    };
    std::vector<double> first;
    std::vector<double> last;
    const auto lastSketch = sketchOf(session, session.features().back().id);
    for (int repeat = 0; repeat < 5; ++repeat) {
        const double shift = repeat % 2 == 0 ? 0.5 : -0.5;
        first.push_back(edit(sketchOf(session, base), shift));
        last.push_back(edit(lastSketch, shift));
    }
    reportSeries("edit_first_recompute", first);
    reportSeries("edit_last_recompute", last);
}

void bodyStage(application::CadSession& session, std::size_t count) {
    std::vector<document::FeatureId> boxes;
    for (std::size_t index = 0; index < count; ++index) {
        const auto box = committed(session.createBoxFeature({10, 10, 10}), "Box");
        document::BoxParameters parameters{
            {10, 10, 10},
            {static_cast<double>(index % 10) * 20, static_cast<double>(index / 10) * 20, 0,
             kernel::Radians{0}, kernel::Radians{0}, kernel::Radians{0}}};
        const auto placed = session.editBoxParameters(box, parameters);
        // The first box already stands at the origin: no change is no failure.
        if (placed.status() != CommandStatus::committed &&
            placed.reason() != application::RejectionReason::noChange)
            throw std::runtime_error("Box placement");
        boxes.push_back(box);
    }
    report("bodies", static_cast<double>(session.validBodies().size()), "count");
    std::vector<double> edits;
    for (int repeat = 0; repeat < 5; ++repeat) {
        const auto box = boxes[boxes.size() / 2];
        document::BoxParameters parameters{{10, 10, 10.0 + (repeat % 2 == 0 ? 1 : 0)},
                                           {static_cast<double>(boxes.size() / 2 % 10) * 20,
                                            static_cast<double>(boxes.size() / 2 / 10) * 20, 0,
                                            kernel::Radians{0}, kernel::Radians{0},
                                            kernel::Radians{0}}};
        edits.push_back(milliseconds([&] {
            if (session.editBoxParameters(box, parameters).status() != CommandStatus::committed)
                throw std::runtime_error("Box edit");
        }));
    }
    reportSeries("edit_one_body_recompute", edits);
}

void fileStage(application::CadSession& session, const std::filesystem::path& directory) {
    const auto file = directory / ("perf-" + stageName + ".volund");
    report("save", milliseconds([&] { session.saveDocument(file); }), "ms");
    report("file_size", static_cast<double>(std::filesystem::file_size(file)) / 1024.0, "KiB");
    std::vector<double> opens;
    const double before = memory().first;
    for (int cycle = 0; cycle < 10; ++cycle) {
        opens.push_back(milliseconds([&] { session.loadDocument(file); }));
        session.newDocument();
    }
    reportSeries("open", opens);
    report("memory_after_10_open_close_growth", memory().first - before, "MiB");
    std::filesystem::remove(file);
}

const std::map<std::string, std::function<void(application::CadSession&)>> stages{
    {"sketch-100", [](auto& session) { sketchStage(session, 100); }},
    {"sketch-1000", [](auto& session) { sketchStage(session, 1000); }},
    {"features-10", [](auto& session) { featureStage(session, 10); }},
    {"features-100", [](auto& session) { featureStage(session, 100); }},
    {"features-500", [](auto& session) { featureStage(session, 500); }},
    {"bodies-1", [](auto& session) { bodyStage(session, 1); }},
    {"bodies-10", [](auto& session) { bodyStage(session, 10); }},
    {"bodies-100", [](auto& session) { bodyStage(session, 100); }},
};

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || !stages.contains(argv[1])) {
            std::cerr << "usage: voelund_perf_bench <stage> [<work directory>]\nstages:";
            for (const auto& [name, run] : stages)
                std::cerr << ' ' << name;
            std::cerr << '\n';
            return 2;
        }
        stageName = argv[1];
        const std::filesystem::path directory =
            argc > 2 ? std::filesystem::path(argv[2]) : std::filesystem::temp_directory_path();
        application::CadSession session;
        session.newDocument();
        report("total", milliseconds([&] { stages.at(stageName)(session); }), "ms");
        fileStage(session, directory);
        const auto [resident, peak] = memory();
        report("memory_resident", resident, "MiB");
        report("memory_peak", peak, "MiB");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "stage=" << stageName << " failed: " << error.what() << '\n';
        return 1;
    }
}
