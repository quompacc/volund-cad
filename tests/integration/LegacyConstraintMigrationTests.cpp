#include <voelund/document/DocumentSerializer.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>

namespace {
using namespace voelund::document;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
using K = SketchConstraintKind;
struct Case {
    int version;
    const char* record;
    K kind;
    std::uint64_t first, second;
    double value;
};
// Explicit legacy records and independently stated semantic expectations supplement
// the eleven kinds already covered by v01-v16 and DocumentParserTests.
const Case cases[]{
    {4, "VERTICAL 1 START 0 START 0", K::vertical, 1, 0, 0},
    {4, "DISTANCE 1 START 0 START 10", K::distance, 1, 0, 10},
    {4, "COINCIDENT 1 START 2 START 0", K::coincident, 1, 2, 0},
    {4, "PARALLEL 1 START 2 START 0", K::parallel, 1, 2, 0},
    {4, "PERPENDICULAR 1 START 2 START 0", K::perpendicular, 1, 2, 0},
    {6, "COORDINATE_Y 1 START 0 START -3", K::coordinateY, 1, 0, -3},
    {6, "POINT_ON_OBJECT 1 START 2 START 0", K::pointOnObject, 1, 2, 0},
    {7, "TANGENT 1 START 3 START 0", K::tangent, 1, 3, 0},
    {9, "ANGLE 1 START 2 START -90", K::angle, 1, 2, 0},
    {10, "POINT_DISTANCE_Y 1 START 2 START -4", K::pointDistanceY, 1, 2, -4},
    {11, "PERPENDICULAR_BISECTOR 1 START 2 START 0", K::perpendicularBisector, 1, 2, 0},
    {12, "CONCENTRIC 2 START 3 START 0", K::concentric, 2, 3, 0},
    {13, "COLLINEAR 1 START 2 START 0", K::collinear, 1, 2, 0},
    {13, "VERTICAL_POINTS 1 START 2 START 0", K::verticalPoints, 1, 2, 0},
    {14, "POINT_LINE_DISTANCE 1 START 2 START 4", K::pointLineDistance, 1, 2, 4},
};
std::string fixture(const std::filesystem::path& root, int version) {
    std::ostringstream name;
    name << 'v' << std::setw(2) << std::setfill('0') << version << ".volund";
    std::ifstream input(root / name.str(), std::ios::binary);
    require(bool(input), "Missing fixed legacy fixture");
    return {std::istreambuf_iterator<char>(input), {}};
}
std::string replace(std::string text, const std::string& from, const std::string& to) {
    const auto at = text.find(from);
    require(at != std::string::npos, "Supplement fixture mismatch");
    text.replace(at, from.size(), to);
    return text;
}
ModelDocument parse(const std::string& text) {
    std::istringstream input(text);
    return DocumentSerializer::read(input);
}
void rejected(const std::string& text) {
    bool failed = false;
    try { (void)parse(text); } catch (const std::exception&) { failed = true; }
    require(failed, "Invalid legacy supplement accepted");
}
void check(const ModelDocument& model, const Case& item) {
    const auto sketch = model.sketchParameters(FeatureId{3});
    require(sketch.constraints.values.size() == 1 && sketch.constraints.nextId == 2,
        "Constraint count or allocation changed");
    const auto& c = sketch.constraints.values.front();
    require(c.id == SketchConstraintId{1} && c.kind == item.kind &&
        c.first == SketchGeometryReference{SketchElementId{item.first}, SketchPointPosition::start} &&
        c.second == (item.second ? std::optional{SketchGeometryReference{SketchElementId{item.second},
            SketchPointPosition::start}} : std::nullopt) && !c.third && c.auxiliaryValues.empty() &&
        c.value == item.value, "Legacy constraint semantic fields changed");
    if (item.kind == K::angle)
        require(std::abs(c.angleValue.value() + std::numbers::pi / 2) < 1e-14, "Legacy degrees not converted to signed radians");
}
void run(const std::filesystem::path& root) {
    for (const auto& item : cases) {
        const auto original = fixture(root, item.version);
        auto text = original;
        const auto begin = text.find("CONSTRAINT 1 ");
        const auto end = text.find('\n', begin);
        require(begin != std::string::npos && end != std::string::npos, "Missing constraint record");
        const auto record = std::string("CONSTRAINT 1 ") + item.record +
            (item.version >= 8 ? " 0 START" : "") + (item.version >= 14 ? " 0" : "");
        text.replace(begin, end - begin, record);
        if (item.kind == K::concentric)
            text = replace(text, "ELEMENT LINE 2 0 4 10 4 FREE 0", "ELEMENT CIRCLE 2 20 20 4 0");
        auto model = parse(text);
        check(model, item);
        const auto expected = DocumentSnapshot::capture(model);
        const auto identity = model.documentId();
        std::ostringstream output;
        DocumentSerializer::write(model, output);
        auto native = parse(output.str());
        check(native, item);
        require(native.documentId() == identity && DocumentSnapshot::capture(native) == expected,
            "Supplement native roundtrip lost fields/identity");
        // These are parameter migration fixtures, not claims of solver satisfaction.
        auto dimensions = native.boxDimensions(FeatureId{1});
        dimensions.width = 21;
        require(native.setBoxDimensions(FeatureId{1}, dimensions), "Supplement edit was a no-op");
        const auto edited = DocumentSnapshot::capture(native);
        require(edited != expected && native.undo() && DocumentSnapshot::capture(native) == expected &&
            native.redo() && DocumentSnapshot::capture(native) == edited, "Supplement edit/undo/redo failed");
        rejected(replace(text, record, "CONSTRAINT 1 UNKNOWN 1 START 0 START 0"));
        require(fixture(root, item.version) == original, "Supplement modified fixed source fixture");
        std::cout << "PASS VOLUND " << item.version << ' ' << item.record << '\n';
    }
}
} // namespace
int main(int argc, char** argv) {
    try { require(argc == 2, "Fixture directory required"); run(argv[1]); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
