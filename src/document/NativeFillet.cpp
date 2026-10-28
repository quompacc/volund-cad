// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "NativeFillet.hpp"
#include "NativeSketchSupport.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <TDataStd_RealArray.hxx>
#include <charconv>
#include <cstdint>
#include <sstream>
#include <string>

namespace voelund::document::detail {
namespace {
void require(bool value) {
    if (!value)
        throw std::invalid_argument("Invalid native fillet data");
}
TDF_Label child(const TDF_Label& label, int tag) {
    const auto result = label.FindChild(tag, false);
    require(!result.IsNull());
    return result;
}
std::uint64_t readDecimal(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.NbChildren() == 0 && label.FindAttribute(TDataStd_AsciiString::GetID(), value) &&
            value->Get().Length() <= 20);
    const std::string text{value->Get().ToCString(),
                           static_cast<std::size_t>(value->Get().Length())};
    std::uint64_t id = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && id != 0 &&
            text == std::to_string(id));
    return id;
}
std::uint64_t decimal(const Handle(TDataStd_AsciiString) & value) {
    require(value->Get().Length() <= 20);
    const std::string text{value->Get().ToCString(),
                           static_cast<std::size_t>(value->Get().Length())};
    std::uint64_t id = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && id != 0 &&
            text == std::to_string(id));
    return id;
}
SubshapeOrigin readFace(const TDF_Label& face, std::size_t& remainingRecords, int schemaVersion) {
    // Children 2, 3, 4 and the optional extension 6 of the shared origin encoding.
    require(face.NbChildren() == 3 + nativeSubshapeOriginExtension(face));
    return readNativeSubshapeOrigin(face, remainingRecords, schemaVersion);
}

std::size_t edgeRecords(const std::vector<FilletEdge>& edges) {
    std::size_t records = edges.size();
    for (const auto& edge : edges)
        records += edge.first.extrusionBoundary.size() + edge.second.extrusionBoundary.size();
    return records;
}

// M05/M07: the value (radius or distance) on the label, body (1), edges (2).
void writeEdges(const TDF_Label& label, BodyId body, const std::vector<FilletEdge>& edges,
                double value) {
    TDataStd_Real::Set(label, value);
    TDataStd_AsciiString::Set(label.FindChild(1), std::to_string(body.value()).c_str());
    const auto list = label.FindChild(2);
    TDataStd_Integer::Set(list, static_cast<int>(edges.size()));
    for (std::size_t index = 0; index < edges.size(); ++index) {
        const auto edge = list.FindChild(static_cast<int>(index + 1));
        writeNativeSubshapeOrigin(edge.FindChild(1), edges[index].first);
        writeNativeSubshapeOrigin(edge.FindChild(2), edges[index].second);
        TDataStd_AsciiString::Set(edge, std::to_string(edges[index].id).c_str()); // schema 17
    }
}

struct DecodedEdges final {
    BodyId body;
    std::vector<FilletEdge> edges;
    double value;
};
DecodedEdges readEdges(const TDF_Label& label, std::size_t remainingRecords, int schemaVersion) {
    require(label.NbChildren() == 2);
    Handle(TDataStd_Real) value;
    require(label.FindAttribute(TDataStd_Real::GetID(), value));
    DecodedEdges result{BodyId{readDecimal(child(label, 1))}, {}, value->Get()};
    const auto edges = child(label, 2);
    Handle(TDataStd_Integer) size;
    require(edges.FindAttribute(TDataStd_Integer::GetID(), size));
    const int count = size->Get();
    require(count > 0 && static_cast<std::size_t>(count) <= maximumFilletEdges &&
            static_cast<std::size_t>(count) <= remainingRecords && edges.NbChildren() == count);
    remainingRecords -= static_cast<std::size_t>(count);
    for (int index = 1; index <= count; ++index) {
        const auto edge = child(edges, index);
        require(edge.NbChildren() == 2);
        auto first = readFace(child(edge, 1), remainingRecords, schemaVersion);
        auto second = readFace(child(edge, 2), remainingRecords, schemaVersion);
        // Schema 17 stores each edge's id; earlier files number the edges in order.
        Handle(TDataStd_AsciiString) id;
        const bool stored = edge.FindAttribute(TDataStd_AsciiString::GetID(), id);
        require(stored == (schemaVersion >= 17));
        result.edges.push_back({std::move(first), std::move(second),
                                stored ? decimal(id) : static_cast<std::uint64_t>(index)});
    }
    return result;
}
} // namespace

std::size_t nativeFilletRecords(const FilletParameters& parameters) {
    return edgeRecords(parameters.edges);
}

void writeNativeFillet(const TDF_Label& label, const FilletParameters& parameters) {
    validateFilletParameters(parameters);
    writeEdges(label, parameters.body, parameters.edges, parameters.radius);
}

FilletParameters readNativeFillet(const TDF_Label& label, std::size_t remainingRecords,
                                  int schemaVersion) {
    require(schemaVersion >= 14);
    auto decoded = readEdges(label, remainingRecords, schemaVersion);
    FilletParameters result{decoded.body, std::move(decoded.edges), decoded.value};
    validateFilletParameters(result);
    return result;
}

std::size_t nativeChamferRecords(const ChamferParameters& parameters) {
    return edgeRecords(parameters.edges);
}

void writeNativeChamfer(const TDF_Label& label, const ChamferParameters& parameters) {
    validateChamferParameters(parameters);
    writeEdges(label, parameters.body, parameters.edges, parameters.distance);
}

ChamferParameters readNativeChamfer(const TDF_Label& label, std::size_t remainingRecords,
                                    int schemaVersion) {
    require(schemaVersion >= 15);
    auto decoded = readEdges(label, remainingRecords, schemaVersion);
    ChamferParameters result{decoded.body, std::move(decoded.edges), decoded.value};
    validateChamferParameters(result);
    return result;
}
namespace {
// N09 (schema 43): kind, head (diameter, depth, countersink angle), drill point and thread,
// written only for a hole that is not a plain one (older files stay as they were).
void writeHoleVariant(const TDF_Label& label, const HoleParameters& parameters) {
    const auto& variant = parameters.variant;
    if (variant == HoleVariant{})
        return;
    TDataStd_Integer::Set(label.FindChild(5), static_cast<int>(variant.kind));
    auto head = TDataStd_RealArray::Set(label.FindChild(6), 1, 3);
    head->SetValue(1, variant.headDiameter);
    head->SetValue(2, variant.headDepth);
    head->SetValue(3, variant.countersinkAngle);
    TDataStd_Integer::Set(label.FindChild(7), variant.drillPoint ? 1 : 0);
    TDataStd_AsciiString::Set(label.FindChild(8), variant.thread.c_str());
}
// N09b (schema 44): the sketch point elements, as decimal IDs separated by spaces.
// N09c (schema 45): the two edges (their faces in 1 and 2) and their distances.
void writeHoleOffsets(const TDF_Label& label, const HoleParameters& parameters) {
    if (parameters.offsets.empty())
        return;
    const auto offsets = label.FindChild(10);
    auto distances = TDataStd_RealArray::Set(offsets, 1, 2);
    for (int index = 1; index <= 2; ++index) {
        const auto& offset = parameters.offsets[static_cast<std::size_t>(index - 1)];
        distances->SetValue(index, offset.distance);
        const auto edge = offsets.FindChild(index);
        writeNativeSubshapeOrigin(edge.FindChild(1), offset.edge.first);
        writeNativeSubshapeOrigin(edge.FindChild(2), offset.edge.second);
    }
}
void readHoleOffsets(const TDF_Label& label, std::size_t& remainingRecords, int schemaVersion,
                     HoleParameters& result) {
    if (label.FindChild(10, false).IsNull())
        return;
    require(schemaVersion >= 45 && remainingRecords >= 2);
    const auto offsets = child(label, 10);
    Handle(TDataStd_RealArray) distances;
    require(offsets.NbChildren() == 2 &&
            offsets.FindAttribute(TDataStd_RealArray::GetID(), distances) &&
            distances->Lower() == 1 && distances->Upper() == 2);
    for (int index = 1; index <= 2; ++index) {
        const auto edge = child(offsets, index);
        require(edge.NbChildren() == 2);
        auto first = readFace(child(edge, 1), remainingRecords, schemaVersion);
        auto second = readFace(child(edge, 2), remainingRecords, schemaVersion);
        result.offsets.push_back({{std::move(first), std::move(second)}, distances->Value(index)});
    }
}
void writeHolePoints(const TDF_Label& label, const HoleParameters& parameters) {
    if (parameters.points.empty())
        return;
    std::string text;
    for (const auto point : parameters.points)
        text += (text.empty() ? "" : " ") + std::to_string(point.value());
    TDataStd_AsciiString::Set(label.FindChild(9), text.c_str());
}
void readHolePoints(const TDF_Label& label, int schemaVersion, HoleParameters& result) {
    if (label.FindChild(9, false).IsNull())
        return;
    require(schemaVersion >= 44);
    Handle(TDataStd_AsciiString) text;
    require(child(label, 9).FindAttribute(TDataStd_AsciiString::GetID(), text));
    std::istringstream input(text->Get().ToCString());
    std::uint64_t value = 0;
    while (input >> value) {
        require(value != 0 && result.points.size() < maximumHolePoints);
        result.points.push_back(SketchElementId{value});
    }
    require(input.eof() && !result.points.empty());
}
void readHoleVariant(const TDF_Label& label, int schemaVersion, HoleVariant& result) {
    if (label.FindChild(5, false).IsNull())
        return;
    require(schemaVersion >= 43);
    Handle(TDataStd_Integer) kind;
    require(child(label, 5).FindAttribute(TDataStd_Integer::GetID(), kind));
    result.kind = static_cast<HoleKind>(kind->Get());
    Handle(TDataStd_RealArray) head;
    require(child(label, 6).FindAttribute(TDataStd_RealArray::GetID(), head) &&
            head->Lower() == 1 && head->Upper() == 3);
    result.headDiameter = head->Value(1);
    result.headDepth = head->Value(2);
    result.countersinkAngle = head->Value(3);
    Handle(TDataStd_Integer) point;
    require(child(label, 7).FindAttribute(TDataStd_Integer::GetID(), point) &&
            (point->Get() == 0 || point->Get() == 1));
    result.drillPoint = point->Get() == 1;
    Handle(TDataStd_AsciiString) thread;
    require(child(label, 8).FindAttribute(TDataStd_AsciiString::GetID(), thread) &&
            thread->Get().Length() <= 16);
    result.thread = thread->Get().ToCString();
}
} // namespace

// N09-N09c: variant (5-8), points (9) and edge offsets (10), each written only when used.
int optionalHoleChildren(const TDF_Label& label) {
    int count = 0;
    for (const auto& [tag, children] : {std::pair{5, 4}, std::pair{9, 1}, std::pair{10, 1}})
        if (!label.FindChild(tag, false).IsNull())
            count += children;
    return count;
}

std::size_t nativeHoleRecords(const HoleParameters& parameters) {
    std::size_t records = parameters.face.origin.extrusionBoundary.size();
    for (const auto& offset : parameters.offsets) // N09c
        records += 1 + offset.edge.first.extrusionBoundary.size() +
                   offset.edge.second.extrusionBoundary.size();
    return records;
}

void writeNativeHole(const TDF_Label& label, const HoleParameters& parameters) {
    validateHoleParameters(parameters);
    TDataStd_Real::Set(label, parameters.diameter);
    TDataStd_AsciiString::Set(label.FindChild(1),
                              std::to_string(parameters.face.body.value()).c_str());
    writeNativeSubshapeOrigin(label.FindChild(2), parameters.face.origin);
    auto position = TDataStd_RealArray::Set(label.FindChild(3), 1, 2);
    position->SetValue(1, parameters.x);
    position->SetValue(2, parameters.y);
    if (parameters.depth)
        TDataStd_Real::Set(label.FindChild(4), *parameters.depth);
    writeHoleVariant(label, parameters);
    writeHolePoints(label, parameters);
    writeHoleOffsets(label, parameters);
}

HoleParameters readNativeHole(const TDF_Label& label, std::size_t remainingRecords,
                              int schemaVersion) {
    const auto depth = label.FindChild(4, false);
    const int extended = optionalHoleChildren(label);
    require(schemaVersion >= 16 && label.NbChildren() == (depth.IsNull() ? 3 : 4) + extended);
    Handle(TDataStd_Real) diameter;
    require(label.FindAttribute(TDataStd_Real::GetID(), diameter));
    const BodyId body{readDecimal(child(label, 1))};
    auto origin = readFace(child(label, 2), remainingRecords, schemaVersion);
    Handle(TDataStd_RealArray) position;
    const auto at = child(label, 3);
    require(at.NbChildren() == 0 && at.FindAttribute(TDataStd_RealArray::GetID(), position) &&
            position->Lower() == 1 && position->Upper() == 2);
    HoleParameters result{BodyFaceReference{body, std::move(origin)}, position->Value(1),
                          position->Value(2), diameter->Get()};
    if (!depth.IsNull()) {
        Handle(TDataStd_Real) value;
        require(depth.NbChildren() == 0 && depth.FindAttribute(TDataStd_Real::GetID(), value));
        result.depth = value->Get();
    }
    readHoleVariant(label, schemaVersion, result.variant);
    readHolePoints(label, schemaVersion, result);
    readHoleOffsets(label, remainingRecords, schemaVersion, result);
    validateHoleParameters(result);
    return result;
}
} // namespace voelund::document::detail
