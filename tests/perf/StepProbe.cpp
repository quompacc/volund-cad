// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

// I01: what happens to real STEP files, by hand.
//
//   voelund_step_probe read  <file.step>             read, sizes, meshing, session import
//   voelund_step_probe tools <file.step> [every-nth] every tool on the file's solids
//
// `tools` takes every nth solid into a smaller file first, so parts of a file too large to
// import can still be tried. A result counts as wrong when it is committed but no valid solid or
// its volume did not change. Each result is undone before the next try.

#include <voelund/application/Actions.hpp>
#include <voelund/application/CadSession.hpp>
#include <voelund/kernel/StepExchange.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>

#include <psapi.h>
#endif

namespace {
using namespace voelund;
using Clock = std::chrono::steady_clock;
using application::CommandResult;
using application::CommandStatus;

double seconds(const std::function<void()>& action) {
    const auto start = Clock::now();
    action();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

double peakMiB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
    return static_cast<double>(counters.PeakWorkingSetSize) / 1048576.0;
#else
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line))
        if (line.rfind("VmHWM:", 0) == 0)
            return std::stod(line.substr(6)) / 1024.0;
    return 0;
#endif
}

double volumeOf(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}

int read(const std::filesystem::path& path) {
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "file " << path.filename().string() << ' '
              << static_cast<double>(std::filesystem::file_size(path)) / 1048576.0 << " MiB\n";
    kernel::StepImport result;
    std::cout << "readStep " << seconds([&] { result = kernel::readStep(path); }) << " s, peak "
              << peakMiB() << " MiB\n";
    if (result.error) {
        std::cout << "error: " << result.message << '\n';
        return 1;
    }
    std::cout << "solids " << result.solids.size() << ", skipped " << result.skippedShapes
              << ", invalid " << result.invalidSolids << ", unit " << result.sourceUnit << '\n';
    std::size_t faces = 0;
    std::set<const void*> unique;
    std::size_t colored = 0;
    for (const auto& solid : result.solids) {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> map;
        TopExp::MapShapes(solid.solid, TopAbs_FACE, map);
        faces += static_cast<std::size_t>(map.Extent());
        unique.insert(solid.solid.TShape().get());
        colored += solid.color ? 1 : 0;
    }
    std::cout << "faces " << faces << ", different shapes " << unique.size() << ", with colour "
              << colored << '\n';

    std::size_t bytes = 0;
    std::size_t largest = 0;
    std::cout << "BRep bytes " << seconds([&] {
        for (const auto& solid : result.solids) {
            const auto size = kernel::writeShapeBytes(solid.solid).size();
            bytes += size;
            largest = std::max(largest, size);
        }
    })
              << " s: " << static_cast<double>(bytes) / 1048576.0 << " MiB, largest "
              << static_cast<double>(largest) / 1048576.0 << " MiB, peak " << peakMiB() << " MiB\n";

    std::cout << "mesh (0.1 mm, 0.5 rad) " << seconds([&] {
        for (const auto& solid : result.solids)
            BRepMesh_IncrementalMesh(solid.solid, 0.1, false, 0.5, true);
    }) << " s, peak "
              << peakMiB() << " MiB\n";

    application::CadSession session;
    session.newDocument();
    application::StepImportResult imported;
    std::cout << "session import " << seconds([&] { imported = session.importStep(path); })
              << " s, peak " << peakMiB() << " MiB: status " << static_cast<int>(imported.status)
              << " " << imported.message << '\n';
    if (imported.status != application::StepImportStatus::imported)
        return 0;
    std::cout << "bodies " << session.validBodies().size() << '\n';
    const auto saved = std::filesystem::temp_directory_path() / "voelund-step-probe.volund";
    std::cout << "save " << seconds([&] { session.saveDocument(saved); })
              << " s: " << static_cast<double>(std::filesystem::file_size(saved)) / 1048576.0
              << " MiB, peak " << peakMiB() << " MiB\n";
    application::CadSession reopened;
    std::cout << "open " << seconds([&] { reopened.loadDocument(saved); })
              << " s: " << reopened.validBodies().size() << " bodies, peak " << peakMiB()
              << " MiB\n";
    const auto first = session.features().front().id;
    std::cout << "move one body " << seconds([&] {
        static_cast<void>(session.moveBody(first, {1, 0, 0}, std::nullopt, 0));
    }) << " s, peak "
              << peakMiB() << " MiB\n";
    std::cout << "undo " << seconds([&] { static_cast<void>(session.undo()); }) << " s, peak "
              << peakMiB() << " MiB\n";
    std::filesystem::remove(saved);
    return 0;
}

struct Tally final {
    int committed{0};
    int rejected{0};
    int wrong{0};
    double time{0};
    std::map<std::string, int> reasons;
};

// A cylinder face whose outward normal points to its axis: the wall of a hole.
bool isHoleWall(const TopoDS_Face& face) {
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Cylinder)
        return false;
    BRepGProp_Face properties(face);
    double uMin = 0;
    double uMax = 0;
    double vMin = 0;
    double vMax = 0;
    properties.Bounds(uMin, uMax, vMin, vMax);
    gp_Pnt point;
    gp_Vec normal;
    properties.Normal((uMin + uMax) / 2, (vMin + vMax) / 2, point, normal);
    const auto cylinder = surface.Cylinder();
    const gp_Vec toAxis(point, cylinder.Axis().Location());
    const gp_Vec along(cylinder.Axis().Direction());
    return (toAxis - along * toAxis.Dot(along)).Dot(normal) > 0;
}

// All the faces of a body on the same cylinder as `index`: a hole is often two half walls.
std::vector<std::uint32_t>
sameCylinder(const NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>& faces,
             int index) {
    const auto cylinder = BRepAdaptor_Surface(TopoDS::Face(faces(index))).Cylinder();
    std::vector<std::uint32_t> result;
    for (int other = 1; other <= faces.Extent(); ++other) {
        BRepAdaptor_Surface surface(TopoDS::Face(faces(other)));
        if (surface.GetType() != GeomAbs_Cylinder)
            continue;
        const auto candidate = surface.Cylinder();
        if (std::abs(candidate.Radius() - cylinder.Radius()) < 1e-6 &&
            candidate.Axis().IsCoaxial(cylinder.Axis(), 1e-6, 1e-6))
            result.push_back(static_cast<std::uint32_t>(other));
    }
    return result;
}

class ToolRun final {
  public:
    explicit ToolRun(application::CadSession& session) : session_(session) {}

    void attempt(const std::string& tool, const std::function<CommandResult()>& command) {
        auto& tally = tallies_[tool];
        const double before = total();
        std::optional<CommandResult> result;
        tally.time += seconds([&] {
            try {
                result = command();
            } catch (const std::exception& error) {
                ++tally.rejected;
                ++tally.reasons[std::string("exception: ") + error.what()];
            }
        });
        if (!result)
            return;
        if (result->status() != CommandStatus::committed) {
            ++tally.rejected;
            std::string reason = result->diagnostic();
            for (const auto& issue : result->evaluation().features)
                if (reason.empty())
                    reason =
                        issue.technicalDetails.empty() ? issue.message : issue.technicalDetails;
            ++tally.reasons[reason.substr(0, 90)];
            return;
        }
        const bool valid = std::ranges::all_of(session_.validBodies(), [](const auto& body) {
            return BRepCheck_Analyzer(body.shape).IsValid();
        });
        const bool changed = tool == "move" || std::abs(total() - before) > 1e-9;
        if (valid && changed)
            ++tally.committed;
        else {
            ++tally.wrong;
            ++tally.reasons[valid ? "committed, volume unchanged" : "committed, invalid solid"];
        }
        static_cast<void>(session_.undo());
    }

    void note(const std::string& tool, const std::string& reason) {
        auto& tally = tallies_[tool];
        ++tally.rejected;
        ++tally.reasons[reason];
    }

    void report() const {
        std::cout << std::left << std::setw(24) << "tool" << std::right << std::setw(6) << "ok"
                  << std::setw(9) << "refused" << std::setw(7) << "wrong" << std::setw(9) << "avg s"
                  << '\n';
        for (const auto& [tool, tally] : tallies_) {
            const int tries = tally.committed + tally.rejected + tally.wrong;
            std::cout << std::left << std::setw(24) << tool << std::right << std::setw(6)
                      << tally.committed << std::setw(9) << tally.rejected << std::setw(7)
                      << tally.wrong << std::setw(9) << std::setprecision(2)
                      << (tries > 0 ? tally.time / tries : 0.0) << '\n';
            for (const auto& [reason, count] : tally.reasons)
                std::cout << "    " << count << "x " << reason << '\n';
        }
    }

  private:
    [[nodiscard]] double total() const {
        double sum = 0;
        for (const auto& body : session_.validBodies())
            sum += volumeOf(body.shape);
        return sum;
    }

    application::CadSession& session_;
    std::map<std::string, Tally> tallies_;
};

document::SubshapeOrigin importedFace(document::FeatureId feature, std::uint32_t number) {
    return document::SubshapeOrigin{.feature = feature, .role = {}, .importedFace = number};
}

constexpr int casesPerTool = 2;

application::CommandResult editFaces(application::CadSession& session, document::FeatureId feature,
                                     const std::vector<std::uint32_t>& numbers,
                                     document::FaceEditMode mode, double distance) {
    std::vector<document::SubshapeOrigin> origins;
    for (const auto number : numbers)
        origins.push_back(importedFace(feature, number));
    return session.createFaceEdit(
        application::CreateFaceEditAction{feature, std::move(origins), mode, distance});
}

void probePlane(application::CadSession& session, ToolRun& run, document::FeatureId feature,
                const features::SupportFaceFrame& frame) {
    run.attempt("offset plane +0.2", [&] {
        return session.createFaceEdit(application::CreateFaceEditAction{
            feature, {frame.origin}, document::FaceEditMode::offset, 0.2});
    });
    run.attempt("delete plane", [&] {
        return session.createFaceEdit(application::CreateFaceEditAction{
            feature, {frame.origin}, document::FaceEditMode::remove, 0});
    });
    run.attempt("push/pull +0.5", [&] { return session.pushPullFace(feature, frame.origin, 0.5); });
    run.attempt("push/pull -0.2",
                [&] { return session.pushPullFace(feature, frame.origin, -0.2); });
    const double middleX = (frame.minX + frame.maxX) / 2;
    const double middleY = (frame.minY + frame.maxY) / 2;
    const double half = std::min(frame.maxX - frame.minX, frame.maxY - frame.minY) / 8;
    if (half <= 1e-3)
        return;
    run.attempt("sketch on face + extrude", [&] {
        return session.createSketchExtrusion(
            feature, frame.plane,
            document::SketchRectangle{middleX - half, middleY - half, middleX + half,
                                      middleY + half},
            half);
    });
    run.attempt("hole on face", [&] {
        return session.createHole(feature, frame.origin, middleX, middleY, half, std::nullopt);
    });
}

void probeHole(application::CadSession& session, ToolRun& run, document::FeatureId feature,
               const std::vector<std::uint32_t>& wall, std::uint32_t number) {
    using document::FaceEditMode;
    run.attempt("hole: offset +0.1",
                [&] { return editFaces(session, feature, wall, FaceEditMode::offset, 0.1); });
    run.attempt("hole: delete walls",
                [&] { return editFaces(session, feature, wall, FaceEditMode::remove, 0); });
    run.attempt("hole: delete one face",
                [&] { return editFaces(session, feature, {number}, FaceEditMode::remove, 0); });
}

void probeCurvedFaces(application::CadSession& session, ToolRun& run, document::FeatureId feature,
                      const NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>& faces) {
    using document::FaceEditMode;
    int holes = 0;
    int rounds = 0;
    std::set<std::uint32_t> seen;
    for (int index = 1; index <= faces.Extent(); ++index) {
        const auto& face = TopoDS::Face(faces(index));
        const BRepAdaptor_Surface surface(face);
        const auto number = static_cast<std::uint32_t>(index);
        const bool wall = isHoleWall(face);
        if (wall && !seen.contains(number) && holes < casesPerTool) {
            ++holes;
            const auto all = sameCylinder(faces, index);
            seen.insert(all.begin(), all.end());
            probeHole(session, run, feature, all, number);
        } else if (!wall && rounds < casesPerTool &&
                   (surface.GetType() == GeomAbs_Torus || surface.GetType() == GeomAbs_Cylinder)) {
            ++rounds;
            run.attempt("curved: offset +0.1", [&] {
                return editFaces(session, feature, {number}, FaceEditMode::offset, 0.1);
            });
            run.attempt("curved: delete", [&] {
                return editFaces(session, feature, {number}, FaceEditMode::remove, 0);
            });
        }
    }
}

void probeEdges(application::CadSession& session, ToolRun& run, document::FeatureId feature) {
    int rounded = 0;
    // An undo reads the stored solid again: only the current shape's edges carry names.
    const auto current = session.featureShape(feature);
    for (TopExp_Explorer edges(*current, TopAbs_EDGE); edges.More() && rounded < casesPerTool;
         edges.Next()) {
        const auto named = session.filletEdge(feature, edges.Current());
        if (!named)
            continue;
        ++rounded;
        run.attempt("fillet edge 0.1",
                    [&] { return session.createFillet(feature, std::span{&*named, 1}, 0.1); });
        run.attempt("chamfer edge 0.1",
                    [&] { return session.createChamfer(feature, std::span{&*named, 1}, 0.1); });
    }
    if (rounded == 0)
        run.note("fillet edge 0.1", "no edge between two named faces");
}

void probeBody(application::CadSession& session, ToolRun& run, document::FeatureId feature,
               const TopoDS_Shape& shape) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    int plane = 0;
    for (const auto& frame : session.supportFaces(feature))
        if (!frame.secondary && plane++ < casesPerTool)
            probePlane(session, run, feature, frame);
    probeCurvedFaces(session, run, feature, faces);
    probeEdges(session, run, feature);
}

int tools(const std::filesystem::path& path, std::size_t every) {
    auto source = path;
    const auto scratch = std::filesystem::temp_directory_path() / "voelund-step-probe.step";
    if (every > 1) {
        const auto read = kernel::readStep(path);
        if (read.error) {
            std::cout << "error: " << read.message << '\n';
            return 1;
        }
        std::vector<kernel::StepSolid> chosen;
        for (std::size_t index = 0; index < read.solids.size(); index += every)
            chosen.push_back(read.solids[index]);
        std::cout << "taking " << chosen.size() << " of " << read.solids.size() << " solids\n";
        if (kernel::writeStep(scratch, chosen).error) {
            std::cout << "cannot write the smaller file\n";
            return 1;
        }
        source = scratch;
    }
    application::CadSession session;
    session.newDocument();
    const auto imported = session.importStep(source);
    if (imported.status != application::StepImportStatus::imported) {
        std::cout << "import refused: " << imported.message << '\n';
        return 1;
    }
    std::vector<std::pair<document::FeatureId, TopoDS_Shape>> bodies;
    for (const auto& feature : session.features())
        if (const auto shape = session.featureShape(feature.id))
            bodies.emplace_back(feature.id, *shape);
    std::cout << "bodies " << bodies.size() << '\n';
    ToolRun run(session);
    for (const auto& [feature, shape] : bodies) {
        run.attempt("move", [&, feature = feature] {
            return session.moveBody(feature, {1, 2, 3}, std::nullopt, 0);
        });
        probeBody(session, run, feature, shape);
    }
    run.report();
    const auto saved = std::filesystem::temp_directory_path() / "voelund-step-probe.volund";
    session.saveDocument(saved);
    application::CadSession reopened;
    reopened.loadDocument(saved);
    std::cout << "save and open: " << reopened.validBodies().size() << " of " << bodies.size()
              << " bodies, " << static_cast<double>(std::filesystem::file_size(saved)) / 1048576.0
              << " MiB\n";
    std::filesystem::remove(saved);
    std::filesystem::remove(scratch);
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    const std::string mode = argc >= 3 ? argv[1] : "";
    if (mode == "read")
        return read(argv[2]);
    if (mode == "tools")
        return tools(argv[2], argc >= 4 ? std::stoul(argv[3]) : 1);
    std::cerr << "usage: voelund_step_probe read|tools <file.step> [every-nth]\n";
    return 2;
}
