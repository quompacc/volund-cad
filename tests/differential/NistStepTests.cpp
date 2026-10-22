// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

// Q01: NIST MBE PMI test parts (CTC/FTC/STC) as an outside reference for STEP import. The
// AP242 files carry geometric validation properties (volume, wetted area, centroid) computed
// by the CAD system that wrote them; Völund's import must reproduce them.
//
//   voelund_nist_step_tests <directory>            prints the measured values of every *.stp
//   voelund_nist_step_tests <directory> <table>    checks them against the reference table

#include <voelund/kernel/StepExchange.hpp>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Measured {
    std::size_t solids{0};
    double volume{0};
    double area{0};
    double x{0}, y{0}, z{0};
    std::string unit;
};

Measured measure(const std::filesystem::path& path) {
    const auto imported = voelund::kernel::readStep(path);
    if (imported.error)
        throw std::runtime_error(path.filename().string() + ": " + imported.message);
    Measured result;
    result.solids = imported.solids.size();
    result.unit = imported.sourceUnit;
    GProp_GProps volume;
    GProp_GProps area;
    for (const auto& solid : imported.solids) {
        GProp_GProps one;
        // Adaptive integration to a relative error of 1e-9: the fixed Gauss rule of the default
        // overload is off by up to ~1e-6 on the freeform faces of these parts.
        BRepGProp::VolumeProperties(solid.solid, one, 1e-9);
        volume.Add(one);
        GProp_GProps surface;
        BRepGProp::SurfaceProperties(solid.solid, surface, 1e-9);
        area.Add(surface);
    }
    result.volume = volume.Mass();
    result.area = area.Mass();
    const auto centre = volume.CentreOfMass();
    result.x = centre.X();
    result.y = centre.Y();
    result.z = centre.Z();
    return result;
}

void print(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        if (entry.path().extension() == ".stp")
            files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    std::cout << std::setprecision(12);
    for (const auto& file : files) {
        try {
            const auto value = measure(file);
            std::cout << file.filename().string() << " solids " << value.solids << " unit "
                      << value.unit << " volume " << value.volume << " area " << value.area
                      << " centre " << value.x << ' ' << value.y << ' ' << value.z << '\n';
        } catch (const std::exception& error) {
            std::cout << error.what() << '\n';
        }
    }
}

struct Reference {
    std::string file;
    double volume{0};
    std::optional<double> area;
    double x{0}, y{0}, z{0};
};

std::string trimmed(const std::string& text) {
    const auto first = text.find_first_not_of(" \t");
    const auto last = text.find_last_not_of(" \t\r");
    return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
}

std::vector<Reference> readTable(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("No reference table: " + path.string());
    std::vector<Reference> table;
    std::string line;
    while (std::getline(input, line)) {
        if (trimmed(line).empty() || trimmed(line).front() == '#')
            continue;
        std::vector<std::string> fields;
        std::stringstream parts(line);
        for (std::string field; std::getline(parts, field, '|');)
            fields.push_back(trimmed(field));
        if (fields.size() != 5)
            throw std::runtime_error("Malformed reference line: " + line);
        Reference reference{fields[0], std::stod(fields[1]), std::nullopt};
        if (fields[2] != "-")
            reference.area = std::stod(fields[2]);
        std::stringstream centre(fields[3]);
        if (!(centre >> reference.x >> reference.y >> reference.z))
            throw std::runtime_error("Malformed centroid: " + line);
        table.push_back(reference);
    }
    if (table.empty())
        throw std::runtime_error("Empty reference table");
    return table;
}

bool relativelyClose(double actual, double expected, double tolerance) {
    return std::abs(actual - expected) <= tolerance * std::abs(expected);
}

struct Generation {
    voelund::kernel::StepImport read;
    Measured measured;
};

// Writes the solids in the unit and reads the file back.
Generation exported(const std::vector<voelund::kernel::StepSolid>& solids,
                    voelund::kernel::StepLengthUnit unit, const std::string& name) {
    const auto target = std::filesystem::temp_directory_path() / ("voelund-nist-" + name + ".stp");
    const auto written = voelund::kernel::writeStep(target, solids, unit);
    if (written.error)
        throw std::runtime_error(name + ": export failed: " + written.message);
    Generation result{voelund::kernel::readStep(target), measure(target)};
    std::filesystem::remove(target);
    return result;
}

std::string describe(const Measured& value) {
    return std::to_string(value.solids) + " solids, volume " + std::to_string(value.volume) +
           ", area " + std::to_string(value.area);
}

// Exported and read back twice (inch export: application.step). Every STEP read heals again,
// so an imperfect source drifts slightly (FTC-07: up to 4e-6, far below its 0.03-0.1 mm edge
// tolerance); each generation must stay within the reference tolerance of 1e-5.
void roundTrip(const std::filesystem::path& source, const Measured& imported,
               std::vector<std::string>& problems) {
    const auto read = voelund::kernel::readStep(source);
    const auto unit = voelund::kernel::StepLengthUnit::millimetre;
    const auto first = exported(read.solids, unit, source.stem().string() + "-1");
    const auto second = exported(first.read.solids, unit, source.stem().string() + "-2");
    for (const auto* generation : {&first.measured, &second.measured}) {
        if (generation->solids != imported.solids ||
            !relativelyClose(generation->volume, imported.volume, 1e-5) ||
            !relativelyClose(generation->area, imported.area, 1e-5))
            problems.push_back(source.filename().string() +
                               ": export round trip changed the solid: " + describe(*generation));
    }
}

int check(const std::filesystem::path& directory, const std::filesystem::path& tablePath) {
    std::vector<std::string> problems;
    const auto table = readTable(tablePath);
    std::cout << std::setprecision(12);
    for (const auto& reference : table) {
        const auto path = directory / reference.file;
        const auto value = measure(path);
        const auto centre =
            std::hypot(value.x - reference.x, value.y - reference.y, value.z - reference.z);
        std::cout << reference.file << ": volume " << value.volume << " (" << reference.volume
                  << "), area " << value.area << ", centroid off by " << centre << " mm\n";
        if (value.solids != 1)
            problems.push_back(reference.file + ": " + std::to_string(value.solids) + " solids");
        if (!relativelyClose(value.volume, reference.volume, 1e-5))
            problems.push_back(reference.file + ": volume " + std::to_string(value.volume));
        if (reference.area && !relativelyClose(value.area, *reference.area, 1e-5))
            problems.push_back(reference.file + ": area " + std::to_string(value.area));
        if (centre > 0.01)
            problems.push_back(reference.file + ": centroid off by " + std::to_string(centre));
        roundTrip(path, value, problems);
    }
    for (const auto& problem : problems)
        std::cerr << "FAIL " << problem << '\n';
    if (!problems.empty())
        return 1;
    std::cout << "PASS NIST reference parts: " << table.size()
              << " volumes, areas and centroids within tolerance, export round trips\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2) {
            print(argv[1]);
            return 0;
        }
        if (argc == 3)
            return check(argv[1], argv[2]);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cerr << "usage: voelund_nist_step_tests <directory> [<table>]\n";
    return 2;
}
