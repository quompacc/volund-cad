#include <voelund/kernel/StepExchange.hpp>

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <IGESControl_Reader.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Pln.hxx>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// N04 (user decision 2026-10-22): a box 40 x 20 x 10 and a cylinder r 5 h 10 at x 60 as IGES
// in mm; OCCT's IGES reader gets two solids back with the same total volume and bounding box.
// No solid, a face and an unwritable place are refused.
namespace {
using namespace voelund::kernel;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

double volumeOf(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}
} // namespace

int main() {
    try {
        const auto directory = std::filesystem::temp_directory_path() / "voelund-n04-iges-Ä";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        const StepSolid solids[] = {
            {BRepPrimAPI_MakeBox(40, 20, 10).Shape(), "Platte"},
            {BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(60, 0, 0), gp::DZ()), 5, 10).Shape(), "Rohr"}};
        const auto file = directory / "teil.igs";
        const auto written = writeIges(file, solids);
        require(!written.error, "IGES written: " + written.message);

        IGESControl_Reader reader;
        require(reader.ReadFile(file.string().c_str()) == IFSelect_RetDone, "IGES readable");
        reader.TransferRoots();
        const TopoDS_Shape back = reader.OneShape();
        int count = 0;
        for (TopExp_Explorer explorer(back, TopAbs_SOLID); explorer.More(); explorer.Next())
            ++count;
        const double expected = 8000 + 3.14159265358979 * 25 * 10;
        Bnd_Box box;
        BRepBndLib::Add(back, box);
        double xMin = 0, yMin = 0, zMin = 0, xMax = 0, yMax = 0, zMax = 0;
        box.Get(xMin, yMin, zMin, xMax, yMax, zMax);
        require(count == 2 && std::abs(volumeOf(back) - expected) < 1e-3 * expected &&
                    std::abs(xMax - 65) < 0.01 && std::abs(yMin + 5) < 0.01 &&
                    std::abs(zMax - 10) < 0.01,
                "Two solids back, same volume and box: " + std::to_string(count) + " solids, " +
                    std::to_string(volumeOf(back)));

        require(writeIges(directory / "leer.igs", {}).error == StepExportError::nothingToWrite,
                "Nothing to write");
        const StepSolid face[] = {{BRepBuilderAPI_MakeFace(gp_Pln(), 0, 1, 0, 1).Shape(), "F"}};
        require(writeIges(directory / "f.igs", face).error == StepExportError::notASolid,
                "A face is refused");
        require(writeIges(directory / "fehlt" / "x.igs", solids).error ==
                    StepExportError::writeFailed,
                "An unwritable place is reported");
        std::filesystem::remove_all(directory);
        std::cout << "PASS N04 IGES export: two named solids round-trip with volume and box, "
                     "refusals\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
