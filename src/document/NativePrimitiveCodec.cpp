#include "NativePrimitiveCodec.hpp"
#include "DocumentSerializationDetail.hpp"
#include "NativeDocumentMetadata.hpp"
#include "NativeExtrusion.hpp"
#include "NativeParameters.hpp"
#include "NativeSketchGeometry.hpp"
#include <TDF_Reference.hxx>
#include <TDF_Tool.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_RealArray.hxx>
#include <charconv>
#include <map>
#include <set>
#include <voelund/document/LegacyBodyIdentity.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

namespace voelund::document::detail {
namespace {
void require(bool condition) {
    if (!condition)
        throw std::invalid_argument("Invalid or unsupported native primitive data");
}
TDF_Label child(const TDF_Label& parent, int tag) {
    require(!parent.IsNull());
    const auto label = parent.FindChild(tag, false);
    require(!label.IsNull());
    return label;
}
std::string text(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.FindAttribute(TDataStd_AsciiString::GetID(), value));
    const auto& string = value->Get();
    require(string.Length() <= static_cast<int>(maximumNameBytes));
    return {string.ToCString(), static_cast<std::size_t>(string.Length())};
}
std::uint64_t id(const TDF_Label& label) {
    const auto encoded = text(label);
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(encoded.data(), encoded.data() + encoded.size(), value);
    require(parsed.ec == std::errc{} && parsed.ptr == encoded.data() + encoded.size() &&
            encoded == std::to_string(value) && hasIdCapacity(value, 1));
    return value;
}
int integer(const TDF_Label& label) {
    Handle(TDataStd_Integer) value;
    require(label.FindAttribute(TDataStd_Integer::GetID(), value));
    return value->Get();
}
int count(const TDF_Label& label) {
    const auto size = integer(label);
    require(size >= 0 && size <= static_cast<int>(maximumFeatureCount) &&
            label.NbChildren() == size);
    return size;
}
std::string entry(const TDF_Label& label) {
    TCollection_AsciiString value;
    TDF_Tool::Entry(label, value);
    return value.ToCString();
}
TDF_Label reference(const TDF_Label& label, const TDF_Label& root) {
    Handle(TDF_Reference) value;
    require(label.FindAttribute(TDF_Reference::GetID(), value));
    const auto target = value->Get();
    require(!target.IsNull() && target.Data() == root.Data());
    return target;
}
int modeCode(CylinderResultMode mode) {
    switch (mode) {
    case CylinderResultMode::join:
        return 1;
    case CylinderResultMode::cut:
        return 2;
    case CylinderResultMode::intersect:
        return 3;
    case CylinderResultMode::separate:
        return 4;
    }
    throw std::invalid_argument("Unknown native cylinder mode");
}
CylinderResultMode modeValue(int mode) {
    switch (mode) {
    case 1:
        return CylinderResultMode::join;
    case 2:
        return CylinderResultMode::cut;
    case 3:
        return CylinderResultMode::intersect;
    case 4:
        return CylinderResultMode::separate;
    default:
        throw std::invalid_argument("Unknown native cylinder mode");
    }
}
std::vector<double> values(const FeatureParameters& parameters) {
    std::vector<double> data;
    const kernel::RigidTransform* placement;
    if (const auto* box = std::get_if<BoxParameters>(&parameters)) {
        data = {box->dimensions.width, box->dimensions.depth, box->dimensions.height};
        placement = &box->placement;
    } else {
        const auto& cylinder = std::get<CylinderParameters>(parameters);
        data = {cylinder.radius, cylinder.height};
        placement = &cylinder.placement;
    }
    data.insert(data.end(), {placement->translationX, placement->translationY,
                             placement->translationZ, placement->rotationX.value(),
                             placement->rotationY.value(), placement->rotationZ.value()});
    return data;
}
std::vector<FeatureBodyIdentity> validate(std::span<const FeatureDescriptor> features,
                                          std::span<const FeatureParameters> parameters,
                                          std::uint64_t next, const ParameterTable& table) {
    require(features.size() == parameters.size() && features.size() <= maximumFeatureCount &&
            hasIdCapacity(next, 0) &&
            nativeParameterRecords(table) <= maximumDocumentRecords - features.size());
    validateParameterBindings(parameters, table);
    std::size_t remainingElements =
        maximumDocumentRecords - features.size() - nativeParameterRecords(table);
    for (std::size_t index = 0; index < features.size(); ++index) {
        const auto& feature = features[index];
        const bool box = feature.kind == FeatureKind::box;
        if (feature.kind == FeatureKind::extrusion) {
            require(std::holds_alternative<ExtrusionParameters>(parameters[index]) &&
                    feature.id.value() < next && feature.name.size() <= maximumNameBytes &&
                    feature.name.find('\0') == std::string::npos);
            const auto& extrusion = std::get<ExtrusionParameters>(parameters[index]);
            require(nativeExtrusionRecords(extrusion) <= remainingElements);
            remainingElements -= nativeExtrusionRecords(extrusion);
            validateNativeExtrusion(extrusion);
            continue;
        }
        if (feature.kind == FeatureKind::sketch) {
            require(std::holds_alternative<SketchParameters>(parameters[index]) &&
                    feature.id.value() < next && feature.name.size() <= maximumNameBytes &&
                    feature.name.find('\0') == std::string::npos);
            const auto& sketch = std::get<SketchParameters>(parameters[index]);
            require(sketch.elements.values.size() <= remainingElements);
            remainingElements -= sketch.elements.values.size();
            require(sketch.constraints.values.size() <= remainingElements);
            remainingElements -= sketch.constraints.values.size();
            validateNativeSketchGeometry(sketch);
            if (sketch.supportBinding) {
                const auto size = sketch.supportBinding->reference.origin.extrusionBoundary.size();
                require(size <= remainingElements);
                remainingElements -= size;
            }
            continue;
        }
        require((box || feature.kind == FeatureKind::cylinder) &&
                (box ? std::holds_alternative<BoxParameters>(parameters[index])
                     : std::holds_alternative<CylinderParameters>(parameters[index])) &&
                feature.id.value() < next && feature.name.size() <= maximumNameBytes &&
                feature.name.find('\0') == std::string::npos);
        if (!box)
            validateCylinderParameters(std::get<CylinderParameters>(parameters[index]));
        const auto data = values(parameters[index]);
        const unsigned dimensions = box ? 3U : 2U;
        for (unsigned item = 0; item < data.size(); ++item) {
            require(std::isfinite(data[item]));
            if (item < dimensions)
                require(kernel::modelLength(data[item]));
            else if (item < dimensions + 3)
                require(kernel::modelCoordinate(data[item]));
        }
    }
    // Includes unique/non-reserved IDs, input kinds, missing references and cycles.
    auto assignments = legacyBodyIdentities(features, parameters);
    // Sketches create no solid. Store them in the group of the body they work on: the
    // support's body, a base-plane sketch's own body (BodyId = its FeatureId, schema 6) or
    // the body its prisms all name (schema 8).
    const auto contexts = featureBodyContexts(features, parameters);
    for (std::size_t index = 0; index < features.size(); ++index)
        if (features[index].kind == FeatureKind::sketch)
            assignments[index].body = contexts[index];
    return assignments;
}
FeatureParameters parameters(const TDF_Label& feature, bool box) {
    Handle(TDataStd_RealArray) data;
    require(child(feature, 4).FindAttribute(TDataStd_RealArray::GetID(), data));
    const int dimensions = box ? 3 : 2;
    require(data->Lower() == 1 && data->Upper() == dimensions + 6);
    const kernel::RigidTransform placement{data->Value(dimensions + 1),
                                           data->Value(dimensions + 2),
                                           data->Value(dimensions + 3),
                                           kernel::Radians{data->Value(dimensions + 4)},
                                           kernel::Radians{data->Value(dimensions + 5)},
                                           kernel::Radians{data->Value(dimensions + 6)}};
    if (box)
        return BoxParameters{{data->Value(1), data->Value(2), data->Value(3)}, placement};
    return CylinderParameters{data->Value(1), data->Value(2), placement,
                              modeValue(integer(child(feature, 6)))};
}
struct Record {
    TDF_Label label;
    BodyId body;
    FeatureDescriptor feature;
    FeatureParameters parameters;
};
} // namespace

void NativePrimitiveCodec::write(const Handle(TDocStd_Document) & document,
                                 const IdentifiedDocumentSnapshot& identified) {
    const auto& snapshot = identified.snapshot();
    const auto assignments =
        validate(snapshot.features(), snapshot.parameters(), snapshot.nextFeatureId(),
                 snapshot.parameterTable());
    for (std::size_t i = 0; i < snapshot.parameters().size(); ++i)
        if (const auto* sketch = std::get_if<SketchParameters>(&snapshot.parameters()[i]); sketch) {
            if (sketch->supportBinding)
                require(sketch->supportBinding->reference.document == identified.id() &&
                        assignments[i].body == sketch->supportBinding->reference.body);
            for (const auto& constraint : sketch->constraints.values)
                if (constraint.projectedEdge)
                    require(constraint.projectedEdge->document == identified.id() &&
                            assignments[i].body == constraint.projectedEdge->body);
        }
    // All domain validation precedes the first native mutation.
    initializeNativeMetadata(document, {identified.id(), snapshot.nextFeatureId()});
    const auto bodies = document->Main().FindChild(10);
    const auto order = document->Main().FindChild(20);
    TDataStd_AsciiString::Set(bodies, "Primitives.v1");
    TDataStd_Integer::Set(order, static_cast<int>(snapshot.features().size()));
    std::map<std::uint64_t, TDF_Label> bodyLabels;
    std::map<std::uint64_t, TDF_Label> featureLabels;
    for (std::size_t index = 0; index < snapshot.features().size(); ++index) {
        const auto& feature = snapshot.features()[index];
        const auto bodyId = assignments[index].body->value();
        auto found = bodyLabels.find(bodyId);
        if (found == bodyLabels.end()) {
            const auto body = bodies.FindChild(static_cast<int>(bodyLabels.size() + 1));
            TDataStd_AsciiString::Set(body.FindChild(1), std::to_string(bodyId).c_str());
            found = bodyLabels.emplace(bodyId, body).first;
        }
        const auto members = found->second.FindChild(10);
        const auto native = members.FindChild(members.NbChildren() + 1);
        TDataStd_Integer::Set(members, members.NbChildren());
        const bool box = feature.kind == FeatureKind::box;
        const bool sketch = feature.kind == FeatureKind::sketch;
        const bool extrusion = feature.kind == FeatureKind::extrusion;
        TDataStd_AsciiString::Set(native.FindChild(1), box         ? "box"
                                                       : sketch    ? "sketch"
                                                       : extrusion ? "extrusion"
                                                                   : "cylinder");
        TDataStd_AsciiString::Set(native.FindChild(2), std::to_string(feature.id.value()).c_str());
        TDataStd_AsciiString::Set(native.FindChild(3), feature.name.c_str());
        if (sketch) {
            writeNativeSketchGeometry(native.FindChild(4),
                                      std::get<SketchParameters>(snapshot.parameters()[index]));
        } else if (extrusion) {
            writeNativeExtrusion(native.FindChild(4),
                                 std::get<ExtrusionParameters>(snapshot.parameters()[index]));
        } else {
            const auto data = values(snapshot.parameters()[index]);
            auto array =
                TDataStd_RealArray::Set(native.FindChild(4), 1, static_cast<int>(data.size()));
            for (int item = 1; item <= static_cast<int>(data.size()); ++item)
                array->SetValue(item, data[static_cast<std::size_t>(item - 1)]);
        }
        if (feature.kind == FeatureKind::cylinder)
            TDataStd_Integer::Set(
                native.FindChild(6),
                modeCode(std::get<CylinderParameters>(snapshot.parameters()[index]).resultMode));
        featureLabels.emplace(feature.id.value(), native);
        TDF_Reference::Set(order.FindChild(static_cast<int>(index + 1)), native);
    }
    TDataStd_Integer::Set(bodies, static_cast<int>(bodyLabels.size()));
    if (!(snapshot.parameterTable() == ParameterTable{}))
        writeNativeParameterTable(document->Main().FindChild(nativeParameterSlot),
                                  snapshot.parameterTable());
    for (const auto& feature : snapshot.features())
        if (feature.inputFeature)
            TDF_Reference::Set(featureLabels.at(feature.id.value()).FindChild(5),
                               featureLabels.at(feature.inputFeature->value()));
}

IdentifiedDocumentSnapshot NativePrimitiveCodec::read(const Handle(TDocStd_Document) & document) {
    const auto metadata = readNativeMetadata(document);
    const auto root = document->Main();
    const auto parameterLabel = root.FindChild(nativeParameterSlot, false);
    require(root.NbChildren() == (parameterLabel.IsNull() ? 6 : 7) &&
            (parameterLabel.IsNull() || metadata.schemaVersion >= 7));
    const auto bodies = child(root, 10);
    const auto order = child(root, 20);
    require(text(bodies) == "Primitives.v1");
    const int bodyCount = count(bodies), featureCount = count(order);
    std::size_t remainingElements = maximumDocumentRecords - static_cast<std::size_t>(featureCount);
    const auto table = parameterLabel.IsNull()
                           ? ParameterTable{}
                           : readNativeParameterTable(parameterLabel, remainingElements);
    remainingElements -= nativeParameterRecords(table);
    std::vector<Record> records;
    std::map<std::string, std::size_t> labels;
    std::set<std::uint64_t> bodyIds;
    for (int index = 1; index <= bodyCount; ++index) {
        const auto body = child(bodies, index);
        require(body.NbChildren() == 2);
        const BodyId bodyId{id(child(body, 1))};
        require(bodyIds.insert(bodyId.value()).second);
        const auto members = child(body, 10);
        const int memberCount = count(members);
        require(memberCount > 0 && records.size() + static_cast<std::size_t>(memberCount) <=
                                       static_cast<std::size_t>(featureCount));
        for (int member = 1; member <= memberCount; ++member) {
            const auto native = child(members, member);
            const auto kind = text(child(native, 1));
            const bool box = kind == "box";
            const bool sketch = kind == "sketch";
            const bool extrusion = kind == "extrusion";
            // A base-plane sketch (schema 6) has no input reference child 5.
            const bool standaloneSketch =
                sketch && native.FindChild(5, false).IsNull() && metadata.schemaVersion >= 6;
            require((box || sketch || extrusion || kind == "cylinder") &&
                    native.NbChildren() == (box || standaloneSketch ? 4
                                            : (sketch || extrusion) ? 5
                                                                    : 6));
            labels.emplace(entry(native), records.size());
            FeatureParameters decoded =
                sketch ? FeatureParameters{readNativeSketchGeometry(
                             child(native, 4), remainingElements, metadata.schemaVersion)}
                : extrusion
                    ? FeatureParameters{readNativeExtrusion(child(native, 4), remainingElements,
                                                            metadata.schemaVersion)}
                    : parameters(native, box);
            if (sketch) {
                const auto& data = std::get<SketchParameters>(decoded);
                remainingElements -= data.elements.values.size() + data.constraints.values.size();
                if (data.supportBinding)
                    remainingElements -=
                        data.supportBinding->reference.origin.extrusionBoundary.size();
            }
            if (extrusion)
                remainingElements -= nativeExtrusionRecords(std::get<ExtrusionParameters>(decoded));
            records.push_back({native,
                               bodyId,
                               {FeatureId{id(child(native, 2))},
                                box         ? FeatureKind::box
                                : sketch    ? FeatureKind::sketch
                                : extrusion ? FeatureKind::extrusion
                                            : FeatureKind::cylinder,
                                text(child(native, 3)), std::nullopt},
                               std::move(decoded)});
        }
    }
    require(records.size() == static_cast<std::size_t>(featureCount));
    const auto resolve = [&](const TDF_Label& label) {
        const auto found = labels.find(entry(reference(label, root)));
        require(found != labels.end());
        return found->second;
    };
    for (auto& record : records)
        if (record.feature.kind != FeatureKind::box && !record.label.FindChild(5, false).IsNull())
            record.feature.inputFeature = records[resolve(child(record.label, 5))].feature.id;
    std::vector<FeatureDescriptor> features;
    std::vector<FeatureParameters> data;
    std::vector<BodyId> storedBodies;
    std::set<std::size_t> visited;
    for (int index = 1; index <= featureCount; ++index) {
        const auto recordIndex = resolve(child(order, index));
        require(visited.insert(recordIndex).second);
        const auto& record = records[recordIndex];
        features.push_back(record.feature);
        data.push_back(record.parameters);
        storedBodies.push_back(record.body);
    }
    const auto assignments = validate(features, data, metadata.nextFeatureId, table);
    for (std::size_t index = 0; index < assignments.size(); ++index)
        require(assignments[index].body == storedBodies[index]);
    for (std::size_t i = 0; i < data.size(); ++i)
        if (const auto* sketch = std::get_if<SketchParameters>(&data[i]); sketch) {
            if (sketch->supportBinding)
                require(sketch->supportBinding->reference.document == metadata.id &&
                        assignments[i].body == sketch->supportBinding->reference.body);
            for (const auto& constraint : sketch->constraints.values)
                if (constraint.projectedEdge)
                    require(constraint.projectedEdge->document == metadata.id &&
                            assignments[i].body == constraint.projectedEdge->body);
        }
    return IdentifiedDocumentSnapshot::restore(
        metadata.id,
        DocumentSnapshot{std::move(features), std::move(data), metadata.nextFeatureId, table});
}
} // namespace voelund::document::detail
