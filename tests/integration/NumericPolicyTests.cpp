// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>
#include <type_traits>
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchProfiles.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

namespace {
using namespace voelund;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> bool rejects(F action) {
    try {
        action();
    } catch (const std::exception& error) {
        return std::string(error.what()).find("[0.001, 1000000]") != std::string::npos;
    }
    return false;
}
void ranges() {
    for (double size : {0.000999, 0.001, 1000000.0, 1000000.1}) {
        const bool valid = kernel::modelLength(size);
        document::ModelDocument doc;
        const auto revision = doc.revision();
        const auto original = doc.boxDimensions(document::FeatureId{1});
        const bool documentRejected = rejects([&] {
            static_cast<void>(doc.setBoxDimensions(document::FeatureId{1}, {size, size, size}));
        });
        const bool kernelRejected = rejects([&] {
            const auto shape = kernel::ShapeFactory::makeBox({size, size, size});
            require(BRepCheck_Analyzer(shape).IsValid(), "Range boundary box invalid");
        });
        std::ostringstream bytes;
        bytes << std::setprecision(17) << "VOLUND 1\nNEXT_ID 2\nFEATURES 1\nBOX 1 \"Box\" " << size
              << ' ' << size << ' ' << size << " 0 0 0 0 0 0\n";
        const bool importRejected = rejects([&] {
            std::istringstream input(bytes.str());
            static_cast<void>(document::DocumentSerializer::read(input));
        });
        require(documentRejected == !valid && kernelRejected == !valid && importRejected == !valid,
                "Document/import/kernel disagree on size range");
        if (!valid)
            require(doc.revision() == revision &&
                        doc.boxDimensions(document::FeatureId{1}) == original,
                    "Invalid size mutated document");
    }
}
document::SketchElement line(std::uint64_t id, document::SketchPoint a, document::SketchPoint b) {
    return {document::SketchElementId{id}, document::SketchLine{a, b}};
}
void profiles() {
    for (double size : {0.001, 10.0, 100000.0}) {
        const double offset = size * 2;
        const double tolerance = kernel::connectionTolerance(offset + size).millimetres;
        for (double factor : {0.99, 1.01}) {
            const double gap = factor * tolerance;
            document::SketchParameters sketch{
                {0, 0, 0, 0, 0, 1, 1, 0, 0},
                {{line(1, {offset, 0}, {offset + size, 0}),
                  line(2, {offset + size, gap}, {offset + size, size}),
                  line(3, {offset + size, size}, {offset, size}),
                  line(4, {offset, size}, {offset, 0})},
                 5}};
            // The gap shortens an edge: keep every edge within the input range.
            if (size == 0.001) {
                std::get<document::SketchLine>(sketch.elements.values[1].geometry).end.y *= 2;
                std::get<document::SketchLine>(sketch.elements.values[2].geometry).start.y *= 2;
                std::get<document::SketchLine>(sketch.elements.values[2].geometry).end.y *= 2;
                std::get<document::SketchLine>(sketch.elements.values[3].geometry).start.y *= 2;
            }
            const auto original = sketch.elements;
            const auto detected = features::detectClosedProfiles(sketch.elements.values);
            require(detected.empty() == (factor > 1), "Profile threshold classification mismatch");
            bool built = false;
            try {
                const auto faces = features::evaluateSketchProfileFaces(sketch);
                require(BRepCheck_Analyzer(faces).IsValid(), "Near-connected face is invalid");
                std::size_t edgeCount = 0;
                for (TopExp_Explorer edges(faces, TopAbs_EDGE); edges.More(); edges.Next())
                    ++edgeCount;
                require(edgeCount == 4, "Wire reconstruction added an edge");
                GProp_GProps lengths;
                BRepGProp::LinearProperties(faces, lengths);
                const double expected = (size == 0.001 ? 6 : 4) * size - gap;
                require(std::abs(lengths.Mass() - expected) < size * 1e-12,
                        "Wire changed original curve lengths by closing a gap");
                built = true;
            } catch (const features::GeometryEvaluationError&) {
            }
            require(built == (factor < 1), "Classification and real face construction disagree");
            require(sketch.elements == original, "Profile evaluation moved stored endpoints");
        }
    }
}
void curvedProfiles() {
    for (double size : {0.01, 10.0, 100000.0}) {
        for (double factor : {0.99, 1.01}) {
            const double gap = factor * kernel::connectionTolerance(size).millimetres;
            document::SketchParameters sketch{
                {0, 0, 0, 0, 0, 1, 1, 0, 0},
                {{{document::SketchElementId{1},
                   document::SketchArc{{0, 0}, size, 0, std::numbers::pi}},
                  line(2, {-size, gap}, {size, gap})},
                 3}};
            const auto detected = features::detectClosedProfiles(sketch.elements.values);
            require(detected.empty() == (factor > 1), "Arc profile classification mismatch");
            bool built = false;
            try {
                const auto faces = features::evaluateSketchProfileFaces(sketch);
                require(BRepCheck_Analyzer(faces).IsValid(), "Arc face invalid");
                GProp_GProps lengths;
                BRepGProp::LinearProperties(faces, lengths);
                require(std::abs(lengths.Mass() - size * (2 + std::numbers::pi)) < size * 1e-12,
                        "Arc profile changed its original curves");
                built = true;
            } catch (const features::GeometryEvaluationError&) {
            }
            require(built == (factor < 1), "Arc classification and construction disagree");
        }
    }
}
void editorRejectsRangesAtomically() {
    application::CadSession session;
    application::SketchEditorSession editor(session, document::FeatureId{1},
                                            {0, 0, 20, 0, 0, 1, 1, 0, 0});
    require(editor.setPreview({0, 0}, {5, 0}) && editor.confirmPreview().has_value() &&
                editor.selectElement(document::SketchElementId{1}) &&
                editor.setSelectedLineLength(6),
            "Editor range fixture failed");
    const auto constraint = editor.constraints().front().id;
    const auto before = editor.draftParameters();
    for (double value : {0.000999, 1000000.1}) {
        require(!editor.setSelectedLineLength(value) &&
                    !editor.setConstraintValue(constraint, value) &&
                    editor.draftParameters() == before && editor.canAccept(),
                "Rejected editor size changed the draft");
    }
    require(!editor.translateSelected(1000001, 0) && editor.draftParameters() == before &&
                editor.canAccept(),
            "Rejected editor translation changed draft or solve status");
    require(!editor.setPreview({0, 0}, {0.0005, 0}) && editor.draftParameters() == before,
            "Subminimum preview changed draft");
}
void directionalDistancesRejectSupportMetadata() {
    for (auto kind : {document::SketchConstraintKind::pointDistanceX,
                      document::SketchConstraintKind::pointDistanceY}) {
        document::ModelDocument doc;
        document::SketchParameters sketch{
            {0, 0, 20, 0, 0, 1, 1, 0, 0},
            {{line(1, {0, 0}, {5, 5})}, 2},
            {{{document::SketchConstraintId{1},
               kind,
               {document::SketchElementId{1}, document::SketchPointPosition::start},
               document::SketchGeometryReference{document::SketchElementId{1},
                                                 document::SketchPointPosition::end},
               5}},
             2}};
        const auto id = doc.addSketchFeature("Distance", document::FeatureId{1}, sketch);
        const auto revision = doc.revision();
        const auto original = doc.sketchParameters(id);
        sketch.constraints.values.front().auxiliaryValues = {1, 0.5};
        bool rejected = false;
        try {
            static_cast<void>(doc.setSketchParameters(id, sketch));
        } catch (const std::invalid_argument& error) {
            rejected = std::string(error.what()).find("auxiliary values") != std::string::npos;
        }
        require(rejected && doc.revision() == revision && doc.sketchParameters(id) == original,
                "Directional distance accepted support-only metadata or changed document");
    }
}
} // namespace
int main() {
    static_assert(!std::is_convertible_v<kernel::PickingTolerance, kernel::ConnectionTolerance>);
    static_assert(!std::is_convertible_v<kernel::AngularTolerance, kernel::ConnectionTolerance>);
    try {
        ranges();
        profiles();
        curvedProfiles();
        editorRejectsRangesAtomically();
        directionalDistancesRejectSupportMetadata();
        std::cout << "PASS numeric ranges and actual profile threshold faces; kernel floor "
                  << Precision::Confusion() << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
