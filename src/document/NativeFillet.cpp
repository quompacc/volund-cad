#include "NativeFillet.hpp"
#include "NativeSketchSupport.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <charconv>
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
        result.edges.push_back({std::move(first), std::move(second)});
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
} // namespace voelund::document::detail
