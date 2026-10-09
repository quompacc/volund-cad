#include "../../src/document/NativePrimitiveCodec.hpp"
#include <BinDrivers.hxx>
#include <PCDM_ReaderStatus.hxx>
#include <PCDM_StoreStatus.hxx>
#include <TDataStd_Integer.hxx>
#include <TDocStd_Application.hxx>
#include <cmath>
#include <iostream>
#include <numbers>
#include <sstream>
#include <voelund/features/SketchConstraintSolver.hpp>

// S12a: the associative sketch pattern as data. Instances follow linear, circular and mirror
// rules from the current sources; validation refuses counts outside the limits, dependent
// patterns (instance as source or axis), relations on instances and legacy rectangle sources;
// the solver keeps instances out of its unknowns and regenerates them; schema 11 stores the
// patterns, schema 10 refuses them, and a sketch without patterns keeps its schema 10 form.
namespace {
using namespace voelund;
using namespace document;
using detail::NativePrimitiveCodec;
constexpr double pi = std::numbers::pi;

void require(bool value, const std::string& message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Action> void rejected(Action action, const std::string& message) {
    bool failed = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed, message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
bool near(SketchPoint a, SketchPoint b) { return near(a.x, b.x) && near(a.y, b.y); }
SketchElementId id(std::uint64_t value) { return SketchElementId{value}; }

const SketchGeometry& geometry(const SketchParameters& sketch, std::uint64_t element) {
    for (const auto& item : sketch.elements.values)
        if (item.id == id(element))
            return item.geometry;
    throw std::runtime_error("No element");
}

// Sources: line 1 and arc 2; line 3 is the mirror axis x = 20. Instances start as copies.
SketchParameters patterned() {
    SketchParameters sketch{datumSketchPlane(DatumPlane::xy), {}};
    sketch.datum = DatumPlane::xy;
    const SketchLine line{{0, 0}, {4, 0}};
    const SketchArc arc{{10, 0}, 2, 0, pi / 2};
    sketch.elements.values = {{id(1), line}, {id(2), arc}, {id(3), SketchLine{{20, -10}, {20, 10}}},
                              // linear: 4 line, 5 arc, 6 line, 7 arc; circular 8-10; mirror 11
                              {id(4), line}, {id(5), arc}, {id(6), line}, {id(7), arc},
                              {id(8), line}, {id(9), line}, {id(10), line}, {id(11), arc}};
    sketch.elements.nextId = 12;
    sketch.patterns.values = {
        {1, SketchPatternKind::linear, {id(1), id(2)}, 3, {0, 5}, {}, 0, std::nullopt,
         {id(4), id(5), id(6), id(7)}},
        {2, SketchPatternKind::circular, {id(1)}, 4, {}, {0, 0}, pi / 2, std::nullopt,
         {id(8), id(9), id(10)}},
        {3, SketchPatternKind::mirror, {id(2)}, 2, {}, {}, 0, id(3), {id(11)}}};
    sketch.patterns.nextId = 4;
    return sketch;
}

void rules() {
    auto sketch = patterned();
    validateSketchParameters(sketch);
    regenerateSketchPatterns(sketch.elements, sketch.patterns);
    const auto& moved = std::get<SketchLine>(geometry(sketch, 6));
    require(near(moved.start, {0, 10}) && near(moved.end, {4, 10}), "Linear: third position");
    require(near(std::get<SketchArc>(geometry(sketch, 5)).center, {10, 5}), "Linear arc");
    const auto& turned = std::get<SketchLine>(geometry(sketch, 8));
    require(near(turned.end, {0, 4}), "Circular: a quarter turn about the center");
    require(near(std::get<SketchLine>(geometry(sketch, 9)).end, {-4, 0}), "Circular: half turn");
    const auto& mirrored = std::get<SketchArc>(geometry(sketch, 11));
    const auto at = [&](double angle) {
        return SketchPoint{mirrored.center.x + 2 * std::cos(angle),
                           mirrored.center.y + 2 * std::sin(angle)};
    };
    // (12, 0) -> (10, 2) counterclockwise; its image runs from (30, 2) to (28, 0).
    require(near(mirrored.center, {30, 0}) && near(at(mirrored.startAngle), {30, 2}) &&
                near(at(mirrored.endAngle), {28, 0}),
            "Mirror: the arc's image across x = 20");

    const auto refuses = [](auto change, const std::string& what) {
        auto broken = patterned();
        change(broken);
        rejected([&] { validateSketchParameters(broken); }, what);
    };
    refuses([](auto& s) { s.patterns.values[0].count = 1; }, "Count below 2");
    refuses([](auto& s) { s.patterns.values[1].count = 1001; }, "Count above 1000");
    refuses([](auto& s) { s.patterns.values[0].instances.pop_back(); }, "Instances per source");
    refuses([](auto& s) { s.patterns.values[2].sources = {id(4)}; },
            "An instance as source: dependent patterns");
    refuses([](auto& s) { s.patterns.values[2].axis = id(8); }, "An instance as mirror axis");
    refuses([](auto& s) { s.patterns.values[2].axis = id(2); }, "A source as its own axis");
    refuses([](auto& s) { s.patterns.values[1].instances[0] = id(4); },
            "An element is instance of one pattern");
    refuses([](auto& s) { s.patterns.values[2].id = 1; }, "Duplicate pattern ID");
    refuses([](auto& s) { s.patterns.nextId = 3; }, "Next pattern ID");
    refuses([](auto& s) { s.patterns.values[1].angle = 2 * pi; }, "A full turn per step");
    refuses([](auto& s) { s.patterns.values[0].step = {0, 0}; }, "A linear pattern needs a step");
    refuses([](auto& s) { s.elements.values[0].geometry = SketchRectangle(0, 0, 4, 1); },
            "A legacy rectangle as source");
    refuses(
        [](auto& s) {
            s.constraints.values.push_back({SketchConstraintId{1}, SketchConstraintKind::horizontal,
                                            {id(6), SketchPointPosition::start}, std::nullopt});
            s.constraints.nextId = 2;
        },
        "A relation on an instance");
}

void solving() {
    auto sketch = patterned();
    sketch.constraints = {{{SketchConstraintId{1}, SketchConstraintKind::horizontal,
                            {id(1), SketchPointPosition::start}, std::nullopt},
                           {SketchConstraintId{2}, SketchConstraintKind::coordinateX,
                            {id(1), SketchPointPosition::start}, std::nullopt, 0},
                           {SketchConstraintId{3}, SketchConstraintKind::coordinateY,
                            {id(1), SketchPointPosition::start}, std::nullopt, 0},
                           {SketchConstraintId{4}, SketchConstraintKind::distance,
                            {id(1), SketchPointPosition::start}, std::nullopt, 6}},
                          5};
    const auto result = features::solveSketchConstraints(sketch);
    require(result.status == features::SketchSolveStatus::success, "Solved");
    // Arc 2 (5) and axis 3 (4) stay free; line 1 is fixed; instances add no unknowns.
    require(result.degreesOfFreedom == 9, "Instances are no unknowns, got " +
                                              std::to_string(result.degreesOfFreedom));
    require(near(std::get<SketchLine>(geometry(sketch, 4)).end, {6, 5}),
            "The instance follows the source's new length");
    const auto free = [&](std::uint64_t element) {
        return std::ranges::find(result.freeElements, id(element)) != result.freeElements.end();
    };
    require(!free(1) && !free(4) && !free(8) && free(5) && free(11),
            "An instance is free exactly while a source or the axis is");
}

Handle(TDocStd_Document) fresh(const Handle(TDocStd_Application)& app) {
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinOcaf", doc);
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}

void persistence(const Handle(TDocStd_Application)& app) {
    auto model = ModelDocument::empty();
    auto sketch = patterned();
    regenerateSketchPatterns(sketch.elements, sketch.patterns);
    (void)model.addDatumSketchFeature("Muster", sketch);
    const auto expected =
        IdentifiedDocumentSnapshot::restore(DocumentId{3, 4}, DocumentSnapshot::capture(model));
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    require(app->SaveAs(doc, bytes) == PCDM_SS_OK, "Native save");
    Handle(TDocStd_Document) loaded;
    require(app->Open(bytes, loaded) == PCDM_RS_OK, "Native open");
    require(NativePrimitiveCodec::read(loaded) == expected, "Pattern roundtrip");
    app->Close(loaded);
    TDataStd_Integer::Set(doc->Main().FindChild(2), 10);
    rejected([&] { (void)NativePrimitiveCodec::read(doc); }, "Schema 10 accepted a pattern");
    app->Close(doc);

    // Without patterns nothing new is written; the document still reads as schema 10.
    auto plainModel = ModelDocument::empty();
    auto plainSketch = patterned();
    plainSketch.patterns = {};
    (void)plainModel.addDatumSketchFeature("Ohne", plainSketch);
    const auto plain = IdentifiedDocumentSnapshot::restore(DocumentId{5, 6},
                                                           DocumentSnapshot::capture(plainModel));
    auto old = fresh(app);
    NativePrimitiveCodec::write(old, plain);
    TDataStd_Integer::Set(old->Main().FindChild(2), 10);
    require(NativePrimitiveCodec::read(old) == plain, "Schema 10 document without patterns");
    app->Close(old);
}

} // namespace

int main() {
    try {
        rules();
        solving();
        Handle(TDocStd_Application) app = new TDocStd_Application;
        BinDrivers::DefineFormat(app);
        persistence(app);
        std::cout << "PASS sketch patterns: rules, solver, schema 11\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
