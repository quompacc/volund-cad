#include "NativeSolidFeatures.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_ByteArray.hxx>
#include <TDataStd_Integer.hxx>
#include <charconv>
#include <climits>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

// M17 (schema 35): an import keeps its file name (1), source unit (2) and its solids (3), each
// with its name (1) and its OCCT binary BRep (2). A part keeps its index on the label and its
// import (1).
namespace voelund::document::detail {
namespace {
void require(bool value) {
    if (!value)
        throw std::invalid_argument("Invalid native import data");
}
TDF_Label child(const TDF_Label& label, int tag) {
    const auto result = label.FindChild(tag, false);
    require(!result.IsNull());
    return result;
}
int integer(const TDF_Label& label) {
    Handle(TDataStd_Integer) value;
    require(label.FindAttribute(TDataStd_Integer::GetID(), value));
    return value->Get();
}
std::string text(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.NbChildren() == 0 && label.FindAttribute(TDataStd_AsciiString::GetID(), value));
    return {value->Get().ToCString(), static_cast<std::size_t>(value->Get().Length())};
}
std::uint64_t readId(const TDF_Label& label) {
    const auto encoded = text(label);
    std::uint64_t id = 0;
    const auto parsed = std::from_chars(encoded.data(), encoded.data() + encoded.size(), id);
    require(encoded.size() <= 20 && parsed.ec == std::errc{} &&
            parsed.ptr == encoded.data() + encoded.size() && id != 0 &&
            encoded == std::to_string(id));
    return id;
}
// The byte array type of TDataStd_ByteArray: TColStd_HArray1OfByte in OCCT 7.9,
// NCollection_HArray1<uint8_t> in OCCT 8.
using ByteArray = std::remove_cvref_t<
    decltype(std::declval<TDataStd_ByteArray&>().InternalArray())>::element_type;
void writeBytes(const TDF_Label& label, const std::string& bytes) {
    require(!bytes.empty() && bytes.size() <= static_cast<std::size_t>(INT_MAX));
    Handle(ByteArray) data = new ByteArray(1, static_cast<int>(bytes.size()));
    std::memcpy(&data->ChangeValue(1), bytes.data(), bytes.size());
    TDataStd_ByteArray::Set(label, 1, 1)->ChangeArray(data, false);
}
std::string readBytes(const TDF_Label& label) {
    Handle(TDataStd_ByteArray) value;
    require(label.NbChildren() == 0 && label.FindAttribute(TDataStd_ByteArray::GetID(), value));
    const auto data = value->InternalArray();
    require(!data.IsNull() && data->Length() > 0);
    std::string bytes(static_cast<std::size_t>(data->Length()), '\0');
    std::memcpy(bytes.data(), &data->Value(data->Lower()), bytes.size());
    return bytes;
}
} // namespace

std::size_t nativeImportRecords(const ImportParameters& parameters) {
    return parameters.solids.size();
}

void writeNativeImport(const TDF_Label& label, const ImportParameters& parameters) {
    validateImportParameters(parameters);
    TDataStd_AsciiString::Set(label.FindChild(1), parameters.fileName.c_str());
    TDataStd_AsciiString::Set(label.FindChild(2), parameters.sourceUnit.c_str());
    const auto solids = label.FindChild(3);
    TDataStd_Integer::Set(solids, static_cast<int>(parameters.solids.size()));
    int index = 0;
    for (const auto& solid : parameters.solids) {
        const auto item = solids.FindChild(++index);
        TDataStd_AsciiString::Set(item.FindChild(1), solid.name.c_str());
        writeBytes(item.FindChild(2), *solid.brep);
    }
}

ImportParameters readNativeImport(const TDF_Label& label, std::size_t remainingRecords,
                                  int schemaVersion) {
    require(schemaVersion >= 35 && label.NbChildren() == 3);
    ImportParameters result;
    result.fileName = text(child(label, 1));
    result.sourceUnit = text(child(label, 2));
    const auto solids = child(label, 3);
    const int count = integer(solids);
    require(count > 0 && static_cast<std::size_t>(count) <= maximumImportedSolids &&
            static_cast<std::size_t>(count) <= remainingRecords && solids.NbChildren() == count);
    for (int index = 1; index <= count; ++index) {
        const auto item = child(solids, index);
        require(item.NbChildren() == 2);
        result.solids.push_back(importedSolid(readBytes(child(item, 2)), text(child(item, 1))));
    }
    validateImportParameters(result);
    return result;
}

void writeNativeImportPart(const TDF_Label& label, const ImportPartParameters& parameters) {
    TDataStd_Integer::Set(label, static_cast<int>(parameters.index));
    TDataStd_AsciiString::Set(label.FindChild(1), std::to_string(parameters.import.value()).c_str());
}

ImportPartParameters readNativeImportPart(const TDF_Label& label, int schemaVersion) {
    require(schemaVersion >= 35 && label.NbChildren() == 1);
    const int index = integer(label);
    require(index >= 1 && static_cast<std::size_t>(index) < maximumImportedSolids);
    return {FeatureId{readId(child(label, 1))}, static_cast<std::uint32_t>(index)};
}

} // namespace voelund::document::detail
