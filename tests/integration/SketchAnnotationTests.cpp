#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// S07: one annotation projection for every constraint kind (category, label, value,
// editability, place on the geometry); redundant relations are reported instead of a wrong
// degree-of-freedom count; a dimension is edited as one checked step whose refusals name the
// reason and change nothing.
namespace {
using namespace voelund;
using application::SketchEditorSession;
using document::SketchArc;
using document::SketchCircle;
using document::SketchConstraint;
using document::SketchConstraintId;
using document::SketchConstraintKind;
using document::SketchElementId;
using document::SketchGeometryReference;
using document::SketchLine;
using features::SketchAnnotationCategory;
using Position = document::SketchPointPosition;
using Kind = SketchConstraintKind;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
SketchGeometryReference ref(std::uint64_t element, Position point = Position::start) {
    return {SketchElementId{element}, point};
}
SketchConstraint relation(std::uint64_t id, Kind kind, SketchGeometryReference first,
                          std::optional<SketchGeometryReference> second = std::nullopt,
                          double value = 0.0) {
    return {SketchConstraintId{id}, kind, first, second, value};
}

struct Fixture final {
    application::CadSession session;
    std::unique_ptr<SketchEditorSession> editor;
    Fixture(const std::vector<document::SketchGeometry>& geometry,
            std::vector<SketchConstraint> constraints) {
        session.newDocument();
        editor = std::make_unique<SketchEditorSession>(session, document::DatumPlane::xy);
        document::SketchElements elements;
        for (const auto& item : geometry)
            elements.values.push_back({SketchElementId{elements.nextId++}, item});
        document::SketchConstraints values{std::move(constraints), 0};
        values.nextId = values.values.size() + 1;
        require(editor->loadDraft(std::move(elements), std::move(values)), "Draft loads");
    }
    void refused(std::uint64_t id, double value, const std::string& reason) {
        const auto before = editor->draftParameters();
        try {
            static_cast<void>(editor->editDimension(SketchConstraintId{id}, value));
        } catch (const std::invalid_argument& error) {
            require(std::string(error.what()).find(reason) != std::string::npos,
                    "Expected \"" + reason + "\", got \"" + error.what() + "\"");
            require(editor->draftParameters() == before, "No change for " + reason);
            return;
        }
        throw std::runtime_error("Not refused: " + reason);
    }
};

// The expected label prefix of each kind; a new kind fails to compile here (-Wswitch).
const char* expectedPrefix(Kind kind) {
    switch (kind) {
    case Kind::horizontal: return "[H]";
    case Kind::vertical: return "[V]";
    case Kind::distance: return "L ";
    case Kind::radius: return "R ";
    case Kind::coincident: return "[C]";
    case Kind::parallel: return "[||]";
    case Kind::perpendicular: return "[90]";
    case Kind::coordinateX: return "X ";
    case Kind::coordinateY: return "Y ";
    case Kind::pointOnObject: return "[O]";
    case Kind::equal: return "[=]";
    case Kind::tangent: return "[T]";
    case Kind::symmetric: return "[S]";
    case Kind::pointDistance: return "D ";
    case Kind::angle: return "A ";
    case Kind::pointDistanceX: return "DX ";
    case Kind::pointDistanceY: return "DY ";
    case Kind::midpoint: return "[M]";
    case Kind::perpendicularBisector: return "[PB]";
    case Kind::concentric: return "[CC]";
    case Kind::diameter: return "Ø ";
    case Kind::collinear: return "[CL]";
    case Kind::horizontalPoints: return "[HP]";
    case Kind::verticalPoints: return "[VP]";
    case Kind::fixed: return "[F]";
    case Kind::pointLineDistance: return "PL ";
    }
    return "";
}

void everyKind() {
    Fixture f({SketchLine{{0, 0}, {10, 0}}, SketchLine{{0, 0}, {0, 8}}, SketchCircle{{20, 0}, 3},
               SketchArc{{30, 0}, 4, 0, std::numbers::pi}, SketchLine{{5, 5}, {15, 5}},
               SketchLine{{40, 0}, {50, 0}}, SketchCircle{{20, 10}, 3}},
              {});
    require(f.session.createParameter("Breite", document::ParameterUnit::length, 10).status() ==
                application::CommandStatus::committed,
            "Parameter");
    const auto parameter = f.session.parameterTable().values.front().id;
    std::vector<SketchConstraint> all{
        relation(1, Kind::horizontal, ref(1)),
        relation(2, Kind::vertical, ref(2)),
        relation(3, Kind::distance, ref(1), std::nullopt, 10),
        relation(4, Kind::radius, ref(3, Position::center), std::nullopt, 3),
        relation(5, Kind::coincident, ref(1), ref(2)),
        relation(6, Kind::parallel, ref(1), ref(5)),
        relation(7, Kind::perpendicular, ref(1), ref(2)),
        relation(8, Kind::coordinateX, ref(5), std::nullopt, 5),
        relation(9, Kind::coordinateY, ref(5), std::nullopt, 5),
        relation(10, Kind::pointOnObject, ref(5, Position::end), ref(3)),
        relation(11, Kind::equal, ref(3), ref(7)),
        relation(12, Kind::tangent, ref(6), ref(4)),
        {SketchConstraintId{13}, Kind::symmetric, ref(2, Position::end), ref(5), 0.0, ref(6)},
        relation(14, Kind::pointDistance, ref(1), ref(5, Position::end), 15),
        {SketchConstraintId{15}, Kind::angle, ref(1), ref(2), kernel::Radians(0.5)},
        relation(16, Kind::pointDistanceX, ref(1), ref(5), 5),
        relation(17, Kind::pointDistanceY, ref(1), ref(5), -5),
        relation(18, Kind::midpoint, ref(3, Position::center), ref(6)),
        relation(19, Kind::perpendicularBisector, ref(2), ref(6)),
        relation(20, Kind::concentric, ref(3, Position::center), ref(4, Position::center)),
        relation(21, Kind::diameter, ref(7, Position::center), std::nullopt, 6),
        relation(22, Kind::collinear, ref(1), ref(6)),
        relation(23, Kind::horizontalPoints, ref(1), ref(6)),
        relation(24, Kind::verticalPoints, ref(2, Position::end), ref(5)),
        {SketchConstraintId{25}, Kind::fixed, ref(6), std::nullopt, 40, std::nullopt, {0, 50, 0}},
        relation(26, Kind::pointLineDistance, ref(5), ref(1), 5),
        // Not editable: bound to a parameter, following a support edge.
        relation(27, Kind::distance, ref(5), std::nullopt, 10),
        {SketchConstraintId{28}, Kind::coordinateY, ref(2, Position::end), std::nullopt, 8,
         std::nullopt, {1, 0.5}}};
    all[26].parameter = parameter;
    document::SketchElements elements{f.editor->draftParameters().elements};
    document::SketchConstraints constraints{all, all.size() + 1};
    require(f.editor->loadDraft(elements, constraints), "All kinds load");

    const auto annotations = f.editor->evaluatedConstraints();
    require(annotations.size() == all.size(), "One annotation per constraint");
    for (std::size_t index = 0; index < all.size(); ++index) {
        const auto& constraint = all[index];
        const auto& annotation = annotations[index];
        const std::string name = "Kind " + std::to_string(static_cast<int>(constraint.kind));
        require(annotation.id == constraint.id && annotation.kind == constraint.kind,
                name + ": in order");
        require(annotation.label.starts_with(expectedPrefix(constraint.kind)),
                name + ": label " + annotation.label);
        const bool dimension = annotation.category == SketchAnnotationCategory::dimension;
        const bool withUnit =
            annotation.label.ends_with(" mm") || annotation.label.ends_with("°");
        require(dimension == withUnit && dimension != annotation.label.starts_with("["),
                name + ": a dimension shows a value with unit, a relation a symbol");
        const double expected = !dimension                   ? 0.0
                                : constraint.kind == Kind::angle ? constraint.angleValue.value()
                                                                 : constraint.value;
        require(near(annotation.value, expected), name + ": value");
        require(annotation.editable == (dimension && index < 26), name + ": editable");
    }
    require(annotations[14].label == "A 28.648°", "Angles in degrees: " + annotations[14].label);
    require(annotations[20].label == "Ø 6.000 mm", "Diameter shows the diameter");
    require(annotations[26].parameter == parameter, "The binding is part of the projection");
    // Element relations sit on the element, not on an end point.
    require(near(annotations[5].position.X(), 5) && near(annotations[5].position.Y(), 0),
            "Parallel on the middle of its first line");
    require(near(annotations[11].position.X(), 30) && near(annotations[11].position.Y(), 0),
            "Tangent at the foot of the arc's center on the line");

    // Refusals before any change.
    f.refused(6, 1, "Beziehung hat keinen Wert");
    // PAR02 (rule 7): a bound dimension takes a number, which unbinds it.
    require(!f.editor->dimensionProblem(SketchConstraintId{27}), "A bound dimension can be edited");
    f.refused(28, 3, "Referenzkante");
}

void editing() {
    Fixture f({SketchLine{{0, 0}, {10, 0}}, SketchLine{{0, 0}, {0, 5}}},
              {relation(1, Kind::distance, ref(1), std::nullopt, 10),
               relation(2, Kind::horizontal, ref(1)),
               relation(3, Kind::coincident, ref(1), ref(2)),
               {SketchConstraintId{4}, Kind::angle, ref(1), ref(2),
                kernel::Radians(std::numbers::pi / 2)},
               relation(5, Kind::coordinateX, ref(1), std::nullopt, 0)});
    const auto before = f.editor->draftParameters();
    require(f.editor->editDimension(SketchConstraintId{1}, 12), "Change the length");
    const auto& line = std::get<SketchLine>(f.editor->elements().front().geometry);
    require(near(std::hypot(line.end.x - line.start.x, line.end.y - line.start.y), 12),
            "The geometry follows the dimension");
    f.editor->commitHistory();
    require(f.editor->undo() && f.editor->draftParameters() == before, "Undo is one step");
    require(f.editor->redo(), "Redo");
    require(!f.editor->editDimension(SketchConstraintId{1}, 12), "The same value is no change");
    require(f.editor->editDimension(SketchConstraintId{5}, 3), "Coordinates are dimensions");
    require(near(std::get<SketchLine>(f.editor->elements().front().geometry).start.x, 3),
            "The point moved to x = 3");
    f.refused(1, -2, "größer als 0");
    f.refused(4, 4.0, "zwischen 0° und 180°");
    f.refused(1, std::nan(""), "keine Zahl");

    // A dimension the sketch cannot satisfy is refused and leaves the sketch solved.
    Fixture fixed({SketchLine{{0, 0}, {10, 0}}},
                  {{SketchConstraintId{1}, Kind::fixed, ref(1), std::nullopt, 0, std::nullopt,
                    {0, 10, 0}},
                   relation(2, Kind::distance, ref(1), std::nullopt, 10)});
    require(!fixed.editor->lastSolveResult().redundantConstraints.empty() &&
                fixed.editor->isConstraintRedundant(SketchConstraintId{2}),
            "A length on a fixed line is redundant and reported");
    fixed.refused(2, 12, "nicht lösen");
    require(fixed.editor->lastSolveResult().status == features::SketchSolveStatus::success,
            "The sketch stays solved");
}

void states() {
    Fixture free({SketchLine{{0, 0}, {10, 0}}}, {relation(1, Kind::horizontal, ref(1))});
    require(free.editor->lastSolveResult().degreesOfFreedom == 3 &&
                free.editor->lastSolveResult().redundantConstraints.empty(),
            "A horizontal line keeps three degrees of freedom");
    Fixture determined({SketchLine{{0, 0}, {10, 0}}},
                       {{SketchConstraintId{1}, Kind::fixed, ref(1), std::nullopt, 0,
                         std::nullopt, {0, 10, 0}}});
    require(determined.editor->lastSolveResult().degreesOfFreedom == 0 &&
                determined.editor->lastSolveResult().redundantConstraints.empty() &&
                determined.editor->isElementDetermined(SketchElementId{1}),
            "A fixed line is fully determined");
    Fixture twice({SketchLine{{0, 0}, {10, 0}}},
                  {relation(1, Kind::horizontal, ref(1)),
                   relation(2, Kind::horizontalPoints, ref(1), ref(1, Position::end))});
    require(twice.editor->lastSolveResult().status == features::SketchSolveStatus::success &&
                twice.editor->lastSolveResult().redundantConstraints.size() == 1,
            "Horizontal twice: one redundant relation");
}

} // namespace

int main() {
    try {
        everyKind();
        editing();
        states();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
