// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/document/NativeExtrusion.hpp"
#include "../../src/document/NativePrimitiveCodec.hpp"
#include "../../src/document/NativeSketchGeometry.hpp"
#include "NativeCodecTestSupport.hpp"
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BinDrivers.hxx>
#include <GProp_GProps.hxx>
#include <PCDM_ReaderStatus.hxx>
#include <PCDM_StoreStatus.hxx>
#include <TDF_Reference.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <TDataStd_RealArray.hxx>
#include <TDocStd_Application.hxx>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
#include <sstream>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/features/SketchEvaluation.hpp>

namespace {
using namespace voelund;
using namespace document;
using detail::NativePrimitiveCodec;
using namespace test::native_codec;
IdentifiedDocumentSnapshot fixture() {
    auto model = ModelDocument::empty();
    for (int index = 0; index < 3; ++index) {
        auto support = model.addBoxFeature("Körper Ω", {40, 30, 20});
        const auto sketch =
            model.addSketchFeature("Profile Ω", support,
                                   {{0, 0, 20, 0, 0, 1, 1, 0, 0},
                                    {{{SketchElementId{9}, SketchRectangle{1, 1, 5, 5}},
                                      {SketchElementId{2}, SketchRectangle{10, 10, 13, 14}}},
                                     10}});
        std::vector<SketchProfileId> profiles;
        if (index > 0)
            profiles.emplace_back(9);
        if (index > 1)
            profiles.emplace_back(2);
        support = model.addExtrusionFeature("Extrusion Ω", sketch, {3, profiles});
        if (index == 2) {
            const auto next =
                model.addSketchFeature("Auf Extrusion", support,
                                       {{0, 0, 23, 0, 0, 1, 1, 0, 0},
                                        {{{SketchElementId{7}, SketchRectangle{2, 2, 4, 4}}}, 8}});
            (void)model.addExtrusionFeature("Folgeextrusion", next, {2, {SketchProfileId{7}}});
        }
    }
    return IdentifiedDocumentSnapshot::restore(DocumentId{UINT64_MAX, 0x0123456789abcdefULL},
                                               DocumentSnapshot::capture(model));
}
void check(const Handle(TDocStd_Document) & doc, const IdentifiedDocumentSnapshot& expected) {
    const auto actual = NativePrimitiveCodec::read(doc);
    require(actual == expected, "Extrusion snapshot changed");
    const auto result = features::FeatureGraph{}.evaluate(actual.snapshot().toDocument());
    require(result.bodies.size() == 3, "Extrusions changed body count");
    const double volumes[]{24084, 24048, 24092};
    const std::uint64_t bodies[]{1, 4, 7}, producers[]{3, 6, 11};
    for (std::size_t index = 0; index < result.bodies.size(); ++index) {
        const auto& body = result.bodies[index];
        GProp_GProps properties;
        BRepGProp::VolumeProperties(body.shape, properties);
        require(body.bodyId.value() == bodies[index] &&
                    body.sourceFeature.value() == producers[index] &&
                    BRepCheck_Analyzer(body.shape).IsValid() &&
                    std::abs(properties.Mass() - volumes[index]) < 1e-7,
                "Extrusion identity, BRep or analytical volume changed");
    }
    for (std::size_t index : {1U, 4U, 7U, 9U})
        require(!result.features[index].bodyId, "Sketch acquired a solid body output");
}
void historicalProfiles(const Handle(TDocStd_Application) & app) {
    auto model = fixture().snapshot().toDocument();
    require(model.setExtrusionParameters(FeatureId{3},
                                         {3,
                                          {SketchProfileId{UINT64_MAX},
                                           SketchProfileId{UINT64_MAX - 1}, SketchProfileId{99}}}),
            "Could not set historical selection");
    const auto expected =
        IdentifiedDocumentSnapshot::restore(fixture().id(), DocumentSnapshot::capture(model));
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    auto loaded = open(app, bytes);
    require(NativePrimitiveCodec::read(loaded) == expected, "Historical profiles lost or repaired");
    bool failed = false;
    try {
        (void)features::FeatureGraph{}.evaluate(
            NativePrimitiveCodec::read(loaded).snapshot().toDocument());
    } catch (const features::FeatureEvaluationError&) {
        failed = true;
    }
    require(failed, "Unknown profile selection unexpectedly evaluated");
    app->Close(loaded);
    app->Close(doc);
}
// R09 schema 5: region selections, chunked signatures and the schema-4 reading path.
IdentifiedDocumentSnapshot regionFixture(std::vector<SketchProfileId> profiles,
                                         std::vector<SketchRegionId> regions) {
    auto model = ModelDocument::empty();
    const auto support = model.addBoxFeature("Support", {40, 30, 20});
    const auto sketch = model.addSketchFeature("Ring", support,
                                               {{0, 0, 20, 0, 0, 1, 1, 0, 0},
                                                {{{SketchElementId{1}, SketchRectangle{1, 1, 9, 7}},
                                                  {SketchElementId{2}, SketchCircle{{5, 4}, 1}}},
                                                 3}});
    (void)model.addExtrusionFeature("Extrusion", sketch,
                                    {5, std::move(profiles), std::move(regions)});
    return IdentifiedDocumentSnapshot::restore(DocumentId{7, 9}, DocumentSnapshot::capture(model));
}
SketchRegionId region(const char* text) {
    const auto id = SketchRegionId::parse(text);
    require(id.has_value(), "Region fixture text");
    return *id;
}
void regionSelections(const Handle(TDocStd_Application) & app) {
    const auto ring = region("o:1.0,1.1,1.2,1.3;h:2.0+");
    const auto expected = regionFixture({}, {ring, region("o:2.0+")});
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    auto loaded = open(app, bytes);
    require(NativePrimitiveCodec::read(loaded) == expected, "Region selection roundtrip");
    app->Close(loaded);
    const auto result = features::FeatureGraph{}.evaluate(expected.snapshot().toDocument());
    GProp_GProps properties;
    BRepGProp::VolumeProperties(result.bodies.front().shape, properties);
    // The selected inner disc fills the ring's hole: one face, no touching second solid.
    require(
        features::evaluateRegionFaces(expected.snapshot().sketchParameters(FeatureId{2}),
                                      expected.snapshot().extrusionParameters(FeatureId{3}).regions)
                .size() == 1,
        "Selected inner region did not fill the hole");
    require(std::abs(properties.Mass() - (24000 + 48 * 5)) < 1e-6,
            "Ring and inner disc together fill the rectangle");
    const auto ringOnly = regionFixture({}, {ring});
    const auto ringResult = features::FeatureGraph{}.evaluate(ringOnly.snapshot().toDocument());
    BRepGProp::VolumeProperties(ringResult.bodies.front().shape, properties);
    require(BRepCheck_Analyzer(ringResult.bodies.front().shape).IsValid() &&
                std::abs(properties.Mass() - (24000 + (48 - std::numbers::pi) * 5)) < 1e-6,
            "Ring region extrudes with its hole");

    // Signatures beyond one attribute are chunked; every chunk but the last is full.
    std::vector<SketchRegionBoundaryUse> uses;
    for (std::uint64_t element = 1; element <= 2000; ++element)
        uses.push_back({SketchElementId{element}, 0, std::nullopt});
    const auto large = SketchRegionId::fromBoundaries(uses, {});
    require(large.text().size() > 8000, "Large signature fixture");
    const auto chunked = regionFixture({}, {large});
    auto other = fresh(app);
    NativePrimitiveCodec::write(other, chunked);
    require(NativePrimitiveCodec::read(other) == chunked, "Chunked signature roundtrip");
    app->Close(other);

    const auto payload = feature(doc, 3).FindChild(4);
    const auto regions = payload.FindChild(2);
    doc->SetUndoLimit(10);
    const auto corrupt = [&](auto mutate) {
        doc->NewCommand();
        mutate();
        rejected([&] { (void)NativePrimitiveCodec::read(doc); });
        doc->AbortCommand();
        require(NativePrimitiveCodec::read(doc) == expected, "Rejected edit damaged document");
    };
    for (const char* text : {"o:1.1,1.0", "o:2.0+ ", "o:", "o:2.0+;h:", "o:0.0"})
        corrupt([&] { TDataStd_AsciiString::Set(regions.FindChild(2).FindChild(1), text); });
    corrupt([&] {
        TDataStd_AsciiString::Set(regions.FindChild(2).FindChild(1), ring.text().c_str());
    }); // duplicate
    for (int count : {-1, 0, 1, 3})
        corrupt([&] { TDataStd_Integer::Set(regions, count); });
    corrupt([&] { TDataStd_Integer::Set(regions.FindChild(1), 2); });
    corrupt([&] { TDataStd_Integer::Set(doc->Main().FindChild(2), 4); }); // regions on schema 4
    rejected([&] { detail::validateNativeExtrusion({1, {SketchProfileId{1}}, {ring}}); });
    rejected([&] { detail::validateNativeExtrusion({1, {}, {ring, ring}}); });

    // Without regions the extrusion keeps the schema-4 encoding; legacy profiles read unchanged.
    const auto legacy = regionFixture({SketchProfileId{1}}, {});
    auto old = fresh(app);
    NativePrimitiveCodec::write(old, legacy);
    TDataStd_Integer::Set(old->Main().FindChild(2), 4);
    std::stringstream oldBytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, old, oldBytes);
    auto reopened = open(app, oldBytes);
    require(NativePrimitiveCodec::read(reopened) == legacy, "Schema 4 legacy profiles");
    app->Close(reopened);
    app->Close(old);
    app->Close(doc);
    std::cout << "schema 5 regions, chunking, ring evaluation and schema-4 reading passed\n";
}
// V01a schema 6: a base-plane sketch has no input reference and opens its own body.
void datumSketches(const Handle(TDocStd_Application) & app) {
    auto model = ModelDocument::empty();
    SketchParameters sketch{datumSketchPlane(DatumPlane::xz),
                            {{{SketchElementId{1}, SketchRectangle{0, 0, 4, 3}}}, 2}};
    sketch.datum = DatumPlane::xz;
    const auto sketchId = model.addDatumSketchFeature("Base", sketch);
    (void)model.addExtrusionFeature("Body", sketchId, {2, {}, {region("o:1.0,1.1,1.2,1.3")}});
    const auto expected =
        IdentifiedDocumentSnapshot::restore(DocumentId{3, 4}, DocumentSnapshot::capture(model));
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    auto loaded = open(app, bytes);
    require(NativePrimitiveCodec::read(loaded) == expected, "Datum sketch roundtrip");
    app->Close(loaded);
    const auto result = features::FeatureGraph{}.evaluate(expected.snapshot().toDocument());
    GProp_GProps properties;
    BRepGProp::VolumeProperties(result.bodies.front().shape, properties);
    require(result.bodies.size() == 1 && result.bodies.front().bodyId == BodyId{1} &&
                std::abs(properties.Mass() - 24) < 1e-9,
            "Datum body identity and volume");
    TDataStd_Integer::Set(doc->Main().FindChild(2), 5);
    rejected([&] { (void)NativePrimitiveCodec::read(doc); }); // datum needs schema 6
    TDataStd_Integer::Set(doc->Main().FindChild(2), 6);
    auto shifted = sketch;
    shifted.plane.originY = 1;
    rejected([&] { detail::validateNativeSketchGeometry(shifted); });
    auto bound = sketch;
    bound.repairReason = SketchRepairReason::legacySupport;
    rejected([&] { detail::validateNativeSketchGeometry(bound); });
    app->Close(doc);
    std::cout << "schema 6 datum sketch, own body, schema gate and plane checks passed\n";
}
// T02, schema 8: operation, reversed direction and target body as optional children. A cut
// from a second base-plane sketch into the first body round-trips, groups its sketch with
// that body and is refused by a schema 7 reader.
void extrusionModes(const Handle(TDocStd_Application) & app) {
    auto model = ModelDocument::empty();
    SketchParameters plate{datumSketchPlane(DatumPlane::xy),
                           {{{SketchElementId{1}, SketchRectangle{0, 0, 4, 3}}}, 2}};
    plate.datum = DatumPlane::xy;
    const auto plateSketch = model.addDatumSketchFeature("Plate", plate);
    ExtrusionParameters body{2, {}, {region("o:1.0,1.1,1.2,1.3")}};
    body.operation = ExtrusionOperation::newBody;
    (void)model.addExtrusionFeature("Body", plateSketch, body);
    SketchParameters hole{datumSketchPlane(DatumPlane::xy),
                          {{{SketchElementId{1}, SketchRectangle{1, 1, 2, 2}}}, 2}};
    hole.datum = DatumPlane::xy;
    const auto holeSketch = model.addDatumSketchFeature("Hole", hole);
    ExtrusionParameters cut{2, {}, {region("o:1.0,1.1,1.2,1.3")}};
    cut.operation = ExtrusionOperation::cut;
    cut.target = BodyId{plateSketch.value()};
    const auto cutId = model.addExtrusionFeature("Cut", holeSketch, cut);
    auto reversed = cut;
    reversed.reversed = true;
    reversed.distance = 1;
    require(model.setExtrusionParameters(cutId, reversed), "Reversed cut set");
    auto stray = cut;
    stray.target = BodyId{99};
    rejected([&] { (void)model.setExtrusionParameters(cutId, stray); });
    auto newBodyTarget = cut;
    newBodyTarget.operation = ExtrusionOperation::newBody;
    rejected([&] { (void)model.setExtrusionParameters(cutId, newBodyTarget); });
    const auto expected =
        IdentifiedDocumentSnapshot::restore(DocumentId{3, 5}, DocumentSnapshot::capture(model));
    Handle(TDocStd_Document) doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    Handle(TDocStd_Document) loaded = open(app, bytes);
    require(NativePrimitiveCodec::read(loaded) == expected, "Extrusion mode roundtrip");
    app->Close(loaded);
    // Reversed, the hole is cut downwards from z = 0 and misses the plate above: an error,
    // not a silent no-op. Along the normal it cuts 1 x 1 x 1 from the bottom up.
    bool missed = false;
    try {
        (void)features::FeatureGraph{}.evaluate(expected.snapshot().toDocument());
    } catch (const features::FeatureEvaluationError& error) {
        missed = error.featureId() == cutId;
    }
    reversed.reversed = false;
    require(model.setExtrusionParameters(cutId, reversed), "Upward cut set");
    const auto result = features::FeatureGraph{}.evaluate(model);
    GProp_GProps properties;
    BRepGProp::VolumeProperties(result.resultShape, properties);
    require(missed && result.bodies.size() == 1 && std::abs(properties.Mass() - 23) < 1e-9,
            "Cut direction and target body");
    TDataStd_Integer::Set(doc->Main().FindChild(2), 7);
    rejected([&] { (void)NativePrimitiveCodec::read(doc); }); // T02 children need schema 8
    app->Close(doc);

    // T03, schema 9: start offset with its binding, and an end face on the plate.
    ParameterTable table{{{ParameterId{1}, "V", ParameterUnit::length, -0.5}}, 2};
    require(model.setParameterTable(table), "Offset parameter");
    reversed.offset = -0.5;
    reversed.offsetParameter = ParameterId{1};
    reversed.upToFace =
        BodyFaceReference{BodyId{plateSketch.value()},
                          {FeatureId{plateSketch.value() + 1}, std::nullopt, {SketchElementId{1}}}};
    require(model.setExtrusionParameters(cutId, reversed), "Offset and end face set");
    auto foreign = reversed;
    foreign.upToFace->body = BodyId{99};
    rejected([&] { (void)model.setExtrusionParameters(cutId, foreign); });
    const auto withEnd =
        IdentifiedDocumentSnapshot::restore(DocumentId{3, 6}, DocumentSnapshot::capture(model));
    doc = fresh(app);
    NativePrimitiveCodec::write(doc, withEnd);
    std::stringstream endBytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, endBytes);
    loaded = open(app, endBytes);
    require(NativePrimitiveCodec::read(loaded) == withEnd, "Offset and end face roundtrip");
    app->Close(loaded);
    // From z = -0.5 up to the plate's top cap at z = 2: the 1 x 1 hole goes through.
    const auto through = features::FeatureGraph{}.evaluate(withEnd.snapshot().toDocument());
    GProp_GProps throughVolume;
    BRepGProp::VolumeProperties(through.resultShape, throughVolume);
    require(std::abs(throughVolume.Mass() - 22) < 1e-9, "Cut from the offset up to the top face");
    TDataStd_Integer::Set(doc->Main().FindChild(2), 8);
    rejected([&] { (void)NativePrimitiveCodec::read(doc); }); // T03 children need schema 9
    app->Close(doc);
    std::cout << "schema 8/9 operation, direction, target, offset, end face and gates passed\n";
}
// T04, schema 10: push/pull. A face extrusion has no input feature; its profile is a box
// side, and the next one pulls the first one's end cap (role -2 in the file).
void faceExtrusions(const Handle(TDocStd_Application) & app) {
    auto model = ModelDocument::empty();
    const auto box = model.addBoxFeature("Box", {10, 10, 10});
    ExtrusionParameters pull{5, {}};
    pull.operation = ExtrusionOperation::join;
    pull.faceProfile =
        BodyFaceReference{BodyId{box.value()}, {box, kernel::PrimitiveRole::boxPositiveZ}};
    const auto first = model.addFaceExtrusionFeature("Pull", pull);
    auto again = pull;
    again.distance = 3;
    again.faceProfile->origin = {first, std::nullopt, {}, true};
    (void)model.addFaceExtrusionFeature("Again", again);
    // T04b: the +x side of the first pull, named by the box edge it was swept from.
    auto sideways = pull;
    sideways.distance = 2;
    sideways.faceProfile->origin = {first, std::nullopt, {}, true};
    sideways.faceProfile->origin.extrusionFace = ExtrusionFace::side;
    sideways.faceProfile->origin.sweptEdge = PrimitiveEdgeOrigin{
        box, kernel::PrimitiveRole::boxPositiveX, kernel::PrimitiveRole::boxPositiveZ};
    sideways.mergeSeams = true; // T05 (schema 36): merging changes no volume
    (void)model.addFaceExtrusionFeature("Sideways", sideways);
    auto targeted = pull;
    targeted.target = BodyId{box.value()};
    rejected([&] { (void)model.addFaceExtrusionFeature("Target", targeted); });
    auto newBody = pull;
    newBody.operation = ExtrusionOperation::newBody;
    rejected([&] { (void)model.addFaceExtrusionFeature("New", newBody); });
    auto foreign = pull;
    foreign.faceProfile->body = BodyId{99};
    rejected([&] { (void)model.addFaceExtrusionFeature("Foreign", foreign); });
    const auto expected =
        IdentifiedDocumentSnapshot::restore(DocumentId{3, 7}, DocumentSnapshot::capture(model));
    Handle(TDocStd_Document) doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    Handle(TDocStd_Document) loaded = open(app, bytes);
    require(NativePrimitiveCodec::read(loaded) == expected, "Face extrusion roundtrip");
    app->Close(loaded);
    const auto result = features::FeatureGraph{}.evaluate(expected.snapshot().toDocument());
    GProp_GProps properties;
    BRepGProp::VolumeProperties(result.resultShape, properties);
    require(result.bodies.size() == 1 && std::abs(properties.Mass() - 1900) < 1e-6,
            "Two pulls on a box and one on the first pull's side: 1800 + 2 x 10 x 5");
    TDataStd_Integer::Set(doc->Main().FindChild(2), 35); // T05: child 15 needs schema 36
    rejected([&] { (void)NativePrimitiveCodec::read(doc); });
    TDataStd_Integer::Set(doc->Main().FindChild(2), 9);
    rejected([&] { (void)NativePrimitiveCodec::read(doc); }); // T04 children need schema 10
    app->Close(doc);
    std::cout << "schema 10/36 push/pull cap, seams round-trip and gates passed" << std::endl;
}
// M02, schema 13: a symmetric intersect of a second base-plane sketch with the first body
// round-trips; a schema 12 reader refuses the symmetric child and operation 4; the model
// refuses symmetric with a direction or an offset.
void symmetricIntersect(const Handle(TDocStd_Application) & app) {
    auto model = ModelDocument::empty();
    SketchParameters plate{datumSketchPlane(DatumPlane::xy),
                           {{{SketchElementId{1}, SketchRectangle{0, 0, 4, 3}}}, 2}};
    plate.datum = DatumPlane::xy;
    const auto plateSketch = model.addDatumSketchFeature("Plate", plate);
    ExtrusionParameters body{2, {}, {region("o:1.0,1.1,1.2,1.3")}};
    body.operation = ExtrusionOperation::newBody;
    (void)model.addExtrusionFeature("Body", plateSketch, body);
    SketchParameters tool{datumSketchPlane(DatumPlane::xy),
                          {{{SketchElementId{1}, SketchRectangle{1, 1, 3, 2}}}, 2}};
    tool.datum = DatumPlane::xy;
    const auto toolSketch = model.addDatumSketchFeature("Tool", tool);
    ExtrusionParameters common{2, {}, {region("o:1.0,1.1,1.2,1.3")}};
    common.operation = ExtrusionOperation::intersect;
    common.target = BodyId{plateSketch.value()};
    common.symmetric = true;
    const auto commonId = model.addExtrusionFeature("Common", toolSketch, common);
    for (const auto change : {+[](ExtrusionParameters& p) { p.reversed = true; },
                              +[](ExtrusionParameters& p) { p.offset = 1; }}) {
        auto bad = common;
        change(bad);
        rejected([&] { (void)model.setExtrusionParameters(commonId, bad); });
    }
    const auto expected =
        IdentifiedDocumentSnapshot::restore(DocumentId{3, 7}, DocumentSnapshot::capture(model));
    Handle(TDocStd_Document) doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    Handle(TDocStd_Document) loaded = open(app, bytes);
    require(NativePrimitiveCodec::read(loaded) == expected, "Symmetric intersect roundtrip");
    app->Close(loaded);
    TDataStd_Integer::Set(doc->Main().FindChild(2), 12);
    rejected([&] { (void)NativePrimitiveCodec::read(doc); }); // schema 13 children
    app->Close(doc);
    // Intersect alone (no symmetric child) also needs schema 13.
    common.symmetric = false;
    require(model.setExtrusionParameters(commonId, common), "Plain intersect set");
    Handle(TDocStd_Document) plain = fresh(app);
    NativePrimitiveCodec::write(plain, IdentifiedDocumentSnapshot::restore(
                                           DocumentId{3, 8}, DocumentSnapshot::capture(model)));
    TDataStd_Integer::Set(plain->Main().FindChild(2), 12);
    rejected([&] { (void)NativePrimitiveCodec::read(plain); }); // operation 4
    app->Close(plain);
    std::cout << "schema 13 symmetric intersect round-trip and gate passed\n";
}
// Schema 17 (user decision 2026-10-13): fillet edges carry ids and may name a rounding face; a
// schema 16 file has none and Open numbers them 1..n; schema 17 data read as 16 is refused.
void treatedEdgeIds(const Handle(TDocStd_Application) & app) {
    ModelDocument model;
    const auto box = model.addBoxFeature("Box", {20, 20, 10});
    const auto side = [&](kernel::PrimitiveRole role) { return SubshapeOrigin{box, role}; };
    FilletParameters fillet{
        BodyId{box.value()},
        {{side(kernel::PrimitiveRole::boxPositiveY), side(kernel::PrimitiveRole::boxPositiveZ), 4},
         {side(kernel::PrimitiveRole::boxNegativeY), side(kernel::PrimitiveRole::boxPositiveZ), 7}},
        1.0};
    const auto rounded = model.addFilletFeature("Rounding", fillet);
    SubshapeOrigin roundFace{rounded, std::nullopt};
    roundFace.treatedEdge = 4;
    static_cast<void>(model.addChamferFeature(
        "Chamfer", ChamferParameters{BodyId{box.value()},
                                     {{roundFace, side(kernel::PrimitiveRole::boxPositiveX), 1}},
                                     0.3}));
    const auto expected =
        IdentifiedDocumentSnapshot::restore(DocumentId{3, 9}, DocumentSnapshot::capture(model));
    Handle(TDocStd_Document) doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    Handle(TDocStd_Document) loaded = open(app, bytes);
    require(NativePrimitiveCodec::read(loaded) == expected,
            "Edge ids and rounding faces round-trip");
    app->Close(loaded);
    TDataStd_Integer::Set(doc->Main().FindChild(2), 16);
    rejected([&] { (void)NativePrimitiveCodec::read(doc); }); // ids need schema 17
    app->Close(doc);

    // A schema 16 file: the fillet alone, without ids.
    auto older = ModelDocument::empty();
    const auto block = older.addBoxFeature("Box", {20, 20, 10});
    fillet.body = BodyId{block.value()};
    for (auto& edge : fillet.edges) {
        edge.first.feature = block;
        edge.second.feature = block;
    }
    static_cast<void>(older.addFilletFeature("Rounding", fillet));
    Handle(TDocStd_Document) legacy = fresh(app);
    NativePrimitiveCodec::write(legacy, IdentifiedDocumentSnapshot::restore(
                                            DocumentId{3, 10}, DocumentSnapshot::capture(older)));
    const auto edges = feature(legacy, 2).FindChild(4).FindChild(2);
    legacy->OpenCommand();
    for (int index = 1; index <= 2; ++index)
        require(edges.FindChild(index).ForgetAttribute(TDataStd_AsciiString::GetID()),
                "Edge id removed for the schema 16 fixture");
    legacy->CommitCommand();
    TDataStd_Integer::Set(legacy->Main().FindChild(2), 16);
    const auto read = NativePrimitiveCodec::read(legacy);
    const auto& numbered = read.snapshot().filletParameters(read.snapshot().features()[1].id);
    require(numbered.edges.size() == 2 && numbered.edges[0].id == 1 && numbered.edges[1].id == 2,
            "A schema 16 fillet gets edge ids 1..n");
    app->Close(legacy);
    std::cout << "schema 17 edge ids, rounding faces and schema 16 numbering passed\n";
}

void regressions(const Handle(TDocStd_Application) & app) {
    treatedEdgeIds(app);
    historicalProfiles(app);
    datumSketches(app);
    extrusionModes(app);
    faceExtrusions(app);
    symmetricIntersect(app);
    regionSelections(app);
    auto doc = fresh(app);
    const auto expected = fixture();
    NativePrimitiveCodec::write(doc, expected);
    check(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    auto loaded = open(app, bytes);
    check(loaded, expected);
    app->Close(loaded);
    auto other = fresh(app);
    NativePrimitiveCodec::write(other, expected);
    const auto extrusion = feature(doc, 9);
    const auto payload = extrusion.FindChild(4);
    const auto profiles = payload.FindChild(1);
    doc->SetUndoLimit(10);
    const auto corrupt = [&](auto mutate) {
        doc->NewCommand();
        mutate();
        rejected([&] { (void)NativePrimitiveCodec::read(doc); });
        doc->AbortCommand();
        require(NativePrimitiveCodec::read(doc) == expected, "Rejected edit damaged document");
    };
    for (double distance : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::quiet_NaN()})
        corrupt([&] { TDataStd_Real::Set(payload, distance); });
    corrupt([&] { payload.ForgetAttribute(TDataStd_Real::GetID()); });
    for (int count : {-1, 0, 3, 1000001})
        corrupt([&] { TDataStd_Integer::Set(profiles, count); });
    for (const char* id : {"0", "02", "-1", "18446744073709551616", "2"})
        corrupt([&] { TDataStd_AsciiString::Set(profiles.FindChild(1), id); });
    corrupt([&] { profiles.FindChild(1).ForgetAllAttributes(); });
    corrupt([&] { extrusion.FindChild(5).ForgetAllAttributes(); });
    corrupt([&] { TDF_Reference::Set(extrusion.FindChild(5), feature(other, 8)); });
    corrupt([&] { TDF_Reference::Set(extrusion.FindChild(5), doc->Main()); });
    corrupt([&] { TDF_Reference::Set(extrusion.FindChild(5), feature(doc, 7)); });
    corrupt([&] { TDF_Reference::Set(extrusion.FindChild(5), feature(doc, 5)); }); // wrong body
    corrupt([&] { TDF_Reference::Set(feature(doc, 8).FindChild(5), extrusion); }); // cycle
    rejected([&] { (void)detail::readNativeExtrusion(payload, 1, 5); });
    rejected([&] { detail::validateNativeExtrusion({0, {}}); });
    rejected([&] { detail::validateNativeExtrusion({1, {SketchProfileId{0}}}); });
    rejected(
        [&] { detail::validateNativeExtrusion({1, {SketchProfileId{2}, SketchProfileId{2}}}); });
    doc->NewCommand();
    TDataStd_Real::Set(payload, 4);
    require(doc->CommitCommand(), "Extrusion edit failed");
    const auto edited = NativePrimitiveCodec::read(doc);
    require(edited != expected && edited.id() == expected.id() && doc->Undo() &&
                NativePrimitiveCodec::read(doc) == expected && doc->Redo() &&
                NativePrimitiveCodec::read(doc) == edited && doc->Undo(),
            "Extrusion history lost identity");
    doc->NewCommand();
    extrusion.FindChild(5).ForgetAllAttributes();
    std::stringstream malformed(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, malformed);
    loaded = open(app, malformed);
    rejected([&] { (void)NativePrimitiveCodec::read(loaded); });
    app->Close(loaded);
    doc->AbortCommand();
    app->Close(other);
    app->Close(doc);
}
} // namespace
int main(int argc, char** argv) {
    try {
        Handle(TDocStd_Application) app = new TDocStd_Application;
        BinDrivers::DefineFormat(app);
        if (argc == 1)
            regressions(app);
        else {
            require(argc == 3 || argc == 4, "Usage: create FILE | read FILE [REWRITE]");
            Handle(TDocStd_Document) doc;
            const std::string mode = argv[1];
            if (mode == "create") {
                doc = fresh(app);
                NativePrimitiveCodec::write(doc, fixture());
            } else {
                require(mode == "read", "Unknown mode");
                std::ifstream input(argv[2], std::ios::binary);
                doc = open(app, input);
            }
            check(doc, fixture());
            if (mode == "create" || argc == 4) {
                std::ofstream output(argv[argc - 1], std::ios::binary);
                save(app, doc, output);
                output.close();
                require(!output.fail(), "Fixture flush failed");
            }
            app->Close(doc);
        }
        std::cout << "PASS native extrusions: profiles, distance, references, identities, BRep, "
                     "history\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << kernel::occtFailureDiagnostic(error) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
